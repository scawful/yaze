# Yaze capability assessment

Last reviewed: 2026-09-23 for the v0.8.0 development line.

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
- Subsequent entity-authoring candidate: `a730d6557`, following plan commit
  `5fc5af950`. It adds DA-1 door/sprite/pot-item undo and DA-2 shared entity
  properties. App and unit-test builds succeeded; 150 selected tests across
  21 suites passed with no skips, including 22 entity lifecycle and five shared
  inspector UI cases. Only formatting whitespace changed after that run, checked
  through whitespace-normalized staged content. ROM save/reopen, runtime, CI,
  installation, and release qualification remain pending.
- Prior room/chest-content candidate: `aeb0b1200` on
  `codex/editor-parity-dungeon-authoring`. It adds room metadata undo, named
  property controls, shared existing chest reward/type editing, and an atomic
  multiroom **Clear stale** action. App and unit builds passed; **475 focused
  tests across 36 suites passed with zero skips**, including 94 new cases.
  Scoped Clang analyzer checks passed on the two new mutation modules.
  Synthetic save/decode/byte-preservation tests do not establish vanilla/Oracle
  application-to-disk or game-runtime qualification. Mainline and the installed
  application remain unchanged.
- Prior compound chest candidate: `478206247` on the same
  integration branch. Placement, deletion, and small/big conversion keep the
  object and contents together in one undo action. Ordinary object copy/paste
  preserves rewards; reorder/layer edits preserve chest correspondence.
  The inspector starts canvas placement and follows selected chests. Source
  implementation is present; focused verification: **569 tests across 37 suites passed, with zero failures and zero skips**.
  App and unit builds passed. Scoped Clang analyzer checks passed for the two new mutation modules. The shared six-slot
  chest/big-key-lock limit and global 168-record table are preflighted, including
  other dirty and unopened rooms and exact manifest-protected chest regions.
  Object-stream allocation still checks at Save; full application disk/game,
  manual UX, CI, and packaged acceptance remain unqualified.
- Current mixed-selection candidate: `eac49e2bd` on the same branch.
  Delete/duplicate/cut/paste, nudge, and group drag stage objects, doors, sprites,
  pot items, and paired chest rewards together, with one undo action. Copy
  preserves the prior clipboard on failure. Shared grids preserve spacing;
  doors require valid wall anchors and duplicate in place. **640
  focused tests across 39 suites passed with zero failures and
  zero skips**. Save-time allocation, vanilla/Oracle application/runtime,
  manual UX, CI, installation, and Release qualification remain separate.
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
| Edit doors, sprites, and pot items in place | **Candidate** | `a730d6557`: shared Workbench/standalone property controls, domain-aware undo/redo, render refresh, reserved-sprite validation, and paste-selection restoration; Source + Focused evidence | ROM save/reopen, runtime, CI, and packaged acceptance remain. Inspector edits are blocked when connected-view and interaction room contexts differ. |
| Edit room properties | **Candidate**, Source + Focused at `aeb0b1200` | Typed header/tag/layout/floor/destination edits with undo; named room choices; atomic multiroom metadata batch. | Qualify application persistence/runtime on the exact candidate. |
| Create, edit, copy, and delete dungeon chests | **Candidate**, Source + Focused at `478206247` | Paired object/contents operations and undo; named rewards; canvas placement/selection; ordinary object clipboard retains rewards; ordering, shared event slots, global contents capacity, and manifest preflight. | Object-stream allocation remains Save-time. Qualify vanilla/Oracle application disk persistence, game behavior, manual UX, and release acceptance. |
| Edit mixed dungeon selections | **Candidate**, Source + Focused at `eac49e2bd` | One operation and undo across objects, doors, sprites, pot items, and paired chest rewards; validated clipboard; rigid nudge/drag; rejected/no-op edits preserve data/history. | Object-stream space checks at Save; author-time manifest checks currently cover chest regions. Full application/runtime and packaged acceptance remain. |
| Complete editing of every dungeon element | **Partial** | Entity editing at `a730d6557`, metadata at `aeb0b1200`, paired chests at `478206247`, and mixed selections at `eac49e2bd` extend the original audit. | Remaining authoring domains, reciprocal connections, complete room operations, and exact-candidate qualification keep DA-1 and DA-2 open. |
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
   and bounded save/reopen/runtime acceptance. The next bounded implementation
   is bounded DA-3 reciprocal connection authoring using the existing metadata
   and mixed-selection transaction boundaries, followed by DA-4 complete room
   cloning/import. Preserve the implemented selection and chest operations.
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
