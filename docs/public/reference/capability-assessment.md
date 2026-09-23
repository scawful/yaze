# Yaze capability assessment

Last reviewed: 2026-09-22 for the v0.8.0 development line.

The target is complete ALttP authoring workflows covering the capabilities of
Hyrule Magic and ZScream: **create or edit → preview → undo/redo → save → reopen
→ verify in-game**. A visible panel, a serializer, or a passing synthetic test
alone does not establish that workflow. There is no percentage-parity claim.

The [editor capability parity plan](../../internal/plans/editor-capability-parity-plan.md)
defines the implementation sequence and acceptance requirements. The
[editor readiness matrix](feature-coverage-report.md) records current save
boundaries and source evidence. These supersede older parity scores and broad
Stable/Beta/WIP labels.

## Baseline and comparison scope

- Yaze source baseline: `d609e6254`. This is a development baseline, not a
  qualified 0.8.0 release.
- Dungeon placement candidate: `7ba7d76ce`, built on the reviewed PR #256 repairs
  and placement fixes. Its local app build and 43 focused tests passed; it was
  unpublished and had not replaced the installed app at this review.
- Reference-source snapshots used in the audit: Hyrule Magic `7d17cc2` and
  ZScream `0f6812d`. These are source comparison pins, not a statement that the
  two checkouts represent every published feature.
