#!/usr/bin/env python3
"""
Dungeon Room Object Analyzer for ALTTP ROM Hacking.

This script parses room data from a Link to the Past ROM to understand which
objects are on each layer (BG1/BG2). Useful for debugging layer compositing
and understanding room structure.

Usage:
    python analyze_room.py [OPTIONS] [ROOM_IDS...]

Examples:
    python analyze_room.py 1                    # Analyze room 001
    python analyze_room.py 1 2 3                # Analyze rooms 001, 002, 003
    python analyze_room.py --range 0 10         # Analyze rooms 0-10
    python analyze_room.py --all                # Analyze all 296 rooms (summary only)
    python analyze_room.py 1 --json             # Output as JSON
    python analyze_room.py 1 --rom path/to.sfc  # Use specific ROM file
    python analyze_room.py --list-bg2           # List all rooms with BG2 overlay objects

Collision Offset Features:
    python analyze_room.py 0x27 --collision              # Show collision offsets
    python analyze_room.py 0x27 --collision --asm        # Output ASM format
    python analyze_room.py 0x27 --collision --filter-id 0xD9  # Filter by object ID
    python analyze_room.py 0x27 --collision --area       # Expand objects to full tile area
"""

import argparse
import json
import os
import struct
import sys
from typing import Dict, List, Optional, Tuple

# ROM addresses from dungeon_rom_addresses.h
ROOM_OBJECT_POINTER = 0x874C  # Object data pointer table
ROOM_HEADER_POINTER = 0xB5DD  # Room header pointer
NUMBER_OF_ROOMS = 296

# Default ROM path (relative to script location)
DEFAULT_ROM_PATHS = [
    "roms/alttp_vanilla.sfc",
    "../roms/alttp_vanilla.sfc",
    "roms/vanilla.sfc",
    "../roms/vanilla.sfc",
]

