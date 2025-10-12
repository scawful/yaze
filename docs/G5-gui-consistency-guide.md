# E6 - GUI Consistency and Card-Based Architecture Guide

This guide establishes standards for GUI consistency across all yaze editors, focusing on the modern card-based architecture, theming system, and layout patterns introduced in the Dungeon Editor v2 and Palette Editor refactorings.

## Table of Contents

1. [Introduction](#1-introduction)
2. [Card-Based Architecture](#2-card-based-architecture)
3. [Toolset System](#3-toolset-system)
4. [Themed Widget System](#4-themed-widget-system)
5. [Begin/End Patterns](#5-beginend-patterns)
6. [Layout Helpers](#6-layout-helpers)
7. [Future Editor Improvements](#7-future-editor-improvements)
8. [Migration Checklist](#8-migration-checklist)
9. [Code Examples](#9-code-examples)
10. [Common Pitfalls](#10-common-pitfalls)

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

### Visibility Flag Synchronization

Critical pattern for proper card behavior:

```cpp
absl::Status MyEditor::Update() {
  // For each card, sync visibility flags
  if (show_my_card_ && my_card_instance_) {
    // Ensure internal show_ flag is set
    if (!my_card_instance_->IsVisible()) {
      my_card_instance_->Show();
    }

    my_card_instance_->Draw();

    // Sync back if user closed with X button
    if (!my_card_instance_->IsVisible()) {
      show_my_card_ = false;
    }
  }

  return absl::OkStatus();
}
```

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

## 3. Toolset System

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

## 4. Themed Widget System

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
#include "app/gui/themed_widgets.h"

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

## 5. Begin/End Patterns

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

**Child Window Pattern:**
```cpp
if (ImGui::BeginChild("##ScrollRegion", ImVec2(0, 200), true)) {
  // Scrollable content
  for (int i = 0; i < 100; i++) {
    ImGui::Text("Item %d", i);
  }
}
ImGui::EndChild();
```

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

## 6. Layout Helpers

### Overview

`app/gui/layout_helpers.h` provides utilities for consistent spacing, sizing, and layout across all editors.

### Standard Input Widths

```cpp
#include "app/gui/layout_helpers.h"

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

## 7. Future Editor Improvements

This section outlines specific improvements needed for each editor to achieve GUI consistency.

### Graphics Editor

**Current State:** Tab-based UI with multiple editing modes mixed together

**Needed Improvements:**
1. Remove tab-based UI
2. Create independent cards:
   - `GraphicsSheetCard` - Sheet selection and editing
   - `TitleScreenCard` - Title screen graphics editor
   - `PaletteEditCard` - Integrated palette editing
   - `SheetPropertiesCard` - Sheet metadata and properties
3. Register all cards with `EditorCardManager`
4. Add `Toolset` for mode switching (Draw/Erase/Select)
5. Implement keyboard shortcuts:
   - `Ctrl+Shift+G` - Graphics Control Panel
   - `Ctrl+Alt+1` - Graphics Sheets
   - `Ctrl+Alt+2` - Title Screen
   - `Ctrl+Alt+3` - Palette Editor
   - `Ctrl+Alt+4` - Sheet Properties

**Migration Steps:**
```cpp
// 1. Add visibility flags
bool show_control_panel_ = true;
bool show_graphics_sheets_ = false;
bool show_title_screen_ = false;
bool show_palette_editor_ = false;

// 2. Register in Initialize()
void GraphicsEditor::Initialize() {
  auto& card_manager = gui::EditorCardManager::Get();

  card_manager.RegisterCard({
      .card_id = "graphics.control_panel",
      .display_name = "Graphics Controls",
      .icon = ICON_MD_IMAGE,
      .category = "Graphics",
      .shortcut_hint = "Ctrl+Shift+G",
      .visibility_flag = &show_control_panel_,
      .priority = 10
  });
  // Register other cards...
}

// 3. Create card classes
class GraphicsSheetCard {
public:
  void Draw();
  // ...
};

// 4. Update Update() method to draw cards
absl::Status GraphicsEditor::Update() {
  if (show_control_panel_) {
    DrawControlPanel();
  }

  if (show_graphics_sheets_) {
    DrawGraphicsSheetsCard();
  }
  // Draw other cards...

  return absl::OkStatus();
}
```

### Sprite Editor

**Current State:** Mixed UI with embedded viewers

**Needed Improvements:**
1. Convert to card-based architecture
2. Create independent cards:
   - `SpriteListCard` - Searchable sprite list
   - `SpritePropertiesCard` - Sprite properties editor
   - `SpritePreviewCard` - Visual sprite preview
   - `SpriteAnimationCard` - Animation frame editor
3. Add `Toolset` with sprite type filters
4. Implement keyboard shortcuts:
   - `Ctrl+Shift+S` - Sprite Control Panel
   - `Ctrl+Alt+1` - Sprite List
   - `Ctrl+Alt+2` - Sprite Properties
   - `Ctrl+Alt+3` - Preview Window
   - `Ctrl+Alt+4` - Animation Editor

### Message Editor

**Current State:** Partially card-based, needs consistency

**Needed Improvements:**
1. Unify existing cards with `EditorCardManager`
2. Ensure all cards follow Begin/End pattern
3. Add keyboard shortcuts:
   - `Ctrl+Shift+M` - Message Control Panel
   - `Ctrl+Alt+1` - Message List
   - `Ctrl+Alt+2` - Message Editor
   - `Ctrl+Alt+3` - Font Preview
4. Replace any hardcoded colors with `Themed*` widgets
5. Add `Toolset` for quick message navigation

### Music Editor

**Current State:** Tracker-based UI, needs modernization

**Needed Improvements:**
1. Extract tracker into `TrackerCard`
2. Create additional cards:
   - `InstrumentCard` - Instrument editor
   - `SequenceCard` - Sequence/pattern editor
   - `PlaybackCard` - Playback controls and mixer
3. Add `Toolset` for playback controls
4. Implement keyboard shortcuts:
   - `Ctrl+Shift+U` - Music Control Panel (U for mUsic)
   - `Ctrl+Alt+1` - Tracker
   - `Ctrl+Alt+2` - Instruments
   - `Ctrl+Alt+3` - Sequences
   - `Ctrl+Alt+4` - Playback

### Overworld Editor

**Current State:** Good modular structure, minor improvements needed

**Needed Improvements:**
1. Verify all property panels use `Themed*` widgets
2. Ensure `Toolset` consistency (already has good implementation)
3. Document existing keyboard shortcuts in EditorCardManager
4. Add minimize-to-icon for control panel
5. Consider extracting large panels into separate cards:
   - `MapPropertiesCard` - Currently inline, could be card
   - `TileEditCard` - Tile16 editor as independent card

**Verification Checklist:**
- [x] Uses `Toolset` - Yes
- [ ] All cards registered with `EditorCardManager` - Needs verification
- [ ] No hardcoded colors - Needs audit
- [x] Modular entity renderer - Yes
- [x] Callback-based communication - Yes

## 8. Migration Checklist

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
- [ ] Update E2-development-guide.md editor status if applicable
- [ ] Add example to this guide if pattern is novel
- [ ] Update CLAUDE.md if editor behavior changed significantly

## 9. Code Examples

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

#include "app/gui/editor_card_manager.h"
#include "app/gui/editor_layout.h"
#include "app/gui/icons.h"
#include "app/gui/themed_widgets.h"
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

## 10. Common Pitfalls

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
