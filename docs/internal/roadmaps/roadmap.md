# Roadmap

**Last Updated: October 4, 2025**

This roadmap tracks upcoming releases and major ongoing initiatives.

## Current Focus

- Finish overworld editor parity (sprite workflows, performance tuning).
- Resolve dungeon object rendering and tile painting gaps.
- Close out Tile16 palette inconsistencies.
- Harden the `z3ed` automation paths before expanding functionality.

## 0.4.0 (Next Major Release) - SDL3 Modernization & Core Improvements

**Status:** Planning  
**Type:** Major Breaking Release  
**Timeline:** 6-8 weeks

### Primary Goals

1. SDL3 migration across graphics, audio, and input
2. Dependency reorganization (`src/lib/` + `third_party/` → `external/`)
3. Backend abstraction layer for renderer/audio/input
4. Editor polish and UX clean-up

### Phase 1: Infrastructure (Week 1-2)
- Merge `src/lib/` and `third_party/` into `external/`
- Update CMake, submodules, and CI presets
- Validate builds on Windows, macOS, Linux

### Phase 2: SDL3 Core Migration (Week 3-4)
- Switch to SDL3 with GPU-based rendering
- Introduce `GraphicsBackend` abstraction
- Restore window creation and baseline editor rendering
- Update the ImGui SDL3 backend

### Phase 3: Complete SDL3 Integration (Week 5-6)
- Port editors (Overworld, Dungeon, Graphics, Palette, Screen, Music) to the new backend
- Implement SDL3 audio backend for the emulator
- Implement SDL3 input backend with improved gamepad support
- Benchmark and tune rendering performance

### Phase 4: Editor Features & UX (Week 7-8)
- Resolve Tile16 palette inconsistencies
- Complete overworld sprite add/remove/move workflow
- Improve dungeon editor labels and tab management
- Add lazy loading for rooms to cut load times

### Phase 5: AI Agent Enhancements (Throughout)
- Vim-style editing in `simple-chat` (complete)
- Autocomplete engine with fuzzy matching (complete)
- Harden live LLM integration (Gemini function-calling, prompts)
- Attach AI workflows to GUI regression harness
- Extend tool coverage for dialogue, music, sprite data

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

-   **Plugin Architecture**: Design and implement the initial framework for community-developed extensions and custom tools.
-   **Advanced Graphics Editing**: Implement functionality to edit and re-import full graphics sheets.
-   **`z3ed` AI Agent Enhancements**:
    -   **Collaborative Sessions**: Enhance the network collaboration mode with shared AI proposals and ROM synchronization.
    -   **Multi-modal Input**: Integrate screenshot capabilities to send visual context to Gemini for more accurate, context-aware commands.

---

## 0.6.X - Content & Integration

-   **Advanced Content Editors**:
    -   Implement a user interface for the music editing system.
    -   Enhance the Hex Editor with better search and data interpretation features.
-   **Documentation Overhaul**:
    -   Implement a system to auto-generate C++ API documentation from Doxygen comments.
    -   Write a comprehensive user guide for ROM hackers, covering all major editor workflows.

---

## Recently Completed (v0.3.3 - October 6, 2025)

- Vim mode for `simple-chat`: modal editing, navigation, history, autocomplete
- Autocomplete engine with fuzzy matching and FTXUI dropdown
- TUI enhancements: integrated autocomplete UI components and CMake wiring

## Recently Completed (v0.3.2)

- Dungeon editor: migrated to `TestRomManager`, resolved crash backlog
- Windows build: fixed stack overflows and file dialog regressions
- `z3ed learn`: added persistent storage for AI preferences and ROM metadata
- Gemini integration: switched to native function calling API
- Tile16 editor: refactored layout, added dynamic zoom controls
