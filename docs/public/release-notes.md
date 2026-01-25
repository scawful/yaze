# Release Notes

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
