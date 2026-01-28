#!/usr/bin/env python3
"""
Sentinel - Autonomous Soft Lock Watchdog for Oracle of Secrets

Monitors for soft lock conditions including:
- B007: Y-coordinate overflow during Light World transitions
- B009: Mode 0x00 reset detection (save state compatibility)
- Dungeon transition hangs (Mode 0x06 stuck)
- INIDISP black screen (0x80 stuck during gameplay)
- General transition stagnation
- Link position stagnation
"""
import sys
import os
import time
import argparse
from typing import Dict, Optional, List
from collections import deque

# Add Oracle scripts to path
ORACLE_SCRIPTS = os.path.expanduser("~/src/hobby/oracle-of-secrets/scripts")
sys.path.append(ORACLE_SCRIPTS)

# Add current directory to path to import crash_dump
sys.path.append(os.path.dirname(os.path.abspath(__file__)))

from mesen2_client_lib.client import OracleDebugClient
from mesen2_client_lib.constants import OracleRAM, GameMode, OverworldSubmode
from crash_dump import CrashInvestigator

# Additional RAM addresses not in constants
INIDISP_ADDR = 0x7E001A  # Screen display register
INIDISP_QUEUE = 0x7E0013  # INIDISP queue value


class Sentinel:
    def __init__(self, crash_investigator: CrashInvestigator):
        self.client = OracleDebugClient()
        self.investigator = crash_investigator
        self.last_input_time = time.time()
        self.last_x = -1
        self.last_y = -1
        self.stuck_start_time = 0
        self.last_mode = -1
        self.last_submode = -1
        self.mode_stuck_start = 0

        # B007: Y-overflow tracking
        self.y_overflow_detected = False

        # B009: Reset detection
        self.gameplay_started = False
        self.last_gameplay_mode = -1

        # INIDISP black screen detection (sampling to avoid false positives)
        self.inidisp_samples: deque = deque(maxlen=30)  # 30 samples (15s at 0.5s poll)
        self.black_screen_threshold = 20  # Must be 0x80 for 20+ consecutive samples

        # Configuration
        self.input_stagnation_threshold = 10.0  # Seconds unchanged
        self.transition_stuck_threshold = 5.0   # Seconds in transition
        self.underworld_load_threshold = 2.0    # Shorter for Mode 0x06 (dungeon load)
        self.poll_interval = 0.5

    def ensure_connected(self):
        if not self.client.ensure_connected():
            print("Error: Could not connect to Mesen2.")
            sys.exit(1)

    def check_coordinate_overflow(self, link_y: int) -> Optional[str]:
        """
        B007: Detect Y-coordinate overflow.

        When transitioning from Dark World to Light World via north exit,
        the destination Y calculation can overflow, resulting in Y=65504 (0xFFF0).
        Link becomes stuck and cannot move in any direction.
        """
        # Y > 60000 indicates overflow (normal range is 0-512 for overworld)
        if link_y > 60000:
            if not self.y_overflow_detected:
                self.y_overflow_detected = True
                return f"B007: Y-Coordinate Overflow detected! Y={link_y} (0x{link_y:04X})"
        else:
            self.y_overflow_detected = False
        return None

    def check_game_reset(self, mode: int) -> Optional[str]:
        """
        B009: Detect unexpected game reset to Mode 0x00.

        Cross-instance save state loading causes the game to reset after
        exactly 60 frames (1 second). This detects the transition from
        gameplay mode to title/reset mode.
        """
        is_gameplay = mode in (GameMode.OVERWORLD, GameMode.DUNGEON)

        if is_gameplay:
            self.gameplay_started = True
            self.last_gameplay_mode = mode

        # Detect unexpected reset: was in gameplay, now in title/reset
        if self.gameplay_started and mode == GameMode.TITLE_RESET:
            self.gameplay_started = False
            return f"B009: Unexpected game reset! Was in Mode 0x{self.last_gameplay_mode:02X}, now Mode 0x00"

        return None

    def check_black_screen(self, mode: int) -> Optional[str]:
        """
        Detect INIDISP stuck at blank (0x80) during gameplay modes.

        INIDISP normally cycles through 0x80 during HDMA brightness animation,
        so we sample over time and only trigger if stuck for extended period.
        This avoids false positives during normal transitions.
        """
        try:
            inidisp = self.client.read_address(INIDISP_ADDR)
            is_gameplay = mode in (GameMode.OVERWORLD, GameMode.DUNGEON)

            # Only track during gameplay modes
            if is_gameplay:
                # Sample is True if screen is blank (0x80 = forced blank, 0x00 = off)
                is_blank = inidisp in (0x80, 0x00)
                self.inidisp_samples.append(is_blank)

                # Check for sustained black screen
                if len(self.inidisp_samples) >= self.black_screen_threshold:
                    recent = list(self.inidisp_samples)[-self.black_screen_threshold:]
                    if all(recent):
                        # Clear samples to avoid repeated triggers
                        self.inidisp_samples.clear()
                        return f"Black Screen: INIDISP stuck at 0x{inidisp:02X} for {self.black_screen_threshold} samples during Mode 0x{mode:02X}"
            else:
                # Reset samples when not in gameplay (transitions are expected)
                self.inidisp_samples.clear()

        except Exception as e:
            print(f"Error reading INIDISP: {e}")

        return None

    def check_softlock(self) -> Optional[str]:
        """
        Check for softlock conditions.

        Returns a descriptive string if a soft lock is detected, None otherwise.
        Detection priority:
        1. B007: Y-coordinate overflow
        2. B009: Unexpected game reset
        3. INIDISP black screen
        4. Mode 0x06 (UnderworldLoad) stuck
        5. General transition stuck
        6. Link position stagnation
        """
        try:
            # Read state
            mode = self.client.read_address(OracleRAM.MODE)
            submode = self.client.read_address(OracleRAM.SUBMODE)
            link_x = self.client.read_address16(OracleRAM.LINK_X)
            link_y = self.client.read_address16(OracleRAM.LINK_Y)

            now = time.time()

            # 1. B007: Y-Coordinate Overflow Check
            overflow_result = self.check_coordinate_overflow(link_y)
            if overflow_result:
                return overflow_result

            # 2. B009: Game Reset Detection
            reset_result = self.check_game_reset(mode)
            if reset_result:
                return reset_result

            # 3. INIDISP Black Screen Check
            black_screen_result = self.check_black_screen(mode)
            if black_screen_result:
                return black_screen_result

            # 4. Mode-specific stuck detection
            is_playable_mode = mode in (GameMode.OVERWORLD, GameMode.DUNGEON)
            is_load_mode = mode in (GameMode.DUNGEON_LOAD, GameMode.OVERWORLD_LOAD,
                                     GameMode.OVERWORLD_SPECIAL_LOAD)

            if mode == self.last_mode and submode == self.last_submode:
                stuck_duration = now - self.mode_stuck_start

                # 4a. Mode 0x06 (UnderworldLoad) - shorter threshold
                if mode == GameMode.DUNGEON_LOAD:
                    if stuck_duration > self.underworld_load_threshold:
                        return f"Stuck in UnderworldLoad (Mode 0x06, Sub 0x{submode:02X}) for {self.underworld_load_threshold}s"

                # 4b. Other load modes - shorter threshold
                elif is_load_mode:
                    if stuck_duration > self.underworld_load_threshold:
                        return f"Stuck in Load Mode (Mode 0x{mode:02X}, Sub 0x{submode:02X}) for {self.underworld_load_threshold}s"

                # 4c. Playable mode but not in player control
                elif is_playable_mode and submode != 0:
                    if stuck_duration > self.transition_stuck_threshold:
                        return f"Stuck in Transition (Mode 0x{mode:02X}, Sub 0x{submode:02X}) for {self.transition_stuck_threshold}s"

                # Non-playable modes (Title, Menu, etc) - don't trigger false positives
            else:
                self.last_mode = mode
                self.last_submode = submode
                self.mode_stuck_start = now

            # 5. Link Position Stagnation (only in playable state with player control)
            if is_playable_mode and submode == 0:
                if link_x == self.last_x and link_y == self.last_y:
                    if now - self.stuck_start_time > self.input_stagnation_threshold:
                        # The Sentinel assumes an Agent is driving. If human playing, disable this.
                        return f"Link Stagnation (Pos {link_x},{link_y}) for {self.input_stagnation_threshold}s"
                else:
                    self.last_x = link_x
                    self.last_y = link_y
                    self.stuck_start_time = now
            else:
                # Reset stagnation timer if not in control
                self.stuck_start_time = now

            return None

        except Exception as e:
            print(f"Error reading state: {e}")
            return None

    def run(self):
        print("=" * 50)
        print("Sentinel - Autonomous Soft Lock Watchdog")
        print("=" * 50)
        print("Detection Thresholds:")
        print(f"  - Transition Stuck: {self.transition_stuck_threshold}s")
        print(f"  - UnderworldLoad Stuck: {self.underworld_load_threshold}s")
        print(f"  - Link Stagnation: {self.input_stagnation_threshold}s")
        print(f"  - Black Screen Samples: {self.black_screen_threshold}")
        print()
        print("Active Detectors:")
        print("  - B007: Y-coordinate overflow (Y > 60000)")
        print("  - B009: Unexpected game reset (Mode 0x09/0x07 → 0x00)")
        print("  - INIDISP black screen (0x80 sustained)")
        print("  - Mode 0x06 (UnderworldLoad) hang")
        print("  - Transition stuck (Mode/Submode unchanged)")
        print("  - Position stagnation (Link not moving)")
        print("=" * 50)
        
        try:
            while True:
                # 1. Check Crash (Paused state) via crash_investigator's logic?
                # Actually crash_investigator.monitor() loops. We want to loop ourselves.
                # Check run state
                run_state = self.client.get_run_state()
                paused = run_state.get("paused", False)
                
                if paused:
                    print("\n[Sentinel] Emulator Paused (Crash/Break). Triggering Dump...")
                    self.investigator.dump("sentinel_crash_detected")
                    # Wait for resume
                    while self.client.is_paused():
                        time.sleep(1)
                    continue

                # 2. Check Softlock
                softlock_reason = self.check_softlock()
                if softlock_reason:
                    print(f"\n[Sentinel] Softlock Detected: {softlock_reason}")
                    self.investigator.dump(f"softlock_{softlock_reason.replace(' ', '_')}")
                    # Debounce
                    self.last_x = -1 # Reset logic
                    self.mode_stuck_start = time.time()
                    time.sleep(5) 

                time.sleep(self.poll_interval)

        except KeyboardInterrupt:
            print("\nSentinel stopped.")

def main():
    parser = argparse.ArgumentParser(description="The Sentinel: Autonomous Watchdog")
    parser.add_argument("--z3ed", default=os.path.expanduser("~/src/hobby/yaze/build_ai/bin/Debug/z3ed"), help="Path to z3ed binary")
    parser.add_argument("--rom", default=os.path.expanduser("~/src/hobby/oracle-of-secrets/Roms/oos168x.sfc"), help="Path to ROM")
    
    args = parser.parse_args()
    
    if not os.path.exists(args.z3ed):
        print(f"Error: z3ed not found at {args.z3ed}")
        sys.exit(1)

    # Initialize Investigator for dumping
    investigator = CrashInvestigator(args.z3ed, args.rom)
    
    sentinel = Sentinel(investigator)
    sentinel.ensure_connected()
    sentinel.run()

if __name__ == "__main__":
    main()
