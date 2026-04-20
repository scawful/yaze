# Release Notes

## v0.7.1 (April 2026)

**Type:** Packaging + Welcome Screen Overhaul + Dungeon Editor Parity/Polish
**Date:** 2026-04-19

### Welcome Screen & Project Startup
- Added a guided New Project flow with clearer startup/project creation affordances.
- Added async ROM metadata scanning for recent projects plus pin/rename/notes actions.
- Added a short undo window for recent-project removal and command-palette entry points for welcome actions.

### Dungeon Editor Parity & Polish
- Restored BG1/BG2 layout routing while preserving pit/mask semantics.
- Fixed real-room `0x034` rendering for single-tile payloads.
- Added replay-geometry selection bounds for more accurate object hitboxes.
- Added ROM-backed parity tests and room render snapshots.
- Simplified workbench inspector/navigation flows, kept hidden room state sparse, and lazily materialized room state.

### Performance & Footprint
- Added lazy session-editor construction.
- Deferred hidden full-mode asset loads.
- Trimmed eager overworld bitmap memory.
- Released unused room buffer textures and split render-target texture creation.

### CLI / Build / CI
- Fixed the WASM dungeon tile-row build path.
- Added overworld map ID validation before ROM access.
- Hardened Linux GUI smoke-run binary path handling.

### Deferred to 0.8.0
- z3dk integration proposal tracks embedded assembler/LSP/lint workflows, unified Mesen2 client work, and `.mlb` export.

---

## v0.7.0 (March 2026)

**Type:** Feature Completion + iOS Remote Control + API Expansion
**Date:** 2026-03-03

### 📱 iOS Remote Control
- Added Bonjour LAN discovery (`_yaze._tcp.`) for auto-detecting desktop instances.
- Added Remote Room Viewer for all 296 dungeon rooms with overlay/metadata controls.
- Added Remote Command Runner with command catalog autocomplete/history and `--write` safety confirmation.
- Added Annotation Review Mode with create/edit/delete workflows synced through desktop REST endpoints.
- Added desktop connection controls with discovered hosts, manual IP fallback, and persistent status pill.

### 🌐 Desktop HTTP API
- Added `POST /api/v1/command/execute` for remote z3ed command execution.
- Added `GET /api/v1/command/list` for command metadata and autocomplete catalogs.
- Added `GET/POST/PUT/DELETE /api/v1/annotations` for annotation CRUD against `annotations.json`.
- Added Bonjour publisher integration for desktop service advertisement on macOS.

### 🧰 Editor Completion (0.7.0 must-ship + should-ship)
- Added Sprite Editor undo/redo with snapshot actions and unit tests.
- Added Screen Editor undo/redo and restored toolbar wiring for map edits.
- Added Message Editor replace + replace-all behavior.
- Completed music tracker stubs (rest/key-off insertion, range delete, song rename popup).

### 🏰 Dungeon Editor Improvements
- Added `DungeonUsageTracker` visual usage grids for blockset/spriteset/palette analysis.
- Polished dungeon workbench/panel workflow (mode clarity, status badges, and return affordances).
- Extended undo/redo coverage into adjacent dungeon map edit workflows.

### 🛠️ Custom ROM Hack Features
- Added desktop BPS patch export/import with CRC validation for patch-based release flows.
- Strengthened overworld hack editing reliability with hard-delete semantics, list/filter/sort + duplicate/nudge iteration flow, and undo-backed item workflows.

### 💻 CLI Expansion
- Added palette command set (`palette-get-colors`, `palette-set-color`, `palette-analyze`) for scripted ROM-hack iteration and automation.

### 🎨 UI and Theming
- Added themed tab-bar primitives (`BeginThemedTabBar`/`EndThemedTabBar`).
- Adopted themed widgets in key editor surfaces (dungeon workbench, status bar, pixel editor, screen editor).

### ✅ Validation Snapshot
- `ctest --preset mac-ai-unit --output-on-failure` passed on the release candidate revision.
- `ctest --preset mac-ai-integration --output-on-failure` passed on the release candidate revision.

### 🗂️ Deferred to 0.8.0
- Overworld scratch-pad persistence.
- Overworld eyedropper mode.
- Graphics editor clipboard workflow.
- Music SPC/MML import.
- Final dungeon draw-routine registry dedupe/docs cleanup.

---

## v0.6.2 (February 2026)

**Type:** Release Consistency + Bundle/Validation Hardening
**Date:** 2026-02-24

### 🔄 Version Consistency
- Updated all current-release markers and app-facing metadata to `0.6.2`.
- Aligned release/coverage documentation with the current build version.

### 📦 `.yazeproj` Bundle Workflow
- Hardened bundle unpack cleanup behavior on invalid bundle failures.
- Expanded dry-run validation and bundle verification coverage.

### 🧪 Oracle Validation
- Continued smoke/preflight workflow improvements for Oracle development loops.

