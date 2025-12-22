# UI/UX Improvement Plan: IDE-like Interface

> Terminology update: ongoing refactor renames Card → Panel. References to
> `CardInfo`/`EditorCardRegistry` should be read as
> `PanelDescriptor`/`PanelManager` going forward.

**Created**: 2025-01-25
**Status**: Design Phase
**Priority**: High
**Target**: Make yaze feel like VSCode/JetBrains IDEs

---

## Executive Summary

This document outlines comprehensive UI/UX improvements to transform yaze into a professional, IDE-like application with consistent theming, responsive layouts, proper disabled states, and Material Design principles throughout.

**Key Improvements**:
1. Enhanced sidebar system with responsive sizing, badges, and disabled states
2. Improved menu bar with proper precondition checks and consistent spacing
3. Responsive panel system that adapts to window size
4. Extended AgentUITheme for application-wide consistency
5. Status cluster enhancements for better visual feedback

---

## 1. Sidebar System Improvements

### Current Issues
- No visual feedback for disabled cards (when ROM not loaded)
- Fixed sizing doesn't respond to window height changes
- No badge indicators for notifications/counts
- Hover effects are basic (only tooltip)
- Missing selection state visual feedback
- No category headers/separators for large card lists

### Proposed Solutions

#### 1.1 Disabled State System

**File**: `src/app/editor/system/editor_card_registry.h`

```cpp
struct CardInfo {
  // ... existing fields ...
  std::function<bool()> enabled_condition;  // NEW: Card enable condition
  int notification_count = 0;               // NEW: Badge count (0 = no badge)
};
```

**File**: `src/app/editor/system/editor_card_registry.cc` (DrawSidebar)

```cpp
// In the card drawing loop:
bool is_active = card.visibility_flag && *card.visibility_flag;
bool is_enabled = card.enabled_condition ? card.enabled_condition() : true;

if (!is_enabled) {
  ImGui::PushStyleVar(ImGuiStyleVar_Alpha, 0.4f);  // Dim disabled cards
}

// ... draw button ...

if (!is_enabled) {
  ImGui::PopStyleVar();
}

if (ImGui::IsItemHovered()) {
  if (!is_enabled) {
    ImGui::SetTooltip("%s\nRequires ROM to be loaded", card.display_name.c_str());
  } else {
    // Normal tooltip
  }
}
```

#### 1.2 Notification Badges

**New Helper Function**: `src/app/editor/system/editor_card_registry.cc`

```cpp
void EditorCardRegistry::DrawCardWithBadge(const CardInfo& card, bool is_active) {
  const auto& theme = gui::ThemeManager::Get().GetCurrentTheme();

  // Draw the main button
  // ... existing button code ...

  // Draw notification badge if count > 0
  if (card.notification_count > 0) {
    ImDrawList* draw_list = ImGui::GetWindowDrawList();
    ImVec2 button_max = ImGui::GetItemRectMax();

    // Badge position (top-right corner of button)
    float badge_radius = 8.0f;
    ImVec2 badge_pos(button_max.x - badge_radius, button_max.y - 32.0f + badge_radius);

    // Draw badge circle
    ImVec4 error_color = gui::ConvertColorToImVec4(theme.error);
    draw_list->AddCircleFilled(badge_pos, badge_radius, ImGui::GetColorU32(error_color));

    // Draw count text
    if (card.notification_count < 100) {
      char badge_text[4];
      snprintf(badge_text, sizeof(badge_text), "%d", card.notification_count);

      ImVec2 text_size = ImGui::CalcTextSize(badge_text);
      ImVec2 text_pos(badge_pos.x - text_size.x * 0.5f,
                      badge_pos.y - text_size.y * 0.5f);

      draw_list->AddText(text_pos, IM_COL32(255, 255, 255, 255), badge_text);
    } else {
      draw_list->AddText(
        ImVec2(badge_pos.x - 6, badge_pos.y - 6),
        IM_COL32(255, 255, 255, 255), "!");
    }
  }
}
```

#### 1.3 Enhanced Hover Effects

**Style Improvements**:

```cpp
// In DrawSidebar, replace existing button style:
if (is_active) {
  ImGui::PushStyleColor(ImGuiCol_Button,
    ImVec4(accent_color.x, accent_color.y, accent_color.z, 0.6f));
  ImGui::PushStyleColor(ImGuiCol_ButtonHovered,
    ImVec4(accent_color.x, accent_color.y, accent_color.z, 0.8f));
  ImGui::PushStyleColor(ImGuiCol_ButtonActive, accent_color);

  // Add glow effect
  ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 8.0f);
} else {
  ImGui::PushStyleColor(ImGuiCol_Button, button_bg);
  ImGui::PushStyleColor(ImGuiCol_ButtonHovered,
    ImVec4(accent_color.x, accent_color.y, accent_color.z, 0.3f));
  ImGui::PushStyleColor(ImGuiCol_ButtonActive,
    gui::ConvertColorToImVec4(theme.button_active));
  ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 4.0f);
}
```

#### 1.4 Responsive Sizing

**Current code calculates fixed heights**. Improve with dynamic calculations:

```cpp
// Replace fixed utility_section_height calculation:
const float utility_section_height = 4 * 40.0f + 80.0f;

// With dynamic calculation:
const float button_height = 40.0f * theme.widget_height_multiplier;
const float spacing = ImGui::GetStyle().ItemSpacing.y;
const float utility_button_count = 4;
const float utility_section_height =
  (utility_button_count * (button_height + spacing)) +
  80.0f * theme.panel_padding_multiplier;

const float current_y = ImGui::GetCursorPosY();
const float available_height = viewport_height - current_y - utility_section_height;
```

#### 1.5 Category Headers/Separators

**For editors with many cards**, add collapsible sections:

```cpp
void EditorCardRegistry::DrawSidebarWithSections(
    size_t session_id,
    const std::string& category,
    const std::map<std::string, std::vector<CardInfo>>& card_sections) {

  for (const auto& [section_name, cards] : card_sections) {
    if (ImGui::CollapsingHeader(section_name.c_str(),
                                 ImGuiTreeNodeFlags_DefaultOpen)) {
      for (const auto& card : cards) {
        DrawCardWithBadge(card, /* is_active */ ...);
      }
    }
    ImGui::Spacing();
  }
}
```

---

## 2. Menu Bar System Improvements

### Current Issues
- Many menu items don't gray out when preconditions not met
- Inconsistent spacing/padding between menu items
- Status cluster feels cramped
- No visual separation between menu groups

### Proposed Solutions

#### 2.1 Comprehensive Enabled Conditions

**File**: `src/app/editor/system/menu_orchestrator.cc`

Add helper methods:

```cpp
// Existing helpers (keep these):
bool MenuOrchestrator::HasActiveRom() const;
bool MenuOrchestrator::CanSaveRom() const;
bool MenuOrchestrator::HasCurrentEditor() const;
bool MenuOrchestrator::HasMultipleSessions() const;

// NEW helpers:
bool MenuOrchestrator::HasActiveSelection() const {
  // Check if current editor has selected entities/objects
  // Delegate to EditorManager to query active editor
}

bool MenuOrchestrator::CanUndo() const {
  // Check if undo stack has entries
  return HasCurrentEditor() && editor_manager_->CanUndo();
}

bool MenuOrchestrator::CanRedo() const {
  return HasCurrentEditor() && editor_manager_->CanRedo();
}

bool MenuOrchestrator::HasClipboardData() const {
  // Check if clipboard has paste-able data
  return HasCurrentEditor() && editor_manager_->HasClipboardData();
}

bool MenuOrchestrator::IsRomModified() const {
  return HasActiveRom() && rom_manager_.GetCurrentRom()->dirty();
}
```

#### 2.2 Apply Conditions to All Menu Items

**File**: `src/app/editor/system/menu_orchestrator.cc`

