# B7 - Architecture Refactoring Plan

**Date**: October 15, 2025
**Status**: Proposed
**Author**: Gemini AI Assistant

## 1. Overview & Goals

This document outlines a comprehensive refactoring plan for the YAZE architecture. The current structure has resulted in tight coupling between components, slow incremental build times, and architectural inconsistencies (e.g., shared libraries located within the `app/` directory).

The primary goals of this refactoring are:

1.  **Establish a Clear, Layered Architecture**: Separate foundational libraries (`core`, `gfx`, `zelda3`) from the applications that consume them (`app`, `cli`).
2.  **Improve Modularity & Maintainability**: Decompose large, monolithic libraries into smaller, single-responsibility modules.
3.  **Drastically Reduce Build Times**: Minimize rebuild cascades by ensuring changes in one module do not trigger unnecessary rebuilds in unrelated components.
4.  **Enable Future Development**: Create a flexible foundation for new features like alternative rendering backends (SDL3, Metal, Vulkan) and a fully-featured CLI.

## 2. Proposed Target Architecture

The proposed architecture organizes the codebase into two distinct layers: **Foundational Libraries** and **Applications**.

```
/src
├── core/         (NEW) 📖 Project model, Asar wrapper, etc.
├── gfx/          (MOVED) 🎨 Graphics engine, backends, resource management
├── zelda3/       (MOVED) Game Game-specific data models and logic
├── util/         (EXISTING)  Low-level utilities (logging, file I/O)
│
├── app/          (REFACTORED)  Main GUI Application
│   ├── controller.cc   (MOVED)  Main application controller
│   ├── platform/       (MOVED) 윈도우 Windowing, input, platform abstractions
│   ├── service/        (MOVED) AI gRPC services for automation
│   ├── editor/         (EXISTING) 🎨 Editor implementations
│   └── gui/            (EXISTING)  Shared ImGui widgets
│
└── cli/          (EXISTING)  z3ed Command-Line Tool
```

## 3. Detailed Refactoring Plan

This plan will be executed in three main phases.

### Phase 1: Create `yaze_core_lib` (Project & Asar Logic)

This phase establishes a new, top-level library for application-agnostic project management and ROM patching logic.

1.  **Create New Directory**: Create `src/core/`.
2.  **Move Files**:
    *   Move `src/app/core/{project.h, project.cc}` → `src/core/`
    *   Move `src/app/core/{asar_wrapper.h, asar_wrapper.cc}` → `src/core/`
    *   Move `src/app/core/features.h` → `src/core/`
3.  **Update Namespace**: In the moved files, change the namespace from `yaze::core` to `yaze::project` for clarity.
4.  **Create CMake Target**: In a new `src/core/CMakeLists.txt`, define the `yaze_core_lib` static library containing the moved files. This library should have minimal dependencies (e.g., `yaze_util`, `absl`).

### Phase 2: Elevate `yaze_gfx_lib` (Graphics Engine)

This phase decouples the graphics engine from the GUI application, turning it into a foundational, reusable library. This is critical for supporting multiple rendering backends as outlined in `docs/G2-renderer-migration-plan.md`.

1.  **Move Directory**: Move the entire `src/app/gfx/` directory to `src/gfx/`.
2.  **Create CMake Target**: In a new `src/gfx/CMakeLists.txt`, define the `yaze_gfx_lib` static library. This will aggregate all graphics components (`backend`, `core`, `resource`, etc.).
3.  **Update Dependencies**: The `yaze` application target will now explicitly depend on `yaze_gfx_lib`.

### Phase 3: Streamline the `app` Layer

This phase dissolves the ambiguous `src/app/core` directory and simplifies the application's structure.

1.  **Move Service Layer**: Move the `src/app/core/service/` directory to `src/app/service/`. This creates a clear, top-level service layer for gRPC implementations.
2.  **Move Platform Code**: Move `src/app/core/{window.cc, window.h, timing.h}` into the existing `src/app/platform/` directory. This consolidates all platform-specific windowing and input code.
3.  **Elevate Main Controller**: Move `src/app/core/{controller.cc, controller.h}` to `src/app/`. This highlights its role as the primary orchestrator of the GUI application.
4.  **Update CMake**:
    *   Eliminate the `yaze_app_core_lib` target.
    *   Add the source files from the moved directories (`app/controller.cc`, `app/platform/window.cc`, `app/service/*.cc`, etc.) directly to the main `yaze` executable target.

## 4. Alignment with EditorManager Refactoring

This architectural refactoring fully supports and complements the ongoing `EditorManager` improvements detailed in `docs/H2-editor-manager-architecture.md`.

-   The `EditorManager` and its new coordinators (`UICoordinator`, `PopupManager`, `SessionCoordinator`) are clearly components of the **Application Layer**.
-   By moving the foundational libraries (`core`, `gfx`) out of `src/app`, we create a clean boundary. The `EditorManager` and its helpers will reside within `src/app/editor/` and `src/app/editor/system/`, and will consume the new `yaze_core_lib` and `yaze_gfx_lib` as dependencies.
-   This separation makes the `EditorManager`'s role as a UI and session coordinator even clearer, as it no longer lives alongside low-level libraries.

## 5. Migration Checklist

1.  [ ] **Phase 1**: Create `src/core/` and move `project`, `asar_wrapper`, and `features` files.
2.  [ ] **Phase 1**: Create the `yaze_core_lib` CMake target.
3.  [ ] **Phase 2**: Move `src/app/gfx/` to `src/gfx/`.
4.  [ ] **Phase 2**: Create the `yaze_gfx_lib` CMake target.
5.  [ ] **Phase 3**: Move `src/app/core/service/` to `src/app/service/`.
6.  [ ] **Phase 3**: Move `window.cc`, `timing.h` to `src/app/platform/`.
7.  [ ] **Phase 3**: Move `controller.cc` to `src/app/`.
8.  [ ] **Phase 3**: Update the `yaze` executable's CMake target to include the moved sources and link against the new libraries.
9.  [ ] **Phase 3**: Delete the now-empty `src/app/core/` directory and its `core_library.cmake` file.
10. [ ] **Verification**: Perform a clean build of all targets (`yaze`, `z3ed`, `yaze_test`) to ensure all dependencies are correctly resolved.
11. [ ] **Cleanup**: Search the codebase for any remaining `#include "app/core/..."` or `#include "app/gfx/..."` directives and update them to their new paths.

## 6. Expected Benefits

-   **Faster Builds**: Incremental build times are expected to decrease by **40-60%** as changes will be localized to smaller libraries.
-   **Improved Maintainability**: A clear, layered architecture makes the codebase easier to understand, navigate, and extend.
-   **True CLI Decoupling**: The `z3ed` CLI can link against `yaze_core_lib` and `yaze_zelda3_lib` without pulling in any GUI or rendering dependencies, resulting in a smaller, more portable executable.
-   **Future-Proofing**: The abstracted `gfx` library paves the way for supporting SDL3, Metal, or Vulkan backends with minimal disruption to the rest of the application.
