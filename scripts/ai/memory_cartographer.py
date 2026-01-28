#!/usr/bin/env python3
import sys
import os
import time
import argparse
import json
from typing import Dict, List, Set, Optional

# Add Oracle scripts to path to find mesen2_client_lib
ORACLE_SCRIPTS = os.path.expanduser("~/src/hobby/oracle-of-secrets/scripts")
sys.path.append(ORACLE_SCRIPTS)

from mesen2_client_lib.client import OracleDebugClient

class MemoryCartographer:
    def __init__(self):
        self.client = OracleDebugClient()
        if not self.client.ensure_connected():
            print("Error: Could not connect to Mesen2.")
            sys.exit(1)
        
        self.baseline: Dict[int, int] = {}
        self.current: Dict[int, int] = {}
        self.candidates: Set[int] = set()
        self.history: Dict[int, List[int]] = {} # Track changes over time for noise filtering

    def scan_range(self, start_addr: int, end_addr: int) -> Dict[int, int]:
        """Read a range of memory from Mesen2."""
        # This is slow byte-by-byte. Ideally we'd use a bulk read command if available.
        # client.read_memory is byte-by-byte. 
        # But bridge.read_memory uses 'read_memory' lua command? 
        # Let's check if there's a bulk read. The MesenBridge doesn't seem to have one exposed cleanly 
        # in the Python client yet, but we can implement a loop.
        # Optimally: Use `MEMORY_SEARCH` logic or add `READ_BLOCK` to bridge.
        # For now, we will scan small ranges or accept slowness.
        
        # Optimization: Use BATCH command if possible, or just loop.
        snapshot = {}
        # Reading 0x100 bytes takes time. Let's limit scan size or warn user.
        if (end_addr - start_addr) > 0x400:
            print(f"Warning: Scanning {end_addr - start_addr} bytes may be slow.")
            
        for addr in range(start_addr, end_addr + 1):
            val = self.client.bridge.read_memory(addr)
            snapshot[addr] = val
        return snapshot

    def start_scan(self, start: int, end: int):
        print(f"Scanning range 0x{start:X} - 0x{end:X}...")
        self.baseline = self.scan_range(start, end)
        self.candidates = set(self.baseline.keys())
        print(f"Baseline captured. {len(self.baseline)} addresses.")

    def update_scan(self):
        if not self.baseline:
            print("No active scan. Use 'start' first.")
            return

        print("Updating scan...")
        # Only re-scan candidates to save time? 
        # No, we might want to find things that changed *later*.
        # But 'candidates' usually implies "addresses that match criteria".
        # If we are just tracking changes from baseline, we need to read everything.
        
        # If candidates set is small, scan only those.
        # If it's the first update, scan all baseline keys.
        targets = sorted(list(self.candidates))
        if not targets:
            return

        for addr in targets:
            val = self.client.bridge.read_memory(addr)
            self.current[addr] = val
            
            # Update history for noise detection
            if addr not in self.history:
                self.history[addr] = []
            self.history[addr].append(val)
            if len(self.history[addr]) > 10:
                self.history[addr].pop(0)

    def filter_changed(self):
        """Keep addresses that have changed from baseline."""
        new_candidates = set()
        for addr in self.candidates:
            if self.current.get(addr) != self.baseline.get(addr):
                new_candidates.add(addr)
        
        removed = len(self.candidates) - len(new_candidates)
        self.candidates = new_candidates
        print(f"Filter 'changed': Kept {len(self.candidates)}, Removed {removed}")

    def filter_stable(self):
        """Keep addresses that have NOT changed from baseline (or previous frame)."""
        # Usually 'stable' means "has not changed since last update".
        new_candidates = set()
        for addr in self.candidates:
            # Check history: is it stable over the last N updates?
            if len(self.history.get(addr, [])) >= 2:
                last = self.history[addr][-1]
                prev = self.history[addr][-2]
                if last == prev:
                    new_candidates.add(addr)
            else:
                # Not enough history, keep it? or assuming stable if equal to baseline?
                if self.current.get(addr) == self.baseline.get(addr):
                    new_candidates.add(addr)

        removed = len(self.candidates) - len(new_candidates)
        self.candidates = new_candidates
        print(f"Filter 'stable': Kept {len(self.candidates)}, Removed {removed}")

    def filter_value(self, value: int):
        """Keep addresses equal to value."""
        new_candidates = set()
        for addr in self.candidates:
            if self.current.get(addr) == value:
                new_candidates.add(addr)
        removed = len(self.candidates) - len(new_candidates)
        self.candidates = new_candidates
        print(f"Filter 'value == {value}': Kept {len(self.candidates)}, Removed {removed}")

    def filter_increased(self):
        """Keep addresses that increased."""
        new_candidates = set()
        for addr in self.candidates:
            if self.current.get(addr) > self.baseline.get(addr):
                new_candidates.add(addr)
        removed = len(self.candidates) - len(new_candidates)
        self.candidates = new_candidates
        print(f"Filter 'increased': Kept {len(self.candidates)}, Removed {removed}")

    def report(self):
        print(f"\n--- Candidates ({len(self.candidates)}) ---")
        sorted_addrs = sorted(list(self.candidates))
        # Print top 20
        for addr in sorted_addrs[:20]:
            base = self.baseline.get(addr, "??")
            curr = self.current.get(addr, "??")
            print(f"0x{addr:06X}: {base} -> {curr}")
        if len(sorted_addrs) > 20:
            print(f"... and {len(sorted_addrs) - 20} more.")

def main():
    parser = argparse.ArgumentParser(description="Memory Cartographer")
    subparsers = parser.add_subparsers(dest="command")

    scan_p = subparsers.add_parser("scan")
    scan_p.add_argument("start", type=lambda x: int(x, 0))
    scan_p.add_argument("end", type=lambda x: int(x, 0))
    scan_p.add_argument("--duration", type=float, default=0, help="Duration to monitor (0=snapshot)")

    interactive_p = subparsers.add_parser("interactive")

    args = parser.parse_args()

    cart = MemoryCartographer()

    if args.command == "scan":
        cart.start_scan(args.start, args.end)
        if args.duration > 0:
            time.sleep(args.duration)
            cart.update_scan()
            cart.filter_changed() # Default assumption for a timed scan
            cart.report()
        else:
            print("Baseline taken. Use interactive mode for filtering.")

    elif args.command == "interactive":
        print("Interactive Mode. Commands: start <s, e>, update, filter <mode>, report, quit")
        while True:
            try:
                line = input("> ").strip().split()
                if not line: continue
                cmd = line[0].lower()
                
                if cmd == "quit": break
                elif cmd == "start" and len(line) == 3:
                    cart.start_scan(int(line[1], 0), int(line[2], 0))
                elif cmd == "update":
                    cart.update_scan()
                elif cmd == "filter" and len(line) >= 2:
                    mode = line[1]
                    if mode == "changed": cart.filter_changed()
                    elif mode == "stable": cart.filter_stable()
                    elif mode == "increased": cart.filter_increased()
                    elif mode == "value" and len(line) == 3: cart.filter_value(int(line[2], 0))
                    else: print("Unknown filter")
                elif cmd == "report":
                    cart.report()
                else:
                    print("Unknown command")
            except Exception as e:
                print(f"Error: {e}")

if __name__ == "__main__":
    main()
