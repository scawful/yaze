# Dungeon 0.8.0 completion backlog and agent assignments

Status: ACTIVE

Owner: backend-infra-engineer (integration), with zelda3-hacking-expert

Created: 2026-06-28

Last Reviewed: 2026-09-14

Next Review: 2026-09-28

Universe task: `task_20260913T233703Z_21853`

Planning audit: `task_20260914T160613Z_19789`

## Release decision

Ship v0.8.0 as a dependable Dungeon Editor milestone: audited vanilla object
rendering, safe editing and persistence, and a validated Oracle workflow.
Continue preview builds and reviewed merges; hold the release tag until the
requirements are complete. The [roadmap](../roadmap.md) owns product priority,
the [rendering spec](../agents/dungeon-object-rendering-spec.md) owns behavior,
and the [release checklist](../release-checklist.md) owns final acceptance.
This file owns the work breakdown and coverage gaps; do not create another
parallel object backlog.

Scope excludes a universal custom ASM designer, completion of Oracle's dungeon
content, every other editor, and untested compatibility with arbitrary hacks.
Existing Oracle systems must work through their documented build path. Native
release readiness and WASM preview readiness are separate claims.

## Planning baseline, not new test results

The September audit inspected mainline `d42785c24` and the clean combined
preview `75f817d521c0d656a251926916a520f5b44c4b89`. The preview includes draft
custom-object and UI work that is not yet all on mainline. Recheck each branch
before implementation. No ROM writes, emulator captures, or runtime tests were
performed by this planning audit. Existing tests below are evidence locations,
not newly passed acceptance gates.

## First implementation results (2026-09-14, local integration)

Three agents executed bounded R1, C1, and T1 slices based on preview
`75f817d521c0d656a251926916a520f5b44c4b89`. Their reviewed changes are integrated
on `codex/tester-preview-consolidation`; this is not a mainline merge, release
qualification, or deployment. The installed preview and canonical ROMs were
left unchanged.

| Slice | Result | Remaining proof |
| --- | --- | --- |
| R1 strips/bars | **Fixed-awaiting-proof:** `0x4C` incorrectly repeated twelve-word stamps. USDASM uses nine source words for cap/body/cap columns, width `2*nibble+4`, height 3. Drawer, parser, registry minimum and selection dimensions now agree. `0x8F` payload corrected from six words to four. No draw-rule defect found for the other six assigned thin-strip IDs or vertical `0x8F` in the tested scenarios. | Independent Oracle `0x042` bar/corner joins; broader floor slice remains open. |
| C1 custom assets | **Verified asset-format contract:** all 21 current Oracle assets decode, publish to temporary copies, reload, and match the source-backed tilemap replay. Sparse words and full tile attributes have additional synthetic coverage. No production codec change was needed. | Actual source rebuild/runtime, wall-override migration and minecart edit/publish/collision workflow are not certified by this test. |
| T1 audit gates | **Verified runner contract:** single-config `bin/` and `bin/test`, explicit multi-config selection, nonempty discovery and exact execution, missing/skipped tests, fresh bounds reports, and report/input-ROM alias rejection are covered by 29 fixtures. Registered as `DungeonVisualParityAuditContract` in CTest. | One-object GUI Save ROM / quit / fresh-process reopen remains the next T1 slice. Cross-platform package execution remains Q1. |

Implementation commits: R1 `2b30cee4b`, `912449837` and shared corrections
`d5b406dd9`; C1 `a5693fd5a`; T1 `d637097a2`, `9c375739d`, `8c25295c6`.

Fresh local evidence (single-config Release, four-worker build limit):

- Targeted strip/bar/parser/dimension regression: **10/10 passed**, including
  `BarPayloadsAndDrawTracesMatchUsdasm/Vanilla`. The pre-fix run reproduced six
  failures out of nine checks; the ROM test was then added for GREEN.
- Broader parser/dimension/drawing/layer/custom contracts: **354/354 passed**.
  This includes the custom-asset run, not an additional independent total.
- Custom contracts: **45/45 passed**, including **21 real assets**, zero skips.
  `YAZE_TEST_ORACLE_CUSTOM_OBJECTS` explicitly selected Oracle's
  `Dungeons/Objects/Data`. Unset skips those opt-in asset tests; a configured
  missing/malformed asset fails. XML records each source path and SHA-256.
- Maintained parity ladder: Tier 1 **36/36**; Tier 2 **11/11** plus **1/1**
  payload-table check; Tier 3 **10/10**; Tier 4 **7/7** existing Mesen-baseline
  tests, all with zero skips. No baseline was refreshed and no new Mesen
  capture was made. These counts do not imply full-family pixel parity.
- Tier 5: **1,190 cases, zero mismatches, zero empty traces**.
- `ctest --test-dir build/presets/mac-ai --output-on-failure
  -R '^DungeonVisualParityAuditContract$'`: **1/1**, covering 29 scenarios.
  Both shell scripts passed `bash -n` and ShellCheck.

Canonical vanilla SHA-1 stayed `6d4f10a8b10e10dbe624cb23cf03b88bb8252973`;
Oracle base `oos168.sfc` stayed `58c9fadc6228d3d78d6baa7b7cc1c31cf57ed196`.
The read-only asset inventory used Oracle HEAD
`f55fbed8aafcac84f31127fcc98ea4382437343b`: 21 files, 456 bytes, 155 tile words.
The source decoder is a separately written ASM replay, not emulator execution.

Local run artifacts: `/tmp/yaze-wave1-bars-red.xml`,
`/tmp/yaze-wave1-bars-green.xml`, `/tmp/yaze-wave1-focused.xml`,
`/tmp/yaze-wave1-oracle-assets.xml`, `/tmp/yaze-wave1-parity-audit.log`, and
`/tmp/yaze-wave1-dungeon-object-validation.json`. These temporary artifacts
are not committed fixtures; use the commands below to regenerate evidence.

## Second implementation slice: object controls and floor rules (2026-09-14)

Continue development without assigning routine qualification work to the user.
The integration owner runs automated checks; manual acceptance remains a later
release gate, not a prerequisite for the next object improvement.

- The active Workbench inspector now exposes width and height in tiles for
  repeating 4x4/3x3 floor and 2x2 spike families. Wheel changes height;
  Shift+wheel changes width. Each two-bit axis clamps independently instead of
  carrying into the other dimension. Custom objects expose mapped, named
  variants and are excluded from generic wheel/bulk resizing.
- Placement wheel events update the pending object, not a previous selection.
  The tile handler owns the preview; palette refresh redraws it without
  restoring stale dimensions. The legacy editor also uses the shared resize
  helper instead of its unsupported `0x10/0x08/0x04` increments.
- The routine-58 audit covers all 19 mapped IDs, all 16 sizes and all three
  stored streams, plus room-edge motif phase and unrelated game-state changes.
  Floor1/Floor2 wrappers now delegate to the same stamper. No new rendering
  defect was established for these water/ice/moving-floor stamps.
