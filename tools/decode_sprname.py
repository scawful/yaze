#!/usr/bin/env python3
"""Decode hmagic sprname.dat standard format into JSON.

Format (standard):
- byte 0: flag (0 = standard, non-zero = alt)
- bytes 1-4: little-endian size of payload
- payload: 0x11c entries, each [len (1 byte), name bytes]
Alt format not implemented here (would be 256 entries of 9 bytes each if flag!=0).
"""
import json
import struct
import sys
from pathlib import Path


def decode_standard(data: bytes):
    if len(data) < 5:
        raise ValueError("file too small")
    flag = data[0]
    size = struct.unpack_from('<I', data, 1)[0]
    buf = data[5:5 + size]
    if flag != 0:
        raise ValueError("non-zero flag: alt format not handled here")
    names = []
    pos = 0
    for i in range(0x11C):
        if pos >= len(buf):
            raise ValueError(f"truncated at entry {i}")
        name_len = buf[pos]
        pos += 1
        if name_len > 15:
            raise ValueError(f"name too long at entry {i}: {name_len}")
        if pos + name_len > len(buf):
            raise ValueError(f"truncated payload at entry {i}")
        name = buf[pos:pos + name_len].decode('latin1')
        pos += name_len
        names.append(name)
    return names


def main():
    if len(sys.argv) != 2:
        print("Usage: decode_sprname.py <sprname.dat>")
        sys.exit(1)
    path = Path(sys.argv[1])
    data = path.read_bytes()
    names = decode_standard(data)
    print(json.dumps(names, indent=2))


if __name__ == "__main__":
    main()