```cpp
void MenuOrchestrator::BuildFileMenu() {
  menu_builder_.BeginMenu("File", ICON_MD_FOLDER);

  menu_builder_.Item(
    "Open ROM", ICON_MD_FILE_OPEN,
    [this]() { OnOpenRom(); },
    "Ctrl+O",
    []() { return true; }  // Always enabled
  );

  menu_builder_.Item(
    "Save ROM", ICON_MD_SAVE,
    [this]() { OnSaveRom(); },
    "Ctrl+S",
    [this]() { return CanSaveRom(); }  // EXISTING
  );

  menu_builder_.Item(
    "Save ROM As...", ICON_MD_SAVE_AS,
    [this]() { OnSaveRomAs(); },
    "Ctrl+Shift+S",
    [this]() { return HasActiveRom(); }  // NEW
  );

  menu_builder_.Separator();

  menu_builder_.Item(
    "Create Project", ICON_MD_CREATE_NEW_FOLDER,
    [this]() { OnCreateProject(); },
    "Ctrl+Shift+N",
    [this]() { return HasActiveRom(); }  // NEW - requires ROM first
  );

  // ... continue for all items

  menu_builder_.EndMenu();
}

void MenuOrchestrator::BuildEditMenu() {
  menu_builder_.BeginMenu("Edit", ICON_MD_EDIT);

  menu_builder_.Item(
    "Undo", ICON_MD_UNDO,
    [this]() { OnUndo(); },
    "Ctrl+Z",
    [this]() { return CanUndo(); }  // NEW
  );

  menu_builder_.Item(
    "Redo", ICON_MD_REDO,
    [this]() { OnRedo(); },
    "Ctrl+Y",
    [this]() { return CanRedo(); }  // NEW
  );

  menu_builder_.Separator();

  menu_builder_.Item(
    "Cut", ICON_MD_CONTENT_CUT,
    [this]() { OnCut(); },
    "Ctrl+X",
    [this]() { return HasActiveSelection(); }  // NEW
  );

  menu_builder_.Item(
    "Copy", ICON_MD_CONTENT_COPY,
    [this]() { OnCopy(); },
    "Ctrl+C",
    [this]() { return HasActiveSelection(); }  // NEW
  );

  menu_builder_.Item(
    "Paste", ICON_MD_CONTENT_PASTE,
    [this]() { OnPaste(); },
    "Ctrl+V",
    [this]() { return HasClipboardData(); }  // NEW
  );

  menu_builder_.EndMenu();
}
```

#### 2.3 Menu Spacing and Visual Grouping

**File**: `src/app/editor/ui/menu_builder.h`

Add spacing control methods:

```cpp
class MenuBuilder {
public:
  // ... existing methods ...

  // NEW: Visual spacing
  void SeparatorWithSpacing(float height = 4.0f) {
    ImGui::Spacing();
    ImGui::Separator();
    ImGui::Dummy(ImVec2(0, height));
  }

  // NEW: Menu section header (for long menus)
  void SectionHeader(const char* label) {
    const auto& theme = gui::ThemeManager::Get().GetCurrentTheme();
    ImGui::PushStyleColor(ImGuiCol_Text,
      gui::ConvertColorToImVec4(theme.text_disabled));
    ImGui::TextUnformatted(label);
    ImGui::PopStyleColor();
    ImGui::Spacing();
  }
};
```

**Usage**:

```cpp
void MenuOrchestrator::BuildToolsMenu() {
  menu_builder_.BeginMenu("Tools", ICON_MD_BUILD);

  menu_builder_.SectionHeader("ROM Analysis");
  menu_builder_.Item("ROM Info", ICON_MD_INFO, ...);
  menu_builder_.Item("Validate ROM", ICON_MD_CHECK_CIRCLE, ...);

  menu_builder_.SeparatorWithSpacing();

  menu_builder_.SectionHeader("Utilities");
  menu_builder_.Item("Global Search", ICON_MD_SEARCH, ...);
  menu_builder_.Item("Command Palette", ICON_MD_TERMINAL, ...);

  menu_builder_.EndMenu();
}
```

#### 2.4 Status Cluster Improvements

**File**: `src/app/editor/ui/ui_coordinator.cc`

