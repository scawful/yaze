#!/usr/bin/env python3
"""
Location Mapper for ALTTP ROMs (Multi-ROM Support)

Creates documentation for all indoor locations: dungeons, shrines, caves, houses, shops, special areas.
Supports multiple ROM profiles (vanilla ALTTP, Oracle of Secrets, and custom ROM hacks).

Usage:
    python3 location_mapper.py --list                           # List all locations (auto-detect ROM)
    python3 location_mapper.py --profiles                       # List available profiles
    python3 location_mapper.py --rom=vanilla.sfc --list         # List locations for vanilla
    python3 location_mapper.py --profile=oracle --list          # Use specific profile
    python3 location_mapper.py --location goron_mines           # Generate specific location
    python3 location_mapper.py --location goron_mines --save    # Save to Docs
    python3 location_mapper.py --discover 0x26                  # Discover rooms from entrance ID
    python3 location_mapper.py --tracks --room 0x78             # Show tracks for a specific room
    python3 location_mapper.py --json --room 0x78               # JSON output for a room

Output files are written to: <docs_subdir>/<Category>/<LocationName>_Map.md
"""

import argparse
import subprocess
import json
import sys
import os
from dataclasses import dataclass, field
from typing import Optional, List, Dict
from collections import defaultdict

# Import the profile system
from profiles import (
    RomProfile, LocationConfig, LocationType,
    detect_rom_profile, REGISTERED_PROFILES,
    VANILLA_PROFILE, ORACLE_PROFILE,
)
from profiles.detect import get_profile_by_name, list_profiles

# Path to z3ed binary
Z3ED = os.environ.get("Z3ED_PATH", "/Users/scawful/src/hobby/yaze/build/bin/Debug/z3ed")

# Default ROM path (used if --rom not specified and detection fails)
DEFAULT_ROM = os.environ.get("ALTTP_ROM", "/Users/scawful/src/hobby/oracle-of-secrets/Roms/oos168x.sfc")


# =============================================================================
# DATA STRUCTURES
# =============================================================================

@dataclass
class RoomInfo:
    """Runtime room analysis data (populated by z3ed queries)."""
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
    except FileNotFoundError:
        print(f"z3ed not found at: {Z3ED}", file=sys.stderr)
        print("Set Z3ED_PATH environment variable or build z3ed", file=sys.stderr)
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


def get_entrance_info(rom_path: str, entrance_id: int) -> Optional[dict]:
    """Get entrance table data for an entrance ID."""
    data = run_z3ed("entrance-info", rom_path, entrance=f"0x{entrance_id:02X}")
    if data and "entrance" in data:
        return data["entrance"]
    return None


# =============================================================================
# ROOM ANALYSIS
# =============================================================================

def analyze_room(rom_path: str, room_id: int, config: LocationConfig) -> RoomInfo:
    """Load room data and analyze its features."""

    # Get custom name or use default
    name = config.room_names.get(room_id, f"Room 0x{room_id:02X}")

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
    has_boss = room_id == config.boss_room
    has_big_chest = room_id == config.big_chest_room

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
        door_count=0,
    )


# =============================================================================
# OUTPUT GENERATION - DUNGEONS
# =============================================================================

