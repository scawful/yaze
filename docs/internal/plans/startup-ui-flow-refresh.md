# Startup UI Flow Refresh
Status: IMPLEMENTED
Owner: imgui-frontend-engineer
Created: 2025-12-07
Last Reviewed: 2025-12-06
Implemented: 2025-12-06
Board Link: [Coordination Board – 2025-12-05 imgui-frontend-engineer – Panel launch/log filtering UX](../agents/coordination-board.md)

## Implementation Summary (2025-12-06)

### Completed:
1. **StartupSurface enum** (`ui_coordinator.h`) - Single source of truth for startup state: `kWelcome`, `kDashboard`, `kEditor`
2. **Centralized visibility logic** (`ui_coordinator.cc`) - `ShouldShowWelcome()`, `ShouldShowDashboard()`, `ShouldShowActivityBar()` methods
3. **Activity Bar hidden until ROM load** - Modified `EditorManager::Update()` and `LayoutCoordinator::GetLeftLayoutOffset()`
4. **Category-specific expressive colors** (`panel_manager.cc`) - Each category has unique vibrant color (gold for Dungeon, green for Overworld, etc.)
5. **Enhanced icon rendering** (`activity_bar.cc`) - 4px accent border, 30% glow background, outer glow shadow for active categories
6. **Updated TransparentIconButton** (`themed_widgets.cc`) - Added `active_color` parameter for custom icon colors
7. **Side panel starts collapsed** (`panel_manager.h`) - Changed `panel_expanded_` default to `false`
8. **Dashboard dismisses on category selection** - Added `TriggerCategorySelected()` callback, wired to `SetStartupSurface(kEditor)`

### Key Files Modified:
- `src/app/editor/ui/ui_coordinator.h` - Added `StartupSurface` enum and methods
- `src/app/editor/ui/ui_coordinator.cc` - Centralized startup logic
- `src/app/editor/menu/activity_bar.cc` - Expressive icon theming, category selection callback
- `src/app/editor/system/panel_manager.h/cc` - Category theme colors, `on_category_selected_` callback
- `src/app/gui/widgets/themed_widgets.h/cc` - Active color parameter
- `src/app/editor/editor_manager.cc` - Activity Bar visibility gating, category selection handler
- `src/app/editor/layout/layout_coordinator.cc` - Layout offset calculation

### Current Behavior:
| State | Welcome | Dashboard | Activity Bar | Side Panel |
|-------|---------|-----------|--------------|------------|
| Cold start (no ROM) | Visible | Hidden | Hidden | Hidden |
| ROM loaded | Hidden | Visible | Icons shown | Collapsed |
| Category icon clicked | Hidden | Hidden | Icons (active glow) | Expanded |
| Same icon clicked again | Hidden | Hidden | Icons | Collapsed (toggle) |

---

## Summary
The current startup surface (Welcome, Dashboard, Activity Bar/Panel Sidebar, Status Bar) overlaps and produces inconsistent state: welcome + sidebar visible, dashboard sticking when opening editors, status bar obscured by the Activity Bar, and panels (emulator/memory/agent) registered before ROM load but not actually usable. This plan proposes a single source of truth for startup/state gates, VSCode-style welcome behavior (only when empty), and clear rules for sidebar/dashboard/status visibility—respecting user overrides and CLI-driven flows for agents/tests.

## Problems Observed
- Welcome screen shows alongside Activity Bar/Panel tree, causing clutter; welcome sometimes “wins” focus and blocks panels opened via sidebar/presets.
- Dashboard occasionally remains visible after switching editors/categories.
- Status bar is covered by Activity Bar in some layouts.
- Panels for emulator/memory/agent are registered and “visible” before ROM load but cannot open, creating confusion.
- Startup flags/CLI (`--open_panels`, `--editor`) need to bypass stickiness to enable automation (e.g., MusicEditor for audio debugging).

## Goals
- Single gatekeeper for startup surface; welcome only when no ROM/project and no explicit editor/panel request.
- Deterministic sidebar/panel sidebar behavior on startup and editor switch; status bar always visible (no overlap).
- Hide or defer unusable panels until prerequisites (ROM/project) are met; provide inline messaging when blocked.
- Respect explicit user intent (CLI flags, last-session settings) over defaults.
- Make flows testable and scriptable for AI agents/CLI.

