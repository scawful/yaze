#!/usr/bin/env python3
import sys
import os
import argparse
from typing import Dict, Any, Callable

# Add Oracle scripts to path
ORACLE_SCRIPTS = os.path.expanduser("~/src/hobby/oracle-of-secrets/scripts")
sys.path.append(ORACLE_SCRIPTS)

from mesen2_client_lib.client import OracleDebugClient
from mesen2_client_lib.constants import OracleRAM, GameMode, ITEMS, STORY_FLAGS

class StateQuery:
    def __init__(self):
        self.client = OracleDebugClient()
        # Cache for single-frame queries to avoid redundant reads?
        # For now, read live.

    def ensure_connected(self):
        if not self.client.ensure_connected():
            print("Error: Could not connect to Mesen2.")
            sys.exit(1)

    # --- Predicates ---

    def is_overworld(self) -> bool:
        mode = self.client.read_address(OracleRAM.MODE)
        indoors = self.client.read_address(OracleRAM.INDOORS)
        return mode == GameMode.OVERWORLD and indoors == 0

    def is_dungeon(self) -> bool:
        mode = self.client.read_address(OracleRAM.MODE)
        # Dungeon mode is 0x07. Also check indoors flag for safety?
        return mode == GameMode.DUNGEON

    def is_in_cutscene(self) -> bool:
        # Check specific cutscene flag
        val = self.client.read_address(OracleRAM.IN_CUTSCENE)
        return val != 0

    def is_link_control(self) -> bool:
        """Can the player control Link?"""
        mode = self.client.read_address(OracleRAM.MODE)
        submode = self.client.read_address(OracleRAM.SUBMODE)
        # 0x00 is PLAYER_CONTROL in both OW (09) and Dungeon (07)?
        # Need to verify dungeon submodes. Assuming 0x00 is generally 'play'.
        if mode == GameMode.OVERWORLD or mode == GameMode.DUNGEON:
            return submode == 0
        return False

    def is_safe(self) -> bool:
        """Is Link safe (not taking damage, not falling, health > 0)?"""
        hp = self.client.read_address(OracleRAM.HEALTH_CURRENT)
        state = self.client.read_address(OracleRAM.LINK_STATE)
        # State 0 is usually 'grounded/neutral'. 
        # Need to map Link States. 
        # Assuming non-zero might be action/damage.
        # Ideally we check an "invincibility timer" or "recoil timer".
        # For now: HP > 0.
        return hp > 0

    def has_item(self, item_name: str) -> bool:
        """Check if player has an item."""
        if item_name.lower() not in ITEMS:
            return False
        val, _ = self.client.get_item(item_name.lower())
        return val > 0

    def get_rupee_count(self) -> int:
        val, _ = self.client.get_item("rupees")
        return val

    # --- Reflection ---

    def query(self, query_str: str) -> Any:
        """Execute a natural language-like query."""
        q = query_str.lower().strip()
        
        # 1. Direct function map
        if q == "is_safe": return self.is_safe()
        if q == "is_overworld": return self.is_overworld()
        if q == "is_dungeon": return self.is_dungeon()
        if q == "in_cutscene": return self.is_in_cutscene()
        if q == "can_control": return self.is_link_control()
        
        # 2. Parameterized queries
        if q.startswith("has "):
            item = q.split(" ")[1]
            return self.has_item(item)
            
        if q == "rupees": return self.get_rupee_count()

        return "Unknown Query"

def main():
    parser = argparse.ArgumentParser(description="Semantic State Query")
    parser.add_argument("query", nargs="+", help="Query string (e.g. 'is_safe', 'has bow')")
    
    args = parser.parse_args()
    query_str = " ".join(args.query)
    
    sq = StateQuery()
    sq.ensure_connected()
    
    result = sq.query(query_str)
    print(f"{query_str} -> {result}")

if __name__ == "__main__":
    main()
