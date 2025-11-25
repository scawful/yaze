# UI Panel System Architecture

**Status:** FUNCTIONAL - UX Polish Needed  
**Owner:** ui-architect  
**Created:** 2025-01-25  
**Last Reviewed:** 2025-01-25  
**Next Review:** 2025-02-08

> ✅ **UPDATE (2025-01-25):** Core rendering issues RESOLVED. Sidebars and panels now render correctly. Remaining work is UX consistency polish for the right panel system (notifications, proposals, settings panels).

---

## Overview

This document describes the UI sidebar, menu, and panel system implemented for YAZE. The system provides a VSCode-inspired layout with:

1. **Left Sidebar** - Editor card toggles (existing `EditorCardRegistry`)
2. **Right Panel System** - Sliding panels for Agent Chat, Proposals, Settings (new `RightPanelManager`)
3. **Menu Bar System** - Reorganized menus with ROM-dependent item states
4. **Status Cluster** - Right-aligned menu bar elements with panel toggles

```
+------------------------------------------------------------------+
|  [≡] File  Edit  View  Tools  Window  Help  [v0.x][●][S][P][🔔]  |
+--------+---------------------------------------------+-----------+
|        |                                             |           |
| LEFT   |                                             |  RIGHT    |
| SIDE   |          MAIN DOCKING SPACE                 |  PANEL    |
| BAR    |          (adjusts with both sidebars)       |           |
| (cards)|                                             | - Agent   |
|        |                                             | - Props   |
|        |                                             | - Settings|
+--------+---------------------------------------------+-----------+
```

---

## Component Architecture

### 1. RightPanelManager (`src/app/editor/ui/right_panel_manager.h/cc`)

**Purpose:** Manages right-side sliding panels for ancillary functionality.

**Key Types:**
```cpp
enum class PanelType {
  kNone = 0,      // No panel open
  kAgentChat,     // AI Agent conversation
  kProposals,     // Agent proposal review
  kSettings,      // Application settings
  kHelp           // Help & documentation
};
```

**Key Methods:**
| Method | Description |
|--------|-------------|
| `TogglePanel(PanelType)` | Toggle specific panel on/off |
| `OpenPanel(PanelType)` | Open specific panel (closes any active) |
| `ClosePanel()` | Close currently active panel |
| `IsPanelExpanded()` | Check if any panel is open |
| `GetPanelWidth()` | Get current panel width for layout offset |
| `Draw()` | Render the panel and its contents |
| `DrawPanelToggleButtons()` | Render toggle buttons for status cluster |

**Panel Widths:**
- Agent Chat: 380px
- Proposals: 420px
- Settings: 480px
- Help: 350px

**Integration Points:**
```cpp
// In EditorManager constructor:
right_panel_manager_ = std::make_unique<RightPanelManager>();
right_panel_manager_->SetToastManager(&toast_manager_);
right_panel_manager_->SetProposalDrawer(&proposal_drawer_);

#ifdef YAZE_WITH_GRPC
right_panel_manager_->SetAgentChatWidget(agent_editor_.GetChatWidget());
#endif
```

**Drawing Flow:**
1. `EditorManager::Update()` calls `right_panel_manager_->Draw()` after sidebar
2. `UICoordinator::DrawMenuBarExtras()` calls `DrawPanelToggleButtons()`
3. Panel positions itself at `(viewport_width - panel_width, menu_bar_height)`

---

### 2. EditorCardRegistry (Sidebar)

**File:** `src/app/editor/system/editor_card_registry.h/cc`

**Key Constants:**
```cpp
static constexpr float GetSidebarWidth() { return 48.0f; }
static constexpr float GetCollapsedSidebarWidth() { return 16.0f; }
```

**Sidebar State:**
```cpp
bool sidebar_collapsed_ = false;

bool IsSidebarCollapsed() const;
void SetSidebarCollapsed(bool collapsed);
void ToggleSidebarCollapsed();
```

