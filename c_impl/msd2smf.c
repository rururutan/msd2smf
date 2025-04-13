/*
 * msd2smf.h - Convert MSD to MIDI (SMF) format
 * Copyright (C) 2025  Ru^3
 *
 * Based on source code: msd2smf.rb
 * Copyright (C) 2007  Silver Hirame
 *
 * Converts the MSD format - used in F&C Windows games - to a standard MIDI file (format 0).
 * This file is licensed under the MIT License.
 */

#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

#define MSD_MAGIC "WMSD"
#define MSD_HEADER_SIZE 0x14
#define DEFAULT_TRACK_ALLOC 65536

static uint32_t read_le32(const uint8_t* p) {
    return p[0] | (p[1] << 8) | (p[2] << 16) | (p[3] << 24);
}

static uint16_t to_be16(const uint16_t val) {
    return (val >> 8) | (val << 8);
}

static uint32_t to_be32(const uint32_t val) {
    return ((val >> 24) & 0xFF) | ((val >> 8) & 0xFF00) |
           ((val << 8) & 0xFF0000) | ((val << 24) & 0xFF000000);
}

// Write variable-length quantity
static int write_vlq(uint32_t value, uint8_t* out) {
    int len = 0;
    uint8_t buf[4] = {0};
    buf[3] = value & 0x7F;
    len = 1;
    while ((value >>= 7)) {
        buf[3 - len] = 0x80 | (value & 0x7F);
        len++;
    }
    memcpy(out, &buf[4 - len], len);
    return len;
}

// Write meta event
static int write_meta_event(uint8_t* out, uint32_t delta, uint8_t type, const uint8_t* data, uint32_t len) {
    int pos = 0;
    pos += write_vlq(delta, out + pos);
    out[pos++] = 0xFF;
    out[pos++] = type;
    pos += write_vlq(len, out + pos);
    if (len && data) memcpy(out + pos, data, len);
    return pos + len;
}

// Write short MIDI message
static int write_short_message(uint8_t* out, uint32_t delta, const uint8_t* msg, int len) {
    int pos = 0;
    pos += write_vlq(delta, out + pos);
    memcpy(out + pos, msg, len);
    return pos + len;
}

// Write SysEx event
static int write_sysex_event(uint8_t* out, uint32_t delta, const uint8_t* data, uint32_t len) {
    int pos = 0;
    pos += write_vlq(delta, out + pos);
    out[pos++] = 0xF0;
    pos += write_vlq(len - 1, out + pos);
    memcpy(out + pos, data + 1, len - 1);
    return pos + len - 1;
}

// Get MIDI message length by command
static int midi_cmd_len(uint8_t status) {
    static const uint8_t len_table[8] = {3, 3, 2, 3, 2, 2, 3, 0};
    return len_table[(status >> 4) & 0x7];
}

