/*
 * msd2smf.h - Convert MSD to MIDI (SMF) format
 * Copyright (C) 2025  Ru^3
 *
 * Based on source code: msd2smf.rb
 * Copyright (C) 2007  Silver Hirame
 *
 * This file is licensed under the MIT License.
 */
#ifndef MDS_TO_SMF_H_
#define MDS_TO_SMF_H_
#pragma once

// Convert MSD to SMF
//
// @param [in] msd_data Pointer of MSD data
// @param [in] msd_size MSD data size
// @param [in] smf_data Pointer of output buffer
// @param [in/out] smf_size in:output buffer size / out:write data size
// @param [in] flag Loop format 0:Meta event (like FF7 PC) / 1:CC111 (like RPG Maker)
// @return 0:success / other:fail
int convert_msd_to_smf(const uint8_t* msd_data, size_t msd_size, uint8_t* smf_buff, size_t* smf_size, int flag);

#endif