**Sidebar Toggle Button (in `EditorManager::DrawMenuBar()`):**
```cpp
// Always visible, icon changes based on state
const char* sidebar_icon = card_registry_.IsSidebarCollapsed()
                               ? ICON_MD_MENU
                               : ICON_MD_MENU_OPEN;

if (ImGui::SmallButton(sidebar_icon)) {
  card_registry_.ToggleSidebarCollapsed();
}
```

---

### 3. Dockspace Layout Adjustment

**File:** `src/app/controller.cc`

The main dockspace adjusts its position and size based on sidebar/panel state:

```cpp
// In Controller::OnLoad()
const float left_offset = editor_manager_.GetLeftLayoutOffset();
const float right_offset = editor_manager_.GetRightLayoutOffset();

ImVec2 dockspace_pos = viewport->WorkPos;
ImVec2 dockspace_size = viewport->WorkSize;

dockspace_pos.x += left_offset;
dockspace_size.x -= (left_offset + right_offset);

ImGui::SetNextWindowPos(dockspace_pos);
ImGui::SetNextWindowSize(dockspace_size);
```

**EditorManager Layout Offset Methods:**
```cpp
// Returns sidebar width when visible and expanded
float GetLeftLayoutOffset() const {
  if (!ui_coordinator_ || !ui_coordinator_->IsCardSidebarVisible()) {
    return 0.0f;
  }
  return card_registry_.IsSidebarCollapsed() ? 0.0f
                                             : EditorCardRegistry::GetSidebarWidth();
}

// Returns right panel width when expanded
float GetRightLayoutOffset() const {
  return right_panel_manager_ ? right_panel_manager_->GetPanelWidth() : 0.0f;
}
```

---

### 4. Menu Bar System

**File:** `src/app/editor/system/menu_orchestrator.cc`

**Menu Structure:**
```
File    - ROM/Project operations
Edit    - Undo/Redo/Cut/Copy/Paste
View    - Editor shortcuts (ROM-dependent), Display settings, Welcome screen
Tools   - Search, Performance, ImGui debug
Window  - Sessions, Layouts, Cards, Panels, Workspace presets
Help    - Documentation links
```

**Key Changes:**
1. **Cards submenu moved from View → Window menu**
2. **Panels submenu added to Window menu**
3. **ROM-dependent items disabled when no ROM loaded**

**ROM-Dependent Item Pattern:**
```cpp
menu_builder_
    .Item(
        "Overworld", ICON_MD_MAP,
        [this]() { OnSwitchToEditor(EditorType::kOverworld); }, "Ctrl+1",
        [this]() { return HasActiveRom(); })  // Enable condition
```

**Cards Submenu (conditional):**
```cpp
if (HasActiveRom()) {
  AddCardsSubmenu();
} else {
  if (ImGui::BeginMenu("Cards")) {
    ImGui::MenuItem("(No ROM loaded)", nullptr, false, false);
    ImGui::EndMenu();
  }
}
```

**Panels Submenu:**
```cpp
void MenuOrchestrator::AddPanelsSubmenu() {
  if (ImGui::BeginMenu("Panels")) {
#ifdef YAZE_WITH_GRPC
    if (ImGui::MenuItem("AI Agent", "Ctrl+Shift+A")) {
      OnShowAIAgent();
    }
#endif
    if (ImGui::MenuItem("Proposals", "Ctrl+Shift+R")) {
      OnShowProposalDrawer();
    }
    if (ImGui::MenuItem("Settings")) {
      OnShowSettings();
    }
    // ...
    ImGui::EndMenu();
  }
}
```

---

### 5. Status Cluster

**File:** `src/app/editor/ui/ui_coordinator.cc`

**Order (left to right):**
```
[version] [●dirty] [📄session] [🤖agent] [📋proposals] [⚙settings] [🔔bell] [⛶fullscreen]
```