```cpp
void UICoordinator::DrawMenuBarExtras() {
  const auto& theme = gui::ThemeManager::Get().GetCurrentTheme();

  // Calculate cluster width dynamically
  float cluster_width = 0.0f;
  cluster_width += 30.0f;  // Dirty badge
  cluster_width += 30.0f;  // Notification bell
  if (session_coordinator_.HasMultipleSessions()) {
    cluster_width += 80.0f;  // Session button with name
  }
  cluster_width += 60.0f;  // Version
  cluster_width += 20.0f;  // Padding

  // Right-align with proper spacing
  ImGui::SameLine(ImGui::GetWindowWidth() - cluster_width);

  // Add subtle separator before status cluster
  ImGui::PushStyleColor(ImGuiCol_Separator,
    gui::ConvertColorToImVec4(theme.border));
  ImGui::SeparatorEx(ImGuiSeparatorFlags_Vertical);
  ImGui::PopStyleColor();

  ImGui::SameLine();
  ImGui::Dummy(ImVec2(8.0f, 0));  // Spacing
  ImGui::SameLine();

  // 1. Dirty badge (with animation)
  Rom* current_rom = rom_manager_.GetCurrentRom();
  if (current_rom && current_rom->dirty()) {
    DrawDirtyBadge();
  }

  // 2. Notification bell
  DrawNotificationBell();

  // 3. Session button (if multiple)
  if (session_coordinator_.HasMultipleSessions()) {
    DrawSessionButton();
  }

  // 4. Version
  ImGui::SameLine();
  ImGui::PushStyleColor(ImGuiCol_Text,
    gui::ConvertColorToImVec4(theme.text_disabled));
  ImGui::TextUnformatted(ICON_MD_INFO " v0.1.0");
  ImGui::PopStyleColor();
}

void UICoordinator::DrawDirtyBadge() {
  const auto& theme = gui::ThemeManager::Get().GetCurrentTheme();

  // Pulsing animation
  static float pulse_time = 0.0f;
  pulse_time += ImGui::GetIO().DeltaTime;
  float pulse_alpha = 0.7f + 0.3f * sin(pulse_time * 3.0f);

  ImVec4 warning_color = gui::ConvertColorToImVec4(theme.warning);
  warning_color.w = pulse_alpha;

  ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0, 0, 0, 0));
  ImGui::PushStyleColor(ImGuiCol_ButtonHovered,
    ImVec4(warning_color.x, warning_color.y, warning_color.z, 0.3f));

  if (ImGui::SmallButton(ICON_MD_CIRCLE)) {
    // Quick save on click
    OnSaveRom();
  }

  ImGui::PopStyleColor(2);

  if (ImGui::IsItemHovered()) {
    ImGui::SetTooltip("Unsaved changes\nClick to save (Ctrl+S)");
  }

  ImGui::SameLine();
}
```

---

## 3. AgentUITheme Extensions

### New Theme Properties Needed

**File**: `src/app/editor/agent/agent_ui_theme.h`

```cpp
struct AgentUITheme {
  // ... existing fields ...

  // NEW: Sidebar specific colors
  ImVec4 sidebar_bg;
  ImVec4 sidebar_border;
  ImVec4 sidebar_icon_active;
  ImVec4 sidebar_icon_inactive;
  ImVec4 sidebar_icon_disabled;
  ImVec4 sidebar_icon_hover;
  ImVec4 sidebar_badge_bg;
  ImVec4 sidebar_badge_text;

  // NEW: Menu bar specific colors
  ImVec4 menu_separator;
  ImVec4 menu_section_header;
  ImVec4 menu_item_disabled;

  // NEW: Panel sizing (responsive)
  float sidebar_width_base = 48.0f;
  float sidebar_icon_size = 40.0f;
  float sidebar_icon_spacing = 6.0f;
  float panel_padding = 8.0f;
  float status_cluster_spacing = 8.0f;

  // NEW: Animation timing
  float hover_transition_speed = 0.15f;
  float badge_pulse_speed = 3.0f;

  static AgentUITheme FromCurrentTheme();
};
```

**File**: `src/app/editor/agent/agent_ui_theme.cc`