- Water edges `0x3F–0x46`, `0x79/0x7A` and waterfalls `0x47/0x48` have
  source-backed payload, size, boundary, attribute and compatible-corner
  regressions. These are not new emulator captures or proof of room-level
  composition/palette correctness.

The third slice below follows up on room-level water/ice/corner composition
and the different base dimensions of `0xC1/0xDC/0xDD`. Moving-wall
`0xCD/0xCE` direction selectors remain separate from independent floor axes.

Fresh local evidence: the app and unit target build in Release; **420/420**
selected tests pass, including actual ImGui combo clicks at narrow/wide and
large-font layouts, stable popup registration, width/height independence,
custom variant protection, and preview resize -> palette refresh -> placement.
This total includes the 150-test focused run. The shared-floor trace fixture's
initial H/V input ordering mistake was corrected against `TileInfo`'s V/H
constructor contract; no renderer or expected trace was weakened to pass it.
The maintained parity ladder also passed again: Tier 1 **37/37**, Tier 2
**11/11** plus **1/1** table check, Tier 3 **10/10**, Tier 4 **7/7**, all with
zero skips; Tier 5 **1,190 cases, zero mismatches and empty traces**. No goldens
were refreshed or new Mesen images captured. Both canonical ROM SHA-1 values
above remained unchanged. Logs: `/tmp/yaze-object-wave2-broad.log` and
`/tmp/yaze-object-wave2-parity.log`.

## Third implementation slice: room graphics, fixed walls and platforms (2026-09-14)

Integrated on the same preview branch; no mainline merge, installation or ROM
write. Source review and automated checks remain the integration owner's work.

- **Mushroom Grotto water:** the animated-sheet loader incorrectly treated a
  24-bit pointer operand as the table address and indexed the room tileset
  instead of the entrance main group. It now dereferences the ROM pointer,
  preserves relocated tables, and loads the selected/common frame spans.
  Oracle room `0x04A` changes from garbled pool interiors to the water motif.
  This is a static-frame correction, not live animation support.
- **Sanctuary wall `0x13C`:** corrected the payload from 16 to 24 words,
  replaced the generic 4x4 renderer with the source's 24x6 facade, and kept
  the center on the active layer while the facade targets BG1. The empty
  bottom-center opening and horizontal-flip OR behavior are covered.
- **Fixed corners:** twelve subtype-2 aliases now reuse fixed routine 116
  instead of repeating when an editor object contains a stale nonzero size.
  The canonical size-zero corner traces already matched USDASM. Tests cover
  all 24 `0x100–0x117` IDs; this is not a new all-corners visual-parity claim.
- **Platform controls:** `0xC1/0xDC/0xDD` expose their actual dimensions and
  independent two-tile axis changes. The inspector and hover text use the
  existing dimension table; moving-wall direction bits and custom variants
  stay protected.
- **Puffstool:** Oracle sprite `0xB1` now uses its source frame-zero tiles,
  offsets, overlapping OAM order and palette selector. The active canvas
  selects this override from the loaded Oracle project, without mutating a
  global profile. Bare ROM opens and unrelated projects retain the default
  preview. Actual runtime CGRAM remains separate proof.

Implementation commits: room graphics `872e419a0`, Sanctuary `4429dc58e`,
fixed corners `b5ec124c8`, platforms `a3632aa85`, Puffstool `c2eb94f86`.

Fresh Release evidence, with four build workers:

- **415/415** selected unit/ROM tests passed, zero skips. This includes the
  corrected precondition in three new tests: the dimension table must be
  loaded before querying it. No expected geometry was changed to pass.
- The animated-loader regression failed before the fix; both canonical
  vanilla and Oracle base ROMs pass the pointer/group/frame-span regression.
  Invalid table pointers include WRAM aliases against a 4 MiB synthetic ROM.
- Sanctuary's two pre-fix tests failed on payload length and missing facade;
  source-backed post-fix mapping, tile trace and mixed-layer checks pass.
- All seven existing Mesen-baseline tests pass without changing their images.
  Only the four BG2/composite fingerprint values for vanilla room `0x016`
  were refreshed. A counterfactual regression reproduces the old hashes and
  counts exactly by substituting the old, incorrectly indexed sheet `0x94`
  for water sheet `0x5D`. It proves that all 43,316 changed BG2 pixels belong
  to tiles `0x1B0/0x1B1`, with other graphics, tile words and coverage intact.
  Per-pixel priority changes follow corrected opacity; tile priority bits do
  not change. The composite is unchanged outside those water pixels.
- Final maintained ladder: Tier 1 **42/42**; Tier 2 **11/11** plus **1/1**
  table check; Tier 3 **11/11**; Tier 4 **7/7**, all zero skips. The runner
  now requires the new Sanctuary/corner families; its CTest contract passes
  **33 scenarios**, including missing-family rejection.
- Tier 5: **1,190 cases, zero mismatches, zero empty traces**.
  Both canonical ROM SHA-1 values above remain unchanged.

Reproduce the focused run from the preview checkout (`YAZE_TEST_ROM_VANILLA`
and `YAZE_TEST_ROM_EXPANDED` must point to the canonical vanilla and Oracle
base ROMs, respectively):

```sh
cmake --build build/presets/mac-ai --config Release --target yaze_test_unit yaze z3ed --parallel 4
build/presets/mac-ai/bin/yaze_test_unit --gtest_filter='RoomGraphicsPaletteTest.*:SupportedRomRoles/RoomObjectRomParityTest.AnimatedRoomGraphicsFollowRomPointerAndMainGroup/*:RoomObjectEncodingTest.*:TileObjectHandlerTest.*:DungeonWorkbenchObjectSizeUiTest.*:SpriteRenderPreviewTest.*:DrawRoutineMappingTest.*:ObjectDrawerRegistryReplayTest.*:ObjectDimensionsTest.*:ObjectDrawingComprehensiveTest.*:ObjectDimensionTableTest.*:DrawRoutineRegistryTest.*:DimensionServiceTest.*:ObjectLayerSemanticsTest.*' --gtest_output=xml:/tmp/yaze-wave3-broad.xml
scripts/agents/audit-dungeon-visual-parity.sh --build-dir build/presets/mac-ai --config Release --with-validate-report /tmp/yaze-wave3-validation.json
ctest --test-dir build/presets/mac-ai --output-on-failure -R '^DungeonVisualParityAuditContract$'
```

Local artifacts: `/tmp/yaze-wave3-broad.log`, `/tmp/yaze-wave3-broad.xml`,
`/tmp/yaze-wave3-drift.xml`, `/tmp/yaze-wave3-parity.log`,
`/tmp/yaze-wave3-validation.json`, and read-only room renders
`/tmp/yaze-wave3-room04a.png` / `/tmp/yaze-wave3-room04a-fixed.png`.
These are temporary evidence, not committed fixtures or emulator captures.

