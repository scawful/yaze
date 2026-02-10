# ROM Safety Guardrails (Yaze)

This repo is used to edit ROM hacks (including Oracle of Secrets). Treat ROM writes as high-risk: prefer failing closed over “best effort”.

## Workflow Guardrails

- Always work on a copy of the ROM when doing manual editor sessions:
  - `./scripts/rom_safety_preflight.sh path/to/rom.sfc`
  - This prints `sha256`, optionally runs `z3ed rom-doctor`, and writes a timestamped copy to `/tmp` by default.
- Keep backups enabled:
  - Project setting: `workspace_settings.backup_on_save=true`
  - Backups are managed via `RomFileManager` retention/daily keep.

## Save-Time Write Fences (C++)

Yaze supports a strict allow-list for save-time writes:

- API: `src/rom/write_fence.h`
  - `yaze::rom::WriteFence` declares allowed PC ranges `[start, end)`.
  - `yaze::rom::ScopedWriteFence` pushes the fence onto the ROM.
  - Fences are **stacked**; writes must be allowed by **all** active fences (logical AND).

Example:

```cpp
yaze::rom::WriteFence fence;
RETURN_IF_ERROR(fence.Allow(start_pc, end_pc, "MyFeature"));
yaze::rom::ScopedWriteFence scope(rom, &fence);

RETURN_IF_ERROR(rom->WriteVector(start_pc, bytes));
```

### Current Oracle-Focused Coverage

- Custom collision writers are fenced to:
  - pointer table: `kCustomCollisionRoomPointers .. kCustomCollisionRoomPointers + kNumberOfRooms*3`
  - data bank: `kCustomCollisionDataPosition .. kCustomCollisionDataSoftEnd`
- Water fill table writers are fenced to:
  - `kWaterFillTableStart .. kWaterFillTableEnd`

## When Adding a New ROM-Writing Feature

Keep this checklist short and non-negotiable:

1. Define a reserved region (or reuse an existing one) and document constants.
2. Fence the writer(s) to only that region (and any required pointer tables).
3. Add a fast unit test that proves:
   - writes outside the allowed region are blocked (write fence)
   - missing/truncated regions return `FailedPrecondition` (fail closed)
4. Add/extend a region-preservation integration test when applicable:
   - `test/integration/zelda3/dungeon_save_region_test.cc`

## Fast Tests

- Quick suite: `./scripts/test_fast.sh --quick`
- Targeted runs:
  - `./scripts/test_fast.sh --quick --filter WriteFenceTest`
  - `./scripts/test_fast.sh --quick --filter SaveAllCollisionRomPresenceTest`

