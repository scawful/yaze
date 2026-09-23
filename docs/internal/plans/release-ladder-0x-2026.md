# 0.x Release Ladder (Editor-First)

**Status:** ACTIVE

**Owner (Agent ID):** docs-janitor

**Created:** 2026-04-20

**Last Reviewed:** 2026-09-23 (capability baseline; later version assignments remain proposals)

**Next Review:** 2026-10-06

**Universe Task:** `task_20260923T000806Z_4560`

## Summary

This plan defines the intended `0.x` release ladder after `v0.7.1`.

The [editor capability parity plan](editor-capability-parity-plan.md) owns
work-package definitions, implementation instructions, and acceptance. The
[roadmap](../roadmap.md) owns current priorities. The
[dungeon completion backlog](dungeon-0.8.0-issue-test-backlog-2026-06-28.md)
retains detailed `DA-5` rendering and qualification evidence. This ladder maps
those packages to release themes; later version assignments are proposals.

The reviewed baseline is mainline `d609e6254` plus local candidate `7ba7d76ce`.
The candidate is not merged, published, or installed. Its 43 focused tests do
not qualify a release. Do not infer capabilities or readiness from the version
number alone.

The subsequent `a730d6557` candidate adds shared entity properties and
per-domain door/sprite/pot-item undo, with an app/unit build and 150 focused
passing tests. The newer `aeb0b1200` candidate adds room metadata and existing
chest-content undo, named property/reward controls, and atomic multiroom
staircase cleanup. Its app/unit build and 475 focused tests passed with zero
skips; scoped Clang analyzer checks passed on two new mutation modules.

The compound chest candidate `478206247` now pairs chest objects
and contents for placement, deletion, type changes, and undo. Ordinary object
copy/paste retains rewards; reorder/layer edits preserve record correspondence.
Source + Focused evidence; focused verification: **569 tests across 37 suites passed, with zero failures and zero skips**.
App and unit builds passed. Scoped Clang analyzer checks passed for the two new mutation modules. Preflight enforces six
combined chest/big-key-lock slots, chest-before-lock order, the global 168-record
table including dirty/unopened rooms, and exact chest-region manifest policy.
Object-stream allocation remains a Save-time check.

The prior mixed-selection candidate `eac49e2bd` adds one operation
and undo across tile objects, doors, sprites, pot items, and paired chest rewards.
Delete/duplicate/cut/copy/paste/nudge/group drag use shared validation; rejected
operations publish no partial edits, and no-op drags preserve redo history.
Movement uses a common grid and exact door slots; door duplication and paste
without a canvas target retain the source anchors. Save closes pending gestures.
Source + Focused evidence: **640 tests across 39
suites passed with zero failures and zero skips**. Allocation remains Save-time;
author-time manifest preflight currently covers chest ranges only.

The prior reciprocal normal-door candidate `be973563f` adds
paired endpoint previews and explicit **Create Return Door** / **Update Pair**
commands for ordinary outer-wall doors.
Creating a return or changing both layers uses one existing batch Undo/Redo;
exact slot pairing, room-table boundaries, passage conflicts, stale previews,
ROM identity, and unopened target parsing are validated. Lazy loading retains
clean project WaterFill tiles, mask, and count and rejects dirty partial state.
Verification:
**807 tests across 44 suites —
passed with zero failures and zero skips**. App/unit builds: passed. Scoped
analyzer: passed for the six selected implementation files and their explicit header scope.

The current authored-room clone/import candidate `bb185d1ae`
follows authored-room source `2520aa6b0` and documentation `df1cc4f2d`. It adds `yaze.room` v1 clipboard exchange and same-project
clone, seven selectable domains with Core defaults, preserved target destinations
unless explicitly included, detached persistence/capacity/write-policy preflight,
stale-source/target rejection, and one undo action. Other rooms retain their
water masks, and the shared water table preserves unopened saved zones.
Verification: **989 tests across 49
suites; all passed, zero failures and zero skipped**. The prior 807-test result belongs to the
connection checkpoint, not this candidate.


The qualification repair enforces native block storage before the torch table,
accepts shared empty pot terminators, moves preview GameData to the heap, and
offers explicit recovery without shared room properties. JSON preserves residual
chest records and normalizes physical block slots. The exit-door report remains
not reproduced; confirmed exit guards have regression coverage. Independent
save/reopen qualification must be rerun on this source.

DA-1/DA-2 remain partial for broader authoring domains and qualification. DA-3
remains partial for stairs, pits, special door families, and runtime traversal.
DA-4 now has the authored-room transfer increment; project-file/asset
compatibility and portable resources remain. Numeric graphics/layout/message
references are not an asset pack, and native file selection is not included.
**Next qualify the combined candidate through DA-5 before broadening features:**
application disk transaction/rollback, independent reopen, and game behavior.
Synthetic persistence checks do not qualify 0.8.0. Vanilla/Oracle GUI-to-disk,
runtime, manual UX, CI, and packaged acceptance remain separate gates. See the
canonical plan and its handoff for exact commands and evidence scope.