The fourth slice below addresses Manhandla's source preview and the headless
layer-manager setup. Remaining water/ice composition and sprite CGRAM still
need isolated runtime evidence; these source-backed tests do not close the
full R1/V1 release packets.

## Fourth implementation slice: export composition and Oracle boss preview (2026-09-14)

Integrated on the preview branch only. No mainline merge, installation or ROM
write is part of this slice.

- **Headless composition:** `RenderService` now applies room merge and effect
  settings before compositing. The regression failed before the fix in six
  vanilla rooms (`0x034/0x035/0x036/0x037/0x076/0x08F`) at each of three
  scales. It compares every decoded PNG pixel against a separately loaded,
  configured room composite. This protects agreement with the canvas, not
  independent SNES color math.
- **Export safety:** dungeon PNG output rejects aliases of both the active
  ROM and the original source ROM, including sandbox workflows. Collision
  and render exports share the existing path guard instead of duplicating
  it. Symlinks are resolved before collapsing `..`, preserving actual
  filesystem identity for source and output paths. CLI/API scale parsing
  and direct service calls reject malformed,
  non-finite and out-of-range values; the supported range is `0.25–8.0`.
  PNG-disabled builds return `Unimplemented`, never raw RGBA labeled PNG.
- **Manhandla `0x88`:** the Oracle project preview uses the source static
  front head, not vanilla Mothula. Its two 16×16 entries use CHR `0x120` at
  `(0,8)` then `0x100` at `(0,-8)`, OBJ palette 1, and no flips. Raw planar
  graphics come from `Bosses/manhandla.bin` relative to `assets_folder`
  (`Sprites` in Oracle), with an exact 8,192-byte size requirement. The
  viewer caches the file per project/assets/profile context; reopening the
  project refreshes successes and failures. Missing or malformed assets
  leave a labeled marker. Neither the room graphics nor the ROM is changed.
- **Ice/water source audit:** 38 Oracle `0xC8/0xD1` objects in rooms
  `0x08C/0x0CE/0x04A/0x033` matched all 2,640 independently expanded
  `(x,y,tile,layer,H/V/priority,palette)` writes. Actual payloads, palette
  pointers and frame-zero animation spans agree with the source rules.
  No new stamp/anchor defect was demonstrated, so those draw routines were
  not changed. These traces do not close their independent visual proof.

Manhandla source limit: `ApplyManhandlaGraphics` loads OBJ page 1 in room
`0x05A`; an editor preview of the room `0x08C` placement does not establish
that runtime hook's behavior there. `ApplyManhandlaPalette` changes BG CGRAM,
not the head's OBJ palette 1. Spawned heads, BG body, animation, and actual
runtime sprite CGRAM remain outside this static source contract.

Verified against implementation head `742cf6247` in the Release `mac-ai`
build, with four build workers:

- **477/477** selected unit/ROM tests passed, zero skips; execution matched
  the discovered test inventory exactly. This includes the third-slice
  regression set, 40 collision-command tests using the extracted path
  helpers, 15 render-safety/HTTP tests, and seven new sprite/resource tests.
- **57/57 full-image comparisons** passed in two integration tests:
  15 vanilla plus four Oracle rooms, each at scales `0.25`, `1`, and `2`.
  The vanilla test failed before the fix on 18 room/scale combinations;
  its expected images were not changed to obtain a pass.
- The actual Manhandla source asset passed the static-head fingerprint
  check: 331 visible indexed pixels. Asset SHA-256:
  `dc3f3a479ee3ed5cf7b5ccf5e63eef63823c699eadf325407830ad93cc9053bb`.
- A separate temporary executable compiled `RenderService` and its test
  with `YAZE_CLI_HAS_PNG` undefined and passed the explicit missing-encoder
  regression. This verifies the disabled service branch on macOS arm64
  with existing dependencies, **not** a wholly libpng-free build or another
  platform's build. The normal build configuration was not changed.
- Maintained ladder: Tier 1 **42/42**; Tier 2 **11/11** plus **1/1** table
  check; Tier 3 **12/12**; Tier 4 **7/7**, zero skips throughout. Tier 5:
  **1,190 cases, zero mismatches, zero empty traces**. No golden or Mesen
  image was refreshed in this slice.
- Native `yaze`, `z3ed`, unit and integration targets build successfully.
  Both canonical ROM SHA-256 values are unchanged. Fresh Oracle
  `rom-doctor` reports zero critical/errors and the same two heuristic
  `known_corruption_pattern` warnings at `0x1E878B` and `0x1EF540`; no
  automatic repair was performed.

Reproduce with `YAZE_TEST_ROM_VANILLA` and `YAZE_TEST_ROM_EXPANDED` set as in
the third slice, and `YAZE_TEST_ORACLE_SPRITE_ASSETS` pointing to Oracle's
`Sprites` directory:

```sh
cmake --build build/presets/mac-ai --config Release --target yaze_test_unit yaze_test_integration yaze z3ed --parallel 4
wave4_filter='DungeonRenderScaleTest.*:DungeonRenderServiceTest.*:DungeonRenderCommandsTest.*:HttpApiHandlersTest.DungeonRender*:DungeonCollisionJsonCommandsTest.*:SpritePreviewResourceCacheTest.*:RoomGraphicsPaletteTest.*:SupportedRomRoles/RoomObjectRomParityTest.AnimatedRoomGraphicsFollowRomPointerAndMainGroup/*:RoomObjectEncodingTest.*:TileObjectHandlerTest.*:DungeonWorkbenchObjectSizeUiTest.*:SpriteRenderPreviewTest.*:DrawRoutineMappingTest.*:ObjectDrawerRegistryReplayTest.*:ObjectDimensionsTest.*:ObjectDrawingComprehensiveTest.*:ObjectDimensionTableTest.*:DrawRoutineRegistryTest.*:DimensionServiceTest.*:ObjectLayerSemanticsTest.*'
build/presets/mac-ai/bin/yaze_test_unit --gtest_list_tests --gtest_filter="$wave4_filter"
build/presets/mac-ai/bin/yaze_test_unit --gtest_filter="$wave4_filter" --gtest_output=xml:/tmp/yaze-wave4-focused.xml
build/presets/mac-ai/bin/yaze_test_integration --gtest_filter='Dungeon*RoomRenderParityTest.*HeadlessPngMatchesRoomComposite' --gtest_output=xml:/tmp/yaze-wave4-export-green.xml
scripts/agents/audit-dungeon-visual-parity.sh --build-dir build/presets/mac-ai --config Release --with-validate-report /tmp/yaze-wave4-validation.json
ctest --test-dir build/presets/mac-ai --output-on-failure -R '^DungeonVisualParityAuditContract$'
```

