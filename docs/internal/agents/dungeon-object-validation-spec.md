# Dungeon Object Draw-Routine Validation Framework (ALTTP ROM)

**Status:** Draft
**Owner:** TBD (High model)
**Created:** 2026-02-03
**Last Reviewed:** 2026-02-04
**Next Review:** 2026-02-17
**Validation/Exit Criteria:** See "Acceptance Criteria" and "UX Parity Exit Criteria".
**Coordination Board:** `docs/internal/agents/coordination-board.md` (2026-02-03 ai-infra-architect entry)

## Decision Summary
- Start with vanilla ALTTP ROM only.
- Validation oracle for now is ROM-static interpreter + yaze trace capture.
- ROM micro-emu is planned but deferred until emulator infra stabilizes.
- No full emulator fallback for now.
- Custom Oracle-of-Secrets objects remain out of scope for validation, but UI toggles/overlays are in scope.
- UI/UX parity audit vs ZScreamDungeon is required, with improvements beyond parity where feasible.

## Goals
- Automatically validate dungeon object draw routines and hit-test bounds to catch issues (e.g., large decor) without manual inspection.
- Use vanilla ALTTP ROM as the source of truth.
- Exclude custom Oracle-of-Secrets objects injected via ASM (no validation required for those).
- Match or exceed ZScreamDungeon UX for selection, drag modifiers, and context menus.
- Fix panel/window management so positions persist and remain visible.
- Make custom object rendering predictable (toggleable overlays, consistent selection rules).

## Scope
**In scope**
- Vanilla ALTTP dungeon objects drawn by standard routines.
- Validation of draw output and selection bounds.
- UX/selection parity audit vs ZScreamDungeon.
- Context menu parity and improvements for dungeon rooms.
- Panel management (persisted positions, safe window placement).
- Custom object rendering toggles and overlays (minecart origins/tracks), without validation.

**Out of scope (for now)**
- Custom objects (0x31/0x32 and 0x100–0x103 minecart variants).
- Full emulator-based oracle (defer until micro-emu coverage is insufficient).

## Known Pain Points (User Reports)
- Large decor objects do not draw correctly.
- Selection boxes and drag modifiers are inconsistent vs ZScreamDungeon.
- Context menu for dungeon rooms is missing actions / parity.
- Panel management is poor: windows pop out of view and do not persist positions.
- Custom elements need a toolbar toggle; minecart origin/track setup needs reliable overlays.

## Goron Mines / Minecart System Insights (Claude, 2026-02-05)
**Context:** These inform editor overlays and validation helpers (not draw validation for custom objects).
**Design plan:** `oracle-of-secrets/Docs/Plans/goron_mines_minecart_design.md` (track slot assignments, phased rollout, camera origin strategy).

**Data gaps & design risks**
- **4-of-32 track data populated:** Only track subtypes 0–3 have real starting data; subtypes 4–31 are filler (room `0x89`, X `0x1300`, Y `0x1100`). This makes most rooms with track tiles non-functional without manual cart placement.
- **Camera origin gap:** The cart movement uses indoor camera logic; at cart speed (`0x20` subpixels/frame), camera can lag or snap at quadrant boundaries. A pre-scroll origin hint is needed for smooth cart rides in layout-7 rooms.
- **Dead mechanics:** `RoomTag_ShutterDoorRequiresCart` hook is present but commented out, and the `$36` speed switch infra exists but is unused.

**Room audit summary**
- Track-heavy rooms without cart access: `0x77`, `0x79`, `0x89`, `0x97`, `0xA8`, `0xB8`, `0xB9`, `0xD7`, `0xD8`, `0xD9`, `0xDA` (see `oracle-of-secrets/Docs/World/Dungeons/GoronMines_Map.md`).
- Rooms with functional minecarts (tracks 0–3): `0x98`, `0x88`, `0x87`.

**Tooling opportunities (z3ed/yaze)**
- Enumerate all track objects (ID `0x31`) per room to align visuals with collision tiles.
- Render collision layer (`$7F2000+`) and highlight track tiles `0xB0–0xBE` + `0xD0–0xD3`.
- Validate that every room with track collision tiles has a minecart sprite on a stop tile (`0xB7–0xBA`).

## Progress Updates
**2026-02-04**
- Panel/window management: added viewport clamping for floating panels so at least 32px stays visible (PanelWindow).
- Selection UX: added rectangle size threshold to avoid accidental marquee selection; Alt now clears selection; Shift locks drag axis for object dragging.
- Custom overlays: added minecart track origin overlay + toolbar/context-menu toggle (uses ASM data from track editor panel).
- Room matrix context menu: added open/focus, swap-with-current, and copy room id/name actions.
- Large decor fix: corrected Single4x4 draw routine to render 4x4 tile8 (matches ZScream object_FEB) instead of 8x8 repeat.
- Tests: added unit tests for window clamping, rectangle size threshold, and Single4x4 trace order.