# Object descriptions - comprehensive list
OBJECT_DESCRIPTIONS = {
    # Type 1 Objects (0x00-0xFF)
    0x00: "Ceiling (2x2)",
    0x01: "Wall horizontal (2x4)",
    0x02: "Wall horizontal (2x4, variant)",
    0x03: "Diagonal wall NW->SE",
    0x04: "Diagonal wall NE->SW",
    0x05: "Pit horizontal (4x2)",
    0x06: "Pit vertical (2x4)",
    0x07: "Floor pattern",
    0x08: "Water edge",
    0x09: "Water edge variant",
    0x0A: "Conveyor belt",
    0x0B: "Conveyor belt variant",
    0x0C: "Diagonal acute",
    0x0D: "Diagonal acute variant",
    0x0E: "Pushable block",
    0x0F: "Rail",
    0x10: "Diagonal grave",
    0x11: "Diagonal grave variant",
    0x12: "Wall top edge",
    0x13: "Wall bottom edge",
    0x14: "Diagonal acute 2",
    0x15: "Diagonal acute 2 variant",
    0x16: "Wall pattern",
    0x17: "Wall pattern variant",
    0x18: "Diagonal grave 2",
    0x19: "Diagonal grave 2 variant",
    0x1A: "Inner corner NW",
    0x1B: "Inner corner NE",
    0x1C: "Diagonal acute 3",
    0x1D: "Diagonal acute 3 variant",
    0x1E: "Diagonal grave 3",
    0x1F: "Diagonal grave 3 variant",
    0x20: "Diagonal acute 4",
    
    0x21: "Floor edge 1x2",
    0x22: "Has edge 1x1",
    0x23: "Has edge 1x1 variant",
    0x24: "Has edge 1x1 variant 2",
    0x25: "Has edge 1x1 variant 3",
    0x26: "Has edge 1x1 variant 4",
    
    0x30: "Bottom corners 1x2",
    0x31: "Nothing A",
    0x32: "Nothing A",
    0x33: "Floor 4x4",
    0x34: "Solid 1x1",
    0x35: "Door switcher",
    0x36: "Decor 4x4",
    0x37: "Decor 4x4 variant",
    0x38: "Statue 2x3",
    0x39: "Pillar 2x4",
    0x3A: "Decor 4x3",
    0x3B: "Decor 4x3 variant",
    0x3C: "Doubled 2x2",
    0x3D: "Pillar 2x4 variant",
    0x3E: "Decor 2x2",
    
    0x47: "Waterfall",
    0x48: "Waterfall variant",
    0x49: "Floor tile 4x2",
    0x4A: "Floor tile 4x2 variant",
    0x4C: "Bar 4x3",
    0x4D: "Shelf 4x4",
    0x4E: "Shelf 4x4 variant",
    0x4F: "Shelf 4x4 variant 2",
    0x50: "Line 1x1",
    0x51: "Cannon hole 4x3",
    0x52: "Cannon hole 4x3 variant",
    
    0x60: "Wall vertical (2x2)",
    0x61: "Wall vertical (4x2)",
    0x62: "Wall vertical (4x2, variant)",
    0x63: "Diagonal wall NW->SE (vert)",
    0x64: "Diagonal wall NE->SW (vert)",
    0x65: "Decor 4x2",
    0x66: "Decor 4x2 variant",
    0x67: "Floor 2x2",
    0x68: "Floor 2x2 variant",
    0x69: "Has edge 1x1 (vert)",
    0x6A: "Edge 1x1",
    0x6B: "Edge 1x1 variant",
    0x6C: "Left corners 2x1",
    0x6D: "Right corners 2x1",
    
    0x70: "Floor 4x4 (vert)",
    0x71: "Solid 1x1 (vert)",
    0x72: "Nothing B",
    0x73: "Decor 4x4 (vert)",
    
    0x85: "Cannon hole 3x4",
    0x86: "Cannon hole 3x4 variant",
    0x87: "Pillar 2x4 (vert)",
    0x88: "Big rail 3x1",
    0x89: "Block 2x2",
    
    0xA0: "Diagonal ceiling TL",
    0xA1: "Diagonal ceiling BL",
    0xA2: "Diagonal ceiling TR",
    0xA3: "Diagonal ceiling BR",
    0xA4: "Big hole 4x4",
    0xA5: "Diagonal ceiling TL B",
    0xA6: "Diagonal ceiling BL B",
    0xA7: "Diagonal ceiling TR B",
    0xA8: "Diagonal ceiling BR B",
    
    0xC0: "Chest",
    0xC1: "Chest variant",
    0xC2: "Big chest",
    0xC3: "Big chest variant",
    0xC4: "Interroom stairs",
    0xC5: "Torch",
    0xC6: "Torch (variant)",
    
    0xE0: "Pot",
    0xE1: "Block",
    0xE2: "Pot variant",
    0xE3: "Block variant",
    0xE4: "Pot (skull)",
    0xE5: "Block (push any)",
    0xE6: "Skull pot",
    0xE7: "Big gray block",
    0xE8: "Spike block",
    0xE9: "Spike block variant",
    
    # Type 2 objects (0x100+)
    0x100: "Corner NW (concave)",
    0x101: "Corner NE (concave)",
    0x102: "Corner SW (concave)",
    0x103: "Corner SE (concave)",
    0x104: "Corner NW (convex)",
    0x105: "Corner NE (convex)",
    0x106: "Corner SW (convex)",
    0x107: "Corner SE (convex)",
    0x108: "4x4 Corner NW",
    0x109: "4x4 Corner NE",
    0x10A: "4x4 Corner SW",
    0x10B: "4x4 Corner SE",
    0x10C: "Corner piece NW",
    0x10D: "Corner piece NE",
    0x10E: "Corner piece SW",
    0x10F: "Corner piece SE",
    0x110: "Weird corner bottom NW",
    0x111: "Weird corner bottom NE",
    0x112: "Weird corner bottom SW",
    0x113: "Weird corner bottom SE",
    0x114: "Weird corner top NW",
    0x115: "Weird corner top NE",
    0x116: "Platform / Floor overlay",
    0x117: "Platform variant",
    0x118: "Statue / Pillar",
    0x119: "Statue / Pillar variant",
    0x11A: "Star tile switch",
    0x11B: "Star tile switch variant",
    0x11C: "Rail platform",
    0x11D: "Rail platform variant",
    0x11E: "Somaria platform",
    0x11F: "Somaria platform variant",
    0x120: "Stairs up (north)",
    0x121: "Stairs down (south)",
    0x122: "Stairs left",
    0x123: "Stairs right",
    0x124: "Spiral stairs up",
    0x125: "Spiral stairs down",
    0x126: "Sanctuary entrance",
    0x127: "Sanctuary entrance variant",
    0x128: "Hole/pit",
    0x129: "Hole/pit variant",
    0x12A: "Warp tile",
    0x12B: "Warp tile variant",
    0x12C: "Layer switch NW",
    0x12D: "Layer switch NE",
    0x12E: "Layer switch SW",
    0x12F: "Layer switch SE",
    0x130: "Light cone",
    0x131: "Light cone variant",
    0x132: "Floor switch",
    0x133: "Floor switch (heavy)",
    0x134: "Bombable floor",
    0x135: "Bombable floor variant",
    0x136: "Cracked floor",
    0x137: "Cracked floor variant",
    0x138: "Stairs inter-room",
    0x139: "Stairs inter-room variant",
    0x13A: "Stairs straight",
    0x13B: "Stairs straight variant",
    0x13C: "Eye switch",
    0x13D: "Eye switch variant",
    0x13E: "Crystal switch",
    0x13F: "Crystal switch variant",
}

