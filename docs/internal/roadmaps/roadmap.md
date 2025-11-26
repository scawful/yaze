# Roadmap

**Last Updated: November 26, 2025**

This roadmap tracks upcoming releases and major ongoing initiatives.

## Current Focus (v0.4.0)

- **SDL3 Backend Infrastructure**: Complete (17 new files for IWindowBackend/IAudioBackend/IInputBackend/IRenderer interfaces)
- **WASM Web Port**: Complete (Milestones 0-4 shipped, real-time collaboration active)
- **EditorManager Refactoring**: Complete (90% feature parity, 44% code reduction)
- **AI Agent Tools**: Phases 1-4 complete (meta-tools, schemas, context, batching, validation)
- **GUI Bug Fixes**: BeginChild/EndChild patterns, duplicate rendering issues resolved

## 0.4.0 (Next Major Release) - SDL3 Modernization & Core Improvements

**Status:** In Progress  
**Type:** Major Breaking Release  
**Target:** Q1 2026

### Completed ✅

#### SDL3 Backend Infrastructure
- ✅ IWindowBackend/IAudioBackend/IInputBackend/IRenderer interfaces (commit a5dc884612)
- ✅ 17 new abstraction files in `src/app/platform/`

#### WASM Web Port
- ✅ Emscripten build preset (`wasm-release`)
- ✅ Web shell with ROM upload/download
- ✅ IndexedDB file system integration
- ✅ Progressive loading with WasmLoadingManager
- ✅ Real-time collaboration (WebSocket-based multi-user editing)
- ✅ Offline support via service workers
- ✅ WebAudio for SPC700 playback
- ✅ CI workflow for automated builds and GitHub Pages deployment

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

### In Progress 🟡

#### Emulator Accuracy
- 🟡 PPU JIT catch-up integration
- 🟡 Semantic API for AI agents
- 🟡 State injection improvements

#### Editor Fixes
- 🟡 Dungeon object rendering regression
- 🟡 ZSOW v3 large-area palette issues

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

### v0.3.9 (November 2025)
- WASM web port with real-time collaboration
- SDL3 backend infrastructure
- EditorManager refactoring (90% feature parity)
- AI agent tools Phases 1-4
- CI optimization (PR runs ~5-10 min, was 15-20)
- Test suite gating (optional tests OFF by default)

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