**Implementation:**
```cpp
void UICoordinator::DrawMenuBarExtras() {
  // Calculate cluster width for right alignment
  float cluster_width = 280.0f;
  ImGui::SameLine(ImGui::GetWindowWidth() - cluster_width);

  // 1. Version (leftmost)
  ImGui::Text("v%s", version.c_str());
  
  // 2. Dirty badge (when ROM has unsaved changes)
  if (current_rom && current_rom->dirty()) {
    ImGui::Text(ICON_MD_FIBER_MANUAL_RECORD);  // Orange dot
  }
  
  // 3. Session button (when 2+ sessions)
  if (session_coordinator_.HasMultipleSessions()) {
    DrawSessionButton();
  }
  
  // 4. Panel toggle buttons
  editor_manager_->right_panel_manager()->DrawPanelToggleButtons();
  
  // 5. Notification bell (rightmost)
  DrawNotificationBell();
  
#ifdef __EMSCRIPTEN__
  // 6. Menu bar hide button (WASM only)
  if (ImGui::SmallButton(ICON_MD_FULLSCREEN)) {
    show_menu_bar_ = false;
  }
#endif
}
```

---

### 6. WASM Menu Bar Toggle

**Files:** `ui_coordinator.h/cc`, `controller.cc`

**Purpose:** Allow hiding the menu bar for a cleaner web UI experience.

**State:**
```cpp
bool show_menu_bar_ = true;  // Default visible

bool IsMenuBarVisible() const;
void SetMenuBarVisible(bool visible);
void ToggleMenuBar();
```

**Controller Integration:**
```cpp
// In OnLoad()
bool show_menu_bar = true;
if (editor_manager_.ui_coordinator()) {
  show_menu_bar = editor_manager_.ui_coordinator()->IsMenuBarVisible();
}

ImGuiWindowFlags window_flags = ImGuiWindowFlags_NoDocking;
if (show_menu_bar) {
  window_flags |= ImGuiWindowFlags_MenuBar;
}

// ...

if (show_menu_bar) {
  editor_manager_.DrawMenuBar();
}

// Draw restore button when hidden
if (!show_menu_bar && editor_manager_.ui_coordinator()) {
  editor_manager_.ui_coordinator()->DrawMenuBarRestoreButton();
}
```

**Restore Button:**
- Small floating button in top-left corner
- Also responds to Alt key press

---

### 7. Dropdown Popup Positioning

**Pattern for right-anchored popups:**
```cpp
// Store button position before drawing
ImVec2 button_min = ImGui::GetCursorScreenPos();

if (ImGui::SmallButton(icon)) {
  ImGui::OpenPopup("##PopupId");
}

ImVec2 button_max = ImGui::GetItemRectMax();

// Calculate position to prevent overflow
const float popup_width = 320.0f;
const float screen_width = ImGui::GetIO().DisplaySize.x;
const float popup_x = std::min(button_min.x, screen_width - popup_width - 10.0f);

ImGui::SetNextWindowPos(ImVec2(popup_x, button_max.y + 2.0f), ImGuiCond_Appearing);

if (ImGui::BeginPopup("##PopupId")) {
  // Popup contents...
  ImGui::EndPopup();
}
```

---

## Data Flow Diagram

```
┌─────────────────┐
│   Controller    │
│   OnLoad()      │
└────────┬────────┘
         │ GetLeftLayoutOffset()
         │ GetRightLayoutOffset()
         ▼
┌─────────────────┐     ┌──────────────────┐
│  EditorManager  │────▶│ EditorCardRegistry│
│                 │     │ (Left Sidebar)    │
│  DrawMenuBar()  │     └──────────────────┘
│  Update()       │
└────────┬────────┘
         │
         ├──────────────────────┐
         ▼                      ▼
┌─────────────────┐    ┌───────────────────┐
│ MenuOrchestrator│    │  RightPanelManager │
│ BuildMainMenu() │    │  Draw()            │
└────────┬────────┘    └───────────────────┘
         │
         ▼
┌─────────────────┐
│  UICoordinator  │
│DrawMenuBarExtras│
│DrawPanelToggles │
└─────────────────┘
```