## Proposed Behavior
### Welcome Screen
- Show only when **no ROM/project loaded** and **no explicit startup editor/panel flags**.
- Auto-hide on first successful ROM/project load or when user explicitly opens an editor/category.
- If a user/flag forces show (`--welcome` or pref), keep visible but collapse sidebars to reduce clutter.
- When welcome is visible, panel/sidebar buttons should surface a toast “Load a ROM/project to open panels” instead of silently failing.

### Dashboard Panel
- Dashboard is a lightweight chooser: show only when welcome is hidden **and** no editor is active.
- Hide immediately when switching to an editor or when any panel/category is activated via Activity Bar.
- On return to “no editor” state (e.g., close session with no ROM), re-show dashboard unless suppressed by CLI.

### Activity Bar & Panel Sidebar
- Cold start (no ROM/project, no CLI intent): **no Activity Bar, no category panel/sidebar**, only the welcome screen.
- On ROM load: show **icon-only Activity Bar** by default (no persisted expanded state, no category preselected). Dashboard remains until user picks a category.
- No auto-selection of categories/panels. Selecting a category from the icon bar or dashboard activates that category, shows its defaults, dismisses dashboard/welcome. Pinned items remain cross-category.
- User can reopen the dashboard from the Activity Bar (dashboard icon) to deselect the current category; category panels collapse when leaving the category.

### Status Bar
- Always visible when app is running; compute offsets so Activity Bar/side panels never overlap it. If dockspace margin math is incorrect, fix via LayoutCoordinator offsets.

### Panel Availability & Prereqs
- Panels with hard dependencies (ROM) advertise `IsEnabled()` and return a tooltip “Load a ROM to use this panel.” Activity Bar selection opens a toast and does not flip visibility flags until enabled.
- On ROM load, auto-reenable last requested-but-blocked panels (e.g., emulator/memory) and focus them if they were in the request queue.

### CLI / Agent Overrides
- Flags `--editor`, `--open_panels`, `--welcome`, `--dashboard`, `--sidebar` define initial intent; intent beats persisted settings.
- If `--open_panels` includes panels needing ROM and ROM is absent, queue them and show a status toast; auto-open after load.
- Provide a small “startup state” summary in logs for reproducibility (welcome? dashboard? sidebar? active editor? queued panels?).

## Implementation Steps (high level)
1) **State owner**: Centralize startup/welcome/dashboard/sidebar decisions in UICoordinator + PanelManager callbacks; remove competing logic elsewhere. Emit a structured log of the chosen state on startup.
2) **Welcome rules**: Gate welcome on “no ROM/project and no explicit intent”; collapse sidebars while welcome is shown; hide on any editor/panel activation or ROM/project load.
3) **Dashboard rules**: Show only when welcome is hidden and no active editor; hide on category/editor switch; reopen when back to empty.
4) **Sidebar/Activity Bar**: Apply CLI/user overrides once; default to hidden when welcome is active, shown otherwise; clicking categories hides welcome/dashboard and activates layout defaults.
5) **Prereq-aware panels**: Implement `IsEnabled`/tooltip on emulator/memory/agent panels; queue panel requests until ROM loaded; on load, auto-show queued panels and focus first.
6) **Status bar spacing**: Ensure LayoutCoordinator offsets account for Activity Bar + side panel width so status bar is never obscured.
7) **Tests/telemetry**: Add a lightweight sanity check (e.g., headless mode) asserting startup state matrix for combinations of flags (welcome/dashboard/sidebar/editor/panels with/without ROM).

## Defaults (proposed)
- First launch, no ROM: Welcome shown; no Activity Bar/sidebar; dashboard hidden.
- After ROM load: Welcome hidden; Activity Bar icons shown; dashboard shown until a category is chosen; no category selected by default.
- CLI `--editor music --open_panels=music.piano_roll`: Welcome/dashboard skipped, Activity Bar icons shown, Music defaults + piano roll visible; if ROM missing, queue + toast.

### Activity Bar affordances
- Fix the collapse/expand icon inversion so the glyph reflects the actual state.
- Standardize the sidebar toggle shortcut and audit related Window/shortcut-manager entries so labels match behavior; ensure consistency across menu, tooltip, and shortcut display.

## Additional UX Ideas (Future Work)

These enhancements would improve the startup experience further. Each includes implementation guidance for future agents.

---

### 1. Empty State Checklist on Welcome/Dashboard

**Goal**: Guide new users through initial setup with actionable steps.

