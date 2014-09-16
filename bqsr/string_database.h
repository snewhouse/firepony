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

#include <vector>
#include <string>
#include <map>

// utility struct to keep track of string identifiers using integers
struct string_database
{
    // returns the id of string if it exists in the database, otherwise returns -1
    uint32 lookup(const std::string& string) const;
    // returns the string corresponding to the given integer id
    const std::string& lookup(uint32 id) const;

    // inserts a string into the database, returning the new ID
    // if string already exists, returns the existing ID
    uint32 insert(const std::string& string);

    // computes a hash of a string
    static uint32 hash(const char* s);
    static uint32 hash(const std::string& s);

private:
    std::vector<std::string> string_identifiers;
    std::map<uint32, uint32> string_hash_to_id;
};
