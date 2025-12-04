# Coordination Board

**STOP:** Before posting, verify your **Agent ID** in [personas.md](personas.md). Use only canonical IDs.
**Guidelines:** Keep entries concise (<=5 lines). Archive completed work weekly. Target <=40 active entries.

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
- TASK: Evaluate WASM web collaboration vs yaze-server and deploy compatible backend to halext-server.
- SCOPE: src/app/platform/wasm/*collaboration*, src/web/collaboration_ui.*, ~/Code/yaze-server, halext-server deployment.
- STATUS: COMPLETE
- NOTES: Added WASM-protocol shim + passwords/rate limits + Gemini AI handler to yaze-server/server.js (halext pm2 `yaze-collab`, port 8765). Web client wired to collab via exported bindings; docked chat/console UI added. Needs wasm rebuild to ship UI; AI requires GEMINI_API_KEY/AI_AGENT_ENDPOINT set server-side.

### 2025-11-24 CODEX – Dungeon objects & ZSOW palette (ACTIVE)
- TASK: Fix dungeon object rendering regression + ZSOW v3 large-area palette issues; add regression tests
- SCOPE: dungeon editor rendering, overworld palette mapping/tests
- STATUS: ACTIVE
- NOTES: Visual defects reported; will run regression tests and patch palettes

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
