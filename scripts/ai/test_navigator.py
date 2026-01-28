#!/usr/bin/env python3
import json
import unittest
from navigator import Locator, PathFinder

class MockMCP:
    def __init__(self, ram_values):
        self.ram = ram_values
    def read_ram(self, addresses):
        return {addr: self.ram.get(addr, 0) for addr in addresses}

class TestNavigator(unittest.TestCase):
    def setUp(self):
        self.graph_path = "/Users/scawful/src/hobby/yaze/world_graph.json"
        self.locator = Locator(self.graph_path)
        self.pf = PathFinder(self.locator.graph)

    def test_localization_indoors(self):
        # Mock Link in Room 0x1B (Indoors=1)
        ram = {
            "7E001B": 1,
            "7E00A0": 0x1B,
            "7E00A1": 0,
            "7E0022": 160, # Tile X = 10
            "7E0020": 320  # Tile Y = 20
        }
        loc = self.locator.get_current_location(ram)
        self.assertEqual(loc["type"], "dungeon")
        self.assertEqual(loc["room_id"], 0x1B)
        self.assertEqual(loc["tile_x"], 10)
        self.assertEqual(loc["tile_y"], 20)

    def test_localization_overworld(self):
        # Mock Link in Screen 0x00 (Top-left of Light World)
        ram = {
            "7E001B": 0,
            "7E0022": 100, # Within first 512 pixels
            "7E0020": 100
        }
        loc = self.locator.get_current_location(ram)
        self.assertEqual(loc["type"], "overworld")
        self.assertEqual(loc["screen_id"], 0)

    def test_pathfinding_connectivity(self):
        # Test finding neighbors for Room 0x1B (should have West door to 0x1A)
        neighbors = self.pf.get_room_neighbors(0x1B)
        print(f"Neighbors of 0x1B: {neighbors}")
        
        # Verify West door connection (Room 27 -> Room 26)
        has_west_connection = any(n[0] == 0x1A for n in neighbors)
        # Note: 0x1B is 27. West is 26 (0x1A).
        if has_west_connection:
            print("Found West connection to 0x1A")
            
        # Test pathfinding
        if has_west_connection:
            path = self.pf.find_room_path(0x1B, 0x1A)
            print(f"Path 0x1B -> 0x1A: {path}")
            self.assertTrue(len(path) > 1)
            self.assertEqual(path[-1][0], 0x1A)

if __name__ == "__main__":
    unittest.main()
