# Gemini Workflow Instructions for the `yaze` Project

This document provides a summary of the `yaze` project to guide an AI assistant in understanding the codebase, architecture, and development workflows.

> **Coordination Requirement**  
> Gemini-based agents must read and update the shared coordination board
> (`docs/internal/agents/coordination-board.md`) before making changes. Follow the protocol in
> `AGENTS.md`, use the appropriate persona ID (e.g., `GEMINI_AUTOM`), and respond to any pending
> entries targeting you.

## User Profile

- **User**: A Google programmer working on ROM hacking projects on macOS.
- **IDE**: Visual Studio Code with the CMake Tools extension.
- **Build System**: CMake with a preference for the "Unix Makefiles" generator.
- **Workflow**: Uses CMake presets and a separate `build_test` directory for test builds.
- **AI Assistant Build Policy**: When the AI assistant needs to build the project, it must use a dedicated build directory (e.g., `build_ai` or `build_agent`) to avoid interrupting the user's active builds. Never use `build` or `build_test` directories.

## Project Overview

- **`yaze`**: A cross-platform GUI editor for "The Legend of Zelda: A Link to the Past" (ALTTP) ROMs. It is designed for compatibility with ZScream projects.
- **`z3ed`**: A powerful command-line interface (CLI) for `yaze`. It features a resource-oriented design (`z3ed <resource> <action>`) and serves as the primary API for an AI-driven conversational agent.
- **`yaze.org`**: The file `docs/yaze.org` is an Emacs Org-Mode file used as a development tracker for active issues and features.

## Build Instructions

- Use the presets in `CMakePresets.json` (debug, AI, release, dev, CI, etc.). Always run the verifier
  script before the first build on a machine.
- Gemini agents must configure/build in dedicated directories (`build_ai`, `build_agent`, …) to avoid
  touching the user’s `build` or `build_test` folders.
- Consult [`docs/public/build/quick-reference.md`](docs/public/build/quick-reference.md) for the
  canonical command list, preset overview, and testing guidance.

## Testing

- **Framework**: GoogleTest.
- **Test Categories**:
    - `STABLE`: Fast, reliable, run in CI.
    - `ROM_DEPENDENT`: Require a ROM file, skipped in CI unless a ROM is provided.
    - `EXPERIMENTAL`: May be unstable, allowed to fail.
- **Running Tests**:
  ```bash
  # Run stable tests using ctest and presets
  ctest --preset dev

  # Run comprehensive overworld tests (requires a ROM)
  ./scripts/run_overworld_tests.sh /path/to/zelda3.sfc
  ```
- **E2E GUI Testing**: The project includes a sophisticated end-to-end testing framework using `ImGuiTestEngine`, accessible via a gRPC service. The `z3ed agent test` command can execute natural language prompts as GUI tests.

## Core Architecture & Features

- **Overworld Editor**: Full support for vanilla and `ZSCustomOverworld` v2/v3 ROMs, ensuring compatibility with ZScream projects.
- **Dungeon Editor**: A modular, component-based system for editing rooms, objects, sprites, and more. See `docs/E2-dungeon-editor-guide.md` and `docs/E3-dungeon-editor-design.md`.
- **Graphics System**: A performant system featuring:
    - `Arena`-based resource management.
    - `Bitmap` class for SNES-specific graphics formats.
    - `Tilemap` with an LRU cache.
    - `AtlasRenderer` for batched drawing.
- **Asar Integration**: Built-in support for the Asar 65816 assembler to apply assembly patches to the ROM.

## Editor System Architecture

The editor system is designed around a central `EditorManager` that orchestrates multiple editors and UI components.

- **`EditorManager`**: The top-level class that manages multiple `RomSession`s, the main menu, and all editor windows. It handles the application's main update loop.
- **`RomSession`**: Each session encapsulates a `Rom` instance and a corresponding `EditorSet`, allowing multiple ROMs to be open simultaneously.
- **`EditorSet`**: A container for all individual editor instances (Overworld, Dungeon, etc.) associated with a single ROM session.
- **`Editor` (Base Class)**: A virtual base class (`src/app/editor/editor.h`) that defines a common interface for all editors, including methods like `Initialize`, `Load`, `Update`, and `Save`.

### Component-Based Editors
The project is moving towards a component-based architecture to improve modularity and maintainability. This is most evident in the Dungeon Editor.