# Draw routine names for detailed analysis
DRAW_ROUTINES = {
    0x01: "RoomDraw_Rightwards2x4_1to15or26",
    0x02: "RoomDraw_Rightwards2x4_1to15or26",
    0x03: "RoomDraw_Rightwards2x4_1to16_BothBG",
    0x04: "RoomDraw_Rightwards2x4_1to16_BothBG",
    0x33: "RoomDraw_Rightwards4x4_1to16",
    0x34: "RoomDraw_Rightwards1x1Solid_1to16_plus3",
    0x38: "RoomDraw_RightwardsStatue2x3spaced2_1to16",
    0x61: "RoomDraw_Downwards4x2_1to15or26",
    0x62: "RoomDraw_Downwards4x2_1to15or26",
    0x63: "RoomDraw_Downwards4x2_1to16_BothBG",
    0x64: "RoomDraw_Downwards4x2_1to16_BothBG",
    0x71: "RoomDraw_Downwards1x1Solid_1to16_plus3",
    0xA4: "RoomDraw_BigHole4x4_1to16",
    0xC6: "RoomDraw_Torch",
}


def snes_to_pc(snes_addr: int) -> int:
    """Convert SNES LoROM address to PC file offset."""
    bank = (snes_addr >> 16) & 0xFF
    addr = snes_addr & 0xFFFF
    
    if bank >= 0x80:
        bank -= 0x80
    
    if addr >= 0x8000:
        return (bank * 0x8000) + (addr - 0x8000)
    else:
        return snes_addr & 0x3FFFFF


def read_long(rom_data: bytes, offset: int) -> int:
    """Read a 24-bit little-endian long address."""
    return struct.unpack('<I', rom_data[offset:offset+3] + b'\x00')[0]