Local evidence: `/tmp/yaze-wave4-focused.xml`,
`/tmp/yaze-wave4-export-red.xml`, `/tmp/yaze-wave4-export-green.xml`,
`/tmp/yaze-wave4-no-png.xml`, `/tmp/yaze-wave4-parity.log`,
`/tmp/yaze-wave4-validation.json`, and `/tmp/yaze-wave4-rom-doctor.json`.
These are temporary run artifacts, not emulator captures or committed fixtures.

The fifth slice below follows up on ice/water composition and sprite
palettes. The `0x0CE` ice footprint is partly covered by an upper-layer pit;
its capture must exercise the composite, not assume an unobstructed BG2 stamp.

## Fifth implementation slice: color math and Babasu preview (2026-09-14)

Integrated and verified at `264afe558` on the preview branch only. The
installed app and mainline were not changed; no release qualification is
claimed.

- **Layer mode 7:** use saturating five-bit color addition, not mode 4's
  averaging. USDASM `$02A20C` selects `CGADSUB=$32` for full addition;
  `$02A212` selects `$62` for half addition. Equal colors must also add.
  Both tile-priority settings are covered. Mode 4 retains its existing
  behavior, and the door-sensitive mode 6 branch is unchanged. The output
  still chooses the nearest color in the winning indexed palette bank;
  this is not general exact SNES RGBA color math.
- **Babasu `0x9D`:** the static preview now uses OBJ palette 5 and a full
  16×16 upper tile at `(0,-8)`, replacing the wrong palette and 16×8 piece
  at `(0,-16)`. Source frame 12 at `$0DBCA0` / PC `0x6BCA0` uses CHR
  `4E/5E` at `y=-8/0`. Tabulated properties `$0A XOR $01` select page 1,
  palette 5. Tests pin all preview pixels and the actual Expanded-ROM
  frame/property bytes. Shared overlap rows contain identical pixels, so
  this pose cannot independently prove OAM order. Runtime animation and
  sprite CGRAM remain outside the static test.
- **Oracle room audit:** rooms `0x0CE`, `0x033`, and `0x04A` all use mode 6,
  not 7. The mode 7 fix therefore does not explain their reported symptoms.
  `0x0CE` base/patched headers and all 93 ordered objects agree, as does
  the 64×64 D1 composite ROI at `(232,184)`. Other room pixels differ
  because the patched ROM includes wall overrides. Do not use full-room
  base/patched equality as an acceptance requirement.

Verification, with four build workers and explicit vanilla/Expanded-ROM and
Oracle sprite-asset paths as in the fourth slice:

- **RED before fixes:** mode 7 full-add failed while the mode 4 control
  passed; Babasu's pixel regression failed while its ROM source contract
  passed. No expected image was refreshed to obtain GREEN.
- **509/509 unit/ROM tests**, zero failures/skips, exact agreement with
  discovered names. This includes all fourth-slice selections plus all 30
  `RoomLayerManagerTest` cases; 45 tests cover compositor and sprite/cache
  behavior specifically. The maintained ladder does not select those
  suites, so retain this separate run.
- **57/57 PNG/composite comparisons** across 19 rooms and three scales;
  maintained ladder: 42 synthetic, 12 ROM/payload, 12 room/composition,
  seven unchanged Mesen tests, and 1,190 bounds cases with zero mismatches
  or empty traces. The audit-runner CTest contract passed.
- Native `yaze`, `z3ed`, unit and integration targets build. Canonical
  vanilla, Oracle base and Oracle patched ROM SHA-256 values are unchanged.
  Fresh base-ROM `rom-doctor` still reports zero critical/errors and two
  heuristic warnings; no repair was performed.

Reproduce using the fourth slice's environment and `wave4_filter`:

```sh
cmake --build build/presets/mac-ai --config Release --target yaze_test_unit yaze_test_integration yaze z3ed --parallel 4
wave5_filter="RoomLayerManagerTest.*:$wave4_filter"
build/presets/mac-ai/bin/yaze_test_unit --gtest_list_tests --gtest_filter="$wave5_filter"
build/presets/mac-ai/bin/yaze_test_unit --gtest_filter="$wave5_filter" --gtest_output=xml:/tmp/yaze-wave5-broad.xml
build/presets/mac-ai/bin/yaze_test_integration --gtest_filter='Dungeon*RoomRenderParityTest.*HeadlessPngMatchesRoomComposite' --gtest_output=xml:/tmp/yaze-wave5-export-green.xml
scripts/agents/audit-dungeon-visual-parity.sh --build-dir build/presets/mac-ai --config Release --with-validate-report /tmp/yaze-wave5-validation.json
ctest --test-dir build/presets/mac-ai --output-on-failure -R '^DungeonVisualParityAuditContract$'
```

### Ice capture gap and next source-backed fixes

An isolated fresh-boot Mesen session reached Oracle room `0x0CE`, without a
savestate load or original-ROM writes. Runtime tilemap words match the D1
stamp, with `TM=$16`, `TS=$01`, `CGWSEL=$02`, `CGADSUB=$20`, mode 6.
**No new independent RGBA fixture was accepted:** moving Link onto the pit
started a fall/fade, and the captured animation phase was incomplete.
Diagnostic color correspondence is not a pixel-parity pass. The temporary
session was stopped; other emulator sessions were left alone.

For the next capture, Oracle relocates entrance 34's room word to
`$0F8068` (the legacy `$02C87B` recipe does not apply). Link Y/X words are
`$0F8E68` / `$0F9068`; local safe-floor `(80,80)` in room `0x0CE` maps to
world `(X=$1C50,Y=$1850)`. With camera `(X=$1C80,Y=$1870)`, the room ROI
`(232,184,64,64)` maps to screenshot `(104,71,64,64)`. Record stable room
and submode, current CGRAM, both bytes of animation offset `$7EC00F`, and
DMA source `$7E0ADC`; wait for frame-zero graphics to transfer. Its offset
cycles `0000/0400/0800`, so reading only the low byte cannot identify a frame.

Next code slice: Mushroom Grotto room `0x033` has two source-confirmed
geometry discrepancies, not a shared-palette defect:

- `0xA7` Stalfos at `(8,26)`: source frame zero at PC `0x6C0F3` uses
  16×16 CHR `00` at `(0,-10)` and CHR `06` at `(0,0)` (a duplicate body
  entry is intentional). The current CHR `0C` body at `y=12` is wrong.
  Keep OBJ palette 4; head-direction table is at PC `0x6C213`.
- `0x91` StalfosKnight at `(16,18)`: source body at PC `0xF2CEC` uses
  an 8×8 shoulder `64`, two 16×16 body tiles `61/62`, and mirrored 8×8
  feet `74` at `(-3,16)/(11,16)`. Head table PC `0xF2E46` selects front
  CHR `46` at `(0,-12)`. Keep OBJ palette 5. Cover head-first OAM overlap
  and asymmetric foot mirroring; a static pose does not establish initial
  runtime visibility because this enemy starts hidden.

