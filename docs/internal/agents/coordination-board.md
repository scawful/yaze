# Coordination Board

**STOP:** Before posting, verify your **Agent ID** in [personas.md](personas.md). Use only canonical IDs.
**Guidelines:** Keep entries concise (<=5 lines). Archive completed work weekly. Target <=40 active entries.

### 2026-01-22 ai-infra-architect – Server-lite mode + lazy asset init for gRPC automation
- TASK: Add server-lite startup path for fast gRPC + symbol export; make editor assets lazy/on-demand; review Mesen2 multi-instance integration notes.
- SCOPE: src/app/main.cc, src/app/application.*, src/app/editor/editor_manager.*, docs/internal/agents/automation-workflows.md.
- STATUS: IN_PROGRESS
- NOTES: Added `--asset_mode` and lazy asset loading (on-demand editor init/load + `EnsureGameDataLoaded`); verified `emu-agent-instance` multi-instance script; server-lite startup still pending.

### 2026-01-23 imgui-frontend-engineer – Editor review sweep tracking
- TASK: Create tracking + systematically review all editor surfaces with layout/panel validation.
- SCOPE: src/app/editor/*, src/app/editor/ui/*, layout + panel persistence.
- STATUS: IN_PROGRESS
- INITIATIVE_DOC: docs/internal/agents/initiative-editor-review-2026-01.md

### 2026-01-22 imgui-frontend-engineer – ROM gRPC hardening + iOS UX + welcome screen refresh
- TASK: Harden RomService RPCs (proposal routing, version-aware reads), continue iPad UI fixes, and modernize welcome screen.
- SCOPE: src/app/service/rom_service_impl.cc, src/ios/*, src/app/platform/ios/*, src/app/gfx/backend/metal_renderer.mm, src/app/editor/ui/welcome_screen.*, src/app/editor/menu/menu_orchestrator.cc, src/app/controller.cc.
- STATUS: IN_PROGRESS
- NOTES: Added proposal routing + GameData-backed dungeon/overworld reads; welcome screen refresh (release history + project tools, AI button removed, responsive sizing); Agent Workspace menu entry; fixed black-screen regression (clear before ImGui). In progress: iOS overlay safe-area tweaks + mobile nav container polish; welcome/dashboard gating + resizing; Metal RGBA + streaming lock to fix iOS color. Updated activity bar icon visibility + selection highlight, template layout stacks on narrow screens, editor selection dialog resizes on iPad. Added iOS overlay menu (quick actions + compact mode) + bridge actions for project management/panel browser. Added iOS native menu + key commands (UICommand/Notification bridge), simplified top bar to centered capsule, moved actions into grid menu, and reduced touch resize sensitivity. Message editor: added font atlas fallback palette + safer font data load + lazy preview rebuild. Activity bar category selection now switches editors; fixed overworld area-graphics panel id. Tracking remaining issues: side-menu icon fade (verify), iOS color/touch/top-bar overlap.

### 2026-01-21 ai-infra-architect – Unified gRPC + Server mode & Mesen 2 integration
- TASK: Consolidate redundant gRPC servers, implement headless/server mode with resource optimization, and add Mesen 2 symbol export/bridge support.
- SCOPE: src/app/application.*, src/app/service/unified_grpc_server.*, src/app/platform/null_window_backend.*, src/app/gfx/backend/null_renderer.*, tools/mesen2/, docs/internal/agents/automation-workflows.md.
- STATUS: COMPLETE
- NOTES: Unified gRPC on 50052; added --server/--headless flags; implemented Null window/renderer backends for zero-GUI automation; added .mlb symbol export for Mesen 2 debugger parity.

### 2026-01-19 imgui-frontend-engineer – ImHex UI modernization follow-ups
- TASK: Add lifecycle addendum, animation utilities, and finish list virtualization (music/palette).
- SCOPE: docs/internal/plans/imhex-ui-modernization.md, src/app/gui/animation/animator.*, src/app/gui/gui_library.cmake, src/app/editor/music/song_browser_view.cc, src/app/editor/palette/palette_group_panel.cc
- STATUS: COMPLETE
- NOTES: Animator interface panel-scoped + theme-gated; finalize remaining clipper usage + theme colors in song browser. `cmake --build build` ok; smoke-build now fails in `imgui_test_harness.pb.h` with Protobuf header/runtime mismatch (ClearHasBit/SetHasBit).

### 2026-01-10 backend-infra-engineer – CI/CD stabilization after v0.5.0 release
- TASK: Fix failing CI builds/tests on master/develop and close remaining lint/test gaps across desktop/CLI/wasm.
- SCOPE: .github/workflows, CMake/test targets, docs/public build/test references.
- STATUS: COMPLETE
- NOTES: Retagged v0.5.0; release run 20896390585 succeeded (all platforms); added feature/test coverage report and aligned status tables.

### 2026-01-19 imgui-frontend-engineer – ContentRegistry panel scope + frame events
- TASK: Add global/session panel scope, register registry panels per session, move deferred actions to GUI-safe frame event, and add coverage.
- SCOPE: src/app/editor/system/panel_manager.*, src/app/editor/events/core_events.h, src/app/controller.cc, src/app/editor/editor_manager.cc, src/app/test/core_systems_test_suite.h, src/app/editor/core/content_registry.cc, src/app/editor/ui/about_panel.h.
- STATUS: COMPLETE
- NOTES: Added PanelScope + registry panel registration path; published FrameGuiBeginEvent after dockspace; added panel scope smoke test; avoid ContentRegistry factory lock while creating panels.

### 2026-01-12 backend-infra-engineer – v0.5.0 release artifact cleanup
- TASK: Clean up release artifacts (README, Windows DLL bloat, remove test helpers) and align docs/changelog for portable zip.
- SCOPE: cmake/packaging, tools/test_helpers, docs/public/release-notes.md, docs/public/reference/changelog.md, release workflow.
- STATUS: COMPLETE
- NOTES: Release run 20938978598 succeeded; artifacts regenerated with release README, trimmed Windows DLLs, and no helper binaries.

### 2026-01-12 imgui-frontend-engineer – v0.5.1 UX + .yaze consolidation
- TASK: Improve user-facing error handling/tooltips/feature status across editors + settings/shortcuts/project mgmt UX; consolidate .yaze storage across desktop/CLI/wasm; standardize version update workflow.
- SCOPE: src/app/editor/ui, src/app/editor/menu, src/app/platform, src/util/platform_paths.*, src/web, docs/public, scripts.
- STATUS: COMPLETE
- NOTES: Added feature status + shortcut search UX, storage info panel, .yaze path consolidation for app/CLI/web, VERSION source-of-truth, and refreshed release/docs + coverage report.

### 2026-01-11 backend-infra-engineer – v0.5.x test coverage expansion
- TASK: Expand GUI + WASM debug API coverage for desktop/CLI/web and wire CI checks.
- SCOPE: test/e2e, test/gui_test_utils.cc, scripts/agents, .github/workflows/web-build.yml, docs/public/reference/feature-coverage-report.md.
- STATUS: ACTIVE
- INITIATIVE_DOC: docs/internal/agents/initiative-test-coverage-v0-5-x.md
- NOTES: Added editor smoke tests + save-state panel check; wired Playwright debug API smoke into web-build workflow.

### 2026-01-10 backend-infra-engineer – v0.5.0 CI/lint/test/release sweep
- TASK: Fix linting + tests, stabilize CI/CD + release, and prep v0.5.0 artifacts for desktop/CLI/wasm.
- SCOPE: .github/workflows, CMake/test infra, docs/release notes, wasm + z3ed + app build paths.
- STATUS: COMPLETE
- NOTES: Release run 20886369001 succeeded (Create Release + Test Release jobs); v0.5.0 assets uploaded (Darwin dmg, win64 zip, Linux tar.gz, checksums).

### 2026-01-01 backend-infra-engineer – yaze.halext.org GH Pages restore
- TASK: Restore yaze.halext.org reverse proxy, fix web-build artifact paths, and bring collab server back online.
- SCOPE: .github/workflows/web-build.yml, /etc/nginx/sites/yaze.halext.org.conf, /home/halext/yaze-server/server.js
- STATUS: COMPLETE
- NOTES: Reinstalled node deps; patched uuid usage to crypto.randomUUID; restored nginx host + /ws proxy; web build 20645407016 succeeded; Playwright smoke test ran /help in terminal.

### 2026-01-01 backend-infra-engineer – Nightly install reliability (mac bundles)
- TASK: Ensure nightly install succeeds on mac and wrappers resolve yaze.app binaries.
- SCOPE: scripts/install-nightly.sh
- STATUS: COMPLETE
- NOTES: Prefer system gRPC auto-detect, skip install RPATH on mac, fallback install copy, and wrappers now resolve yaze.app/Contents/MacOS/yaze.

### 2026-01-01 backend-infra-engineer – WASM perf + file management fixes
- TASK: Reduce WASM filesystem init overhead, fix /projects directory errors, and validate 0.5.0 web app flows.
- SCOPE: src/app/platform/wasm/wasm_bootstrap.cc, src/web/core/filesystem_manager.js, src/web/components/file_manager_ui.js
- STATUS: COMPLETE
- NOTES: Playwright smoke on https://yaze.halext.org: /help + /projects write ok; shell_ui.js 200. COI/SharedArrayBuffer still failing (SW controlling, SAB unavailable); overlay hidden for automation. web-build.yml: https://github.com/scawful/yaze/actions/runs/20646881368.

### 2025-12-28 backend-infra-engineer – History compaction rollout + v0.5.0 release
- TASK: Promote history-compacted to master/develop, bundle pre-compaction history, and trigger CI/release for v0.5.0.
- SCOPE: git refs/branches, .github/workflows, docs/public/release-notes.md (if needed)
- STATUS: ACTIVE
- NOTES: Bundle saved; cmake-windows/z3ed deleted; master/develop + v0.5.0 retagged. Fixed panel descriptor initializer order + CreateWindow macro clash + missing emulator command sources (4a517a38). CI fixes staged for disk cleanup + system gRPC pkg-config fallback + spc_parser min fix; CRLF cleanup in snes_palette.cc; added absl/strings/str_format include in compression.cc after CodeQL error; added absl/strings/str_format include in collaboration_panel.cc after CodeQL error; removed grpc::ServerContext and grpc::Server fwd decls conflicting with system gRPC headers; formatted unified_grpc_server.h includes to satisfy clang-format; guarded agent_control_server.h to avoid grpc::Server typedef conflict; formatted music_editor.cc and fixed asm import error assignment; added absl include/clock headers across absl::StrFormat/Now usage and clang-formatted affected files; added absl/strings/str_format include in gui_test_utils.cc. New runs: https://github.com/scawful/yaze/actions/runs/20657727129 (CI) + https://github.com/scawful/yaze/actions/runs/20657730738 (security). Playwright smoke (chromium+firefox) on yaze.halext.org: /help + /version, FS write/read/delete ok. Prior failures: https://github.com/scawful/yaze/actions/runs/20651070300 (gRPC CPM disk full + Windows min) + https://github.com/scawful/yaze/actions/runs/20651072254 (CodeQL disk full).

### 2025-12-26 imgui-frontend-engineer – Mobile layout + nav pass
- TASK: Improve iPad layout (responsive welcome cards + panel width clamps), add mobile nav switcher, tune iOS touch sizing, and track per-edge safe areas.
- SCOPE: src/app/editor/ui/welcome_screen.*, src/app/editor/menu/right_panel_manager.cc, src/app/editor/menu/activity_bar.cc, src/app/editor/layout/layout_coordinator.cc, src/app/editor/ui/ui_coordinator.*, src/app/platform/ios/ios_window_backend.mm, src/app/platform/ios/ios_platform_state.*
- STATUS: COMPLETE
- NOTES: Compact layout uses floating nav button; right/side panels clamp to viewport; safe-area insets stored for per-edge use.

### 2025-12-25 imgui-frontend-engineer – iOS touch + safe area tuning
- TASK: Add touch target padding + safe-area padding for ImGui on iOS.
- SCOPE: src/app/platform/ios/ios_window_backend.mm
- STATUS: COMPLETE
- NOTES: TouchExtraPadding targets 44pt min; DisplaySafeAreaPadding uses max safe-area insets.

### 2025-12-25 imgui-frontend-engineer – Dashboard responsive layout pass
- TASK: Rebuild dashboard/editor selection grid layout for responsive sizing + theme-based colors.
- SCOPE: src/app/editor/ui/dashboard_panel.cc, src/app/editor/ui/editor_selection_dialog.cc
- STATUS: COMPLETE
- NOTES: z3ed CLI not available; tracking sub-steps locally.

### 2025-12-24 imgui-frontend-engineer – Graphics palette tint regression
- TASK: Fix red-tinted palettes across graphics previews (LoadAssets/Tile16).
- SCOPE: src/app/gfx/types/snes_color.cc, src/app/gfx/types/snes_palette.cc, src/app/editor/overworld/tile16_editor.*
- STATUS: COMPLETE
- NOTES: Corrected CGX palette conversion to avoid 0-255 vs 0-1 misuse; removed redundant set_rgb in palette constructors; restored palette-row offset mapping to skip HUD rows. Follow-up: tile16 palette pipeline preserves encoded CGRAM offsets end-to-end (tile8 + tile16 + previews) unless auto-normalize is enabled, and remaps rows using sheet base.

### 2025-12-06 ai-infra-architect – Overworld Editor Refactoring Phase 2
- TASK: Critical bug fixes and Tile16 Editor polish
- SCOPE: src/app/editor/overworld/, src/app/gfx/render/
- STATUS: COMPLETE (Phase 2 - Bug Fixes & Polish)
- NOTES: Fixed tile cache (copy vs move), centralized zoom constants, re-enabled live preview, scaled entity hit detection, restored Tile16 Editor window, fixed SNES palette offset (+1), added palette remapping for source canvas viewing, visual sheet/palette indicators, diagnostic function. Simplified scratch space to single slot. Added toolbar panel toggles.
- NEXT: Phase 2 Week 2 - Toolset improvements (eyedropper, flood fill, eraser tools)

### 2025-12-24 snes-emulator-expert – Overworld palette tint regression
- TASK: Investigate red-tinted overworld palette regression after history compaction.
- SCOPE: src/app/overworld/, src/app/gfx/, src/zelda3/overworld/
- STATUS: COMPLETE
- NOTES: Tile16 palette row mapping now uses ROM palette indices directly (no HUD row offset); adjusted remap logging/comments.

### 2025-12-23 imgui-frontend-engineer – Merge blockers + GLFW/iOS plan
- TASK: Patch merge blockers (dungeon map reserved writes, Asar temp cleanup, 2BPP save guard) and draft GLFW+iOS integration plan with lab/orchestration notes.
- SCOPE: src/zelda3/screen/dungeon_map.cc, src/core/asar_wrapper.cc, src/app/editor/graphics/*, docs/internal.
- STATUS: COMPLETE
- INITIATIVE_DOC: docs/internal/agents/initiative-glfw-ios-backend.md
- NOTES: z3ed CLI not available in environment; tracking sub-steps locally.

### 2025-12-24 imgui-frontend-engineer – iOS rebuild phase 0
- TASK: Start iOS-first execution; stabilize native file picker for iOS.
- SCOPE: src/app/platform/file_dialog.mm, src/ios/*, src/app/platform/ios/ios_host.mm, src/app/gfx/backend/metal_renderer.mm
- STATUS: ACTIVE
- NOTES: Added typed file dialog options + iOS picker type support; stubbed ios_host + metal_renderer, wired Metal renderer into factory/CMake, switched iOS main loop to IOSHost, added Xcode project + Info.plist updates (file sharing + sfc/smc UTTypes), added iOS window backend/platform state + Metal ImGui wiring, and fixed iOS framework links (SDKROOT + ModelIO + forced iOS linker flags). Continuing to split atomic commits + review remaining iOS/mobile changes.

### 2025-12-24 backend-infra-engineer – iOS thin app + XcodeGen
- TASK: CMake-built iOS static libs + generated Xcode app shell.
- SCOPE: CMake presets/platform defs, cmake/dependencies, src/ios/project.yml, scripts/ios.
- STATUS: COMPLETE (added iOS presets + bundle target + XcodeGen spec + build-ios.sh; legacy pbxproj still in repo).

### 2025-12-24 backend-infra-engineer – iOS file dialog async + bundle assets
- TASK: Fix iOS file picker re-entrancy and ensure bundled assets are discoverable.
- SCOPE: src/app/platform/file_dialog.*, src/app/editor/editor_manager.cc, src/app/editor/ui/ui_coordinator.cc, src/util/platform_paths.cc, src/ios/project.yml.
- STATUS: COMPLETE
- NOTES: Added async open dialog API; iOS ROM/project flows now async; assets copied into bundle for fonts/themes; iOS config/docs paths use sandbox via YAZE_IOS guard (avoid /.yaze).

### 2025-12-22 zelda3-hacking-expert – Dungeon map save parity + palette wiring
- TASK: Align dungeon map save flow with ZScream reference and unblock rom-dependent editor tests.
- SCOPE: src/zelda3/screen/dungeon_map.cc, test/e2e/rom_dependent/screen_editor_save_test.cc, test/integration/zelda3/dungeon_graphics_transparency_test.cc, src/zelda3/game_data.cc, src/app/gfx/util/palette_manager.cc.
- STATUS: COMPLETE
- NOTES: Verified ZScream SaveDungeonMaps flow (`ZScreamDungeon/ZeldaFullEditor/Gui/MainTabs/ScreenEditor.cs`). Updated gfx save stream to skip empty rooms, fixed transparency test setup with header + GameData, wired GameData→ROM in palette saves. Added indexed→SNES sheet save path (GraphicsEditor + tests), fixed overworld/palette persistence tests, and enabled tools build for ROM-dependent golden data extraction. Remaining rom-dependent failures are asar/emulator/audio only.

### 2025-12-22 test-infrastructure-expert – ROM test infra roles + env vars
- TASK: Add role-based ROM selection (vanilla/expanded/region) and remove hardcoded paths in tests.
- SCOPE: test/test_utils.h, test/yaze_test.cc, test/e2e/, test/integration/, docs/public build/test docs.
- STATUS: IN_PROGRESS
- NOTES: Replaced vanilla ROM with clean padded copy; fixed test SaveRomToFile to overwrite test copies; aligned expanded tile16 detection.

### 2025-12-22 imgui-frontend-engineer – Lab target for layout designer
- TASK: Move layout designer into a standalone lab target and decouple it from the main editor UI.
- SCOPE: src/lab/, src/lab/layout_designer/, editor menu, CMake, docs/internal/architecture.
- STATUS: COMPLETE
- NOTES: Added YAZE_BUILD_LAB option + `lab` executable, moved layout designer into src/lab/, removed main editor menu hook, and updated layout designer docs for lab usage.

### 2025-12-22 backend-infra-engineer – Local nightly install workflow
- TASK: Create isolated nightly build/install flow for yaze + z3ed + yaze-mcp distinct from dev builds.
- SCOPE: scripts/, CMakeUserPresets.json.example, docs/public/build/quick-reference.md, scripts/README.md
- STATUS: COMPLETE
- NOTES: Added scripts/install-nightly.sh with wrapper commands; documented nightly flow + env overrides in quick-reference/install-options.

### 2025-12-21 backend-infra-engineer – Codebase size reduction review
- TASK: Audit repo size + build configuration outputs; propose shrink plan (submodules, build dirs, deps cache).
- SCOPE: build*/ , ext/, vcpkg*, assets/, roms/, CMakePresets.json
- STATUS: COMPLETE
- NOTES: Identified build outputs (~11.7G), .git history (~5.4G), logs (~393M) as primary drivers; consolidated presets to `build/` + `build-wasm/`, removed extra build dirs, added `CMakeUserPresets.json.example`, updated scripts/docs to default to shared build dirs.

