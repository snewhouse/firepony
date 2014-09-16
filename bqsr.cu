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
#include <nvbio/basic/vector.h>
#include <nvbio/basic/dna.h>
#include <nvbio/io/sequence/sequence.h>
#include <nvbio/io/vcf.h>
#include <nvbio/io/sequence/sequence_pac.h>

#include <map>

#include "bam_loader.h"
#include "reference.h"
#include "util.h"
#include "variants.h"
#include "bqsr_context.h"
#include "filters.h"

using namespace nvbio;

/*
// sort batch by increasing alignment position
void device_sort_batch(BAM_alignment_batch_device *batch)
{
    D_VectorU32 temp_pos = batch->alignment_positions;

    thrust::sort_by_key(temp_pos.begin(),
                        temp_pos.begin() + temp_pos.size(),
                        batch->read_order.begin());
}
*/


int main(int argc, char **argv)
{
    // load the reference genome
    const char *ref_name = "hs37d5";
    const char *vcf_name = "/home/nsubtil/hg96/ALL.chr20.integrated_phase1_v3.20101123.snps_indels_svs.genotypes-stripped.vcf";
    //const char *vcf_name = "/home/nsubtil/hg96/ALL.chr20.integrated_phase1_v3.20101123.snps_indels_svs.genotypes.vcf";
    //const char *vcf_name = "/home/nsubtil/hg96/one-variant.vcf";
    const char *bam_name = "/home/nsubtil/hg96/HG00096.chrom20.ILLUMINA.bwa.GBR.low_coverage.20120522.bam";
    //const char *bam_name = "/home/nsubtil/hg96/one-read.bam";

    struct reference_genome genome;

    printf("loading reference %s...\n", ref_name);

    if (genome.load(ref_name) == false)
    {
        printf("failed to load reference %s\n", ref_name);
        exit(1);
    }

    genome.download();

    SNPDatabase_refIDs db;
    printf("loading variant database %s...\n", vcf_name);
    io::loadVCF(db, vcf_name);
    db.compute_sequence_offsets(genome);

    DeviceSNPDatabase dev_db;
    dev_db.load(db);


    printf("%lu variants\n", db.genome_start_positions.size());
    printf("reading BAM %s...\n", bam_name);

    BAMfile bam(bam_name);

    BAM_alignment_batch_host h_batch;
    BAM_alignment_batch_device batch;

    bqsr_context ctx(bam.header, dev_db, genome.device);

    while(bam.next_batch(&h_batch, true, 2000000))
    {
        // load the next batch on the device
        batch.load(h_batch);
        ctx.start_batch(batch);

        // build read offset list
        build_read_offset_list(&ctx, batch);
        // build read alignment window list
        build_alignment_windows(&ctx, batch);

        // apply read filters
        filter_reads(&ctx, batch);

        // filter known SNPs from active_loc_list
        filter_snps(&ctx, batch);

#if 0
        H_ReadOffsetList h_read_offset_list = ctx.snp_filter.read_offset_list;
        H_VectorU2 h_alignment_windows = ctx.alignment_windows;
        H_VectorU2 h_vcf_ranges = ctx.snp_filter.active_vcf_ranges;

        H_VectorU32 h_read_order = ctx.active_read_list;
        for(uint32 read_id = 0; read_id < 50 && read_id < h_read_order.size(); read_id++)
        {
            io::SequenceDataView view = plain_view(*genome.h_ref);
            const uint32 read_index = h_read_order[read_id];

            const BAM_CRQ_index& idx = h_batch.crq_index[read_index];

            printf("read order %d read %d: cigar = [", read_id, read_index);
            for(uint32 i = idx.cigar_start; i < idx.cigar_start + idx.cigar_len; i++)
            {
                printf("%d%c", h_batch.cigars[i].len, h_batch.cigars[i].ascii_op());
            }
            printf("] offset list = [ ");

            for(uint32 i = idx.read_start; i < idx.read_start + idx.read_len; i++)
            {
                printf("%d ", h_read_offset_list[i]);
            }
            printf("]\n  sequence name [%s] sequence base [%u] sequence offset [%u] alignment window [%u, %u]\n",
                    &view.m_name_stream[view.m_name_index[h_batch.alignment_sequence_IDs[read_index]]],
                    genome.ref_sequence_offsets[h_batch.alignment_sequence_IDs[read_index]],
                    h_batch.alignment_positions[read_index],
                    h_alignment_windows[read_index].x,
                    h_alignment_windows[read_index].y);
            printf("  active VCF range: [%u, %u[\n", h_vcf_ranges[read_index].x, h_vcf_ranges[read_index].y);
        }
#endif

#if 0
        printf("active VCF ranges: %lu out of %lu reads (%f %%)\n",
                ctx.snp_filter.active_read_ids.size(),
                ctx.active_read_list.size(),
                100.0 * float(ctx.snp_filter.active_read_ids.size()) / ctx.active_read_list.size());

        H_ActiveLocationList h_bplist = ctx.snp_filter.active_location_list;
        uint32 zeros = 0;
        for(uint32 i = 0; i < h_bplist.size(); i++)
        {
            if (h_bplist[i] == 0)
                zeros++;
        }

        printf("active BPs: %u out of %u (%f %%)\n", h_bplist.size() - zeros, h_bplist.size(), 100.0 * float(h_bplist.size() - zeros) / float(h_bplist.size()));
#endif
    }

    printf("%d reads filtered out of %d (%f%%)\n", ctx.stats.filtered_reads, ctx.stats.total_reads, float(ctx.stats.filtered_reads) / float(ctx.stats.total_reads) * 100.0);

    return 0;
}
