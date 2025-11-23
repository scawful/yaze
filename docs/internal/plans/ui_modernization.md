# UI Modernization & Architecture Plan

## Overview
This document outlines the standard for UI development in `yaze`, focusing on the transition to a component-based architecture and full utilization of ImGui Docking.

## Core Architecture

### 1. The "Modern Editor" Standard
New editors should follow the pattern established by `DungeonEditorV2`.

**Key Characteristics:**
- **Component-Based**: The Editor class acts as a coordinator. Logic is delegated to specialized components (e.g., `RoomSelector`, `CanvasViewer`).
- **Dependency Injection**: Use `EditorDependencies` struct for passing core systems (`Rom`, `EditorCardRegistry`, `Renderer`).
- **ImGui Docking**: Use `ImGuiWindowClass` to group related windows (e.g., all Dungeon Editor tool windows dock together).
- **No "Mega-Functions"**: Avoid massive `Draw()` methods. Each component handles its own drawing.

### 2. Window Management
- **DockSpace**: The main application DockSpace is managed by `Controller` and `DockSpaceRenderer`.
- **Editor Windows**: Editors should create their own top-level windows using `ImGui::Begin()` with appropriate flags.
- **Card System**: Use `EditorCardRegistry` for auxiliary tool windows (e.g., "Room List", "Object Properties"). This allows users to toggle them via the "View" menu or Sidebar.

### 3. UI Coordinator
`UICoordinator` is the central hub for application-level UI.
- **Responsibilities**:
    - Drawing global UI (Command Palette, Welcome Screen, Dialogs).
    - Managing global popups.
    - coordinating focus between editors.
- **Future Goal**: Move the main DockSpace creation from `Controller` to `UICoordinator` to centralize all UI logic.

## Immediate Improvements (Implemented)

### 1. Fix DockSpace Lifecycle
`Controller::OnLoad` was missing the call to `DockSpaceRenderer::EndEnhancedDockSpace()`. This has been corrected to ensure proper cleanup and potential future post-processing effects.

### 2. Branch Organization
Unstaged changes have been analyzed and a plan for organizing them into feature branches has been created (`docs/internal/plans/branch_organization.md`).

## Future Work

### 1. Centralize Main Window Logic
Move the "DockSpaceWindow" creation from `Controller` to `UICoordinator::BeginFrame()`. This will allow `Controller` to remain agnostic of the specific UI implementation details.

### 2. Standardize Editor Flags
Create a helper method `Editor::BeginWindow(const char* name, bool* p_open, ImGuiWindowFlags flags)` that automatically applies standard flags (like `ImGuiWindowFlags_UnsavedDocument` if dirty).

### 3. Visual Polish
- **Background**: Enhance `DockSpaceRenderer` to support more dynamic backgrounds (currently supports grid/gradient).
- **Theming**: Fully utilize `ThemeManager` for all new components. Avoid hardcoded colors.

## Migration Guide for Legacy Editors
To convert a legacy editor (e.g., `GraphicsEditor`) to the new system:
1.  Identify distinct functional areas (e.g., "Tile Viewer", "Palette Selector").
2.  Extract these into separate classes/components.
3.  Update the main Editor class to initialize and update these components.
4.  Register the components as "Cards" in `EditorCardRegistry`.
5.  Remove the monolithic `Draw()` method.