def decode_object(b1: int, b2: int, b3: int, layer: int) -> Dict:
    """Decode 3-byte object data into object properties."""
    obj = {
        'b1': b1, 'b2': b2, 'b3': b3,
        'layer': layer,
        'type': 1,
        'id': 0,
        'x': 0,
        'y': 0,
        'size': 0
    }
    
    # Type 2: 111111xx xxxxyyyy yyiiiiii
    if b1 >= 0xFC:
        obj['type'] = 2
        obj['id'] = (b3 & 0x3F) | 0x100
        obj['x'] = ((b2 & 0xF0) >> 4) | ((b1 & 0x03) << 4)
        obj['y'] = ((b2 & 0x0F) << 2) | ((b3 & 0xC0) >> 6)
        obj['size'] = 0
    # Type 3: xxxxxxii yyyyyyii 11111iii
    elif b3 >= 0xF8:
        obj['type'] = 3
        obj['id'] = (b3 << 4) | 0x80 | ((b2 & 0x03) << 2) | (b1 & 0x03)
        obj['x'] = (b1 & 0xFC) >> 2
        obj['y'] = (b2 & 0xFC) >> 2
        obj['size'] = ((b1 & 0x03) << 2) | (b2 & 0x03)
    # Type 1: xxxxxxss yyyyyyss iiiiiiii
    else:
        obj['type'] = 1
        obj['id'] = b3
        obj['x'] = (b1 & 0xFC) >> 2
        obj['y'] = (b2 & 0xFC) >> 2
        obj['size'] = ((b1 & 0x03) << 2) | (b2 & 0x03)
    
    return obj


def get_object_description(obj_id: int) -> str:
    """Return a human-readable description of an object ID."""
    return OBJECT_DESCRIPTIONS.get(obj_id, f"Object 0x{obj_id:03X}")


def get_draw_routine(obj_id: int) -> str:
    """Return the draw routine name for an object ID."""
    return DRAW_ROUTINES.get(obj_id, "")


# =============================================================================
# Collision Offset Functions
# =============================================================================

def calculate_collision_offset(x_tile: int, y_tile: int) -> int:
    """Calculate offset into $7F2000 collision map.

    Collision map is 64 bytes per row (64 tiles wide).
    Each position is 1 byte, but SNES uses 16-bit addressing.
    Formula: offset = (Y * 64) + X
    """
    return (y_tile * 64) + x_tile


def expand_object_area(obj: Dict) -> List[Tuple[int, int]]:
    """Expand object to full tile coverage based on size.

    Object 'size' field encodes dimensions differently per object type.
    Water/flood objects use size as horizontal span.
    Type 2 objects (0x100+) are typically fixed-size.
    """
    tiles = []
    x, y, size = obj['x'], obj['y'], obj['size']
    obj_id = obj['id']

    # Water/flood objects (0x0C9, 0x0D9, etc.) - horizontal span
    # Size encodes horizontal extent
    if obj_id in [0xC9, 0xD9, 0x0C9, 0x0D9]:
        # Size is the horizontal span (number of tiles - 1)
        for dx in range(size + 1):
            tiles.append((x + dx, y))

    # Floor 4x4 objects (0x33, 0x70)
    elif obj_id in [0x33, 0x70]:
        # 4x4 block, size adds to dimensions
        width = 4 + (size & 0x03)
        height = 4 + ((size >> 2) & 0x03)
        for dy in range(height):
            for dx in range(width):
                tiles.append((x + dx, y + dy))

    # Wall objects (size extends in one direction)
    elif obj_id in [0x01, 0x02, 0x03, 0x04]:
        # Horizontal walls
        for dx in range(size + 1):
            for dy in range(4):  # 4 tiles tall
                tiles.append((x + dx, y + dy))

    elif obj_id in [0x61, 0x62, 0x63, 0x64]:
        # Vertical walls
        for dx in range(4):  # 4 tiles wide
            for dy in range(size + 1):
                tiles.append((x + dx, y + dy))

    # Type 2 objects (0x100+) - fixed sizes, no expansion
    elif obj_id >= 0x100:
        tiles.append((x, y))

    # Default: single tile or small area based on size
    else:
        # Generic expansion: size encodes width/height
        width = max(1, (size & 0x03) + 1)
        height = max(1, ((size >> 2) & 0x03) + 1)
        for dy in range(height):
            for dx in range(width):
                tiles.append((x + dx, y + dy))

    return tiles


