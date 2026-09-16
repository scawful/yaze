# Yaze roadmap

Status: ACTIVE

Owner: [`backend-infra-engineer` with editor owners](agents/personas.md)

Created: 2026-09-14

Last reviewed: 2026-09-14

Next review: 2026-09-28

Universe task: `task_20260913T233703Z_21853` ([coordination system](agents/universe-coordination-spec.md))

Intent: ship v0.8.0 as a dependable Dungeon Editor milestone, with audited
vanilla object rendering and a validated Oracle editing workflow. Continue
bounded preview builds while completing the release requirements below.

The canonical editor status is the
[feature coverage report](../public/reference/feature-coverage-report.md).
Completed release history belongs in the root
[`CHANGELOG.md`](../../CHANGELOG.md), the
[release notes](../public/release-notes.md), and the
[detailed changelog](../public/reference/changelog.md), not here. The
[roadmap through v0.7.2](archive/roadmaps/roadmap-through-v0.7.2.md) is
preserved as a frozen snapshot for details that were formerly tracked here.

The [dungeon completion backlog](plans/dungeon-0.8.0-issue-test-backlog-2026-06-28.md)
owns object coverage, agent assignments, and execution order. The
[0.x release ladder](plans/release-ladder-0x-2026.md) owns later milestones.
The [z3dk v0.8.0 integration proposal](plans/z3dk-integration-0.8.0.md) remains
overdue for a scope refresh; it must not displace dungeon completion. See the
[plan directory guide](plans/README.md) for navigation.

## Release rule

An editor is not promoted because its panel opens, its serializer has a unit
test, or a direct ROM writer can roundtrip bytes. Promotion requires a named
user workflow and evidence appropriate to that workflow.

Merge reviewed, verified slices throughout development. Hold the v0.8.0 tag,
not consolidation. A bounded preview is not evidence that the full dungeon
milestone is complete. Unsupported custom ASM and universal hack compatibility
are not implied by this release.

## P0: v0.8.0 dungeon completion

All five outcomes are release requirements. Early previews must name their
remaining gaps rather than weakening these exit criteria.

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
- Present Music as playback/viewing until coordinated saving and incomplete
  writers are resolved.

Exit: application labels, getting-started material, beta instructions, and the
feature matrix tell the same story.

### 3. Application-path persistence proof

- Build one reusable harness for:
  **edit -> File > Save ROM -> close -> reopen disk file -> verify**.
- Complete Dungeon first, including undo/redo, object size and stream, sprites,
  doors, and changed room metadata. Cover the supported block/pit limits and
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

1. **Palette:** either join coordinated save transactionally or formalize the
   two-step model with full UI-to-disk readback.
2. **Graphics:** implement a safe per-sheet serializer and promote only after
   write/reopen/readback tests; keep current fail-closed behavior until then.
3. **Screen:** enable one independently verified data domain at a time instead
   of one all-or-nothing writer.
4. **Dungeon extensions:** expand independent room/state coverage beyond the
   release baseline and support new object/runtime contracts. Remove obsolete
   code alongside verified fixes, not as an unbounded separate rewrite.
5. **Custom-object authoring:** consider a broader catalog/designer beyond the
   supported Oracle assets only after the existing override, ice, and minecart
   workflows satisfy v0.8.0. Preserve source-patch compatibility.
6. **Overworld and Message:** add dedicated user guides and broader
   application-path coverage.
7. **UI system:** continue spacing, hierarchy, responsive panel, keyboard, and
   accessibility passes using shared theme/layout primitives rather than
   editor-local styling.
8. **Release:** add platform signing/notarization and architecture coverage only
   when the produced artifacts can be verified on the claimed targets.

## P2: advanced and experimental surfaces

- Complete Music instrument/sample persistence and coordinated song saving.
- Define the standalone Sprite editor's supported vanilla and `.zsm` workflows.
- Add a real Hex / Memory transaction, undo, dirty state, search, and readback.
- Complete Emulator save-state and conditional-breakpoint workflows.
- Publish Agent UI capabilities by build/provider and test one conditional GUI
  workflow.
- Expand WASM storage/download regression coverage; keep it labeled preview
  until it matches a clearly stated native subset.
- Consider ZScream/Hyrule Magic migration helpers after core save paths are
  proven; compatibility import must not outrank data safety.

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
