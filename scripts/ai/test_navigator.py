#!/usr/bin/env python3
"""Unit tests for scripts/ai/navigator.py.

The world graph is a generated artifact (see scripts/ai/map_compiler.py) and is
gitignored, so these tests skip when it is absent.

Run: python3 -m unittest discover -s scripts/ai
Override the graph location with YAZE_WORLD_GRAPH.
"""

import os
import sys
import unittest
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))

from navigator import Locator, PathFinder  # noqa: E402

REPO_ROOT = Path(__file__).resolve().parents[2]
GRAPH_PATH = os.environ.get(
    "YAZE_WORLD_GRAPH", str(REPO_ROOT / "world_graph.json")
)


@unittest.skipUnless(
    os.path.exists(GRAPH_PATH),
    f"world graph not found at {GRAPH_PATH}; "
    "generate it with scripts/ai/map_compiler.py or set YAZE_WORLD_GRAPH",
)
class TestNavigator(unittest.TestCase):
    def setUp(self):
        self.locator = Locator(GRAPH_PATH)
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
        # Room 0x1B (27) is expected to have a West door to Room 0x1A (26).
        neighbors = self.pf.get_room_neighbors(0x1B)
        self.assertTrue(
            any(n[0] == 0x1A for n in neighbors),
            f"expected a 0x1B -> 0x1A connection, got {neighbors}",
        )

        path = self.pf.find_room_path(0x1B, 0x1A)
        self.assertGreater(len(path), 1)
        self.assertEqual(path[-1][0], 0x1A)


if __name__ == "__main__":
    unittest.main()
