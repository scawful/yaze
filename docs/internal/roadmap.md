# Roadmap

**Last Updated: February 5, 2026**

This roadmap tracks upcoming releases and major ongoing initiatives.

## Current Focus (v0.5.6)

### Priority 0: AI Registry & Model Context
- **AI Registry (Phase 1)**: unify provider/model discovery across UI + CLI; surface model metadata, display names, and refresh controls.
- **Model Context Settings**: per-profile context presets (system prompts, tool scopes, cache TTLs) with safe defaults for local vs hosted providers.
- **Provider Parity**: align OpenAI/Anthropic/Gemini/Ollama/LM Studio base URL handling and configuration UX.
- **In Progress**: ModelRegistry foundation (caching + HTTP API refresh) landing in CLI/API.

### Priority 1: Editor Feature Completion
- **Palette JSON Import/Export**: Complete `palette_group_panel.cc:529,534` for palette file exchange
- **Palette Clipboard Import**: Implement `palette_group_panel.cc:559` for copy/paste workflows
- **Graphics Screen Editor**: Implement edit operations (Undo/Redo/Cut/Copy/Paste) in `screen_editor.h:46-52`
- **Link Sprite Reset to Vanilla**: `link_sprite_panel.cc:339` - preserve vanilla backup

### Priority 2: Music Editor Serialization
- **SaveInstruments**: `music_bank.cc:925` - instrument data persistence
- **SaveSamples (BRR encoding)**: `music_bank.cc:996` - sample data with BRR compression
- **WAV/BRR Integration**: `music_bank.cc:400` - proper loading and encoding

### Priority 3: Workspace & Layout
- **Layout Serialization**: `workspace_manager.cc:18,26,34` - ImGui docking layout save/load/reset
- **Preset Layouts**: Developer, Designer, Modder presets in `workspace_manager.cc:129,136,143`
- **Window Cycling**: Forward/backward navigation in `workspace_manager.cc:242,246`

### Priority 4: Platform & Performance
- **Shutdown Performance**: Fix slow shutdown in `window.cc:168` (graphics arena ordering)
- **CRC32 Calculation**: Complete ASAR checksums in `asar_wrapper.cc:330,501`
- **ZScream Format**: Parse ZScream project format when spec available

### Deferred to v0.5.7+
- Room object type verification (12+ dungeon object unknowns)
- WASM proposal system completion
- AI Vision Verifier PNG/base64 encoding

## v0.5.3 (Released)

- **WASM/Web Infrastructure**: Service worker caching, persistent storage expansion, build tool stubs
- **Local AI Support**: LMStudio and OpenAI-compatible local server support via `--openai_base_url`
- **Release Packaging**: DMG validation improvements, VERSION file as source of truth
- **Code Review Fixes**: build_tool boolean output, filesystem path guard, SW streaming/throttling

### WASM Web Port Status

**Status**: Technically complete but **EXPERIMENTAL/PREVIEW**
- ✅ Build system, file loading, basic editors functional; GH Pages deploy path hardened (web-build caching/branch gating, updated `src/web/` structure).
- ✅ New browser UI: command palette, file manager, pixel inspector, panelized shell UI, theme definitions, touch gesture support, and expanded debug hooks.
- ✅ Async queue/serialization guard to avoid Asyncify crashes; browser `yaze_agent` build enabled with web AI providers.
- ⚠️ Editors are incomplete/preview quality - not production-ready; emulator audio/plugins/advanced editing still missing; WASM FS/persistence hardening in progress (another agent).
- **Recommendation**: Desktop build for serious ROM hacking
- **Documentation**: See `docs/public/usage/web-app.md`

## 0.4.0 (Historical Release) - Music Editor & UI Polish

**Status:** Released
**Type:** Feature Release
**Released:** November 2025

### Highlights

#### Music Editor (New!)
- ✅ Complete SPC music editing with tracker and piano roll views
- ✅ Authentic N-SPC audio preview with ADSR envelopes
- ✅ Instrument and sample editors with bank management
- ✅ Piano roll with playback cursor, note editing, and velocity control
- ✅ ASM export/import for custom music integration
- ✅ Per-song tracker windows (like dungeon room cards)
- ✅ Layout system integration with staggered default positions

