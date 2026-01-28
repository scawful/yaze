#!/usr/bin/env python3
import json
import time
from typing import Dict, List, Any, Optional, Tuple

class Locator:
    def __init__(self, graph_path: str):
        with open(graph_path, 'r') as f:
            self.graph = json.load(f)
            
    def get_current_location(self, ram: Dict[str, int]) -> Dict[str, Any]:
        indoors = ram.get("7E001B", 0) == 1
        room_id = ram.get("7E00A0", 0) | (ram.get("7E00A1", 0) << 8)
        x = ram.get("7E0022", 0)
        y = ram.get("7E0020", 0)
        
        if indoors:
            room_data = self.graph["dungeons"].get(str(room_id), {})
            return {
                "type": "dungeon",
                "room_id": room_id,
                "name": room_data.get("name"),
                "local_x": x, # 0-511
                "local_y": y, # 0-511
                "tile_x": x // 16,
                "tile_y": y // 16
            }
        else:
            # Overworld Screen calculation
            # Each screen is 512x512 pixels
            screen_x = x // 512
            screen_y = y // 512
            screen_id = screen_y * 8 + screen_x # Basic 8x8 grid for LW/DW
            
            screen_data = self.graph["overworld"].get(str(screen_id), {})
            return {
                "type": "overworld",
                "screen_id": screen_id,
                "world": screen_data.get("world"),
                "world_x": x,
                "world_y": y,
                "local_x": x % 512,
                "local_y": y % 512,
                "tile_x": (x % 512) // 16,
                "tile_y": (y % 512) // 16
            }

class PathFinder:
    def __init__(self, graph: Dict):
        self.graph = graph

    def find_room_path(self, start_room: int, end_room: int) -> List[Any]:
        # BFS returning list of (room_id, action_to_get_there)
        queue = [[(start_room, None)]]
        visited = {start_room}
        
        while queue:
            path = queue.pop(0)
            node, _ = path[-1]
            
            if node == end_room:
                return path
                
            neighbors = self.get_room_neighbors(node)
            for target, action in neighbors:
                if target not in visited:
                    visited.add(target)
                    new_path = list(path)
                    new_path.append((target, action))
                    queue.append(new_path)
        return []

    def get_room_neighbors(self, room_id: int) -> List[Tuple[int, Dict]]:
        neighbors = []
        room_data = self.graph["dungeons"].get(str(room_id), {})
        
        for neighbor in room_data.get("neighbors", []):
            target = neighbor.get("target")
            if target is not None:
                neighbors.append((target, neighbor))
                
        return neighbors

class Navigator:
    def __init__(self, locator: Locator, mcp_client: Any):
        self.locator = locator
        self.mcp = mcp_client
        self.path_finder = PathFinder(locator.graph)

    def move_to_tile(self, target_tile_x: int, target_tile_y: int):
        """Micro-pathing: Move Link to a specific tile in the current room."""
        while True:
            ram = self.mcp.read_ram(["7E0022", "7E0020", "7E001B", "7E00A0"])
            loc = self.locator.get_current_location(ram)
            
            curr_x, curr_y = loc["tile_x"], loc["tile_y"]
            if curr_x == target_tile_x and curr_y == target_tile_y:
                break
                
            # Simple vector movement
            dx = target_tile_x - curr_x
            dy = target_tile_y - curr_y
            
            inputs = []
            if dx > 0: inputs.append("RIGHT")
            elif dx < 0: inputs.append("LEFT")
            if dy > 0: inputs.append("DOWN")
            elif dy < 0: inputs.append("UP")
            
            self.mcp.press(",".join(inputs), frames=5)
            time.sleep(0.05)

def main():
    print("Hyrule Navigator Module Loaded")

if __name__ == "__main__":
    main()