The primary release train is completion of the main ALTTP editors:
- Dungeon
- Overworld
- Graphics
- Screen
- Sprite
- Music
- Memory
- workspace/project lifecycle

Secondary trains remain important, but they should not displace the main
editor backlog:
- `z3ed` CLI expansion and hardening
- `z3dk` integration
- Oracle AI / emulator-debugging infrastructure
- Oracle of Secrets ROM-hack workflows
- iOS/macOS companion follow-through

This is a long-running `0.x` product line. `0.8.0`, `0.9.0`, `0.10.0`,
`0.11.0`, `0.12.0`, and later minors are normal milestone releases. None of
them should be treated as an implied ramp to `1.0`.

## Decisions And Constraints

- Use patch releases for stabilization, release follow-through, and narrow
  polish.
- Use minor releases for one coherent milestone or a tightly bundled set of
  related editor gaps.
- Keep the main ALTTP editors as the default prioritization anchor unless a
  secondary train directly unblocks editor completion.
- Prefer overview and navigation expansions that complement focused
  single-room editing instead of replacing it outright.
- Treat `z3dk` as a staged multi-release train, not as the sole headline that
  can consume an entire minor by default.
- Treat `z3ed`, Oracle debugging, and OoS support as continuous supporting
  workstreams that land in slices alongside the primary editor milestones.

## Release Ladder

### 0.7.2 (historical release scope)

This section preserves the earlier scope; it is not the active work queue.

**Primary goal:** stabilization and release follow-through

**Must-ship themes:**
- post-`0.7.1` build/docs/version alignment
- editor source-layout stabilization after the reorg
- current dungeon/workbench/object-tile/editor cleanup
- focused parity/polish already in the active panel lanes

**Secondary slices allowed:**
- `z3ed` coverage expansion
- Oracle debugger fixes that are already in-flight
- Mesen event/subscription cleanup
- low-risk cartographer performance work

**Do not let this become:**
- the z3dk release
- a broad multi-editor backlog bundle

### 0.8.0

**Primary goal:** Dungeon Editor completion milestone

Complete the supported dungeon workflow before tagging. Continue bounded
previews and verified merges while finishing it; a preview is not the release
exit criterion.

**Must-ship themes:**
- `DA-1`: consistent undo across supported room-edit domains
- `DA-2`: contextual door, sprite, item/chest, and room-property editing
- `DA-3`: visual destination editing with correct adjacency/stair/pit rules
- `DA-4`: complete room cloning and reusable selections
- `DA-5`: qualify the combined dungeon authoring and rendering workflow
- object selector/browser preview parity
- verification of the remaining unknown dungeon object types
- remaining visible object/render discrepancies
- better room object type identification
- dungeon workbench/save-path follow-through so integrated workbench and
  panel flows stay reliable under real editing sessions
- responsive room navigation and toolbar layout that preserve center-canvas
  visibility in tighter windows and pane configurations
- supported pits/blocks persistence through editable room state, with verified
  capacity guards; expansion beyond the vanilla runtime limit is separate work
- supported Oracle wall overrides, ice objects, and minecart editing with
  explicit source publication, ROM save, rebuild, and in-game verification
- independent object-family evidence and complete application save/reopen tests
- exact-candidate package acceptance on each claimed desktop platform, with
  WASM still labeled by its verified preview subset

**Secondary slices allowed:**
- narrow `z3ed` automation or validation improvements
- Oracle debugging work that materially helps dungeon parity work
- optional connected-room overview / grouped-room navigation work when it
  materially improves dungeon navigation and context without replacing
  focused single-room editing

**Do not let this become:**
- primarily a z3dk integration release
- a general platform/runtime refactor
- a universal custom ASM/object designer or a blanket all-hacks compatibility claim

### 0.9.0

**Primary goal:** Overworld Editor completion milestone

**Must-ship themes:**
- `OW-1`: sprite saving across all three game states
- `OW-2`: shared entity undo and complete entity editing
- application-path save/reopen coverage for advertised overworld operations
- qualify and extend existing paint/paste undo and persistent scratchpad
- verify export, eyedropper, and remaining Tile16 workflows against the
  reference baseline before adding new work

**Secondary slices allowed:**
- `z3dk` M0-M1 class groundwork:
  - optional `z3dk-core` link
  - basic assemble path behind a flag
  - diagnostics surfacing
- `z3ed` CLI ergonomics that support overworld workflows

**Do not let this become:**
- a broad AI-platform milestone
- a mixed editor catch-all release

### 0.10.0

**Primary goal:** graphics and screen persistence milestone

