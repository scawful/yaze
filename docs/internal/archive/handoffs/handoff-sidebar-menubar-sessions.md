# Handoff: Sidebar, Menu Bar, and Session Systems

**Created**: 2025-01-24  
**Last Updated**: 2025-01-24  
**Status**: Active Reference  
**Owner**: UI/UX improvements

---

## Overview

This document describes the architecture and interactions between three core UI systems:
1. **Sidebar** (`EditorCardRegistry`) - Icon-based card toggle panel
2. **Menu Bar** (`MenuOrchestrator`, `MenuBuilder`) - Application menus and status cluster
3. **Sessions** (`SessionCoordinator`, `RomSession`) - Multi-ROM session management

---

## 1. Sidebar System

### Key Files
- `src/app/editor/system/editor_card_registry.h` - Card registration and sidebar state
- `src/app/editor/system/editor_card_registry.cc` - Sidebar rendering (`DrawSidebar()`)

### Architecture

The sidebar is a VSCode-style icon panel on the left side of the screen. It's managed by `EditorCardRegistry`, which:

1. **Stores card metadata** in `CardInfo` structs:
```cpp
struct CardInfo {
  std::string card_id;           // "dungeon.room_selector"
  std::string display_name;      // "Room Selector"
  std::string window_title;      // " Rooms List" (for DockBuilder)
  std::string icon;              // ICON_MD_GRID_VIEW
  std::string category;          // "Dungeon"
  std::string shortcut_hint;     // "Ctrl+Shift+R"
  bool* visibility_flag;         // Pointer to bool controlling visibility
  int priority;                  // Display order
};
```

2. **Tracks collapsed state** via `sidebar_collapsed_` member

### Collapsed State Behavior

When `sidebar_collapsed_ == true`:
- `DrawSidebar()` returns immediately (no sidebar drawn)
- A hamburger icon (≡) appears in the menu bar before "File" menu
- Clicking hamburger sets `sidebar_collapsed_ = false`

```cpp
// In EditorManager::DrawMenuBar()
if (card_registry_.IsSidebarCollapsed()) {
  if (ImGui::SmallButton(ICON_MD_MENU)) {
    card_registry_.SetSidebarCollapsed(false);
  }
}
```

### Card Registration

Editors register their cards during initialization:

```cpp
card_registry_.RegisterCard({
    .card_id = "dungeon.room_selector",
    .display_name = "Room Selector",
    .window_title = " Rooms List",
    .icon = ICON_MD_GRID_VIEW,
    .category = "Dungeon",
    .visibility_flag = &show_room_selector_,
    .priority = 10
});
```

### Utility Icons

The sidebar has a fixed "utilities" section at the bottom with:
- Emulator (ICON_MD_PLAY_ARROW)
- Hex Editor (ICON_MD_MEMORY)
- Settings (ICON_MD_SETTINGS)
- Card Browser (ICON_MD_DASHBOARD)

These are wired via callbacks:
```cpp
card_registry_.SetShowEmulatorCallback([this]() { ... });
card_registry_.SetShowSettingsCallback([this]() { ... });
card_registry_.SetShowCardBrowserCallback([this]() { ... });
```

### Improvement Areas
- **Disabled state styling**: Cards could show disabled state when ROM isn't loaded
- **Dynamic population**: Cards could auto-hide based on editor type
- **Badge indicators**: Cards could show notification badges

---

## 2. Menu Bar System

### Key Files
- `src/app/editor/system/menu_orchestrator.h` - Menu structure and callbacks
- `src/app/editor/system/menu_orchestrator.cc` - Menu building logic
- `src/app/editor/ui/menu_builder.h` - Fluent menu construction API
- `src/app/editor/ui/ui_coordinator.cc` - Status cluster rendering

### Architecture

The menu system has three layers:

1. **MenuBuilder** - Fluent API for ImGui menu construction
2. **MenuOrchestrator** - Business logic, menu structure, callbacks
3. **UICoordinator** - Status cluster (right side of menu bar)

### Menu Structure

```
[≡] [File] [Edit] [View] [Tools] [Window] [Help]    [●][🔔][📄▾][v0.x.x]
hamburger  menus                                     status cluster
(collapsed)
```

### MenuOrchestrator

Builds menus using `MenuBuilder`:

