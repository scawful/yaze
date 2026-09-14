# Yaze roadmap

Status: ACTIVE

Owner: `backend-infra-engineer` with editor owners

Created: 2026-09-14

Last reviewed: 2026-09-14

Next review: 2026-09-28

Universe task: `task_20260913T233703Z_21853`

Intent: make Yaze safe and understandable enough for a bounded v0.8.0 tester
preview, then promote editors by proving complete user save workflows.

The canonical editor status is the
[feature coverage report](../public/reference/feature-coverage-report.md).
Completed release history belongs in `CHANGELOG.md` and
`docs/public/release-notes.md`, not here.

## Release rule

An editor is not promoted because its panel opens, its serializer has a unit
test, or a direct ROM writer can roundtrip bytes. Promotion requires a named
user workflow and evidence appropriate to that workflow.

## P0: tester preview

These are the only blockers for inviting a small, explicitly bounded tester
group.

### 1. Release foundation

- Merge the cross-platform release-gate work after exact-head native, security,
  WASM, and publish-disabled Release jobs are terminal green.
- Require relocatable Linux TGZ/DEB, macOS DMG/app, and Windows ZIP/NSIS
  artifacts with version, provenance, assets, and loader smoke checks.
- Manually open the packaged application on each claimed platform before a
  public tester build. Automated loader checks are not GUI acceptance.

Exit: one exact commit has green package gates and a recorded platform
acceptance matrix.

### 2. Honest editor boundaries

- Publish Dungeon, Overworld, and Message as the initial ROM-editing lanes.
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
- Apply it to Dungeon, Overworld, Message, and Palette first.
- Make ROM-backed GUI coverage visibly skip or fail when its ROM fixture is
  unavailable. Do not count window-only smoke as editor persistence.

Exit: each tester-ready editor has at least one automated complete-path test or
a documented temporary manual gate with an owner and follow-up.

### 4. Dungeon daily-driver pass

- Continue the ROM parser/drawer, fingerprint, Mesen ROI, and `z3ed` validation
  ladder for reported object families.
- Prioritize remaining water/ice/moving-floor strips, bar and staircase objects,
  corners, and sprite-preview palettes reported during hands-on testing.
- Inventory project-mapped custom-object overrides, especially wall/corner
  aliases, and keep decorative `0x31` subtypes separate from minecart semantics.
- Treat custom-object `.bin` publishing, room-object placement, minecart start
  tables, and generated collision as separate transactions until one workflow
  can validate and commit them together.
- Separate render correctness from in-game behavior and from editor UI layout.
- Finish the stable, non-reflowing issue-report dialog so reports are easy to
  capture without moving the canvas or context menu.

Exit: known issues are reproducible by room/object, fixed issues have an
independent proof tier, and remaining exceptions are listed rather than hidden
behind a “1:1” claim.

### 5. Accepted UI consolidation

- Merge the theme/visual-language pass after its release dependency is green.
- Rebase and merge the simplified welcome flow and stable dungeon issue dialog.
- Preserve a clear center canvas, predictable side panels, and user-controlled
  picker placement; avoid transient text that changes canvas geometry.

Exit: the consolidated macOS application is deployed for hands-on acceptance,
and focused layout/theme tests pass.

## P1: daily-driver editors

Work in this order after the tester preview is contained:

1. **Palette:** either join coordinated save transactionally or formalize the
   two-step model with full UI-to-disk readback.
2. **Graphics:** implement a safe per-sheet serializer and promote only after
   write/reopen/readback tests; keep current fail-closed behavior until then.
3. **Screen:** enable one independently verified data domain at a time instead
   of one all-or-nothing writer.
4. **Dungeon:** close the remaining systematic object-parity backlog and remove
   superseded render special cases as verified rules replace them.
5. **Custom and gameplay objects:** replace scattered filename/subtype/overlay
   conventions with a project catalog that owns identity, preview, placement,
   collision semantics, source output, validation, and migration. Start with
   wall-graphics overrides, icy/slippery floors, moving-floor/water objects, and
   minecart tracks. Preserve source-patch compatibility; do not create
   editor-only visuals that cannot reach the ROM build.
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