```cpp
AgentUITheme AgentUITheme::FromCurrentTheme() {
  AgentUITheme theme;
  const auto& current = gui::ThemeManager::Get().GetCurrentTheme();

  // ... existing code ...

  // NEW: Sidebar colors
  theme.sidebar_bg = ConvertColorToImVec4(current.surface);
  theme.sidebar_border = ConvertColorToImVec4(current.border);
  theme.sidebar_icon_active = ConvertColorToImVec4(current.accent);
  theme.sidebar_icon_inactive = ConvertColorToImVec4(current.button);
  theme.sidebar_icon_disabled = ImVec4(0.3f, 0.3f, 0.3f, 1.0f);
  theme.sidebar_icon_hover = ConvertColorToImVec4(current.button_hovered);
  theme.sidebar_badge_bg = ConvertColorToImVec4(current.error);
  theme.sidebar_badge_text = ImVec4(1.0f, 1.0f, 1.0f, 1.0f);

  // NEW: Menu bar colors
  theme.menu_separator = ConvertColorToImVec4(current.border);
  theme.menu_section_header = ConvertColorToImVec4(current.text_disabled);
  theme.menu_item_disabled = ImVec4(0.4f, 0.4f, 0.4f, 1.0f);

  // NEW: Responsive sizing from theme
  theme.sidebar_width_base = 48.0f * current.compact_factor;
  theme.sidebar_icon_size = 40.0f * current.widget_height_multiplier;
  theme.sidebar_icon_spacing = 6.0f * current.spacing_multiplier;
  theme.panel_padding = 8.0f * current.panel_padding_multiplier;

  return theme;
}
```

### Helper Functions

**File**: `src/app/editor/agent/agent_ui_theme.h`

```cpp
namespace AgentUI {

// ... existing helpers ...

// NEW: Sidebar helpers
void PushSidebarStyle();
void PopSidebarStyle();

void RenderSidebarButton(const char* icon, bool is_active, bool is_enabled,
                         int notification_count = 0);

// NEW: Menu helpers
void PushMenuSectionStyle();
void PopMenuSectionStyle();

// NEW: Status cluster helpers
void RenderDirtyIndicator(bool dirty);
void RenderNotificationBadge(int count);
void RenderSessionIndicator(const char* session_name, bool is_active);

}  // namespace AgentUI
```

---

## 4. Right Panel System Integration

### Current State
- `RightPanelManager` exists and handles agent chat, proposals, settings, help
- Uses fixed widths per panel type
- No animation (animating_ flag exists but unused)
- Good theme integration already

### Improvements Needed

#### 4.1 Slide Animation

**File**: `src/app/editor/ui/right_panel_manager.cc`

```cpp
void RightPanelManager::Draw() {
  if (active_panel_ == PanelType::kNone && panel_animation_ <= 0.0f) {
    return;
  }

  const auto& theme = gui::ThemeManager::Get().GetCurrentTheme();
  const ImGuiViewport* viewport = ImGui::GetMainViewport();
  const float viewport_height = viewport->WorkSize.y;
  const float viewport_width = viewport->WorkSize.x;

  // Animate panel expansion/collapse
  if (animating_) {
    const float animation_speed = 6.0f * ImGui::GetIO().DeltaTime;

    if (active_panel_ != PanelType::kNone) {
      // Expanding
      panel_animation_ = std::min(1.0f, panel_animation_ + animation_speed);
      if (panel_animation_ >= 1.0f) {
        animating_ = false;
      }
    } else {
      // Collapsing
      panel_animation_ = std::max(0.0f, panel_animation_ - animation_speed);
      if (panel_animation_ <= 0.0f) {
        animating_ = false;
        return;  // Fully collapsed, don't draw
      }
    }
  }

  // Eased animation curve (smooth in/out)
  float eased_progress = panel_animation_ * panel_animation_ * (3.0f - 2.0f * panel_animation_);

  const float target_width = GetPanelWidth();
  const float current_width = target_width * eased_progress;

  // ... rest of drawing code, use current_width instead of GetPanelWidth()
}
```

#### 4.2 Responsive Width Calculation