**Must-ship themes:**
- `GF-1`: safe compressed graphics writes, allocation, and import/export
- `SC-1`: screen/map persistence integrated with coordinated save
- `SC-2`: secondary-screen and clipboard workflows with named format scope
- qualified UI-to-disk readback for existing Palette JSON import/export and
  its two-step save contract
- deeper workflow coverage for the existing Sprite `.zsm` editor; vanilla
  sprite room placement remains a Dungeon responsibility

**Secondary slices allowed:**
- Oracle desktop workflow maturation:
  - live SRAM wiring into progression views
  - Annotation Overlay implementation
- `z3ed` doctor/editor automation command expansion

**Do not let this become:**
- the full Oracle platform release
- a large project/session architecture rewrite

### 0.11.0

**Primary goal:** Music + Memory completion release

**Must-ship themes:**
- `AU-1`: instrument/sample persistence and real WAV/BRR import
- `AU-2`: Music event clipboard with undo and validation
- coordinate existing song saving with the complete audio transaction
- Memory Editor search and safe write/undo contracts, separately scoped

**Secondary slices allowed:**
- `z3dk` M2-M3 class work:
  - symbol extraction
  - project symbol table population
  - unified Mesen2 client consolidation
- AI/debugging infrastructure improvements tied to real disassembly, symbols,
  and trace support

**Do not let this become:**
- a full IDE-in-yaze release
- a platform migration release

### 0.12.0

**Primary goal:** workspace/project lifecycle release

**Must-ship themes:**
- layout/workspace serialization
- `.yaze` / `.yazeproj` lifecycle tightening
- settings/layout persistence coverage
- migration/path normalization regression coverage
- emulator save-state workflow completion

**Secondary slices allowed:**
- `z3dk` M4-M5 class work:
  - hover / go-to-definition
  - `.mlb` export
  - lint-on-save
- Oracle of Secrets assembly/project workflow improvements:
  - symbol navigation
  - build error mapping
  - snapshot/diff ergonomics

**Do not let this become:**
- an SDL3 migration milestone
- a plugin architecture milestone

### 0.13.0+

These are later architectural trains once the main editor backlog is no longer
the dominant product risk:
- SDL3 migration
- plugin architecture
- enhanced memory tooling beyond search
- broader documentation overhaul
- larger AI/editor convergence work, including AI-assisted map generation after
  the manual Overworld/Dungeon editors and validators are safe enough to review
  generated output (`docs/internal/plans/ai-map-generation-roadmap-2026-07-03.md`)

## Reference-editor compatibility

`CO-1` tracks named Hyrule Magic / ZScream layout, expansion, and format
compatibility after the relevant writers are safe. It may land alongside the
editor it supports. No version assignment promises universal hack support.
The [legacy HM / Parallel Worlds proposal](hyrule-magic-support-plan.md)
contains unverified assumptions and is not an implementation recipe.

## Secondary Train Placement

### z3ed

`z3ed` should ship continuously as support infrastructure:
- ROM doctor/validation growth
- editor automation command coverage
- test/CI-friendly provider and automation workflows

It should not normally become the only headline of a minor release.

### z3dk

`z3dk` should be planned as a staged train across multiple minors:
- `0.9.x`: optional embedding + diagnostics groundwork
- `0.11.x`: symbols + Mesen client consolidation
- `0.12.x`: hover/go-to-definition + `.mlb` export + lint-on-save

The existing scoped proposal remains the feature-level reference:
`docs/internal/plans/z3dk-integration-0.8.0.md`

### Oracle AI / Debugging Infrastructure

Land in narrow slices where they directly improve debugging and automation:
- real disassembly
- execution trace buffering
- symbol loading
- memory watchpoints/breakpoints
- bulk memory reads
- progression/annotation workflow wiring

The feature-level references remain:
- `docs/internal/plans/ai-infra-improvements.md`
- `docs/internal/plans/oracle-yaze-integration.md`

### Oracle of Secrets ROM-Hack Workflow

Treat OoS support as a long-lived product workflow, not a one-off side project:
- assembly editor symbol navigation
- build-error mapping
- snippets/macros
- snapshots/diff ergonomics
- RAM live views, annotation overlays, follow-mode tooling

These should land incrementally alongside the release ladder, especially once
the main ALTTP editor milestones are under control.

## Exit Criteria

This plan is doing its job if:
- release discussions default to the editor-completion ladder first
- `0.8.0` through `0.12.0` are scoped around the main editor backlog before
  platform/AI/tooling expansion
- secondary trains remain visible, scheduled, and bounded
- roadmap/status docs stop implying that `0.8.0` is primarily the z3dk release

## Validation and status updates

Update package status in the canonical capability plan and readiness report.
Record source SHA, discovered/executed/skipped tests, and ROM/package evidence
for each promoted workflow. A documentation change or an existing smoke test
is not implementation or release evidence. Keep z3dk, Oracle integration, and
AI infrastructure plans subordinate to the editor milestone they support.
