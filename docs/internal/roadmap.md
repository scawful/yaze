# Roadmap

**Last Updated: November 29, 2025**

This roadmap tracks upcoming releases and major ongoing initiatives.

## Current Focus (v0.4.0)

- **WASM Web App UX/Stability**: ✅ Command palette, file manager, pixel inspector, shell UI refresh, theme updates; async queue + gesture/input fixes reduce Asyncify crashes; GH Pages deploy stabilized via updated web-build workflow; browser `yaze_agent` build enabled.
- **AI Agent UX Revamp**: ✅ Dedicated chat/proposals panels with session-aware chat, provider hardening, and YAML autodetect for safer prompts.
- **Music Editor Overhaul**: 🟡 Tracker + piano roll + instrument/sample editors with SPC parser/bank loader and emulator audio hooks; regression tests added.
- **Tile16 & Project Persistence**: 🟡 Pending-change workflow, overworld context menus, 16-color palettes; project metadata refactor + JSON helper; tile16/music integration/unit tests.
- **SDL3 Backend Infrastructure**: ✅ Groundwork landed (IWindow/IAudio/IInput/IRenderer); SDL3 readiness report + entry-point/flag cleanup owned by another agent.
- **Emulator Input/Render**: 🟡 PPU catch-up, dungeon preview render service, and input persistence still in flight.

### WASM Web Port Status

**Status**: Technically complete but **EXPERIMENTAL/PREVIEW**
- ✅ Build system, file loading, basic editors functional; GH Pages deploy path hardened (web-build caching/branch gating, updated `src/web/` structure).
- ✅ New browser UI: command palette, file manager, pixel inspector, panelized shell UI, theme definitions, touch gesture support, and expanded debug hooks.
- ✅ Async queue/serialization guard to avoid Asyncify crashes; browser `yaze_agent` build enabled with web AI providers.
- ⚠️ Editors are incomplete/preview quality - not production-ready; emulator audio/plugins/advanced editing still missing; WASM FS/persistence hardening in progress (another agent).
- **Recommendation**: Desktop build for serious ROM hacking
- **Documentation**: See `docs/public/usage/web-app.md`

## 0.4.0 (Next Major Release) - SDL3 Modernization & Core Improvements

**Status:** In Progress  
**Type:** Major Breaking Release  
**Target:** Q1 2026

### Completed ✅

#### SDL3 Backend Infrastructure
- ✅ IWindowBackend/IAudioBackend/IInputBackend/IRenderer interfaces (commit a5dc884612)
- ✅ 17 new abstraction files in `src/app/platform/`

#### WASM Web Port (Experimental)
- ✅ Emscripten build preset (`wasm-release`)
- ✅ Web shell with ROM upload/download
- ✅ IndexedDB file system integration
- ✅ Progressive loading with WasmLoadingManager
- ✅ Real-time collaboration (WebSocket-based multi-user editing)
- ✅ Offline support via service workers
- ✅ WebAudio for SPC700 playback
- ✅ CI workflow for automated builds and GitHub Pages deployment
- ✅ Public documentation (web-app.md) with preview status
- ⚠️ **Note**: Infrastructure complete but editors are preview/incomplete quality

#### EditorManager Refactoring
- ✅ Delegated architecture (8 specialized managers)
- ✅ UICoordinator, MenuOrchestrator, PopupManager, SessionCoordinator
- ✅ EditorCardRegistry, LayoutManager, ShortcutConfigurator
- ✅ 34 editor cards with X-button close
- ✅ 10 editor-specific DockBuilder layouts
- ✅ Multi-session support

#### AI Agent Infrastructure
- ✅ Tools directory integration and discoverability
- ✅ Meta-tools (tools-list/describe/search)
- ✅ ToolSchemas for LLM documentation
- ✅ AgentContext for state management
- ✅ Batch execution support
- ✅ ValidationTool + RomDiffTool
- ✅ Semantic Inspection API Phase 1

#### AI Agent UX & Browser Support
- ✅ Dedicated chat/proposals panels, agent sessions, and configuration UI with session-aware chat history, safer provider handling, and YAML autodetect.
- ✅ `yaze_agent` builds enabled in the browser shell with web AI providers wired through the WASM build.

#### WASM Web UX & Stability
- ✅ Command palette, file manager, pixel inspector, panelized shell UI, theme definitions, touch gestures, and debug overlays added to the web app.
- ✅ Async queue/serialization guard to prevent Asyncify crashes; WASM control API and message queue refactors to harden async flows.
- ✅ Web architecture/card layout documentation added; web-build workflow updated for new `src/web/` structure and GH Pages caching/branch gating.

#### Editor Layout & Menu Refactor
- ✅ Activity bar/right panel rebuild with menu assets moved under the menu namespace; layout presets documented and popup/toast managers reorganized under UI.
- ✅ Project file/editor refactors and card registry cleanup to reduce coupling; JSON abstraction helper added for consistent serialization.