```cpp
float RightPanelManager::GetPanelWidth() const {
  const ImGuiViewport* viewport = ImGui::GetMainViewport();
  const float viewport_width = viewport->WorkSize.x;

  // Calculate max panel width (30% of viewport, clamped)
  const float max_panel_width = std::min(600.0f, viewport_width * 0.3f);

  float base_width = 0.0f;
  switch (active_panel_) {
    case PanelType::kAgentChat:
      base_width = agent_chat_width_;
      break;
    case PanelType::kProposals:
      base_width = proposals_width_;
      break;
    case PanelType::kSettings:
      base_width = settings_width_;
      break;
    case PanelType::kHelp:
      base_width = help_width_;
      break;
    default:
      return 0.0f;
  }

  return std::min(base_width, max_panel_width);
}
```

#### 4.3 Docking Space Adjustment

**File**: `src/app/editor/editor_manager.cc`

When drawing the main docking space, reserve space for the right panel:

```cpp
void EditorManager::UpdateMainDockingSpace() {
  const ImGuiViewport* viewport = ImGui::GetMainViewport();

  float sidebar_width = 0.0f;
  if (ui_coordinator_->IsCardSidebarVisible() &&
      !card_registry_.IsSidebarCollapsed()) {
    sidebar_width = EditorCardRegistry::GetSidebarWidth();
  }

  // NEW: Reserve space for right panel
  float right_panel_width = right_panel_manager_->GetPanelWidth();
  if (right_panel_manager_->IsPanelExpanded()) {
    right_panel_width *= right_panel_manager_->GetAnimationProgress();
  }

  // Docking space occupies the remaining space
  ImVec2 dockspace_pos(viewport->WorkPos.x + sidebar_width, viewport->WorkPos.y);
  ImVec2 dockspace_size(
    viewport->WorkSize.x - sidebar_width - right_panel_width,
    viewport->WorkSize.y
  );

  ImGui::SetNextWindowPos(dockspace_pos);
  ImGui::SetNextWindowSize(dockspace_size);

  // ... rest of docking space setup
}
```

---

## 5. Overall Layout System

### 5.1 Layout Zones

```
┌───────────────────────────────────────────────────────────────┐
│ Menu Bar (Full Width)                         [●][🔔][📄][v]│
├─┬─────────────────────────────────────────────────────────┬───┤
│S│                                                         │ R │
│i│                                                         │ i │
│d│               Main Docking Space                        │ g │
│e│                                                         │ h │
│b│                 (Editor Cards)                          │ t │
│a│                                                         │   │
│r│                                                         │ P │
│ │                                                         │ a │
│ │                                                         │ n │
│ │                                                         │ e │
│ │                                                         │ l │
│ │                                                         │   │
└─┴─────────────────────────────────────────────────────────┴───┘
```

### 5.2 Responsive Breakpoints

**Add to EditorManager or new LayoutManager class**:

```cpp
enum class LayoutMode {
  kCompact,   // < 1280px width
  kNormal,    // 1280-1920px
  kWide       // > 1920px
};

LayoutMode GetLayoutMode() const {
  const ImGuiViewport* viewport = ImGui::GetMainViewport();
  float width = viewport->WorkSize.x;

  if (width < 1280.0f) return LayoutMode::kCompact;
  if (width < 1920.0f) return LayoutMode::kNormal;
  return LayoutMode::kWide;
}

// Adjust UI based on mode:
void ApplyLayoutMode(LayoutMode mode) {
  switch (mode) {
    case LayoutMode::kCompact:
      // Auto-collapse sidebar on small screens?
      // Reduce panel widths
      right_panel_manager_->SetPanelWidth(PanelType::kAgentChat, 300.0f);
      break;
    case LayoutMode::kNormal:
      right_panel_manager_->SetPanelWidth(PanelType::kAgentChat, 380.0f);
      break;
    case LayoutMode::kWide:
      right_panel_manager_->SetPanelWidth(PanelType::kAgentChat, 480.0f);
      break;
  }
}
```

---

## 6. Implementation Priority Order

### Phase 1: Critical Foundation (Week 1)
**Priority: P0 - Immediate**

1. **Extend AgentUITheme** with sidebar/menu colors and sizing
   - Files: `agent_ui_theme.h`, `agent_ui_theme.cc`
   - Effort: 2 hours
   - Benefit: Foundation for all other improvements