def format_collision_asm(offsets: List[int], room_id: int, label: str = None,
                         objects: List[Dict] = None) -> str:
    """Generate ASM-ready collision data block."""
    lines = []
    label = label or f"Room{room_id:02X}_CollisionData"

    lines.append(f"; Room 0x{room_id:02X} - Collision Offsets")
    lines.append(f"; Generated by analyze_room.py")

    if objects:
        for obj in objects:
            lines.append(f"; Object 0x{obj['id']:03X} @ ({obj['x']},{obj['y']}) size={obj['size']}")

    lines.append(f"{label}:")
    lines.append("{")
    lines.append(f"  db {len(offsets)}  ; Tile count")

    # Group offsets by rows of 8 for readability
    for i in range(0, len(offsets), 8):
        row = offsets[i:i+8]
        hex_vals = ", ".join(f"${o:04X}" for o in sorted(row))
        lines.append(f"  dw {hex_vals}")

    lines.append("}")
    return "\n".join(lines)


def analyze_collision_offsets(result: Dict, filter_id: Optional[int] = None,
                               expand_area: bool = False, asm_output: bool = False,
                               verbose: bool = True) -> Dict:
    """Analyze collision offsets for objects in a room."""
    analysis = {
        'room_id': result['room_id'],
        'objects': [],
        'offsets': [],
        'tiles': []
    }

    # Collect all objects from all layers
    all_objects = []
    for layer_num in [0, 1, 2]:
        all_objects.extend(result['objects_by_layer'][layer_num])

    # Filter by object ID if specified
    if filter_id is not None:
        all_objects = [obj for obj in all_objects if obj['id'] == filter_id]

    analysis['objects'] = all_objects

    # Calculate collision offsets
    all_tiles = []
    for obj in all_objects:
        if expand_area:
            tiles = expand_object_area(obj)
        else:
            tiles = [(obj['x'], obj['y'])]

        for (tx, ty) in tiles:
            # Validate tile coordinates
            if 0 <= tx < 64 and 0 <= ty < 64:
                offset = calculate_collision_offset(tx, ty)
                all_tiles.append((tx, ty, offset, obj))

    # Remove duplicates and sort
    seen_offsets = set()
    unique_tiles = []
    for (tx, ty, offset, obj) in all_tiles:
        if offset not in seen_offsets:
            seen_offsets.add(offset)
            unique_tiles.append((tx, ty, offset, obj))

    analysis['tiles'] = unique_tiles
    analysis['offsets'] = sorted(list(seen_offsets))

    # Output
    if asm_output:
        asm = format_collision_asm(analysis['offsets'], result['room_id'],
                                    objects=all_objects)
        print(asm)
    elif verbose:
        print(f"\n{'='*70}")
        print(f"COLLISION OFFSETS - Room 0x{result['room_id']:02X}")
        print(f"{'='*70}")

        if filter_id is not None:
            print(f"Filtered by object ID: 0x{filter_id:03X}")

        print(f"\nObjects analyzed: {len(all_objects)}")
        for obj in all_objects:
            desc = get_object_description(obj['id'])
            print(f"  ID=0x{obj['id']:03X} @ ({obj['x']},{obj['y']}) size={obj['size']} - {desc}")

        print(f"\nTile coverage: {len(unique_tiles)} tiles")
        if expand_area:
            print("(Area expansion enabled)")

        print(f"\nCollision offsets (for $7F2000):")
        for i, (tx, ty, offset, obj) in enumerate(sorted(unique_tiles, key=lambda t: t[2])):
            print(f"  ({tx:2d},{ty:2d}) -> ${offset:04X}")
            if i > 20 and len(unique_tiles) > 25:
                print(f"  ... and {len(unique_tiles) - i - 1} more")
                break

    return analysis