**Implementation**:
- **File**: `src/app/editor/ui/welcome_screen.cc`
- **Location**: Add to main content area, below recent projects section
- **Components**:
  ```cpp
  struct ChecklistItem {
    const char* icon;
    const char* label;
    const char* description;
    std::function<bool()> is_completed;  // Returns true if step is done
    std::function<void()> on_click;      // Action when clicked
  };
  ```
- **Items**:
  - "Load ROM" → `!rom_loaded` → `TriggerOpenRom()`
  - "Open Project" → `!project_loaded` → `TriggerOpenProject()`
  - "Pick an Editor" → `current_editor == nullptr` → `ShowDashboard()`
- **Styling**: Use `AgentUI::GetTheme()` colors, checkmark icon when complete, muted when incomplete

---

### 2. Panel Request Queue (Auto-open after ROM load)

**Goal**: Remember blocked panel requests and auto-open them when prerequisites are met.

**Implementation**:
- **File**: `src/app/editor/system/panel_manager.h/cc`
- **New members**:
  ```cpp
  std::vector<std::string> queued_panel_requests_;  // Panels awaiting ROM

  void QueuePanelRequest(const std::string& panel_id);
  void ProcessQueuedPanels();  // Called when ROM loads
  ```
- **Wire up**: In `EditorManager`, after successful ROM load:
  ```cpp
  panel_manager_.ProcessQueuedPanels();
  ```
- **Toast**: Use `ToastManager::Show("Panel will open when ROM loads", ToastType::kInfo)`
- **Focus**: Auto-focus first queued panel via `ImGui::SetWindowFocus()`

---

### 3. Dashboard Quick-Picks

**Goal**: One-click access to recent work without navigating the sidebar.

**Implementation**:
- **File**: `src/app/editor/ui/dashboard_panel.cc`
- **Location**: Add row above editor grid
- **Components**:
  - "Open Last Editor" button → reads from `user_settings_.prefs().last_editor`
  - "Recent Files" dropdown → reads from `recent_projects.txt`
- **Callback**: `SetQuickPickCallback(std::function<void(QuickPickType, std::string)>)`
- **Styling**: Use `gui::PrimaryButton()` for emphasis, secondary buttons for recent files

---

### 4. Sidebar Toggle Tooltip with Shortcut

**Goal**: Show current action and keyboard shortcut in tooltip.

**Implementation**:
- **File**: `src/app/editor/menu/activity_bar.cc`
- **Location**: Collapse/expand button tooltip (line ~232)
- **Current**: `ImGui::SetTooltip("Collapse Panel");`
- **Change to**:
  ```cpp
  const char* action = panel_manager_.IsPanelExpanded() ? "Collapse" : "Expand";
  const char* shortcut = "Ctrl+B";  // or Cmd+B on macOS
  ImGui::SetTooltip("%s Panel (%s)", action, shortcut);
  ```
- **Platform detection**: Use `#ifdef __APPLE__` for Cmd vs Ctrl

---

### 5. Status Bar Chips (ROM Status, Agent Status)

**Goal**: Always-visible indicators for why features might be disabled.

**Implementation**:
- **File**: `src/app/editor/ui/status_bar.cc`
- **New method**: `DrawStatusChips()`
- **Chips**:
  ```cpp
  struct StatusChip {
    const char* icon;
    const char* label;
    ImVec4 color;  // From theme (success, warning, error)
  };

  // ROM chip
  if (rom_ && rom_->is_loaded()) {
    DrawChip(ICON_MD_CHECK_CIRCLE, rom_->name(), theme.success);
  } else {
    DrawChip(ICON_MD_ERROR, "No ROM", theme.warning);
  }

  // Agent chip (if YAZE_BUILD_AGENT_UI)
  DrawChip(agent_connected ? ICON_MD_CLOUD_DONE : ICON_MD_CLOUD_OFF,
           agent_connected ? "Agent Online" : "Agent Offline",
           agent_connected ? theme.success : theme.text_disabled);
  ```
- **Position**: Right side of status bar, before version info
- **Interaction**: Click chip → opens relevant action (load ROM / agent settings)

---

### 6. Global Search on Welcome/Dashboard

**Goal**: Quick access to commands, panels, and recent files from a single input.

**Implementation**:
- **File**: `src/app/editor/ui/welcome_screen.cc` or new `global_search_widget.cc`
- **Components**:
  ```cpp
  class GlobalSearchWidget {
    char query_[256];
    std::vector<SearchResult> results_;

    struct SearchResult {
      enum Type { kCommand, kPanel, kRecentFile, kEditor };
      Type type;
      std::string display_name;
      std::string icon;
      std::function<void()> on_select;
    };

    void Search(const char* query);
    void Draw();
  };
  ```