- **Dungeon Editor**: Refactored into a collection of single-responsibility components orchestrated by `DungeonEditor`:
    - `DungeonRoomSelector`: Manages the UI for selecting rooms and entrances.
    - `DungeonCanvasViewer`: Handles the rendering of the dungeon room on the main canvas.
    - `DungeonObjectSelector`: Provides the UI for browsing and selecting objects, sprites, and other editable elements.
    - `DungeonObjectInteraction`: Manages mouse input, selection, and drag-and-drop on the canvas.
    - `DungeonToolset`: The main toolbar for the editor.
    - `DungeonRenderer`: A dedicated rendering engine for dungeon objects, featuring a cache for performance.
    - `DungeonRoomLoader`: Handles the logic for loading all room data from the ROM, now optimized with parallel processing.
    - `DungeonUsageTracker`: Analyzes and displays statistics on resource usage (blocksets, palettes, etc.).

- **Overworld Editor**: Also employs a component-based approach with helpers like `OverworldEditorManager` for ZSCustomOverworld v3 features and `MapPropertiesSystem` for UI panels.

### Specialized Editors
- **Code Editors**: `AssemblyEditor` (a full-featured text editor) and `MemoryEditor` (a hex viewer).
- **Graphics Editors**: `GraphicsEditor`, `PaletteEditor`, `GfxGroupEditor`, `ScreenEditor`, and the highly detailed `Tile16Editor` provide tools for all visual assets.
- **Content Editors**: `SpriteEditor`, `MessageEditor`, and `MusicEditor` manage specific game content.

### System Components
Located in `src/app/editor/system/`, these components provide the core application framework:
- `SettingsEditor`: Manages global and project-specific feature flags.
- `PopupManager`, `ToastManager`: Handle all UI dialogs and notifications.
- `ShortcutManager`, `CommandManager`: Manage keyboard shortcuts and command palette functionality.
- `ProposalDrawer`, `AgentChatWidget`: Key UI components for the AI Agent Workflow, allowing for proposal review and conversational interaction.

## Game Data Models (`zelda3` Namespace)

The logic and data structures for ALTTP are primarily located in `src/zelda3/`.

- **`Rom` Class (`app/rom.h`)**: This is the most critical data class. It holds the entire ROM content in a `std::vector<uint8_t>` and provides the central API for all data access.
    - **Responsibilities**: Handles loading/saving ROM files, stripping SMC headers, and providing low-level read/write primitives (e.g., `ReadByte`, `WriteWord`).
    - **Game-Specific Loading**: The `LoadZelda3` method populates game-specific data structures like palettes (`palette_groups_`) and graphics groups.
    - **State**: Manages a `dirty_` flag to track unsaved changes.

- **Overworld Model (`zelda3/overworld/`)**:
    - `Overworld`: The main container class that orchestrates the loading of all overworld data, including maps, tiles, and entities. It correctly handles logic for both vanilla and `ZSCustomOverworld` ROMs.
    - `OverworldMap`: Represents a single overworld screen, loading its own properties (palettes, graphics, music) based on the ROM version.
    - `GameEntity`: A base class in `zelda3/common.h` for all interactive overworld elements like `OverworldEntrance`, `OverworldExit`, `OverworldItem`, and `Sprite`.

- **Dungeon Model (`zelda3/dungeon/`)**:
    - `DungeonEditorSystem`: A high-level API that serves as the backend for the UI, managing all dungeon editing logic (adding/removing sprites, items, doors, etc.).
    - `Room`: Represents a single dungeon room, containing its objects, sprites, layout, and header information.
    - `RoomObject` & `RoomLayout`: Define the structural elements of a room.
    - `ObjectParser` & `ObjectRenderer`: High-performance components for directly parsing object data from the ROM and rendering them, avoiding the need for full SNES emulation.

- **Sprite Model (`zelda3/sprite/`)**:
    - `Sprite`: Represents an individual sprite (enemy, NPC).
    - `SpriteBuilder`: A fluent API for programmatically constructing custom sprites.
    - `zsprite.h`: Data structures for compatibility with Zarby's ZSpriteMaker format.

- **Other Data Models**:
    - `MessageData` (`message/`): Handles the game's text and dialogue system.
    - `Inventory`, `TitleScreen`, `DungeonMap` (`screen/`): Represent specific non-gameplay screens.
    - `music::Tracker` (`music/`): Contains legacy code from Hyrule Magic for handling SNES music data.

