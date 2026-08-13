#!/usr/bin/env python3
"""List SF2 presets (bank/program/name)."""
import struct
import sys

def read_chunks(data, start, end):
    """Yield (id, payload_start, payload_size) for chunks in [start, end)."""
    pos = start
    while pos + 8 <= end:
        id_ = data[pos:pos+4]
        size = struct.unpack_from('<I', data, pos+4)[0]
        payload_start = pos + 8
        payload_end = payload_start + size
        if payload_end > end:
            break
        yield id_, payload_start, size
        pos = payload_end
        if size % 2:
            pos += 1

def read_sf2_presets(path):
    with open(path, 'rb') as f:
        data = f.read()

    if data[:4] != b'RIFF' or data[8:12] != b'sfbk':
        raise ValueError('not an SF2 file')

    # top-level chunks inside RIFF/sfbk
    top_start = 12
    top_end = 8 + struct.unpack_from('<I', data, 4)[0]

    pdta_start = pdta_size = None
    for id_, pstart, psize in read_chunks(data, top_start, top_end):
        if id_ == b'LIST':
            list_type = data[pstart:pstart+4]
            if list_type == b'pdta':
                pdta_start, pdta_size = pstart + 4, psize - 4
                break
    if pdta_start is None:
        raise ValueError('pdta not found')

    phdr_start = phdr_size = None
    for id_, pstart, psize in read_chunks(data, pdta_start, pdta_start + pdta_size):
        if id_ == b'phdr':
            phdr_start, phdr_size = pstart, psize
            break
    if phdr_start is None:
        raise ValueError('phdr not found')

    presets = []
    count = phdr_size // 38
    for i in range(count):
        off = phdr_start + i * 38
        name = data[off:off+20].split(b'\x00', 1)[0].decode('latin-1')
        preset = struct.unpack_from('<H', data, off + 20)[0]
        bank = struct.unpack_from('<H', data, off + 22)[0]
        presets.append((bank, preset, name))
    return presets

if __name__ == '__main__':
    path = sys.argv[1] if len(sys.argv) > 1 else 'spiffs_image/soundfonts/default.sf2'
    presets = read_sf2_presets(path)
    for bank, prog, name in presets:
        print(f'bank={bank:3d} program={prog:3d} name="{name}"')
