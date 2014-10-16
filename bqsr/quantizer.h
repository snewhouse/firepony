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
#include "covariates_table.h"

struct covariate_empirical_value
{
    uint32 observations;
    double mismatches;
    double expected_errors;
    double estimated_quality;
    double empirical_quality;
};

struct quantizer_context
{
    // read group table
    D_Vector<covariate_key> read_group_keys;
    D_Vector<covariate_empirical_value> read_group_values;

    struct view
    {
        D_Vector<covariate_key>::view read_group_keys;
        D_Vector<covariate_empirical_value>::view read_group_values;
    };

    operator view()
    {
        view v = {
            read_group_keys,
            read_group_values,
        };

        return v;
    }
};

void build_read_group_table(bqsr_context *context);
void output_read_group_table(bqsr_context *context);