---

## Future Improvement Ideas

### High Priority

#### 1. Panel Animation
Currently panels appear/disappear instantly. Add smooth slide-in/out animation:
```cpp
// In RightPanelManager
float target_width_;          // Target panel width
float current_width_ = 0.0f;  // Animated current width
float animation_speed_ = 8.0f;

void UpdateAnimation(float delta_time) {
  float target = (active_panel_ != PanelType::kNone) ? GetPanelWidth() : 0.0f;
  current_width_ = ImLerp(current_width_, target, animation_speed_ * delta_time);
}

float GetAnimatedWidth() const {
  return current_width_;
}
```

#### 2. Panel Resizing
Allow users to drag panel edges to resize:
```cpp
void DrawResizeHandle() {
  ImVec2 handle_pos(panel_x - 4.0f, menu_bar_height);
  ImVec2 handle_size(8.0f, viewport_height);
  
  if (ImGui::InvisibleButton("##PanelResize", handle_size)) {
    resizing_ = true;
  }
  
  if (resizing_ && ImGui::IsMouseDown(0)) {
    float delta = ImGui::GetIO().MouseDelta.x;
    SetPanelWidth(active_panel_, GetPanelWidth() - delta);
  }
}
```

#### 3. Panel Memory/Persistence
Save panel states to user settings:
```cpp
// In UserSettings
struct PanelSettings {
  bool agent_panel_open = false;
  float agent_panel_width = 380.0f;
  bool proposals_panel_open = false;
  float proposals_panel_width = 420.0f;
  // ...
};

// On startup
void RightPanelManager::LoadFromSettings(const PanelSettings& settings);

// On panel state change
void RightPanelManager::SaveToSettings(PanelSettings& settings);
```

#### 4. Multiple Simultaneous Panels
Allow multiple panels to be open side-by-side:
```cpp
// Replace single active_panel_ with:
std::vector<PanelType> open_panels_;

void TogglePanel(PanelType type) {
  auto it = std::find(open_panels_.begin(), open_panels_.end(), type);
  if (it != open_panels_.end()) {
    open_panels_.erase(it);
  } else {
    open_panels_.push_back(type);
  }
}

float GetTotalPanelWidth() const {
  float total = 0.0f;
  for (auto panel : open_panels_) {
    total += GetPanelWidth(panel);
  }
  return total;
}
```

### Medium Priority

#### 5. Keyboard Shortcuts for Panels
Add shortcuts to toggle panels directly:
```cpp
// In ShortcutConfigurator
shortcut_manager_.RegisterShortcut({
    .id = "toggle_agent_panel",
    .keys = {ImGuiMod_Ctrl | ImGuiMod_Shift, ImGuiKey_A},
    .callback = [this]() {
      editor_manager_->right_panel_manager()->TogglePanel(
          RightPanelManager::PanelType::kAgentChat);
    }
});
```

#### 6. Panel Tab Bar
When multiple panels open, show tabs at top:
```cpp
void DrawPanelTabBar() {
  if (ImGui::BeginTabBar("##PanelTabs")) {
    for (auto panel : open_panels_) {
      if (ImGui::BeginTabItem(GetPanelTypeName(panel))) {
        active_tab_ = panel;
        ImGui::EndTabItem();
      }
    }
    ImGui::EndTabBar();
  }
}
```

#### 7. Sidebar Categories Icon Bar
Add VSCode-style icon bar on left edge showing editor categories:
```cpp
void DrawSidebarIconBar() {
  // Vertical strip of category icons
  for (const auto& category : GetActiveCategories()) {
    bool is_current = (category == current_category_);
    if (DrawIconButton(GetCategoryIcon(category), is_current)) {
      SetCurrentCategory(category);
    }
  }
}
```