### 🎛️ Editor UX
- Continued refinement of dungeon placement feedback and tile selector usability.

---

## v0.6.1 (February 2026)

**Type:** Oracle Validation + Bundle Workflow + Editor UX Hardening
**Date:** 2026-02-24

### 🧪 Oracle Validation Workflow
- Added Oracle-focused smoke/preflight command flows and reporting improvements.
- Added D6 track-room threshold gating (`--min-d6-track-rooms`) to detect minecart regressions in structural checks.

### 📦 `.yazeproj` Bundle Workflow
- Added project bundle verify/pack/unpack command support in `z3ed`.
- Added safer unpack defaults:
  - rejects traversal entries,
  - cleans partial output on invalid bundle failure,
  - supports `--keep-partial-output` for debugging.
- Added `--dry-run` unpack mode for non-writing structural checks.

### 🔐 Integrity & Hashing
- Standardized SHA1 generation across platforms for bundle verification.
- Added bundle ROM hash verification (`--check-rom-hash`) and robust hash normalization.

### 🎛️ Editor UX
- Improved dungeon placement feedback near/at entity limits.
- Added custom-object overlay controls and faster D6 room navigation in dungeon workbench.
- Improved Tile16/tile selector interactions with hover previews, filter/jump UX, range validation feedback, and explicit decimal tile ID input (`d:<id>`).

---

## v0.6.0 (February 2026)

**Type:** Undo System + Dungeon Compositing + UI Overhaul
**Date:** 2026-02-10

### Undo/Redo
- Unified `UndoManager` embedded in `Editor` base class.
- Per-editor undo/redo: overworld, dungeon, graphics, music, message.
- Custom collision and water fill undo actions.

### Dungeon Editor
- SNES priority compositing with coverage masks.
- Entity drag-drop with selection inspector.
- Custom collision editor with JSON import/export.
- Water fill zone authoring with brush radius.
- Mutation tagging by domain (tile objects, collision, water fill).

### UI
- Semantic color system replacing hardcoded style pushes.
- EventBus migration replacing legacy callback navigation.
- Right panel manager and sidebar simplification.
- Viewport-relative sizing helpers for responsive dialogs.

### ROM Safety
- Write fence stack rejecting out-of-bounds writes.
- Dirty custom collision save without full room reload.

### Cleanup
- Removed `SessionObserver`, `PanelManager` callbacks, legacy navigation APIs.
- Removed deprecated `SetMutationHook` alias.
- Archived 20 stale internal docs.

---

## v0.5.6 (February 2026)

**Type:** Dungeon Editor UX + Minecart Tooling
**Date:** 2026-02-05

### ⛏️ Dungeon & Minecart Tooling
- Configurable minecart collision IDs and track object IDs (project-level `[dungeon_overlay]` settings).
- Track collision overlay with legend + per-tile direction arrows (straights, corners, T-junctions, switches).
- Minecart Track Editor audit: 32 slots, filler detection, missing-start warnings, and room coverage reporting.
- Minecart sprite overlay to flag carts placed off stop tiles.
- Camera quadrant overlay to plan fast cart routes.

### 🎯 Object Editing UX
- Custom object previews keyed by subtype and bounds derived from custom extents.
- Hover/selection now respects the active layer filter.

### 🧪 Stability
- Headless ImGui initialization hardened for editor tests.

---

## v0.5.5 (January 2026)

**Type:** Editor Foundations + Stability
**Date:** 2026-01-31

### 🧰 Editor & Architecture
- Modernized `EditorManager` for better isolation and testability.
- Introduced `yaze_core_lib` to separate core logic from the app shell.

### 🧪 Tests
- Added `AsarCompilerTest` and `EditorManagerTest` suites.

### 🎨 Graphics
- Fallback to grayscale palette for graphics sheets missing a palette.

### 🧱 Build
- Cleaned up CMake entry points and presets; unified `main` entry point logic.

---

## v0.5.4 (Release Candidate January 2026)

**Type:** Stability + Mesen2 Debugging
**Date:** 2026-01-25

### 🐞 Mesen2 Debugging
- New Mesen2 debug panel in the Agent editor (socket picker, overlay controls, save/load, screenshot capture).
- Mesen2 debug shortcut (Ctrl+Shift+M) and socket list refresh on panel open.
- New z3ed `mesen-*` CLI commands for live Mesen2 inspection and control.

### 🤖 AI & HTTP API
- Model registry caching with `/api/v1/models?refresh` support.
- CORS + error handling for HTTP API endpoints and `/symbols` format validation.
- Normalized OpenAI base URL detection for local OpenAI-compatible servers.

### 🧰 Desktop UX
- Fix message editor preview/font atlas rendering after ROM load.
- Sync editor/panel context on category switches to avoid blank views.

