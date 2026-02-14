# G5 - GUI Consistency and Panel-Based Architecture Guide

> Legacy note: this guide still includes historical **Card** examples.
> Current implementation standards use **Panel** terminology
> (`PanelDescriptor`, `PanelManager`, `PanelWindow`) and stable ImGui window
> names (`label##panel_id`) for docking/focus.

This guide establishes standards for GUI consistency across all yaze editors, focusing on the modern card-based architecture, theming system, and layout patterns.

## Table of Contents

1. [Introduction](#1-introduction)
2. [Current Conventions (2026-02)](#current-conventions-2026-02)
3. [Card-Based Architecture](#2-card-based-architecture)
4. [VSCode-Style Sidebar System](#3-vscode-style-sidebar-system)
4. [Toolset System](#4-toolset-system)
5. [GUI Library Architecture](#5-gui-library-architecture)
6. [Themed Widget System](#6-themed-widget-system)
7. [Begin/End Patterns](#7-beginend-patterns)
8. [Avoiding Duplicate Rendering](#8-avoiding-duplicate-rendering)
9. [Currently Integrated Editors](#9-currently-integrated-editors)
10. [Layout Helpers](#10-layout-helpers)
11. [Workspace Management](#11-workspace-management)
12. [Future Editor Improvements](#12-future-editor-improvements)
13. [Migration Checklist](#13-migration-checklist)
14. [Code Examples](#14-code-examples)
15. [Common Pitfalls](#15-common-pitfalls)

## 1. Introduction

### Purpose

This guide establishes GUI consistency standards to ensure all editors in yaze provide a unified, modern user experience with maintainable code. The card-based architecture allows editors to present multiple independent windows that can be opened, closed, minimized, and managed independently.

### Benefits

- **User Experience**: Consistent keyboard shortcuts, visual styling, and interaction patterns
- **Maintainability**: Reusable components reduce duplication and bugs
- **Modularity**: Independent cards can be developed and tested separately
- **Flexibility**: Users can arrange their workspace as needed
- **Discoverability**: Central EditorCardManager makes all features accessible

### Target Audience

Contributors working on:
- New editor implementations
- Refactoring existing editors
- Adding new UI features to editors
- Improving user experience consistency

## Current Conventions (2026-02)

These conventions override older `EditorCardManager`-centric examples when they disagree.

1. **Panel window identity is stable and explicit**
   - Register/resolve windows through `PanelDescriptor::GetImGuiWindowName()` and
     `PanelManager::GetPanelWindowName(...)`.
   - Docking and focus logic (`LayoutManager`, `LayoutOrchestrator`, activity bar,
     panel browser) must use this resolved name, not raw display labels.
2. **Sidebar sizing is user-owned and persisted**
   - Left activity side panel is resizable with a drag handle and persisted as
     `sidebar.panel_width` / `sidebar_panel_width`.
   - Right sidebar is resizable per panel type with panel-specific min/max limits;
     widths persist in `layouts.right_panel_widths`.
3. **Panel browser UX should mirror IDE behavior**
   - Use a category navigation pane plus panel table/content area.
   - Keep the category pane width draggable (splitter + `ResizeEW` cursor) and
     expose batch visibility actions (show/hide current scope).
4. **Agent integrations should route through UI actions**
   - Use `UIActionRequestEvent` actions (`kShowAgentChatSidebar`,
     `kShowAgentProposalsSidebar`) to open contextual sidebars from activity bar
     controls and agent quick actions.
5. **Prefer declarative panel metadata over ad-hoc registration**
   - Register panels through `PanelHost` + `PanelDefinition` so visibility,
     labels, aliases, and callbacks stay centralized.
6. **Use layout profiles for intent-driven workspace switches**
   - Prefer built-in profiles (`code`, `debug`, `mapping`, `chat`) and temporary
     snapshot capture/restore for workflow pivots instead of custom one-off
     panel toggling sequences.

## 2. Card-Based Architecture

### Philosophy

Modern yaze editors use **independent, modular windows** called "cards" rather than traditional tab-based or fixed-layout UIs. Each card:

- Is a top-level ImGui window (not a child window)
- Can be opened/closed independently via keyboard shortcuts
- Can be minimized to a floating icon
- Registers with `EditorCardManager` for centralized control
- Has its own visibility flag synchronized with the manager

### Core Components

#### EditorCardManager

Central singleton registry for all editor cards across the application.

**Key Features:**
- Global card registration with metadata
- Keyboard shortcut management
- View menu integration
- Workspace preset system
- Programmatic card control

**Registration Example:**
```cpp
void MyEditor::Initialize() {
  auto& card_manager = gui::EditorCardManager::Get();

  card_manager.RegisterCard({
      .card_id = "myeditor.control_panel",
      .display_name = "My Editor Controls",
      .icon = ICON_MD_SETTINGS,
      .category = "MyEditor",
      .shortcut_hint = "Ctrl+Shift+M",
      .visibility_flag = &show_control_panel_,
      .priority = 10
  });

  // Register more cards...
}
```

#### EditorCard

Wrapper class for individual card windows with Begin/End pattern.

**Key Features:**
- Automatic positioning (Right, Left, Bottom, Floating, Free)
- Default size management
- Minimize/maximize support
- Focus management
- Docking control

**Usage Pattern:**
```cpp
void MyEditor::DrawMyCard() {
  gui::EditorCard card("Card Title", ICON_MD_ICON, &show_card_);
  card.SetDefaultSize(400, 300);
  card.SetPosition(gui::EditorCard::Position::Right);

  if (card.Begin(&show_card_)) {
    // Draw card content here
    ImGui::Text("Card content");
  }
  card.End();
}
```

### Centralized Visibility Pattern

All editors now use the centralized visibility system where EditorCardManager owns and manages all visibility bools:

```cpp
// Registration (in Initialize()):
card_manager.RegisterCard({
    .card_id = "music.tracker",
    .display_name = "Music Tracker",
    .icon = ICON_MD_MUSIC_NOTE,
    .category = "Music"
});

// Usage (in Update()):
static gui::EditorCard card("Music Tracker", ICON_MD_MUSIC_NOTE);
if (card.Begin(card_manager.GetVisibilityFlag("music.tracker"))) {
    DrawContent();
    card.End();
}
```

**Benefits:**
- Single source of truth for all card visibility
- No scattered bool members in editor classes
- Automatic X button close functionality
- Consistent behavior across all cards
- Easy to query/modify from anywhere

### Reference Implementations

**Best Examples:**
- `src/app/editor/dungeon/dungeon_editor_v2.cc` - Gold standard implementation
- `src/app/editor/palette/palette_editor.cc` - Recently refactored, clean patterns

**Key Patterns from Dungeon Editor v2:**
- Independent top-level cards (no parent wrapper)
- Control panel with minimize-to-icon
- Toolset integration
- Proper card registration with shortcuts
- Room cards in separate docking class

## 3. VSCode-Style Sidebar System

### Overview

The VSCode-style sidebar provides a unified interface for managing editor cards. It's a fixed 48px sidebar on the left edge with icon-based card toggles.

**Key Features:**
- Fixed position on left edge (48px width)
- Icon-based card toggles
- Category switcher for multi-editor sessions
- Card browser button (Ctrl+Shift+B)
- Collapse button (Ctrl+B)
- Theme-aware styling
- Recent categories stack (last 5 used)

### Usage

Each card-based editor simply calls:

```cpp
void MyEditor::DrawToolset() {
  auto& card_manager = gui::EditorCardManager::Get();
  card_manager.DrawSidebar("MyEditor");
}
```

The sidebar automatically reads from the existing card registry - no per-editor configuration needed.

### Card Browser

Press `Ctrl+Shift+B` to open the card browser:
- Search/filter cards by name
- Category tabs
- Visibility toggle for all cards
- Statistics (total/visible cards)
- Preset management
- Batch operations (Show All, Hide All per category)

## 4. Toolset System

### Overview

`gui::Toolset` provides an ultra-compact toolbar that merges mode buttons with inline settings. It's designed for minimal vertical space usage while maximizing functionality.

**Design Philosophy:**
- Single horizontal bar with everything inline
- Small icon-only buttons for modes
- Inline property editing (InputHex with scroll)
- Vertical separators for visual grouping
- No wasted space

### Basic Usage

```cpp
void MyEditor::DrawToolset() {
  static gui::Toolset toolbar;
  toolbar.Begin();

  // Add toggle buttons for cards
  if (toolbar.AddToggle(ICON_MD_LIST, &show_list_, "Show List (Ctrl+1)")) {
    // Optional: React to toggle
  }

  if (toolbar.AddToggle(ICON_MD_GRID_VIEW, &show_grid_, "Show Grid (Ctrl+2)")) {
    // Toggled
  }

  toolbar.AddSeparator();

  // Add action buttons
  if (toolbar.AddAction(ICON_MD_SAVE, "Save All")) {
    SaveAllChanges();
  }

  if (toolbar.AddAction(ICON_MD_REFRESH, "Reload")) {
    ReloadData();
  }

  toolbar.End();
}
```

### Advanced Features

**Inline Property Editing:**
```cpp
// Hex properties with scroll wheel support
toolbar.AddProperty(ICON_MD_PALETTE, "Palette", &palette_id_,
                   []() { OnPaletteChanged(); });

toolbar.AddProperty(ICON_MD_IMAGE, "GFX", &gfx_id_,
                   []() { OnGfxChanged(); });
```

**Mode Button Groups:**
```cpp
toolbar.BeginModeGroup();
bool draw_mode = toolbar.ModeButton(ICON_MD_BRUSH, mode_ == Mode::Draw, "Draw");
bool erase_mode = toolbar.ModeButton(ICON_MD_DELETE, mode_ == Mode::Erase, "Erase");
bool select_mode = toolbar.ModeButton(ICON_MD_SELECT, mode_ == Mode::Select, "Select");
toolbar.EndModeGroup();

if (draw_mode) mode_ = Mode::Draw;
if (erase_mode) mode_ = Mode::Erase;
if (select_mode) mode_ = Mode::Select;
```

**Version Badges:**
```cpp
// For ROM version indicators
toolbar.AddRomBadge(rom_->asm_version(), []() {
  ShowUpgradeDialog();
});

toolbar.AddV3StatusBadge(rom_->asm_version(), []() {
  ShowV3Settings();
});
```

### Best Practices

1. **Keep it compact**: Only essential controls belong in the Toolset
2. **Use icons**: Prefer icon-only buttons with tooltips
3. **Group logically**: Use separators to group related controls
4. **Provide shortcuts**: Include keyboard shortcuts in tooltips
5. **Consistent ordering**: Toggles first, properties second, actions third

## 5. GUI Library Architecture

### Modular Library Structure

The yaze GUI is organized into focused, layered libraries for improved build times and maintainability:

**gui_core (Foundation)**
- Theme management, colors, styling
- Icons, input handling, layout helpers
- Dependencies: yaze_util, ImGui, SDL2

**canvas (Core Widget)**
- Canvas widget system
- Canvas utilities, modals, context menus
- Dependencies: gui_core, yaze_gfx

**gui_widgets (Reusable Components)**
- Themed widgets, palette widgets
- Asset browser, text editor, tile selector
- Dependencies: gui_core, yaze_gfx

**gui_automation (Testing & AI)**
- Widget ID registry, auto-registration
- Widget state capture and measurement
- Dependencies: gui_core

**gui_app (Application-Specific UI)**
- EditorCardManager, EditorLayout
- Background renderer, collaboration panel
- Dependencies: gui_core, gui_widgets, gui_automation

**yaze_gui (Interface Library)**
- Aggregates all sub-libraries
- Single link target for executables

### Theme-Aware Sizing System

All UI sizing respects the theme's `compact_factor` (0.8-1.2) for global density control:

```cpp
#include "app/gui/layout_helpers.h"

using gui::LayoutHelpers;

// Standard widget sizing
float widget_height = LayoutHelpers::GetStandardWidgetHeight();
float spacing = LayoutHelpers::GetStandardSpacing();
float toolbar_height = LayoutHelpers::GetToolbarHeight();

// All sizing respects: theme.compact_factor * multiplier * ImGui::GetFontSize()
```

**Layout Helpers API:**
- `BeginTableWithTheming()` - Tables with automatic theme colors
- `BeginCanvasPanel() / EndCanvasPanel()` - Canvas containers
- `BeginPaddedPanel() / EndPaddedPanel()` - Consistent padding
- `InputHexRow()` - Labeled hex inputs
- `BeginPropertyGrid() / EndPropertyGrid()` - 2-column property tables
- `PropertyRow()` - Label + widget in table row
- `SectionHeader()` - Colored section headers
- `HelpMarker()` - Tooltip help icons

## 6. Themed Widget System

### Philosophy

**Never use hardcoded colors.** All UI elements must derive colors from the central theme system to ensure consistency and support for future dark/light theme switching.

### Themed Widget Prefixes

All theme-aware widgets are prefixed with `Themed*`:

**Available Widgets:**
- `ThemedButton()` - Standard button with theme colors
- `ThemedIconButton()` - Icon-only button
- `PrimaryButton()` - Emphasized primary action (e.g., Save)
- `DangerButton()` - Dangerous action (e.g., Delete, Discard)
- `SectionHeader()` - Visual section divider with text

### Usage Examples

```cpp
#include "app/gui/core/themed_widgets.h"

using gui::ThemedButton;
using gui::ThemedIconButton;
using gui::PrimaryButton;
using gui::DangerButton;
using gui::SectionHeader;

void MyCard::DrawContent() {
  SectionHeader("Settings");

  if (PrimaryButton("Save Changes", ImVec2(-1, 0))) {
    SaveToRom();
  }

  if (DangerButton("Discard All", ImVec2(-1, 0))) {
    DiscardChanges();
  }

  ImGui::Separator();

  SectionHeader("Quick Actions");

  if (ThemedIconButton(ICON_MD_REFRESH, "Reload")) {
    Reload();
  }

  ImGui::SameLine();

  if (ThemedIconButton(ICON_MD_COPY, "Duplicate")) {
    Duplicate();
  }
}
```

### Theme Colors

Access theme colors via `AgentUITheme` (despite the name, it's used project-wide):

```cpp
#include "app/editor/agent/agent_ui_theme.h"

void DrawCustomUI() {
  const auto& theme = AgentUI::GetTheme();

  // Use semantic colors
  ImGui::PushStyleColor(ImGuiCol_ChildBg, theme.panel_bg_color);
  ImGui::PushStyleColor(ImGuiCol_Text, theme.text_color);

  // Draw content
  ImGui::BeginChild("MyPanel");
  ImGui::Text("Themed panel content");
  ImGui::EndChild();

  ImGui::PopStyleColor(2);
}
```

**Common Theme Colors:**
- `panel_bg_color` - Background for panels
- `text_color` - Primary text
- `text_dim_color` - Secondary/disabled text
- `accent_color` - Highlights and accents
- `status_success` - Success indicators (green)
- `status_warning` - Warning indicators (yellow)
- `status_error` - Error indicators (red)

### Migration from Hardcoded Colors

**Before (Bad):**
```cpp
ImGui::PushStyleColor(ImGuiCol_ChildBg, ImVec4(0.2f, 0.2f, 0.25f, 1.0f));
ImGui::TextColored(ImVec4(1.0f, 0.0f, 0.0f, 1.0f), "Error!");
```

**After (Good):**
```cpp
const auto& theme = AgentUI::GetTheme();
ImGui::PushStyleColor(ImGuiCol_ChildBg, theme.panel_bg_color);
ImGui::TextColored(theme.status_error, "Error!");
```

### WhichKey Command System

Spacemacs-style hierarchical command navigation:

```cpp
// Press Space → Shows root menu with colored categories
// Press category key (e.g., w) → Shows window submenu
// Press command key → Executes command and closes
// Press ESC → Go back or close
```

**Features:**
- Fixed bottom bar (150px height)
- Color-coded categories
- Breadcrumb navigation ("Space > w")
- Auto-close after 5 seconds of inactivity

**Integration:**
```cpp
command_manager_.RegisterPrefix("w", 'w', "Window", "Window management");
command_manager_.RegisterSubcommand("w", "w.s", 's', "Show All", "Show all windows",
    [this]() { workspace_manager_.ShowAllWindows(); });
```

## 7. Begin/End Patterns

### Philosophy

All resource-managing UI elements use the Begin/End pattern for RAII-style cleanup. This prevents resource leaks and ensures proper ImGui state management.

### EditorCard Begin/End

**Pattern:**
```cpp
void DrawMyCard() {
  gui::EditorCard card("Title", ICON_MD_ICON, &show_flag_);
  card.SetDefaultSize(400, 300);

  // Begin returns false if window is collapsed/hidden
  if (card.Begin(&show_flag_)) {
    // Draw content only when visible
    DrawCardContent();
  }
  // End MUST be called regardless of Begin result
  card.End();
}
```

**Critical Rules:**
1. Always call `End()` even if `Begin()` returns false
2. Put Begin/End calls in same scope for exception safety
3. Check Begin() return value before expensive drawing
4. Pass `p_open` to both constructor and Begin() for proper close button handling

### ImGui Native Begin/End

**Window Pattern:**
```cpp
void DrawWindow() {
  ImGui::SetNextWindowSize(ImVec2(400, 300), ImGuiCond_FirstUseEver);

  if (ImGui::Begin("Window Title", &show_window_)) {
    // Draw window content
    ImGui::Text("Content");
  }
  ImGui::End();  // ALWAYS call, even if Begin returns false
}
```

**Table Pattern:**
```cpp
if (ImGui::BeginTable("##MyTable", 3, ImGuiTableFlags_Borders)) {
  ImGui::TableSetupColumn("Column 1");
  ImGui::TableSetupColumn("Column 2");
  ImGui::TableSetupColumn("Column 3");
  ImGui::TableHeadersRow();

  for (int row = 0; row < 10; row++) {
    ImGui::TableNextRow();
    for (int col = 0; col < 3; col++) {
      ImGui::TableNextColumn();
      ImGui::Text("Cell %d,%d", row, col);
    }
  }

  ImGui::EndTable();
}
```

**Child Window Pattern (CRITICAL):**

⚠️ **This is the most commonly misused pattern.** Unlike tables, `EndChild()` must ALWAYS be called after `BeginChild()`, regardless of the return value.

```cpp
// ✅ CORRECT: EndChild OUTSIDE the if block
if (ImGui::BeginChild("##ScrollRegion", ImVec2(0, 200), true)) {
  // Scrollable content - only drawn when visible
  for (int i = 0; i < 100; i++) {
    ImGui::Text("Item %d", i);
  }
}
ImGui::EndChild();  // ALWAYS called, even if BeginChild returned false

// ❌ WRONG: EndChild INSIDE the if block - causes state corruption!
if (ImGui::BeginChild("##ScrollRegion", ImVec2(0, 200), true)) {
  for (int i = 0; i < 100; i++) {
    ImGui::Text("Item %d", i);
  }
  ImGui::EndChild();  // BUG: Not called when BeginChild returns false!
}
```

**Why this matters:** When `BeginChild()` returns false (child window is clipped or not visible), ImGui still expects `EndChild()` to be called to properly clean up internal state. Failing to call it corrupts ImGui's window stack, which can cause seemingly unrelated errors like table assertions or missing UI elements.

**Pattern Comparison:**

| Function | Call End when Begin returns false? |
|----------|-----------------------------------|
| `BeginChild()` / `EndChild()` | ✅ **YES - ALWAYS** |
| `Begin()` / `End()` (windows) | ✅ **YES - ALWAYS** |
| `BeginTable()` / `EndTable()` | ❌ **NO - only if Begin returned true** |
| `BeginTabBar()` / `EndTabBar()` | ❌ **NO - only if Begin returned true** |
| `BeginTabItem()` / `EndTabItem()` | ❌ **NO - only if Begin returned true** |
| `BeginPopup()` / `EndPopup()` | ❌ **NO - only if Begin returned true** |
| `BeginMenu()` / `EndMenu()` | ❌ **NO - only if Begin returned true** |

### Toolset Begin/End

```cpp
void DrawToolbar() {
  static gui::Toolset toolbar;

  toolbar.Begin();
  // Add toolbar items
  toolbar.AddToggle(ICON_MD_ICON, &flag_, "Tooltip");
  toolbar.End();  // Finalizes layout
}
```

### Error Handling

**With Status Returns:**
```cpp
absl::Status DrawEditor() {
  gui::EditorCard card("Editor", ICON_MD_EDIT);

  if (card.Begin()) {
    RETURN_IF_ERROR(DrawContent());  // Can early return
  }
  card.End();  // Still called via normal flow

  return absl::OkStatus();
}
```

**Exception Safety:**
```cpp
// If exceptions are enabled, use RAII wrappers
struct ScopedCard {
  gui::EditorCard& card;
  explicit ScopedCard(gui::EditorCard& c) : card(c) { card.Begin(); }
  ~ScopedCard() { card.End(); }
};
```

## 8. Avoiding Duplicate Rendering

### Overview

Duplicate rendering occurs when the same UI content is drawn multiple times per frame. This wastes GPU resources and can cause visual glitches, flickering, or assertion errors in ImGui.

### Common Causes

1. **Calling draw functions from multiple places**
2. **Forgetting to check visibility flags**
3. **Shared functions called by different cards**
4. **Rendering in callbacks that fire every frame**

### Pattern 1: Shared Draw Functions

When multiple cards need similar content, don't call the same draw function from multiple places:

```cpp
// ❌ WRONG: DrawMetadata called twice when both cards are visible
void DrawCardA() {
  gui::EditorCard card("Card A", ICON_MD_A);
  if (card.Begin()) {
    DrawCanvas();
    DrawMetadata();  // Called here...
  }
  card.End();
}

void DrawCardB() {
  gui::EditorCard card("Card B", ICON_MD_B);
  if (card.Begin()) {
    DrawMetadata();  // ...AND here! Duplicate!
    DrawCanvas();
  }
  card.End();
}

// ✅ CORRECT: Each card has its own content
void DrawCardA() {
  gui::EditorCard card("Card A", ICON_MD_A);
  if (card.Begin()) {
    DrawCanvas();
    // Card A specific content only
  }
  card.End();
}

void DrawCardB() {
  gui::EditorCard card("Card B", ICON_MD_B);
  if (card.Begin()) {
    DrawMetadata();  // Card B specific content only
  }
  card.End();
}
```

### Pattern 2: Nested Function Calls

Watch out for functions that call other functions with overlapping content:

```cpp
// ❌ WRONG: DrawSpriteCanvas calls DrawMetadata, then DrawCustomSprites
// also calls DrawMetadata AND DrawSpriteCanvas
void DrawSpriteCanvas() {
  // ... canvas code ...
  DrawAnimationFrames();
  DrawCustomSpritesMetadata();  // BUG: This shouldn't be here!
}

void DrawCustomSprites() {
  if (BeginTable(...)) {
    TableNextColumn();
    DrawCustomSpritesMetadata();  // First call
    TableNextColumn();
    DrawSpriteCanvas();           // Calls DrawCustomSpritesMetadata again!
    EndTable();
  }
}

// ✅ CORRECT: Each function has clear, non-overlapping responsibilities
void DrawSpriteCanvas() {
  // ... canvas code ...
  DrawAnimationFrames();
  // NO DrawCustomSpritesMetadata here!
}

void DrawCustomSprites() {
  if (BeginTable(...)) {
    TableNextColumn();
    DrawCustomSpritesMetadata();  // Only place it's called
    TableNextColumn();
    DrawSpriteCanvas();           // Just draws the canvas
    EndTable();
  }
}
```

### Pattern 3: Expensive Per-Frame Operations

Don't call expensive rendering operations every frame unless necessary:

```cpp
// ❌ WRONG: RenderRoomGraphics called every frame when card is visible
void DrawRoomGraphicsCard() {
  if (graphics_card.Begin()) {
    auto& room = rooms_[current_room_id_];
    room.RenderRoomGraphics();  // Expensive! Called every frame!
    DrawRoomGfxCanvas();
  }
  graphics_card.End();
}

// ✅ CORRECT: Only render when room data changes
void DrawRoomGraphicsCard() {
  if (graphics_card.Begin()) {
    auto& room = rooms_[current_room_id_];
    // RenderRoomGraphics is called in DrawRoomTab when room loads
    // or when data changes - NOT every frame here
    DrawRoomGfxCanvas();  // Just displays already-rendered data
  }
  graphics_card.End();
}
```

### Pattern 4: Visibility Flag Checks

Always check visibility before drawing:

```cpp
// ❌ WRONG: Card drawn without visibility check
void Update() {
  DrawMyCard();  // Always called!
}

// ✅ CORRECT: Check visibility first
void Update() {
  if (show_my_card_) {
    DrawMyCard();
  }
}
```

### Debugging Duplicate Rendering

1. **Add logging to draw functions:**
   ```cpp
   void DrawMyContent() {
     LOG_DEBUG("UI", "DrawMyContent called");  // Count calls per frame
     // ...
   }
   ```

2. **Check for multiple card instances:**
   ```cpp
   // Search for multiple cards with similar names
   grep -n "EditorCard.*MyCard" src/app/editor/
   ```

3. **Trace call hierarchy:**
   - Use a debugger or add call stack logging
   - Look for functions that call each other unexpectedly

## 9. Currently Integrated Editors

The card system is integrated across 11 of 13 editors:

| Editor | Cards | Status |
|--------|-------|--------|
| **DungeonEditorV2** | Room selector, canvas, object selector, object editor, entrance editor, tile painter, sprite placer | Complete |
| **PaletteEditor** | Group editor, animation editor, color picker | Complete |
| **GraphicsEditor** | Sheet editor, browser, player animations, prototype viewer | Complete |
| **ScreenEditor** | Dungeon maps, inventory, overworld map, title screen, naming screen | Complete |
| **SpriteEditor** | Vanilla sprites, custom sprites | Complete |
| **OverworldEditor** | Canvas, tile16/tile8 selectors, area graphics, scratch workspace, GFX groups, usage stats, properties, exits, items, sprites, settings | Complete |
| **MessageEditor** | Message list, editor, font atlas, dictionary | Complete |
| **HexEditor** | Hex editor with comparison | Complete |
| **AssemblyEditor** | Assembly editor, file browser | Complete |
| **MusicEditor** | Music tracker, instrument editor, assembly view | Complete |
| **Emulator** | CPU debugger, PPU viewer, memory viewer, breakpoints, performance, AI agent, save states, keyboard config, APU debugger, audio mixer | Complete |

**Not Yet Ported:**
- **SettingsEditor** - Monolithic settings window, low usage frequency
- **AgentEditor** - Complex AI agent UI, under active development

## 10. Layout Helpers

### Overview

`app/gui/layout_helpers.h` provides utilities for consistent spacing, sizing, and layout across all editors.

### Standard Input Widths

```cpp
#include "app/gui/core/layout_helpers.h"

using gui::LayoutHelpers;

void DrawSettings() {
  ImGui::Text("Property:");
  ImGui::SameLine();

  // Standard width for input fields (120px)
  ImGui::SetNextItemWidth(LayoutHelpers::GetStandardInputWidth());
  ImGui::InputInt("##value", &my_value_);
}
```

### Help Markers

```cpp
ImGui::Text("Complex Setting");
ImGui::SameLine();
LayoutHelpers::HelpMarker(
  "This is a detailed explanation of what this setting does. "
  "It appears as a tooltip when hovering the (?) icon."
);
```

### Spacing Utilities

```cpp
// Vertical spacing
LayoutHelpers::VerticalSpacing(10.0f);  // 10px vertical space

// Horizontal spacing
ImGui::SameLine();
LayoutHelpers::HorizontalSpacing(20.0f);  // 20px horizontal space

// Separator with text
LayoutHelpers::SeparatorText("Section Name");
```

### Responsive Layout

```cpp
// Get available width
float available_width = ImGui::GetContentRegionAvail().x;

// Calculate dynamic widths
float button_width = available_width * 0.5f;  // 50% of available

if (ImGui::Button("Full Width", ImVec2(-1, 0))) {
  // -1 = fill available width
}

if (ImGui::Button("Half Width", ImVec2(button_width, 0))) {
  // Fixed to 50%
}
```

### Grid Layouts

```cpp
// Two-column grid
if (ImGui::BeginTable("##Grid", 2, ImGuiTableFlags_SizingStretchSame)) {
  ImGui::TableNextRow();
  ImGui::TableNextColumn();
  ImGui::Text("Label 1:");
  ImGui::TableNextColumn();
  ImGui::InputText("##input1", buffer1, sizeof(buffer1));

  ImGui::TableNextRow();
  ImGui::TableNextColumn();
  ImGui::Text("Label 2:");
  ImGui::TableNextColumn();
  ImGui::InputText("##input2", buffer2, sizeof(buffer2));

  ImGui::EndTable();
}
```

## 11. Workspace Management

The workspace manager provides comprehensive window and layout operations:

**Window Management:**
- `ShowAllWindows() / HideAllWindows()` - via EditorCardManager
- `MaximizeCurrentWindow()` - Dock to central node
- `RestoreAllWindows()` - Reset window sizes
- `CloseAllFloatingWindows()` - Close undocked windows

**Window Navigation:**
- `FocusNextWindow() / FocusPreviousWindow()` - Window cycling
- `SplitWindowHorizontal() / SplitWindowVertical()` - Split current window
- `CloseCurrentWindow()` - Close focused window

**Command Integration:**
```cpp
workspace_manager_.ExecuteWorkspaceCommand(command_id);
// Supports: w.s (show all), w.h (hide all), l.s (save layout), etc.
```

## 12. Future Editor Improvements

This section outlines remaining improvements for editors not yet fully integrated.

### SettingsEditor

**Current State:** Monolithic settings window

**Potential Improvements:**
1. Split into categorized cards
2. Register with EditorCardManager
3. Add search/filter functionality

### AgentEditor

**Current State:** Complex AI agent UI, under active development

**Potential Improvements:**
1. Consider card-based refactoring when stable
2. Integrate with EditorCardManager
3. Add keyboard shortcuts for common operations

## 13. Migration Checklist

Use this checklist when converting an editor to the card-based architecture:

### Planning Phase
- [ ] Identify all major UI components that should become cards
- [ ] Design keyboard shortcut scheme (Ctrl+Alt+[1-9] for cards)
- [ ] Plan `Toolset` contents (toggles, actions, properties)
- [ ] List all hardcoded colors to be replaced

### Implementation Phase - Core Structure
- [ ] Add visibility flags for all cards (e.g., `bool show_my_card_ = false;`)
- [ ] Create `Initialize()` method if not present
- [ ] Register all cards with `EditorCardManager` in `Initialize()`
- [ ] Add card priority values (10, 20, 30, etc.)
- [ ] Include shortcut hints in registration

### Implementation Phase - Toolset
- [ ] Create `DrawToolset()` method
- [ ] Add toggle buttons for each card
- [ ] Include keyboard shortcut hints in tooltips
- [ ] Add separators between logical groups
- [ ] Add action buttons for common operations

### Implementation Phase - Control Panel
- [ ] Create `DrawControlPanel()` method
- [ ] Call `DrawToolset()` at top of control panel
- [ ] Add checkbox grid for quick toggles
- [ ] Add minimize-to-icon button at bottom
- [ ] Include modified status indicators if applicable
- [ ] Add "Save All" / "Discard All" buttons if applicable

### Implementation Phase - Cards
- [ ] Create card classes or Draw methods
- [ ] Use `gui::EditorCard` wrapper with Begin/End
- [ ] Set default size and position for each card
- [ ] Pass visibility flag to both constructor and Begin()
- [ ] Implement proper card content

### Implementation Phase - Update Method
- [ ] Update `Update()` to draw control panel (if visible)
- [ ] Update `Update()` to draw minimize-to-icon (if minimized)
- [ ] Add visibility flag synchronization for each card:
  ```cpp
  if (show_card_ && card_instance_) {
    if (!card_instance_->IsVisible()) card_instance_->Show();
    card_instance_->Draw();
    if (!card_instance_->IsVisible()) show_card_ = false;
  }
  ```

### Implementation Phase - Theming
- [ ] Replace all `ImVec4` color literals with theme colors
- [ ] Use `ThemedButton()` instead of `ImGui::Button()` where appropriate
- [ ] Use `PrimaryButton()` for save/apply actions
- [ ] Use `DangerButton()` for delete/discard actions
- [ ] Use `SectionHeader()` for visual hierarchy
- [ ] Use `ThemedIconButton()` for icon-only buttons

### Testing Phase
- [ ] Test opening each card via control panel checkbox
- [ ] Test opening each card via keyboard shortcut
- [ ] Test closing cards with X button
- [ ] Test minimize-to-icon on control panel
- [ ] Test reopening from icon
- [ ] Verify EditorCardManager shows all cards in View menu
- [ ] Test that closing control panel doesn't affect other cards
- [ ] Verify visibility flags sync properly
- [ ] Test docking behavior (if enabled)
- [ ] Verify all themed widgets render correctly

### Documentation Phase
- [ ] Document keyboard shortcuts in header comment
- [ ] Update `architecture.md` editor status if applicable
- [ ] Add example to this guide if pattern is novel
- [ ] Update CLAUDE.md if editor behavior changed significantly

## 14. Code Examples

### Complete Editor Implementation

This example shows a minimal but complete editor implementation using all the patterns:

```cpp
// my_editor.h
#ifndef YAZE_APP_EDITOR_MY_EDITOR_H
#define YAZE_APP_EDITOR_MY_EDITOR_H

#include "app/editor/editor.h"
#include "app/rom.h"

namespace yaze {
namespace editor {

class MyEditor : public Editor {
 public:
  explicit MyEditor(Rom* rom = nullptr) : rom_(rom) {
    type_ = EditorType::kMyEditor;
  }

  void Initialize() override;
  absl::Status Load() override;
  absl::Status Update() override;

  void set_rom(Rom* rom) { rom_ = rom; }
  Rom* rom() const { return rom_; }

 private:
  void DrawToolset();
  void DrawControlPanel();
  void DrawListCard();
  void DrawPropertiesCard();

  Rom* rom_;

  // Card visibility flags
  bool show_control_panel_ = true;
  bool show_list_card_ = false;
  bool show_properties_card_ = false;
  bool control_panel_minimized_ = false;

  // Data
  int selected_item_ = -1;
};

}  // namespace editor
}  // namespace yaze

#endif  // YAZE_APP_EDITOR_MY_EDITOR_H
```

```cpp
// my_editor.cc
#include "my_editor.h"

#include "app/gui/app/editor_card_manager.h"
#include "app/gui/app/editor_layout.h"
#include "app/gui/core/icons.h"
#include "app/gui/core/themed_widgets.h"
#include "imgui/imgui.h"

namespace yaze {
namespace editor {

void MyEditor::Initialize() {
  auto& card_manager = gui::EditorCardManager::Get();

  card_manager.RegisterCard({
      .card_id = "myeditor.control_panel",
      .display_name = "My Editor Controls",
      .icon = ICON_MD_SETTINGS,
      .category = "MyEditor",
      .shortcut_hint = "Ctrl+Shift+M",
      .visibility_flag = &show_control_panel_,
      .priority = 10
  });

  card_manager.RegisterCard({
      .card_id = "myeditor.list",
      .display_name = "Item List",
      .icon = ICON_MD_LIST,
      .category = "MyEditor",
      .shortcut_hint = "Ctrl+Alt+1",
      .visibility_flag = &show_list_card_,
      .priority = 20
  });

  card_manager.RegisterCard({
      .card_id = "myeditor.properties",
      .display_name = "Properties",
      .icon = ICON_MD_TUNE,
      .category = "MyEditor",
      .shortcut_hint = "Ctrl+Alt+2",
      .visibility_flag = &show_properties_card_,
      .priority = 30
  });
}

absl::Status MyEditor::Load() {
  if (!rom_ || !rom_->is_loaded()) {
    return absl::NotFoundError("ROM not loaded");
  }

  // Load data from ROM
  // ...

  return absl::OkStatus();
}

absl::Status MyEditor::Update() {
  if (!rom_ || !rom_->is_loaded()) {
    gui::EditorCard loading_card("My Editor Loading", ICON_MD_SETTINGS);
    loading_card.SetDefaultSize(400, 200);
    if (loading_card.Begin()) {
      ImGui::Text("Waiting for ROM to load...");
    }
    loading_card.End();
    return absl::OkStatus();
  }

  // Control panel (can be hidden/minimized)
  if (show_control_panel_) {
    DrawControlPanel();
  } else if (control_panel_minimized_) {
    // Minimize-to-icon
    ImGui::SetNextWindowPos(ImVec2(10, 100));
    ImGui::SetNextWindowSize(ImVec2(50, 50));
    ImGuiWindowFlags icon_flags = ImGuiWindowFlags_NoTitleBar |
                                  ImGuiWindowFlags_NoResize |
                                  ImGuiWindowFlags_NoScrollbar |
                                  ImGuiWindowFlags_NoCollapse |
                                  ImGuiWindowFlags_NoDocking;

    if (ImGui::Begin("##MyEditorControlIcon", nullptr, icon_flags)) {
      if (ImGui::Button(ICON_MD_SETTINGS, ImVec2(40, 40))) {
        show_control_panel_ = true;
        control_panel_minimized_ = false;
      }
      if (ImGui::IsItemHovered()) {
        ImGui::SetTooltip("Open My Editor Controls");
      }
    }
    ImGui::End();
  }

  // Independent cards
  if (show_list_card_) {
    DrawListCard();
  }

  if (show_properties_card_) {
    DrawPropertiesCard();
  }

  return absl::OkStatus();
}

void MyEditor::DrawToolset() {
  static gui::Toolset toolbar;
  toolbar.Begin();

  if (toolbar.AddToggle(ICON_MD_LIST, &show_list_card_,
                        "Item List (Ctrl+Alt+1)")) {
    // Toggled
  }

  if (toolbar.AddToggle(ICON_MD_TUNE, &show_properties_card_,
                        "Properties (Ctrl+Alt+2)")) {
    // Toggled
  }

  toolbar.AddSeparator();

  if (toolbar.AddAction(ICON_MD_REFRESH, "Reload")) {
    Load();
  }

  toolbar.End();
}

void MyEditor::DrawControlPanel() {
  using gui::PrimaryButton;
  using gui::DangerButton;

  ImGui::SetNextWindowSize(ImVec2(280, 220), ImGuiCond_FirstUseEver);
  ImGui::SetNextWindowPos(ImVec2(10, 100), ImGuiCond_FirstUseEver);

  if (ImGui::Begin(ICON_MD_SETTINGS " My Editor Controls",
                   &show_control_panel_)) {
    DrawToolset();

    ImGui::Separator();
    ImGui::Text("Quick Toggles:");

    if (ImGui::BeginTable("##QuickToggles", 2,
                          ImGuiTableFlags_SizingStretchSame)) {
      ImGui::TableNextRow();
      ImGui::TableNextColumn();
      ImGui::Checkbox("List", &show_list_card_);

      ImGui::TableNextColumn();
      ImGui::Checkbox("Properties", &show_properties_card_);

      ImGui::EndTable();
    }

    ImGui::Separator();

    if (ImGui::SmallButton(ICON_MD_MINIMIZE " Minimize to Icon")) {
      control_panel_minimized_ = true;
      show_control_panel_ = false;
    }
  }
  ImGui::End();
}

void MyEditor::DrawListCard() {
  gui::EditorCard card("Item List", ICON_MD_LIST, &show_list_card_);
  card.SetDefaultSize(300, 500);
  card.SetPosition(gui::EditorCard::Position::Left);

  if (card.Begin(&show_list_card_)) {
    ImGui::Text("Item List Content");

    if (ImGui::BeginChild("##ItemListScroll", ImVec2(0, 0), true)) {
      for (int i = 0; i < 50; i++) {
        bool is_selected = (selected_item_ == i);
        if (ImGui::Selectable(absl::StrFormat("Item %d", i).c_str(),
                              is_selected)) {
          selected_item_ = i;
        }
      }
    }
    ImGui::EndChild();
  }
  card.End();
}

void MyEditor::DrawPropertiesCard() {
  using gui::ThemedIconButton;
  using gui::SectionHeader;
  using gui::PrimaryButton;

  gui::EditorCard card("Properties", ICON_MD_TUNE, &show_properties_card_);
  card.SetDefaultSize(350, 400);
  card.SetPosition(gui::EditorCard::Position::Right);

  if (card.Begin(&show_properties_card_)) {
    if (selected_item_ < 0) {
      ImGui::TextDisabled("No item selected");
    } else {
      SectionHeader("Item Properties");

      ImGui::Text("Item: %d", selected_item_);

      static char name_buffer[64] = "Item Name";
      ImGui::InputText("Name", name_buffer, sizeof(name_buffer));

      static int value = 100;
      ImGui::InputInt("Value", &value);

      ImGui::Separator();

      if (PrimaryButton("Save Changes", ImVec2(-1, 0))) {
        // Save to ROM
      }

      if (ThemedIconButton(ICON_MD_REFRESH, "Reset to defaults")) {
        // Reset
      }
    }
  }
  card.End();
}

}  // namespace editor
}  // namespace yaze
```

## 15. Common Pitfalls

### 1. Forgetting Bidirectional Visibility Sync

**Problem:** Cards don't reopen after being closed with X button.

**Cause:** Not syncing the visibility flag back when the card is closed.

**Solution:**
```cpp
// WRONG
if (show_my_card_) {
  my_card_->Draw();
}

// CORRECT
if (show_my_card_ && my_card_) {
  if (!my_card_->IsVisible()) my_card_->Show();
  my_card_->Draw();
  if (!my_card_->IsVisible()) show_my_card_ = false;  // Sync back!
}
```

### 2. Using Hardcoded Colors

**Problem:** UI looks inconsistent, doesn't respect theme.

**Cause:** Using `ImVec4` literals instead of theme colors.

**Solution:**
```cpp
// WRONG
ImGui::TextColored(ImVec4(1.0f, 0.0f, 0.0f, 1.0f), "Error!");

// CORRECT
const auto& theme = AgentUI::GetTheme();
ImGui::TextColored(theme.status_error, "Error!");
```

### 3. Not Calling Show() Before Draw()

**Problem:** Newly opened cards don't appear.

**Cause:** The card's internal `show_` flag isn't set when visibility flag changes.

**Solution:**
```cpp
// WRONG
if (show_my_card_) {
  my_card_->Draw();  // Card won't appear if internally hidden
}

// CORRECT
if (show_my_card_) {
  if (!my_card_->IsVisible()) my_card_->Show();  // Ensure visible
  my_card_->Draw();
}
```

### 4. Missing EditorCardManager Registration

**Problem:** Cards don't appear in View menu, shortcuts don't work.

**Cause:** Forgot to register cards in `Initialize()`.

**Solution:**
```cpp
void MyEditor::Initialize() {
  auto& card_manager = gui::EditorCardManager::Get();

  // Register ALL cards
  card_manager.RegisterCard({
      .card_id = "myeditor.my_card",
      .display_name = "My Card",
      .icon = ICON_MD_ICON,
      .category = "MyEditor",
      .shortcut_hint = "Ctrl+Alt+1",
      .visibility_flag = &show_my_card_,
      .priority = 20
  });
}
```

### 5. Improper Begin/End Pairing

**Problem:** ImGui asserts, UI state corruption.

**Cause:** Not calling `End()` when `Begin()` returns false, or early returns.

**Solution:**
```cpp
// WRONG
if (card.Begin()) {
  DrawContent();
  card.End();  // Only called if Begin succeeded
}

// CORRECT
if (card.Begin()) {
  DrawContent();
}
card.End();  // ALWAYS called
```

### 5a. BeginChild/EndChild Mismatch (Most Common Bug!)

**Problem:** `EndTable() call should only be done while in BeginTable() scope` assertion, or other strange ImGui crashes.

**Cause:** `EndChild()` placed inside the if block instead of outside.

**Why it's confusing:** Unlike `BeginTable()`, the `BeginChild()` function requires `EndChild()` to be called regardless of the return value. Many developers assume all Begin/End pairs work the same way.

**Solution:**
```cpp
// ❌ WRONG - EndChild inside if block
void DrawList() {
  if (ImGui::BeginChild("##List", ImVec2(0, 0), true)) {
    for (int i = 0; i < items.size(); i++) {
      ImGui::Selectable(items[i].c_str());
    }
    ImGui::EndChild();  // BUG! Not called when BeginChild returns false!
  }
}

// ✅ CORRECT - EndChild outside if block
void DrawList() {
  if (ImGui::BeginChild("##List", ImVec2(0, 0), true)) {
    for (int i = 0; i < items.size(); i++) {
      ImGui::Selectable(items[i].c_str());
    }
  }
  ImGui::EndChild();  // ALWAYS called!
}
```

**Files where this bug was found and fixed:**
- `sprite_editor.cc` - `DrawSpriteCanvas()`, `DrawSpritesList()`
- `dungeon_editor_v2.cc` - `DrawRoomsListCard()`, `DrawEntrancesListCard()`
- `assembly_editor.cc` - `DrawCurrentFolder()`
- `object_editor_card.cc` - `DrawTemplatesTab()`

### 5b. Duplicate Rendering in Shared Functions

**Problem:** UI elements appear twice, performance degradation, visual glitches.

**Cause:** A draw function is called from multiple places, or a function calls another function that draws the same content.

**Example of the bug:**
```cpp
// DrawSpriteCanvas was calling DrawCustomSpritesMetadata
// DrawCustomSprites was also calling DrawCustomSpritesMetadata AND DrawSpriteCanvas
// Result: DrawCustomSpritesMetadata rendered twice!

void DrawSpriteCanvas() {
  // ... canvas drawing ...
  DrawAnimationFrames();
  DrawCustomSpritesMetadata();  // ❌ BUG: Also called by DrawCustomSprites!
}

void DrawCustomSprites() {
  TableNextColumn();
  DrawCustomSpritesMetadata();  // First call
  TableNextColumn();
  DrawSpriteCanvas();           // ❌ Calls DrawCustomSpritesMetadata AGAIN!
}
```

**Solution:** Each function should have clear, non-overlapping responsibilities:
```cpp
void DrawSpriteCanvas() {
  // ... canvas drawing ...
  DrawAnimationFrames();
  // NO DrawCustomSpritesMetadata here - it belongs in DrawCustomSprites only
}

void DrawCustomSprites() {
  TableNextColumn();
  DrawCustomSpritesMetadata();  // Only place it's called
  TableNextColumn();
  DrawSpriteCanvas();           // Just draws canvas + animations
}
```

### 5c. Expensive Operations Called Every Frame

**Problem:** Low FPS, high CPU usage when certain cards are visible.

**Cause:** Expensive operations like `RenderRoomGraphics()` called unconditionally every frame.

**Solution:**
```cpp
// ❌ WRONG - Renders every frame
void DrawRoomGraphicsCard() {
  if (graphics_card.Begin()) {
    room.RenderRoomGraphics();  // Expensive! Called 60x per second!
    DrawCanvas();
  }
  graphics_card.End();
}

// ✅ CORRECT - Only render when needed
void DrawRoomGraphicsCard() {
  if (graphics_card.Begin()) {
    // RenderRoomGraphics is called in DrawRoomTab when room loads,
    // or when room data changes - NOT every frame
    DrawCanvas();  // Just displays already-rendered data
  }
  graphics_card.End();
}
```

### 6. Not Testing Minimize-to-Icon

**Problem:** Control panel can't be reopened after minimizing.

**Cause:** Forgot to implement the minimize-to-icon floating button.

**Solution:**
```cpp
if (show_control_panel_) {
  DrawControlPanel();
} else if (control_panel_minimized_) {
  // Draw floating icon button
  ImGui::SetNextWindowPos(ImVec2(10, 100));
  ImGui::SetNextWindowSize(ImVec2(50, 50));
  ImGuiWindowFlags flags = ImGuiWindowFlags_NoTitleBar |
                          ImGuiWindowFlags_NoResize |
                          ImGuiWindowFlags_NoScrollbar |
                          ImGuiWindowFlags_NoCollapse |
                          ImGuiWindowFlags_NoDocking;

  if (ImGui::Begin("##ControlIcon", nullptr, flags)) {
    if (ImGui::Button(ICON_MD_SETTINGS, ImVec2(40, 40))) {
      show_control_panel_ = true;
      control_panel_minimized_ = false;
    }
  }
  ImGui::End();
}
```

### 7. Wrong Card Position Enum

**Problem:** Card appears in unexpected location.

**Cause:** Using wrong `Position` enum value.

**Solution:**
```cpp
// Card positions
card.SetPosition(gui::EditorCard::Position::Right);   // Dock to right side
card.SetPosition(gui::EditorCard::Position::Left);    // Dock to left side
card.SetPosition(gui::EditorCard::Position::Bottom);  // Dock to bottom
card.SetPosition(gui::EditorCard::Position::Floating);// Save position
card.SetPosition(gui::EditorCard::Position::Free);    // No positioning
```

### 8. Not Handling Null Rom

**Problem:** Editor crashes when ROM isn't loaded.

**Cause:** Not checking `rom_` before access.

**Solution:**
```cpp
absl::Status MyEditor::Update() {
  if (!rom_ || !rom_->is_loaded()) {
    // Show loading card
    gui::EditorCard loading_card("Editor Loading", ICON_MD_ICON);
    loading_card.SetDefaultSize(400, 200);
    if (loading_card.Begin()) {
      ImGui::Text("Waiting for ROM...");
    }
    loading_card.End();
    return absl::OkStatus();
  }

  // Safe to use rom_ now
  DrawEditor();
  return absl::OkStatus();
}
```

### 9. Forgetting Toolset Begin/End

**Problem:** Toolset items don't render or layout is broken.

**Cause:** Missing `Begin()` or `End()` calls.

**Solution:**
```cpp
void DrawToolset() {
  static gui::Toolset toolbar;

  toolbar.Begin();  // REQUIRED

  toolbar.AddToggle(ICON_MD_ICON, &flag_, "Tooltip");
  toolbar.AddSeparator();
  toolbar.AddAction(ICON_MD_SAVE, "Save");

  toolbar.End();  // REQUIRED
}
```

### 10. Hardcoded Shortcuts in Tooltips

**Problem:** Shortcuts shown in tooltips don't match actual keybinds.

**Cause:** Tooltip string doesn't match `shortcut_hint` in registration.

**Solution:**
```cpp
// In Registration
card_manager.RegisterCard({
    .shortcut_hint = "Ctrl+Alt+1",  // Define once
    // ...
});

// In Toolset
toolbar.AddToggle(ICON_MD_LIST, &show_list_,
                 "Item List (Ctrl+Alt+1)");  // Match exactly
```

---

## Summary

Following this guide ensures:
- **Consistency**: All editors use the same patterns and components
- **Maintainability**: Reusable components reduce code duplication
- **User Experience**: Predictable keyboard shortcuts and visual styling
- **Flexibility**: Independent cards allow custom workspace arrangements
- **Discoverability**: EditorCardManager makes all features accessible

When adding new editors or refactoring existing ones, refer to:
1. **Dungeon Editor v2** (`dungeon_editor_v2.cc`) - Gold standard implementation
2. **Palette Editor** (`palette_editor.cc`) - Recently refactored, clean patterns
3. **This Guide** - Comprehensive reference for all patterns

For questions or suggestions about GUI consistency, please open an issue on GitHub or discuss in the development chat.

---

**Last Updated**: November 26, 2025
