#!/usr/bin/env python3
"""
Room Discovery Script for ALTTP ROMs

Uses z3ed entrance-info and dungeon-discover commands to auto-discover
dungeon room layouts. Can update profile configurations with discovered rooms.

Usage:
    # Discover all rooms reachable from entrance 0x08 (Eastern Palace)
    python3 discover_rooms.py --rom=vanilla.sfc --entrance=0x08

    # Discover with custom depth limit
    python3 discover_rooms.py --rom=vanilla.sfc --entrance=0x08 --depth=15

    # Output as JSON for further processing
    python3 discover_rooms.py --rom=vanilla.sfc --entrance=0x08 --json

    # Update profile configuration with discovered rooms
    python3 discover_rooms.py --rom=vanilla.sfc --entrance=0x08 --update-profile

    # Scan multiple entrances at once
    python3 discover_rooms.py --rom=vanilla.sfc --entrances=0x08,0x09,0x0A
"""

import argparse
import json
import os
import subprocess
import sys
from typing import Optional, List, Dict, Set, Tuple
from dataclasses import dataclass, field

# Path to z3ed binary
# Prefer the repo wrapper script which selects the newest build (build_ai first).
Z3ED = os.environ.get(
    "Z3ED_BIN",
    os.environ.get(
        "Z3ED_PATH",
        os.path.join(os.path.dirname(__file__), "z3ed"),
    ),
)


@dataclass
class RoomDiscovery:
    """Results from a room discovery operation."""
    entrance_id: int
    start_room: int
    dungeon_id: int
    discovered_rooms: List[int] = field(default_factory=list)
    room_names: Dict[int, str] = field(default_factory=dict)
    connections: List[Dict] = field(default_factory=list)


def run_z3ed(command: str, rom_path: str, **kwargs) -> Optional[dict]:
    """Run a z3ed command and return JSON output."""
    args = [Z3ED, command, f"--rom={rom_path}"]
    for key, value in kwargs.items():
        args.append(f"--{key.replace('_', '-')}={value}")

    try:
        result = subprocess.run(args, capture_output=True, text=True, timeout=60)
        if result.returncode != 0:
            print(f"z3ed error: {result.stderr}", file=sys.stderr)
            return None
        return json.loads(result.stdout)
    except subprocess.TimeoutExpired:
        print(f"z3ed timeout for: {' '.join(args)}", file=sys.stderr)
        return None
    except json.JSONDecodeError as e:
        print(f"JSON parse error: {e}", file=sys.stderr)
        print(f"Output was: {result.stdout[:500]}", file=sys.stderr)
        return None
    except FileNotFoundError:
        print(f"z3ed not found at: {Z3ED}", file=sys.stderr)
        print("Set Z3ED_PATH environment variable or build z3ed", file=sys.stderr)
        return None


def get_entrance_info(rom_path: str, entrance_id: int) -> Optional[dict]:
    """Get entrance table data."""
    return run_z3ed("entrance-info", rom_path, entrance=f"0x{entrance_id:02X}")


def discover_rooms(rom_path: str, entrance_id: int, depth: int = 20) -> Optional[RoomDiscovery]:
    """
    Discover all rooms reachable from an entrance.

    Uses z3ed dungeon-discover command which performs BFS through
    stair and holewarp connections.
    """
    data = run_z3ed("dungeon-discover", rom_path,
                    entrance=f"0x{entrance_id:02X}",
                    depth=str(depth))

    if not data or "discovery" not in data:
        return None

    discovery = data["discovery"]

    result = RoomDiscovery(
        entrance_id=entrance_id,
        start_room=int(discovery.get("start_room", "0x00"), 16),
        dungeon_id=int(discovery.get("dungeon_id", "0x00"), 16),
    )

    # Parse discovered rooms
    for room in discovery.get("discovered_rooms", []):
        room_id = int(room.get("room_id", "0x00"), 16)
        result.discovered_rooms.append(room_id)
        result.room_names[room_id] = room.get("name", f"Room 0x{room_id:02X}")

    # Parse connections
    result.connections = discovery.get("connections", [])

    return result


