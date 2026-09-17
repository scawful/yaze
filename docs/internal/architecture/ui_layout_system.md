# YAZE UI Layout Documentation

This document describes the layout logic for the YAZE editor interface, specifically focusing on the menu bar and sidebar interactions.

## Menu Bar Layout

The main menu bar in `UICoordinator::DrawMenuBarExtras` handles the right-aligned status cluster.

### Right-Aligned Status Cluster
The status cluster in `DrawMenuBarExtras` includes (in order from left to right):
1.  **Dirty Indicator**: Warning-colored dot (Visible when ROM has unsaved changes)
2.  **Session Switcher**: Layers icon (Visible when multiple sessions are open, may be hidden on narrow windows)
3.  **Notification Bell**: Bell icon (Always visible - high priority; opens the Notifications drawer)

Version text is **not** shown in the menu bar (see **Help > About**).

### Drawers Overflow Control
A single **Drawers** overflow button (`ICON_MD_VERTICAL_SPLIT`) is drawn at the end of the menu bar using screen coordinates:
1.  **Drawers overflow**: Opens a popup listing every switchable drawer from `GetDrawerCatalog()` (Project, Properties, Agent, Proposals, Notifications, Help, Settings). Active drawer is checked; selecting toggles.
2.  **WASM Toggle**: Chevron icon (Visible only in Emscripten builds)

These are positioned using `ImGui::SetCursorScreenPos()` with coordinates calculated from the true viewport (not the dockspace window). This ensures they remain in a fixed position even when panels open/close and the dockspace resizes.

The open drawer’s header also exposes the same catalog via its panel switcher popup. **View > Drawers** is the text-menu home for the same toggles. Layout presets live under **Windows > Layout** only.

### Button Styling
All menu bar icon buttons use consistent styling via `DrawMenuBarIconButton()`:
- Transparent background
- `SurfaceContainerHigh` color on hover
- `SurfaceContainerHighest` color when active/pressed
- `TextSecondary` color for inactive icons
- `Primary` color for active icons (e.g., when a panel is open)

### Sizing Calculation
Status-cluster button widths use SmallButton metrics (`CalcTextSize` + `FramePadding.x * 2`). The drawers overflow region width comes from `RightDrawerManager::GetDrawerToggleClusterWidth()` so menu-bar reservation stays in sync with the single overflow control (no hardcoded icon count).

### Responsive Behavior
When the window is too narrow to display all elements, they are hidden progressively based on priority:
1. **Always shown**: Notification bell, drawers overflow, WASM toggle
2. **High priority**: Dirty indicator
3. **Medium priority**: Session switcher button

The available width is calculated as:
```cpp
float available_width = menu_bar_end - menu_items_end - padding;
```

### Right Panel Interaction
When the Right Panel (Agent, Settings, etc.) is expanded, it occupies the right side of the viewport.

The menubar uses **screen coordinate positioning** for optimal UX:

1. **Fixed Drawers Overflow**: The Drawers overflow button is positioned using `ImGui::SetCursorScreenPos()` with coordinates calculated from the true viewport. This keeps it at a fixed screen position regardless of dockspace resizing.

2. **Status Cluster**: Dirty indicator, session button, and notification bell are drawn inside the dockspace menu bar using relative positioning. They shift naturally when panels open/close as the dockspace resizes.

```cpp
// Drawers overflow screen positioning (in DrawMenuBarExtras)
const ImGuiViewport* viewport = ImGui::GetMainViewport();
float panel_screen_x = viewport->WorkPos.x + viewport->WorkSize.x - panel_region_width;
if (drawer_manager->IsDrawerExpanded()) {
  panel_screen_x -= drawer_manager->GetDrawerWidth();
}
ImGui::SetCursorScreenPos(ImVec2(panel_screen_x, menu_bar_y));
```

This ensures users can quickly open drawers without chasing a moving button.

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

**Use for:** Dirty indicator, session button, notification bell

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

**Use for:** Drawers overflow button, any UI that should stay accessible when panels open

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

### Panel Header (`DrawPanelHeader`)

The panel header (`SurfaceContainerHigh`, `kPanelHeaderHeight` tall) contains
(left → right):

1. **Icon chip** — 24×24 rounded rect with semi-transparent primary fill, icon
   centred in `GetPrimaryVec4()`.
2. **Title text** — standard `ImGuiCol_Text`.
3. **Context badge** (`DrawHeaderContextBadge`) — type-aware inline widget:
   - `kAgentChat` → green `ICON_MD_CIRCLE` when agent ready
   - `kNotifications` → pill badge with unread count
   - `kProperties` → `ICON_MD_LOCK` in warning color when selection is locked
   - `kHelp` → editor-context tag chip (`Overworld`, `Dungeon`, etc.)
4. **Right-aligned chrome** (right → left): Close (`ICON_MD_CANCEL`), Switcher
   popup (`ICON_MD_SWAP_HORIZ`), panel-specific quick actions (lock toggle for
   Properties; clear/save/proposals for Agent Chat; clear/mark-read for
   Notifications; copy for Tool Output; open-docs for Help).

Keyboard shortcut: **Escape** closes the drawer. Status bar shows a `Drawer`
segment while a right drawer is open.

### Nav Strip (`DrawDrawerNavStrip`)

A 32 px tall icon strip rendered **below** the header, driven by
`GetDrawerCatalog()`:

- Background: `SurfaceContainerVec4`; bottom border: `OutlineVec4`.
- Tab cells are equal-width (`max(24, floor((avail − gaps) / count))`), so the
  strip **always shows all catalog icons** regardless of drawer width (no inline
  tab-vs-popup toggle).
