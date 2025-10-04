# Roadmap

**Last Updated: October 4, 2025**

This document outlines the development roadmap for YAZE. The project has made significant strides in core features, AI integration, and testing infrastructure. The immediate focus is on stabilizing the test suite and completing key editor functionalities.

## Current Focus: Stability & Core Features

The highest priority is to fix the broken test suites to ensure the stability and correctness of the existing, largely complete, feature set. Alongside this, work will continue on completing the core AI and editor functionalities.

---

## 0.4.X (Next Major Release) - Stability & Core Tooling

### Priority 1: Testing & Stability (BLOCKER)
-   **Fix Dungeon Editor Test Suite**: Resolve the critical `SIGBUS` and `SIGSEGV` crashes that are blocking all integration tests for the dungeon system.
-   **Refactor Integration Tests**: Migrate tests from the unstable `MockRom` to use the `TestRomManager` with real ROM files, following the pattern set by the passing rendering tests.
-   **Expand E2E Coverage**: Create a comprehensive end-to-end smoke test for the Dungeon Editor and expand coverage for the Overworld editor.

### Priority 2: `z3ed` AI Agent
-   **Complete Live LLM Testing**: Verify and harden the function-calling loop with live Ollama and Gemini models.
-   **Enhance GUI Chat Widget**: Implement state persistence and integrate proposal review shortcuts directly into the editor's chat window.
-   **Expand Agent Toolset**: Add new read-only tools for inspecting dialogue, sprite properties, and other game resources.

### Priority 3: Editor Features
-   **Dungeon Editor**: Implement missing features identified from the ZScream comparison, such as custom collision and object limits tracking.
-   **Overworld Sprites**: Complete the sprite editing workflow, including adding, removing, and moving sprites on the canvas.
-   **Sprite Property Editor**: Create a UI to edit sprite attributes and behavior.

---

## 0.5.X - Feature Expansion

-   **Plugin Architecture**: Design and implement the initial framework for community-developed extensions and custom tools.
-   **Advanced Graphics Editing**:
    -   Finalize the Tile16 Editor palette system, fixing all known color consistency bugs.
    -   Implement functionality to edit and re-import full graphics sheets.
-   **`z3ed` AI Agent Enhancements**:
    -   **Collaborative Sessions**: Investigate and implement infrastructure for multi-user collaborative editing and AI interaction.
    -   **Multi-modal Input**: Integrate screenshot capabilities to send visual context to Gemini for more accurate, context-aware commands.

---

## 0.6.X - Polish & Integration

-   **Cross-Platform Stability**: Conduct full testing and bug fixing cycles for Windows and Linux to ensure a stable experience equivalent to macOS.
-   **Advanced Content Editors**:
    -   Implement a user interface for the music editing system.
    -   Enhance the Hex Editor with better search and data interpretation features.
-   **Documentation Overhaul**:
    -   Implement a system to auto-generate C++ API documentation.
    -   Write a comprehensive user guide for ROM hackers, covering all major editor workflows.

---

## 0.7.X and Beyond - Path to 1.0

-   **Performance Optimization**: Implement high-priority items from the graphics performance plan, such as lazy loading of graphics and streaming assets from the ROM.
-   **UI/UX Refinements**: Improve UI consistency, iconography, and overall layout based on user feedback.
-   **Beta Release**: Code freeze on major features to focus on bug fixing and final polish.
-   **1.0 Stable Release**: Final, production-ready version with a comprehensive changelog.