2. **Add CardInfo enabled_condition field**
   - Files: `editor_card_registry.h`, `editor_card_registry.cc`
   - Effort: 1 hour
   - Benefit: Enables disabled state system

3. **Implement disabled state rendering in sidebar**
   - Files: `editor_card_registry.cc` (DrawSidebar)
   - Effort: 3 hours
   - Benefit: Immediate visual feedback improvement

### Phase 2: Menu Bar Improvements (Week 1-2)
**Priority: P1 - High**

4. **Add helper methods to MenuOrchestrator**
   - Files: `menu_orchestrator.h`, `menu_orchestrator.cc`
   - Methods: `CanUndo()`, `CanRedo()`, `HasActiveSelection()`, etc.
   - Effort: 4 hours
   - Benefit: Foundation for proper menu item states

5. **Apply enabled conditions to all menu items**
   - Files: `menu_orchestrator.cc` (all Build*Menu methods)
   - Effort: 6 hours
   - Benefit: Professional menu behavior

6. **Improve status cluster layout and animations**
   - Files: `ui_coordinator.cc` (DrawMenuBarExtras)
   - Effort: 4 hours
   - Benefit: Polished status area

### Phase 3: Sidebar Enhancements (Week 2)
**Priority: P1 - High**

7. **Add notification badge system**
   - Files: `editor_card_registry.h`, `editor_card_registry.cc`
   - Effort: 4 hours
   - Benefit: Visual notification system

8. **Enhance hover effects and animations**
   - Files: `editor_card_registry.cc` (DrawSidebar)
   - Effort: 3 hours
   - Benefit: More polished interactions

9. **Implement responsive sidebar sizing**
   - Files: `editor_card_registry.cc` (DrawSidebar)
   - Effort: 2 hours
   - Benefit: Better UX on different screen sizes

### Phase 4: Right Panel System (Week 2-3)
**Priority: P2 - Medium**

10. **Add slide-in/out animation**
    - Files: `right_panel_manager.cc`
    - Effort: 4 hours
    - Benefit: Smooth panel transitions

11. **Implement responsive panel widths**
    - Files: `right_panel_manager.cc`
    - Effort: 2 hours
    - Benefit: Adapts to screen size

12. **Integrate panel with docking space**
    - Files: `editor_manager.cc`
    - Effort: 3 hours
    - Benefit: Proper space reservation

### Phase 5: Polish and Refinement (Week 3-4)
**Priority: P3 - Nice to Have**

13. **Add category headers/separators to sidebar**
    - Files: `editor_card_registry.h`, `editor_card_registry.cc`
    - Effort: 4 hours
    - Benefit: Better organization for large card lists

14. **Implement layout breakpoints**
    - Files: New `layout_manager.h/cc` or in `editor_manager.cc`
    - Effort: 6 hours
    - Benefit: Professional responsive design

15. **Add menu section headers**
    - Files: `menu_builder.h`, `menu_orchestrator.cc`
    - Effort: 3 hours
    - Benefit: Better menu organization

---

## 7. New Classes/Methods Summary

### New Classes
- None required (all improvements extend existing classes)

### New Methods

**EditorCardRegistry**:
- `void DrawCardWithBadge(const CardInfo& card, bool is_active)`
- `void DrawSidebarWithSections(size_t session_id, const std::string& category, const std::map<std::string, std::vector<CardInfo>>& card_sections)`

**MenuOrchestrator**:
- `bool HasActiveSelection() const`
- `bool CanUndo() const`
- `bool CanRedo() const`
- `bool HasClipboardData() const`
- `bool IsRomModified() const`

**MenuBuilder**:
- `void SeparatorWithSpacing(float height = 4.0f)`
- `void SectionHeader(const char* label)`

**UICoordinator**:
- `void DrawDirtyBadge()`

**RightPanelManager**:
- `float GetAnimationProgress() const { return panel_animation_; }`