def parse_room_objects(rom_data: bytes, room_id: int, verbose: bool = True) -> Dict:
    """Parse all objects for a given room."""
    result = {
        'room_id': room_id,
        'floor1': 0,
        'floor2': 0,
        'layout': 0,
        'objects_by_layer': {0: [], 1: [], 2: []},
        'doors': [],
        'data_address': 0,
    }
    
    # Get room object data pointer
    object_ptr_table = read_long(rom_data, ROOM_OBJECT_POINTER)
    object_ptr_table_pc = snes_to_pc(object_ptr_table)
    
    # Read room-specific pointer (3 bytes per room)
    room_ptr_addr = object_ptr_table_pc + (room_id * 3)
    room_data_snes = read_long(rom_data, room_ptr_addr)
    room_data_pc = snes_to_pc(room_data_snes)
    
    result['data_address'] = room_data_pc
    
    if verbose:
        print(f"\n{'='*70}")
        print(f"ROOM {room_id:03d} (0x{room_id:03X}) OBJECT ANALYSIS")
        print(f"{'='*70}")
        print(f"Room data at PC: 0x{room_data_pc:05X} (SNES: 0x{room_data_snes:06X})")
    
    # First 2 bytes: floor graphics and layout
    floor_byte = rom_data[room_data_pc]
    layout_byte = rom_data[room_data_pc + 1]
    
    result['floor1'] = floor_byte & 0x0F
    result['floor2'] = (floor_byte >> 4) & 0x0F
    result['layout'] = (layout_byte >> 2) & 0x07
    
    if verbose:
        print(f"Floor: BG1={result['floor1']}, BG2={result['floor2']}, Layout={result['layout']}")
    
    # Parse objects starting at offset 2
    pos = room_data_pc + 2
    layer = 0
    
    if verbose:
        print(f"\n{'='*70}")
        print("OBJECTS (Layer 0=BG1 main, Layer 1=BG2 overlay, Layer 2=BG1 priority)")
        print(f"{'='*70}")
    
    while pos + 2 < len(rom_data):
        b1 = rom_data[pos]
        b2 = rom_data[pos + 1]
        
        # Check for layer terminator (0xFFFF)
        if b1 == 0xFF and b2 == 0xFF:
            if verbose:
                print(f"\n--- Layer {layer} END ---")
            pos += 2
            layer += 1
            if layer >= 3:
                break
            if verbose:
                print(f"\n--- Layer {layer} START ---")
            continue
        
        # Check for door section marker (0xF0FF)
        if b1 == 0xF0 and b2 == 0xFF:
            if verbose:
                print(f"\n--- Doors ---")
            pos += 2
            while pos + 1 < len(rom_data):
                d1 = rom_data[pos]
                d2 = rom_data[pos + 1]
                if d1 == 0xFF and d2 == 0xFF:
                    break
                door = {
                    'position': (d1 >> 4) & 0x0F,
                    'direction': d1 & 0x03,
                    'type': d2
                }
                result['doors'].append(door)
                if verbose:
                    print(f"  Door: pos={door['position']}, dir={door['direction']}, type=0x{door['type']:02X}")
                pos += 2
            continue
        
        # Read 3rd byte for object
        b3 = rom_data[pos + 2]
        pos += 3
        
        obj = decode_object(b1, b2, b3, layer)
        result['objects_by_layer'][layer].append(obj)
        
        if verbose:
            desc = get_object_description(obj['id'])
            routine = get_draw_routine(obj['id'])
            layer_names = ["BG1_Main", "BG2_Overlay", "BG1_Priority"]
            routine_str = f" [{routine}]" if routine else ""
            print(f"  L{layer} ({layer_names[layer]}): [{b1:02X} {b2:02X} {b3:02X}] -> "
                  f"T{obj['type']} ID=0x{obj['id']:03X} @ ({obj['x']:2d},{obj['y']:2d}) "
                  f"sz={obj['size']:2d} - {desc}{routine_str}")
    
    # Summary
    if verbose:
        print(f"\n{'='*70}")
        print("SUMMARY")
        print(f"{'='*70}")
        for layer_num, layer_name in [(0, "BG1 Main"), (1, "BG2 Overlay"), (2, "BG1 Priority")]:
            objs = result['objects_by_layer'][layer_num]
            print(f"Layer {layer_num} ({layer_name}): {len(objs)} objects")
            if objs:
                id_counts = {}
                for obj in objs:
                    id_counts[obj['id']] = id_counts.get(obj['id'], 0) + 1
                for obj_id, count in sorted(id_counts.items()):
                    desc = get_object_description(obj_id)
                    print(f"  0x{obj_id:03X}: {count}x - {desc}")
    
    return result


