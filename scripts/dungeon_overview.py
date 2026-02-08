#!/usr/bin/env python3
"""
Dungeon Overview Generator for Oracle of Secrets
Creates simplified dungeon maps with room shapes, door types, and key features.

Usage:
    python3 dungeon_overview.py                          # Generate Goron Mines overview
    python3 dungeon_overview.py --dungeon glacia         # Generate Glacia Estate
    python3 dungeon_overview.py --tracks --room 0x78     # Show tracks for a specific room
    python3 dungeon_overview.py --list                   # List all configured dungeons
    python3 dungeon_overview.py --json --room 0x78       # JSON output for a room

Output files are written to: Docs/World/Dungeons/<DungeonName>_Map.md
"""

import argparse
import subprocess
import json
import sys
import os
from dataclasses import dataclass, field
from typing import Optional, List, Dict
from collections import defaultdict

ROOT = os.path.abspath(os.path.join(os.path.dirname(__file__), ".."))

# Path to z3ed binary
# Prefer the repo wrapper script which selects the newest build (build_ai first).
Z3ED = os.environ.get("Z3ED_BIN") or os.environ.get("Z3ED_PATH") or os.path.join(ROOT, "scripts", "z3ed")

# Default ROM path
DEFAULT_ROM = os.environ.get(
    "OOS_ROM",
    "/Users/scawful/src/hobby/oracle-of-secrets/Roms/oos168x.sfc",
)

# Output directory for generated docs
DOCS_OUTPUT = "/Users/scawful/src/hobby/oracle-of-secrets/Docs/World/Dungeons"

# =============================================================================
# DUNGEON CONFIGURATIONS
# =============================================================================
# Add new dungeons here with their room IDs and metadata

DUNGEON_CONFIGS = {
    "goron_mines": {
        "name": "Goron Mines",
        "dungeon_id": "custom",
        "boss": "King Dodongo (King Helmasaur reskin)",
        "dungeon_item": "Hammer, Fire Shield",
        "big_chest_room": 0x88,
        "miniboss": ("Lanmolas", 0x78),
        "entrance_room": 0x98,
        "boss_room": 0xC8,
        "floors": {
            "F1": {
                "name": "Main Level",
                "grid": "3x3",
                "rooms": [0x77, 0x78, 0x79, 0x87, 0x88, 0x89, 0x97, 0x98, 0x99],
            },
            "B1": {
                "name": "Basement 1",
                "grid": "2x2 + side",
                "rooms": [0x69, 0xA8, 0xA9, 0xB8, 0xB9],
            },
            "B2": {
                "name": "Basement 2 + Boss",
                "grid": "linear",
                "rooms": [0xC8, 0xD7, 0xD8, 0xD9, 0xDA],
            },
        },
        "all_rooms": [
            0x77, 0x78, 0x79,
            0x87, 0x88, 0x89,
            0x97, 0x98, 0x99,
            0x69, 0xA8, 0xA9, 0xB8, 0xB9,
            0xC8, 0xD7, 0xD8, 0xD9, 0xDA,
        ],
        "room_names": {
            0x77: "NW Hall", 0x78: "Lanmolas Miniboss", 0x79: "NE Hall",
            0x87: "West Hall", 0x88: "Big Chest", 0x89: "East Hall",
            0x97: "SW Hall", 0x98: "Entrance", 0x99: "SE Hall",
            0x69: "B1 Side", 0xA8: "B1 NW", 0xA9: "B1 NE",
            0xB8: "B1 SW", 0xB9: "B1 SE",
            0xC8: "Boss (King Dodongo)", 0xD7: "B2 West", 0xD8: "Pre-Boss",
            0xD9: "B2 Mid", 0xDA: "B2 East",
        },
    },
    "glacia_estate": {
        "name": "Glacia Estate",
        "dungeon_id": "0x0B",
        "boss": "TBD",
        "dungeon_item": "TBD",
        "big_chest_room": None,
        "miniboss": None,
        "entrance_room": None,
        "boss_room": None,
        "floors": {},
        "all_rooms": [],  # To be filled in
        "room_names": {},
    },
    # Add more dungeons here as they are developed
}

