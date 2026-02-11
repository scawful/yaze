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

## Shared Oracle Preflight Gate

Oracle save/import writes now share a single preflight validator:

- API: `src/zelda3/dungeon/oracle_rom_safety_preflight.h`
- Implementation: `src/zelda3/dungeon/oracle_rom_safety_preflight.cc`
- Current call sites:
  - `EditorManager::CheckOracleRomSafetyPreSave` (GUI save)
  - `dungeon-import-custom-collision-json`
  - `dungeon-import-water-fill-json`

Fail-closed checks include:

- Missing reserved regions (`ORACLE_WATER_FILL_REGION_MISSING`)
- Corrupted water-fill header (`ORACLE_WATER_FILL_HEADER_CORRUPT`)
- Invalid/overlapping collision pointers (`ORACLE_COLLISION_POINTER_INVALID`)
- Invalid water-fill table state (`ORACLE_WATER_FILL_TABLE_INVALID`)

For CLI imports, preflight details are included in `--report` output under
`preflight` (`ok` + structured `errors[]`).

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

## CLI Guardrails For Collision/Water JSON

When importing dungeon collision or water-fill JSON, use a two-step flow:

1. Preview only (no ROM writes):
   - `z3ed dungeon-import-custom-collision-json --in data.json --dry-run --report import.report.json`
   - `z3ed dungeon-import-water-fill-json --in water.json --dry-run --report water.report.json`
2. Apply after review:
   - `z3ed dungeon-import-custom-collision-json --in data.json`
   - `z3ed dungeon-import-water-fill-json --in water.json`

Safety semantics:

- `--dry-run` performs full parsing/validation and emits impact counts without writing.
- `--report <path>` writes machine-readable JSON with `status`, `mode`, and structured error code/message.
- `dungeon-import-custom-collision-json --replace-all` is destructive and requires `--force` in write mode.
- `dungeon-import-water-fill-json --strict-masks` fails closed if SRAM mask normalization would be needed.

Agent instruction (mandatory):

- For automated workflows, always run a `--dry-run --report` preflight before any write-mode import.
- If report `status=error`, stop and fix inputs; do not retry in write mode automatically.
