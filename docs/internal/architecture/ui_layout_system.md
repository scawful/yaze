# YAZE UI Layout Documentation

This document describes the layout logic for the YAZE editor interface, specifically focusing on the menu bar and sidebar interactions.

## Menu Bar Layout

The main menu bar in `UICoordinator::DrawMenuBarExtras` handles the right-aligned status cluster.

### Right-Aligned Status Cluster
The status cluster in `DrawMenuBarExtras` includes (in order from left to right):
1.  **Version**: `vX.Y.Z` (May be hidden on narrow windows)
2.  **Dirty Indicator**: Warning-colored dot (Visible when ROM has unsaved changes)
3.  **Session Switcher**: Layers icon (Visible when multiple sessions are open, may be hidden on narrow windows)
4.  **Notification Bell**: Bell icon (Always visible - high priority)

### Panel Toggle Buttons
Panel toggle buttons are drawn at the end of the menu bar using screen coordinates:
1.  **Panel Toggles**: Icons for Agent, Proposals, Settings, Properties
2.  **WASM Toggle**: Chevron icon (Visible only in Emscripten builds)

These are positioned using `ImGui::SetCursorScreenPos()` with coordinates calculated from the true viewport (not the dockspace window). This ensures they remain in a fixed position even when panels open/close and the dockspace resizes.

### Button Styling
All menu bar icon buttons use consistent styling via `DrawMenuBarIconButton()`:
- Transparent background
- `SurfaceContainerHigh` color on hover
- `SurfaceContainerHighest` color when active/pressed
- `TextSecondary` color for inactive icons
- `Primary` color for active icons (e.g., when a panel is open)

### Sizing Calculation
The `cluster_width` is calculated dynamically using `GetMenuBarIconButtonWidth()` which accounts for:
- Icon text width (using `ImGui::CalcTextSize`)
- Frame padding (`FramePadding.x * 2`)
- Item spacing between elements (6px)

The number of panel toggle buttons is determined at compile time:
- With `YAZE_WITH_GRPC`: 4 buttons (Agent, Proposals, Settings, Properties)
- Without `YAZE_WITH_GRPC`: 3 buttons (Proposals, Settings, Properties)

### Responsive Behavior
When the window is too narrow to display all elements, they are hidden progressively based on priority:
1. **Always shown**: Notification bell, WASM toggle, dirty indicator
2. **High priority**: Version text
3. **Medium priority**: Session switcher button
4. **Low priority**: Panel toggle buttons

The available width is calculated as:
```cpp
float available_width = menu_bar_end - menu_items_end - padding;
```

### Right Panel Interaction
When the Right Panel (Agent, Settings, etc.) is expanded, it occupies the right side of the viewport.

The menubar uses **screen coordinate positioning** for optimal UX:

1. **Fixed Panel Toggles**: Panel toggle buttons are positioned using `ImGui::SetCursorScreenPos()` with coordinates calculated from the true viewport. This keeps them at a fixed screen position regardless of dockspace resizing.

2. **Status Cluster**: Version, dirty indicator, session button, and notification bell are drawn inside the dockspace menu bar using relative positioning. They shift naturally when panels open/close as the dockspace resizes.

```cpp
// Panel toggle screen positioning (in DrawMenuBarExtras)
const ImGuiViewport* viewport = ImGui::GetMainViewport();
float panel_screen_x = viewport->WorkPos.x + viewport->WorkSize.x - panel_region_width;
if (panel_manager->IsPanelExpanded()) {
  panel_screen_x -= panel_manager->GetPanelWidth();
}
ImGui::SetCursorScreenPos(ImVec2(panel_screen_x, menu_bar_y));
```

This ensures users can quickly toggle panels without chasing moving buttons.

## Menu Bar Positioning Patterns

When adding or modifying menu bar elements, choose the appropriate positioning strategy:

### Pattern 1: Relative Positioning (Elements That Shift)

Use standard `ImGui::SameLine()` with window-relative coordinates for elements that should move naturally when the dockspace resizes:

```cpp
const float window_width = ImGui::GetWindowWidth();
float start_pos = window_width - element_width - padding;
ImGui::SameLine(start_pos);
ImGui::Text("Shifting Element");
```

**Use for:** Version text, dirty indicator, session button, notification bell

**Behavior:** These elements shift left when a panel opens (dockspace shrinks)

### Pattern 2: Screen Positioning (Elements That Stay Fixed)

Use `ImGui::SetCursorScreenPos()` with true viewport coordinates for elements that should remain at a fixed screen position:

```cpp
// Get TRUE viewport dimensions (not affected by dockspace resize)
const ImGuiViewport* viewport = ImGui::GetMainViewport();
float screen_x = viewport->WorkPos.x + viewport->WorkSize.x - element_width;

// Adjust for any open panels
if (panel_manager->IsPanelExpanded()) {
  screen_x -= panel_manager->GetPanelWidth();
}

// Keep Y from current menu bar context
float screen_y = ImGui::GetCursorScreenPos().y;

// Position and draw
ImGui::SetCursorScreenPos(ImVec2(screen_x, screen_y));
ImGui::Button("Fixed Element");
```

**Use for:** Panel toggle buttons, any UI that should stay accessible when panels open

**Behavior:** These elements stay at a fixed screen position regardless of dockspace size

### Key Coordinate Functions

| Function | Returns | Use Case |
|----------|---------|----------|
| `ImGui::GetWindowWidth()` | Dockspace window width | Relative positioning within menu bar |
| `ImGui::GetMainViewport()->WorkSize.x` | True viewport width | Fixed screen positioning |
| `ImGui::GetWindowPos()` | Window screen position | Converting between coordinate systems |
| `ImGui::GetCursorScreenPos()` | Current cursor screen position | Getting Y coordinate for screen positioning |
| `ImGui::SetCursorScreenPos()` | N/A (sets position) | Positioning at absolute screen coordinates |

### Common Pitfall

Do NOT use `ImGui::GetWindowWidth()` when calculating fixed positions. The window width changes when panels open/close, causing elements to shift. Always use `ImGui::GetMainViewport()` for fixed positioning.

## Right Panel Styling

### Panel Header
The panel header uses an elevated background (`SurfaceContainerHigh`) with:
- Icon in primary color
- Title in standard text color
- Large close button (28x28) with rounded corners
- Keyboard shortcut: **Escape** closes the panel

### Panel Content Styling
Content uses consistent styling helpers:
- `BeginPanelSection()` / `EndPanelSection()`: Collapsible sections with icons
- `DrawPanelDivider()`: Themed separators
- `DrawPanelLabel()`: Secondary text color labels
- `DrawPanelValue()`: Label + value pairs
- `DrawPanelDescription()`: Wrapped disabled text for descriptions

### Color Scheme
- **Backgrounds**: `SurfaceContainer` for panel, `SurfaceContainerHigh` for sections
- **Borders**: `Outline` color
- **Text**: Primary for titles, Secondary for labels, Disabled for descriptions
- **Accents**: Primary color for icons and active states

## Sidebar Layout

The left sidebar (`EditorCardRegistry`) provides navigation for editor cards.

### Placeholder Sidebar
When no ROM is loaded, `EditorManager::DrawPlaceholderSidebar` renders a placeholder.
- **Theme**: Uses `Surface Container` color for background to distinguish it from the main window.
- **Content**: Displays "Open ROM" and "New Project" buttons.
- **Behavior**: Fills the full height of the viewport work area (below the dockspace menu bar).

### Active Sidebar
When a ROM is loaded, the sidebar displays editor categories and cards.
- **Width**: Fixed width defined in `EditorCardRegistry`.
- **Collapse**: Can be collapsed via the hamburger menu in the menu bar or `Ctrl+B`.
- **Theme**: Matches the placeholder sidebar for consistency.

## Theme Integration
The UI uses `ThemeManager` for consistent colors:
- **Sidebar Background**: `gui::GetSurfaceContainerVec4()`
- **Sidebar Border**: `gui::GetOutlineVec4()`
- **Text**: `gui::GetTextSecondaryVec4()` (for placeholders)
