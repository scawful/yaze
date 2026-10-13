# Test-suite debloat, Phase 1 log (2026-09-27)

Status: ACTIVE
Owner: test-infrastructure-expert
Last Reviewed: 2026-09-28

This log records every test that Phase 1 of the test-suite debloat deleted,
registered, or fixed, with the category and the reason, so that a reviewer can
check each decision and restore anything that was wrong to remove. The work
was done on `356ca86b6`, rebased onto `origin/master` `403fa148f` (#262) and then
onto `claude/test-sharding` `adf50e498`;
every deleted case was checked to exist, unchanged, on the new base.

The plan is `test-debloat-plan-2026-09-27.md` (kept beside the worktrees, not in
the repository): §1.4 orphan files, §2(a) and Appendix A trivial tests, §2(d)
dead code, §2(f) flaky tests, §3 the protected set, §5 Phase 1.

## How to restore a deleted test

1. Find the commit that deleted it: `git log --oneline -S'<TestName>' -- <file>`.
2. Take the file as it was before that commit:
   `git show <commit>^:<file> > /tmp/old.cc`, then copy the `TEST(...)` block back.
3. For a deleted file, `git checkout <commit>^ -- <file>` and add its path back
   to the right list in `test/CMakeLists.txt`.

## Result

The series sits on `claude/test-sharding` (`adf50e498`), which registers
`stable` as fixture-grouped shards, so the number of ctest entries no longer
tracks the number of tests. Case counts below come from running
`yaze_test_unit` as one process. Windows uses the `ci-windows` preset that CI
uses for `stable` (no `YAZE_ENABLE_AGENT_CLI`, so the 76 deleted cases in
`test/unit/tools/*` and `api_handlers_test.cc` do not exist there). The
per-case rows were measured before the sharding rebase.

| Sharded base, Windows `ci-windows` | `adf50e498` | this series |
|---|---:|---:|
| `ctest -L stable` entries | 13 | 13 |
| `ctest -L stable` time at `-j29` | 11.7 s | 11.8 s |
| `yaze_test_unit` cases / fixtures (one process) | 4,780 / 503 | 4,747 / 503 |
| failures in the sharded run | 5 known Windows-only cases + `z3ed_self_test` Not Run | same |

In the one-process run both sides also fail
`DungeonCollisionJsonCommandsTest.WaterFillImportFailsWhenRequiredD4RoomHasNoCollisionData`
(an order dependence that the shards do not hit); it is not from this series.

| Per-case registration | Before | After |
|---|---:|---:|
| Windows `ci-windows`, `ctest -L stable` entries (`403fa148f`) | 5,157 | 5,096 |
| Windows: passed / failed / skipped | 4,801 / 5 / 352 | 4,748 / 5 / 344 (final tree; 3 of 4 runs) |
| Windows: ctest time at `-j29` | 51.7 s | 49.8-53.3 s |
| mac-ai `ctest -C Debug -L '^stable$' --show-only` (`356ca86b6`) | 5,509 | 5,372 |
| mac-ai `yaze_test_unit` / `yaze_test_integration` cases | 5,128 / 376 | 5,019 / 348 |
| mac-ai `ctest -L '^stable$' -j 4` wall, loaded machine | 1,508 s, 3 failures (`ToolDispatcherTest`, §4) | not rerun (full runs moved to the Windows box) |

The 5 Windows failures on both sides are the known Windows-only ones on
master (`FreshRomFixtureOutputTest`, `SpriteCatalogSessionTest.ProjectPathsRoundTrip*`,
`CutsceneCameraPanelTest.SavesToTheProjectShotsFile`,
`GraphicsSaveStoplossTest.RelocatedSheet*`, `SpriteCatalogTest.RejectsUnsafeBindingPaths`).
Before its fix, the newly registered write-conflict suite added a sixth
(§2). In 1 of the 4 final runs,
`EditorManagerProjectActionsTest.AutosavePersistsProjectOnlyWork` also failed
with "Failed to replace project file ... Access is denied." Its temp paths are
already unique and the test holds no handle, so it is a separate intermittent
Windows file-replace problem, not a temp-path race and not caused by this
series; it is left for its own fix. The 8 fewer skips are the deleted ROM-gated cases.

The counts move by −165 on mac-ai (167 deleted, 2 of them `#else`
placeholders that mac-ai does not compile) and +28 for the registered suite;
on Windows by −89 and +28. The plan predicted 5,465 → about 5,320 from a
smaller base (PR #260 before the census and overworld CLI merges).

Flaky-fix check (§4), mac-ai Debug on the final tree over `403fa148f`,
per-case ctest registration, `taskpolicy -b nice -n 10`:

- `ctest -R 'ToolDispatcherTest|CustomObjectManagerTest|LayoutManagerPersistenceTest' -j 4 --repeat until-fail:20`:
  51 cases (26 + 20 + 5) x 20 = 1,020 runs, 0 failures, 256 s. On
  `356ca86b6` a single `-j 4` stable pass failed 3 `ToolDispatcherTest` cases.
- `ctest -R 'ObjectTileEditorTest\.Capture|DungeonObjectValidateTest\.All|RomTest\.SaveTruncatesExistingFile|ProjectBundle|EditorManagerWriteConflictTest' -j 4 --repeat until-fail:10`:
  110 cases x 10 = 1,100 runs, 0 failures, 533 s.
- Windows, 4 stable runs of the final tree at `-j29`: every fixed suite passed
  in all 4 (`CustomObjectManagerTest` 20, `LayoutManagerPersistenceTest` 5,
  the capture/trace/save cases, 73 `ProjectBundle*`, 28 write-conflict cases).
  `ToolDispatcherTest` is not built there (no `YAZE_ENABLE_AGENT_CLI` in the
  Windows presets).

## 1. Orphan and dead test sources (plan §1.4, §2(d)): 17 files, 3,592 lines

None of these files was in any list in `test/CMakeLists.txt`, so no preset
compiled or ran them (the WASM presets set `YAZE_BUILD_TESTS=OFF`). Their
entries in `scripts/audit_test_registration.py` were removed with them.

| File | Lines | `TEST`s | Why it was dead |
|---|---:|---:|---|
| `test/browser_ai_test.cc` | 80 | 0 | `main()` probe under `#ifdef __EMSCRIPTEN__`; not gtest |
| `test/integration/wasm_message_queue_test.cc` | 150 | 7 | whole body under `#ifdef __EMSCRIPTEN__`; no build compiles tests for WASM |
| `test/platform/wasm_error_handler_test.cc` | 59 | 7 | Emscripten-only, and none of the 7 cases asserts anything |
| `test/unit/wasm_patch_export_test.cc` | 133 | 6 | the native branch only checks the stub returns empty or `kUnimplemented` |
| `test/integration/zelda3/dungeon_rendering_test.cc` | 363 | 9 | marked deprecated in Nov 2025; `integration/zelda3/dungeon_object_rendering_tests.cc` replaced it |
| `test/unit/zelda3/dungeon/object_rendering_test.cc` | 373 | 11 | deprecated the same way; the routine ids added to it in `c02c5f0e2` (0x49, 0x51, 0x55, 0xA0-0xA3) never compiled, and `DrawRoutineMappingTest.MapsDiagonalCeilingFamilies` plus the registry checks in `object_dimensions_test.cc` assert them |
| `test/integration/zelda3/dungeon_object_rendering_tests_new.cc` | 219 | 7 | includes `app/gfx/background_buffer.h` and `app/gfx/snes_palette.h`, which no longer exist; a copy of the registered suite |
| `test/e2e/dungeon_object_rendering_e2e_tests.cc` | 1,179 | 1 | the only case is `GTEST_SKIP() << "E2E tests need rewrite for DungeonEditorV2..."` |
| `test/integration/editor/editor_integration_test.{cc,h}` | 301 | 0 | legacy `Controller`-based fixture base with no tests; retired in favour of `yaze_test_gui` |
| `test/standalone/test_sdl3_audio_compile.cc` | 121 | 0 | manual `g++` compile probe |
| `test/test_conversation_minimal.cc` | 30 | 0 | manual `main()` probe |
| `test/test_editor.{cc,h}` | 187 | 0 | helper whose only include is commented out in `test/yaze_test.cc` |
| `test/yaze_test_ci.cc` | 47 | 0 | alternate `main()` that no target builds |
| `test/e2e/imgui_test_engine_demo.{cc,h}` | 350 | 0 | 7 `E2ETest_ImGui*` functions that nothing registers with `IM_REGISTER_TEST`; it was the only dead entry in `yaze_test_gui`'s list |

Compile check (`-fsyntax-only` with the `yaze_test_unit` flags, before deletion):
`dungeon_rendering_test.cc` and `object_rendering_test.cc` fail (no matching
constructor), `dungeon_object_rendering_tests_new.cc` fails (missing header),
`wasm_patch_export_test.cc` fails natively (20 errors in
`wasm_patch_export.h`), and `dungeon_object_rendering_e2e_tests.cc` fails
(`ImGuiTestContext` API drift, 20+ errors). `test_editor.cc`,
`test_conversation_minimal.cc`, `yaze_test_ci.cc` and
`editor_integration_test.cc` still compile but contain no tests.

Not deleted, although the plan's §2(d) proposes it:

- The `DungeonE2E_*` ImGui tests (`e2e/dungeon_e2e_tests.cc`,
  `dungeon_object_drawing_test.cc`, `dungeon_canvas_interaction_test.cc`,
  `dungeon_layer_rendering_test.cc`). They compile in `yaze_test_gui` and are the
  only ImGui drivers for the dungeon canvas. The plan replaces them with E2E-07;
  they should go when E2E-07 lands, not before.
- Moving `e2e/dungeon_visual_verification_test.cc`, `e2e/ai_multimodal_test.cc`
  and `integration/ai/ai_gui_controller_test.cc` to the experimental binary. That
  adds list lines and needs the GUI harness in `yaze_test_experimental`; it is
  left for the Phase 4 label work.
- `DungeonCanvasAssetRefreshTest.DISABLED_ProfileSpritePreviewFrames`: an opt-in
  profiler added with `c4dfeae5a` (sprite preview cache). It never runs by
  default, and moving it to the benchmark binary means moving its fixture and
  test peer too.

## 2. Registered instead of deleted (plan §1.3, §3)

| File | Tests | Change |
|---|---:|---|
| `test/unit/editor/editor_manager_write_conflict_test.cc` | 28 | Added to `STABLE_UNIT_TEST_SOURCES`. It was only in the quick editor list, whose binary no preset builds, so its ROM write-conflict, backup and save-safety cases (§3 protected set) ran nowhere. It is still maintained (`903376666`, `1677e0e7d`). Its first Windows run failed `SaveRomBlocksAndAllowsBypass` with "Failed to move temp ROM into place: Access is denied.": the test held an `std::ifstream` on the ROM open across `SaveRom()`. The test now closes it first. |
| `test/unit/gui/empty_state_test.cc` | 0 | The second copy of the line inside `STABLE_UNIT_TEST_SOURCES` was removed. CMake already de-duplicated the source, so the test count does not change. |

## 3. Trivial tests (plan §2(a), Appendix A, §5 Phase 1 step 2)

Each Appendix A entry was read. Before deleting, `git log -S'<name>' -- <file>`
and `git log --grep='<name>'` were checked for a bug-fix commit that added or
relied on the test. The only fix commit found, `142ed8880`, added
`PaletteJsonTest.JsonSupportDisabled`, whose whole body is `GTEST_SKIP()`; the
other hits were bulk snapshot imports ("hotfix7 snapshot") or commits that only
mention an API with the same name.

Totals: 167 cases deleted from `stable` (A1 64, A2 34, A3 1, A4 58, object
selection duplicates 10), 4 given the assertion they were missing, and 50 kept
(1 of the 34 A2 deletions kept its checks as namespace-scope `static_assert`s).
Two files lost every case and were deleted with their `test/CMakeLists.txt`
lines: `test/integration/zelda3/sprite_position_test.cc` (3 print-only cases)
and `test/integration/zelda3/dungeon_room_test.cc` (1 case, no assertion). Both
also needed a ROM, so they always skipped. Section banners left without tests
were removed with them.

### A1. Metadata getters (plan Appendix A1): 64 deleted

| Test | File | Reason |
|---|---|---|
| `MemoryAnalyzeToolTest.GetNameReturnsCorrectName` | `test/unit/tools/memory_inspector_tool_test.cc` | string constant getter |
| `MemoryAnalyzeToolTest.GetUsageContainsAddress` | `test/unit/tools/memory_inspector_tool_test.cc` | usage string substring |
| `MemoryAnalyzeToolTest.GetUsageContainsLength` | `test/unit/tools/memory_inspector_tool_test.cc` | usage string substring |
| `MemoryAnalyzeToolTest.GetDescriptionIsNotEmpty` | `test/unit/tools/memory_inspector_tool_test.cc` | description not empty |
| `MemoryAnalyzeToolTest.DoesNotRequireLabels` | `test/unit/tools/memory_inspector_tool_test.cc` | constant flag getter |
| `MemorySearchToolTest.GetNameReturnsCorrectName` | `test/unit/tools/memory_inspector_tool_test.cc` | string constant getter |
| `MemorySearchToolTest.GetUsageContainsPattern` | `test/unit/tools/memory_inspector_tool_test.cc` | usage string substring |
| `MemorySearchToolTest.GetUsageContainsStartEnd` | `test/unit/tools/memory_inspector_tool_test.cc` | usage string substring |
| `MemorySearchToolTest.GetDescriptionIsNotEmpty` | `test/unit/tools/memory_inspector_tool_test.cc` | description not empty |
| `MemorySearchToolTest.DoesNotRequireLabels` | `test/unit/tools/memory_inspector_tool_test.cc` | constant flag getter |
| `MemoryCompareToolTest.GetNameReturnsCorrectName` | `test/unit/tools/memory_inspector_tool_test.cc` | string constant getter |
| `MemoryCompareToolTest.GetUsageContainsAddress` | `test/unit/tools/memory_inspector_tool_test.cc` | usage string substring |
| `MemoryCompareToolTest.GetUsageContainsExpected` | `test/unit/tools/memory_inspector_tool_test.cc` | usage string substring |
| `MemoryCompareToolTest.GetDescriptionIsNotEmpty` | `test/unit/tools/memory_inspector_tool_test.cc` | description not empty |
| `MemoryCompareToolTest.DoesNotRequireLabels` | `test/unit/tools/memory_inspector_tool_test.cc` | constant flag getter |
| `MemoryCheckToolTest.GetNameReturnsCorrectName` | `test/unit/tools/memory_inspector_tool_test.cc` | string constant getter |
| `MemoryCheckToolTest.GetUsageContainsRegion` | `test/unit/tools/memory_inspector_tool_test.cc` | usage string substring |
| `MemoryCheckToolTest.GetDescriptionIsNotEmpty` | `test/unit/tools/memory_inspector_tool_test.cc` | description not empty |
| `MemoryCheckToolTest.DoesNotRequireLabels` | `test/unit/tools/memory_inspector_tool_test.cc` | constant flag getter |
| `MemoryRegionsToolTest.GetNameReturnsCorrectName` | `test/unit/tools/memory_inspector_tool_test.cc` | string constant getter |
| `MemoryRegionsToolTest.GetUsageContainsFilter` | `test/unit/tools/memory_inspector_tool_test.cc` | usage string substring |
| `MemoryRegionsToolTest.GetUsageContainsFormat` | `test/unit/tools/memory_inspector_tool_test.cc` | usage string substring |
| `MemoryRegionsToolTest.GetDescriptionIsNotEmpty` | `test/unit/tools/memory_inspector_tool_test.cc` | description not empty |
| `MemoryRegionsToolTest.DoesNotRequireLabels` | `test/unit/tools/memory_inspector_tool_test.cc` | constant flag getter |
| `ProjectToolsTest.ProjectStatusToolName` | `test/unit/tools/project_tool_test.cc` | string constant getter |
| `ProjectToolsTest.ProjectSnapshotToolName` | `test/unit/tools/project_tool_test.cc` | string constant getter |
| `ProjectToolsTest.ProjectRestoreToolName` | `test/unit/tools/project_tool_test.cc` | string constant getter |
| `ProjectToolsTest.ProjectExportToolName` | `test/unit/tools/project_tool_test.cc` | string constant getter |
| `ProjectToolsTest.ProjectImportToolName` | `test/unit/tools/project_tool_test.cc` | string constant getter |
| `ProjectToolsTest.ProjectDiffToolName` | `test/unit/tools/project_tool_test.cc` | string constant getter |
| `ProjectToolsTest.StatusToolUsageFormat` | `test/unit/tools/project_tool_test.cc` | usage string substring |
| `ProjectToolsTest.SnapshotToolUsageFormat` | `test/unit/tools/project_tool_test.cc` | usage string substring |
| `ProjectToolsTest.RestoreToolUsageFormat` | `test/unit/tools/project_tool_test.cc` | usage string substring |
| `ProjectToolsTest.ExportToolUsageFormat` | `test/unit/tools/project_tool_test.cc` | usage string substring |
| `ProjectToolsTest.ImportToolUsageFormat` | `test/unit/tools/project_tool_test.cc` | usage string substring |
| `ProjectToolsTest.DiffToolUsageFormat` | `test/unit/tools/project_tool_test.cc` | usage string substring |
| `BuildConfigureCommandHandlerTest.GetNameReturnsCorrectName` | `test/unit/tools/build_tool_test.cc` | string constant getter |
| `BuildConfigureCommandHandlerTest.GetUsageReturnsValidUsage` | `test/unit/tools/build_tool_test.cc` | usage string substring |
| `BuildCompileCommandHandlerTest.GetNameReturnsCorrectName` | `test/unit/tools/build_tool_test.cc` | string constant getter |
| `BuildCompileCommandHandlerTest.GetUsageReturnsValidUsage` | `test/unit/tools/build_tool_test.cc` | usage string substring |
| `BuildTestCommandHandlerTest.GetNameReturnsCorrectName` | `test/unit/tools/build_tool_test.cc` | string constant getter |
| `BuildTestCommandHandlerTest.GetUsageReturnsValidUsage` | `test/unit/tools/build_tool_test.cc` | usage string substring |
| `BuildStatusCommandHandlerTest.GetNameReturnsCorrectName` | `test/unit/tools/build_tool_test.cc` | string constant getter |
| `BuildStatusCommandHandlerTest.GetUsageReturnsValidUsage` | `test/unit/tools/build_tool_test.cc` | usage string substring |
| `CodeGenToolsTest.AsmHookToolName` | `test/unit/tools/code_gen_tool_test.cc` | string constant getter |
| `CodeGenToolsTest.FreespacePatchToolName` | `test/unit/tools/code_gen_tool_test.cc` | string constant getter |
| `CodeGenToolsTest.SpriteTemplateToolName` | `test/unit/tools/code_gen_tool_test.cc` | string constant getter |
| `CodeGenToolsTest.EventHandlerToolName` | `test/unit/tools/code_gen_tool_test.cc` | string constant getter |
| `CodeGenToolsTest.AsmHookToolUsageFormat` | `test/unit/tools/code_gen_tool_test.cc` | usage string substring |
| `CodeGenToolsTest.FreespacePatchToolUsageFormat` | `test/unit/tools/code_gen_tool_test.cc` | usage string substring |
| `CodeGenToolsTest.SpriteTemplateToolUsageFormat` | `test/unit/tools/code_gen_tool_test.cc` | usage string substring |
| `CodeGenToolsTest.EventHandlerToolUsageFormat` | `test/unit/tools/code_gen_tool_test.cc` | usage string substring |
| `FileSystemToolTest.ListToolGetNameReturnsCorrectName` | `test/unit/tools/filesystem_tool_test.cc` | string constant getter |
| `FileSystemToolTest.ListToolGetUsageContainsPath` | `test/unit/tools/filesystem_tool_test.cc` | usage string substring |
| `FileSystemToolTest.ReadToolGetNameReturnsCorrectName` | `test/unit/tools/filesystem_tool_test.cc` | string constant getter |
| `FileSystemToolTest.ReadToolGetUsageContainsPath` | `test/unit/tools/filesystem_tool_test.cc` | usage string substring |
| `FileSystemToolTest.ExistsToolGetNameReturnsCorrectName` | `test/unit/tools/filesystem_tool_test.cc` | string constant getter |
| `FileSystemToolTest.InfoToolGetNameReturnsCorrectName` | `test/unit/tools/filesystem_tool_test.cc` | string constant getter |
| `EditorPanelTest.IdentityMethods` | `test/unit/editor/panel_system_test.cc` | returns the mock's own constructor arguments |
| `EditorPanelTest.DefaultBehavior` | `test/unit/editor/panel_system_test.cc` | base-class member defaults |
| `ResourcePanelTest.GeneratedId` | `test/unit/editor/panel_system_test.cc` | same id format is checked by MultiplePanelTest.DifferentResourceTypes (kept) |
| `ResourcePanelTest.GeneratedDisplayName` | `test/unit/editor/panel_system_test.cc` | cosmetic default label |
| `PaletteCommandsTest.AnalyzeHasNoRequiredArgs` | `test/unit/cli/palette_commands_test.cc` | asserts only that Run fails without a ROM; the named validation is never isolated |
| `PaletteCommandsTest.HandlerNames` | `test/unit/cli/palette_commands_test.cc` | string constant getters |

### A2. No assertion (plan Appendix A2): 34 deleted

| Test | File | Reason |
|---|---|---|
| `InteractionCoordinatorTest.CanAccessDoorHandler` | `test/unit/editor/interaction_coordinator_test.cc` | accessor compiles; no assertion |
| `InteractionCoordinatorTest.CanAccessSpriteHandler` | `test/unit/editor/interaction_coordinator_test.cc` | accessor compiles; no assertion |
| `InteractionCoordinatorTest.CanAccessItemHandler` | `test/unit/editor/interaction_coordinator_test.cc` | accessor compiles; no assertion |
| `InteractionCoordinatorTest.CanAccessTileHandler` | `test/unit/editor/interaction_coordinator_test.cc` | accessor compiles; no assertion |
| `InteractionCoordinatorTest.DrawGhostPreviewsDoesNotCrashWithNoActiveHandler` | `test/unit/editor/interaction_coordinator_test.cc` | no assertion; draws outside an ImGui frame |
| `InteractionCoordinatorTest.DrawSelectionHighlightsDoesNotCrash` | `test/unit/editor/interaction_coordinator_test.cc` | no assertion; draws outside an ImGui frame |
| `MapRefreshCoordinatorTest.RefreshOverworldMapOnDemandNegativeIndexNoCrash` | `test/unit/editor/map_refresh_coordinator_test.cc` | no assertion; ForceRefreshGraphics bounds tests keep the bounds contract |
| `MapRefreshCoordinatorTest.RefreshOverworldMapOnDemandExcessiveIndexNoCrash` | `test/unit/editor/map_refresh_coordinator_test.cc` | no assertion; an OOB write would be UB, not a failure |
| `MapRefreshCoordinatorTest.RefreshOverworldMapDelegatesToOnDemand` | `test/unit/editor/map_refresh_coordinator_test.cc` | no assertion; one-line delegation |
| `MapRefreshCoordinatorTest.UpdateBlocksetNotLoadedReturnsImmediately` | `test/unit/editor/map_refresh_coordinator_test.cc` | no assertion; early return on a flag |
| `CanvasAutomationAPITest.ScrollToTile_ValidTile` | `test/unit/gui/canvas_automation_api_test.cc` | no assertion ("depends on ImGui state") |
| `CanvasAutomationAPITest.ScrollToTile_OutOfBounds` | `test/unit/gui/canvas_automation_api_test.cc` | no assertion |
| `CanvasAutomationAPITest.CenterOn_ValidTile` | `test/unit/gui/canvas_automation_api_test.cc` | reads scroll but never asserts it |
| `CanvasAutomationAPITest.CenterOn_OutOfBounds` | `test/unit/gui/canvas_automation_api_test.cc` | no assertion |
| `SpritePositionTest.SpriteCoordinateSystem` | `test/integration/zelda3/sprite_position_test.cc` | prints sprite fields; no assertion (file deleted) |
| `SpritePositionTest.SpriteFilteringLogic` | `test/integration/zelda3/sprite_position_test.cc` | prints a copy of the filter; no assertion (file deleted) |
| `SpritePositionTest.MapCoordinateCalculations` | `test/integration/zelda3/sprite_position_test.cc` | prints match/mismatch; no assertion (file deleted) |
| `ObjectDrawerTest.ChestStateHandling` | `test/unit/dungeon_object_drawer_test.cc` | body is only comments |
| `ObjectDrawerTest.ChestStateHandlingDirect` | `test/unit/dungeon_object_drawer_test.cc` | no assertion; chest open/closed draw is covered by ObjectDrawerRegistryReplayTest chest cases |
| `FileSystemToolTest.DotDotInPathBlocked` | `test/unit/tools/filesystem_tool_test.cc` | no assertion; PathTraversalBlocked and AbsolutePathTraversalBlocked assert the sandbox |
| `FileSystemToolTest.NegativeOffsetParameter` | `test/unit/tools/filesystem_tool_test.cc` | no assertion ("either fail or treat as 0") |
| `TileSelectorWidgetTest.AttachCanvas` | `test/unit/gui/tile_selector_widget_test.cc` | no assertion |
| `TileSelectorWidgetTest.ScrollToTile` | `test/unit/gui/tile_selector_widget_test.cc` | no assertion |
| `TileObjectHandlerTest.SetPreviewObject` | `test/unit/editor/tile_object_handler_test.cc` | no assertion |
| `PaletteJsonTest.JsonSupportDisabled` | `test/unit/palette_json_test.cc` | skip-only placeholder in the no-JSON #else branch |
| `ToolSchemaBuilderTest.RequiresAiRuntimeAndJson` | `test/unit/cli/tool_schema_builder_test.cc` | skip-only placeholder in the no-AI #else branch |
| `OverworldRegressionTest.VanillaRomUsesFetchLargeMaps` | `test/unit/zelda3/overworld_regression_test.cc` | body is only planning comments |
| `ObjectTileEditorTest.StandardWritePlansAreOpaqueAndBuilderOwned` | `test/unit/zelda3/dungeon/object_tile_editor_test.cc` | static_asserts moved to namespace scope (still enforced at compile time); runtime body was SUCCEED() |
| `Bpp3To8ConversionTest.OutputSizeIs64BytesPerTile` | `test/unit/zelda3/dungeon/bpp_conversion_test.cc` | SUCCEED() only; the other Bpp3To8 cases assert the output bytes |
| `EditorManagerTest.PublicAPISurface` | `test/unit/editor/editor_manager_test.cc` | no assertion; duplicate of the init in every other case |
| `InteractionDelegationTest.AllHandlersAccessible` | `test/integration/interaction_delegation_test.cc` | accessor compiles; no assertion |
| `PaletteManagerTest.ResetColorWithoutInitializationReturnsError` | `test/integration/palette_manager_test.cc` | no assertion ("depends on implementation") |
| `DungeonObjectRenderingTests.VariousObjectTypes` | `test/integration/zelda3/dungeon_object_rendering_tests.cc` | prints failures instead of asserting |
| `DungeonRoomTest.SingleRoomLoadOk` | `test/integration/zelda3/dungeon_room_test.cc` | loads room 0 with no assertion (file deleted) |

### A3. `*_NO_THROW` only (plan Appendix A3): 1 deleted

| Test | File | Reason |
|---|---|---|
| `CommandListHandlerTest.BodyIsValidJson` | `test/unit/cli/api_handlers_test.cc` | redundant: ResponseContainsCommandsArray and later cases json::parse the same body |

### A4. Initial or default state, reviewed one by one (plan Appendix A4): 58 deleted

| Test | File | Reason |
|---|---|---|
| `PaletteManagerTest.InitializationState` | `test/integration/palette_manager_test.cc` | null-ROM init; comment says it is order dependent |
| `PaletteManagerTest.HasNoUnsavedChangesInitially` | `test/integration/palette_manager_test.cc` | initial state |
| `PaletteManagerTest.UndoRedoInitialState` | `test/integration/palette_manager_test.cc` | initial state |
| `PaletteManagerTest.DiscardGroupWithoutInitializationIsNoOp` | `test/integration/palette_manager_test.cc` | no-op on uninitialized singleton |
| `PaletteManagerTest.DiscardAllWithoutInitializationIsNoOp` | `test/integration/palette_manager_test.cc` | no-op on uninitialized singleton |
| `PaletteManagerTest.IsGroupModifiedInitiallyFalse` | `test/integration/palette_manager_test.cc` | initial state |
| `PaletteManagerTest.IsPaletteModifiedInitiallyFalse` | `test/integration/palette_manager_test.cc` | initial state |
| `PaletteManagerTest.IsColorModifiedInitiallyFalse` | `test/integration/palette_manager_test.cc` | initial state |
| `BuildToolTest.DefaultConfigUsesCorrectBuildDirectory` | `test/unit/tools/build_tool_test.cc` | struct member default |
| `BuildToolTest.DefaultConfigUsesCorrectTimeout` | `test/unit/tools/build_tool_test.cc` | struct member default |
| `BuildToolTest.DefaultConfigEnablesCaptureOutput` | `test/unit/tools/build_tool_test.cc` | struct member default |
| `BuildToolTest.DefaultConfigUsesCorrectMaxOutputSize` | `test/unit/tools/build_tool_test.cc` | struct member default |
| `BuildToolTest.InitialBuildStatusNotRunning` | `test/unit/tools/build_tool_test.cc` | initial state |
| `BuildToolTest.GetLastResultInitiallyEmpty` | `test/unit/tools/build_tool_test.cc` | initial state |
| `SnesColorConversionTest.DefaultConstructor` | `test/unit/gfx/snes_palette_test.cc` | default-constructed black |
| `SnesPaletteTest.DefaultConstructor` | `test/unit/gfx/snes_palette_test.cc` | empty container |
| `SnesPaletteTest.VectorConstructor` | `test/unit/gfx/snes_palette_test.cc` | size() after construction |
| `EditFileHeaderTest.DefaultValuesAreCorrect` | `test/unit/tools/project_tool_test.cc` | member default equals constant |
| `ProjectSnapshotTest.DefaultConstruction` | `test/unit/tools/project_tool_test.cc` | empty members |
| `ProjectManagerTest.IsNotInitializedByDefault` | `test/unit/tools/project_tool_test.cc` | initial state |
| `ProjectManagerTest.ListSnapshotsEmptyInitially` | `test/unit/tools/project_tool_test.cc` | empty list after init |
| `RoomLayerManagerTest.DefaultVisibilityAllLayersVisible` | `test/unit/zelda3/dungeon/room_layer_manager_test.cc` | initial state |
| `RoomLayerManagerTest.DefaultBlendModeIsNormal` | `test/unit/zelda3/dungeon/room_layer_manager_test.cc` | initial state |
| `RoomLayerManagerTest.DefaultObjectsNotTranslucent` | `test/unit/zelda3/dungeon/room_layer_manager_test.cc` | initial state |
| `EditorPanelTest.RelationshipDefaults` | `test/unit/editor/panel_system_test.cc` | base-class member defaults |
| `PanelCategoryTest.EditorBoundDefault` | `test/unit/editor/panel_system_test.cc` | base-class member default |
| `ResourcePanelTest.AllowMultipleInstancesDefault` | `test/unit/editor/panel_system_test.cc` | base-class member default |
| `ResourcePanelLimitsTest.DefaultLimits` | `test/unit/editor/panel_system_test.cc` | constants |
| `DockNodeTest.DefaultIsEmptyLeaf` | `test/unit/editor/layout/dock_tree_test.cc` | struct member defaults |
| `DockTreeTest.DefaultHasEmptyLeafRoot` | `test/unit/editor/layout/dock_tree_test.cc` | struct member defaults |
| `DockTreeTest.NamedConstructorStoresName` | `test/unit/editor/layout/dock_tree_test.cc` | constructor stores its argument |
| `DockNodeIdTest.DefaultConstructedNodeHasInvalidId` | `test/unit/editor/layout/dock_tree_test.cc` | struct member default |
| `DiggableTilesTest.DefaultStateIsAllClear` | `test/unit/diggable_tiles_test.cc` | initial state |
| `DiggableTilesTest.GetAllDiggableTileIdsEmpty` | `test/unit/diggable_tiles_test.cc` | initial state |
| `TileSimilarityMatchTest.DefaultInitialization` | `test/unit/tools/visual_analysis_tool_test.cc` | aggregate = {} zero-init |
| `PaletteUsageStatsTest.DefaultInitialization` | `test/unit/tools/visual_analysis_tool_test.cc` | aggregate = {} zero-init |
| `TileUsageEntryTest.DefaultInitialization` | `test/unit/tools/visual_analysis_tool_test.cc` | aggregate = {} zero-init |
| `TileSelectorWidgetTest.Construction` | `test/unit/gui/tile_selector_widget_test.cc` | initial state |
| `TileSelectorWidgetTest.ConstructionWithConfig` | `test/unit/gui/tile_selector_widget_test.cc` | initial state |
| `TileSelectorWidgetTest.RangeFilterDefaultInactive` | `test/unit/gui/tile_selector_widget_test.cc` | initial state |
| `ObjectParserStructsTest.ObjectRoutineInfoDefaultConstructor` | `test/unit/zelda3/object_parser_structs_test.cc` | struct member defaults |
| `ObjectParserStructsTest.ObjectSubtypeInfoDefaultConstructor` | `test/unit/zelda3/object_parser_structs_test.cc` | struct member defaults |
| `ObjectParserStructsTest.ObjectSizeInfoDefaultConstructor` | `test/unit/zelda3/object_parser_structs_test.cc` | struct member defaults |
| `InteractionCoordinatorTest.InitializesInSelectMode` | `test/unit/editor/interaction_coordinator_test.cc` | initial state |
| `InteractionCoordinatorTest.InitiallyHasNoEntitySelection` | `test/unit/editor/interaction_coordinator_test.cc` | initial state |
| `InteractionCoordinatorTest.GetEntityAtPositionReturnsNulloptOnEmpty` | `test/unit/editor/interaction_coordinator_test.cc` | hit test on an empty room |
| `CodeGenerationResultTest.DefaultConstruction` | `test/unit/tools/code_gen_tool_test.cc` | empty members |
| `CodeGenToolBaseTest.GetAllTemplatesNotEmpty` | `test/unit/tools/code_gen_tool_test.cc` | non-empty table; the generate cases use the templates |
| `DungeonObjectSelectorPaletteTest.InitialInvalidationCountIsZero` | `test/unit/editor/dungeon_object_selector_palette_test.cc` | test-counter baseline |
| `DungeonEditorV2IntegrationTest.EditorInitialization` | `test/integration/dungeon_editor_v2_test.cc` | Initialize() with a ROM pointer set |
| `DungeonEditorV2IntegrationTest.ComponentsInitializedAfterLoad` | `test/integration/dungeon_editor_v2_test.cc` | Load()+Update() smoke; the other cases in the file Load too |
| `CanvasCoordinateSyncTest.HoverMousePos_InitialState` | `test/unit/gui/canvas_coordinate_sync_test.cc` | initial state (>= 0) |
| `EditorManagerTest.Initialization` | `test/unit/editor/editor_manager_test.cc` | EXPECT_TRUE(true) after init; every other case initializes |
| `ThemeStyleSnapshotTest.EntityMarkerDefaultsFollowTheDocumentedHues` | `test/unit/editor/theme_style_snapshot_test.cc` | colour constants |
| `MoreActionsRegistryTest.StartsEmpty` | `test/unit/editor/activity_bar_actions_registry_test.cc` | initial state |
| `UserSettingsNamedLayoutsTest.DefaultsAreEmpty` | `test/unit/editor/user_settings_named_layouts_test.cc` | initial state |
| `ThemePersistenceTest.DefaultIsEmpty` | `test/unit/editor/theme_persistence_test.cc` | initial state |
| `DungeonEditorSystemIntegrationTest.BasicInitialization` | `test/integration/zelda3/dungeon_editor_system_integration_test.cc` | constructor stores its arguments |

### Duplicates in `integration/object_selection_integration_test.cc` (plan §2(b), §2(c), §5 Phase 1 step 2): 10 deleted

| Test | File | Reason |
|---|---|---|
| `ObjectSelectionIntegrationTest.InitialStateHasNoSelection` | `test/integration/object_selection_integration_test.cc` | initial state (A4) |
| `ObjectSelectionIntegrationTest.SetSelectedObjectsUpdatesSelection` | `test/integration/object_selection_integration_test.cc` | subset of SetSelectedObjectsReplacesPreviousSelection |
| `ObjectSelectionIntegrationTest.ClearSelectionRemovesAllSelections` | `test/integration/object_selection_integration_test.cc` | covered by IsObjectSelectActiveWhenHasSelection and ObjectSelectionTest.ClearSelection |
| `ObjectSelectionIntegrationTest.IsObjectSelectedReturnsCorrectValue` | `test/integration/object_selection_integration_test.cc` | duplicate of the replace case |
| `ObjectSelectionIntegrationTest.MultipleSelectionChangesFireMultipleCallbacks` | `test/integration/object_selection_integration_test.cc` | weaker duplicate of SelectionCallbackFires |
| `ObjectSelectionIntegrationTest.GetSelectionCountReturnsCorrectCount` | `test/integration/object_selection_integration_test.cc` | count is asserted by every kept case |
| `ObjectSelectionIntegrationTest.SelectionPersistsAcrossRoomAccess` | `test/integration/object_selection_integration_test.cc` | reading room data cannot change selection |
| `ObjectSelectionIntegrationTest.OutOfBoundsIndicesAreAccepted` | `test/integration/object_selection_integration_test.cc` | freezes a lack of validation (plan 2b) |
| `ObjectSelectionIntegrationTest.EmptyVectorClearsSelection` | `test/integration/object_selection_integration_test.cc` | same effect as ClearSelection |
| `ObjectSelectionIntegrationTest.ClearSelectionIsIdempotent` | `test/integration/object_selection_integration_test.cc` | trivial |

### Assertion added instead of deleting (A2): 4 tests, plus 1 conversion

Each of these had a clear, useful intent but no check. The added assertion is
the one the test name or comment already described.

| Test | File | Added check |
|---|---|---|
| `InteractionCoordinatorTest.SetContextPropagatestoHandlers` | `test/unit/editor/interaction_coordinator_test.cc` | `PlaceObjectAt` succeeds through the fresh coordinator and room 42 gains one object (fails if the handler did not get the context) |
| `MapRefreshCoordinatorTest.RefreshOverworldMapOnDemandCurrentMapNotDeferred` | `test/unit/editor/map_refresh_coordinator_test.cc` | map 5 stays unmodified (only the deferred path marks it) |
| `MapRefreshCoordinatorTest.RefreshOverworldMapOnDemandSameWorldNotDeferred` | `test/unit/editor/map_refresh_coordinator_test.cc` | map 10 stays unmodified (only the deferred path marks it) |
| `TileObjectHandlerTest.HasValidContextReturnsFalseWithoutContext` | `test/unit/editor/tile_object_handler_test.cc` | `PlaceObjectAt` and `DeleteObjects` both return false without a context |
| `ObjectTileEditorTest.StandardWritePlansAreOpaqueAndBuilderOwned` | `test/unit/zelda3/dungeon/object_tile_editor_test.cc` | the two `static_assert`s moved to namespace scope, so the compiler still enforces them; the runtime entry (`SUCCEED()`) is gone. Counted as deleted in the A2 table. |

### Flagged by the plan but kept

| Test | Group | Why it stays |
|---|---|---|
| `GraphicsEditorWindowIds.PolyhedralPanelMatchesWrapperId` | A1 | Checks that two separate classes agree on one window id; a mismatch duplicates the window in the registry. |
| `LayoutPresetsTest.Tile16EditorHasUsableFirstUseSizeButIsOptional` | A1 | Added with the tile16 editor fix (`9fee4ae94`); pins a minimum first-use size, not a constant name. |
| `MultiplePanelTest.DifferentResourceTypes` | A1 | Checks the computed `{category}.{type}_{id}` id that saved layouts key on. `ResourcePanelTest.GeneratedId` (deleted) was a subset. |
| `ObjectCoveragePanelTest.UnplacedObjectKeepsParentWindowActive`, `NoSelectionKeepsParentWindowActive` | A2 | False positive: the `DrawDetailsAndCheckParent` helper asserts the ImGui window stack. |
| `DungeonRoomRegressionFixturesTest.ScanAllRoomsForFixtureCandidates`, `DiscoverFixtureFingerprints` | A2 | Env-gated recorders (`YAZE_SCAN_DUNGEON_ROOMS`, `YAZE_RECORD_DUNGEON_ROOM_FIXTURES`) that print the golden fingerprints; they are the tool for re-recording the regression fixtures. |
| `MesenSocketClientTest.SubscribeDispatchesFrameEvents` | A2 / skip placeholder | Its body was restored since the audit and now asserts the event payload. |
| `OracleValidationViewModelTest.ParseSmokeInvalidJsonReturnsError` | A2 | False positive: it asserts the status code. |
| `RightDrawerManagerTest.RenderFrameWithAllDrawersDoesNotCrash` | A3 | Renders every drawer type inside a real ImGui frame; a Begin/End mismatch or crash in any drawer fails it. That is a render smoke test, not an `absl::Status` no-throw check. |
| `RomTest.Uninitialized`, `RomTest.LoadFromFileEmpty` | A4 | `unit/rom/rom_test.cc` is in the §3 protected set; `LoadFromFileEmpty` also checks an error path. |
| `PaletteManagerTest.ResetPaletteWithoutInitializationFails`, `SaveGroupWithoutInitializationFails`, `SaveAllWithoutInitializationFails` | A4 | Fail-closed save guards in a protected file: saving before initialization must return `FailedPrecondition` instead of writing. |
| `ChestEditTest.AddsSmallAndBigChestsWithDefaultReward`, `OrdinaryObjectBecomingChestGetsDefaultContents` | A4 | Real planner logic (the plan's own sample called these meaningful). |
| `DungeonEntranceEditPolicyTest.SpawnSlotCannotMarkDefaultModelDirtyBeforeLoad`, `DedicatedSpawnSlotCannotMarkDefaultModelDirtyBeforeLoad` | A4 | ROM-safety guards: a default model must not become dirty before load. |
| `DungeonObjectSelectorPaletteTest.ObjectPreviewsDefaultOn` | A4 | Documented UX policy (plan sample: meaningful). |
| `ToolDispatcherUnitTest.RegistryInitializesBuiltinToolsOnDemand`, `DefaultPreferencesDisallowMutatingTools` | A4 | Lazy registry initialization, and the safety default that the agent cannot mutate the ROM unless allowed. |
| `LayoutManagerDockTreeTest.DungeonDefaultBuildsSingleDockLeaf`, `StartupReapplyNoopWhenLastAppliedEmpty` | A4 | Real layout behavior, not member defaults. |
| `RoomLayerManagerTest.DefaultDrawOrderBG2First` | A4 | Computes the draw order after `SetBG2OnTop(false)`. |
| `DiggableTilesTest.SetVanillaDefaultsClearsExisting` | A4 | Behavior: custom tiles are cleared and the vanilla set applied. |
| `BuildToolTest.ListAvailablePresetsNotEmpty`, `IsBuildDirectoryReadyInitiallyFalse` | A4 | Read `CMakePresets.json` and probe a missing directory; not member defaults. |
| `SnesColorConversionTest.RGBConstructor`, `SNESConstructor`, `SnesColorTest.ConstructFromSnesValue`, `ConstructFromSnesBlack` | A4 | Colour conversion (codec-adjacent); the plan merges these in Phase 2 instead. |
| `AsarWrapperTest.DoubleInitialization`, `AsmPatchTest.DefaultNameFromFilename`, `AIConfigUtilsTest.NormalizeOpenAiBaseUrlDefaultsWhenEmpty`, `EmptyStateTest.DrawEmptyStateNoopsWhenEmpty`, `ResizeHandlesTest.DefaultColorComesFromTheme`, `ResourceLabelsTest.ResolvesVanillaLabelsByDefault`, `OverworldAreaRenderTest.AnimatedFallbackUsesWorldDefaultSheet7`, `RoomCollisionTest.AttributeTableCombinesDefaultAndCustomTypes`, `SheetRolePaletteTableTest.UnclassifiedBindingIsEmpty`, `EmulatorRuntimePolicyTest.PreferStartupCategoryNeverDefaultsToEmulator`, `OracleValidationViewModelTest.BuildCliCommandReconstructsCorrectly`, `MenuShortcutLabelsTest.UnknownUnboundOrMissingManagerIsEmpty`, `CollisionSourcePairingTest.MissingJsonIsCreated`, `TileObjectHandlerTest.PasteEmptyClipboardReturnsEmpty`, `DockTreeJsonTest.MissingRootUsesDefaultEmptyLeaf`, `ExpandedBankTest.ReadExpandedTextDataEmpty`, `DungeonEditorIntegrationTest.DungeonEditorInitialization`, `WindowBackendFactoryTest.CreateGlfwFallsBackToDefault` | A4 | Each calls a function with logic (parsing, lookup, fallback, idempotence, save, codec edge case) rather than reading a freshly constructed member. The heuristic matched a word like "Default" or "Empty" in the name. |

## 4. Flaky tests fixed (plan §2(f))

With per-case registration (the default before the fixture-sharded
registration of `claude/test-sharding`, and still `YAZE_TEST_PER_CASE=ON`)
every case is its own process and ctest runs several at once, so a temp path
that nothing makes unique is shared by concurrent processes. Shards keep a
fixture's cases in one process, but other shards, parallel CI jobs and other
worktrees on the same machine still share `/tmp` and the app-data directory.
Every fix below routes the path through `yaze::test::UniqueTempPath` (`test/unique_temp_path.h`:
test name, steady-clock stamp and a counter). No test was skipped or marked
flaky.

| Test | Root cause | Fix |
|---|---|---|
| `ToolDispatcherTest.*` (`test/integration/agent/tool_dispatcher_test.cc`) | All cases used `current_path()/test_temp/yaze_dispatcher_test`. The baseline run failed 3 of them (`ToolPreferencesDisableDungeon`, `InvalidToolCallReturnsError`, `MessageToolResolves`) with `filesystem error: in remove_all: No such file or directory` thrown in `TearDown()`, because another case had already removed the shared directory. | Per-case directory name from `UniqueTempPath(...).filename()`, still under `current_path()/test_temp` because the filesystem tools only allow paths inside the project tree; `TearDown` uses the `error_code` overload of `remove_all`. |
| `CustomObjectManagerTest.*` (`test/unit/zelda3/custom_object_test.cc`) | The root was a bare steady-clock nonce, and `SetUp` ran `remove_all` on it before `ASSERT_TRUE(create_directories(...))`. The process-global `CustomObjectManager` was restored only in `TearDown`. The earlier logs show no failure of this suite; the plan lists it as flaky, so this fix is preventive. | `UniqueTempPath` root; no pre-emptive `remove_all`; a `ScopedCustomObjectManagerState` member snapshots the manager when the fixture is built and restores it when the fixture is destroyed, even after a fatal assertion. |
| `LayoutManagerPersistenceTest.*` (`test/unit/editor/layout_manager_persistence_test.cc`) | Every case used the project key `layout-manager-window-schema-test`, so all cases read and wrote one `layouts/projects/<key>.json` in the app-data directory, and each `SetUp`/`TearDown` removed it. Another agent saw `PrefersWindowsKeyWhenBothSchemasExist` fail on the Windows box, where all processes of a run share one `YAZE_APP_DATA_DIR`. | Per-case key from `UniqueTempPath(...).filename()`. |
| `ObjectTileEditorTest.CaptureVanillaWallCornersIgnoresConfiguredTrackMap`, `CaptureWallCornerWithCustomAssetFolderStaysRomBacked` (`test/unit/zelda3/dungeon/object_tile_editor_test.cc`) | Fixed `/tmp/yaze_test_wall_corner_capture[_no_map]` roots, removed with `remove_all` at the end; two worktrees running the suite at once delete each other's files. | `UniqueTempPath` roots. |
| `DungeonObjectValidateTest.AllSizesUsesOnlyFixedSubtypeLegalSizes`, `AllStatesTracksExpectedEmptyBranches` (`test/unit/cli/dungeon_object_validate_test.cc`) | Fixed trace file names in the system temp directory. | `UniqueTempPath(stem, ".json")`. |
| `RomTest.SaveTruncatesExistingFile` (`test/unit/rom/rom_test.cc`) | Wrote `test_temp_rom.sfc` in the working directory and never removed it. | The file's own `ScopedTempDirectory`, which removes it. The assertions are unchanged (protected file). |
| `ProjectBundlePackTest.*`, `ProjectBundleUnpackTest.*`, `ProjectBundleArchiveTest.*`, `ProjectBundleVerifyTest.*` (`test/unit/cli/project_bundle_{archive,verify}_test.cc`) | `ScopedTempDir` named the root with a hash of its own `this` pointer, which two processes can share. | `UniqueTempPath` root. |

Not changed here: `SettingsPanelTest.DisplayDensityKeepsClassicYazePaintedByColorsYaze`
(owned by the `claude/test-sharding` work), and the `rom_dependent` suite's
shared `/tmp/yaze_test_states` (the generation test writes states that the
preview test reads on purpose, and that binary is not built).