int convert_msd_to_smf(const uint8_t* msd, size_t size, uint8_t* out_buff, size_t* out_size, int flag) {
    if (size < MSD_HEADER_SIZE || memcmp(msd, MSD_MAGIC, 4) != 0) return -1;

    uint32_t timebase = read_le32(msd + 4);
    uint32_t packet_count = read_le32(msd + 0x10);

    const uint8_t* ptr = msd + MSD_HEADER_SIZE;
    const uint8_t* end = msd + size;

    // The converted size should be at most twice the size.
    size_t track_alloc = (size * 2 > DEFAULT_TRACK_ALLOC) ? size * 2 : DEFAULT_TRACK_ALLOC;
    uint8_t* track = (uint8_t*)malloc(track_alloc);
    if (!track) return -2;

    size_t track_len = 0;
    uint32_t delta_time = 0;
    int loop_started = 0;

    uint32_t* nid_list = (uint32_t*)malloc(sizeof(uint32_t) * packet_count);
    if (!nid_list) { free(track); return -3; }

    const uint8_t* chk_ptr = ptr;
    for (uint32_t i = 0; i < packet_count && chk_ptr + 16 <= end; ++i) {
        uint32_t nid = read_le32(chk_ptr + 4);
        uint32_t len = read_le32(chk_ptr + 12);
        nid_list[i] = nid;
        chk_ptr += 16;
        if (chk_ptr + len > end) break;
        chk_ptr += (len + 3) & ~3;
    }

    for (uint32_t i = 0; i < packet_count && ptr + 16 <= end; ++i) {
        uint32_t pid = read_le32(ptr);
        //uint32_t nid = read_le32(ptr + 4);
        uint32_t len = read_le32(ptr + 12);
        ptr += 16;

        if (ptr + len > end) break;

        const uint8_t* payload = ptr;
        ptr += (len + 3) & ~3;

        if (pid == nid_list[packet_count - 1] && !loop_started) {
            // Loop start marker
            if (flag == 0) {
                // Meta event loopStart
                uint8_t meta[32];
                int mlen = write_meta_event(meta, delta_time, 0x06, (const uint8_t*)"loopStart", 9);
                memcpy(track + track_len, meta, mlen);
                track_len += mlen;
            } else if (flag == 1) {
                // CC111 event: Bn 6F xx (channel 0, CC#111, value 0)
                const uint8_t msg[3] = { 0xB0, 0x6F, 0x00 };
                int mlen = write_short_message(track + track_len, delta_time, msg, 3);
                track_len += mlen;
            }
            delta_time = 0;
            loop_started = 1;
        }

        size_t offset = 0;
        while (offset + 12 <= len) {
            const uint8_t* ev = payload + offset;
            uint32_t delta = read_le32(ev);
            delta_time += delta;
            uint32_t param = read_le32(ev + 8);
            uint8_t type = ev[11] & 0xBF;

            if (type == 0 && ev[8] != 0xFF) {
                int msglen = midi_cmd_len(ev[8]);
                if (msglen > 0) {
                    uint8_t tmp[16] = {0};
                    int wlen = write_short_message(tmp, delta_time, ev + 8, msglen);
                    memcpy(track + track_len, tmp, wlen);
                    track_len += wlen;
                    delta_time = 0;
                }
            } else if (type == 1) {
                uint8_t tempo[3] = { ev[10], ev[9], ev[8] };
                uint8_t tmp[16] = {0};
                int wlen = write_meta_event(tmp, delta_time, 0x51, tempo, 3);
                memcpy(track + track_len, tmp, wlen);
                track_len += wlen;
                delta_time = 0;
            } else if (type == 0x80) {
                uint32_t sysex_len = param & 0xFFFFFF;
                const uint8_t* sysex = payload + offset + 12;
                if (offset + 12 + sysex_len <= len) {
                    uint8_t tmp[1024];
                    int wlen = write_sysex_event(tmp, delta_time, sysex, sysex_len);
                    memcpy(track + track_len, tmp, wlen);
                    track_len += wlen;
                    delta_time = 0;
                    offset += ((sysex_len + 3) & ~3);
                } else {
                    break;
                }
            } else if (ev[11] & 0x80) {
                uint32_t skip_len = param & 0xFFFFFF;
                offset += ((skip_len + 3) & ~3);
                continue;
            }

            offset += 12;
        }
    }

    // Loop end marker
    if (loop_started && flag == 0) {
        uint8_t tmp[32];
        int mlen = write_meta_event(tmp, delta_time, 0x06, (const uint8_t*)"loopEnd", 7);
        memcpy(track + track_len, tmp, mlen);
        track_len += mlen;
        delta_time = 0;
    }

    // End of track
    uint8_t tmp[16];
    int mlen = write_meta_event(tmp, delta_time, 0x2F, NULL, 0);
    memcpy(track + track_len, tmp, mlen);
    track_len += mlen;

    // SMF header + track chunk
    size_t smf_size = 14 + 8 + track_len;

    if (out_buff == NULL || *out_size < smf_size) {
        free(nid_list);
        free(track);
        return -4;  // buffer too small
    }

    uint8_t* p = out_buff;

    memcpy(p, "MThd", 4); p += 4;
    *(uint32_t*)p = to_be32(6); p += 4;
    *(uint16_t*)p = to_be16(0); p += 2;
    *(uint16_t*)p = to_be16(1); p += 2;
    *(uint16_t*)p = to_be16((uint16_t)timebase); p += 2;

    memcpy(p, "MTrk", 4); p += 4;
    *(uint32_t*)p = to_be32((uint32_t)track_len); p += 4;
    memcpy(p, track, track_len);

    free(nid_list);
    free(track);
    if (out_size) *out_size = smf_size;
    return 0;
}