**2026-02-05**
- Reviewed Goron Mines minecart design plan and aligned editor tooling priorities.
- Added camera quadrant boundary overlay (toolbar + context menu toggle) to help plan cart routes in layout-7 rooms.
- Added minecart sprite alignment overlay to highlight carts not placed on stop tiles (uses configured sprite IDs + collision tiles).

## Glossary
- **TileTrace:** A captured list of tile writes from a draw routine (tile ID + tile coordinates). It is internal instrumentation only, not a user-facing tool.
- **Oracle (validation):** A secondary implementation used as a reference for draw output (ROM-static interpreter, later a ROM micro-emu).
- **Parity Audit:** A structured comparison between ZScreamDungeon and yaze to list gaps and planned fixes.

## Data Sources & Truth
- Primary source of truth is the ROM itself (tables + routines).
- Optional cross-check against usdasm/jpdasm or z3dk labels for pointer discovery only.
- No hardcoded object catalogs unless backed by ROM parsing.

## Architecture Overview
**Modules**
- `RomReader`: load ROM, resolve banks, read pointer tables.
- `RoutineCatalog`: map object IDs to routine types + metadata.
- `StaticInterpreter`: execute declarative draw routines and emit `TileTrace`.
- `MicroEmu`: minimal 65816 interpreter + tilemap write traps.
- `TraceNormalizer`: normalize to 8x8 tile units and canonical ordering.
- `Validator`: compare traces, bounds, and report diffs.
- `Reporter`: JSON + Markdown reports; optional per-object dumps.

## Validation Oracles

### Oracle A: ROM-Static Interpreter
- Parse routine tables directly from ROM (same sources yaze uses for mapping).
- Execute a declarative library of draw routines (rightwards, downwards, diagonals, super-square, single patterns).
- Produce a standardized tile trace.

### Oracle B: ROM Micro-Emu
- Execute draw routine code in a minimal 65816 subset.
- Trap writes to a scratch tilemap buffer to produce a standardized tile trace.
- If unsupported opcodes are encountered, mark the object as “micro-unsupported”.

## Trace Format
All oracles and yaze must emit the same trace format:

```
struct TileTrace {
  uint16_t object_id;
  uint8_t size;
  uint8_t layer;   // BG1/BG2/BG3 if applicable
  int16_t x_tile;
  int16_t y_tile;
  uint16_t tile_id; // tile8 or tile16 depending on routine category (be consistent)
  uint8_t flags;    // hflip, vflip, priority if available (optional)
};
```

Minimum required fields: `object_id`, `size`, `x_tile`, `y_tile`, `tile_id`.

## Trace Normalization Rules
- Coordinates are in 8x8 tile units relative to the object origin (top-left).
- If a routine emits 16x16 tiles, expand to four 8x8 tiles with consistent ordering.
- Always include `tile_id` as the final 8x8 tile index after expansion.
- Sort traces by `(y_tile, x_tile, tile_id)` for stable diffing.

## Yaze Instrumentation
**Targets:**
- `src/zelda3/dungeon/object_drawer.cc`
- `src/zelda3/dungeon/object_drawer.h`

**Requirements:**
- Add a trace collector that records every tile write.
- Optional trace-only mode that emits traces without writing to the background buffer.

**Output:**
- `std::vector<TileTrace> yaze_trace`

## Validation Pipeline
1. Load ROM and build `RoutineCatalog`.
2. Generate `oracle_static_trace` via `StaticInterpreter`.
3. Generate `oracle_micro_trace` via `MicroEmu` when supported.
4. Instrument yaze draw to capture `yaze_trace`.
5. Normalize and compare traces.
6. Compute bounds from trace and compare to `ObjectDimensionTable`.
7. Emit JSON + Markdown reports and optional per-object trace dumps.

## Validation Logic
**Comparison tiers:**
- **Green:** `yaze_trace == oracle_static_trace == oracle_micro_trace`
- **Yellow:** `yaze_trace == oracle_static_trace`, micro-emu unsupported
- **Red:** mismatch (diff report required)

**Diff reporting:**
- Missing tiles (present in oracle, absent in yaze)
- Extra tiles (present in yaze, absent in oracle)
- Position mismatches
- Bounds mismatch vs `ObjectDimensionTable`

## Bounds Validation
- Compute bounds from trace (`min_x, min_y, max_x, max_y`).
- Compare against `ObjectDimensionTable` for `object_id, size`.
- Report mismatches with object ID + size.

## UX Parity Audit (ZScreamDungeon vs yaze)
**Deliverable:** A parity matrix document (table) listing feature, ZScream behavior, yaze behavior, gap, fix plan, and status.
**Draft matrix:** `docs/internal/agents/dungeon-object-ux-parity-matrix.md`

**Audit Inputs**
- ZScreamDungeon behavior capture (short notes or recordings per interaction).
- yaze implementation review (object selection, drag handlers, context menus, windowing).
- Validation report buckets (mismatches that affect selection/UX).

