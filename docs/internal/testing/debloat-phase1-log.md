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
