YAZE `zelda3` Library Refactoring & Migration Plan

Author: Gemini
Date: 2025-10-11
Status: Proposed

1. Introduction & Motivation

The zelda3 library, currently located at src/app/zelda3, encapsulates all the data models and logic specific to "A Link
to the Past." It serves as the foundational data layer for both the yaze GUI application and the z3ed command-line tool.

Its current structure and location present two primary challenges:

  1. Monolithic Design: Like the gfx and gui libraries, zelda3 is a single, large static library. This creates a
    tightly-coupled module where a change to any single component (e.g., dungeon objects) forces a relink of the entire
    library and all its dependents.
  2. Incorrect Location: The library resides within src/app/, which is designated for the GUI application's specific code.
    However, its logic is shared with the cli target. This violates architectural principles and creates an improper
    dependency from the cli module into the app module's subdirectory.

This document proposes a comprehensive plan to both refactor the zelda3 library into logical sub-modules and migrate it
to a top-level directory (src/zelda3) to correctly establish it as a shared, core component.

2. Goals

  * Establish as a Core Shared Library: Physically and logically move the library to src/zelda3 to reflect its role as a
    foundational component for both the application and the CLI.
  * Improve Incremental Build Times: Decompose the library into smaller, focused modules to minimize the scope of rebuilds
    and relinks.
  * Clarify Domain Boundaries: Create a clear separation between the major game systems (Overworld, Dungeon, Sprites, etc.)
    to improve code organization and maintainability.
  * Isolate Legacy Code: Encapsulate the legacy Hyrule Magic music tracker code into its own module to separate it from the
    modern C++ codebase.

3. Proposed Architecture

The zelda3 library will be moved to src/zelda3/ and broken down into six distinct, layered libraries.

```
  1 +-----------------------------------------------------------------+
  2 | Executables (yaze, z3ed, tests)                                 |
  3 +-----------------------------------------------------------------+
  4       ^
  5       | Links against
  6       v
  7 +-----------------------------------------------------------------+
  8 | zelda3 (INTERFACE Library)                                      |
  9 +-----------------------------------------------------------------+
  10       ^
  11       | Aggregates
  12       |-----------------------------------------------------------|
  13       |                           |                               |
  14       v                           v                               v
  15 +-----------------+   +-----------------+   +---------------------+
  16 | zelda3_screen   |-->| zelda3_dungeon  |-->| zelda3_overworld    |
  17 +-----------------+   +-----------------+   +---------------------+
  18       |                 |         ^         |         ^
  19       |                 |         |         |         |
  20       |-----------------|---------|---------|---------|
  21       |                 |         |         |         |
  22       v                 v         |         v         v
  23 +-----------------+   +-----------------+   +---------------------+
  24 | zelda3_music    |-->| zelda3_sprite   |-->| zelda3_core         |
  25 +-----------------+   +-----------------+   +---------------------+
```

3.1. zelda3_core (Foundation)
  * Responsibility: Contains fundamental data structures, constants, and labels used across all other zelda3 modules.
  * Contents: common.h, zelda3_labels.h/.cc, dungeon/dungeon_rom_addresses.h.
  * Dependencies: yaze_util.

3.2. zelda3_sprite (Shared Game Entity)
  * Responsibility: Manages the logic and data for sprites, which are used in both dungeons and the overworld.
  * Contents: sprite/sprite.h/.cc, sprite/sprite_builder.h/.cc, sprite/overlord.h.
  * Dependencies: zelda3_core.

3.3. zelda3_dungeon (Dungeon System)
  * Responsibility: The complete, self-contained system for all dungeon-related data and logic.
  * Contents: All files from dungeon/ (room.h/.cc, room_object.h/.cc, dungeon_editor_system.h/.cc, etc.).
  * Dependencies: zelda3_core, zelda3_sprite.

3.4. zelda3_overworld (Overworld System)
  * Responsibility: The complete, self-contained system for all overworld-related data and logic.
  * Contents: All files from overworld/ (overworld.h/.cc, overworld_map.h/.cc, etc.).
  * Dependencies: zelda3_core, zelda3_sprite.

3.5. zelda3_screen (Specific Game Screens)
  * Responsibility: High-level components representing specific, non-gameplay screens.
  * Contents: All files from screen/ (dungeon_map.h/.cc, inventory.h/.cc, title_screen.h/.cc).
  * Dependencies: zelda3_dungeon, zelda3_overworld.

3.6. zelda3_music (Legacy Isolation)
  * Responsibility: Encapsulates the legacy Hyrule Magic music tracker code.
  * Contents: music/tracker.h/.cc.
  * Dependencies: zelda3_core.

4. Migration Plan

This plan details the steps to move the library from src/app/zelda3 to src/zelda3.

  1. Physical File Move:
      * Move the directory /Users/scawful/Code/yaze/src/app/zelda3 to /Users/scawful/Code/yaze/src/zelda3.

  2. Update CMake Configuration:
      * In src/CMakeLists.txt, change the line include(zelda3/zelda3_library.cmake) to
        include(zelda3/zelda3_library.cmake).
      * In the newly moved src/zelda3/zelda3_library.cmake, update all target_include_directories paths to remove the app/
        prefix (e.g., change ${CMAKE_SOURCE_DIR}/src/app to ${CMAKE_SOURCE_DIR}/src).

  3. Update Include Directives (Global):
      * Perform a project-wide search-and-replace for all occurrences of #include "zelda3/ and change them to #include
        "zelda3/.
      * This will be the most extensive step, touching files in src/app/, src/cli/, and test/.

  4. Verification:
      * After the changes, run a full CMake configure and build (cmake --preset mac-dev -B build_ai && cmake --build
        build_ai) to ensure all paths are correctly resolved and the project compiles successfully.

5. Implementation Plan (CMake)

The refactoring will be implemented within the new src/zelda3/zelda3_library.cmake file.

  1. Define Source Groups: Create set() commands for each new library (ZELDA3_CORE_SRC, ZELDA3_DUNGEON_SRC, etc.).
  2. Create Static Libraries: Use add_library(yaze_zelda3_core STATIC ...) for each module.
  3. Establish Link Dependencies: Use target_link_libraries to define the dependencies outlined in section 3.
  4. Create Aggregate Interface Library: The yaze_zelda3 target will be converted to an INTERFACE library that links against
    all the new sub-libraries, providing a single, convenient link target for yaze_gui, yaze_cli, and the test suites.

6. Expected Outcomes

This refactoring and migration will establish the zelda3 library as a true core component of the application. The result
will be a more logical and maintainable architecture, significantly faster incremental build times, and a clear
separation of concerns that will benefit future development.