#### 8. Floating Panel Mode
Allow panels to be undocked and float as separate windows:
```cpp
enum class PanelDockMode {
  kDocked,      // Fixed to right edge
  kFloating,    // Separate movable window
  kMinimized    // Collapsed to icon
};

void DrawAsFloatingWindow() {
  ImGui::SetNextWindowSize(ImVec2(panel_width_, 400.0f), ImGuiCond_FirstUseEver);
  if (ImGui::Begin(GetPanelTypeName(active_panel_), &visible_)) {
    DrawPanelContents();
  }
  ImGui::End();
}
```

### Low Priority / Experimental

#### 9. Panel Presets
Save/load panel configurations:
```cpp
struct PanelPreset {
  std::string name;
  std::vector<PanelType> open_panels;
  std::map<PanelType, float> panel_widths;
  bool sidebar_visible;
};

void SavePreset(const std::string& name);
void LoadPreset(const std::string& name);
```

#### 10. Context-Sensitive Panel Suggestions
Show relevant panels based on current editor:
```cpp
void SuggestPanelsForEditor(EditorType type) {
  switch (type) {
    case EditorType::kOverworld:
      ShowPanelHint(PanelType::kSettings, "Tip: Configure overworld flags");
      break;
    case EditorType::kDungeon:
      ShowPanelHint(PanelType::kProposals, "Tip: Review agent room suggestions");
      break;
  }
}
```

#### 11. Split Panel View
Allow splitting a panel horizontally to show two contents:
```cpp
void DrawSplitView() {
  float split_pos = viewport_height * split_ratio_;
  
  ImGui::BeginChild("##TopPanel", ImVec2(0, split_pos));
  DrawAgentChatPanel();
  ImGui::EndChild();
  
  DrawSplitter(&split_ratio_);
  
  ImGui::BeginChild("##BottomPanel");
  DrawProposalsPanel();
  ImGui::EndChild();
}
```

#### 12. Mini-Map in Sidebar
Show a visual mini-map of the current editor content:
```cpp
void DrawMiniMap() {
  // Render scaled-down preview of current editor
  auto* editor = editor_manager_->GetCurrentEditor();
  if (editor && editor->type() == EditorType::kOverworld) {
    RenderOverworldMiniMap(sidebar_width_ - 8, 100);
  }
}
```

---

## Testing Checklist

### Sidebar Tests
- [ ] Toggle button always visible in menu bar
- [ ] Icon changes based on collapsed state
- [ ] Dockspace adjusts when sidebar expands/collapses
- [ ] Cards display correctly in sidebar
- [ ] Category switching works

### Panel Tests
- [ ] Each panel type opens correctly
- [ ] Only one panel open at a time
- [ ] Panel toggle buttons highlight active panel
- [ ] Dockspace adjusts when panel opens/closes
- [ ] Close button in panel header works
- [ ] Panel respects screen bounds

### Menu Tests
- [ ] Cards submenu in Window menu
- [ ] Panels submenu in Window menu
- [ ] Editor shortcuts disabled without ROM
- [ ] Card Browser disabled without ROM
- [ ] Cards submenu shows placeholder without ROM

### Status Cluster Tests
- [ ] Order: version, dirty, session, panels, bell
- [ ] Panel toggle buttons visible
- [ ] Popups anchor to right edge
- [ ] Session popup doesn't overflow screen
- [ ] Notification popup doesn't overflow screen

### WASM Tests
- [ ] Menu bar hide button visible
- [ ] Menu bar hides when clicked
- [ ] Restore button appears when hidden
- [ ] Alt key restores menu bar

---

## Known Issues

### RESOLVED