**AgentUI namespace**:
- `void PushSidebarStyle()`
- `void PopSidebarStyle()`
- `void RenderSidebarButton(const char* icon, bool is_active, bool is_enabled, int notification_count = 0)`
- `void PushMenuSectionStyle()`
- `void PopMenuSectionStyle()`
- `void RenderDirtyIndicator(bool dirty)`
- `void RenderNotificationBadge(int count)`
- `void RenderSessionIndicator(const char* session_name, bool is_active)`

---

## 8. Testing Strategy

### Unit Tests
1. **CardInfo enabled_condition**: Test cards with/without conditions
2. **Badge rendering**: Test badge count display (0, 1-99, 100+)
3. **Menu item enabling**: Test all enabled condition helpers
4. **Panel animation**: Test animation state transitions

### Integration Tests
1. **Sidebar disabled states**: Open/close ROM, verify card states
2. **Menu bar preconditions**: Test menu items with various editor states
3. **Right panel animation**: Test panel open/close/switch
4. **Responsive layout**: Test at different window sizes (1024, 1920, 2560)

### Visual Regression Tests
1. **Sidebar appearance**: Screenshot tests for active/inactive/disabled states
2. **Menu bar appearance**: Screenshot tests for enabled/disabled items
3. **Status cluster**: Screenshot tests for dirty/clean, notifications
4. **Panel transitions**: Record video of animations for smoothness verification

---

## 9. Migration Notes

### Backward Compatibility
- All changes are additive (new fields/methods)
- Existing code continues to work
- `enabled_condition` is optional (defaults to always enabled)

### Editor Migration
Editors should register cards with enabled conditions:

```cpp
// OLD:
card_registry.RegisterCard(session_id, {
  .card_id = "dungeon.room_selector",
  .display_name = "Room Selector",
  .icon = ICON_MD_GRID_VIEW,
  .category = "Dungeon",
  .visibility_flag = &show_room_selector_
});

// NEW (with enabled condition):
card_registry.RegisterCard(session_id, {
  .card_id = "dungeon.room_selector",
  .display_name = "Room Selector",
  .icon = ICON_MD_GRID_VIEW,
  .category = "Dungeon",
  .visibility_flag = &show_room_selector_,
  .enabled_condition = [this]() { return rom_->is_loaded(); }  // NEW
});
```

---

## 10. Future Enhancements

### Beyond Initial Implementation
1. **Command Palette Integration**: Quick access to all cards/menus
2. **Keyboard Navigation**: Full keyboard control of sidebar/panels
3. **Custom Layouts**: Save/restore sidebar configurations
4. **Accessibility**: Screen reader support, high-contrast mode
5. **Theming**: Allow users to customize sidebar colors
6. **Gesture Support**: Swipe to open/close panels on touch devices

---

## 11. Success Metrics

### Measurable Goals
1. **User feedback**: "Feels like VSCode" in user testing
2. **Discoverability**: New users find features without documentation
3. **Efficiency**: Power users can navigate faster with keyboard
4. **Consistency**: Zero hardcoded colors, all theme-based
5. **Responsiveness**: Smooth at 60fps on all supported platforms

### Acceptance Criteria
- [ ] All sidebar cards show disabled state when ROM not loaded
- [ ] All menu items properly gray out based on preconditions
- [ ] Status cluster is visible and informative
- [ ] Right panel animates smoothly
- [ ] No visual glitches during window resize
- [ ] Theme changes apply to all new UI elements
- [ ] Keyboard shortcuts work for all major actions

---

## Appendix A: Code Snippets Repository

All code snippets from this document are reference implementations. Actual implementation may vary based on:
- Performance profiling results
- User feedback
- Platform-specific considerations
- Integration with existing systems

## Appendix B: Visual Design References

### IDE Inspirations
- **VSCode**: Sidebar icon layout, status bar clusters
- **JetBrains IDEs**: Menu organization, tool window badges
- **Sublime Text**: Minimap, distraction-free mode
- **Atom**: Theme system, package management UI

### Material Design Principles
- **Elevation**: Use shadows/borders for depth
- **Motion**: Meaningful animations (not decorative)
- **Color**: Semantic color usage (error=red, success=green)
- **Typography**: Clear hierarchy, readable at all sizes
