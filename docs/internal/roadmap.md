# Yaze roadmap

Status: ACTIVE

Owner: [`backend-infra-engineer` with editor owners](agents/personas.md)

Created: 2026-09-14

Last reviewed: 2026-09-22

Next review: 2026-10-06

Universe task: `task_20260923T000806Z_4560` ([coordination system](agents/universe-coordination-spec.md))

Intent: ship v0.8.0 as a dependable Dungeon Editor milestone, with audited
vanilla object rendering and a validated Oracle editing workflow. Continue
bounded preview builds while completing the release requirements below.

The [editor capability parity plan](plans/editor-capability-parity-plan.md)
owns the target baseline, work-package IDs, dependencies, and implementation
instructions. The [feature coverage report](../public/reference/feature-coverage-report.md)
records current editor readiness. Hyrule Magic / ZScream parity means completed
user workflows, not matching panel counts or an estimated percentage.
Completed release history belongs in the root
[`CHANGELOG.md`](../../CHANGELOG.md), the
[release notes](../public/release-notes.md), and the
[detailed changelog](../public/reference/changelog.md), not here. The
[roadmap through v0.7.2](archive/roadmaps/roadmap-through-v0.7.2.md) is
preserved as a frozen snapshot for details that were formerly tracked here.

The [dungeon completion backlog](plans/dungeon-0.8.0-issue-test-backlog-2026-06-28.md)
retains detailed object evidence and qualification packets under `DA-5`. The
[0.x release ladder](plans/release-ladder-0x-2026.md) owns later milestones.
The [z3dk v0.8.0 integration proposal](plans/z3dk-integration-0.8.0.md) remains
overdue for a scope refresh; it must not displace dungeon completion. See the
[plan directory guide](plans/README.md) for navigation.

## Reviewed baseline and immediate sequence

The 2026-09-22 review used mainline `d609e6254` and the local Workbench
candidate `7ba7d76ce`. The candidate adds placement/selection controls and has
43 focused passing tests, but is unpublished and is not the installed app.
Do not transfer those results to later commits. Current PR heads and their
separate evidence are recorded in the canonical plan; an open PR is not a
merged or qualified release.

The next local increment, `a730d6557`, implements door/sprite/pot-item undo
and shared entity properties in both room views. App/unit builds and 150
focused tests passed; it remains a Candidate without new ROM/runtime or
release qualification. DA-1 now proceeds to metadata/chests and compound edits;
DA-2 retains the remaining contextual-control work. See the canonical plan for
the exact commands and boundaries.

Execute the dungeon work packages in this order:

1. `DA-1`: shared room-edit undo for every supported mutable room domain.
2. `DA-2`: one contextual entity inspector using that mutation path.
3. `DA-3`: visual room connections that preserve actual game routing rules.
4. `DA-4`: complete room cloning and reusable selections with atomic apply.
5. `DA-5`: application-path persistence, independent rendering evidence, and
   exact-candidate package acceptance. Qualification can proceed alongside
   implementation, but only the final combined candidate closes the release.

Use the canonical plan for per-package prerequisites and acceptance. Preserve
existing pits/blocks models, room navigation, templates, placement, and object
undo. Extend these systems instead of replacing them or recording them as
missing.

## Release rule

An editor is not promoted because its panel opens, its serializer has a unit
test, or a direct ROM writer can roundtrip bytes. Promotion requires a named
user workflow and evidence appropriate to that workflow.

Merge reviewed, verified slices throughout development. Hold the v0.8.0 tag,
not consolidation. A bounded preview is not evidence that the full dungeon
milestone is complete. Unsupported custom ASM and universal hack compatibility
are not implied by this release.

## P0: v0.8.0 dungeon completion

All five outcomes below are release requirements. `DA-1` through `DA-4`
deliver complete dungeon authoring; `DA-5` qualifies those workflows and the
rendering baseline. Early previews must name their remaining gaps.

### 1. Release foundation

- Require exact-head native, security, and WASM checks before merging each
  relevant implementation slice. Run publish-disabled Release jobs again on
  the final combined candidate; an earlier successful package is not enough.
- Require relocatable Linux TGZ/DEB, macOS DMG/app, and Windows ZIP/NSIS
  artifacts with version, provenance, assets, and loader smoke checks.
- Manually open the packaged application on each claimed platform before a
  public tester build. Automated loader checks are not GUI acceptance.

Exit: one exact commit has green package gates and a recorded platform
acceptance matrix.

### 2. Honest editor boundaries

- Make Dungeon the release headline. Keep bounded Overworld and Message
  testing available without promising completion of every editor in v0.8.0.
- Publish Palette only with its required two-step save procedure.
- Keep Graphics and Screen mutation out of persistence testing until their
  serializers are proven.
- Make Hex / Memory read-only for ordinary testers until dirty state, undo, and
  readback exist.
- Remove or disable vanilla Sprite controls that imply unsupported editing.
- Describe Music precisely: playback and song-saving code exist; complete
  instrument/sample persistence and a qualified coordinated save workflow
  remain open. Do not imply that all audio authoring is ready.

Exit: application labels, getting-started material, beta instructions, and the
feature matrix tell the same story.

### 3. Complete authoring and application-path persistence proof

- Build one reusable harness for:
  **edit -> File > Save ROM -> close -> reopen disk file -> verify**.
- Complete `DA-1` through `DA-4`: shared undo, contextual entity editing,
  visual connections, and complete room/prefab reuse. Include sprites, doors,
  items/chests, metadata, and dependent room data in their mutation contracts.
