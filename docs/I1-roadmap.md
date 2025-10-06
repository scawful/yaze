# Roadmap

**Last Updated: October 4, 2025**

This document outlines the development roadmap for yaze. The project has achieved stability in its core editors and testing infrastructure. The focus now shifts to completing advanced AI features, polishing UI/UX, and expanding editor capabilities.

## Current Focus: AI & Editor Polish

With the core systems stable, the immediate priority is to enhance the `z3ed` AI agent, finalize the Tile16 editor, and improve the user experience.

---

## 0.4.X (Next Major Release) - Advanced Tooling & UX

### Priority 1: Editor Features & UX
-   **Tile16 Palette System**: Resolve the remaining color consistency issues in the Tile16 editor's source tile view and implement the palette-switching functionality.
-   **Overworld Sprite Editing**: Complete the workflow for adding, removing, and moving sprites directly on the overworld canvas.
-   **Dungeon Editor UI**: Add human-readable labels for rooms/entrances and implement tab management for a better multi-room workflow.
-   **Performance**: Address the slow initial load time (~2.6 seconds) by implementing lazy loading for rooms.

### Priority 2: `z3ed` AI Agent
-   ✅ **Vim Mode**: Implemented vim-style line editing in simple-chat with full modal editing support
-   ✅ **Autocomplete**: Added intelligent command completion with fuzzy matching in FTXUI chat
-   **Live LLM Hardening**: Finalize testing of the native Gemini function-calling loop and the proactive v3 system prompt.
-   **AI-Driven Editing**: Integrate the AI with the GUI test harness to allow for automated, mouse-driven edits based on natural language commands.
-   **Expand Agent Toolset**: Add new read-only tools for inspecting dialogue, music data, and sprite properties.

### Priority 3: Testing & Stability
-   **Windows Validation**: Perform a full testing cycle on Windows to validate the `z3ed` CLI, GUI test harness, and collaboration features.
-   **Expand E2E Coverage**: Create comprehensive end-to-end smoke tests for the Dungeon and Overworld editors.

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

