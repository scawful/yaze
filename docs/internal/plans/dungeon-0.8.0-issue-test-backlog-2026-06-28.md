# Dungeon 0.8.0 issue/test backlog — 2026-06-28

Source: post-agent audit of the 0.8.0 dungeon drawing/editing slice.

## High-priority cleanup

1. **Pushable block table repointing / expansion**
   - Problem: `SaveAllBlocks` is room-aware, but still bounded by vanilla table capacity.
   - Done when: edited block sets can exceed vanilla capacity through a deliberate repoint/expansion path, with surrounding instruction operands protected.
   - Tests: grow a fixture ROM beyond vanilla capacity; assert pointer/count operands, data bytes, and nearby regions.

2. **Pit-damage membership editor UI**
   - Status: inspector controls implemented for fixed-capacity room replacement; view-model coverage now guards replacement/victim defaults and fixed-capacity swaps. Full ImGui click automation is still a follow-up.
   - Problem: `PitDamageTable` can encode fixed-capacity `RoomsWithPitDamage`, but no panel toggles membership.
   - Done when: the dungeon editor exposes room membership, marks the table dirty, saves via `SaveAllPits(rom, table)`, and reloads the edited membership.
   - Tests: view-model/unit test for toggling; ROM-backed save/reload test; no-op save stays byte-identical.

3. **Independent golden room screenshot ROI**
   - Problem: current fixture checksums guard yaze renderer drift, not correctness against emulator or screenshot truth.
   - Done when: one stable room ROI has a committed PNG/baseline, update procedure, and tolerance/skip rationale.
   - Tests: E2E or headless visual diff for room `0x001` or `0x016`.

## Medium-priority correctness tests

4. **Sparse pixel overlap assertion for object stream ordering**
   - Problem: full-buffer checksums are broad; a targeted overlap pixel would make ordering failures easier to diagnose.
   - Done when: one vanilla or synthetic room asserts the expected top pixel from primary/BG2-overlay/BG1-overlay ordering.

5. **Rare subtype-3 parser parity**
   - Problem: current parser replay checks added size coverage for PrisonCell / BigKeyLock / BombableFloor, but not raw byte/token parity for those routines.
   - Done when: tests compare decoded object metadata against the ROM object table row or fixture bytes, not only `parsed.size()`.

6. **PitDamageTable validation policy**
   - Status: saves now reject duplicate and out-of-range room ids with explicit tests.
   - Problem: fixed-capacity saves reject count mismatch, but do not validate duplicate room ids or out-of-range ids.
   - Done when: either duplicates/out-of-range ids are deliberately allowed and documented, or rejected with explicit tests.

## Low-priority docs/UI polish

7. **Object selector symbology badges**
   - Problem: routine-family labels exist in data/docs, but UI badges are not wired.
   - Done when: selector rows show routine family / special draw behavior without replacing names.

8. **ROM-dependent test classification**
   - Problem: `PitDamageTableTest` lives in the unit suite while requiring a vanilla ROM; this matches some existing tests but can add unit-suite skips.
   - Done when: either split pure capacity/validation tests into unit and move ROM-backed table tests to integration, or document the exception next to the test list.

## Fast verification set

```bash
YAZE_TEST_ROM_VANILLA="$(pwd)/roms/alttp_vanilla.sfc" \
  ctest --test-dir build/presets/mac-ai \
  -R "DungeonRoomRegression|PitDamageTable|RoomObjectRomParity|DungeonRoomRenderParity" \
  --output-on-failure
```