## Graphics System (`gfx` Namespace)

The `gfx` namespace contains a highly optimized graphics engine tailored for SNES ROM hacking.

- **Core Concepts**:
    - **`Bitmap`**: The fundamental class for image data. It supports SNES pixel formats, palette management, and is optimized with features like dirty-region tracking and a hash-map-based palette lookup cache for O(1) performance.
    - **SNES Formats**: `snes_color`, `snes_palette`, and `snes_tile` provide structures and conversion functions for handling SNES-specific data (15-bit color, 4BPP/8BPP tiles, etc.).
    - **`Tilemap`**: Represents a collection of tiles, using a texture `atlas` and a `TileCache` (with LRU eviction) for efficient rendering.

- **Resource Management & Performance**:
    - **`Arena`**: A singleton that manages all graphics resources. It pools `SDL_Texture` and `SDL_Surface` objects to reduce allocation overhead and uses custom deleters for automatic cleanup. It also manages the 223 global graphics sheets for the game.
    - **`MemoryPool`**: A low-level, high-performance memory allocator that provides pre-allocated blocks for common graphics sizes, reducing `malloc` overhead and memory fragmentation.
    - **`AtlasRenderer`**: A key performance component that batches draw calls by combining multiple smaller graphics onto a single large texture atlas.
    - **Batching**: The `Arena` supports batching texture updates via `QueueTextureUpdate`, which minimizes expensive, blocking calls to the SDL rendering API.

- **Format Handling & Optimization**:
    - **`compression.h`**: Contains SNES-specific compression algorithms (LC-LZ2, Hyrule Magic) for handling graphics data from the ROM.
    - **`BppFormatManager`**: A system for analyzing and converting between different bits-per-pixel (BPP) formats.
    - **`GraphicsOptimizer`**: A high-level tool that uses the `BppFormatManager` to analyze graphics sheets and recommend memory and performance optimizations.
    - **`scad_format.h`**: Provides compatibility with legacy Nintendo CAD file formats (CGX, SCR, COL) from the "gigaleak".

- **Performance Monitoring**:
    - **`PerformanceProfiler`**: A comprehensive system for timing operations using a `ScopedTimer` RAII class.
    - **`PerformanceDashboard`**: An ImGui-based UI for visualizing real-time performance metrics collected by the profiler.

## GUI System (`gui` Namespace)

The `yaze` user interface is built with **ImGui** and is located in `src/app/gui`. It features a modern, component-based architecture designed for modularity, performance, and testability.

### Canvas System (`gui::Canvas`)
The canvas is the core of all visual editors. The main `Canvas` class (`src/app/gui/canvas.h`) has been refactored from a monolithic class into a coordinator that leverages a set of single-responsibility components found in `src/app/gui/canvas/`.

- **`Canvas`**: The main canvas widget. It provides a modern, ImGui-style interface (`Begin`/`End`) and coordinates the various sub-components for drawing, interaction, and configuration.
- **`CanvasInteractionHandler`**: Manages all direct user input on the canvas, such as mouse clicks, drags, and selections for tile painting and object manipulation.
- **`CanvasContextMenu`**: A powerful, data-driven context menu system. It is aware of the current `CanvasUsage` mode (e.g., `TilePainting`, `PaletteEditing`) and displays relevant menu items dynamically.
- **`CanvasModals`**: Handles all modal dialogs related to the canvas, such as "Advanced Properties," "Scaling Controls," and "BPP Conversion," ensuring a consistent UX.
- **`CanvasUsageTracker` & `CanvasPerformanceIntegration`**: These components provide deep analytics and performance monitoring. They track user interactions, operation timings, and memory usage, integrating with the global `PerformanceDashboard` to identify bottlenecks.
- **`CanvasUtils`**: A collection of stateless helper functions for common canvas tasks like grid drawing, coordinate alignment, and size calculations, promoting code reuse.

### Theming and Styling
The visual appearance of the editor is highly customizable through a robust theming system.

