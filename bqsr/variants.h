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

#pragma once

#include "bqsr_types.h"
#include "alignment_data.h"
#include "sequence_data.h"
#include "from_nvbio/vcf.h"
#include "util.h"

struct snp_filter_context
{
    // active reads for the VCF search
    D_VectorU32 active_read_ids;
    // active VCF range for each read
    D_VectorU32_2 active_vcf_ranges;

    struct view
    {
        D_VectorU32::view active_read_ids;
        D_VectorU32_2::view active_vcf_ranges;
    };

    operator view()
    {
        view v = {
            active_read_ids,
            active_vcf_ranges
        };
        return v;
    }
};

struct SNPDatabase_refIDs : public from_nvbio::SNPDatabase
{
    // maps a variant ID to a reference sequence ID
    H_VectorU32 variant_sequence_ref_ids;
    // the start and end coordinates of each VCF relative to the whole genome
    H_VectorU32 genome_start_positions;
    H_VectorU32 genome_stop_positions;

    void compute_sequence_offsets(const sequence_data& genome);
};


struct DeviceSNPDatabase
{
    // reference sequence ID for each variant
    D_VectorU32 variant_sequence_ref_ids;
    // start and end coordinates of the variant in the genome (first base in genome is position 0)
    D_VectorU32 genome_start_positions;
    D_VectorU32 genome_stop_positions;
    // start and stop position of the variant in the reference sequence (first base in the sequence is position 0)
    D_VectorU32_2 sequence_positions;

    // packed reference sequences
    D_VectorDNA16 reference_sequences;
    // packed variant sequences
    D_VectorDNA16 variants;
    // an index for both references and variants
    D_Vector<from_nvbio::SNP_sequence_index> ref_variant_index;

    void load(const SNPDatabase_refIDs& ref);

    struct view
    {
        D_VectorU32::view variant_sequence_ref_ids;
        D_VectorU32::view genome_start_positions;
        D_VectorU32::view genome_stop_positions;
        D_VectorU32_2::view sequence_positions;
        D_VectorDNA16::view reference_sequences;
        D_VectorDNA16::view variants;
        D_Vector<from_nvbio::SNP_sequence_index>::view ref_variant_index;
    };

    struct const_view
    {
        D_VectorU32::const_view variant_sequence_ref_ids;
        D_VectorU32::const_view genome_start_positions;
        D_VectorU32::const_view genome_stop_positions;
        D_VectorU32_2::const_view sequence_positions;
        D_VectorDNA16::const_view reference_sequences;
        D_VectorDNA16::const_view variants;
        D_Vector<from_nvbio::SNP_sequence_index>::const_view ref_variant_index;
    };

    operator view()
    {
        view v = {
            variant_sequence_ref_ids,
            genome_start_positions,
            genome_stop_positions,
            sequence_positions,
            reference_sequences,
            variants,
            ref_variant_index,
        };

        return v;
    }

    operator const_view() const
    {
        const_view v = {
            variant_sequence_ref_ids,
            genome_start_positions,
            genome_stop_positions,
            sequence_positions,
            reference_sequences,
            variants,
            ref_variant_index,
        };

        return v;
    }
};

void build_read_offset_list(bqsr_context *context,
                            const alignment_batch& batch);

void build_alignment_windows(bqsr_context *ctx,
                             const alignment_batch& batch);

void filter_known_snps(bqsr_context *context,
                       const alignment_batch& batch);