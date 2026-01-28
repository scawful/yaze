#!/usr/bin/env python3
import sys
import os
import json
import time
import hashlib
import argparse
from typing import Dict, List, Optional

# Add Oracle scripts to path
ORACLE_SCRIPTS = os.path.expanduser("~/src/hobby/oracle-of-secrets/scripts")
sys.path.append(ORACLE_SCRIPTS)

from mesen2_client_lib.client import OracleDebugClient

class DiffRunner:
    def __init__(self):
        self.client = OracleDebugClient()
        self.trace = []

    def ensure_connected(self):
        if not self.client.ensure_connected():
            print("Error: Could not connect to Mesen2.")
            sys.exit(1)

    def get_state_hash(self) -> str:
        """Compute a hash of the current critical game state."""
        state = self.client.get_oracle_state()
        # Canonicalize by sorting keys
        state_str = json.dumps(state, sort_keys=True)
        return hashlib.md5(state_str.encode()).hexdigest()

    def load_scenario(self, path: str) -> List[Dict]:
        with open(path, 'r') as f:
            return json.load(f)

    def run_record(self, scenario_path: str, output_path: str):
        """Run a scenario and record the state trace."""
        self.ensure_connected()
        print(f"Recording scenario: {scenario_path}")
        
        scenario = self.load_scenario(scenario_path)
        # Reset emulator to known state?
        # Ideally scenario includes "load save state X" instruction.
        # For now, assume user has setup start state.
        
        self.trace = []
        
        for step in scenario:
            # step: {"input": "A,Up", "frames": 10}
            inputs = step.get("input", "")
            frames = step.get("frames", 1)
            
            # We need frame-by-frame precision.
            # Client doesn't expose 'hold this input for 1 frame AND advance'.
            # We will use lower level bridge commands if possible, or just loop.
            
            # Hack: Use press_button logic manually?
            # Actually, press_button is blocking.
            # We want: SetInput -> Frame -> Snapshot.
            
            for _ in range(frames):
                # 1. Set Input
                # Note: Mesen2-OoS socket API might not expose raw "Set Input State".
                # It usually exposes "Press Button".
                # If we rely on "Press Button" logic, we might desync if it uses wall-clock time?
                # Usually it uses frame counts.
                # Let's assume press_button works for 1 frame.
                
                # Wait, press_button(frames=1) does: Input On -> Frame -> Input Off.
                # If we want to HOLD for 10 frames, we need:
                # Input On -> Frame -> Frame ... -> Input Off.
                
                # If the scenario is just a list of actions, we can just execute them and log the result state *after* the action.
                # This is "Action-Level Diffing", not "Frame-Level".
                # This is easier and robust enough for logic bugs.
                
                if inputs:
                    self.client.press_button(inputs, frames=1)
                else:
                    self.client.run_frames(1)
                
                # Snapshot state
                state_hash = self.get_state_hash()
                self.trace.append({
                    "input": inputs,
                    "state_hash": state_hash,
                    "state": self.client.get_oracle_state() # Store full state for debug
                })
                
        # Save trace
        with open(output_path, 'w') as f:
            json.dump(self.trace, f, indent=2)
        print(f"Trace saved to {output_path}")

    def run_verify(self, trace_path: str):
        """Replay a trace and verify state matches."""
        self.ensure_connected()
        print(f"Verifying trace: {trace_path}")
        
        with open(trace_path, 'r') as f:
            expected_trace = json.load(f)
            
        for i, entry in enumerate(expected_trace):
            inputs = entry.get("input", "")
            expected_hash = entry.get("state_hash")
            
            # Execute
            if inputs:
                self.client.press_button(inputs, frames=1)
            else:
                self.client.run_frames(1)
                
            # Verify
            current_hash = self.get_state_hash()
            if current_hash != expected_hash:
                print(f"\n❌ Divergence at Step {i}!")
                print(f"Expected Hash: {expected_hash}")
                print(f"Actual Hash:   {current_hash}")
                
                current_state = self.client.get_oracle_state()
                expected_state = entry.get("state")
                
                # Diff states
                print("\nState Diff:")
                for k, v in current_state.items():
                    exp = expected_state.get(k)
                    if v != exp:
                        print(f"  {k}: {exp} -> {v}")
                        
                sys.exit(1)
                
            if i % 60 == 0:
                print(f"Verified {i}/{len(expected_trace)} steps...", end="\r")
                
        print(f"\n✅ Verification passed. ({len(expected_trace)} steps)")

def main():
    parser = argparse.ArgumentParser(description="Diff Runner: Regression Testing")
    subparsers = parser.add_subparsers(dest="command")
    
    rec_p = subparsers.add_parser("record")
    rec_p.add_argument("scenario", help="JSON scenario file input")
    rec_p.add_argument("output", help="JSON trace file output")
    
    ver_p = subparsers.add_parser("verify")
    ver_p.add_argument("trace", help="JSON trace file (source of truth)")
    
    args = parser.parse_args()
    
    runner = DiffRunner()
    
    if args.command == "record":
        runner.run_record(args.scenario, args.output)
    elif args.command == "verify":
        runner.run_verify(args.trace)
    else:
        parser.print_help()

if __name__ == "__main__":
    main()