def generate_dungeon_doc(rom_path: str, location_key: str, config: LocationConfig) -> str:
    """Generate complete dungeon documentation as markdown."""

    lines = []
    lines.append(f"# {config.name} - Dungeon Map\n")
    lines.append(f"**Dungeon ID:** {config.dungeon_id or 'Unknown'}")
    lines.append(f"**Entrance IDs:** {', '.join(f'0x{e:02X}' for e in config.entrance_ids)}")
    if config.ow_screen is not None:
        lines.append(f"**OW Screen:** 0x{config.ow_screen:02X}")
    lines.append(f"**Boss:** {config.boss or 'TBD'}")
    lines.append(f"**Dungeon Item:** {config.dungeon_item or 'TBD'}")

    if config.big_chest_room:
        lines.append(f"**Big Chest:** Room 0x{config.big_chest_room:02X}")

    if config.miniboss:
        miniboss_name, miniboss_room = config.miniboss
        lines.append(f"**Miniboss:** {miniboss_name} in Room 0x{miniboss_room:02X}")

    if config.entrance_room:
        lines.append(f"**Entrance Room:** 0x{config.entrance_room:02X}")

    if config.notes:
        lines.append(f"\n*{config.notes}*")

    lines.append("\n---\n")

    # Check if we have room data
    all_rooms = config.all_rooms
    if not all_rooms:
        lines.append("## Room Data\n")
        lines.append("*Room layout not yet discovered. Use `z3ed dungeon-discover` to map rooms.*\n")
        if config.entrance_ids:
            lines.append(f"```bash\nz3ed dungeon-discover --rom=<rom.sfc> --entrance=0x{config.entrance_ids[0]:02X} --depth=15\n```\n")
        return "\n".join(lines)

    # Analyze all rooms
    rooms_data = {}
    for room_id in all_rooms:
        rooms_data[room_id] = analyze_room(rom_path, room_id, config)

    # Generate floor layouts
    lines.append("## Floor Layout\n")

    for floor_key, floor_info in config.floors.items():
        lines.append(f"### {floor_key} ({floor_info['name']}) - {floor_info.get('grid', 'unknown')} Grid\n")

        lines.append("| Room | Name | Objects | Tracks | Connections |")
        lines.append("|------|------|---------|--------|-------------|")

        for room_id in floor_info.get('rooms', []):
            room = rooms_data.get(room_id)
            if room:
                conns = []
                for dest in room.stairs:
                    dest_name = config.room_names.get(dest, f"0x{dest:02X}")
                    conns.append(f"stair->{dest_name}")
                if room.holewarp:
                    dest_name = config.room_names.get(room.holewarp, f"0x{room.holewarp:02X}")
                    conns.append(f"hole->{dest_name}")

                conn_str = ", ".join(conns) if conns else "-"
                track_str = str(room.track_count) if room.track_count else "-"

                role = ""
                if room.has_boss:
                    role = " **BOSS**"
                elif room.has_big_chest:
                    role = " BIG CHEST"
                elif room_id == config.entrance_room:
                    role = " ENTRANCE"
                elif config.miniboss and room_id == config.miniboss[1]:
                    role = " MINIBOSS"

                lines.append(f"| 0x{room_id:02X} | {room.name}{role} | {room.object_count} | {track_str} | {conn_str} |")

        lines.append("")

    # Room statistics summary
    lines.append("---\n")
    lines.append("## Room Statistics\n")
    lines.append("| Room | Obj | Tracks | Role |")
    lines.append("|------|-----|--------|------|")

    for room_id in sorted(all_rooms):
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
    safe_name = config.name.replace(' ', '')
    lines.append(f"- [{safe_name}_Tracks.md]({safe_name}_Tracks.md) - Detailed track layouts (if applicable)\n")

    return "\n".join(lines)


# =============================================================================
# OUTPUT GENERATION - SHRINES
# =============================================================================

def generate_shrine_doc(rom_path: str, location_key: str, config: LocationConfig) -> str:
    """Generate shrine documentation as markdown."""

    lines = []
    lines.append(f"# {config.name}\n")
    lines.append(f"**Type:** Shrine")

    if config.entrance_ids:
        lines.append(f"**Entrance ID(s):** {', '.join(f'0x{e:02X}' for e in config.entrance_ids)}")
    else:
        lines.append(f"**Entrance ID(s):** TBD")

    if config.ow_screen is not None:
        lines.append(f"**Overworld Location:** OW 0x{config.ow_screen:02X}")
    else:
        lines.append(f"**Overworld Location:** TBD")

    lines.append(f"**Reward:** {config.reward or 'TBD'}")
    lines.append(f"**Boss:** {config.boss or 'None'}")

    lines.append("\n---\n")
    lines.append("## Overview\n")
    lines.append(f"*{config.name} documentation pending room discovery.*\n")

    lines.append("\n---\n")
    lines.append("## Room Layout\n")
    lines.append("```\n[To be mapped]\n```\n")

    lines.append("\n---\n")
    lines.append("## Generation Notes\n")
    lines.append(f"**Generated with:** `location_mapper.py --location {location_key}`\n")

    return "\n".join(lines)


# =============================================================================
# OUTPUT GENERATION - CAVES
# =============================================================================

def generate_cave_doc(rom_path: str, location_key: str, config: LocationConfig) -> str:
    """Generate cave documentation as markdown."""

    lines = []
    lines.append(f"# {config.name}\n")
    lines.append(f"**Type:** {(config.cave_type or 'Cave').title()}")
    lines.append(f"**Entrance ID(s):** {', '.join(f'0x{e:02X}' for e in config.entrance_ids)}")
    lines.append(f"**Region:** {config.region or 'Unknown'}")

    ow_screens = config.ow_screens
    if ow_screens:
        lines.append(f"**OW Screens:** {', '.join(f'0x{s:02X}' for s in ow_screens)}")

    connects = config.connects
    if connects:
        lines.append(f"**Connects:** {' <-> '.join(connects)}")

    contents = config.contents
    if contents:
        lines.append(f"**Contents:** {contents}")

    lines.append("\n---\n")
    lines.append("## Layout\n")
    lines.append("```\n[To be mapped]\n```\n")

    lines.append("\n---\n")
    lines.append("## Generation Notes\n")
    lines.append(f"**Generated with:** `location_mapper.py --location {location_key}`\n")

    return "\n".join(lines)


