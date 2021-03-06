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

#include "bit_packers/read_group.h"
#include "bit_packers/quality_score.h"
#include "bit_packers/event_tracker.h"

#include "../../table_formatter.h"

namespace firepony {

// defines a covariate chain equivalent to GATK's RecalTable1
template <target_system system>
struct covariate_packer_quality_score
{
    // the type that represents the chain of covariates
    typedef covariate_ReadGroup<system,
             covariate_QualityScore<system,
              covariate_EventTracker<system> > > chain;

    // the index of each covariate in the chain
    // (used when decoding a key)
    // the order is defined by the typedef above
    typedef enum {
        ReadGroup = 3,
        QualityScore = 2,
        EventTracker = 1,

        // target covariate is mostly meaningless for recaltable1
        TargetCovariate = QualityScore,
    } CovariateID;

    // extract a given covariate value from a key
    static CUDA_HOST_DEVICE uint32 decode(covariate_key key, CovariateID id)
    {
        return chain::decode(key, id);
    }

    static void dump_table_loop(firepony_context<system>& context, covariate_empirical_table<host>& table, table_formatter& fmt)
    {
        for(uint32 i = 0; i < table.size(); i++)
        {
            // skip null entries in the table
            if (table.values[i].observations == 0)
                continue;

            uint32 rg_id = decode(table.keys[i], ReadGroup);
            const std::string& rg_name = context.bam_header.host.read_groups_db.lookup(rg_id);

            const char ev = cigar_event::ascii(decode(table.keys[i], EventTracker));
            const covariate_empirical_value& val = table.values[i];

            const uint8 qual = decode(table.keys[i], QualityScore);
            char qual_str[256];
            snprintf(qual_str, sizeof(qual_str), "%d", qual);

            fmt.start_row();

            fmt.data(rg_name);
            fmt.data(std::string(qual_str));
            fmt.data(ev);
            fmt.data(val.empirical_quality);
            fmt.data(val.observations);
            fmt.data(val.mismatches);

            fmt.end_row();
        }
    }

    static void dump_table(firepony_context<system>& context, covariate_empirical_table<system>& d_table)
    {
        covariate_empirical_table<host> table;
        table.copyfrom(d_table);

        table_formatter fmt("RecalTable1");
        fmt.add_column("ReadGroup", table_formatter::FMT_STRING);
        // for some very odd reason, GATK outputs this as a string
        fmt.add_column("QualityScore", table_formatter::FMT_STRING);
        fmt.add_column("EventType", table_formatter::FMT_CHAR);
        fmt.add_column("EmpiricalQuality", table_formatter::FMT_FLOAT_4);
        fmt.add_column("Observations", table_formatter::FMT_UINT64);
        fmt.add_column("Errors", table_formatter::FMT_FLOAT_2, table_formatter::ALIGNMENT_RIGHT, table_formatter::ALIGNMENT_LEFT);

        // preprocess table data to compute column widths
        dump_table_loop(context, table, fmt);
        fmt.end_table();

        // output table
        dump_table_loop(context, table, fmt);
        fmt.end_table();
    }
};

} // namespace firepony
