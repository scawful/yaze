# Roadmap

**Last Updated: October 4, 2025**

This document outlines the development roadmap for yaze. The project has achieved stability in its core editors and testing infrastructure. The focus now shifts to completing advanced AI features, polishing UI/UX, and expanding editor capabilities.

## Current Focus: AI & Editor Polish

With the core systems stable, the immediate priority is to finish the overworld editor features, fix dungeon object rendering, finalize the Tile16 editor, enhance the `z3ed` AI agent, and improve the user experience.

---

## 0.4.0 (Next Major Release) - SDL3 Modernization & Core Improvements

**Status:** Planning  
**Type:** Major Breaking Release  
**Timeline:** 6-8 weeks

### 🎯 Primary Goals

1. **SDL3 Migration** - Modernize graphics/audio/input backend
2. **Dependency Reorganization** - Consolidate `src/lib/` + `third_party/` → `external/`
3. **Backend Abstraction** - Clean separation of graphics/audio/input backends
4. **Editor Polish** - Complete remaining UX improvements

### Phase 1: Infrastructure (Week 1-2)
-   **Dependency Consolidation**: Merge `src/lib/` and `third_party/` into unified `external/` directory
-   **CMake Modernization**: Update all build files, submodules, and CI workflows
-   **Build Validation**: Ensure all platforms (Windows/macOS/Linux) build cleanly

### Phase 2: SDL3 Core Migration (Week 3-4)
-   **SDL3 Update**: Migrate from SDL2 to SDL3 with GPU-based rendering
-   **Graphics Backend**: Create abstraction layer (`GraphicsBackend` interface)
-   **Basic Rendering**: Get window creation and basic editor rendering working
-   **ImGui Integration**: Update ImGui SDL3 backend integration

### Phase 3: Complete SDL3 Integration (Week 5-6)
-   **All Editors**: Port all 6 editors (Overworld, Dungeon, Graphics, Palette, Screen, Music) to new backend
-   **Audio Backend**: Implement SDL3 audio backend for emulator
-   **Input Backend**: Implement SDL3 input backend with improved gamepad support
-   **Performance**: Optimize rendering performance, benchmark against v0.3.x

### Phase 4: Editor Features & UX (Week 7-8)
-   **Tile16 Palette System**: Resolve color consistency issues in source tile view
-   **Overworld Sprite Editing**: Complete sprite add/remove/move workflow
-   **Dungeon Editor UI**: Add human-readable room labels and improved tab management
-   **Performance**: Implement lazy loading for rooms (~2.6s → <1s load time)

### Phase 5: AI Agent Enhancements (Throughout)
-   ✅ **Vim Mode**: Implemented vim-style line editing in simple-chat
-   ✅ **Autocomplete**: Added intelligent command completion with fuzzy matching
-   **Live LLM Hardening**: Finalize Gemini function-calling and proactive v3 prompts
-   **AI-Driven Editing**: Integrate AI with GUI test harness for automated edits
-   **Expand Toolset**: Add tools for dialogue, music data, sprite properties

### Success Criteria
- [ ] All platforms build and run with SDL3
- [ ] No performance regression vs v0.3.x
- [ ] All editors functional with new backend
- [ ] Emulator audio/input working
- [ ] Documentation updated
- [ ] Migration guide complete

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

-   ✅ **Vim Mode for CLI**: Full vim-style modal editing in `simple-chat` with normal/insert modes, navigation (hjkl, w/b), editing (dd, yy, p), history, and autocomplete
-   ✅ **Autocomplete System**: Intelligent command completion engine with fuzzy matching, context-aware suggestions, and real-time dropdown in FTXUI chat
-   ✅ **Enhanced TUI**: Integrated autocomplete UI components with proper header files and CMake compilation

## Recently Completed (v0.3.2)

-   ✅ **Dungeon Editor Stability**: Fixed all critical crashes in the test suite by migrating to `TestRomManager`. The editor's core logic is now stable and production-ready.
-   ✅ **Windows Stability**: Resolved stack overflow crashes and file dialog issues, bringing Windows builds to parity with macOS/Linux.
-   ✅ **`z3ed` Learn Command**: Fully implemented the `learn` command, allowing the AI to persist user preferences, ROM patterns, and conversation history.
-   ✅ **Gemini Native Function Calling**: Upgraded from manual `curl` requests to the official Gemini function calling API for improved reliability.
-   ✅ **Tile16 Editor Refactor**: Fixed critical crashes, implemented a new three-column layout, and added dynamic zoom controls.