# =============================================================================
# DATA STRUCTURES
# =============================================================================

@dataclass
class RoomInfo:
    room_id: int
    name: str
    doors: List[tuple] = field(default_factory=list)
    stairs: List[int] = field(default_factory=list)
    holewarp: int = 0
    has_minecart_tracks: bool = False
    track_count: int = 0
    has_boss: bool = False
    has_big_chest: bool = False
    has_chest: bool = False
    object_count: int = 0
    door_count: int = 0


# =============================================================================
# Z3ED INTEGRATION
# =============================================================================

def run_z3ed(command: str, rom_path: str, **kwargs) -> Optional[dict]:
    """Run a z3ed command and return JSON output."""
    args = [Z3ED, command, f"--rom={rom_path}"]
    for key, value in kwargs.items():
        args.append(f"--{key}={value}")

    try:
        result = subprocess.run(args, capture_output=True, text=True, timeout=30)
        if result.returncode != 0:
            print(f"z3ed error: {result.stderr}", file=sys.stderr)
            return None
        return json.loads(result.stdout)
    except subprocess.TimeoutExpired:
        print(f"z3ed timeout for: {' '.join(args)}", file=sys.stderr)
        return None
    except json.JSONDecodeError as e:
        print(f"JSON parse error: {e}", file=sys.stderr)
        return None


def get_room_objects(rom_path: str, room_id: int) -> List[dict]:
    """Get all objects in a room."""
    data = run_z3ed("dungeon-list-objects", rom_path, room=f"0x{room_id:02X}")
    if data and "Dungeon Room Objects" in data:
        return data["Dungeon Room Objects"].get("objects", [])
    return []


def get_room_graph(rom_path: str, room_id: int) -> Optional[dict]:
    """Get room connection data."""
    data = run_z3ed("dungeon-graph", rom_path, room=f"0x{room_id:02X}")
    if data and "dungeon_graph" in data:
        nodes = data["dungeon_graph"].get("nodes", [])
        return nodes[0] if nodes else None
    return None


def count_track_objects(objects: List[dict]) -> int:
    """Count track objects (Object ID 0x31 = 49 decimal)."""
    return sum(1 for obj in objects if obj.get("id") == 0x31)


# =============================================================================
# ROOM ANALYSIS
# =============================================================================

def analyze_room(rom_path: str, room_id: int, dungeon_config: dict) -> RoomInfo:
    """Load room data and analyze its features."""

    # Get custom name or use default
    name = dungeon_config.get("room_names", {}).get(
        room_id, f"Room 0x{room_id:02X}"
    )

    # Get objects
    objects = get_room_objects(rom_path, room_id)
    track_count = count_track_objects(objects)

    # Get connections
    graph = get_room_graph(rom_path, room_id)
    stairs = []
    holewarp = 0
    if graph:
        stairs = [int(s, 16) for s in graph.get("stairs", []) if int(s, 16) != 0]
        holewarp = int(graph.get("holewarp", "0x00"), 16)

    # Determine special room types
    has_boss = room_id == dungeon_config.get("boss_room")
    has_big_chest = room_id == dungeon_config.get("big_chest_room")

    return RoomInfo(
        room_id=room_id,
        name=name,
        stairs=stairs,
        holewarp=holewarp,
        has_minecart_tracks=track_count > 0,
        track_count=track_count,
        has_boss=has_boss,
        has_big_chest=has_big_chest,
        object_count=len(objects),
        door_count=0,  # Would need deeper parsing
    )


# =============================================================================
# OUTPUT GENERATION
# =============================================================================