**Priority Interactions (Must Match)**
1) **Selection rules**
   - Click priority when objects overlap.
   - Hit-test bounds (tile-aligned vs pixel-aligned; inclusive edges).
   - Multi-select: shift/ctrl, marquee box behavior.

2) **Drag modifiers**
   - Axis locking, grid snapping, copy/duplicate on drag.
   - Dragging multi-select groups vs individual objects.
   - ZScream parity first, then optional enhancements.

3) **Context menus (Dungeon rooms)**
   - Room operations: duplicate, move, swap, palette/graphics ops, fill/clear, etc.
   - Actions on selection: align, distribute, lock, group.
   - Right-click behavior should be consistent with ZScream unless intentionally improved.

4) **Panel management / window persistence**
   - Persist size/position per window.
   - Clamp to visible screen bounds on restore.
   - Provide a "Reset layout" command.

5) **Custom object overlays**
   - Toggle in toolbar to show/hide custom elements.
   - Ensure overlays are selectable and snap correctly (minecart origins/tracks).

**Enhancements Beyond Parity (Candidate)**
- Add fast object filtering in palette/search.
- Quick toggle for collision/selection bounds overlay.
- Keyboard-first commands for frequent actions (place, rotate, flip, duplicate).

## Minecart Track Visualization + Validation Helpers (Oracle of Secrets)
**Goal:** Make minecart tracks + collision readable and editable in yaze without requiring emulator correctness.

**Data sources**
- Track starts: `oracle-of-secrets/Sprites/Objects/data/minecart_tracks.asm`
- Collision tiles: `$7F2000+` collision map (track tiles `0xB0–0xBE`, `0xD0–0xD3`) via ROM/room dump
- Track objects: dungeon object ID `0x31` (custom object binaries)
- Room audit: `oracle-of-secrets/Docs/World/Dungeons/GoronMines_Map.md`
- Config overrides: project file `[dungeon_overlay]` (track/stop/switch tiles, track object IDs, minecart sprite IDs)

**Work items (editor + tooling)**
1) **Track-start coverage audit**
   - Show all 32 track slots in the Minecart Track Editor and flag filler entries.
   - Highlight track subtypes used in a room with missing start data (room/x/y still filler).
   - Provide quick-jump from track slot → room in the editor.

2) **Collision overlay (track tiles)**
   - Render collision layer with a toggle (default off).
   - Emphasize track tiles (`0xB0–0xBE`, `0xD0–0xD3`) with a distinct color legend.
   - Mark stop tiles (`0xB7–0xBA`) and switch tiles (`0xD0–0xD3`) separately.
   - Draw per-tile direction arrows for straights/corners/Ts; show both routes on switch tiles.
   - If project overrides collision IDs, arrows are disabled unless a mapping table is added.

3) **Sprite/stop-tile validation**
   - Detect rooms with track collision tiles but no minecart sprite placed on a stop tile.
   - Warn if minecart sprites exist without any stop tile under them.
   - Provide a canvas overlay that highlights minecart sprites on/off stop tiles.

4) **Custom object draw + selection stability**
   - Ensure `DrawCustomObject` always uses custom object tile data (not ROM tiles).
   - Compute selection bounds from custom object extents, not size nibble.
   - Add a dedicated preview path for custom objects in the object selector.

5) **Camera origin hints (design aid)**
   - Show layout quadrant boundaries and allow a per-room “camera origin” note.
   - Use the note to pre-scroll camera in editor previews only (no ROM changes).

6) **Deferred mechanics (document only)**
   - `RoomTag_ShutterDoorRequiresCart` and `$36` speed switch are ready in ROM but unused.
   - Track in design docs; do not hook until stability goals are met.

## Test Matrix
**Default sizes:** `0, 1, 2, 7, 15`

**Special cases:**
- Diagonals: min + mid + max size
- SuperSquare: `0x00, 0x11, 0x33, 0x77, 0xFF`
- Fixed-size objects: size ignored (use 0)

**Exclusions:**
- Custom objects (0x31/0x32 + 0x100–0x103)
- Objects marked “nothing” by ROM tables

## Large Decor Priority Rules
Ensure spacing rules match ROM behavior:
- Rightwards decor 4x4 spacing = 2
- Rightwards decor 4x3 spacing = 4
- Rightwards decor 4x2 spacing = 8
- Rightwards decor 2x2 spacing = 12

These must align with `ObjectDimensionTable` and hit-test bounds.

## Phased Plan
**Phase 0: Scaffolding**
- Add `TileTrace` struct and trace collector to yaze draw path.
- Implement trace-only mode and dump output for a single object.

**Phase 1: Static Interpreter**
- Parse routine tables from ROM and implement declarative routines.
- Validate trace parity vs yaze for a focused object subset.

**Phase 2: Bounds Validation**
- Compute bounds from traces and compare to `ObjectDimensionTable`.
- Auto-flag spacing mismatches (large decor, pillar variants).

**Phase 3: Micro-Emu**
- Implement minimal 65816 coverage for draw routines.
- Expand opcode support based on "micro-unsupported" reports.

