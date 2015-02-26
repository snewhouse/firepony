/*
 * Firepony
 * Copyright (c) 2014-2015, NVIDIA CORPORATION. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *    * Redistributions of source code must retain the above copyright
 *      notice, this list of conditions and the following disclaimer.
 *    * Redistributions in binary form must reproduce the above copyright
 *      notice, this list of conditions and the following disclaimer in the
 *      documentation and/or other materials provided with the distribution.
 *    * Neither the name of the NVIDIA CORPORATION nor the
 *      names of its contributors may be used to endorse or promote products
 *      derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL NVIDIA CORPORATION BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
 * SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#pragma once

#include "../types.h"
#include "../runtime_options.h"

#include "alignment_data_device.h"
#include "sequence_data_device.h"
#include "variant_data_device.h"
#include "snp_filter.h"
#include "covariates.h"
#include "cigar.h"
#include "baq.h"
#include "fractional_errors.h"
#include "util.h"

#include "device/primitives/timer.h"

namespace firepony {

struct pipeline_statistics // host-only
{
    uint64 total_reads;        // total number of reads processed
    uint64 filtered_reads;     // number of reads filtered out in pre-processing
    uint64 baq_reads;          // number of reads for which BAQ was computed
    uint64 num_batches;        // number of batches processed

    time_series io;
    time_series read_filter;
    time_series snp_filter;
    time_series bp_filter;
    time_series cigar_expansion;
    time_series baq;
    time_series fractional_error;
    time_series covariates;

    time_series baq_setup;
    time_series baq_hmm;
    time_series baq_postprocess;

    time_series covariates_gather;
    time_series covariates_filter;
    time_series covariates_sort;
    time_series covariates_pack;

    time_series postprocessing;
    time_series output;

    pipeline_statistics()
        : total_reads(0),
          filtered_reads(0),
          baq_reads(0),
          num_batches(0)
    { }

    pipeline_statistics& operator+=(const pipeline_statistics& other)
    {
        total_reads += other.total_reads;
        filtered_reads += other.filtered_reads;
        baq_reads += other.baq_reads;
        num_batches += other.num_batches;

        io += other.io;
        read_filter += other.read_filter;
        snp_filter += other.snp_filter;
        bp_filter += other.bp_filter;
        cigar_expansion += other.cigar_expansion;
        baq += other.baq;
        fractional_error += other.fractional_error;
        covariates += other.covariates;

        baq_setup += other.baq_setup;
        baq_hmm += other.baq_hmm;
        baq_postprocess += other.baq_postprocess;

        covariates_gather += other.covariates_gather;
        covariates_filter += other.covariates_filter;
        covariates_sort += other.covariates_sort;
        covariates_pack += other.covariates_pack;

        postprocessing += other.postprocessing;
        output += other.output;

        return *this;
    }
};

template <target_system system>
struct firepony_context
{
    // identifies the compute device we're using on this context
    // note that the meaning depends on the target system
    const int compute_device;

    const runtime_options& options;

    const alignment_header<system>& bam_header;
#if 0
    const variant_database<system>& variant_db;
#endif
    const sequence_data<system>& reference;

    // sorted list of active reads
    d_vector_u32<system> active_read_list;
    // alignment windows for each read in reference coordinates
    d_vector_u32_2<system> alignment_windows;
    // alignment windows for each read in local sequence coordinates
    d_vector_u16_2<system> sequence_alignment_windows;

    // list of active BP locations
    d_vector_active_location_list<system> active_location_list;
    // list of read offsets in the reference for each BP (relative to the alignment start position)
    d_vector_u16<system> read_offset_list;

    // temporary storage for CUB calls
    d_vector_u8<system> temp_storage;

    // and more temporary storage
    d_vector_u32<system> temp_u32;
    d_vector_u32<system> temp_u32_2;
    d_vector_u32<system> temp_u32_3;
    d_vector_u32<system> temp_u32_4;
    d_vector_f32<system> temp_f32;
    d_vector_u8<system>  temp_u8;

    // various pipeline states go here
#if 0
    snp_filter_context<system> snp_filter;
#endif
    cigar_context<system> cigar;
    baq_context<system> baq;
    covariates_context<system> covariates;
    fractional_error_context<system> fractional_error;

    // --- everything below this line is host-only and not available on the device
    pipeline_statistics stats;

    firepony_context(const int compute_device,
                     const runtime_options& options,
                     const alignment_header<system>& bam_header,
                     const sequence_data<system>& reference
                     /* const variant_database<system>& variant_db */ )
        : compute_device(compute_device),
          options(options),
          bam_header(bam_header),
          reference(reference)
//          variant_db(variant_db)
    { }

    struct view
    {
        typename alignment_header_device<system>::const_view    bam_header;
#if 0
        typename variant_database_device<system>::const_view    variant_db;
#endif
        typename sequence_data_device<system>::const_view       reference;
        typename d_vector_u32<system>::view                       active_read_list;
        typename d_vector_u32_2<system>::view                     alignment_windows;
        typename d_vector_u16_2<system>::view                     sequence_alignment_windows;
        typename d_vector_active_location_list<system>::view      active_location_list;
        typename d_vector_u16<system>::view                       read_offset_list;
        typename d_vector_u8<system>::view                        temp_storage;
        typename d_vector_u32<system>::view                       temp_u32;
        typename d_vector_u32<system>::view                       temp_u32_2;
        typename d_vector_u32<system>::view                       temp_u32_3;
        typename d_vector_u32<system>::view                       temp_u32_4;
        typename d_vector_u8<system>::view                        temp_u8;
#if 0
        typename snp_filter_context<system>::view               snp_filter;
#endif
        typename cigar_context<system>::view                    cigar;
        typename baq_context<system>::view                      baq;
        typename covariates_context<system>::view               covariates;
        typename fractional_error_context<system>::view         fractional_error;
    };

    operator view()
    {
        view v = {
            bam_header.device,
#if 0
            variant_db.device,
#endif
            reference.device,
            active_read_list,
            alignment_windows,
            sequence_alignment_windows,
            active_location_list,
            read_offset_list,
            temp_storage,
            temp_u32,
            temp_u32_2,
            temp_u32_3,
            temp_u32_4,
            temp_u8,
#if 0
            snp_filter,
#endif
            cigar,
            baq,
            covariates,
            fractional_error,
        };

        return v;
    }

    void start_batch(const alignment_batch<system>& batch);
    void end_batch(const alignment_batch<system>& batch);
};

// encapsulates common state for our thrust functors to save a little typing
template <target_system system>
struct lambda
{
    typename firepony_context<system>::view ctx;
    const typename alignment_batch_device<system>::const_view batch;

    lambda(typename firepony_context<system>::view ctx,
           const typename alignment_batch_device<system>::const_view batch)
        : ctx(ctx),
          batch(batch)
    { }
};
#define LAMBDA_INHERIT_MEMBERS using lambda<system>::ctx; using lambda<system>::batch
#define LAMBDA_INHERIT using lambda<system>::lambda; LAMBDA_INHERIT_MEMBERS

template <target_system system>
struct lambda_context
{
    typename firepony_context<system>::view ctx;

    lambda_context(typename firepony_context<system>::view ctx)
        : ctx(ctx)
    { }
};
#define LAMBDA_CONTEXT_INHERIT_MEMBERS using lambda_context<system>::ctx
#define LAMBDA_CONTEXT_INHERIT using lambda_context<system>::lambda_context; LAMBDA_CONTEXT_INHERIT_MEMBERS

} // namespace firepony