# =============================================================================
# OUTPUT GENERATION - HOUSES/SHOPS/SPECIAL
# =============================================================================

def generate_simple_doc(rom_path: str, location_key: str, config: LocationConfig) -> str:
    """Generate simple location documentation (houses, shops, special)."""

    loc_type = config.type

    lines = []
    lines.append(f"# {config.name}\n")
    lines.append(f"**Type:** {loc_type.value.title()}")
    lines.append(f"**Entrance ID(s):** {', '.join(f'0x{e:02X}' for e in config.entrance_ids)}")

    ow_screen = config.ow_screen
    if ow_screen is not None:
        lines.append(f"**OW Screen:** 0x{ow_screen:02X}")

    # Type-specific fields
    if loc_type == LocationType.HOUSE:
        notable = config.notable
        if notable:
            lines.append(f"**Notable:** {notable}")
    elif loc_type == LocationType.SHOP:
        inventory = config.inventory or 'TBD'
        lines.append(f"**Inventory:** {inventory}")
    elif loc_type == LocationType.SPECIAL:
        purpose = config.purpose or 'TBD'
        lines.append(f"**Purpose:** {purpose}")

    lines.append("\n---\n")
    lines.append("## Description\n")
    lines.append(f"*{config.name} - {loc_type.value} documentation.*\n")

    return "\n".join(lines)


# =============================================================================
# MAIN GENERATOR
# =============================================================================

def generate_location_doc(rom_path: str, location_key: str, profile: RomProfile) -> str:
    """Generate documentation for any location type."""

    config = profile.get_location(location_key)
    if not config:
        return f"Error: Unknown location '{location_key}' in profile '{profile.short_name}'"

    loc_type = config.type

    if loc_type == LocationType.DUNGEON:
        return generate_dungeon_doc(rom_path, location_key, config)
    elif loc_type == LocationType.SHRINE:
        return generate_shrine_doc(rom_path, location_key, config)
    elif loc_type == LocationType.CAVE:
        return generate_cave_doc(rom_path, location_key, config)
    else:
        return generate_simple_doc(rom_path, location_key, config)


def get_output_path(location_key: str, profile: RomProfile, rom_dir: str) -> Optional[str]:
    """Get the output file path for a location."""
    config = profile.get_location(location_key)
    if not config:
        return None

    loc_type = config.type
    safe_name = config.name.replace(' ', '').replace("'", "")

    type_dirs = {
        LocationType.DUNGEON: "Dungeons",
        LocationType.SHRINE: "Shrines",
        LocationType.CAVE: "Caves",
        LocationType.HOUSE: "Houses",
        LocationType.SHOP: "Shops",
        LocationType.SPECIAL: "SpecialAreas",
    }

    subdir = type_dirs.get(loc_type, "Other")

    if loc_type == LocationType.DUNGEON:
        filename = f"{safe_name}_Map.md"
    else:
        filename = f"{safe_name}.md"

    # Use profile's docs_subdir (relative to ROM directory)
    docs_base = os.path.join(rom_dir, profile.docs_subdir)
    return os.path.join(docs_base, subdir, filename)


# =============================================================================
# CLI
# =============================================================================

def list_profiles_command():
    """List all available ROM profiles."""
    print("\nAvailable ROM Profiles:")
    print("-" * 60)
    for profile in REGISTERED_PROFILES:
        dungeon_count = len(profile.get_locations_by_type(LocationType.DUNGEON))
        total_count = len(profile.locations)
        print(f"  {profile.short_name:12} {profile.name:35} ({total_count} locations, {dungeon_count} dungeons)")
    print()


def list_locations(profile: RomProfile, filter_type: Optional[str] = None):
    """List all configured locations for a profile."""

    print(f"\n{profile.name} ({profile.short_name})")
    print("=" * 60)

    by_type = defaultdict(list)
    for key, config in profile.locations.items():
        by_type[config.type].append((key, config))

    for loc_type in LocationType:
        if filter_type and loc_type.value != filter_type:
            continue

        locations = by_type.get(loc_type, [])
        if not locations:
            continue

        print(f"\n{loc_type.value.upper()}S ({len(locations)}):")
        print("-" * 60)

        for key, config in sorted(locations, key=lambda x: x[1].name):
            entrance_str = ", ".join(f"0x{e:02X}" for e in config.entrance_ids[:3])
            if len(config.entrance_ids) > 3:
                entrance_str += "..."
            room_count = len(config.all_rooms)

            status = "mapped" if room_count > 0 else "unmapped"
            print(f"  {key:30} {config.name:25} [{entrance_str}] ({status})")