**Phase 4: CI + Reporting**
- Generate per-commit reports (non-blocking).
- Promote to gating after coverage reaches agreed threshold.

## Task Breakdown & Delegation (Estimates)
**Legend:** S = 0.5-1 day, M = 2-4 days, L = 1-2 weeks

1) **ROM table discovery + doc pass (S)**  
Define pointer tables, entry counts, and address conventions; add notes to this doc.

2) **RomReader + LoROM mapping (M)**  
Reusable reader with bank resolution, bounds checks, and 16/24-bit reads.

3) **RoutineCatalog builder (M)**  
Parse subtype tables and produce a normalized `ObjectRoutine` registry.

4) **TileTrace instrumentation in yaze (S)**  
Add trace collector, trace-only mode, and dump path.

5) **StaticInterpreter core routines (L)**  
Implement declarative routines for all standard draw types (rightwards, diagonals,
super-square, single patterns, edges, both-BG variants).

6) **TraceNormalizer + Validator (M)**  
Canonicalize traces, compare with oracles, and emit diffs + bounds reports.

7) **Bounds validation vs `ObjectDimensionTable` (S)**  
Compute bounds from trace and report mismatches.

8) **CLI + Reporters (S)**  
`dungeon-object-validate` + JSON/Markdown outputs.

9) **Targeted regression tests (M)**  
Golden traces for known-problem objects (large decor, pillars, diagonals).

10) **Micro-Emu (Deferred, L)**  
Hold until the current emulator infra stabilizes; keep behind a flag.

11) **ZScream parity audit doc (M)**  
Produce the parity matrix with concrete behavior notes.

12) **Selection + hit-test parity (M)**  
Align `ObjectDimensionTable` and selection rules with draw output; fix multi-select.

13) **Drag modifiers + shortcuts (S)**  
Implement ZScream parity, then add optional improved shortcuts.

14) **Context menu overhaul (S)**  
Room and object menus; align action ordering and add missing actions.

15) **Panel management persistence (S)**  
Persist window layout + clamp restore; add "Reset layout" action.

16) **Custom object overlay toggle (M)**  
Toolbar toggle + visibility state; ensure minecart overlays are selectable.

## Acceptance Criteria
- `dungeon-object-validate` runs end-to-end against vanilla ALTTP ROM.
- Static interpreter validates at least 90% of standard objects with zero diffs.
- Bounds mismatches are reported with actionable details (object ID, size, expected vs actual).
- Large decor spacing mismatches are auto-detected and grouped.
- Micro-emu coverage is tracked with a per-object supported/unsupported list.

## UX Parity Exit Criteria
- Parity matrix completed with ZScream vs yaze behaviors and tracked fixes.
- Selection rules and drag modifiers match ZScream for core interactions.
- Context menus cover all ZScream actions plus any approved improvements.
- Window/panel positions persist and restore within visible bounds.
- Custom object overlay toggle exists and minecart overlays are placeable + selectable.

## CLI + Reporting
**Command:**
```
yaze dungeon-object-validate --rom /path/to/alttp.sfc
```

**Command (with trace dump):**
```
yaze dungeon-object-validate --rom /path/to/alttp.sfc --trace-out /tmp/dungeon_object_trace_dump.json
```

**Artifacts:**
- `dungeon_object_validation_report.json`
- `dungeon_object_validation_report.csv`
- Optional per-object trace dumps (`--trace-out`)

## CI Strategy
- Start as non-blocking (report only).
- Promote to gating once micro-emu coverage is stable.

## Run Results (2026-02-03)
**Command:**  
`yaze dungeon-object-validate --rom yaze/zelda3.sfc --report /tmp/dungeon_object_validation_report_v2`

**Summary:**  
- object_count: 440  
- size_cases: 5  
- test_cases: 1950  
- mismatch_count: 1334  
- empty_traces: 565  
- negative_offsets: 134  
- skipped_nothing: 50  

**Top buckets observed:**  
- Size semantics mismatches (size=0 / 1to15or32) for IDs 0x000–0x002.  
- Negative Y offsets on diagonals/corners (IDs 0x009, 0x00C–0x020, 0x02F, 0x06C/0x06D, 0x0AC, 0x11F/0x120).  
- “No tiles drawn” for objects with expected >2x2 bounds (e.g. 0x03C, 0x04C, 0x0C0–0x0C9, 0x0D1/0x0D2, 0x0D9/0x0DB, 0x0E0–0x0E4, 0xFF0/0xFF1, 0xFF8).  
- Type 3 defaults (expected 2x2) but trace larger for several IDs (0xFCE, 0xFE6–0xFEB, 0xFDC, 0xFFA, 0xFC8).  

**Notes:**  
- Custom object overrides are disabled during validation to avoid false positives.  
- Routines mapped to `kNothing` are skipped (counted in `skipped_nothing`).  

## Run Results (2026-02-03, v3)
**Command:**  
`yaze dungeon-object-validate --rom yaze/zelda3.sfc --report /tmp/dungeon_object_validation_report_v3`