```cpp
void MenuOrchestrator::BuildMainMenu() {
  ClearMenu();
  BuildFileMenu();
  BuildEditMenu();
  BuildViewMenu();
  BuildToolsMenu();  // Contains former Debug menu items
  BuildWindowMenu();
  BuildHelpMenu();
  menu_builder_.Draw();
}
```

### Menu Item Pattern

```cpp
menu_builder_
    .Item(
        "Open ROM",           // Label
        ICON_MD_FILE_OPEN,    // Icon
        [this]() { OnOpenRom(); },  // Callback
        "Ctrl+O",             // Shortcut hint
        [this]() { return CanOpenRom(); }  // Enabled condition
    )
```

### Enabled Condition Helpers

Key helpers in `MenuOrchestrator`:
```cpp
bool HasActiveRom() const;      // Is a ROM loaded?
bool CanSaveRom() const;        // Can save (ROM loaded + dirty)?
bool HasCurrentEditor() const;  // Is an editor active?
bool HasMultipleSessions() const;
```

### Status Cluster (Right Side)

Located in `UICoordinator::DrawMenuBarExtras()`:

1. **Dirty badge** - Orange dot when ROM has unsaved changes
2. **Notification bell** - Shows notification history dropdown
3. **Session button** - Only visible with 2+ sessions
4. **Version** - Always visible

```cpp
void UICoordinator::DrawMenuBarExtras() {
  // Right-aligned cluster
  ImGui::SameLine(ImGui::GetWindowWidth() - 150.0f);
  
  // 1. Dirty badge (if unsaved)
  if (current_rom && current_rom->dirty()) { ... }
  
  // 2. Notification bell
  DrawNotificationBell();
  
  // 3. Session button (if multiple sessions)
  if (session_coordinator_.HasMultipleSessions()) {
    DrawSessionButton();
  }
  
  // 4. Version
  ImGui::TextDisabled("v%s", version);
}
```

### Notification System

`ToastManager` now tracks notification history:

```cpp
struct NotificationEntry {
  std::string message;
  ToastType type;
  std::chrono::system_clock::time_point timestamp;
  bool read = false;
};

// Methods
size_t GetUnreadCount() const;
const std::deque<NotificationEntry>& GetHistory() const;
void MarkAllRead();
void ClearHistory();
```

### Improvement Areas
- **Disabled menu items**: Many items don't gray out when ROM not loaded
- **Dynamic menu population**: Submenus could populate based on loaded data
- **Context-sensitive menus**: Show different items based on active editor
- **Recent files list**: File menu could show recent ROMs/projects

---

## 3. Session System

### Key Files
- `src/app/editor/system/session_coordinator.h` - Session management
- `src/app/editor/system/session_coordinator.cc` - Session lifecycle
- `src/app/editor/system/rom_session.h` - Per-session state

### Architecture

Each session contains:
- A `Rom` instance
- An `EditorSet` (all editor instances)
- Session-specific UI state

```cpp
struct RomSession : public Session {
  Rom rom;
  std::unique_ptr<EditorSet> editor_set;
  size_t session_id;
  std::string name;
};
```

### Session Switching

```cpp
// In EditorManager
void SwitchToSession(size_t session_id) {
  current_session_id_ = session_id;
  auto* session = GetCurrentSession();
  // Update current_rom_, current_editor_set_, etc.
}
```

### Session UI

The session button in the status cluster shows a dropdown:

```cpp
void UICoordinator::DrawSessionButton() {
  if (ImGui::SmallButton(ICON_MD_LAYERS)) {
    ImGui::OpenPopup("##SessionSwitcherPopup");
  }
  
  if (ImGui::BeginPopup("##SessionSwitcherPopup")) {
    for (size_t i = 0; i < session_coordinator_.GetTotalSessionCount(); ++i) {
      // Draw selectable for each session
    }
    ImGui::EndPopup();
  }
}
```

### Improvement Areas
- **Session naming**: Allow renaming sessions
- **Session indicators**: Show which session has unsaved changes
- **Session persistence**: Save/restore session state
- **Session limit**: Handle max session count gracefully

---

## 4. Integration Points

### EditorManager as Hub

`EditorManager` coordinates all three systems:

```cpp
class EditorManager {
  EditorCardRegistry card_registry_;           // Sidebar
  std::unique_ptr<MenuOrchestrator> menu_orchestrator_;  // Menus
  std::unique_ptr<SessionCoordinator> session_coordinator_;  // Sessions
  std::unique_ptr<UICoordinator> ui_coordinator_;  // Status cluster
};
```

