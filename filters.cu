/*
 * Copyright (c) 2012-14, NVIDIA CORPORATION.  All rights reserved.
 *
 * NVIDIA CORPORATION and its licensors retain all intellectual property
 * and proprietary rights in and to this software, related documentation
 * and any modifications thereto.  Any use, reproduction, disclosure or
 * distribution of this software and related documentation without an express
 * license agreement from NVIDIA CORPORATION is strictly prohibited.
 *
 *
 *
 *
 *
 *
 *
 *
 */

#include <nvbio/basic/types.h>
#include <nvbio/basic/primitives.h>
#include <nvbio/io/sequence/sequence_sam.h> // for read flags

#include "bqsr_types.h"
#include "bqsr_context.h"
#include "filters.h"

// filter if any of the flags are set
template<uint32 flags>
struct filter_if_any_set : public bqsr_lambda
{
    filter_if_any_set(bqsr_context::view ctx,
                      const BAM_alignment_batch_device::const_view batch)
        : bqsr_lambda(ctx, batch)
    { }

    NVBIO_HOST_DEVICE bool operator() (const uint32 read_index)
    {
        if ((batch.flags[read_index] & flags) != 0)
        {
            return false;
        } else {
            return true;
        }
    }
};

// implements the GATK filters MappingQualityUnavailable and MappingQualityZero
struct filter_mapq : public bqsr_lambda
{
    filter_mapq(bqsr_context::view ctx,
                const BAM_alignment_batch_device::const_view batch)
        : bqsr_lambda(ctx, batch)
    { }

    NVBIO_HOST_DEVICE bool operator() (const uint32 read_index)
    {
        if (batch.mapq[read_index] == 0 ||
            batch.mapq[read_index] == 255)
        {
            return false;
        } else {
            return true;
        }
    }
};

// partially implements the GATK MalformedReadFilter
struct filter_malformed_reads : public bqsr_lambda
{
    filter_malformed_reads(bqsr_context::view ctx,
                           const BAM_alignment_batch_device::const_view batch)
        : bqsr_lambda(ctx, batch)
    { }

    NVBIO_HOST_DEVICE bool operator() (const uint32 read_index)
    {
        const BAM_CRQ_index& idx = batch.crq_index[read_index];

        // read is not flagged as unmapped...
        if (!(batch.flags[read_index] & nvbio::io::SAMFlag_SegmentUnmapped))
        {
            // ... but reference sequence ID is invalid (GATK: checkInvalidAlignmentStart)
            if (batch.alignment_sequence_IDs[read_index] == uint32(-1))
            {
                return false;
            }

            // ... but alignment start is -1 (GATK: checkInvalidAlignmentStart)
            if (batch.alignment_positions[read_index] == uint32(-1))
            {
                return false;
            }

            // ... but alignment aligns to negative number of bases in the reference (GATK: checkInvalidAlignmentEnd)
            if (ctx.alignment_windows[read_index].y <= ctx.alignment_windows[read_index].x)
            {
                return false;
            }

            // ... but read is aligned to a point after the end of the contig (GATK: checkAlignmentDisagreesWithHeader)
            if (ctx.sequence_alignment_windows[read_index].y >= ctx.bam_header.sq_lengths[batch.alignment_sequence_IDs[read_index]])
            {
                return false;
            }

            // ... and it has a valid alignment start (tested before), but the CIGAR string is empty (GATK: checkCigarDisagreesWithAlignment)
            // xxxnsubtil: need to verify that this is exactly what GATK does
            if (idx.cigar_len == 0)
            {
                return false;
            }
        }

        // read is aligned to nonexistent contig but alignment start is valid
        // (GATK: checkAlignmentDisagreesWithHeader)
        if (batch.alignment_sequence_IDs[read_index] == uint32(-1) && batch.alignment_positions[read_index] != uint32(-1))
        {
            return false;
        }

        // read has no read group
        // (GATK: checkHasReadGroup)
        if (batch.read_groups[read_index] == uint32(-1))
        {
            return false;
        }

        // read has different number of bases and base qualities
        // (GATK: checkMismatchBasesAndQuals)
        // xxxnsubtil: note that this is meaningless for BAM, but it's here anyway in case we end up parsing SAM files
        if (idx.qual_len != idx.read_len)
        {
            return false;
        }

        // read has no base sequence stored in the file
        // (GATK: checkSeqStored)
        if (idx.read_len == 0)
        {
            return false;
        }

        // CIGAR contains N operators
        // (GATK: checkCigarIsSupported)
        for(uint32 i = idx.cigar_start; i < idx.cigar_start + idx.cigar_len; i++)
        {
            if (batch.cigars[i].op == BAM_cigar_op::OP_N)
            {
                return false;
            }
        }

        return true;
    }
};

// apply read filters to the batch
void filter_reads(bqsr_context *context, const BAM_alignment_batch_device& batch)
{
    D_VectorU32& active_read_list = context->active_read_list;
    D_VectorU32& temp_u32 = context->temp_u32;
    uint32 num_active;
    uint32 start_count;

    // this filter corresponds to the following GATK filters:
    // - DuplicateReadFilter
    // - FailsVendorQualityCheckFilter
    // - NotPrimaryAlignmentFilter
    // - UnmappedReadFilter
    filter_if_any_set<nvbio::io::SAMFlag_Duplicate |
                      nvbio::io::SAMFlag_FailedQC |
                      nvbio::io::SAMFlag_SegmentUnmapped |
                      nvbio::io::SAMFlag_SecondaryAlignment> flags_filter(*context, batch);

    // corresponds to the GATK filters MappingQualityUnavailable and MappingQualityZero
    filter_mapq mapq_filter(*context, batch);
    // corresponds to the GATK filter MalformedReadFilter
    filter_malformed_reads malformed_read_filter(*context, batch);

    start_count = active_read_list.size();

    // make sure the temp buffer is big enough
    context->temp_u32.resize(active_read_list.size());

    // apply the flags filter, copying from active_read_list into temp_u32
    num_active = nvbio::copy_if(active_read_list.size(),
                                active_read_list.begin(),
                                temp_u32.begin(),
                                flags_filter,
                                context->temp_storage);

    // apply the mapq filters, copying from temp_u32 into active_read_list
    num_active = nvbio::copy_if(num_active,
                                temp_u32.begin(),
                                active_read_list.begin(),
                                mapq_filter,
                                context->temp_storage);

    // apply the malformed read filters, copying from active_read_list into temp_u32
    num_active = nvbio::copy_if(num_active,
                                active_read_list.begin(),
                                temp_u32.begin(),
                                malformed_read_filter,
                                context->temp_storage);

    // resize temp_u32 and copy back into active_read_list
    temp_u32.resize(num_active);
    active_read_list = temp_u32;

    context->stats.filtered_reads += start_count - num_active;
}