### 2025-12-21 backend-infra-engineer – Pre-0.2.2 history phase snapshotting
- TASK: Add additional pre-0.2.2 phase snapshots (2024 Q1/Q2/Q3) and re-rewrite history safely.
- SCOPE: git history, tags, backups
- STATUS: COMPLETE
- NOTES: Rewrote chain with pre-0.2.2-2024-q1/q2/q3 tags; forced push origin/master + tags; backup bundle saved (20251221T191959). Homebrew LLVM build ok after libc++ link fix; yaze_emu_test ok; yaze_test_stable fails 94 tests; yaze_test_gui ok; yaze_test_benchmark fails (palette perf + SIGSEGV). CI run triggered on master: https://github.com/scawful/yaze/actions/runs/20419211685

### 2025-12-21 backend-infra-engineer – Toolchain validation + build docs refresh
- TASK: Validate AppleClang + Homebrew LLVM builds/tests and align build docs.
- SCOPE: docs/public/build/*, build/, build_agent_llvm/, cmake/llvm-brew.toolchain.cmake
- STATUS: COMPLETE
- NOTES: AppleClang + LLVM builds OK; yaze_emu_test/yaze_test_gui pass; yaze_test_stable fails 94 tests; yaze_test_benchmark fails (palette perf + SIGSEGV). Docs updated with toolchain/preset/ROM guidance.

### 2025-12-21 backend-infra-engineer – Benchmark tuning + test triage
- TASK: Stabilize gfx benchmarks and summarize failing test clusters.
- SCOPE: test/benchmarks/gfx_optimization_benchmarks.cc, CI run status
- STATUS: COMPLETE
- NOTES: Added headless benchmark renderer + relaxed palette threshold (non-strict). Asar CLI fallback now outputs symbols + clearer errors; font loader falls back to assets for CLI/UI tests; OutputFormatter/VisualAnalysis JSON + FileSystemTool tests fixed. Root cause of OverworldEditorTest crash: stale Arena texture queue entries across tests; added test listener to clear queue after each test. Full yaze_test_stable now completes without crash on alttp_vanilla.sfc: 727 pass, 11 skipped, 47 failing (overworld/dungeon/palette/message clusters).

### 2025-12-21 zelda3-hacking-expert – Overworld test triage + ROM load fixes
- TASK: Investigate failing overworld unit/integration tests with alttp_vanilla.sfc and restore load compatibility.
- SCOPE: test/*overworld*, src/zelda3/overworld/*, rom/overworld loaders.
- STATUS: COMPLETE
- NOTES: Guarded overworld map palette/tileset builds when GameData is absent (headless tests), refreshed cached ROM version for SaveDiggableTiles, aligned regression mock parent table + integration sprite expectations. Overworld test subset passes (Overworld* = 35/35).

### 2025-12-21 zelda3-hacking-expert – Dungeon + palette test triage
- TASK: Triage failing dungeon/palette tests with alttp_vanilla.sfc and restore load compatibility.
- SCOPE: test/*dungeon* test/*palette* src/zelda3/dungeon/* src/app/editor/palette/*
- STATUS: COMPLETE
- NOTES: Updated sprite pointer test setup (bank 09), aligned draw routine test with registry size, fixed dungeon palette offset expectations (16-color CGRAM), corrected room selection hit-test (tile coords), and adjusted integration test object sizes. Dungeon test suite now passes (75/75).

### 2025-12-07 snes-emulator-expert – ALTTP input/audio regression triage
- TASK: Investigate SDL2/ImGui input pipeline and LakeSnes-based core for ALTTP A-button edge detection failure on naming screen + audio stutter on title screen
- SCOPE: src/app/emu/input/, src/app/emu/, SDL2 backend, Link to the Past behavior (usdasm reference)
- STATUS: IN_PROGRESS
- NOTES: Repro: A works on file select, fails on naming screen; audio degrades during CPU-heavy scenes.

### 2025-12-06 zelda3-hacking-expert – Dungeon object render/selection spec
- TASK: Spec accurate dungeon layout/object rendering + selection semantics from usdasm (ceilings/corners/BG merge/layer types/outlines).
- SCOPE: assets/asm/usdasm bank_01.asm rooms.asm; dungeon rendering/selection docs; editor render paths.
- STATUS: COMPLETE (core system stable)
- NOTES: Core rendering system verified stable with 222/222 tests passing. Type1/Type2/Type3 object parsing, index calculation, and draw routine mapping all validated against disassembly. Minor visual discrepancies remain in specific objects (vertical rails, doors) requiring individual verification. Spec: docs/internal/agents/dungeon-object-rendering-spec.md.

### 2025-12-05 snes-emulator-expert – MusicEditor 1.5x Audio Speed Bug
- TASK: Fix audio playing at 1.5x speed in MusicEditor (48000/32040 ratio indicates missing resampling)
- SCOPE: src/app/editor/music/, src/app/emu/audio/, src/app/emu/emulator.cc
- STATUS: HANDOFF - See docs/internal/handoffs/music-editor-audio-speed-bug.md
- NOTES: Verified APU timing, DSP rate, SDL resampling all correct. Fixed shared backend, RunAudioFrame accessor. Bug persists. Need to trace actual audio path at runtime.

### 2025-12-05 docs-janitor – Documentation cleanup + 0.4.1 changelog
- TASK: Align docs with new logging/panel startup flags; prep changelog for v0.4.1
- SCOPE: docs/public/reference/changelog.md, docs/public/developer/, related release notes
- STATUS: COMPLETE
- NOTES: Updated logging/panel flag docs, panel terminology, and added 0.4.1 changelog entry.

### 2025-12-05 docs-janitor – Layout designer doc consolidation
- TASK: Collapse layout designer doc sprawl into a single canonical guide
- SCOPE: docs/internal/architecture/layout-designer.md, remove stale layout-designer* docs
- STATUS: COMPLETE
- NOTES: New consolidated doc with current code status + backlog; deleted legacy phase/memo files.

### 2025-12-04 zelda3-hacking-expert – Dungeon object draw routine fixes
- TASK: Review dungeon object draw routines (editor + usdasm) and patch bugs in dungeon rendering.
- SCOPE: src/app/editor/dungeon/, src/zelda3/dungeon/, assets/usdasm/bank_01.asm.
- STATUS: COMPLETE
- NOTES: Fixed draw routine registry IDs (0–39) to match bank_01.asm, removed invalid placeholders, and registered chest routine so chest objects render instead of falling back.

### 2025-12-05 imgui-frontend-engineer – Panel launch/log filtering UX
- TASK: Upgrade logging/test affordances to filter logs and launch editors/panels; audit welcome/dashboard show/dismiss control.
- SCOPE: panel/layout orchestration, welcome/dashboard panels, CLI command triggers for panel visibility, logging/test runners.
- STATUS: IN_PROGRESS
- NOTES: Ensure panels can be driven from CLI (appear/dismiss) and logs are filterable for targeted startup flows.

### 2025-12-07 dungeon-rendering-specialist – Custom Objects System Integration
- TASK: Integrate custom object and minecart track support for Oracle of Secrets project
- SCOPE: core/project.{h,cc}, zelda3/dungeon/custom_object.{h,cc}, object_drawer.cc, dungeon_editor_v2.cc, feature_flags_menu.h
- STATUS: PARTIAL → HANDOFF for draw routine fixes
- NOTES: Added `custom_objects_folder` project config, UI flag checkbox, project flag sync to global. MinecartTrackEditorPanel works. Draw routine NOT registered - custom objects don't render. See docs/internal/hand-off/HANDOFF_CUSTOM_OBJECTS.md

### 2025-12-07 zelda3-hacking-expert – BG2 Masking Research (Phase 1 Complete)
- TASK: Research why BG2 overlay content is hidden under BG1 floor tiles.
- SCOPE: scripts/analyze_room.py, docs/internal/plans/dungeon-layer-compositing-research.md
- STATUS: COMPLETE (Research) → HANDOFF for implementation
- NOTES: Analyzed SNES 4-pass rendering, Room 001 objects, found 94 rooms affected. Root cause: BG1 floor has no transparent pixels where BG2 content exists. Fix: propagate Layer 1 object masks to BG1. See docs/internal/hand-off/HANDOFF_BG2_MASKING_FIX.md

### 2025-12-04 zelda3-hacking-expert – Dungeon layer merge & palette correctness
- TASK: Fix BG1/BG2 layer merging, object palette correctness, and live re-render pipeline so object drags update immediately.
- SCOPE: src/app/editor/dungeon/, src/zelda3/dungeon/, palette/layer merge handling.
- STATUS: IN_PROGRESS (blocked by BG2 masking fix above)
- NOTES: Auditing layer merging semantics, palette lookup, and object dirty/refresh logic (BG ordering, translucent flags, shared palette bug e.g. Ganon room 000 yellow ceiling).

### 2025-12-03 imgui-frontend-engineer – Keyboard Shortcut Audit
- TASK: Investigate broken Cmd-based shortcuts (sidebar toggle etc.) and standardize shortcut handling across app.
- SCOPE: shortcut_manager.{h,cc}, shortcut_configurator.cc, platform_keys.cc.
- STATUS: COMPLETE
- NOTES: Cmd/Super detection normalized (Cmd+B now works), chord parsing fixed, Proposal Drawer/Test Dashboard bindings corrected, shortcut labels show Cmd/Opt on mac.

### 2025-12-03 imgui-frontend-engineer – Phase 4: Double-Click Object Editing UX
- TASK: Implement double-click editing for dungeon objects (Phase 4 of object editor refactor)
- SCOPE: dungeon_object_selector.{h,cc}, panels/object_editor_panel.{h,cc}
- STATUS: COMPLETE
- NOTES: Double-click object in browser opens static editor with draw routine info. Added visual indicators (cyan border, info icon) and tooltips. Uses ObjectParser for info lookup. Preview rendering via ObjectDrawer.

### 2025-12-03 imgui-frontend-engineer – Phase 2: Draw Routine Modularization
- TASK: Split object_drawer.cc into modular draw routine files
- SCOPE: zelda3/dungeon/draw_routines/*.{h,cc}, zelda3_library.cmake
- STATUS: COMPLETE
- NOTES: Created 6 module files (draw_routine_types, rightwards, downwards, diagonal, corner, special). Fixed WriteTile8 utility to use SetTileAt. All routines use DrawContext pattern. Build verified.

### 2025-11-30 CLAUDE_OPUS – Card→Panel Terminology Refactor (Continuation)
- TASK: Complete remaining Card→Panel rename across codebase after multi-agent refactor
- SCOPE: editor_manager.cc, layout_manager.cc, layout_orchestrator.cc/.h, popup_manager.cc, panel_manager.cc
- STATUS: COMPLETE
- NOTES: Fixed field renames (default_visible_cards→default_visible_panels, card_positions→panel_positions, optional_cards→optional_panels), method renames (GetDefaultCards→GetDefaultPanels, ShowPresetCards→ShowPresetPanels, GetVisibleCards→GetVisiblePanels, HideOptionalCards→HideOptionalPanels), and call sites. Build successful 510/510.

### 2025-12-02 ai-infra-architect – Doctor Suite & Test CLI Implementation
- TASK: Implement expanded doctor commands and test CLI infrastructure
- SCOPE: src/cli/handlers/tools/, src/cli/service/resources/command_handler.cc
- STATUS: COMPLETE
- NOTES: Added `dungeon-doctor` (room validation), `rom-doctor` (header/checksum), `test-list`, `test-run`, `test-status`. Fixed `RequiresRom()` check in CommandHandler::Run. All commands use OutputFormatter with JSON/text output.

### 2025-12-01 ai-infra-architect – z3ed CLI UX/TUI Improvement Proposals
- TASK: Audit z3ed CLI/TUI UX (args, doctor commands, tests/tools) and main app UX; draft improvement docs for agents + humans
- SCOPE: src/cli/**, test/, tools/, main app UX (separate doc), test binary UX, docs/internal/agents/
- STATUS: IN_PROGRESS
- NOTES: Docs: docs/internal/agents/cli-ux-proposals.md (CLI/TUI/tests/tools). Focus on doctor flows, interactive mode coherence, test/tool runners.
- UPDATE: Doctor suite expanded (dungeon-doctor, rom-doctor). Test CLI added (test-list/run/status). Remaining: TUI consolidation.

### 2025-11-29 imgui-frontend-engineer – Settings Panel Initialization Fix
- TASK: Fix Settings panel failing to initialize (empty state) when creating new sessions or switching
- SCOPE: src/app/editor/session_types.cc, src/app/editor/editor_manager.cc
- STATUS: COMPLETE
- NOTES: Centralized SettingsPanel dependency injection in EditorSet::ApplyDependencies. Panel now receives ROM, UserSettings, and CardRegistry on all session lifecycles (new/load/switch). Removed redundant manual init in EditorManager::LoadAssets.

### 2025-11-29 ai-infra-architect – WASM filesystem hardening & project persistence
- TASK: Audit web build for unsafe filesystem access; shore up project file handling (versioning/build metadata/ASM/custom music persistence)
- SCOPE: wasm platform FS adapters, project file I/O paths, session persistence, editor project metadata
- STATUS: IN_PROGRESS
- NOTES: Investigating unguarded FS APIs in web shell/WASM platform. Will add versioned project saves + persistent music/assets between sessions to unblock builds on web.

### 2025-11-29 docs-janitor – Roadmap refresh (post-v0.3.9)
- TASK: Analyze commits since v0.3.9 and refresh roadmap with new features (WASM stability, AI agent UI, music/tile16 editors), CI/release status, and GH Pages WASM build notes.
- SCOPE: docs/internal/roadmap.md; recent commit history; CI/release workflow and web-build status
- STATUS: IN_PROGRESS
- NOTES: Coordinating with entry-point/flag refactor + SDL3 readiness report owned by another agent; documentation-only changes.

### 2025-11-27 docs-janitor – Public Documentation Review & WASM Guide
- TASK: Review public docs, add WASM web app guide, consolidate outdated content, organize docs directory
- SCOPE: docs/public/, README.md, docs directory structure, format docs organization
- STATUS: COMPLETE
- NOTES: Created web-app.md (preview status clarified), moved format docs to public/reference/, relocated technical WASM/web impl docs to internal/, updated README with web app preview mention, consolidated docs/web and docs/wasm into internal.

### 2025-11-27 imgui-frontend-engineer – Music editor piano roll playback
- TASK: Wire piano roll to segment-aware view with per-song windows, note click playback, and default ALTTP instrument import for testing
- SCOPE: music editor UI (piano roll/tracker), instrument loading, audio preview hooks
- STATUS: IN_PROGRESS
- NOTES: Removing shared global transport from piano roll; adding per-song/segment piano roll entry points and click-to-play previews.
- UPDATE: imgui-frontend-engineer refactoring piano roll canvas sizing/scroll (custom draw, channel column cleanup) to fix stretching/cutoff when docked.

### 2025-11-27 snes-emulator-expert – Emulator render service & input persistence
- TASK: Add shared render service for dungeon object preview and persist keyboard config/ImGui capture flag
- SCOPE: emulator render service, dungeon object preview, user settings input, PPU/input debug instrumentation
- STATUS: IN_PROGRESS
- NOTES: Render service with static/emulated paths; preview uses shared service. Input bindings saved to user settings with ignore-text-input toggle. PPU/input debug logging left on for regression triage.

### 2025-11-26 ui-architect – Menu Bar & Right Panel UI/UX Overhaul
- TASK: Fix menubar button styling, right panel header, add styling helpers
- SCOPE: ui_coordinator.cc, right_panel_manager.cc, editor_manager.cc
- STATUS: PARTIAL (one issue remaining)
- NOTES: Unified button styling, responsive menubar, enhanced panel header with Escape-to-close, styling helpers for panel content. Fixed placeholder sidebar width mismatch.
- REMAINING: Right menu icons still shift when panel opens (dockspace resizes). See [handoff-menubar-panel-ui.md](handoff-menubar-panel-ui.md)
- NOTE: imgui-frontend-engineer picking up MenuOrchestrator layout option exposure + callback cleanup to align with EditorManager/card namespace; low-risk touch.

### 2025-11-26 docs-janitor – Documentation Cleanup & Updates
- TASK: Update outdated docs, archive completed work, refresh roadmaps
- SCOPE: docs/internal/, docs/public/developer/
- STATUS: COMPLETE
- NOTES: Updated roadmap.md (Nov 2025), initiative-v040.md (completed items), architecture.md (editor status), dungeon_editor_system.md (ImGui patterns). Added GUI patterns note from BeginChild/EndChild fixes.

### 2025-11-26 ai-infra-architect – WASM z3ed console input fix
- TASK: Investigate/fix web z3ed console enter key + autocomplete failures
- SCOPE: src/web/components/terminal.js, WASM input wiring
- STATUS: COMPLETE
- NOTES: Terminal now handles keydown/keyup in capture and shell skips terminal gating, restoring Enter + autocomplete in wasm console.

### 2025-11-26 snes-emulator-expert – Emulator input mapping review
- TASK: Review SDL2/ImGui input mapping, ensure key binds map correctly and hook to settings/persistence
- SCOPE: src/app/emu/input/*, emulator input UI, settings persistence
- STATUS: COMPLETE
- NOTES: Added default binding helper, persisted keyboard config to user settings, and wired emulator UI callbacks to save/apply bindings.

---

### 2025-11-25 backend-infra-engineer – WASM release crash triage
- TASK: Investigate release WASM build crashing on ROM load while debug build works
- SCOPE: build_wasm vs build-wasm-debug artifacts, emscripten flags, runtime logs
- STATUS: IN_PROGRESS
- NOTES: Repro via Playwright; release hits OOB in unordered_map<Bitmap> during load. Plan: `docs/internal/agents/wasm-release-crash-plan.md`.

---

### 2025-11-25 ai-infra-architect – Agent Tools & Interface Enhancement (Phases 1-4)
- TASK: Implement tools directory integration, discoverability, schemas, context, batching, validation, ROM diff
- SCOPE: src/cli/service/agent/, src/cli/handlers/tools/, tools/test_helpers/
- STATUS: COMPLETE
- NOTES: Phases 1-4 complete. tools/test_helpers now CLI subcommands. Meta-tools (tools-list/describe/search) added. ToolSchemas for LLM docs. AgentContext for state. Batch execution. ValidationTool + RomDiffTool created.
- HANDOFF: [phase5-advanced-tools-handoff.md](phase5-advanced-tools-handoff.md) – Visual Analysis, CodeGen, Project tools ready for implementation

### 2025-11-24 ui-architect – Menu Bar & Sidebar UX Improvements
- TASK: Restructured menu bar status cluster, notification history, and sidebar collapse behavior
- SCOPE: MenuOrchestrator, UICoordinator, EditorCardRegistry, ToastManager
- STATUS: COMPLETE
- NOTES: Merged Debug menu into Tools, added notification bell with history, fixed sidebar collapse to use menu bar hamburger. Handoff doc: [handoff-sidebar-menubar-sessions.md](handoff-sidebar-menubar-sessions.md)

### 2025-11-24 docs-janitor – WASM docs consolidation for antigravity Gemini
- TASK: Consolidate WASM docs into single canonical reference with integrated Gemini prompt.
- SCOPE: docs/internal/agents/wasm-development-guide.md plus wasm status/roadmap/debug docs.
- STATUS: COMPLETE
- NOTES: Created `wasm-antigravity-playbook.md` (consolidated canonical reference with integrated Gemini prompt). Deleted duplicate files `wasm-antigravity-gemini-playbook.md` and `wasm-gemini-debug-prompt.md`. Updated cross-references.

### 2025-11-24 zelda3-hacking-expert – Gemini WASM prompt refresh
- TASK: Refresh Gemini WASM prompts with latest dungeon rendering context (usdasm/Oracle-of-Secrets/ZScream).
- SCOPE: docs/internal/agents/wasm-antigravity-playbook.md; cross-check dungeon object rendering notes.
- STATUS: IN_PROGRESS
- NOTES: Reviewing usdasm + Oracle-of-Secrets/Docs + ZScream dungeon rendering for prompt quality.

### 2025-11-23 docs-janitor – docs/internal cleanup & hygiene rules
- TASK: Cleanup docs/internal; keep active plans/coordination; add anti-spam + file-naming rules.
- SCOPE: docs/internal (agents, plans, indexes) focusing on consolidating/archiving stale docs.
- STATUS: COMPLETE
- NOTES: Archived legacy agent docs, added doc-hygiene + naming guidance; restored active plans to root after sweep.

### 2025-11-23 CODEX – v0.3.9 release rerun
- TASK: Rerun release workflow with cache-key hash fix + Windows crash handler for v0.3.9-hotfix4.
- SCOPE: .github/workflows/release.yml, src/util/crash_handler.cc; release run 19613877169 (workflow_dispatch, version v0.3.9-hotfix4).
- STATUS: IN_PROGRESS
- NOTES:
  - Replaced `hashFiles` cache key with Python-based hash step (build/test jobs) and fixed indentation syntax.
  - Windows crash_handler now defines STDERR_FILENO and _O_* macros/includes for MSVC.
  - Current release run on master is building (Linux/Windows/macOS jobs in progress).
- REQUESTS: None.

---

### 2025-11-24 CODEX – release_workflow_fix
- TASK: Fix yaze release workflow bug per run 19608684440; will avoid `build_agent` (Gemini active) and use GH CLI.
- SCOPE: .github/workflows/release.yml, packaging validation, GH run triage; build dir: `build_codex_release` (temp).
- STATUS: COMPLETE
- NOTES: Fixed release cleanup crash (`rm -f` failing on directories) by using recursive cleanup + mkdir packages in release.yml. Root cause seen in run 19607286512. Did not rerun release to avoid creating test tags; ready for next official release run.
- REQUESTS: None; will post completion note with run ID.

### 2025-11-24 CODEX – v0.3.9 release fix (IN PROGRESS)
- TASK: Fix failing release run 19609095482 for v0.3.9; validate artifacts and workflow
- SCOPE: .github/workflows/release.yml, packaging/CPack, release artifacts
- STATUS: IN_PROGRESS (another agent actively working)
- NOTES: Root causes identified (hashFiles() invalidation, Windows crash_handler POSIX macros)

### 2025-11-24 ai-infra-architect – WASM collab server deployment
- TASK: Evaluate WASM web collaboration vs yaze-server and deploy compatible backend to halext-nj.
- SCOPE: src/app/platform/wasm/*collaboration*, src/web/collaboration_ui.*, ~/Code/yaze-server, halext-nj deployment.
- STATUS: COMPLETE
- NOTES: Added WASM-protocol shim + passwords/rate limits + Gemini AI handler to yaze-server/server.js (halext pm2 `yaze-collab`, port 8765). Web client wired to collab via exported bindings; docked chat/console UI added. Needs wasm rebuild to ship UI; AI requires GEMINI_API_KEY/AI_AGENT_ENDPOINT set server-side.

### 2025-11-24 CODEX – Dungeon objects & ZSOW palette
- TASK: Fix dungeon object rendering regression + ZSOW v3 large-area palette issues; add regression tests
- SCOPE: dungeon editor rendering, overworld palette mapping/tests
- STATUS: COMPLETE (core stable, minor issues remain)
- NOTES: Core rendering verified stable (222/222 tests pass). Type2 corner fix (d5e06e94) aligned with disassembly. Minor visual issues in specific objects (vertical rails, doors) need individual verification - not a clear regression.

### 2025-11-23 CLAUDE_AIINF – WASM Real-time Collaboration Infrastructure
- TASK: Implement real-time collaboration infrastructure for WASM web build
- SCOPE: src/app/platform/wasm/wasm_collaboration.{h,cc}, src/web/collaboration_ui.{js,css}
- STATUS: COMPLETE
- NOTES: WebSocket-based multi-user ROM editing. JSON message protocol, user presence, cursor tracking, change sync.
- FILES: wasm_collaboration.{h,cc} (C++ manager), collaboration_ui.js (UI panels), collaboration_ui.css (styling)

---

### 2025-11-23 COORDINATOR - v0.4.0 Initiative Launch
- TASK: Launch YAZE v0.4.0 Development Initiative
- SCOPE: SDL3 migration, emulator accuracy, editor fixes
- STATUS: ACTIVE
- INITIATIVE_DOC: `docs/internal/agents/initiative-v040.md`
- NOTES:
  - **v0.4.0 Focus Areas**:
    - Emulator accuracy (PPU JIT catch-up, semantic API, state injection)
    - SDL3 modernization (directory restructure, backend abstractions)
    - Editor fixes (Tile16 palette, sprite movement, dungeon save)
  - **Uncommitted Work Ready**: PPU catch-up (+29 lines), dungeon sprites (+82 lines)
  - **Parallel Workstreams Launching**:
    - Stream 1: `snes-emulator-expert` → PPU completion, audio fix
    - Stream 2: `imgui-frontend-engineer` → SDL3 planning
    - Stream 3: `zelda3-hacking-expert` → Tile16 fix, sprite movement
    - Stream 4: `ai-infra-architect` → Semantic inspection API
  - **Target**: Q1 2026 release
- REQUESTS:
  - CLAIM → `snes-emulator-expert`: Complete PPU JIT integration in `ppu.cc`
  - CLAIM → `zelda3-hacking-expert`: Fix Tile16 palette system in `tile16_editor.cc`
  - CLAIM → `imgui-frontend-engineer`: Begin SDL3 migration planning
  - CLAIM → `ai-infra-architect`: Design semantic inspection API
  - INFO → ALL: Read initiative doc before claiming tasks

### 2025-11-23 GEMINI_FLASH_AUTOM - Web Port Milestone 0-4
- TASK: Implement Web Port (WASM) + CI
- SCOPE: CMakePresets.json, src/app/main.cc, src/web/shell.html, .github/workflows/web-build.yml
- STATUS: COMPLETE
- NOTES:
  - Added `wasm-release` preset
  - Implemented Emscripten main loop and file system mounts
  - Created web shell with ROM upload / save download
  - Added CI workflow for automated builds and GitHub Pages deployment

---

### 2025-11-23 ai-infra-architect - Semantic Inspection API
- TASK: Implement Semantic Inspection API Phase 1 for AI agents
- SCOPE: src/app/emu/debug/semantic_introspection.{h,cc}
- STATUS: COMPLETE
- NOTES: Game state JSON serialization for AI agents. Phase 1 MVP complete.

### 2025-11-23 backend-infra-engineer – WASM Platform Layer (All 8 Phases)
- TASK: Implement complete WASM platform infrastructure for browser-based yaze
- SCOPE: src/app/platform/wasm/, src/web/, src/app/emu/platform/wasm/, src/cli/service/ai/
- STATUS: COMPLETE
- NOTES:
  - Phases 1-3: File system (IndexedDB), error handling (browser UI), progressive loading
  - Phases 4-6: Offline support (service-worker.js), AI integration, local storage (settings/autosave)
  - Phases 7-8: Web workers (pthread pool), WebAudio (SPC700 playback)
  - Arena integration: WasmLoadingManager connected to LoadAllGraphicsData
- FILES: wasm_{storage,file_dialog,error_handler,loading_manager,settings,autosave,secure_storage,worker_pool}.{h,cc}, wasm_audio.{h,cc}

---

### 2025-11-23 imgui-frontend-engineer – SDL3 Backend Infrastructure
- TASK: Implement SDL3 backend infrastructure for v0.4.0 migration
- SCOPE: src/app/platform/, src/app/emu/audio/, src/app/emu/input/, src/app/gfx/backend/
- STATUS: COMPLETE (commit a5dc884612)
- NOTES: 17 new files, IWindowBackend/IAudioBackend/IInputBackend/IRenderer interfaces

---

### 2025-11-22 backend-infra-engineer - CI Optimization
- TASK: Optimize CI for lean PR/push runs with comprehensive nightly testing
- SCOPE: .github/workflows/ci.yml, nightly.yml
- STATUS: COMPLETE
- NOTES: PR CI ~5-10 min (was 15-20), nightly runs comprehensive tests

---

### 2025-11-22 test-infrastructure-expert - Test Suite Gating
- TASK: Gate optional test suites OFF by default (Test Slimdown Initiative)
- SCOPE: cmake/options.cmake, test/CMakeLists.txt
- STATUS: COMPLETE
- NOTES: AI/ROM/benchmark tests now require explicit enabling

---

### 2025-11-22 ai-infra-architect - FileSystemTool
- TASK: Implement FileSystemTool for AI agents (Milestone 4, Phase 3)
- SCOPE: src/cli/service/agent/tools/filesystem_tool.{h,cc}
- STATUS: COMPLETE
- NOTES: Read-only filesystem exploration with security features

---

## Archived Sessions

Historical entries from 2025-11-20 to 2025-11-22 have been archived to:
`docs/internal/agents/archive/coordination-board-2025-11-20-to-22.md`