### Completed ✅

#### Music Editor Infrastructure
- ✅ SPC parser and music bank loader
- ✅ N-SPC pitch table for authentic note playback
- ✅ Single call site audio initialization (EnsureAudioReady)
- ✅ DSP interpolation type control (Linear/Hermite/Cosine/Cubic)
- ✅ Playback position tracking with cursor visualization
- ✅ Segment seeking and preview callbacks

#### SDL3 Backend Infrastructure (Groundwork)
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
- ✅ SDL3 backend infrastructure complete (17 abstraction files in `src/app/platform/`)
- ✅ `IWindowBackend`/`IAudioBackend`/`IInputBackend`/`IRenderer` interfaces
- ✅ `WindowBackendFactory` for runtime SDL2/SDL3 selection
- ✅ `YAZE_USE_SDL3` CMake option (experimental, OFF by default)
- 🟡 SDL3 enables ImGui viewports by default (multi-window support)
- 🟡 GLFW not integrated - would require new backend implementation

#### Music Editor Overhaul
- 🟡 Tracker + piano roll + instrument/sample editors are in-flight; stabilize playback/export paths and polish UI/shortcuts.

#### Tile16 Editor & Project Persistence
- 🟡 Finalize pending-change workflow UX, palette handling, and overworld context menus; align project metadata/JSON refactor with WASM FS persistence work (ai-infra-architect).

#### Editor Fixes
- ✅ Dungeon object rendering core system stable (222/222 tests passing)
- 🟡 Minor visual discrepancies in specific objects (vertical rails, doors, etc.) - requires individual verification

### Remaining Work (Deferred to 0.5.x)

#### Editor Polish
- Resolve remaining Tile16 palette inconsistencies
- Complete overworld sprite workflow
- Improve dungeon editor labels and tab management
- Add lazy loading for rooms

### CI/CD & Release Health
- Release workflow repairs (cache key/cleanup, Windows crash handler) merged
- Web-build workflow updated for new `src/web/` layout with caching/branch gating
- CI workflows consolidated with standardized Doxygen 1.10 install

---

## 0.5.0 - SDL3 Migration & Feature Expansion

**Status:** Planning
**Type:** Major Breaking Release

### SDL3 Core Migration (Postponed from 0.4.0)
- Switch to SDL3 with GPU-based rendering
- Port editors to new backend
- Implement SDL3 audio/input backends
- Benchmark and tune performance

### Feature Expansion
- **Plugin Architecture**: Initial framework for community extensions
- **Advanced Graphics Editing**: Edit and re-import full graphics sheets
- **`z3ed` AI Agent Enhancements**:
  - Collaborative sessions with shared AI proposals
  - Multi-modal input with screenshot context for Gemini
  - Visual Analysis Tool (Phase 5 ready for implementation)

### Breaking Changes (Planned)
- SDL2 → SDL3 (requires recompilation)
- API changes in graphics backend (for extensions)

---

## 0.6.X - Content & Integration

- **Advanced Content Editors**:
  - Enhanced Hex Editor with search and data interpretation
  - Advanced message editor with font preview
- **Documentation Overhaul**:
  - Auto-generated C++ API documentation
  - Comprehensive user guide for ROM hackers

---

## Recently Completed

### v0.4.0 (November 2025)
- **Music Editor** - Complete SPC music editing with tracker and piano roll views, authentic N-SPC audio preview, instrument/sample editors
- Piano roll with playback cursor, note editing, velocity/duration control
- Per-song tracker windows with layout system integration
- Single call site audio initialization (EnsureAudioReady)
- DSP interpolation type control for audio quality
- AI agent UX revamp (dedicated chat/proposals, session-aware chat, provider safeguards)
- WASM web app stabilization and hardened web-build workflow
- Tile16 workflow and project persistence upgrades
- Editor menu/layout refactor with ActivityBar + layout presets

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