### DrawMenuBar Flow

```cpp
void EditorManager::DrawMenuBar() {
  if (ImGui::BeginMenuBar()) {
    // 1. Hamburger icon (if sidebar collapsed)
    if (card_registry_.IsSidebarCollapsed()) {
      if (ImGui::SmallButton(ICON_MD_MENU)) {
        card_registry_.SetSidebarCollapsed(false);
      }
    }
    
    // 2. Main menus
    menu_orchestrator_->BuildMainMenu();
    
    // 3. Status cluster (right side)
    ui_coordinator_->DrawMenuBarExtras();
    
    ImGui::EndMenuBar();
  }
}
```

### Sidebar Drawing Flow

```cpp
// In EditorManager::Update()
if (ui_coordinator_ && ui_coordinator_->IsCardSidebarVisible()) {
  card_registry_.DrawSidebar(
      category,           // Current editor category
      active_categories,  // All active editor categories
      category_switch_callback,
      collapse_callback   // Now empty (hamburger handles expand)
  );
}
```

---

## 5. Key Patterns

### Disabled State Pattern

Current pattern for enabling/disabling:
```cpp
menu_builder_.Item(
    "Save ROM", ICON_MD_SAVE,
    [this]() { OnSaveRom(); },
    "Ctrl+S",
    [this]() { return CanSaveRom(); }  // Enabled condition
);
```

To improve: Add visual distinction for disabled items in sidebar.

### Callback Wiring Pattern

Components communicate via callbacks set during initialization:
```cpp
// In EditorManager::Initialize()
card_registry_.SetShowEmulatorCallback([this]() {
  ui_coordinator_->SetEmulatorVisible(true);
});

welcome_screen_.SetOpenRomCallback([this]() {
  status_ = LoadRom();
});
```

### State Query Pattern

Use getter methods to check state:
```cpp
bool HasActiveRom() const { return rom_manager_.HasActiveRom(); }
bool IsSidebarCollapsed() const { return sidebar_collapsed_; }
bool HasMultipleSessions() const { return session_coordinator_.HasMultipleSessions(); }
```

---

## 6. Common Tasks

### Adding a New Menu Item

1. Add callback method to `MenuOrchestrator`:
```cpp
void OnMyNewAction();
```

2. Add to appropriate `Add*MenuItems()` method:
```cpp
menu_builder_.Item("My Action", ICON_MD_STAR, [this]() { OnMyNewAction(); });
```

3. Implement the callback.

### Adding a New Sidebar Card

1. Add visibility flag to editor:
```cpp
bool show_my_card_ = false;
```

2. Register in editor's `Initialize()`:
```cpp
card_registry.RegisterCard({
    .card_id = "editor.my_card",
    .display_name = "My Card",
    .icon = ICON_MD_STAR,
    .category = "MyEditor",
    .visibility_flag = &show_my_card_
});
```

3. Draw in editor's `Update()` when visible.

### Adding Disabled State to Sidebar

Currently not implemented. Suggested approach:
```cpp
struct CardInfo {
  // ... existing fields ...
  std::function<bool()> enabled_condition;  // NEW
};

// In DrawSidebar()
bool enabled = card.enabled_condition ? card.enabled_condition() : true;
if (!enabled) {
  ImGui::PushStyleVar(ImGuiStyleVar_Alpha, 0.5f);
}
// Draw button
if (!enabled) {
  ImGui::PopStyleVar();
}
```

---

## 7. Files Quick Reference

| File | Purpose |
|------|---------|
| `editor_card_registry.h/cc` | Sidebar + card management |
| `menu_orchestrator.h/cc` | Menu structure + callbacks |
| `menu_builder.h` | Fluent menu API |
| `ui_coordinator.h/cc` | Status cluster + UI state |
| `session_coordinator.h/cc` | Multi-session management |
| `editor_manager.h/cc` | Central coordinator |
| `toast_manager.h` | Notifications + history |

---

## 8. Next Steps for Improvement

1. **Disabled menu items**: Ensure all menu items properly disable when ROM not loaded
2. **Sidebar disabled state**: Add visual feedback for cards that require ROM
3. **Dynamic population**: Auto-populate cards based on ROM type/features
4. **Session indicators**: Show dirty state per-session in session dropdown
5. **Context menus**: Right-click menus for cards and session items