- The published ZScream comparison baseline is
  [3.2.5](https://github.com/Zarby89/ZScreamDungeon/releases/tag/3.2.5), together
  with the [3.2.4 feature ledger](https://github.com/Zarby89/ZScreamDungeon/releases/tag/v3.2.4).
  Expansion, per-area graphics, dungeon hole overlays, Tall/Wide overworld
  areas, independent world layouts, Special World editing, and tile-usage
  inspection must be assessed individually. Their appearance in this list
  does not mean Yaze lacks every one of them.

Comparison covers editing outcomes, supported ROM layouts, and persistence.
Yaze does not need identical window layouts or shortcuts. Existing Hyrule Magic
music editing belongs in the baseline; do not characterize music authoring as
unique to Yaze without a narrower, verified comparison.

## Current capability ledger

Status terms are **Implemented**, **Partial**, **Missing**, **Blocked**, and
**Candidate**, as defined in the readiness matrix. Evidence is recorded
separately as **Source**, **Focused**, **ROM**, **Runtime**, or **Release**.

| Workflow | Status at this review | Existing capability | Required completion |
| --- | --- | --- | --- |
| Place and resize dungeon tile objects | **Candidate** for the new Workbench workflow | Preview controls, physical dimensions, repeat/once placement, inserted-object selection, Place another, and uniform area resizing in `7ba7d76ce`; Source + Focused evidence | Qualify on disposable vanilla and Oracle base ROMs; verify undo, save/reopen, and game behavior before release promotion. |
| Edit every dungeon entity in place | **Partial** | Door, sprite, and item handlers and separate property editors exist. Workbench entity summaries are read-only at the baseline. | Unified entity inspector and domain-aware undo for doors, sprites, items, room headers, and compound edits. Tile-object undo does not prove entity undo. |
| Build and reuse complete rooms | **Partial** | Room-template helpers, export, destination fields, and connected-room browsing | Complete template schema and room cloning/import, including doors, chests, and headers; reusable selections; destination previews with engine adjacency constraints. |
| Edit overworld maps and entities | **Partial** | Tile editing, paste undo, persistent scratchpad, entrances, exits, items, properties, graphics groups, and state-specific sprites | Serialize sprites for every supported game state; unify entity undo and verify each saved domain. Existing map save does not persist sprite edits. |
| Edit graphics and graphics groups | **Blocked** for coordinated pixel-sheet persistence; other workflows **Partial** | Pixel editing, undo, graphics-group tools, import surfaces, and polyhedral editing | Safe compression and allocation, write boundaries, import/export roundtrip, and coordinated save. Preserve the current graphics save block until these are proved. |
| Edit screens and maps | **Blocked** for coordinated save with pending Screen edits | Dungeon-map, title, pause-map, and Tile16 editing surfaces; partial inventory UI | Complete and qualify each writer; naming-screen implementation is empty. Credits/ending screen authoring was not found in the reviewed surface and needs a specific audit. |
| Edit messages | **Partial** as a full application workflow | Parsing, preview, search, bundle/source workflows, transactional writer | GUI-to-disk save/reopen and runtime acceptance for named ROM profiles. |
| Edit palettes | **Partial** as a coordinated workflow | Broad palette groups, preview, undo, JSON exchange, explicit ROM-buffer commit | Integrate with coordinated save or qualify and clearly retain the two-step procedure. JSON exchange is already implemented when enabled. |
| Edit music, instruments, and samples | **Partial**; instrument/sample persistence **Missing** | Tracker, piano roll, playback, and song serialization | Complete instrument/sample writers, real WAV/BRR import, event clipboard, and application save integration. The sample import button currently produces placeholder data. |
| Author custom dungeon systems | **Partial** | Project mappings, tile workshop, Oracle collision/water tools, custom assets, source publication | Complete authoring and validation across mappings, source build, collision, visuals, and game behavior. A successful ROM save does not prove a source patch reached the game. |
| Edit global game properties and supported expansions | **Partial**, comparison audit required | Several relevant properties and expanded overworld formats exist | Enumerate the reference editors' settings and layout versions, map them to Yaze, and qualify each supported variant instead of assuming full compatibility. |

## Development order

1. **Complete dungeon authoring for 0.8.0.** Establish shared undo transactions,
   editable entity inspectors, complete room operations, connection editing,
   and bounded save/reopen/runtime acceptance.
2. **Complete overworld entity persistence.** Add the missing sprite writer and
   state handling before describing all entity editing as durable.
3. **Complete graphics and screens by data domain.** Keep unsafe writers blocked
   while compression, allocation, transactional writes, and readback are built.
4. **Complete audio and remaining reference workflows.** Instruments, samples,
   clipboard, naming/credits/ending, global properties, and expansion coverage
   need explicit implementation and acceptance records.

Full cross-editor parity is a sequence of milestones beyond 0.8.0. The
[formal plan](../../internal/plans/editor-capability-parity-plan.md) owns task
dependencies and completion criteria; this page is the public summary.

## Existing strengths to preserve

- ROM/project write policy, range-conflict checks, and backup/restore handling.
- Dungeon diagnostics for IDs, streams, layers, geometry, and render evidence.
- Persistent overworld scratch space, undoable paste, palette JSON exchange,
  graphics-group editing, and polyhedral editing. Do not recreate these because
  an old feature list calls them missing.
- Cross-platform build/package infrastructure and scriptable `z3ed` workflows.
- Validation that distinguishes model behavior, ROM structure, independent
  Mesen output, and the packaged desktop application.

## Dungeon visual parity rule

Synthetic draw-registry replay, real-ROM parser checks, room fingerprints,
independent Mesen captures, and structural `z3ed dungeon-object-validate`
results answer different questions. Record which evidence was collected, the
exact code and ROM identity, and whether a mismatch actually fails the test.
Do not call a diagnostic that only prints mismatches a parity gate.

Runtime effects such as HDMA water control and moving backgrounds may require
emulator-state or structural proof beyond a static crop. Maintain explicit
expected-difference baselines; unexplained new differences fail acceptance.

## Choosing a workflow today

Use the [readiness matrix](feature-coverage-report.md) to choose a bounded edit
on a copied ROM. Dungeon, overworld maps/entrances/exits/items, and messages have
save implementations; their whole-editor status is still Partial. Overworld
sprite edits are not in the save path. Palette currently requires **Save to
ROM**, then **File > Save ROM**. Pending Graphics and Screen edits block that
coordinated save. Music song operations use a separate path and do not establish
sample or instrument persistence.

Hyrule Magic, ZScream, project source patches, and Mesen remain useful independent
comparison tools. The [Beta Testing guide](../usage/beta-testing.md) describes
bounded tester procedures; the current matrix takes precedence if an older
guide describes a broader save capability.