def generate_dungeon_overview(rom_path: str, dungeon_key: str) -> str:
    """Generate complete dungeon overview as markdown."""

    config = DUNGEON_CONFIGS.get(dungeon_key)
    if not config:
        return f"Error: Unknown dungeon '{dungeon_key}'"

    lines = []
    lines.append(f"# {config['name']} - Dungeon Map\n")
    lines.append(f"**Dungeon ID:** {config['dungeon_id']}")
    lines.append(f"**Boss:** {config['boss']}")
    lines.append(f"**Dungeon Item:** {config['dungeon_item']}")

    if config['big_chest_room']:
        lines.append(f"**Big Chest:** Room 0x{config['big_chest_room']:02X}")

    if config['miniboss']:
        miniboss_name, miniboss_room = config['miniboss']
        lines.append(f"**Miniboss:** {miniboss_name} in Room 0x{miniboss_room:02X}")

    lines.append("\n---\n")

    # Analyze all rooms
    rooms_data = {}
    for room_id in config['all_rooms']:
        rooms_data[room_id] = analyze_room(rom_path, room_id, config)

    # Generate floor layouts
    lines.append("## Floor Layout\n")

    for floor_key, floor_info in config.get('floors', {}).items():
        lines.append(f"### {floor_key} ({floor_info['name']}) - {floor_info['grid']} Grid\n")

        # Room summary for this floor
        lines.append("| Room | Name | Objects | Tracks | Connections |")
        lines.append("|------|------|---------|--------|-------------|")

        for room_id in floor_info['rooms']:
            room = rooms_data.get(room_id)
            if room:
                conns = []
                for dest in room.stairs:
                    dest_name = config.get("room_names", {}).get(dest, f"0x{dest:02X}")
                    conns.append(f"stair→{dest_name}")
                if room.holewarp:
                    dest_name = config.get("room_names", {}).get(room.holewarp, f"0x{room.holewarp:02X}")
                    conns.append(f"hole→{dest_name}")

                conn_str = ", ".join(conns) if conns else "-"
                track_str = str(room.track_count) if room.track_count else "-"

                role = ""
                if room.has_boss:
                    role = " **BOSS**"
                elif room.has_big_chest:
                    role = " BIG CHEST"
                elif room_id == config.get('entrance_room'):
                    role = " ENTRANCE"
                elif config['miniboss'] and room_id == config['miniboss'][1]:
                    role = " MINIBOSS"

                lines.append(f"| 0x{room_id:02X} | {room.name}{role} | {room.object_count} | {track_str} | {conn_str} |")

        lines.append("")

    # Room statistics summary
    lines.append("---\n")
    lines.append("## Room Statistics\n")
    lines.append("| Room | Obj | Tracks | Role |")
    lines.append("|------|-----|--------|------|")

    for room_id in sorted(config['all_rooms']):
        room = rooms_data.get(room_id)
        if room:
            role = room.name
            if room.has_boss:
                role = f"**{role}**"
            lines.append(f"| 0x{room_id:02X} | {room.object_count} | {room.track_count or '-'} | {role} |")

    # Track-heavy rooms
    track_rooms = [(r.room_id, r.track_count) for r in rooms_data.values() if r.track_count > 20]
    if track_rooms:
        track_rooms.sort(key=lambda x: -x[1])
        lines.append(f"\n**Track-heavy rooms:** {', '.join(f'0x{r:02X} ({c})' for r, c in track_rooms)}")

    lines.append("\n---\n")
    lines.append("## See Also\n")
    safe_name = config['name'].replace(' ', '')
    lines.append(f"- [{safe_name}_Tracks.md]({safe_name}_Tracks.md) - Detailed minecart track layouts\n")

    return "\n".join(lines)


