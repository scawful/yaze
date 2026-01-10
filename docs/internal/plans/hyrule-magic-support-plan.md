# Implementation Plan - Hyrule Magic & Parallel Worlds Support

## Goal Description
Add support for "doctoring" legacy Hyrule Magic (HM) ROMs and loading Parallel Worlds (PW) ROMs which use custom dungeon pointer tables.

## User Review Required
- **Plan:** Review the proposed detection and compatibility logic.

## Proposed Changes

### 1. Hyrule Magic Doctor (`z3ed rom-doctor`)
- **Detection:**
  - Detect HM header signatures (if any).
  - Detect "Bank 00 Erasure" (already implemented in Phase 1, confirmed working on `GoT-v040.smc`).
  - Detect Parallel Worlds by internal name or specific byte sequences.
- **Fixes:**
  - **Checksum Fix:** Auto-calculate and update SNES checksum.
  - **Resize:** suggest resizing to 2MB/4MB if non-standard (e.g., 1.5MB PW ROMs).
  - **Bank 00 Restoration:** (Optional) If Bank 00 is erased, offer to restore from a vanilla ROM.

### 2. Parallel Worlds Compatibility Mode
- **Problem:** PW uses a custom version of HM that moved dungeon pointer tables to support larger rooms/more data.
- **Solution:**
  - Add `Rom::IsParallelWorlds()` check.
  - Implement `ParallelWorldsDungeonLoader` that uses the modified pointer tables.
  - **Offsets (To Be Verified):**
    - Vanilla Room Pointers: `0x21633` (PC)
    - PW Room Pointers: Need to locate these. Likely in expanded space.

### 3. Implementation Steps
#### [NEW] [hm_support.cc](file://$TRUNK_ROOT/scawful/retro/yaze/src/rom/hm_support.cc)
- Implement detection and fix logic.

#### [MODIFY] [rom_doctor_commands.cc](file://$TRUNK_ROOT/scawful/retro/yaze/src/cli/handlers/tools/rom_doctor_commands.cc)
- Integrate HM/PW checks.

#### [MODIFY] [dungeon_loader.cc](file://$TRUNK_ROOT/scawful/retro/yaze/src/zelda3/dungeon/dungeon_loader.cc)
- Add branching logic for PW ROMs to use alternate pointer tables.

## Verification Plan
- **Doctor:** Run `z3ed rom-doctor` on `GoT-v040.smc` and `PW-V1349.SMC`.
- **Load:** Attempt to load PW dungeon rooms in `yaze` (once loader is updated).
