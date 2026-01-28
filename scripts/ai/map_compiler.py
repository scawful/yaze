#!/usr/bin/env python3
import json
import subprocess
import os
import argparse
from typing import Dict, List, Any

class MapCompiler:
    def __init__(self, z3ed_path: str, rom_path: str):
        self.z3ed_path = z3ed_path
        self.rom_path = rom_path
        self.graph = {
            "dungeons": {},
            "overworld": {},
            "entrances": {} # Map entrance_id -> target room
        }

    def run_z3ed(self, args: List[str]) -> Dict[str, Any]:
        cmd = [self.z3ed_path] + args + ["--rom=" + self.rom_path, "--format=json", "--quiet"]
        try:
            result = subprocess.run(cmd, capture_output=True, text=True, check=True)
            output = result.stdout
            
            # Find the actual JSON boundaries
            start = output.find('{')
            end = output.rfind('}')
            if start == -1 or end == -1:
                print(f"Failed to find JSON in output: {output}")
                return {}
            
            json_str = output[start:end+1]
            
            # Handle the "double brace" bug if present: {{ ... }}
            # If it starts with {{ and ends with }}, try to strip one layer
            if json_str.startswith('{{') and json_str.endswith('}}'):
                json_str = json_str[1:-1]

            return json.loads(json_str)
        except Exception as e:
            print(f"Error running z3ed {args}: {e}")
            return {}

    def compile_dungeons(self):
        print("Compiling Dungeons...")
        for room_id in range(296):
            # print(f"  Room {room_id:03X}...", end="\r")
            data = self.run_z3ed(["dungeon-describe-room", f"--room=0x{room_id:X}"])
            if data and data.get("status") == "success":
                self.graph["dungeons"][room_id] = {
                    "name": data.get("name"),
                    "doors": data.get("doors", []),
                    "staircases": data.get("staircases", []),
                    "chests": data.get("chests", [])
                }
                if room_id == 27:
                    print(f"Added Room 27 to graph. Doors: {len(data.get('doors', []))}")
            elif room_id == 27:
                print(f"Failed to add Room 27. Data: {data}")
        print("\nDungeon compilation complete.")

    def compile_overworld(self):
        print("Compiling Overworld...")
        for screen_id in range(160):
            print(f"  Screen {screen_id:02X}...", end="\r")
            data = self.run_z3ed(["overworld-describe-map", f"--screen=0x{screen_id:X}"])
            if data:
                self.graph["overworld"][screen_id] = {
                    "world": data.get("world"),
                    "entrances": data.get("entrances", []),
                    "exits": data.get("exits", [])
                }
        print("\nOverworld compilation complete.")

    def compile_entrances(self):
        print("Compiling Entrance Metadata...")
        for ent_id in range(132): # Up to 0x84
            print(f"  Entrance {ent_id:02X}...", end="\r")
            data = self.run_z3ed(["dungeon-get-entrance", f"--entrance=0x{ent_id:X}"])
            if data:
                # Convert hex string room_id to int
                room_id_str = data.get("room_id", "0x0000")
                room_id = int(room_id_str, 16)
                self.graph["entrances"][ent_id] = {
                    "room_id": room_id,
                    "pos": data.get("position", {})
                }
        print("\nEntrance compilation complete.")

    def post_process_connectivity(self):
        print("Post-processing connectivity...")
        # Link Dungeon Rooms
        for room_id_str, room_data in self.graph["dungeons"].items():
            room_id = int(room_id_str)
            neighbors = []
            
            # Debug for Room 27 (0x1B)
            if room_id == 27:
                print(f"Processing Room 27. Doors: {len(room_data.get('doors', []))}")

            # 1. Door Inference (Grid Layout)
            for door in room_data.get("doors", []):
                direction = door.get("direction", "")
                target = -1
                if direction == "North": target = room_id - 16
                elif direction == "South": target = room_id + 16
                elif direction == "East": target = room_id + 1
                elif direction == "West": target = room_id - 1
                
                if room_id == 27:
                    print(f"  Door Dir: {direction}, Target: {target}, In Graph: {target in self.graph['dungeons']}")

                if target >= 0 and target in self.graph["dungeons"]:
                    neighbors.append({"type": "door", "target": target, "direction": direction})
                    door["target_room"] = target # Enrich the door object

            # 2. Staircase Targets
            for stair in room_data.get("staircases", []):
                label = stair.get("label", "")
                if "To " in label:
                    try:
                        target = int(label.replace("To ", ""))
                        neighbors.append({"type": "stair", "target": target})
                        stair["target_room"] = target
                    except: pass

            room_data["neighbors"] = neighbors

        # Link Overworld Entrances
        for screen_id_str, screen_data in self.graph["overworld"].items():
            screen_id = int(screen_id_str)
            # Map entrances to rooms
            for ent in screen_data.get("entrances", []):
                ent_id = ent.get("entrance_id")
                if ent_id in self.graph["entrances"]:
                    target_room = self.graph["entrances"][ent_id]["room_id"]
                    ent["target_room"] = target_room
            
            # Map exits (already have room_id)
            for exit in screen_data.get("exits", []):
                # Ensure consistency
                pass

        print("Connectivity graph built.")

    def save(self, output_path: str):
        self.post_process_connectivity()
        with open(output_path, "w") as f:
            json.dump(self.graph, f, indent=2)
        print(f"Graph saved to {output_path}")

def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--z3ed", required=True, help="Path to z3ed binary")
    parser.add_argument("--rom", required=True, help="Path to ROM file")
    parser.add_argument("--output", default="world_graph.json", help="Output JSON path")
    args = parser.parse_args()

    compiler = MapCompiler(args.z3ed, args.rom)
    compiler.compile_dungeons()
    compiler.compile_overworld()
    compiler.compile_entrances()
    compiler.save(args.output)

if __name__ == "__main__":
    main()