def generate_track_map(rom_path: str, room_id: int, dungeon_config: dict = None) -> str:
    """Generate detailed track visualization for a room."""

    objects = get_room_objects(rom_path, room_id)
    track_objects = [obj for obj in objects if obj.get("id") == 0x31]

    if not track_objects:
        return f"Room 0x{room_id:02X} has no track objects."

    # Track subtype names
    TRACK_TYPES = {
        0: "LeftRight", 1: "UpDown",
        2: "TopLeft", 3: "TopRight", 4: "BottomLeft", 5: "BottomRight",
        6: "UpDownFloor", 7: "LeftRightFloor",
        8: "TopLeftFloor", 9: "TopRightFloor", 10: "BottomLeftFloor", 11: "BottomRightFloor",
        12: "FloorAny", 14: "TrackAny",
    }

    lines = []
    name = "Unknown"
    if dungeon_config:
        name = dungeon_config.get("room_names", {}).get(room_id, f"Room 0x{room_id:02X}")

    lines.append(f"## Room 0x{room_id:02X} ({name})\n")
    lines.append(f"**Track Objects:** {len(track_objects)}\n")

    # Group by Y coordinate for layout visualization
    by_y = defaultdict(list)
    for obj in track_objects:
        subtype = (obj.get("layer", 0) << 2) | obj.get("size", 0)
        by_y[obj["y"]].append({
            "x": obj["x"],
            "subtype": subtype,
            "type_name": TRACK_TYPES.get(subtype, f"Unknown({subtype})")
        })

    # Find bounds
    all_x = [obj["x"] for obj in track_objects]
    all_y = [obj["y"] for obj in track_objects]
    min_x, max_x = min(all_x), max(all_x)
    min_y, max_y = min(all_y), max(all_y)

    lines.append(f"**Bounds:** X={min_x}-{max_x}, Y={min_y}-{max_y}\n")

    # Horizontal lines
    horizontal_ys = []
    for y in sorted(by_y.keys()):
        tracks = by_y[y]
        if len(tracks) >= 5:  # Likely a horizontal line
            x_vals = sorted(t["x"] for t in tracks)
            horizontal_ys.append((y, x_vals[0], x_vals[-1], len(tracks)))

    if horizontal_ys:
        lines.append("**Horizontal Lines:**")
        for y, x_start, x_end, count in horizontal_ys:
            lines.append(f"- Y={y}: X={x_start} to X={x_end} ({count} segments)")
        lines.append("")

    # Vertical columns
    by_x = defaultdict(list)
    for obj in track_objects:
        by_x[obj["x"]].append(obj["y"])

    vertical_xs = [(x, min(ys), max(ys), len(ys)) for x, ys in by_x.items() if len(ys) >= 5]
    if vertical_xs:
        vertical_xs.sort(key=lambda t: t[0])
        lines.append("**Vertical Columns:**")
        for x, y_start, y_end, count in vertical_xs:
            lines.append(f"- X={x}: Y={y_start} to Y={y_end} ({count} segments)")
        lines.append("")

    return "\n".join(lines)


# =============================================================================
# CLI
# =============================================================================

def main():
    parser = argparse.ArgumentParser(
        description="Generate dungeon documentation for Oracle of Secrets"
    )
    parser.add_argument("--rom", default=DEFAULT_ROM, help="Path to ROM file")
    parser.add_argument("--dungeon", default="goron_mines",
                        help="Dungeon key (goron_mines, glacia_estate, etc.)")
    parser.add_argument("--list", action="store_true", help="List available dungeons")
    parser.add_argument("--tracks", action="store_true", help="Show track details")
    parser.add_argument("--room", type=lambda x: int(x, 0), help="Specific room (hex)")
    parser.add_argument("--json", action="store_true", help="JSON output")
    parser.add_argument("--output", help="Output file path (default: stdout)")
    parser.add_argument("--save", action="store_true",
                        help=f"Save to {DOCS_OUTPUT}/<DungeonName>_Map.md")

    args = parser.parse_args()

    if args.list:
        print("Available dungeons:")
        for key, config in DUNGEON_CONFIGS.items():
            room_count = len(config.get("all_rooms", []))
            print(f"  {key}: {config['name']} ({room_count} rooms)")
        return

    if args.tracks and args.room:
        config = DUNGEON_CONFIGS.get(args.dungeon)
        output = generate_track_map(args.rom, args.room, config)
        print(output)
        return

    if args.room and args.json:
        objects = get_room_objects(args.rom, args.room)
        print(json.dumps(objects, indent=2))
        return

    # Generate full dungeon overview
    output = generate_dungeon_overview(args.rom, args.dungeon)

    if args.save:
        config = DUNGEON_CONFIGS.get(args.dungeon)
        if config:
            safe_name = config['name'].replace(' ', '')
            filepath = os.path.join(DOCS_OUTPUT, f"{safe_name}_Map.md")
            os.makedirs(DOCS_OUTPUT, exist_ok=True)
            with open(filepath, 'w') as f:
                f.write(output)
            print(f"Saved to: {filepath}")
        else:
            print(output)
    elif args.output:
        with open(args.output, 'w') as f:
            f.write(output)
        print(f"Saved to: {args.output}")
    else:
        print(output)


if __name__ == "__main__":
    main()