- Qualify these paths under `DA-5`, including undo/redo and object size/stream.
  Cover the supported block/pit limits and
  reject overflow without partial saves. Reuse the harness for Overworld,
  Message, and Palette without turning this into a multi-editor rewrite.
- Make ROM-backed GUI coverage visibly skip or fail when its ROM fixture is
  unavailable. Do not count window-only smoke as editor persistence.

Exit: Dungeon has automated application-path save/reopen coverage and packaged
hands-on acceptance. Other advertised tester lanes have a complete-path test or
a documented temporary manual gate with an owner and follow-up.

### 4. Dungeon daily-driver pass

- Complete the systematic object inventory in the linked dungeon backlog.
  Resolve remaining thin/carpet strips, water/ice/moving-floor stamps, bars,
  stairs, corners, door families, and sprite-preview palettes before tagging.
- Audit shared rules against disassembly, then validate real room composition.
  Keep synthetic, ROM parser/drawer, fingerprint, Mesen ROI, and `z3ed` bounds
  evidence separate. Do not refresh a golden to conceal an unexplained change.
- Inventory project-mapped custom-object overrides, especially exact-ID
  wall/corner mappings, and keep decorative `0x31` subtypes separate from
  minecart semantics.
- Treat custom-object `.bin` publishing, room-object placement, minecart start
  tables, and generated collision as separate transactions until one workflow
  can validate and commit them together. For v0.8.0, make the existing staged
  workflow explicit and prove publish/save -> rebuild -> reopen -> Mesen;
  a universal custom-object designer is not required.
- Separate render correctness from in-game behavior and from editor UI layout.
- Finish the stable, non-reflowing issue-report dialog so reports are easy to
  capture without moving the canvas or context menu.

Exit: each supported object family has a recorded render contract, coverage,
and an independently checked representative or state where applicable. Known
render/save defects in that scope are closed. Oracle wall overrides, ice, and
minecart workflows are usable through their documented runtime path. Explicit
editor indicators and unimplemented animation are labeled, not pixel-parity
claims. Deferring a release requirement requires an explicit scope decision.

### 5. Accepted UI consolidation

- Consolidate reviewed theme, welcome, custom-object, and issue-dialog work
  after exact-head CI and the relevant acceptance checks. Preserve existing
  branches and dirty work; do not rewrite history to simplify the merge train.
- Preserve a clear center canvas, predictable side panels, and user-controlled
  picker placement; avoid transient text that changes canvas geometry.

Exit: the consolidated macOS application is deployed for hands-on acceptance,
and focused layout/theme tests pass.

## P1: subsequent editor milestones

Follow the release ladder after the dungeon milestone. These are not reasons
to delay focused dungeon fixes or expand v0.8.0 into an all-editor release:

1. **Overworld (`OW-1`, `OW-2`):** serialize sprites for all three game states
   and unify entity undo. Persistent scratchpad and paint/paste undo already
   exist; extend and qualify them instead of scheduling reimplementation.
2. **Graphics (`GF-1`):** safe compressed-sheet writes, allocation, and an
   import/export workflow. Keep current save rejection until it is safe.
3. **Screens (`SC-1`, `SC-2`):** persist supported screen/map domains through
   coordinated save, then complete secondary-screen workflows.
4. **Audio (`AU-1`, `AU-2`):** sample/instrument persistence and import, then
   event clipboard. Existing playback and song-saving code are starting points.
5. **Reference compatibility (`CO-1`):** verify named Hyrule Magic / ZScream
   layouts, expansion rules, and supported interchange formats. This is
   independent of UI feature parity; no automatic repair or arbitrary-hack
   support follows from detecting a ROM name.

Palette JSON import/export already exists when JSON support is enabled.
Retain its two-step save contract until transactional integration is proven.
Dedicated editor guides, shared UI accessibility/layout improvements, and
platform signing follow the package they support. See the canonical plan for
implementation details rather than maintaining a second feature backlog.

## P2: advanced and experimental surfaces

- Define the standalone Sprite editor's supported vanilla and `.zsm` workflows.
- Add a real Hex / Memory transaction, undo, dirty state, search, and readback.
- Complete Emulator save-state and conditional-breakpoint workflows.
- Publish Agent UI capabilities by build/provider and test one conditional GUI
  workflow.
- Expand WASM storage/download regression coverage; keep it labeled preview
  until it matches a clearly stated native subset.

## Ownership map

| Surface | Primary owner | Promotion evidence |
| --- | --- | --- |
| Editor UI and interaction | `imgui-frontend-engineer` | Focused UI tests plus manual packaged-app acceptance |
| ROM behavior and dungeon parity | `zelda3-hacking-expert` | ROM readback plus independent game/disassembly evidence |
| Save/test harness | `test-infrastructure-expert` | Complete application-path test with real discovered suites |
| Build and packages | `backend-infra-engineer` | Exact-head hosted matrix plus artifact lifecycle checks |
| Emulator runtime | `snes-emulator-expert` | Runtime workflow and state/readback tests |
| Documentation | `docs-janitor` | Link/build checks and agreement with current code |

## Review cadence

Every two weeks:

1. Recheck code and tests before changing a readiness label.
2. Move completed work to changelog/release notes.
3. Keep only active P0/P1/P2 outcomes here.
4. Record exact-head evidence in the release checklist or pull request, not as
   volatile counts in this roadmap.