1. **Sidebars Not Rendering (RESOLVED 2025-01-25)**
   - **Status:** Fixed
   - **Root Cause:** The sidebar and right panel drawing code was placed AFTER an early return in `EditorManager::Update()` that triggered when no ROM was loaded. This meant the sidebar code was never reached.
   - **Fix Applied:** Moved sidebar drawing (lines 754-816) and right panel drawing (lines 819-821) to BEFORE the early return at line 721-723. The sidebar now correctly draws:
     - **No ROM loaded:** `DrawPlaceholderSidebar()` shows "Open ROM" hint
     - **ROM loaded:** Full sidebar with category buttons and card toggles
   - **Files Modified:** `src/app/editor/editor_manager.cc` - restructured `Update()` method

### HIGH PRIORITY - UX Consistency Issues

1. **Right Panel System Visual Consistency**
   - **Status:** Open - needs design pass
   - **Symptoms:** The three right panel types (Notifications, Proposals, Settings) have inconsistent styling
   - **Issues:**
     - Panel headers vary in style and spacing
     - Background colors may not perfectly match theme in all cases
     - Close button positioning inconsistent
     - Panel content padding varies
   - **Location:** `src/app/editor/ui/right_panel_manager.cc`
   - **Fix needed:**
     - Standardize panel header style (icon + title + close button layout)
     - Ensure all panels use `theme.surface` consistently
     - Add consistent padding/margins across all panel types
     - Consider adding subtle panel title bar with drag handle aesthetic

2. **Notification Bell/Panel Integration**
   - **Status:** Open - needs review
   - **Symptoms:** Notification dropdown may not align well with right panel system
   - **Issues:**
     - Notification popup positioning may conflict with right panels
     - Notification styling may differ from panel styling
   - **Location:** `src/app/editor/ui/ui_coordinator.cc` - `DrawNotificationBell()`
   - **Fix needed:**
     - Consider making notifications a panel type instead of dropdown
     - Or ensure dropdown anchors correctly regardless of panel state

3. **Proposal Registry Panel**
   - **Status:** Open - needs consistency pass
   - **Symptoms:** Proposal drawer may have different UX patterns than other panels
   - **Location:** `src/app/editor/system/proposal_drawer.cc`
   - **Fix needed:** 
     - Align proposal UI with other panel styling
     - Ensure consistent header, padding, and interaction patterns

### MEDIUM PRIORITY

4. **Panel Content Not Scrollable:** Some panel contents may overflow without scrollbars. Need to wrap content in `ImGui::BeginChild()` with scroll flags.

5. **Settings Panel Integration:** The `SettingsEditor` is called directly but may need its own layout adaptation for panel context.

6. **Agent Chat State:** When panel closes, the chat widget's `active_` state should be managed to pause updates.

7. **Layout Persistence:** Panel states are not persisted across sessions yet.

### LOW PRIORITY

8. **Status Cluster Notification Positioning:** The `cluster_width` calculation (220px) works but could be dynamically calculated for better responsiveness.

## Debugging Guide

### Sidebar Visibility Issues (RESOLVED)

The sidebar visibility issue has been resolved. The root cause was that sidebar drawing code was placed after an early return in `EditorManager::Update()`. If similar issues occur in the future:

1. **Check execution order:** Ensure UI drawing code executes BEFORE any early returns
2. **Use ImGui Metrics Window:** Enable via View → ImGui Metrics to verify windows exist
3. **Check `GetLeftLayoutOffset()`:** Verify it returns the correct width for dockspace adjustment
4. **Verify visibility flags:** Check `IsCardSidebarVisible()` and `IsSidebarCollapsed()` states

### To Debug Status Cluster Positioning

1. **Visualize element bounds:**
```cpp
// After each element in DrawMenuBarExtras():
ImGui::GetForegroundDrawList()->AddRect(
    ImGui::GetItemRectMin(), ImGui::GetItemRectMax(),
    IM_COL32(255, 0, 0, 255));
```

2. **Log actual widths:**
```cpp
float actual_width = ImGui::GetCursorPosX() - start_x;
LOG_INFO("StatusCluster", "Actual width used: %f", actual_width);
```

