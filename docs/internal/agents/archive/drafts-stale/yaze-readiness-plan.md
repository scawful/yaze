# Yaze Readiness Review + Safety Plan (OOS/ALTTP)

**Status:** Draft  
**Owner:** Codex  
**Created:** 2026-02-05  
**Last Reviewed:** 2026-02-05  
**Next Review:** 2026-02-12

## Purpose
Evaluate yaze readiness for overworld, dungeon, graphics, and message editing in
the Oracle of Secrets (OOS) workflow. Define a safety-first plan to improve
accuracy vs ZScream and to harden backups, ROM role awareness, and tool
integration (z3dk, z3ed, mesen2-oos).

## Scope
- Target ROMs: vanilla ALTTP baseline + OOS patched ROMs.
- Features: overworld editor, dungeon editor, graphics editors, message editor.
- Safety: granular save controls, backup strategy, ROM signature validation.
- Tooling: z3dk lint, z3ed data inspection, mesen2-oos for savestate validation.

## Readiness Snapshot (High-Level)

| Area | Current Readiness | Primary Risks | Notes |
| --- | --- | --- | --- |
| Overworld | **Yellow** (usable with caution) | Data divergence across patches; pointer/address drift; editing on patched ROM without validation | Refactor largely complete; address overrides now configurable per-project. |
| Dungeon | **Yellow** (usable with caution) | ZScream parity gaps in selection/render; accidental global writes; custom object mismatch | Track collision overlays and minecart audits added; parity capture still needed. |
| Graphics | **Yellow** (partially stable) | Sheet/palette mismatch; inconsistent saves | “Edit Graphics” now jumps to object sheet; add palette warnings + better jump hints. |
| Messages | **Yellow** (usable) | Width validation + control code handling; import/export errors | Bundle export/import added; needs strict round‑trip validation UX. |

## Detailed Evaluation (2026-02-05)

### Overworld
**What works**
- Map canvas editing, tile16/tile8 editing, entrances/exits, and map properties.
- ZSCustomOverworld v2/v3 presets and version detection with ROM markers.
- Per-project ROM address overrides for expanded tables.
- Save toggles for maps/entrances/exits/items/properties.

**Risks / gaps**
- Save operations still write whole tables (map16/map32), so blast radius is
  large even when editing a single map.
- ZSCustomOverworld detection relies on markers; a patched ROM with relocated
  markers can mis-detect and write wrong offsets if overrides are incomplete.
- SaveMapProperties includes music/palettes; needs clear scope labeling to
  avoid unintended overwrites in OOS patches.
- Limited automated validation of pointer tables and map data integrity.

**Validation checklist**
- Compare exported map data vs z3ed dump for a known room set.
- Validate ROM markers and pointer tables before/after save (hash + offsets).
- Confirm Save Scope respects overworld flags (maps, entrances, exits, items).

### Dungeon
**What works**
- Room editing, object/sprite editing, and room graphics rendering.
- Custom collision overlay + minecart overlay config (track/stop/switch IDs).
- Custom object preview cache + subtype-aware selection bounds.
- Save Scope toggles and per-room save actions.

**Risks / gaps**
- ZScream parity still incomplete (selection bounds, drag modifiers, context
  menu ordering, and room actions).
- Large decor selection/preview remains inconsistent for some routines.
- Custom object file map requires correct project configuration; missing files
  lead to empty renders/selection with minimal UX guidance.
- Collision/pot/door tables are global; per-room save still risks unintended
  updates if not explicitly gated.

**Validation checklist**
- ZScream runtime capture pass for selection/drag/context menus.
- Room audit: object draw routines vs expected extents and collision tiles.
- Verify Save Room does not write global tables unless explicitly enabled.

### Graphics
**What works**
- Sheet browser + pixel editor + palette controls + link sprite editor.
- Per-sheet save of modified graphics (LC-LZ2 compress/decompress path).
- "Edit Graphics" in dungeon objects now jumps to associated sheet.

**Risks / gaps**
- Graphics pointers are derived from version constants; OOS patches with
  relocated pointer tables need explicit overrides (now supported via
  `rom_addresses` keys for `overworld_gfx_ptr1/2/3`, but still needs validation).
- Compression output can exceed original block size; no fit check or relocation.
- Save path writes directly to ROM; needs clear scope warning and backup.

**Validation checklist**
- Round-trip save on a single sheet, compare ROM bytes to baseline.
- Confirm pointer table addresses on OOS patched ROMs.
- Add size/fit diagnostics for compressed outputs.

### Messages
**What works**
- Message list editor with preview and dictionary support.
- Bundle import/export (JSON) and width warnings.
- Expanded message bank support (OOS) with configurable addresses.

**Risks / gaps**
- Line width warnings are best-effort; no hard enforcement in UI.
- Editor import path is lenient unless using strict CLI.
- Expanded message save assumes patched ROM; needs clear ROM policy gating.

**Validation checklist**
- Export bundle, re-import, diff vs baseline (vanilla + expanded).
- Validate strict parsing in CLI and surface errors in UI.
- Confirm expanded bank boundaries match ROM markers/overrides.

## Known Divergence & Risk Sources
- **ROM address drift:** OOS patches move tables; any hard-coded addresses can
  silently write wrong offsets.
- **ZScream behavior mismatch:** selection bounds, context menu order/actions,
  drag modifiers, and room operations still need parity capture.