- **`ThemeManager`**: A singleton that manages `EnhancedTheme` objects. It can load custom themes from `.theme` files, allowing users to personalize the editor's look and feel. It includes a built-in theme editor and selector UI.
- **`BackgroundRenderer`**: Renders the animated, futuristic grid background for the main docking space, providing a polished, modern aesthetic.
- **`style.cc` & `color.cc`**: Contain custom ImGui styling functions (`ColorsYaze`), `SnesColor` conversion utilities, and other helpers to maintain a consistent visual identity.

### Specialized UI Components
The `gui` namespace includes several powerful, self-contained widgets for specific ROM hacking tasks.

- **`BppFormatUI`**: A comprehensive UI for managing SNES bits-per-pixel (BPP) graphics formats. It provides a format selector, a detailed analysis panel, a side-by-side conversion preview, and a batch comparison tool.
- **`EnhancedPaletteEditor`**: An advanced tool for editing `SnesPalette` objects. It features a grid-based editor, a ROM palette manager for loading palettes directly from the game, and a color analysis view to inspect pixel distribution.
- **`TextEditor`**: A full-featured text editor widget with syntax highlighting for 65816 assembly, undo/redo functionality, and standard text manipulation features.
- **`AssetBrowser`**: A flexible, icon-based browser for viewing and managing game assets, such as graphics sheets.

### Widget Registry for Automation
A key feature for test automation and AI agent integration is the discoverability of UI elements.

- **`WidgetIdRegistry`**: A singleton that catalogs all registered GUI widgets. It assigns a stable, hierarchical path (e.g., `Overworld/Canvas/Map`) to each widget, making the UI programmatically discoverable. This is the backbone of the `z3ed agent test` command.
- **`WidgetIdScope`**: An RAII helper that simplifies the creation of hierarchical widget IDs by managing an ID stack, ensuring that widget paths are consistent and predictable.

## ROM Hacking Context

- **Game**: The Legend of Zelda: A Link to the Past (US/JP).
- **`ZSCustomOverworld`**: A popular system for expanding overworld editing capabilities. `yaze` is designed to be fully compatible with ZScream's implementation of v2 and v3.
- **Assembly**: Uses `asar` for 65816 assembly. A style guide is available at `docs/E1-asm-style-guide.md`.
- **`usdasm` Disassembly**: The user has a local copy of the `usdasm` ALTTP disassembly at `/Users/scawful/Code/usdasm` which can be used for reference.

## Git Workflow

The project follows a simplified Git workflow for pre-1.0 development, with a more formal process documented for the future. For details, see `docs/B4-git-workflow.md`.

- **Current (Pre-1.0)**: A relaxed model is in use. Direct commits to `develop` or `master` are acceptable for documentation and small fixes. Feature branches are used for larger, potentially breaking changes.
- **Future (Post-1.0)**: The project will adopt a formal Git Flow model with `master`, `develop`, feature, release, and hotfix branches.

## AI Agent Workflow (`z3ed agent`)

A primary focus of the `yaze` project is its AI-driven agentic workflow, orchestrated by the `z3ed` CLI.

- **Vision**: To create a conversational ROM hacking assistant that can inspect the ROM and perform edits based on natural language.
- **Core Loop (MCP)**:
    1.  **Model (Plan)**: The user provides a prompt. The agent uses an LLM (Ollama or Gemini) to create a plan, which is a sequence of `z3ed` commands.
    2.  **Code (Generate)**: The LLM generates the commands based on a machine-readable catalog of the CLI's capabilities.
    3.  **Program (Execute)**: The `z3ed` agent executes the commands.
- **Proposal System**: To ensure safety, agent-driven changes are not applied directly. They are executed in a **sandboxed ROM copy** and saved as a **proposal**.
- **Review & Acceptance**: The user can review the proposed changes via `z3ed agent diff` or a dedicated `ProposalDrawer` in the `yaze` GUI. The user must explicitly **accept** a proposal to merge the changes into the main ROM.
- **Tool Use**: The agent can use read-only `z3ed` commands (e.g., `overworld-find-tile`, `dungeon-list-sprites`) as "tools" to inspect the ROM and gather context to answer questions or formulate a plan.
- **API Discovery**: The agent learns the available commands and their schemas by calling `z3ed agent describe`, which exports the entire CLI surface area in a machine-readable format.
- **Function Schemas**: The Gemini AI service uses function calling schemas defined in `assets/agent/function_schemas.json`. These schemas are automatically copied to the build directory and loaded at runtime. To modify the available functions, edit this JSON file rather than hardcoding them in the C++ source.