3. **Check popup positioning:**
```cpp
// Before ImGui::SetNextWindowPos():
LOG_INFO("Popup", "popup_x=%f, button_max.y=%f", popup_x, button_max.y);
```

---

## Recent Updates (2025-01-25)

### Session 2: Menu Bar & Theme Fixes

**Menu Bar Cleanup:**
- Removed raw `ImGui::BeginMenu()` calls from `AddWindowMenuItems()` that were creating root-level "Cards" and "Panels" menus
- These were appearing alongside File/Edit/View instead of inside the Window menu
- Cards are now accessible via sidebar; Panels via toggle buttons on right

**Theme Integration:**
- Updated `DrawPlaceholderSidebar()` to use `theme.surface` and `theme.text_disabled`
- Updated `EditorCardRegistry::DrawSidebar()` to use theme colors
- Updated `RightPanelManager::Draw()` to use theme colors
- Sidebars now match the current application theme

**Status Cluster Improvements:**
- Restored panel toggle buttons (Agent, Proposals, Settings) on right side
- Reduced item spacing to 2px for more compact layout
- Reduced cluster width to 220px

### Session 1: Sidebar/Panel Rendering Fix (RESOLVED)

**Root Cause:** Sidebar and right panel drawing code was placed AFTER an early return in `EditorManager::Update()` at line 721-723. When no ROM was loaded, `Update()` returned early and the sidebar drawing code was never executed.

**Fix Applied:** Restructured `EditorManager::Update()` to draw sidebar and right panel BEFORE the early return.

**Additional Fixes:**
- Sidebars now fill full viewport height (y=0 to bottom)
- Welcome screen centers within dockspace region (accounts for sidebar offsets)
- Added `SetLayoutOffsets()` to WelcomeScreen for proper centering

### Sidebar Visibility States (NOW WORKING)
The sidebar now correctly shows in three states:
1. **No ROM:** Placeholder sidebar with "Open ROM" hint
2. **ROM + Editor:** Full card sidebar with editor cards
3. **Collapsed:** No sidebar (toggle button shows hamburger icon)

---

## Files Modified

| File | Changes |
|------|---------|
| `src/app/editor/ui/right_panel_manager.h` | **NEW** - Panel manager header |
| `src/app/editor/ui/right_panel_manager.cc` | **NEW** - Panel manager implementation, theme colors |
| `src/app/editor/editor_library.cmake` | Added right_panel_manager.cc |
| `src/app/editor/editor_manager.h` | Added RightPanelManager member, layout offset methods |
| `src/app/editor/editor_manager.cc` | Initialize RightPanelManager, sidebar toggle, draw panel, theme colors, viewport positioning |
| `src/app/editor/system/menu_orchestrator.h` | Added AddPanelsSubmenu declaration |
| `src/app/editor/system/menu_orchestrator.cc` | Removed root-level Cards/Panels menus, cleaned up Window menu |
| `src/app/controller.cc` | Layout offset calculation, menu bar visibility |
| `src/app/editor/ui/ui_coordinator.h` | Menu bar visibility state |
| `src/app/editor/ui/ui_coordinator.cc` | Reordered status cluster, panel buttons, compact spacing |
| `src/app/editor/system/editor_card_registry.cc` | Theme colors for sidebar, viewport positioning |
| `src/app/editor/ui/welcome_screen.h` | Added SetLayoutOffsets() method |
| `src/app/editor/ui/welcome_screen.cc` | Dockspace-aware centering with sidebar offsets |

---

## Related Documents

- [Sidebar-MenuBar-Sessions Architecture](handoff-sidebar-menubar-sessions.md) - Original architecture overview
- [EditorManager Architecture](H2-editor-manager-architecture.md) - Full EditorManager documentation
- [Agent Protocol](../../../AGENTS.md) - Agent coordination rules

---

## Contact

For questions about this system, review the coordination board or consult the `ui-architect` agent persona.

