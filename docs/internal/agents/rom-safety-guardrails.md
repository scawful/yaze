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

## Using Oracle Validation Panel in yaze

The **Oracle Validation** panel (`oracle.validation`, category: Oracle) provides a
graphical equivalent of the CLI smoke and preflight checks.

**How to open:** Panels menu → Oracle → Oracle Validation
**Panel ID:** `oracle.validation`

Controls:
- **ROM Path** — prefilled from the currently loaded ROM; editable for a different file
- **Min D6 track rooms** — integer gate; default 4 (catches D6 room regressions)
- **Strict readiness** — when on, the Structural Smoke run also checks D4/D3 collision readiness
- **Write report** — toggle + path to write a JSON report alongside the run

Buttons and what they run in-process:
| Button | Command | Blocking? |
|--------|---------|----------|
| Run Structural Smoke | `oracle-smoke-check` | yes (exits on structural error) |
| Run Strict Readiness | `oracle-smoke-check --strict-readiness` | yes |
| Run Oracle Preflight | `dungeon-oracle-preflight` | yes |

Results appear as collapsible per-subsystem cards. The **Copy CLI** action copies the
exact `z3ed` command for terminal reproduction. Results are preserved across frames until
the next run. `ran` / `skipped` readiness-check states are shown explicitly — never
ambiguous.

Implementation: `src/app/editor/oracle/panels/oracle_validation_panel.h` (panel) +
`src/app/editor/oracle/panels/oracle_validation_view_model.h` (pure C++ parser/builder).

## oracle-smoke-check — Repeatable Regression Smoke Command

Single z3ed command that runs all three Oracle subsystem checks in one pass.

```bash
# Default mode: structural checks only (fast, safe to run in CI)
z3ed oracle-smoke-check --rom path/to/oos168.sfc --format=json

# Strict-readiness mode: also fail when D4/D3 rooms lack authored collision
z3ed oracle-smoke-check --rom path/to/oos168.sfc --strict-readiness --format=json

# D6 regression gate: require all 4 rooms to have track rail objects
z3ed oracle-smoke-check --rom path/to/oos168.sfc --min-d6-track-rooms=4 --format=json

# With JSON report file
z3ed oracle-smoke-check --rom path/to/oos168.sfc --report /tmp/smoke.json
```

Output contract:

| Field | Always present | Meaning |
|-------|---------------|---------|
| `ok` | yes | true iff exit=0 |
| `status` | yes | `"pass"` or `"fail"` |
| `strict_readiness` | yes | flag state |
| `checks.d4_zora_temple.structural_ok` | yes | water-fill region/table valid |
| `checks.d4_zora_temple.required_rooms_check` | yes | `"ran"` or `"skipped"` |
| `checks.d4_zora_temple.required_rooms_ok` | only when `"ran"` | rooms 0x25/0x27 have collision |
| `checks.d6_goron_mines.ok` | yes | minecart audit ran on 0xA8/0xB8/0xD8/0xDA |
| `checks.d6_goron_mines.track_rooms_found` | yes | D6 rooms with non-empty track_object_subtypes |
| `checks.d6_goron_mines.min_track_rooms` | yes | threshold from `--min-d6-track-rooms` (0 = off) |
| `checks.d6_goron_mines.meets_min_track_rooms` | yes | `track_rooms_found >= min_track_rooms` or min=0 |
| `checks.d3_kalyxo_castle.readiness_check` | yes | `"ran"` or `"skipped"` |
| `checks.d3_kalyxo_castle.ok` | only when `"ran"` | room 0x32 has collision |

Readiness checks (`required_rooms_ok`, `d3.ok`) are only emitted when the ROM
has `HasCustomCollisionWriteSupport` (i.e., expanded ROMs ≥ `kCustomCollisionDataEnd`).
On non-expanded ROMs the `*_check` field says `"skipped"` and the boolean is absent —
never misleadingly `true`.

Exit codes:
- `0` — all enabled checks passed (or ROM not found: skip is exit 0)
- `1` — structural failure, or (in `--strict-readiness`) readiness gap on an expanded ROM

**Shell wrapper** (`scripts/oracle_smoke.sh`) differs from the C++ command in two ways:
1. It does not support `--strict-readiness`; D4/D3 readiness gaps are always informational.
2. It defaults to `scripts/z3ed` rather than the system PATH z3ed.
Use the C++ command (`z3ed oracle-smoke-check`) for consistent semantics in CI.

## CI Gate (`.github/workflows/ci.yml` — `oracle-smoke-check` job)

The `oracle-smoke-check` CI job runs on both **pull requests** and **push** to
`master`/`develop`. When `ORACLE_ROM_URL` is not configured (fork PRs, unconfigured
repos), the ROM-dependent steps are individually skipped and the job **completes
successfully** — it does not fail on missing ROM. To enable the full smoke:

1. Add a repository secret `ORACLE_ROM_URL` pointing to a downloadable `oos168.sfc` URL
   (e.g., a GitHub Release asset from the `oracle-of-secrets` repo).
2. The job downloads the ROM, configures CMake (`cmake --preset ci-macos`), builds z3ed,
   and runs two checks:

| Check | Mode | Blocks merge? | Artifact |
|-------|------|--------------|---------|
| Structural | `--min-d6-track-rooms=4` | **Yes** (when ROM configured) | `oracle_smoke_report.json` |
| Strict-readiness | `--strict-readiness --min-d6-track-rooms=4` | No (`continue-on-error`) | `oracle_smoke_strict_report.json` |

Both reports are uploaded as the `oracle-smoke-reports-<run_number>` artifact (retained
14 days) via `if: always()`, including when the structural check fails.

**To make the structural check a required PR gate**: add `oracle-smoke-check` to the
branch protection rule's required status checks in GitHub repository settings.
Without that setting the check is advisory.

**Structural check fails when**:
- The Oracle ROM's water-fill region or table is structurally invalid
- Fewer than 4 D6 rooms have track rail objects

**Informational only**: D4/D3 readiness gaps (rooms lack authored collision) surface in the
strict-readiness report but do not block merge until authoring is complete.

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

## Oracle Workflow Test Guide (D4/D6/D3)

Use this as the default validation path when working on Oracle dungeon systems
in yaze.

### 1) Use the correct `z3ed` binary

Prefer the repo wrapper so you don't accidentally call an older global binary:

- `./scripts/z3ed --doctor`
- `./scripts/z3ed --which`

For scripts, either rely on wrapper defaults or pin explicitly:

- `Z3ED=./scripts/z3ed bash scripts/oracle_smoke.sh`
- `Z3ED_BIN=./build_ai/bin/Debug/z3ed ./scripts/z3ed dungeon-oracle-preflight --help`

### 2) Run one-command smoke first

- `bash scripts/oracle_smoke.sh`

Current smoke semantics:

- Exit `0`: structural checks pass (ROM is safe for workflow development)
- Exit `1`: structural preflight failure or missing required `z3ed` subcommand
- D4 required-room and D3 required-room readiness are informational fields in
  JSON output (`required_rooms_ok` / `ok`), not exit-code failures

### 3) Run focused subsystem checks

D4 Zora Temple (water gate/dam/drain):

- Structural: `./scripts/z3ed dungeon-oracle-preflight --rom roms/oos168.sfc --format=json`
- Required rooms: `./scripts/z3ed dungeon-oracle-preflight --rom roms/oos168.sfc --required-collision-rooms=0x25,0x27 --format=json`

D6 Goron Mines (minecart):

- `./scripts/z3ed dungeon-minecart-audit --rom roms/oos168.sfc --rooms=0xA8,0xB8,0xD8,0xDA --include-track-objects --format=json`

D3 Kalyxo prison readiness:

- `./scripts/z3ed dungeon-oracle-preflight --rom roms/oos168.sfc --required-collision-rooms=0x32 --format=json`

### 4) Regression tests before/after code changes

Fast Oracle-focused integration run:

- `./build_ai/bin/Debug/yaze_test_integration --gtest_filter='MinecartAuditIntegrationTest.*:OracleWorkflowTest.*'`

Core unit+integration Oracle command checks:

- `./build_ai/bin/Debug/yaze_test_unit --gtest_filter='DungeonMinecartAuditTest.*:DungeonCollisionJsonCommandsTest.*:OracleRomSafetyPreflightTest.*:DungeonOraclePreflightTest.*'`

## Mesen2-OOS In-Game Test Planning

Use this plan after CLI checks are green, to validate gameplay behavior that
static preflight cannot prove.

### Session setup

1. Load `oos168.sfc` in Mesen2-OOS.
2. Ensure symbol file is available if needed (`--export_symbols ... --symbol_format mesen` from yaze).
3. Use named save states per scenario (for example `d4_gate_before`, `d6_room_a8`).
4. Record outcomes in universe coordination (`scripts/agents/coord task-heartbeat ...`) with exact room IDs and pass/fail.

### Test matrix

D4 Zora Temple water system:

- Rooms: `0x25`, `0x27`
- Goal: water gate/dam transitions are persistent and reversible
- Pass criteria:
  - Trigger action changes expected water state
  - Re-enter room and state persists
  - Save+reload preserves state

D6 Goron Mines minecart:

- Rooms: `0xA8`, `0xB8`, `0xD8`, `0xDA`
- Goal: carts, stop tiles, and rails are behaviorally aligned
- Pass criteria:
  - Cart can be placed/starts on valid stop tile where intended
  - Track traversal does not softlock
  - Room transitions preserve expected track/cart state

D3 Kalyxo prison:

- Room: `0x32`
- Goal: required custom collision data supports prison sequence interactions
- Pass criteria:
  - Interaction tied to custom collision geometry triggers reliably
  - No collision holes/softlocks in critical path

### Suggested test cadence per feature PR

1. CLI smoke (`oracle_smoke.sh`)
2. Focused command checks (D4/D6/D3 relevant to your change)
3. Targeted integration tests (`OracleWorkflowTest`/`MinecartAuditIntegrationTest`)
4. Mesen2 in-game verification for touched rooms
5. Universe task update with exact commands and result counts (generate markdown snapshot only if a human-readable handoff is needed)