Local evidence: `/tmp/yaze-wave5-mode7-red.xml`,
`/tmp/yaze-wave5-babasu-red.xml`, `/tmp/yaze-wave5-broad.xml`,
`/tmp/yaze-wave5-export-green.xml`, `/tmp/yaze-wave5-parity.log`, and
`/tmp/yaze-wave5-validation.json`. Capture diagnostics are under
`/tmp/yaze-wave5-ice.l5QyYg`; these are temporary artifacts, not release
fixtures. Remaining door/custom-object/runtime proof and platform packages
remain open.

## Sixth implementation slice: UI review and Stalfos previews (2026-09-14)

Verified code at `9ae78a739`, based on preview `55c086538`. The UI integration
checkpoint is `5a5c29290`: a three-way application of the user-confirmed
Grokbot batch, preserving the preview's newer Oracle/runtime/persistence work.
The canonical checkout was not edited. Later Grokbot edits are not part of this
snapshot. This is preview consolidation, not a mainline merge or release.

Four source-confirmed UI regressions were fixed:

1. Drawer tabs used the cursor's next-line position as the title width and
   overlapped header controls. Measure the title rectangle and reserve controls.
2. Sidebar filtering left matching tools inside persisted collapsed groups.
   Render filtered rows directly without changing normal group state.
3. Workbench's broad `dungeon.room_` prefix check hid Room Graphics and Room
   Tags. Recognize only nonempty decimal room-window suffixes.
4. Window Finder toggled an already-open window closed. Reuse the existing
   open/focus path, retain the registered session, and update recent windows.

The seven regression cases failed before those fixes and pass afterward.
Additional tests activate the real empty-state button and press/hold/release
status chips, covering cleared and replaced callbacks. They do not establish
end-to-end multi-session `SaveRom()` correctness.

Mushroom Grotto room `0x033` static sprite previews now follow the source poses
documented in the fifth slice: `0xA7` uses body CHR `06` at `(0,0)` and head at
`(0,-10)`; `0x91` uses the source shoulder/body pieces, CHR `74` mirrored feet,
and head-first OAM overlap. Their OBJ palettes remain 4 and 5. Independent
first-opaque OAM expectations cover overlap, asymmetric feet, transparency,
edge placement, and unchanged sprite metadata. Both pixel regressions failed
before the fix; the third test checks the Expanded-ROM source tables. These
are static poses, not proof of every animation or initial hidden state.

Verification, zero failures/skips after explicit asset setup:

- **53/53 UI and sprite-preview tests** and **730/730 dungeon/Oracle tests**;
  each filter pattern matched discovered tests, and XML execution matched the
  inventories. These sets overlap and must not be added as unique coverage.
- **57/57 PNG/composite comparisons** within two integration tests, covering
  19 rooms at three scales.
- Maintained ladder: Tier 1 **42**, Tier 2 **11 + 1 table check**, Tier 3
  **12**, Tier 4 **7**; Tier 5 **1,190 cases, zero mismatches/empty traces**.
  The audit-script contract also passes. No reference image was refreshed.
- Native `yaze`, `z3ed`, unit and integration targets build. An isolated Mac
  service instance opened an Oracle ROM copy, passed Ping/ListTests/idle,
  produced an inspected screenshot, and shut down with process/listener gone.
  Original Vanilla, Oracle base/patched, and working-copy hashes are unchanged.

Reproduce using the fourth slice's environment and `wave4_filter`, plus
`YAZE_TEST_ORACLE_CUSTOM_OBJECTS=/path/to/oracle/Dungeons/Objects/Data`:

```sh
ui_filter='EmptyStateTest.*:StatusBarContextTest.*:StatusBarContextClickTest.*:WindowSidebarTest.*:WindowSidebarFrameTest.*:RightDrawerManagerTest.*:CommandPalette*.*:ShortcutConfiguratorTest.*:SpriteRenderPreviewTest.*:SpritePreviewResourceCacheTest.*'
wave6_filter="RoomLayerManagerTest.*:$wave4_filter:EditorManagerOracleRomSafetyTest.*:DungeonOraclePreflightTest.*:DungeonObjectSelectorPaletteTest.*:DungeonEditorPaletteRefreshTest.*:CustomObjectManagerTest.*:CustomObjectCodecTest.*:CustomObjectRuntimeTileWordTest.*:OracleRuntimeAssets/CustomObjectOracleAssetTest.*:DungeonSaveTest.*:LayoutManagerPersistenceTest.*"
cmake --build build/presets/mac-ai --config Release --target yaze_test_unit yaze_test_integration yaze z3ed --parallel 4
build/presets/mac-ai/bin/yaze_test_unit --gtest_list_tests --gtest_filter="$ui_filter"
build/presets/mac-ai/bin/yaze_test_unit --gtest_filter="$ui_filter" --gtest_output=xml:/tmp/yaze-wave6-combined.xml
build/presets/mac-ai/bin/yaze_test_unit --gtest_list_tests --gtest_filter="$wave6_filter"
build/presets/mac-ai/bin/yaze_test_unit --gtest_filter="$wave6_filter" --gtest_output=xml:/tmp/yaze-wave6-broad.xml
build/presets/mac-ai/bin/yaze_test_integration --gtest_filter='Dungeon*RoomRenderParityTest.*HeadlessPngMatchesRoomComposite' --gtest_output=xml:/tmp/yaze-wave6-export.xml
scripts/agents/audit-dungeon-visual-parity.sh --build-dir build/presets/mac-ai --config Release --with-validate-report /tmp/yaze-wave6-validation.json
ctest --test-dir build/presets/mac-ai --output-on-failure -R '^DungeonVisualParityAuditContract$'
```

Temporary evidence uses `/tmp/yaze-wave6-*`. Native artifacts and the immutable
UI patch are under `/tmp/yaze-wave6-ui.yDE0C9`; the captured tracked patch SHA-256
is `2ed7f2e830528d2f417978b2328eaff9478c776948f9e5b62e8dec94a046d154`.
These are local diagnostics, not committed emulator reference fixtures.

Native verification limits: the window-key Workbench-tab click failed in the
harness, so the screenshot only confirms the active Object Selector surface.
The harness also wrote BMP bytes for a PNG request; an explicit BMP capture
was losslessly decoded for inspection. Both harness issues need separate
follow-up. No installed app was replaced, and Windows/Linux/WASM packages,
full UI acceptance, independent sprite RGBA captures, and remaining object
families are still open release gates.

## Object coverage checklist

Start by enumerating the supported IDs from `DrawRoutineRegistry` and the room
codec, not from a hand-maintained count. Expand each family below into the
existing audit report: object ID/subtype, routine and ASM label, legal size and
state, source payload, anchor/footprint, tile order/flips/palette, stream/BG
target, room witness, and evidence links. Keep one generated inventory and
extend the existing report schema if necessary; do not create another registry.