def analyze_layer_compositing(result: Dict, verbose: bool = True) -> Dict:
    """Analyze layer compositing issues for a room."""
    analysis = {
        'has_bg2_objects': len(result['objects_by_layer'][1]) > 0,
        'bg2_object_count': len(result['objects_by_layer'][1]),
        'bg2_objects': result['objects_by_layer'][1],
        'same_floor_graphics': result['floor1'] == result['floor2'],
        'potential_issues': []
    }
    
    if analysis['has_bg2_objects'] and analysis['same_floor_graphics']:
        analysis['potential_issues'].append(
            "BG2 overlay objects with same floor graphics - may have compositing issues"
        )
    
    if verbose and analysis['has_bg2_objects']:
        print(f"\n{'='*70}")
        print("LAYER COMPOSITING ANALYSIS")
        print(f"{'='*70}")
        print(f"\nBG2 Overlay objects ({analysis['bg2_object_count']}):")
        for obj in analysis['bg2_objects']:
            desc = get_object_description(obj['id'])
            print(f"  ID=0x{obj['id']:03X} @ ({obj['x']},{obj['y']}) size={obj['size']} - {desc}")
        
        if analysis['potential_issues']:
            print("\nPotential Issues:")
            for issue in analysis['potential_issues']:
                print(f"  - {issue}")
    
    return analysis


def find_rom_file(specified_path: Optional[str] = None) -> Optional[str]:
    """Find a valid ROM file."""
    if specified_path:
        if os.path.isfile(specified_path):
            return specified_path
        print(f"Error: ROM file not found: {specified_path}")
        return None
    
    # Try default paths relative to script location
    script_dir = os.path.dirname(os.path.abspath(__file__))
    project_root = os.path.dirname(script_dir)
    
    for rel_path in DEFAULT_ROM_PATHS:
        full_path = os.path.join(project_root, rel_path)
        if os.path.isfile(full_path):
            return full_path
    
    print("Error: Could not find ROM file. Please specify with --rom")
    print("Tried paths:")
    for rel_path in DEFAULT_ROM_PATHS:
        print(f"  {os.path.join(project_root, rel_path)}")
    return None