def discover_rooms_fallback(rom_path: str, entrance_id: int, depth: int = 20) -> Optional[RoomDiscovery]:
    """
    Fallback room discovery using entrance-info and dungeon-graph.

    Use this if dungeon-discover is not available.
    """
    # Get entrance info first
    entrance_data = get_entrance_info(rom_path, entrance_id)
    if not entrance_data or "entrance" not in entrance_data:
        print(f"Could not get entrance info for 0x{entrance_id:02X}", file=sys.stderr)
        return None

    entrance = entrance_data["entrance"]
    start_room = int(entrance.get("room_id", "0x00"), 16)
    dungeon_id = int(entrance.get("dungeon_id", "0x00"), 16)

    # Use dungeon-graph to get room connections
    graph_data = run_z3ed("dungeon-graph", rom_path, room=f"0x{start_room:02X}")

    if not graph_data or "dungeon_graph" not in graph_data:
        # Return just the entrance room if we can't get graph data
        return RoomDiscovery(
            entrance_id=entrance_id,
            start_room=start_room,
            dungeon_id=dungeon_id,
            discovered_rooms=[start_room],
            room_names={start_room: f"Entrance Room 0x{start_room:02X}"},
        )

    # BFS from start room using graph data
    discovered = set([start_room])
    to_visit = [start_room]
    connections = []
    room_names = {}

    visited_count = 0
    while to_visit and visited_count < depth * 10:  # Safety limit
        visited_count += 1
        current = to_visit.pop(0)

        # Get graph for current room
        room_graph = run_z3ed("dungeon-graph", rom_path, room=f"0x{current:02X}")
        if not room_graph or "dungeon_graph" not in room_graph:
            continue

        nodes = room_graph["dungeon_graph"].get("nodes", [])
        edges = room_graph["dungeon_graph"].get("edges", [])

        # Get room name from node data
        for node in nodes:
            room_id_str = node.get("room_id", "0x00")
            room_id = int(room_id_str, 16)
            if room_id == current:
                room_names[room_id] = node.get("name", f"Room 0x{room_id:02X}")
                break

        # Follow edges to discover new rooms
        for edge in edges:
            from_room = int(edge.get("from", "0x00"), 16)
            to_room = int(edge.get("to", "0x00"), 16)

            if from_room == current:
                connections.append({
                    "from": f"0x{from_room:02X}",
                    "to": f"0x{to_room:02X}",
                    "type": edge.get("type", "unknown")
                })

                if to_room not in discovered and len(discovered) < depth:
                    discovered.add(to_room)
                    to_visit.append(to_room)

    return RoomDiscovery(
        entrance_id=entrance_id,
        start_room=start_room,
        dungeon_id=dungeon_id,
        discovered_rooms=sorted(list(discovered)),
        room_names=room_names,
        connections=connections,
    )


def print_discovery_results(discovery: RoomDiscovery, as_json: bool = False):
    """Print discovery results in human-readable or JSON format."""

    if as_json:
        output = {
            "entrance_id": f"0x{discovery.entrance_id:02X}",
            "start_room": f"0x{discovery.start_room:02X}",
            "dungeon_id": f"0x{discovery.dungeon_id:02X}",
            "rooms_discovered": len(discovery.discovered_rooms),
            "discovered_rooms": [f"0x{r:02X}" for r in discovery.discovered_rooms],
            "room_names": {f"0x{k:02X}": v for k, v in discovery.room_names.items()},
            "connections": discovery.connections,
        }
        print(json.dumps(output, indent=2))
        return

    # Human-readable output
    print(f"\n{'=' * 60}")
    print(f"Room Discovery Results")
    print(f"{'=' * 60}")
    print(f"Entrance ID:    0x{discovery.entrance_id:02X}")
    print(f"Start Room:     0x{discovery.start_room:02X}")
    print(f"Dungeon ID:     0x{discovery.dungeon_id:02X}")
    print(f"Rooms Found:    {len(discovery.discovered_rooms)}")

    print(f"\n{'Discovered Rooms':}")
    print("-" * 40)
    for room_id in discovery.discovered_rooms:
        name = discovery.room_names.get(room_id, "Unknown")
        print(f"  0x{room_id:02X}  {name}")

    if discovery.connections:
        print(f"\n{'Room Connections':}")
        print("-" * 40)
        for conn in discovery.connections:
            print(f"  {conn['from']} --[{conn['type']}]--> {conn['to']}")

    # Generate Python code for profile update
    print(f"\n{'Profile Config (Python):':}")
    print("-" * 40)
    print(f"all_rooms=[")
    for i, room_id in enumerate(discovery.discovered_rooms):
        if i > 0 and i % 8 == 0:
            print()
        print(f"    0x{room_id:02X},", end="")
    print("\n],")
    print(f"room_names={{")
    for room_id, name in sorted(discovery.room_names.items()):
        print(f"    0x{room_id:02X}: \"{name}\",")
    print(f"}},")