**Summary:**  
- object_count: 440  
- size_cases: 5  
- test_cases: 1950  
- mismatch_count: 1334  
- empty_traces: 565  
- negative_offsets: 134  
- skipped_nothing: 50  

**Notes:**  
- Size-zero selection alignment landed (see `ObjectDimensionTable`), but mismatch counts unchanged.  
- Reports: `/tmp/dungeon_object_validation_report_v3.json` and `/tmp/dungeon_object_validation_report_v3.csv`.  

## Run Results (2026-02-03, v5)
**Command:**  
`yaze dungeon-object-validate --rom yaze/zelda3.sfc --report /tmp/dungeon_object_validation_report_v5`

**Summary:**  
- object_count: 440  
- size_cases: 5  
- test_cases: 1950  
- mismatch_count: 1082  
- empty_traces: 550  
- negative_offsets: 124  
- skipped_nothing: 50  

**Notes:**  
- Edge/rail draw routines updated to match ZScream (plus2/plus3/plus23), plus selection size clamp lowered to 1 tile.  
- Reports: `/tmp/dungeon_object_validation_report_v5.json` and `/tmp/dungeon_object_validation_report_v5.csv`.  

## Run Results (2026-02-04, v6)
**Command:**  
`yaze dungeon-object-validate --rom yaze/zelda3.sfc --report /tmp/dungeon_object_validation_report_v6`

**Summary:**  
- object_count: 440  
- size_cases: 5  
- test_cases: 1950  
- mismatch_count: 813  
- empty_traces: 550  
- negative_offsets: 0  
- skipped_nothing: 50  

**Notes:**  
- Selection offsets are now part of bounds validation (expected_offset_x/y in the report).  
- Selection highlight rendering now uses selection offsets.  
- Reports: `/tmp/dungeon_object_validation_report_v6.json` and `/tmp/dungeon_object_validation_report_v6.csv`.  

## Run Results (2026-02-04, v7)
**Command:**  
`yaze dungeon-object-validate --rom yaze/zelda3.sfc --report /tmp/dungeon_object_validation_report_v7`

**Summary:**  
- object_count: 440  
- size_cases: 5  
- test_cases: 1950  
- mismatch_count: 647  
- empty_traces: 550  
- negative_offsets: 0  
- skipped_nothing: 50  

**Notes:**  
- Size/selection fixes reduced mismatches by 166 (v6 → v7).  
- Reports: `/tmp/dungeon_object_validation_report_v7.json` and `/tmp/dungeon_object_validation_report_v7.csv`.  

## Run Results (2026-02-04, v8)
**Command:**  
`yaze dungeon-object-validate --rom yaze/zelda3.sfc --report /tmp/dungeon_object_validation_report_v8`

**Summary:**  
- object_count: 440  
- size_cases: 5  
- test_cases: 1950  
- mismatch_count: 566  
- empty_traces: 550  
- negative_offsets: 0  
- skipped_nothing: 50  

**Notes:**  
- Size-mismatch bucket is nearly cleared; remaining errors are mostly empty traces.  
- Reports: `/tmp/dungeon_object_validation_report_v8.json` and `/tmp/dungeon_object_validation_report_v8.csv`.  

## Run Results (2026-02-04, v9)
**Command:**  
`yaze dungeon-object-validate --rom yaze/zelda3.sfc --report /tmp/dungeon_object_validation_report_v9`

**Summary:**  
- object_count: 440  
- size_cases: 5  
- test_cases: 1950  
- mismatch_count: 550  
- empty_traces: 550  
- negative_offsets: 0  
- skipped_nothing: 50  

**Notes:**  
- All remaining mismatches are “no tiles drawn.”  
- Reports: `/tmp/dungeon_object_validation_report_v9.json` and `/tmp/dungeon_object_validation_report_v9.csv`.  

## Run Results (2026-02-04, v10)
**Command:**  
`yaze dungeon-object-validate --rom yaze/zelda3.sfc --report /tmp/dungeon_object_validation_report_v10`

**Summary:**  
- object_count: 440  
- size_cases: 5  
- test_cases: 1950  
- mismatch_count: 515  
- empty_traces: 480  
- negative_offsets: 0  
- skipped_nothing: 50  

**Notes:**  
- Non-empty mismatches narrowed to: `0x0A4, 0x0D8, 0x0DA, 0x0DD, 0x0DE, 0xF8D, 0xF97` (size/selection parity).  
- Empty traces remain concentrated in Type 2/3 objects with fixed-size patterns.  
- Reports: `/tmp/dungeon_object_validation_report_v10.json` and `/tmp/dungeon_object_validation_report_v10.csv`.  

## Run Results (2026-02-04, v11)
**Command:**  
`yaze dungeon-object-validate --rom yaze/zelda3.sfc --report /tmp/dungeon_object_validation_report_v11`

**Summary:**  
- object_count: 440  
- size_cases: 5  
- test_cases: 1950  
- mismatch_count: 375  
- empty_traces: 355  
- negative_offsets: 0  
- skipped_nothing: 50  