- **Save blast radius:** full ROM saves can overwrite tables for unloaded rooms
  or uninitialized content (e.g., pot item tables).
- **Custom object data:** object binary mapping can diverge from ROM and from
  patched assets; missing files lead to incorrect previews/selection.
- **Tooling gaps:** missing automated validation of draw routines and collision
  overlays vs ROM data.

## Safety & Accuracy Principles
1. **Always validate ROM identity** (hash + signature + marker checks).
2. **Minimize blast radius** via per-subsystem and per-room saves.
3. **Never overwrite unknown data** (preserve ROM data for unloaded rooms).
4. **Provide explicit, granular backups** with clear provenance.
5. **Make parity measurable** (capture ZScream behavior and validate against it).

## Plan (Phased, Compaction-Friendly)

### Phase 1: Safety Baseline (Now → Short Term)
**Goal:** prevent data loss and accidental ROM divergence.
- Add **ROM role metadata** to projects:
  - `rom_role`: base | dev | patched | release.
  - `rom_hash`: expected hash (CRC32/SHA1) with mismatch warnings.
  - `rom_write_policy`: allow | warn | block.
- Expand **backup management**:
  - Per-save snapshot naming with timestamp + editor context.
  - Retention policy (keep last N; optional per-day archive).
  - Quick restore picker (recent backups list).
- Extend **granular save toggles** to overworld/graphics/messages.
- Add **Save Scope** UI (Save Room / Save Selection / Save Subsystem).

### Phase 1 Progress (2026-02-05)
**Implemented**
- ROM role + expected hash + write policy with confirmation gate on mismatch.
- Backup retention controls + daily snapshots + “ROM Backups” restore picker.
- Save Scope popup and per‑subsystem toggles, including message saves.
- Project‑level dungeon overlay config for track/stop/switch tiles, track object IDs,
  and minecart sprite IDs (UI editor included).
- Track collision overlay now supports direction arrows when IDs are configured.
- “Edit Graphics…” from dungeon objects now jumps to the associated sheet.
- Message bundle import/export support (CLI + docs) with line‑width diagnostics.
- Build stability: purge generated protobuf outputs on configure to avoid
  protoc/runtime version mismatches.

**Still pending**
- UI/UX parity capture vs ZScream (selection, drag modifiers, context menus).
- Validation framework for object draw routines (TileTrace + oracle).
- Additional save safety: per‑room/per‑table warnings for global tables.

### Phase 2: ZScream Parity Capture + Validation
**Goal:** make parity gaps concrete and measurable.
- Capture selection bounds, context menus, and drag modifier behavior in ZScream.
- Add automated **behavior checks** (unit/visual tests) for:
  - Selection hit-test vs bounds.
  - Drag modifier behaviors (axis lock, duplicate).
  - Context menu action ordering and availability.
- Implement missing parity items from `dungeon-object-ux-parity-matrix.md`.

### Phase 3: ROM Data Validation & Audit Tools
**Goal:** validate object draw routines and collision data without manual checks.
- Add a **Dungeon Object Validation Framework**:
  - Enumerate objects by room, validate draw routine output vs expected bounds.
  - Cross-check collision overlays and stop tile placements.
- Integrate **z3ed** for structured data dumps (room headers, objects, collision).
- Generate **audit reports** (CSV/JSON) for rooms with mismatches.

### Phase 4: Tooling Integration & Stability
**Goal:** stable dev loop with z3dk + mesen2-oos.
- z3dk:
  - Hook lint into yaze builds (optional) to ensure script/table edits stay valid.
  - Optionally auto-run lint on project save for ASM-related changes.
- mesen2-oos:
  - Add “launch ROM” and “open savestate” shortcuts.
  - Capture savestates for regression tests (transitions, edits, overlays).
  - Save metadata linking edits to savestates for repro.

## Area-Specific Recommendations

### Overworld
- Add project-configurable pointer overrides (similar to dungeon overlays).
- Support explicit “Save Area” and “Save Map Block” paths.
- Validate overworld markers (e.g., expanded table markers).

### Dungeon
- Finish ZScream parity for selection/menus/dragging.
- Integrate track/collision overlay audit into validation framework.
- Add per-room save warnings when collision/pot/door tables are global.

### Graphics
- “Edit Graphics” should jump to the correct sheet by default.
- Add quick “open sheet in graphics editor” context action.
- Add palette locking warnings on overwrite.

### Messages
- Implement robust import/export with validation:
  - Width checking, control codes, overflow warnings.
  - Round-trip diffs that show editor vs ROM deltas.
- Add batch error reporting and “preview line breaks”.

## Immediate Next Steps (Concrete)
1. Create project schema changes for `rom_role` + `rom_hash` + `rom_write_policy`.
2. Add backup retention config + UI (max backups, daily archive toggle).
3. Expand save toggles to overworld/graphics/messages with Save Scope UI.
4. Start ZScream capture pass for selection/menu behaviors.
5. Define object draw validation schema and a first “room audit” CLI/tool.

**Status note:** Items 1–3 are complete in current working tree; focus should
shift to items 4–5 and targeted UX parity fixes.

## Risk Register (Short)
- **Address overrides wrong:** mitigate with ROM signature checks + warnings.
- **Partial room saves:** mitigate with clear scope UI + per-table warnings.
- **Z3DK/ASM drift:** mitigate with lint hooks + metadata about patch version.
- **Parity assumptions:** mitigate with concrete ZScream captures and tests.
