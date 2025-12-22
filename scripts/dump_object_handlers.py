#!/usr/bin/env python3
"""
Dump ALTTP Dungeon Object Handler Tables

This script reads the dungeon object handler tables from ROM and dumps:
1. Handler addresses for Type 1, 2, and 3 objects
2. First 20 Type 1 handler addresses
3. Handler routine analysis

Based on ALTTP ROM structure:
- Type 1 handler table: Bank $01, $8200 (objects 0x00-0xFF)
- Type 2 handler table: Bank $01, $8470 (objects 0x100-0x1FF)
- Type 3 handler table: Bank $01, $85F0 (objects 0x200-0x2FF)

Each entry is a 16-bit pointer (little-endian) to a handler routine in Bank $01.
"""

import sys
import struct
from pathlib import Path


def read_rom(rom_path):
    """Read ROM file and return data, skipping SMC header if present."""
    with open(rom_path, 'rb') as f:
        data = f.read()

    # Check for SMC header (512 bytes)
    if len(data) % 0x400 == 0x200:
        print(f"[INFO] SMC header detected, skipping 512 bytes")
        return data[0x200:]
    return data


def pc_to_snes(pc_addr):
    """Convert PC address to SNES $01:xxxx format."""
    # For LoROM, PC address maps to SNES as:
    # PC 0x00000-0x7FFF -> $00:8000-$00:FFFF
    # PC 0x08000-0x0FFFF -> $01:8000-$01:FFFF
    bank = (pc_addr >> 15) & 0xFF
    offset = (pc_addr & 0x7FFF) | 0x8000
    return f"${bank:02X}:{offset:04X}"


def snes_to_pc(bank, offset):
    """Convert SNES address to PC address (LoROM mapping)."""
    # Bank $01, offset $8000-$FFFF -> PC 0x08000 + (offset - 0x8000)
    if offset < 0x8000:
        raise ValueError(f"Invalid offset ${offset:04X}, must be >= $8000")
    return (bank * 0x8000) + (offset - 0x8000)


def dump_handler_table(rom_data, bank, start_offset, count, name):
    """
    Dump handler table from ROM.

    Args:
        rom_data: ROM data bytes
        bank: SNES bank number
        start_offset: SNES offset in bank
        count: Number of entries to read
        name: Table name for display

    Returns:
        List of handler addresses (as integers)
    """
    pc_addr = snes_to_pc(bank, start_offset)
    print(f"\n{'='*70}")
    print(f"{name}")
    print(f"SNES Address: ${bank:02X}:{start_offset:04X}")
    print(f"PC Address: 0x{pc_addr:06X}")
    print(f"{'='*70}")

    handlers = []
    for i in range(count):
        entry_pc = pc_addr + (i * 2)
        if entry_pc + 1 >= len(rom_data):
            print(f"[ERROR] PC address 0x{entry_pc:06X} out of bounds")
            break

        # Read 16-bit little-endian pointer
        handler_offset = struct.unpack_from('<H', rom_data, entry_pc)[0]
        handlers.append(handler_offset)

        # Convert to full SNES address (same bank)
        handler_snes = f"${bank:02X}:{handler_offset:04X}"

        # Only print first 20 for Type 1
        if i < 20 or name != "Type 1 Handler Table":
            print(f"  Object 0x{i:03X}: {handler_snes} (PC: 0x{snes_to_pc(bank, handler_offset):06X})")

    if name == "Type 1 Handler Table" and count > 20:
        print(f"  ... ({count - 20} more entries)")

    return handlers


def analyze_handler_uniqueness(handlers, name):
    """Analyze how many unique handlers exist."""
    unique_handlers = set(handlers)
    print(f"\n[ANALYSIS] {name}:")
    print(f"  Total objects: {len(handlers)}")
    print(f"  Unique handlers: {len(unique_handlers)}")
    print(f"  Shared handlers: {len(handlers) - len(unique_handlers)}")

    # Find most common handlers
    from collections import Counter
    handler_counts = Counter(handlers)
    most_common = handler_counts.most_common(5)
    print(f"  Most common handlers:")
    for handler_offset, count in most_common:
        print(f"    ${handler_offset:04X}: used by {count} objects")


def dump_handler_bytes(rom_data, bank, handler_offset, byte_count=32):
    """Dump first N bytes of a handler routine."""
    try:
        pc_addr = snes_to_pc(bank, handler_offset)
        if pc_addr + byte_count >= len(rom_data):
            byte_count = len(rom_data) - pc_addr

        handler_bytes = rom_data[pc_addr:pc_addr + byte_count]
        print(f"\n[HANDLER DUMP] ${bank:02X}:{handler_offset:04X} (PC: 0x{pc_addr:06X})")
        print(f"  First {byte_count} bytes:")

        # Print in hex rows of 16 bytes
        for i in range(0, byte_count, 16):
            row = handler_bytes[i:i+16]
            hex_str = ' '.join(f'{b:02X}' for b in row)
            ascii_str = ''.join(chr(b) if 32 <= b < 127 else '.' for b in row)
            print(f"    {i:04X}: {hex_str:<48} {ascii_str}")
    except ValueError as e:
        print(f"[ERROR] {e}")


def main():
    if len(sys.argv) < 2:
        print("Usage: python3 dump_object_handlers.py <rom_path>")
        print("Example: python3 dump_object_handlers.py zelda3.sfc")
        sys.exit(1)

    rom_path = Path(sys.argv[1])
    if not rom_path.exists():
        print(f"[ERROR] ROM file not found: {rom_path}")
        sys.exit(1)

    print(f"[INFO] Reading ROM: {rom_path}")
    rom_data = read_rom(rom_path)
    print(f"[INFO] ROM size: {len(rom_data)} bytes ({len(rom_data) / 1024 / 1024:.2f} MB)")

    # Dump handler tables
    type1_handlers = dump_handler_table(rom_data, 0x01, 0x8200, 256, "Type 1 Handler Table")
    type2_handlers = dump_handler_table(rom_data, 0x01, 0x8470, 64, "Type 2 Handler Table")
    type3_handlers = dump_handler_table(rom_data, 0x01, 0x85F0, 128, "Type 3 Handler Table")

    # Analyze handler distribution
    analyze_handler_uniqueness(type1_handlers, "Type 1")
    analyze_handler_uniqueness(type2_handlers, "Type 2")
    analyze_handler_uniqueness(type3_handlers, "Type 3")

    # Dump first handler (object 0x00)
    if type1_handlers:
        print(f"\n{'='*70}")
        print(f"INVESTIGATING OBJECT 0x00 HANDLER")
        print(f"{'='*70}")
        dump_handler_bytes(rom_data, 0x01, type1_handlers[0], 64)

    # Dump a few more common handlers
    print(f"\n{'='*70}")
    print(f"SAMPLE HANDLER DUMPS")
    print(f"{'='*70}")

    # Object 0x01 (common wall object)
    if len(type1_handlers) > 1:
        dump_handler_bytes(rom_data, 0x01, type1_handlers[1], 32)

    # Type 2 first handler
    if type2_handlers:
        dump_handler_bytes(rom_data, 0x01, type2_handlers[0], 32)

    print(f"\n{'='*70}")
    print(f"SUMMARY")
    print(f"{'='*70}")
    print(f"Handler tables successfully read from ROM.")
    print(f"See documentation at docs/internal/alttp-object-handlers.md")


if __name__ == '__main__':
    main()