def main():
    parser = argparse.ArgumentParser(
        description="Analyze dungeon room objects from ALTTP ROM",
        formatter_class=argparse.RawDescriptionHelpFormatter,
        epilog="""
Examples:
  %(prog)s 1                    # Analyze room 001
  %(prog)s 1 2 3                # Analyze rooms 001, 002, 003
  %(prog)s --range 0 10         # Analyze rooms 0-10
  %(prog)s --all                # Analyze all rooms (summary only)
  %(prog)s --list-bg2           # List rooms with BG2 overlay objects
  %(prog)s 1 --json             # Output as JSON
  %(prog)s 1 --compositing      # Include layer compositing analysis
        """
    )
    
    parser.add_argument('rooms', nargs='*', type=int, help='Room ID(s) to analyze')
    parser.add_argument('--rom', '-r', type=str, help='Path to ROM file')
    parser.add_argument('--range', nargs=2, type=int, metavar=('START', 'END'),
                        help='Analyze range of rooms (inclusive)')
    parser.add_argument('--all', action='store_true', help='Analyze all rooms (summary only)')
    parser.add_argument('--json', '-j', action='store_true', help='Output as JSON')
    parser.add_argument('--quiet', '-q', action='store_true', help='Minimal output')
    parser.add_argument('--compositing', '-c', action='store_true',
                        help='Include layer compositing analysis')
    parser.add_argument('--list-bg2', action='store_true',
                        help='List all rooms with BG2 overlay objects')
    parser.add_argument('--summary', '-s', action='store_true',
                        help='Show summary only (object counts)')

    # Collision offset features
    parser.add_argument('--collision', action='store_true',
                        help='Calculate collision map offsets for objects')
    parser.add_argument('--filter-id', type=lambda x: int(x, 0), metavar='ID',
                        help='Filter objects by ID (e.g., 0xD9 or 217)')
    parser.add_argument('--asm', action='store_true',
                        help='Output collision offsets in ASM format')
    parser.add_argument('--area', action='store_true',
                        help='Expand objects to full tile area (not just origin)')

    args = parser.parse_args()
    
    # Find ROM file
    rom_path = find_rom_file(args.rom)
    if not rom_path:
        sys.exit(1)
    
    # Load ROM
    if not args.quiet:
        print(f"Loading ROM: {rom_path}")
    with open(rom_path, 'rb') as f:
        rom_data = f.read()
    if not args.quiet:
        print(f"ROM size: {len(rom_data)} bytes")
    
    # Determine rooms to analyze
    room_ids = []
    if args.all or args.list_bg2:
        room_ids = list(range(NUMBER_OF_ROOMS))
    elif args.range:
        room_ids = list(range(args.range[0], args.range[1] + 1))
    elif args.rooms:
        room_ids = args.rooms
    else:
        # Default to room 1 if nothing specified
        room_ids = [1]
    
    # Validate room IDs
    room_ids = [r for r in room_ids if 0 <= r < NUMBER_OF_ROOMS]
    
    if not room_ids:
        print("Error: No valid room IDs specified")
        sys.exit(1)
    
    # Analyze rooms
    all_results = []
    verbose = not (args.quiet or args.json or args.list_bg2 or args.all or args.asm)

    for room_id in room_ids:
        try:
            result = parse_room_objects(rom_data, room_id, verbose=verbose)
            
            if args.compositing:
                result['compositing'] = analyze_layer_compositing(result, verbose=verbose)

            if args.collision:
                collision_verbose = not (args.asm or args.quiet)
                result['collision'] = analyze_collision_offsets(
                    result,
                    filter_id=args.filter_id,
                    expand_area=args.area,
                    asm_output=args.asm,
                    verbose=collision_verbose
                )

            all_results.append(result)

        except Exception as e:
            if not args.quiet:
                print(f"Error analyzing room {room_id}: {e}")

    # Output results
    if args.collision and args.asm:
        # Already output by analyze_collision_offsets
        pass

    elif args.json:
        # Convert to JSON-serializable format
        for result in all_results:
            result['objects_by_layer'] = {
                str(k): v for k, v in result['objects_by_layer'].items()
            }
        print(json.dumps(all_results, indent=2))
    
    elif args.list_bg2:
        print(f"\n{'='*70}")
        print("ROOMS WITH BG2 OVERLAY OBJECTS")
        print(f"{'='*70}")
        rooms_with_bg2 = []
        for result in all_results:
            bg2_count = len(result['objects_by_layer'][1])
            if bg2_count > 0:
                rooms_with_bg2.append((result['room_id'], bg2_count))
        
        print(f"\nFound {len(rooms_with_bg2)} rooms with BG2 overlay objects:")
        for room_id, count in sorted(rooms_with_bg2):
            print(f"  Room {room_id:03d} (0x{room_id:03X}): {count} BG2 objects")
    
    elif args.all or args.summary:
        print(f"\n{'='*70}")
        print("ROOM SUMMARY")
        print(f"{'='*70}")
        print(f"{'Room':>6} {'L0':>4} {'L1':>4} {'L2':>4} {'Doors':>5} {'Floor':>8}")
        print("-" * 40)
        for result in all_results:
            l0 = len(result['objects_by_layer'][0])
            l1 = len(result['objects_by_layer'][1])
            l2 = len(result['objects_by_layer'][2])
            doors = len(result['doors'])
            floor = f"{result['floor1']}/{result['floor2']}"
            print(f"{result['room_id']:>6} {l0:>4} {l1:>4} {l2:>4} {doors:>5} {floor:>8}")


if __name__ == "__main__":
    main()