Use these states: **untriaged**, **reproduced**, **fixed-awaiting-proof**,
**verified**, or **intentional-preview-limit**. A passing sibling object does
not close every ID. A reported symptom is not proof of a current renderer bug.

| Family / starting IDs | Existing evidence to reuse | Remaining release work |
| --- | --- | --- |
| Thin/carpet/rug strips: `0x34`, `0x71`, `0xB3/0xB4`, `0x8D/0x8E`; wider `0x33/0x70/0xB2` | Origin/count tests, compatible-corner and conditional-cap replay; a `0x33` ROM parser sample | Real-room anchors, repeat counts and cap overlap. Cover size 0, 1, 15 and boundary placement before broadening all legal sizes. |
| Bars: `0x4C`, `0x8F`, `0xFD6–0xFD9` | Payload and vertical-row trace tests | Complete horizontal/vertical/corner joins. Start with Oracle room `0x042`. |
| Static water: `0x3F–0x46`, `0x79/0x7A`, `0xC8/0xC9/0xD9/0xE7`; waterfalls `0x47/0x48` | Shared floor replay, cap and waterfall traces, reveal-mask coverage | ROM payloads, palette, edge/interior ordering and layer-isolated captures. Oracle witnesses: Mushroom Grotto `0x04A`, then `0x033`. |
| Ice and moving-floor stamps: `0xD1/0xD2`, `0xCA`, `0xE3–0xE6` | Shared routine 58, packed two-bit width/height geometry | Per-ID room payload/context and independent static or state-labeled capture. Ice witnesses: Oracle `0x08C@(10,11)` BG1 and `0x0CE@(29,23)` BG2. Moving walls `0xCD/0xCE` are a different family. |
| Flood controls: `0xD8/0xDA` | ROM parsing and `Room076WaterOverlayWritesBg2ObjectBuffer` | Verify the editor indicator structurally; audit vanilla state branches separately. Do not require the indicator to equal an in-game HDMA water image. |
| Stairs: `0x12D–0x133`, `0xF9B–0xFA1`, `0xFA6–0xFA9`, `0xFB3`, `0x138–0x13B`, `0x12A/0x135/0x136` | Mixed-layer/priority replay and room `0x077` spiral-stair ROM traces | Independent lower-level stair + wall + door visibility, including stored-placement versus BothBG behavior. |
| Corners/diagonals: `0x100–0x117`, diagonal registry families 5/6/17/18/75–78 | ROM corner payloads, BothBG placement, column-major and diagonal geometry tests | Independent corner/seam scenes and boundary anchors. `0xA4` is BigHole, not a diagonal. |
| Doors and room composition | Focused door rules, room `0x001` guidance-wall/door test, one west NormalDoorLower Mesen ROI | Key/shutter/bombable/exploding family evidence; candidates `0x024/0x0B2/0x0BC/0x0C1/0x0C2`. Protect room `0x001` upper/lower overlap and lower stair. |
| Sprite preview palettes | Synthetic aux/CGRAM/transparency tests | Mushroom Grotto/ice witnesses using actual runtime sprite graphics and CGRAM. Separate generic palettes from custom external graphics. |

Witness rooms are investigation starting points, not newly verified matches.
Use the canonical vanilla ROM control and a recorded Oracle ROM digest. Do not
substitute a different hack's object payload or room header without labeling it.

### Evidence required to close a family

1. Confirm every mapped ID and the shared rule against current disassembly.
   Add targeted regressions for size, anchor, tile attributes, and state. An
   audit may correctly conclude **no renderer defect found**.
2. Exercise real room parsing and composition, including the original symptom
   when known. Keep placement/hit bounds, tile drawing, and final composition
   as separate checks.
3. Independently check representative scenes/states with Mesen. Record ROM
   hash, room, position, stream, camera, active BGs, runtime state and crop.
   Record which IDs/states the scene does not cover.
4. Keep fingerprint drift guards and `z3ed` bounds results separate from pixel
   truth. Missing ROM, missing suite, zero executed cases, and skipped captures
   are **not run**, not passes. Never update an unexplained golden to green.
5. Close known in-scope render/save defects. A real unsupported animation or
   HDMA preview limit needs a named contract; deferring a release requirement
   needs an explicit scope decision, not relabeling a bug.

## Agent work packets

These are role-based handoffs suitable for Codex, Cursor, or another agent.
The first implementation slices are recorded above; unlisted acceptance steps
remain assignments to schedule, not completed work.
Use one clean `codex/` worktree per implementation packet and record its base
SHA. Read-only investigation may share the preview checkout.

| Packet / owner | Bounded first deliverable | Owned surface | Done when |
| --- | --- | --- | --- |
| R1 — `zelda3-hacking-expert` | Audit thin strips and bar joins; then shared water/ice/moving-floor stamps | `draw_routines/rightwards_routines.cc`, `downwards_routines.cc`; take `special_routines.cc` only for the later floor slice; nearest focused tests | Source-backed regression or explicit no-defect finding for each ID; minimal fixes with size/boundary tests; scene request handed to V1 |
| V1 — `snes-emulator-expert` | Independent stair/corner scene evidence, then water/ice and sprite palettes | Existing ROM scene tests, `test/fixtures/visual/dungeon/`, palette/preview tests | State-labeled scene comparisons; mismatch isolated to payload, palette, layer, geometry or runtime behavior; no renderer rewrite based solely on a screenshot |
| C1 — `imgui-frontend-engineer` with Oracle support | Existing custom-asset workflow, then one minecart route roundtrip | `custom_object.*`, selector/Object Tile Editor; later `minecart_track_source.*`, track panel and collision generator | Existing assets preserve encoding; source publish and ROM save are explicit; copied project survives rebuild and runtime verification |
| T1 — `test-infrastructure-expert` | Repair audit discovery, then reduce the existing guarded GUI qualification to one object edit | `audit-dungeon-visual-parity.sh`, `run-d6-gui-qualification.sh`, existing GUI fixtures and focused harness tests | Supported binary layouts resolve; suites execute; GUI Save ROM, quit/fresh process/reopen and independent disk readback agree; canonical data unchanged |
| Q1 — `backend-infra-engineer` with UI review | Final exact-candidate native package and strict WASM persistence checks | Existing release/nightly workflows, package smoke scripts and browser smoke test | Final packages pass loader/lifecycle checks and packaged GUI acceptance; browser download/reimport proof is separate from debug smoke |

The integration owner controls shared `room.cc`, `object_drawer.cc`,
`room_layer_manager.*`, render routing, registry/dimensions, and test/CMake
registration. Agents propose changes there or obtain an explicit ownership
handoff. In particular, floor and stair changes share `special_routines.cc`;
do not assign concurrent edits. Use existing tests first; add files only when
the split reduces complexity, and remove a replaced path in the same slice.

