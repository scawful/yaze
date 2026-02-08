#!/usr/bin/env python3
import sys
import os
import time
import argparse
import collections
from typing import Dict, List, Optional

# Add Oracle scripts to path
ORACLE_SCRIPTS = os.path.expanduser("~/src/hobby/oracle-of-secrets/scripts")
sys.path.append(ORACLE_SCRIPTS)

# Add current directory to path
sys.path.append(os.path.dirname(os.path.abspath(__file__)))

from mesen2_client_lib.client import OracleDebugClient
from crash_dump import CrashInvestigator

class Profiler:
    def __init__(self, crash_investigator: CrashInvestigator):
        self.client = OracleDebugClient()
        self.investigator = crash_investigator
        self.pc_histogram = collections.Counter()
        self.symbol_cache = {}

    def ensure_connected(self):
        if not self.client.ensure_connected():
            print("Error: Could not connect to Mesen2.")
            sys.exit(1)

    def sample(self, duration: float):
        """Sample PC execution history for a duration."""
        print(f"Profiling for {duration} seconds...")
        start_time = time.time()
        frames_sampled = 0
        
        while time.time() - start_time < duration:
            # Capture a small trace (e.g., 500 instructions) frequently
            # This gives us a statistical sample of where the CPU is.
            trace = self.investigator.capture_trace(count=500)
            if trace:
                for entry in trace:
                    pc = entry.get("pc")
                    if pc is not None:
                        self.pc_histogram[pc] += 1
                frames_sampled += 1
            
            # Sleep briefly to avoid flooding socket, but fast enough to get new samples
            time.sleep(0.05) 

        print(f"Captured {sum(self.pc_histogram.values())} samples over {frames_sampled} snapshots.")

    def resolve_symbols(self):
        """Resolve all PCs in the histogram to symbols."""
        print("Resolving symbols for hotspots...")
        # Sort by frequency to prioritize hotspots
        sorted_pcs = [pc for pc, count in self.pc_histogram.most_common()]
        
        for i, pc in enumerate(sorted_pcs):
            # Resolve if count is significant (ignore 1-off executions to save time?)
            # Let's resolve all for accuracy, but maybe limit distinct addresses.
            if i % 10 == 0:
                print(f"  Resolving {i}/{len(sorted_pcs)}...", end="\r")
            
            info = self.investigator.resolve_symbol(pc)
            if info.get("status") == "success":
                self.symbol_cache[pc] = info

        print("\nResolution complete.")

    def report(self, limit: int = 20):
        """Generate a hotspot report."""
        print(f"\n=== Profiler Report (Top {limit}) ===")
        print(f"{ 'Count':<8} | {'Address':<8} | {'Symbol':<30} | {'Source'}")
        print("-" * 80)
        
        # Aggregate by symbol to group instructions in the same function
        symbol_counts = collections.Counter()
        
        for pc, count in self.pc_histogram.items():
            info = self.symbol_cache.get(pc)
            if info:
                name = info.get("name", f"unk_{pc:X}")
                # Optional: Group by "Nearest" symbol to catch loops inside functions
                # If match_type is 'nearest', the name is the function start.
                symbol_counts[name] += count
            else:
                symbol_counts[f"unk_{pc:X}"] += count

        for name, count in symbol_counts.most_common(limit):
            # Find a representative address for this symbol to get source info
            # (Just grab the first one we see)
            # This is a bit hacky but works for display.
            representative_src = ""
            for pc, info in self.symbol_cache.items():
                if info.get("name") == name:
                    file = info.get("file", "")
                    line = info.get("line", "")
                    if file:
                        representative_src = f"{os.path.basename(file)}:{line}"
                    break
            
            print(f"{count:<8} | {'--':<8} | {name:<30} | {representative_src}")

def main():
    parser = argparse.ArgumentParser(description="Lag Detector / Profiler")
    default_z3ed = (
        os.environ.get("Z3ED_BIN")
        or os.environ.get("Z3ED_PATH")
        or os.path.abspath(os.path.join(os.path.dirname(__file__), "..", "z3ed"))
    )
    parser.add_argument("--z3ed", default=default_z3ed, help="Path to z3ed binary")
    parser.add_argument(
        "--rom",
        default=os.environ.get(
            "OOS_ROM",
            os.path.expanduser("~/src/hobby/oracle-of-secrets/Roms/oos168x.sfc"),
        ),
        help="Path to ROM",
    )
    parser.add_argument("--duration", type=float, default=5.0, help="Profiling duration in seconds")
    
    args = parser.parse_args()
    
    if not os.path.exists(args.z3ed):
        print(f"Error: z3ed not found at {args.z3ed}")
        sys.exit(1)

    investigator = CrashInvestigator(args.z3ed, args.rom)
    profiler = Profiler(investigator)
    
    profiler.ensure_connected()
    profiler.sample(args.duration)
    profiler.resolve_symbols()
    profiler.report()

if __name__ == "__main__":
    main()