**Notes:**  
- Remaining non-empty mismatches: `0x0DD, 0x0DE, 0xF8D, 0xF97`.  
- SuperSquare size-bit fixes reduced the size/selection bucket; empty traces remain the dominant source.  
- Reports: `/tmp/dungeon_object_validation_report_v11.json` and `/tmp/dungeon_object_validation_report_v11.csv`.  

## Run Results (2026-02-04, v12)
**Command:**  
`yaze dungeon-object-validate --rom yaze/zelda3.sfc --report /tmp/dungeon_object_validation_report_v12`

**Summary:**  
- object_count: 440  
- size_cases: 5  
- test_cases: 1950  
- mismatch_count: 365  
- empty_traces: 355  
- negative_offsets: 0  
- skipped_nothing: 50  

**Notes:**  
- Non-empty mismatches now only `0xF8D` and `0xF97` (PrisonCell, trace 10x4 vs expected 2x2).  
- Empty trace bucket (all remaining IDs):  
  - **Type 2:** `0x11C, 0x122-0x125, 0x128-0x129, 0x12C-0x13F`  
  - **Type 3:** `0xF95, 0xF9B-0xFA1, 0xFA6-0xFAA, 0xFAD-0xFAE, 0xFB3-0xFB9, 0xFCB-0xFCD, 0xFD4-0xFD5, 0xFDB, 0xFDD, 0xFE0-0xFE2, 0xFE9-0xFEA, 0xFEE-0xFF2, 0xFF4, 0xFF6-0xFF9, 0xFFB`  
- Reports: `/tmp/dungeon_object_validation_report_v12.json` and `/tmp/dungeon_object_validation_report_v12.csv`.  

## Run Results (2026-02-04, v13)
**Command:**  
`yaze dungeon-object-validate --rom yaze/zelda3.sfc --report /tmp/dungeon_object_validation_report_v13`

**Summary:**  
- object_count: 440  
- size_cases: 5  
- test_cases: 1950  
- mismatch_count: 314  
- empty_traces: 0  
- negative_offsets: 0  
- skipped_nothing: 50  

**Notes:**  
- Subtype2/3 tile-count fixes eliminated empty traces; remaining mismatches were all bounds/selection size mismatches for fixed-size objects.  
- Reports: `/tmp/dungeon_object_validation_report_v13.json` and `/tmp/dungeon_object_validation_report_v13.csv`.  

## Run Results (2026-02-04, v14)
**Command:**  
`yaze dungeon-object-validate --rom yaze/zelda3.sfc --report /tmp/dungeon_object_validation_report_v14`

**Summary:**  
- object_count: 440  
- size_cases: 5  
- test_cases: 1950  
- mismatch_count: 0  
- empty_traces: 0  
- negative_offsets: 0  
- skipped_nothing: 50  

**Notes:**  
- Selection bounds now match draw traces for all validated objects.  
- Reports: `/tmp/dungeon_object_validation_report_v14.json` and `/tmp/dungeon_object_validation_report_v14.csv`.  

## Run Results (2026-02-04, v15)
**Command:**  
`z3ed dungeon-object-validate --rom zelda3.sfc --report /tmp/dungeon_object_validation_report_v15 --trace-out /tmp/dungeon_object_trace_dump_v15.json --format json`

**Summary:**  
- object_count: 440  
- size_cases: 5  
- test_cases: 1950  
- mismatch_count: 5  
- empty_traces: 0  
- negative_offsets: 0  
- skipped_nothing: 50  

**Notes:**  
- Remaining mismatches are all `0xFEB` (sizes 0/1/2/7/15): trace 4x4 vs expected 8x8.  
- Trace dump: `/tmp/dungeon_object_trace_dump_v15.json`.  

## Run Results (2026-02-04, v16)
**Command:**  
`z3ed dungeon-object-validate --rom zelda3.sfc --report /tmp/dungeon_object_validation_report_v16 --trace-out /tmp/dungeon_object_trace_dump_v16.json --format json`

**Summary:**  
- object_count: 440  
- size_cases: 5  
- test_cases: 1950  
- mismatch_count: 0  
- empty_traces: 0  
- negative_offsets: 0  
- skipped_nothing: 50  

**Notes:**  
- Adjusted `0xFEB` selection bounds to 4x4 (matches trace).  
- Trace dump: `/tmp/dungeon_object_trace_dump_v16.json`.  

## Implementation Notes (2026-02-04, v10–v12)
- Added a TileTrace hook in `draw_routine_types` + `ObjectDrawer` to capture draw writes without rendering (trace-only mode).  
- SuperSquare size bits corrected to use 2-bit X/Y fields (affects 4x4 block and floor/spike variants).  
- Re-aligned several special routines to ZScream: 3x3/4x4 floors, water overlays, big hole, spike 2x2 in 4x4 super-square, table/rock 4x4.  
- Decor routines updated: doubled 2x2 spaced-2, bar 4x3.  
- Dimension table overrides added for SuperSquare and special objects (0xA4/0xD8/0xDA/0xDD/0xDE), leaving only PrisonCell in the non-empty mismatch bucket as of v12.  