- **Active tab**: `SurfaceContainerHighest` fill + `Primary` underline; icon in
  `Primary`. Clicking the active tab calls `CloseDrawer()`.
- **Inactive hover**: `SurfaceContainerHigh` fill; icon in `TextPrimary`.
- **Inactive rest**: icon in `TextSecondary`.
- `kNotifications` tab shows a 3 px `Primary` dot badge when
  `GetUnreadCount() > 0`.
- Tooltips include the shortcut string from `GetDrawerShortcutAction()` when
  assigned.

All semantic colors (`GetPrimaryVec4`, `GetSuccessVec4`, `GetWarningVec4`,
`GetTextPrimaryVec4`, `GetSurfaceContainerVec4`, etc.) are resolved through
`ThemeManager` — do not hardcode `ImVec4` literals in the nav strip or header.

### Status / context strip

Bottom `StatusBar` orientation (left → right), managed mainly by `EditorManager`:

1. **ROM** filename (+ warning dot when ROM buffer dirty)
2. **Dirty scope** — compact tags from pending work (`Rooms+ROM`, `Project`, …); tooltip uses `DescribePendingUnsavedWork`; click saves ROM or opens Project drawer
3. **Session** — display name when multiple sessions are open
4. **Editor** — active category (`Dungeon`, `Overworld`, …); click opens editor switcher (Ctrl+E)
5. Cursor / selection (event-driven)
6. Editor custom segments (`Room`, `Map`, `Drawer`, …)
7. Right-aligned: build/run / agent / zoom / mode

### Empty states

Use `gui::DrawEmptyState` / presets in `app/gui/widgets/empty_state.h` for
ROM-unloaded, no-selection, no-project, and loading surfaces. Call sites own
optional CTAs (`action_label` / `on_action`); do not invent new "No ROM loaded"
copy in panels.

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

The left chrome is an **ActivityBar** (icon rail) plus an optional **WindowSidebar**
(side panel) owned by `WorkspaceWindowManager`.

### Menu information architecture

- **File**: Open/Save ROM & Project, Settings, Quit
- **View**: Sidebar / Status Bar / Display / Welcome / **Drawers** / Switch Editor
- **Windows**: Window Browser, Show/Hide, Sessions, Layout, Sidebar customize, category panels
- **Tools**: Search/Palette/Finder, Hack Workflows, **ROM Analysis** (info, backup, validate, BPS), Asar, Development
- **Help**: Docs; Keyboard Shortcuts opens the shortcuts UI (not Settings)

### Command palette prefixes

For discoverability in Cmd/Ctrl+Shift+P:

- `drawer: <Name>` — toggle a right drawer from `GetDrawerCatalog()` (`drawer: Next` / `drawer: Previous` cycle)
- `window: <DisplayName>` — open and focus a workspace window, recording it in
  recent windows; used by **Find Window…** (Ctrl+P; shortcut id still `Window Finder`).
  Selecting an already-open window must not close it. Explicit `Show:` / `Hide:` /
  `Toggle:` commands remain available.
- Help → **Keyboard Shortcuts** (Ctrl+Shift+/) opens the searchable shortcuts browser.

### Active side panel (`WindowSidebar`)

When expanded (and not on Dashboard), the side panel shows:

1. Category title + collapse
2. Filter + clear + window actions menu
3. Dungeon only: compact Workbench | Windows toggle
4. **Pinned** section (default open)
5. Grouped sections driven by `WindowDescriptor::workflow_group` via `WindowSidebar::SidebarSectionFor`:
   - **Core** (default open)
   - **Editors** (collapsed; stays collapsed in Dungeon Workbench unless filtering)
   - **Rooms** (Dungeon Window mode; collapsed by default)
   - **Advanced** (collapsed)
   - Other/unknown groups render as their own collapsed sections after Advanced

Non-empty filters show matching rows without section headers; clearing the
filter restores the ordinary section expansion state. `DefaultOpen` alone
cannot override a user's stored collapsed state.

In Dungeon Workbench mode, Room List / Matrix and dynamic room windows are
omitted (`ShouldOmitWindowInSidebar`). Dynamic room IDs have a nonempty decimal
suffix, such as `dungeon.room_51`. Do not hide every `dungeon.room_*` ID:
`dungeon.room_graphics` and `dungeon.room_tags` are standalone editing tools.

### Interaction regression checks

The unit suites exercise drawer tab hit areas at the overflow threshold,
sidebar filtering after a section is collapsed, standalone room-tool access,
Window Finder focus/session ownership, status-chip mouse clicks, and empty-state
button activation. Status-context tests verify callback replacement, not the
full multi-session ROM-save workflow. Native appearance and platform packaging
remain separate acceptance checks.

### Placeholder Sidebar
When no ROM is loaded, `EditorManager::DrawPlaceholderSidebar` renders a placeholder.
- **Theme**: Uses `Surface Container` color for background to distinguish it from the main window.
- **Content**: Displays "Open ROM" and "New Project" buttons.
- **Behavior**: Fills the full height of the viewport work area (below the dockspace menu bar).

### Activity rail
- Category icons with pin/hide/reorder prefs; collapse via hamburger or `Ctrl+B`.
- Bottom More Actions: Command Palette, Shortcuts, Open ROM, Settings.

## Theme Integration
The UI uses `ThemeManager` for consistent colors:
- **Sidebar Background**: `gui::GetSurfaceContainerVec4()`
- **Sidebar Border**: `gui::GetOutlineVec4()`
- **Text**: `gui::GetTextSecondaryVec4()` (for placeholders)