- **Sources to search**:
  - `ShortcutManager::GetShortcuts()` → commands
  - `PanelManager::GetAllPanelDescriptors()` → panels
  - `RecentProjects` file → recent files
  - `EditorRegistry::GetAllEditorTypes()` → editors
- **Behavior**: On select → hide welcome/dashboard, execute action
- **Styling**: Centered search bar, dropdown results with fuzzy matching

---

### 7. Sticky Hint for Blocked Actions

**Goal**: Persistent reminder when actions are blocked, clearing when resolved.

**Implementation**:
- **File**: `src/app/editor/ui/toast_manager.h/cc`
- **New toast type**: `ToastType::kStickyHint`
- **Behavior**:
  - Doesn't auto-dismiss (no timeout)
  - Has explicit dismiss callback: `on_condition_met`
  - Displayed in status bar area (not floating)
  ```cpp
  void ShowStickyHint(const std::string& message,
                      std::function<bool()> dismiss_condition);

  // Usage:
  toast_manager_.ShowStickyHint(
      "Load a ROM to enable this feature",
      [this]() { return rom_ && rom_->is_loaded(); });
  ```
- **Visual**: Muted background, dismissible X button, auto-clears when condition met
- **Position**: Below main toolbar or in status bar notification area

---

### 8. Activity Bar Icon State Improvements

**Goal**: Better visual feedback for icon states.

**Implementation**:
- **File**: `src/app/editor/menu/activity_bar.cc`
- **Enhancements**:
  - **Disabled state**: 50% opacity + lock icon overlay for ROM-gated categories
  - **Hover state**: Subtle background highlight even when inactive
  - **Badge support**: Small dot/number for notifications (e.g., "3 unsaved changes")
  ```cpp
  // Badge drawing after icon button
  if (has_badge) {
    ImVec2 badge_pos = {cursor.x + 36, cursor.y + 4};
    DrawList->AddCircleFilled(badge_pos, 6.0f, badge_color);
    // Optional: draw count text
  }
  ```
- **Animation**: Consider subtle pulse for categories with pending actions

---

### 9. Collapse/Expand Icon Inversion Fix

**Goal**: Glyph should reflect the action, not the state.

**Implementation**:
- **File**: `src/app/editor/menu/activity_bar.cc` (line ~232)
- **Current issue**: Icon shows "collapse" when collapsed
- **Fix**:
  ```cpp
  const char* icon = panel_manager_.IsPanelExpanded()
      ? ICON_MD_KEYBOARD_DOUBLE_ARROW_LEFT   // Collapse action
      : ICON_MD_KEYBOARD_DOUBLE_ARROW_RIGHT; // Expand action
  ```
- **Tooltip**: Should match: "Collapse Panel" vs "Expand Panel"

---

### 10. Startup State Logging for CLI/Testing

**Goal**: Emit structured log for reproducibility and debugging.

**Implementation**:
- **File**: `src/app/editor/ui/ui_coordinator.cc`
- **Location**: End of constructor or in `DrawWelcomeScreen()` first call
- **Log format**:
  ```cpp
  LOG_INFO("Startup", "State: surface=%s rom=%s welcome=%s dashboard=%s "
           "sidebar=%s panel_expanded=%s active_category=%s",
           GetStartupSurfaceName(current_startup_surface_),
           rom_loaded ? "loaded" : "none",
           ShouldShowWelcome() ? "visible" : "hidden",
           ShouldShowDashboard() ? "visible" : "hidden",
           ShouldShowActivityBar() ? "visible" : "hidden",
           panel_manager_.IsPanelExpanded() ? "expanded" : "collapsed",
           panel_manager_.GetActiveCategory().c_str());
  ```
- **CLI flag**: `--log-startup-state` to enable verbose startup logging

---

## Priority Order for Future Implementation

1. **Status Bar Chips** - High visibility, low complexity
2. **Collapse/Expand Icon Fix** - Quick fix, improves UX
3. **Sidebar Toggle Tooltip** - Quick enhancement
4. **Panel Request Queue** - Medium complexity, high value
5. **Dashboard Quick-Picks** - Medium complexity
6. **Empty State Checklist** - Helps onboarding
7. **Global Search** - High complexity, high value
8. **Sticky Hints** - Medium complexity
9. **Activity Bar Badges** - Nice polish
10. **Startup State Logging** - Developer tooling