## Implementation Notes (2026-02-04, v13–v14)
- Updated subtype2/3 tile-count mapping so fixed-size routines receive enough tiles (no more empty traces).  
- Expanded type2/type3 dimension overrides to match fixed-size + repeatable patterns (stairs, beams, utility, Triforce, boss shells).  
- PrisonCell bounds now match trace width (10x4).  

## Fix Plan (Based on Report v12)
**Status:** Completed in v14 (mismatch_count=0).  
1) **PrisonCell size parity**  
   - Override `0xF8D` and `0xF97` selection bounds to 10x4 (tile8).  
   - Confirm draw routine matches ROM/ZScream and selection overlay aligns.  

2) **Empty-trace bucket (Type 2/3 fixed-size objects)**  
   - Update subtype tile counts so tile data loads enough tiles for fixed-size routines (beds, stairs, utility, beams, Triforce, etc.).  
   - Re-run validator and confirm traces populate (should collapse empty_trace_count).  
   - If any IDs still return empty tiles, verify ROM offsets and mark true `kNothing` if appropriate.  

3) **Iteration loop**  
   - Re-run `dungeon-object-validate` after each tile-count batch update.  
   - Update this doc with the new mismatch delta and remaining offenders.  

## Fix Plan (Based on Report v2)
1) **Normalize size semantics**  
   - Align size=0 behavior for 1to15or32 routines in both draw routines and `ObjectDimensionTable`.  
   - Verify rightwards/downwards routines that use size nibble splits.  

2) **Anchor corrections for diagonals/corners**  
   - Adjust draw routines so origin aligns with selection bounds (eliminate negative offsets).  
   - Update hit-test bounds if routine origin is intentionally offset.  

3) **Fill “no tiles drawn” gaps**  
   - Identify object IDs with expected >2x2 but empty trace; implement missing draw paths or correct routine mappings.  
   - Confirm any intentionally-empty objects are mapped to `kNothing` to remove from mismatch set.  

4) **Type 3 overrides**  
   - Add explicit dimension overrides for Type 3 objects that draw >2x2.  
   - Validate against ROM disasm where possible.  

5) **Re-run validator and iterate**  
   - Track mismatch_count delta after each batch fix; target ≤300 mismatches before moving on.  

## Fix Plan (Based on Report v6)
1) **Resolve size-mismatch buckets with tiles drawn (top offenders)**  
   - Rightwards decor 1x8 spaced 12: IDs `0x055-0x056`.  
   - Downwards 4x2 variants and decorators: `0x063-0x068`, `0x076-0x077`, `0x081-0x084`, `0x088`.  
   - Weird 4x2 downwards special: `0x0B5`.  
   - Chest platforms with nibble-based sizes: `0x0C1`, `0x0DC`.  
   - Type2 horizontal repeats: `0x100-0x103`.  
   - Type3 overrides: `0xF83/F84/F87/F88/F8A/F8B/F8E/F8F`, `0xF92`, `0xF94`, `0xF96`, `0xFC8`, `0xFCE`, `0xFE6-0xFE8`, `0xFEB`, `0xFEC-0xFED`, `0xFFA`.  

2) **Empty-trace bucket (550 cases)**  
   - Identify objects where `ObjectParser` returns no tiles (type2/type3 heavy).  
   - Verify ROM offsets + routine mapping; mark true `kNothing` cases to remove from expected set.  
   - Add missing draw paths where mapping is correct but draw path is missing.  

3) **Iteration loop**  
   - Re-run validator after each bucket fix.  
   - Update this doc with report deltas and the remaining top offenders.  

## Fix Plan (Based on Report v7)
1) **Type2 repeatables**  
   - `0x104-0x107`: treat as rightwards 4x4 repeats (same as `0x100-0x103`).  
   - `0x11D`, `0x121`, `0x126`: 2x3 statues with 4-tile X spacing (repeatable).  
   - `0x134`: rightwards 2x2 repeat.  

2) **Somaria line bounds**  
   - Use direction-based bounds for `0xF83-0xF8C`, `0xF8E-0xF8F` (horizontal/vertical/diagonal).  
   - Keep offset for down-left (`0xF86`) based on length.  

3) **Turtle Rock pipes**  
   - Ensure full 2x6 / 6x2 footprint regardless of short tile data.  
   - Follow up on ROM tile data counts if visuals still look wrong.  

4) **Empty-trace bucket (550 cases)**  
   - Same as v6: track ObjectParser coverage and map true `kNothing` objects.  
   - v9 confirms all remaining mismatches are empty traces only.  

## Implementation Plan: Yaze Draw + Selection Fixes (v2)
**Goal:** Make draw output, selection bounds, and hit-test rules consistent with ROM behavior and ZScreamDungeon.

