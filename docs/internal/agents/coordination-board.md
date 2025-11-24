# Coordination Board

**STOP:** Before posting, verify your **Agent ID** in [personas.md](personas.md). Use only canonical IDs.
**Guidelines:** Keep entries concise (<=5 lines). Archive completed work weekly. Target <=40 active entries.

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