### C1: Oracle-specific boundaries

- Inventory the 21 existing custom slots: `0x31` has 16, `0x32` has three ice
  props, `0x54` has two boss bodies. Preserve sparse tile words, subtype order,
  geometry, flips, and palettes through decode/publish/reload.
- `0x31:13` (`wall_sword_house.bin`) is not the vanilla wall-table override
  system. Oracle still includes `Dungeons/house_walls.asm`, which patches
  corner/horizontal/side tile words. Yaze's exact-ID `0x100` mapping has a
  synthetic render test, not a proven replacement for those ASM patches.
  Keep ASM authoritative until a migration proves identical rebuilt output
  and a subsequent editor change surviving rebuild. Do not promise an
  editor-only override reaches the game. Replacing every wall patch is not
  necessary to validate the existing workflow.
- Minecart `Publish Tracks` writes manifest-owned ASM. `Save Project` does not
  persist route drafts. Collision application changes room state and needs
  **File > Save ROM**. Verify source readback, collision undo/redo and existing
  collision protection, then rebuild and test a known-working route in Mesen.
  Unfinished Oracle route content is not automatically a Yaze defect.
- Prove stale source/hash, invalid batch and failed saves preserve data.
  Work on copied source/project/base ROM only. `oos168x.sfc` is an emulator
  output, never the edit target.
- Generic sprite palette construction has Light World/green-mail fallbacks;
  it lacks entrance-world context. `0x54` also needs external boss graphics
  and runtime palette data. Isolate those limitations before changing a
  generic palette offset. V1 supplies captures; the integrator owns shared
  `room.cc` changes. Test room/project switches for stale preview caches.

### T1: reuse the existing persistence path

`run-d6-gui-qualification.sh` already isolates its process/settings, verifies
canonical hashes, uses stable GUI actions and the registered Save ROM shortcut,
checks disk bytes and backup, quits, and starts a fresh process for readback.
Generalize its guarded lifecycle; do not clone its large implementation.

Start with the object-only part of `test/fixtures/gui/d6_edit_and_save.json`:
room `0x0B8`, object index 128, Y `7 -> 8`. Requalify the expected object and
pinned input digest before running; that fixture is not portable to arbitrary
Oracle versions. Add a canonical vanilla profile next. Expand to size/stream,
undo/redo, doors, sprites, room metadata and supported block/pit changes.

The first T1 slice repaired the parity helper's old `bin/Debug`/`bin/test`
assumption. It now resolves configured single- and multi-config layouts,
requires nonempty discovery and matching executed cases, and rejects skipped
acceptance tests. Keep that runner contract required while extending coverage.

Direct calls to `Rom::SaveToFile` in serializer tests do not exercise editor
dirty state or save coordination. Release builds disable ImGui test hooks, so
test-enabled app automation also does not certify the unmodified release
artifact. Preserve both application-path automation and package acceptance.

### Q1: release and UX acceptance

- Reuse the hosted publish-disabled Release workflow on the final combined SHA.
  Earlier package results are a baseline, not final-candidate proof.
- Test copied-out macOS DMG app, Windows ZIP/NSIS, and Linux TGZ/DEB in real
  desktop sessions. Record loader/install lifecycle separately from GUI
  edit/save/quit/reopen acceptance.
- Reuse compact/wide and large-font layout tests. Verify picker resize,
  Pop out/restore, context menu, issue capture, and notifications without
  canvas movement. Keep further visual redesign out of the correctness pass.
- Add a strict WASM import -> edit -> download -> fresh-session reimport check.
  Test browser storage separately. Existing debug-smoke failure tolerance is
  not acceptable for a persistence assertion; WASM remains a labeled preview.
- Nightly GUI checks currently allow failures and assume an old binary path.
  Only promote a qualified, nonempty persistence test to a required gate.

## Scheduling and merge contract

1. **Wave 1:** R1 strips/bars, C1 asset/source inventory, and T1 audit-discovery
   plus one-object save/reopen. The root agent integrates evidence and docs.
2. **Wave 2:** R1 shared floors, V1 independent scenes, and C1 minecart workflow.
   Add palette fixes only after V1 isolates the cause; serialize shared files.
3. **Wave 3:** close confirmed defects, consolidate accepted UI/custom work,
   and run Q1 on the final combined candidate.

Use at most three implementation agents plus the integrator at once. Share
the default four-worker build budget on this Mac rather than launching three
four-worker builds together. Independent machines may test platform artifacts.

Each handoff must include exact base/head SHA, files changed, reproduction or
no-defect finding, commands, discovered/executed/skipped counts, ROM/package
digests, residual limits, and one next action. Keep patches narrow. The root
reviews shared rules and verifies terminal CI before merging; changed UI still
needs the relevant hands-on acceptance. No force pushes, broad cleanup, or
changes to the user's live ROM/app session.

## Verification entry points

Use the owning worktree's configured build and actual binary layout. Build the
named targets before discovery. The integration checkout now has unit,
integration, and ROM-dependent binaries. For its single-config Release layout:

```bash
cmake --build build/presets/mac-ai --config Release \
  --target yaze_test_unit yaze_test_integration yaze_test_rom_dependent --parallel 4
build/presets/mac-ai/bin/yaze_test_unit --gtest_list_tests
build/presets/mac-ai/bin/yaze_test_integration --gtest_list_tests
build/presets/mac-ai/bin/yaze_test_unit \
  --gtest_filter='DrawRoutineMappingTest.*:ObjectDrawerRegistryReplayTest.*:ObjectDrawerMaskPropagationTest.*:ObjectGeometryTest.*:RoomLayerManagerTest.*'
```

The second slice's combined object/editor regression command is:

```bash
object_filter='DungeonCanvasViewer*.*:DungeonEditorPaletteRefreshTest.*:DungeonObjectSelectorPaletteTest.*:DungeonSelectionSnapshotTest.*:InteractionCoordinatorTest.*:DungeonWorkbench*.*:TileObjectHandlerTest.*:RoomObjectEncodingTest.*:DungeonObjectEditorDirtyTest.*:ObjectDrawerRegistryReplayTest.*:DrawRoutineMappingTest.*'
build/presets/mac-ai/bin/yaze_test_unit --gtest_list_tests \
  --gtest_filter="$object_filter"
build/presets/mac-ai/bin/yaze_test_unit --gtest_filter="$object_filter" \
  --gtest_output=xml:/tmp/yaze-object-wave2-broad.xml
```

Set `YAZE_TEST_ROM_VANILLA` to an existing canonical control ROM for
`SupportedRomRoles/RoomObjectRomParityTest.*`,
`DungeonRoomRegressionFixturesTest.*`, and `DungeonRoomRenderParityTest.*`.
The ROM-dependent binary owns `DungeonObjectRomValidationTest`, not the
integration binary. Canonical vanilla SHA-1:
`6d4f10a8b10e10dbe624cb23cf03b88bb8252973`.