### 📦 Nightly Builds
- Normalize macOS nightly bundle layout so launchers resolve `yaze.app`.
- Refresh wrapper detection to handle alternate app locations.

### 🧾 Versioning
- Sync version strings across docs, build config, and project defaults to 0.5.4.

---

## v0.5.3 (Released January 2026)

**Type:** Build, WASM & Code Quality
**Date:** 2026-01-20

### 🔧 Build & Release
- Fix release validation scripts for DMG packaging.
- Create VERSION file as canonical source of truth (0.5.3).
- Update CMakeLists.txt fallback version.
- Fix wasm-ai CMake preset (add `YAZE_ENABLE_AI=ON` for AI_RUNTIME dependency).

### 🌐 WASM/Web
- Service worker: Stream responses instead of buffering (fixes memory spikes for large assets).
- Service worker: Throttle cache eviction to once per 60 seconds (reduces O(n) overhead).
- Filesystem tool: Tighten path guard to prevent `/.yazeevil` bypass attack.
- Build tool: Fix boolean output format for JSON responses.

### 🤖 AI & CLI
- Add LMStudio support with configurable `--openai_base_url` flag.
- Allow empty API key for local OpenAI-compatible servers.

---

## v0.5.2 (Released January 2026)

**Type:** Build Fix
**Date:** 2026-01-20

### 🔧 Build
- Fix build when `YAZE_AI_RUNTIME` is disabled.
- Add proper guards around AI runtime-dependent code paths.

---

## v0.5.1 (Released January 2026)

**Type:** UX + UI Polish
**Date:** 2026-01-20

### ✨ UI Modernization (ImHex-inspired)
- Restructure menus and fix sidebar toggle icon.
- Add comprehensive UI polish with animations and theming enhancements.
- Cross-platform theme file system with `~/.yaze/themes/` support.
- Complete ImHex UI modernization phases 2-5.
- Animated hover effects for themed widget buttons.
- List virtualization and expanded command palette.
- Lazy panel initialization with `OnFirstDraw` hook.
- GUI animation system with smooth hover effects.

### 🧩 Architecture
- ContentRegistry panel self-registration with `REGISTER_PANEL` macro.
- Core UI events and texture queue budget.
- Extract `yaze_cli_core` library for shared CLI infrastructure.
- `ZoomChangedEvent` published from Canvas zoom methods.

### 🗂️ Storage & Paths
- Unified app data under `~/.yaze` across desktop/CLI.
- Web build storage consolidated under `/.yaze` (IDBFS).
- Project management panel now surfaces storage paths.

### 🧭 Versioning
- Added `VERSION` file as the source of truth for build/versioning.

---

## v0.5.0 (Released January 2026)

**Type:** Platform Expansion & Stability
**Date:** 2026-01-10

### 🧩 Graphics & Palette Accuracy
- Fixed palette conversion and Tile16 tint regressions.
- Corrected palette slicing for graphics sheets and indexed → SNES planar conversion.
- Stabilized overworld palette/tilemap saves and render service GameData loads.

### 🧭 Editor UX & Reliability
- Refined dashboard/editor selection layouts and card rendering.
- Moved the layout designer into a lab target for safer experimentation.
- Hardened room loading APIs and added room count reporting for C API consumers.
- Refreshed welcome screen and help text across desktop/CLI/web to spotlight multi-provider AI and CLI workflows.

### 🤖 Automation & AI
- Added agent control server support and stabilized gRPC automation hooks.
- Expanded z3ed CLI test commands (`test-list`, `test-run`, `test-status`) and tool metadata.
- Improved agent command routing and help/schema surfacing for AI clients.
- Added OpenAI/Anthropic provider support in z3ed and refreshed AI provider docs/help.
- Introduced vision refiner/verification hooks for AI-assisted validation.

### 🌐 Web/WASM Preview
- Reduced filesystem initialization overhead and fixed `/projects` directory handling.
- Hardened browser terminal integration and storage error reporting.

### 📦 Platform & Build
- Added iOS platform scaffolding (experimental).
- Added build helper scripts and simplified nightly workflow.
- Refreshed toolchain/dependency wiring and standardized build directory policy.
- Hardened Windows gRPC builds by forcing the Win32 macro-compat include for gRPC targets.
- Fixed Linux static link order for test suites and tightened Abseil linkage.
- Release artifacts now include a release-focused README and exclude internal test helper tools.
- Windows ships as a portable zip (no installer) with trimmed runtime DLLs.

### 🧪 Testing
- Added role-based ROM selection and ROM-availability gating.
- Stabilized rendering/benchmark tests and aligned integration expectations.
- Added AgentChat history/telemetry and agent metrics unit coverage; expanded WASM debug API checks.

---

## v0.4.0 - Music Editor & UI Polish
**Released:** November 2025

- Complete SPC music editing infrastructure.
- EditorManager refactoring for better multi-window support.
- AI Agent integration (experimental).