def main():
    parser = argparse.ArgumentParser(
        description="Generate location documentation for ALTTP ROMs (multi-ROM support)"
    )
    parser.add_argument("--rom", default=DEFAULT_ROM, help="Path to ROM file")
    parser.add_argument("--profile", help="Use specific profile (vanilla, oracle)")
    parser.add_argument("--profiles", action="store_true", help="List available profiles")
    parser.add_argument("--location", help="Location key to generate")
    parser.add_argument("--type", choices=[t.value for t in LocationType],
                        help="Filter by location type")
    parser.add_argument("--list", action="store_true", help="List available locations")
    parser.add_argument("--discover", type=lambda x: int(x, 0),
                        help="Discover rooms from entrance ID (hex)")
    parser.add_argument("--tracks", action="store_true", help="Show track details")
    parser.add_argument("--room", type=lambda x: int(x, 0), help="Specific room (hex)")
    parser.add_argument("--json", action="store_true", help="JSON output")
    parser.add_argument("--output", help="Output file path (default: stdout)")
    parser.add_argument("--output-dir", help="Override output directory for --save")
    parser.add_argument("--save", action="store_true",
                        help="Save to appropriate Docs/World/<Type>/ directory")

    args = parser.parse_args()

    # Handle --profiles first
    if args.profiles:
        list_profiles_command()
        return

    # Determine which profile to use
    profile = None

    if args.profile:
        # Explicit profile selection
        profile = get_profile_by_name(args.profile)
        if not profile:
            print(f"Error: Unknown profile '{args.profile}'")
            print(f"Available profiles: {', '.join(list_profiles())}")
            return 1
    else:
        # Auto-detect from ROM
        if os.path.exists(args.rom):
            profile = detect_rom_profile(args.rom)
            if profile:
                print(f"Detected ROM: {profile.name}", file=sys.stderr)
            else:
                print(f"Could not detect ROM type, using Oracle profile", file=sys.stderr)
                profile = ORACLE_PROFILE
        else:
            print(f"ROM not found: {args.rom}", file=sys.stderr)
            profile = ORACLE_PROFILE

    # Handle --list
    if args.list:
        list_locations(profile, args.type)
        return

    # Handle --discover
    if args.discover:
        entrance_info = get_entrance_info(args.rom, args.discover)
        if entrance_info:
            print(json.dumps(entrance_info, indent=2))
        else:
            print(f"Room discovery from entrance 0x{args.discover:02X}:")
            print("  Use z3ed entrance-info command for entrance table data.")
            print(f"  Example: z3ed entrance-info --rom={args.rom} --entrance=0x{args.discover:02X}")
        return

    # Handle --room with --json
    if args.room and args.json:
        objects = get_room_objects(args.rom, args.room)
        print(json.dumps(objects, indent=2))
        return

    # Handle --location
    if not args.location:
        parser.print_help()
        print("\n\nUse --list to see available locations")
        print("Use --profiles to see available ROM profiles")
        return

    # Generate documentation
    output = generate_location_doc(args.rom, args.location, profile)

    if args.save:
        rom_dir = os.path.dirname(os.path.abspath(args.rom))
        if args.output_dir:
            # Override with custom output directory
            docs_base = args.output_dir
            config = profile.get_location(args.location)
            if config:
                type_dirs = {
                    LocationType.DUNGEON: "Dungeons",
                    LocationType.SHRINE: "Shrines",
                    LocationType.CAVE: "Caves",
                    LocationType.HOUSE: "Houses",
                    LocationType.SHOP: "Shops",
                    LocationType.SPECIAL: "SpecialAreas",
                }
                subdir = type_dirs.get(config.type, "Other")
                safe_name = config.name.replace(' ', '').replace("'", "")
                filename = f"{safe_name}_Map.md" if config.type == LocationType.DUNGEON else f"{safe_name}.md"
                filepath = os.path.join(docs_base, subdir, filename)
            else:
                filepath = None
        else:
            filepath = get_output_path(args.location, profile, rom_dir)

        if filepath:
            os.makedirs(os.path.dirname(filepath), exist_ok=True)
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
