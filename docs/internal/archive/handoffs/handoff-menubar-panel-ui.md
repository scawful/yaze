# Handoff: Menu Bar & Right Panel UI/UX Improvements

**Date:** 2025-11-26  
**Status:** Complete  
**Agent:** UI/UX improvements session

## Summary

This session focused on improving the ImGui menubar UI/UX and right panel styling. All improvements have been successfully implemented, including the fix for panel toggle button positioning.

## Completed Work

### 1. Menu Bar Button Styling (ui_coordinator.cc)
- Created `DrawMenuBarIconButton()` helper for consistent button styling across all menubar buttons
- Created `GetMenuBarIconButtonWidth()` for accurate dynamic width calculations
- Unified styling: transparent background, consistent hover/active states, proper text colors
- Panel button count now dynamic based on `YAZE_WITH_GRPC` (4 vs 3 buttons)

### 2. Responsive Menu Bar
- Added responsive behavior that hides elements when window is narrow
- Priority order: bell/dirty always shown, then version, session, panel toggles
- Prevents elements from overlapping or being clipped

### 3. Right Panel Header Enhancement (right_panel_manager.cc)
- Elevated header background using `SurfaceContainerHigh`
- Larger close button (28x28) with rounded corners
- **Escape key** now closes panels
- Better visual hierarchy with icon + title

### 4. Panel Styling Helpers
Added reusable styling functions in `RightPanelManager`:
- `BeginPanelSection()` / `EndPanelSection()` - Collapsible sections with icons
- `DrawPanelDivider()` - Themed separators
- `DrawPanelLabel()` - Secondary text labels
- `DrawPanelValue()` - Label + value pairs  
- `DrawPanelDescription()` - Wrapped description text

### 5. Panel Content Styling
Applied new styling to:
- Help panel - Sections with icons, keyboard shortcuts formatted
- Properties panel - Placeholder with styled sections
- Agent/Proposals/Settings - Improved unavailable state messages

### 6. Left Sidebar Width Fix (editor_manager.cc)
Fixed `DrawPlaceholderSidebar()` to use same width logic as `GetLeftLayoutOffset()`:
- Tree view mode → 200px
- Icon view mode → 48px

This eliminated blank space caused by width mismatch.

### 7. Fixed Panel Toggle Positioning (SOLVED)

**Problem:** When the right panel (Agent, Settings, etc.) opens, the menubar status cluster elements shifted left because the dockspace window shrinks. This made it harder to quickly close the panel since the toggle buttons moved.

**Root Cause:** The dockspace window itself shrinks when the panel opens (in `controller.cc`). The menu bar is drawn inside this dockspace window, so all elements shift with it.

**Solution:** Use `ImGui::SetCursorScreenPos()` with TRUE viewport coordinates for the panel toggles, while keeping them inside the menu bar context.

The key insight is to use `ImGui::GetMainViewport()` to get the actual viewport dimensions (which don't change when panels open), then calculate the screen position for the panel toggles based on that. This is different from using `ImGui::GetWindowWidth()` which returns the dockspace window width (which shrinks when panels open).

```cpp
// Get TRUE viewport dimensions (not affected by dockspace resize)
const ImGuiViewport* viewport = ImGui::GetMainViewport();
const float true_viewport_right = viewport->WorkPos.x + viewport->WorkSize.x;

// Calculate screen X position for panel toggles (fixed at viewport right edge)
float panel_screen_x = true_viewport_right - panel_region_width;
if (panel_manager->IsPanelExpanded()) {
  panel_screen_x -= panel_manager->GetPanelWidth();
}

// Get current Y position within menu bar
float menu_bar_y = ImGui::GetCursorScreenPos().y;

// Position at fixed screen coordinates
ImGui::SetCursorScreenPos(ImVec2(panel_screen_x, menu_bar_y));

// Draw panel toggle buttons
panel_manager->DrawPanelToggleButtons();
```

**Why This Works:**
1. Panel toggles stay at a fixed screen position regardless of dockspace resizing
2. Buttons remain inside the menu bar context (same window, same ImGui state)
3. Other menu bar elements (version, dirty, session, bell) shift naturally with the dockspace
4. No z-ordering or visual integration issues (unlike overlay approach)

**What Didn't Work:**
- **Overlay approach**: Drawing panel toggles as a separate floating window had z-ordering issues and visual integration problems (buttons appeared to float disconnected from menu bar)
- **Simple SameLine positioning**: Using window-relative coordinates caused buttons to shift with the dockspace

## Important: Menu Bar Positioning Guide

For future menu bar changes, here's how to handle different element types:

### Elements That Should Shift (Relative Positioning)
Use standard `ImGui::SameLine()` and window-relative coordinates:
```cpp
float start_pos = window_width - element_width - padding;
ImGui::SameLine(start_pos);
ImGui::Text("Element");
```
Example: Version text, dirty indicator, session button, notification bell

### Elements That Should Stay Fixed (Screen Positioning)
Use `ImGui::SetCursorScreenPos()` with viewport coordinates:
```cpp
const ImGuiViewport* viewport = ImGui::GetMainViewport();
float screen_x = viewport->WorkPos.x + viewport->WorkSize.x - element_width;
// Adjust for any panels that might be open
if (panel_is_open) {
  screen_x -= panel_width;
}
float screen_y = ImGui::GetCursorScreenPos().y;  // Keep Y from current context
ImGui::SetCursorScreenPos(ImVec2(screen_x, screen_y));
ImGui::Button("Fixed Element");
```
Example: Panel toggle buttons

### Key Difference
- `ImGui::GetWindowWidth()` - Returns the current window's width (changes when dockspace resizes)
- `ImGui::GetMainViewport()->WorkSize.x` - Returns the actual viewport width (constant)

## Files Modified

| File | Changes |
|------|---------|
| `src/app/editor/ui/ui_coordinator.h` | Added `DrawMenuBarIconButton()`, `GetMenuBarIconButtonWidth()` declarations |
| `src/app/editor/ui/ui_coordinator.cc` | Button helper, dynamic width calc, responsive behavior, screen-position panel toggles |
| `src/app/editor/ui/right_panel_manager.h` | Added styling helper declarations |
| `src/app/editor/ui/right_panel_manager.cc` | Enhanced header, styling helpers, panel content improvements |
| `src/app/editor/editor_manager.cc` | Sidebar toggle styling, placeholder sidebar width fix |
| `docs/internal/ui_layout.md` | Updated documentation with positioning guide |

## Testing Notes

- Build verified: `cmake --build build --target yaze -j8` ✓
- No linter errors
- Escape key closes panels ✓
- Panel header close button works ✓
- Left sidebar width matches allocated space ✓
- **Panel toggles stay fixed when panels open/close** ✓

## References

- See `docs/internal/ui_layout.md` for detailed layout documentation
- Key function: `UICoordinator::DrawMenuBarExtras()` in `src/app/editor/ui/ui_coordinator.cc`
