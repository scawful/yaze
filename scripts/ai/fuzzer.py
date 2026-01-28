#!/usr/bin/env python3
import sys
import os
import time
import random
import argparse
from typing import Dict, Optional, List

# Add Oracle scripts to path
ORACLE_SCRIPTS = os.path.expanduser("~/src/hobby/oracle-of-secrets/scripts")
sys.path.append(ORACLE_SCRIPTS)

# Add current directory to path
sys.path.append(os.path.dirname(os.path.abspath(__file__)))

from mesen2_client_lib.client import OracleDebugClient
from crash_dump import CrashInvestigator

class ChaosMonkey:
    def __init__(self, crash_investigator: CrashInvestigator):
        self.client = OracleDebugClient()
        self.investigator = crash_investigator
        
        self.buttons = ["A", "B", "X", "Y", "L", "R", "Start", "Select", "Up", "Down", "Left", "Right"]
        self.gameplay_buttons = ["A", "B", "X", "Y", "L", "R"] # Biased towards action
        self.directions = ["Up", "Down", "Left", "Right"]
        self.running = False

    def ensure_connected(self):
        if not self.client.ensure_connected():
            print("Error: Could not connect to Mesen2.")
            sys.exit(1)

    def check_health(self) -> bool:
        """Check if game crashed or softlocked."""
        run_state = self.client.get_run_state()
        if run_state.get("paused", False):
            print("\n[ChaosMonkey] Emulator Paused! Possible crash detected.")
            self.investigator.dump("fuzzer_crash")
            return False
        return True

    def mode_gameplay(self, duration: float):
        """Standard gameplay fuzzing: Move around, press buttons."""
        print(f"Fuzzing Mode: Gameplay ({duration}s)")
        start_time = time.time()
        
        while time.time() - start_time < duration:
            if not self.check_health(): return

            # 1. Random Direction (hold for a bit)
            if random.random() < 0.7:
                d = random.choice(self.directions)
                frames = random.randint(5, 60)
                # We use lower-level bridge send because client helper might be too blocking?
                # Actually client.press_button blocks for 'frames'. Ideally we want non-blocking async input.
                # Mesen2 socket API inputs are "hold for N frames" usually.
                # Let's just use the client helper for now.
                self.client.press_button(d, frames=frames)
            
            # 2. Random Actions
            if random.random() < 0.5:
                # Press 1-3 buttons simultaneously
                count = random.randint(1, 3)
                btns = random.sample(self.gameplay_buttons, count)
                self.client.press_button(",".join(btns), frames=random.randint(1, 10))

            # 3. Occasional Pause
            if random.random() < 0.05:
                self.client.press_button("Start", frames=5)
                time.sleep(0.5) # Wait for menu
                self.client.press_button("Start", frames=5) # Unpause

    def mode_glitch_hunter(self, duration: float):
        """High-frequency, frame-perfect stress test."""
        print(f"Fuzzing Mode: Glitch Hunter ({duration}s)")
        start_time = time.time()
        
        while time.time() - start_time < duration:
            if not self.check_health(): return

            # Rapid-fire inputs
            btns = random.sample(self.buttons, random.randint(2, 5))
            self.client.press_button(",".join(btns), frames=1) # 1 frame only
            
            # Rapid Start/Select (Frame Perfect Pause buffering?)
            if random.random() < 0.2:
                self.client.press_button("Start", frames=1)
                self.client.press_button("Select", frames=1)

    def run(self, mode: str, duration: float):
        self.ensure_connected()
        
        try:
            if mode == "gameplay":
                self.mode_gameplay(duration)
            elif mode == "glitch":
                self.mode_glitch_hunter(duration)
            else:
                print(f"Unknown mode: {mode}")
                return
            
            print("\nFuzzing complete. No crashes detected.")
            
        except KeyboardInterrupt:
            print("\nFuzzer stopped.")

def main():
    parser = argparse.ArgumentParser(description="Chaos Monkey: Automated Fuzzer")
    parser.add_argument("--z3ed", default=os.path.expanduser("~/src/hobby/yaze/build_ai/bin/Debug/z3ed"), help="Path to z3ed binary")
    parser.add_argument("--rom", default=os.path.expanduser("~/src/hobby/oracle-of-secrets/Roms/oos168x.sfc"), help="Path to ROM")
    parser.add_argument("--mode", default="gameplay", choices=["gameplay", "glitch"], help="Fuzzing strategy")
    parser.add_argument("--duration", type=float, default=60.0, help="Duration in seconds")
    
    args = parser.parse_args()
    
    if not os.path.exists(args.z3ed):
        print(f"Error: z3ed not found at {args.z3ed}")
        sys.exit(1)

    # Initialize Investigator for dumping
    investigator = CrashInvestigator(args.z3ed, args.rom)
    
    monkey = ChaosMonkey(investigator)
    monkey.run(args.mode, args.duration)

if __name__ == "__main__":
    main()