The maintained ladder is:

```bash
scripts/agents/audit-dungeon-visual-parity.sh \
  --build-dir build/presets/mac-ai \
  --config Release \
  --with-validate-report /tmp/yaze-dungeon-object-validation.json
```

The command requires `YAZE_TEST_ROM_VANILLA`. Evidence provenance and capture
instructions live in `test/fixtures/visual/dungeon/README.md`; do not duplicate
them in each assignment. First-slice results are recorded above; rerun on the
final candidate rather than treating earlier results as release acceptance.

For the opt-in Oracle asset contract, point the environment variable at the
existing source asset directory (the test writes only temporary copies):

```bash
YAZE_TEST_ORACLE_CUSTOM_OBJECTS=/path/to/oracle/Dungeons/Objects/Data \
  build/presets/mac-ai/bin/yaze_test_unit \
  --gtest_filter='CustomObjectManagerTest.*:CustomObjectCodecTest.*:CustomObjectRuntimeTileWordTest.*:OracleRuntimeAssets/CustomObjectOracleAssetTest.*' \
  --gtest_output=xml:/tmp/yaze-oracle-custom-assets.xml
```

## Earlier issue audit (historical status; revalidate before closing)

The entries below preserve the earlier post-agent audit. Their historical
completion labels do not replace the v0.8.0 criteria above. In particular,
expanding pushable blocks beyond vanilla capacity is not required if supported
editing is safe and overflow is rejected explicitly.

### High-priority cleanup

1. **Pushable block table repointing / expansion**
   - Status: scoped; loader/saver now guard the four bank-02 `LDA.l ...,X`
     operand slots via `ValidateBlocksLoaderPointerOperand` before
     dereferencing them. The expected operand shape is pinned to the US USDASM
     loader at bank_02 `#_02DAF9..#_02DB12` and the vanilla
     `SpecialUnderworldObjects_pushable_block` table at bank_04 `#_04F1DE` by
     `DungeonSaveRegionTest.BlocksLoaderPointerOperandsMatchUsdasmShape`. Unit
     coverage now pins the fixed-capacity boundary: exactly 128 entries writes
     all four 0x80-byte pages, while 129 entries fails before mutating ROM bytes
     or clearing dirty state. Full >128-entry expansion still needs a runtime/WRAM
     layout patch, because the vanilla loader copies four 0x80-byte pages into
     `$7EF940..$7EFB3F` and block drawing scans that fixed buffer before torch
     data.
   - Problem: `SaveAllBlocks` is room-aware, but still bounded by vanilla table capacity.
   - Done when: edited block sets can exceed vanilla capacity through a deliberate repoint/expansion path, with surrounding instruction operands protected.
   - Tests: grow a fixture ROM beyond vanilla capacity; assert pointer/count operands, data bytes, and nearby regions.

2. **Pit-damage membership editor UI**
   - Status: inspector controls implemented for fixed-capacity room replacement; view-model coverage guards replacement/victim defaults and fixed-capacity swaps; ImGui click automation for the Add/Replace buttons landed in PR #65.
   - Problem addressed: `PitDamageTable` could encode fixed-capacity
     `RoomsWithPitDamage`, but no panel toggled membership.
   - Done when: the dungeon editor exposes room membership, marks the table dirty, saves via `SaveAllPits(rom, table)`, and reloads the edited membership.
   - Tests: view-model/unit test for toggling; ROM-backed save/reload test; no-op save stays byte-identical.

3. **Independent golden room screenshot ROI**
   - Status: complete for a stable vanilla room `0x012` left-wall ROI. The
     committed Mesen2 PNG is compared against yaze headlessly with an exact
     3,072-pixel RGBA assertion; fixture provenance, coordinates, update steps,
     and ROM skip policy are documented beside the baseline. Two additional
     exact 32x32 Mesen2 baselines now pin room `0x065` subtype-3 `0xFC7`
     BombableFloor in intact and bombed states. Broader room floor/background
     palette parity remains open.
   - Problem: current fixture checksums guard yaze renderer drift, not correctness against emulator or screenshot truth.
   - Done when: one stable room ROI has a committed PNG/baseline, update procedure, and tolerance/skip rationale.
   - Tests: E2E or headless visual diff for room `0x001` or `0x016`.

### Medium-priority correctness tests

4. **Sparse pixel overlap assertion for object stream ordering**
   - Status: room `0x001` now has a hardcoded sparse object BG1/BG2
     overlap-pixel golden asserting BG1 high-priority palette index `33` wins
     over BG2 low-priority palette index `34` at `(45,120)`.
   - Problem: full-buffer checksums are broad; a targeted overlap pixel would make ordering failures easier to diagnose.
   - Done when: one vanilla or synthetic room asserts the expected top pixel from primary/BG2-overlay/BG1-overlay ordering.

5. **Rare subtype-3 parser parity**
   - Status: complete. `RoomObjectRomParityTest` compares PrisonCell,
     BigKeyLock, and BombableFloor parser payloads with raw ROM words and pins
     their stateful drawer traces. BombableFloor additionally has independent
     exact-RGBA Mesen baselines for both room `0x065` states.
   - Problem addressed: parser replay checks initially covered size but not raw
     byte/token parity or independent runtime pixels for these routines.
   - Done when: tests compare decoded object metadata against the ROM object table row or fixture bytes, not only `parsed.size()`.

6. **PitDamageTable validation policy**
   - Status: saves now reject duplicate and out-of-range room ids with explicit tests.
   - Problem: fixed-capacity saves reject count mismatch, but do not validate duplicate room ids or out-of-range ids.
   - Done when: either duplicates/out-of-range ids are deliberately allowed and documented, or rejected with explicit tests.

### Low-priority docs/UI polish

7. **Object selector symbology badges**
   - Problem: routine-family labels exist in data/docs, but UI badges are not wired.
   - Done when: selector rows show routine family / special draw behavior without replacing names.

8. **ROM-dependent test classification**
   - Status: `PitDamageTableTest` moved to the stable integration suite so the
     unit suite no longer carries this vanilla-ROM skip.
   - Problem: `PitDamageTableTest` lives in the unit suite while requiring a vanilla ROM; this matches some existing tests but can add unit-suite skips.
   - Done when: either split pure capacity/validation tests into unit and move ROM-backed table tests to integration, or document the exception next to the test list.

### Earlier targeted verification set

```bash
YAZE_TEST_ROM_VANILLA="$(pwd)/roms/alttp_vanilla.sfc" \
  ctest --test-dir build/presets/mac-ai \
  -R "DungeonRoomRegression|PitDamageTable|RoomObjectRomParity|DungeonRoomRenderParity" \
  --output-on-failure
```