**Primary files**  
- `src/zelda3/dungeon/object_drawer.cc` (draw routines)  
- `src/zelda3/dungeon/object_dimensions.cc` (selection bounds + spacing)  
- `src/zelda3/dungeon/room_object.h` (object metadata tables)  

**Step A: Size semantics parity**  
1) Add explicit size-zero overrides in `ObjectDimensionTable` for routines that treat size=0 as a large size (e.g., 32 or 26).  
2) Ensure selection bounds use the same size interpretation as draw routines (no divergence between draw and selection).  
3) Add a helper to compute “effective size” for selection to mirror draw routines.  

**Step B: Diagonal/corner anchor alignment**  
1) Identify routines that write tiles with negative offsets (from validation report bucket).  
2) Decide whether to shift draw origin or expand selection bounds upward/left.  
3) Prefer aligning origin to top-left for consistency with ZScreamDungeon unless ROM clearly anchors otherwise.  

**Step C: Empty-draw objects**  
1) For each object ID with empty trace but expected bounds >2x2, confirm routine mapping + tile data offset.  
2) If the ROM marks object as “nothing,” update mapping to `kNothing` so it is excluded.  
3) Otherwise, implement or fix the missing draw routine path.  

**Step D: Type 3 dimension overrides**  
1) Add explicit overrides for Type 3 object IDs that draw larger than 2x2 (per mismatch bucket).  
2) Cross-check with ROM routine types if possible to avoid hardcoding incorrect sizes.  

**Step E: Large decor spacing rules**  
1) Verify spacing constants in `ObjectDimensionTable` for large decor rules (2/4/8/12).  
2) Align selection bounds and draw routines with these spacing rules.  

**Step F: Iterate with validator**  
1) Re-run `dungeon-object-validate` after each batch fix.  
2) Track mismatch_count deltas and update the parity audit notes.  

**Deliverables for delegation (High model)**  
- Patch list with file-level edits + rationale per bucket.  
- Updated validation report (JSON/CSV) with reduced mismatch counts.  
- Short notes on any ROM anomalies or ambiguous cases.  

## Appendix A: ROM Table Discovery (ALTTP LoROM)
**Addressing convention:** the offsets below are PC/ROM offsets as used in yaze
(`room_object.h`, `object_drawer.cc`, `object_dimensions.cc`). For SNES addresses,
apply standard LoROM mapping.

**Subtype 1 tables (objects 0x00-0xF7)**  
- Tile data offset table: `kRoomObjectSubtype1 = 0x018000`  
- Routine pointer table: `0x018200`  
- Entry count: 0xF8 entries (16-bit offsets)

**Subtype 2 tables (objects 0x100-0x13F)**  
- Tile data offset table: `kRoomObjectSubtype2 = 0x0183F0`  
- Routine pointer table: `0x018470`  
- Entry count: 0x40 entries (16-bit offsets)

**Subtype 3 tables (objects 0xF80-0xFFF)**  
- Tile data offset table: `kRoomObjectSubtype3 = 0x0184F0`  
- Routine pointer table: `0x0185F0`  
- Entry count: 0x80 entries (16-bit offsets)

**Door GFX helper tables (reference only)**  
- Door offset tables: `0x4D9E`, `0x4E06`, `0x4E66`, `0x4EC6`  
- `RoomDrawObjectData` base: `0x1B52` (PC, SNES $00:9B52)

**Discovery steps**
1. Read routine pointer table entry (16-bit offset).  
2. Resolve to bank 0x01 routine address and map to known draw routine.  
3. Read tile data offset table entry and compute tile data base + offset.  
4. Cross-check routine types with disasm labels (optional).

**Object ID ranges (from `RoomObject`)**
- Type 1: IDs `0x00-0xFF` (standard objects).  
- Type 2: IDs `0x100-0x1FF` (detected when `b1 >= 0xFC`).  
- Type 3: IDs `0xF80-0xFFF` (detected when `b3 >= 0xF8`; indices `id - 0xF80`).  

**LoROM mapping reminder**
- PC -> SNES: `bank = (pc / 0x8000) + 0x80`, `addr = (pc % 0x8000) + 0x8000`.  
- SNES -> PC: `pc = (bank - 0x80) * 0x8000 + (addr - 0x8000)`.  
- Confirm with a known label before relying on this for pointer decoding.

## Appendix B: Tile Data Parsing Notes
- Tile data offsets point into `RoomDrawObjectData` (`kRoomObjectTileAddress = 0x1B52`).  
- The offset table only gives a starting address; tile count and layout are
  determined by the draw routine (rightwards, diagonal, super-square, etc).  
- For validation, prefer a routine-aware parser (StaticInterpreter) rather
  than assuming a fixed tile count.  
- `ObjectParser` is already used in yaze as a fast, routine-aware loader for
  preview tiles; it can be used as a reference during validation scaffolding.

## Open Questions
- Which routine table addresses need explicit parsing beyond those already in `ObjectDrawer`?
- Minimum 65816 opcode coverage for micro-emu to handle all standard draw routines?
- When to enable full emulator fallback (if ever)?
