# msd2smf.py - A converter from MSD (Used by IDES Windows games) format to standard MIDI (SMF) format
# This script is based on msd2smf.rb created by Silver-Hirame.
# 
# This script parses a binary MSD file, extracts timing and MIDI message-like data,
# and converts it into a valid Standard MIDI File (SMF). It supports multiple events
# including short MIDI messages, tempo changes, and SysEx events. Loop points are
# also marked using Meta Marker events.

import struct
import os
import sys
import glob

def to_smf_chunk(name, data):
    # Create an SMF chunk from a type name and data body.
    return name.encode("ascii") + struct.pack(">I", len(data)) + data

def to_smf_ber(value):
    # Convert an integer into a variable-length quantity (VLQ).
    if value is None:
        return b''
    buffer = bytearray()
    buffer.append(value & 0x7F)
    value >>= 7
    while value > 0:
        buffer.insert(0, (value & 0x7F) | 0x80)
        value >>= 7
    return bytes(buffer)

def to_smf_metaevent(event_type, data, deltatime):
    # Construct a MIDI Meta Event.
    result = to_smf_ber(deltatime) + b'\xff' + bytes([event_type])
    if data is None:
        result += to_smf_ber(0)
    else:
        result += to_smf_ber(len(data)) + data
    return result

def to_smf_sysex(data, deltatime):
    # Construct a MIDI System Exclusive (SysEx) Event.
    if data[0] == 0xF0:
        data = data[1:]
    result = to_smf_ber(deltatime) + b'\xF0' + to_smf_ber(len(data)) + data
    return result

def to_smf_shortmes(data, deltatime):
    # Construct a short MIDI message with delta time.
    return to_smf_ber(deltatime) + data

def to_smf(tracks, timebase):
    # Combine tracks into a single SMF file with header and track chunks.
    header = struct.pack('>hhh', 1 if len(tracks) > 1 else 0, len(tracks), timebase)
    result = to_smf_chunk('MThd', header)
    for track in tracks:
        result += to_smf_chunk('MTrk', track)
    return result

# Lookup table for MIDI message lengths based on the high nibble of the status byte
cmd_len_tbl = [3, 3, 2, 3, 2, 2, 3, 0]

def conv_mes_safe(data, initial_deltatime=0):
    # Safely convert a series of MSD-formatted messages to MIDI events.
    events = []
    offset = 0
    deltatime = initial_deltatime

    while offset + 12 <= len(data):
        chunk = data[offset:offset+12]
        offset += 12
        dtime, _, param = struct.unpack("<III", chunk)
        deltatime += dtime

        event_type = chunk[11] & 0xBF

        if event_type == 0 and chunk[8] != 0xFF:
            cmd = chunk[8]
            length = cmd_len_tbl[(cmd >> 4) & 7]
            msg = chunk[8:8+length]
            events.append(to_smf_shortmes(msg, deltatime))
            deltatime = 0

        elif event_type == 1:
            # Tempo change event (Meta 0x51)
            tempo = bytes([chunk[10], chunk[9], chunk[8]])
            events.append(to_smf_metaevent(0x51, tempo, deltatime))
            deltatime = 0

        elif event_type == 0x80:
            # SysEx event
            sysex_len = param & 0xFFFFFF
            if offset + sysex_len <= len(data):
                sysex_data = data[offset:offset+sysex_len]
                events.append(to_smf_sysex(sysex_data, deltatime))
                offset += (sysex_len + 3) & ~3
            deltatime = 0

        else:
            if chunk[11] & 0x80:
                # Skip over unknown or unused data block
                skip_len = param & 0xFFFFFF
                offset += (skip_len + 3) & ~3

    return b''.join(events), deltatime

def convert_msd_to_midi(msd_bytes):
    # Entry point to convert MSD binary data to a MIDI byte stream.
    if msd_bytes[:4] != b"WMSD":
        raise ValueError("Invalid MSD header")

    timebase = struct.unpack_from("<I", msd_bytes, 4)[0]
    packet_count = struct.unpack_from("<I", msd_bytes, 0x10)[0]

    offset = 0x14
    packets = []
    for _ in range(packet_count):
        pid, nid, _, length = struct.unpack_from("<IIII", msd_bytes, offset)
        offset += 16
        payload = msd_bytes[offset:offset+length]
        packets.append({"This ID": pid, "Next ID": nid, "Length": length, "Payload": payload})
        offset += (length + 3) & ~3

    track_data = []
    deltatime = 0
    loop = False

    for i, pkt in enumerate(packets):
        if pkt["This ID"] == packets[-1]["Next ID"]:
            # Loop start marker (Meta 0x06)
            track_data.append(to_smf_metaevent(0x06, b'loopstart', deltatime))
            deltatime = 0
            loop = True

        if pkt["Length"] == 0:
            track_data.append(b'')
        else:
            mes, deltatime = conv_mes_safe(pkt["Payload"], deltatime)
            track_data.append(mes)

    if loop:
        # Loop end marker (Meta 0x06)
        track_data.append(to_smf_metaevent(0x06, b'loopend', deltatime))
        deltatime = 0

    # End of track marker
    track_data.append(to_smf_metaevent(0x2f, None, deltatime))

    return to_smf([b''.join(track_data)], timebase)

def main():
    # Entry point for command-line usage
    if len(sys.argv) < 2:
        print("usage: msd2smf [path]")
        return

    base_path = sys.argv[1]
    pattern = os.path.join(base_path, "*.msd")
    files = glob.glob(pattern)

    if not files:
        print("no msd files found in:", base_path)
        return

    for i, file in enumerate(files, 1):
        try:
            with open(file, "rb") as f:
                msd_data = f.read()
            midi_data = convert_msd_to_midi(msd_data)
            midi_file = os.path.splitext(file)[0] + ".mid"
            with open(midi_file, "wb") as f:
                f.write(midi_data)
            print(f"{i}: {file} -> {midi_file} ... OK")
        except Exception as e:
            print(f"{i}: {file} ... ERROR: {e}")

if __name__ == "__main__":
    main()