#### Testing
- ✅ New integration/unit coverage for Tile16 editor workflows and music parsing/playback (SPC parser, music bank, editor integration).

### In Progress 🟡

#### Emulator & SDL3 Readiness
- 🟡 PPU JIT catch-up integration
- 🟡 Shared render service for dungeon object preview
- 🟡 Input persistence (keyboard config, ImGui capture flag)
- 🟡 Semantic API for AI agents (Phase 2 planned)
- 🟡 State injection improvements
- 🟡 SDL3 readiness report plus entry-point/flag cleanup (owned by another agent)

#### Music Editor Overhaul
- 🟡 Tracker + piano roll + instrument/sample editors are in-flight; stabilize playback/export paths and polish UI/shortcuts.

#### Tile16 Editor & Project Persistence
- 🟡 Finalize pending-change workflow UX, palette handling, and overworld context menus; align project metadata/JSON refactor with WASM FS persistence work (ai-infra-architect).

#### Editor Fixes
- 🟡 Dungeon object rendering regression (under investigation)

### Remaining Work

#### SDL3 Core Migration
- Switch to SDL3 with GPU-based rendering
- Port editors to new backend
- Implement SDL3 audio/input backends
- Benchmark and tune performance

#### Editor Polish
- Resolve remaining Tile16 palette inconsistencies
- Complete overworld sprite workflow
- Improve dungeon editor labels and tab management
- Add lazy loading for rooms

### Success Criteria
- SDL3 builds pass on Windows, macOS, Linux
- No performance regression versus v0.3.x
- Editors function on the new backend
- Emulator audio/input verified
- Documentation and migration guide updated

### CI/CD & Release Health
- Release workflow repairs (cache key/cleanup, Windows crash handler) merged; v0.3.9-hotfix rerun in progress—monitor GH Actions to confirm all jobs finish green.
- Web-build workflow updated for new `src/web/` layout with caching/branch gating; GH Pages WASM deploys expected to remain stable after recent async/UI hardening.
- CI workflows consolidated with standardized Doxygen 1.10 install; PR CI still optimized (~5–10 minutes) with nightly schedule currently disabled.

**Breaking Changes:**
- SDL2 → SDL3 (requires recompilation)
- Directory restructure (requires submodule re-init)
- API changes in graphics backend (for extensions)

---

## 0.5.X - Feature Expansion

- **Plugin Architecture**: Initial framework for community extensions
- **Advanced Graphics Editing**: Edit and re-import full graphics sheets
- **`z3ed` AI Agent Enhancements**:
  - Collaborative sessions with shared AI proposals
  - Multi-modal input with screenshot context for Gemini
  - Visual Analysis Tool (Phase 5 ready for implementation)

---

## 0.6.X - Content & Integration

- **Advanced Content Editors**:
  - Music editing UI
  - Enhanced Hex Editor with search and data interpretation
- **Documentation Overhaul**:
  - Auto-generated C++ API documentation
  - Comprehensive user guide for ROM hackers

---

## Recently Completed

### Post-v0.3.9 mainline snapshot (Nov 29, 2025)
- Music editor rebuild with tracker/piano roll/instrument/sample views, SPC parser/bank loader, and emulator audio hooks.
- AI agent UX revamp (dedicated chat/proposals, session-aware chat, provider safeguards/YAML autodetect) plus browser `yaze_agent` build.
- WASM web app stabilization (command palette, file manager, pixel inspector, shell UI, async queue/gesture fixes, theme/debug updates) and hardened web-build workflow for GH Pages deploys.
- Tile16 workflow and project persistence upgrades (pending-change workflow, 16-color palettes, project metadata refactor, JSON helper).
- Regression coverage added for Tile16 and music (integration + unit tests).
- Editor menu/layout refactor with ActivityBar + layout presets and card layout architecture documentation.

### v0.3.9 (November 2025)
- WASM web port with real-time collaboration (experimental/preview)
- SDL3 backend infrastructure
- EditorManager refactoring (90% feature parity)
- AI agent tools Phases 1-4
- CI optimization (PR runs ~5-10 min, was 15-20)
- Test suite gating (optional tests OFF by default)
- Documentation cleanup and public web app guide

### v0.3.3 (October 2025)
- Vim mode for `simple-chat`: modal editing, navigation, history, autocomplete
- Autocomplete engine with fuzzy matching and FTXUI dropdown
- TUI enhancements: integrated autocomplete UI components

### v0.3.2
- Dungeon editor: migrated to `TestRomManager`, resolved crash backlog
- Windows build: fixed stack overflows and file dialog regressions
- `z3ed learn`: persistent storage for AI preferences and ROM metadata
- Gemini integration: native function calling API
- Tile16 editor: refactored layout, dynamic zoom controls