def generate_profile_patch(discovery: RoomDiscovery) -> str:
    """Generate Python code to patch a profile with discovered rooms."""
    lines = [
        f"# Room discovery patch for entrance 0x{discovery.entrance_id:02X}",
        f"# Dungeon ID: 0x{discovery.dungeon_id:02X}",
        f"# Total rooms: {len(discovery.discovered_rooms)}",
        f"",
        f"all_rooms=[",
    ]

    # Format room list
    rooms_per_line = 8
    for i in range(0, len(discovery.discovered_rooms), rooms_per_line):
        chunk = discovery.discovered_rooms[i:i+rooms_per_line]
        line = "    " + ", ".join(f"0x{r:02X}" for r in chunk) + ","
        lines.append(line)
    lines.append("],")

    lines.append("")
    lines.append("room_names={")
    for room_id in sorted(discovery.room_names.keys()):
        name = discovery.room_names[room_id]
        lines.append(f"    0x{room_id:02X}: \"{name}\",")
    lines.append("},")

    return "\n".join(lines)


def main():
    parser = argparse.ArgumentParser(
        description="Discover dungeon rooms from entrance IDs using z3ed"
    )
    parser.add_argument("--rom", required=True, help="Path to ROM file")
    parser.add_argument("--entrance", type=lambda x: int(x, 0),
                        help="Entrance ID (hex)")
    parser.add_argument("--entrances", type=str,
                        help="Comma-separated list of entrance IDs (hex)")
    parser.add_argument("--depth", type=int, default=20,
                        help="Maximum discovery depth (default: 20)")
    parser.add_argument("--json", action="store_true",
                        help="Output as JSON")
    parser.add_argument("--update-profile", action="store_true",
                        help="Generate profile update patch")
    parser.add_argument("--output", type=str,
                        help="Output file for patch (with --update-profile)")
    parser.add_argument("--fallback", action="store_true",
                        help="Use fallback discovery method (entrance-info + dungeon-graph)")

    args = parser.parse_args()

    # Check ROM exists
    if not os.path.exists(args.rom):
        print(f"ROM not found: {args.rom}", file=sys.stderr)
        return 1

    # Parse entrance IDs
    entrance_ids = []
    if args.entrance is not None:
        entrance_ids.append(args.entrance)
    if args.entrances:
        for e in args.entrances.split(","):
            entrance_ids.append(int(e.strip(), 0))

    if not entrance_ids:
        print("No entrance IDs specified. Use --entrance or --entrances", file=sys.stderr)
        return 1

    # Discover rooms for each entrance
    all_discoveries = []
    for entrance_id in entrance_ids:
        print(f"Discovering rooms from entrance 0x{entrance_id:02X}...", file=sys.stderr)

        if args.fallback:
            discovery = discover_rooms_fallback(args.rom, entrance_id, args.depth)
        else:
            discovery = discover_rooms(args.rom, entrance_id, args.depth)
            if discovery is None:
                print(f"Primary discovery failed, trying fallback...", file=sys.stderr)
                discovery = discover_rooms_fallback(args.rom, entrance_id, args.depth)

        if discovery:
            all_discoveries.append(discovery)
            if not args.json:
                print_discovery_results(discovery, as_json=False)
        else:
            print(f"Discovery failed for entrance 0x{entrance_id:02X}", file=sys.stderr)

    # JSON output for all discoveries
    if args.json and all_discoveries:
        if len(all_discoveries) == 1:
            print_discovery_results(all_discoveries[0], as_json=True)
        else:
            output = []
            for d in all_discoveries:
                output.append({
                    "entrance_id": f"0x{d.entrance_id:02X}",
                    "start_room": f"0x{d.start_room:02X}",
                    "dungeon_id": f"0x{d.dungeon_id:02X}",
                    "discovered_rooms": [f"0x{r:02X}" for r in d.discovered_rooms],
                    "room_names": {f"0x{k:02X}": v for k, v in d.room_names.items()},
                })
            print(json.dumps(output, indent=2))

    # Generate profile patches
    if args.update_profile and all_discoveries:
        patches = []
        for discovery in all_discoveries:
            patches.append(generate_profile_patch(discovery))

        patch_text = "\n\n".join(patches)

        if args.output:
            with open(args.output, 'w') as f:
                f.write(patch_text)
            print(f"\nProfile patch written to: {args.output}", file=sys.stderr)
        else:
            print("\n" + "=" * 60)
            print("PROFILE UPDATE PATCH")
            print("=" * 60)
            print(patch_text)

    return 0


if __name__ == "__main__":
    sys.exit(main())
