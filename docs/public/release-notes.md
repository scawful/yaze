# Release Notes

## v0.5.1 (Draft)

**Type:** UX + Storage Consolidation
**Status:** In progress

### ✨ UX & Guidance
- Help panel now reflects configured shortcuts and adds searchable shortcut lists.
- Supported Features popup lists platform status + persistence details for desktop/CLI/web.
- User-facing warnings added for unimplemented collaboration and E2E menus.

### 🗂️ Storage & Paths
- Unified app data under `~/.yaze` across desktop/CLI; legacy AppData/Library/XDG migrated.
- Web build storage consolidated under `/.yaze` (IDBFS) for ROMs/saves/projects.
- Project management panel now surfaces storage paths and version mismatch warnings.

### 🧭 Versioning
- Added `VERSION` file as the source of truth for build/versioning.
- Web version badge now syncs to the runtime WASM version.

### 🧪 Testing
- Feature coverage report refreshed with storage and UI persistence notes.
- New coverage plan items added for `.yaze` migration and layout/shortcut persistence.

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
