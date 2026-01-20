# YAZE UI Polish Implementation Plan

**Version:** 1.0
**Date:** 2026-01-20
**Status:** In Progress

---

## Overview

This plan covers 6 major UI polish features for YAZE, organized into 4 phases with clear dependencies.

| Phase | Features | Complexity | Est. Time |
|-------|----------|------------|-----------|
| 1 | Complete theme files, smart defaults | Low-Medium | 2-3 hours |
| 2 | Theme preview, accent colors | Medium-High | 3-4 hours |
| 3 | Typography, micro-interactions | Medium | 4-5 hours |
| 4 | Welcome screen polish | Medium | 3-4 hours |

---

## Phase 1: Theme System Completeness (Foundation)

**Goal:** Ensure all theme files have complete property coverage and add smart defaults.

**Dependencies:** None

### Task 1.1: Complete Theme File Properties

**Files:**
- `assets/themes/cyberpunk.theme` - Missing 35 properties
- `assets/themes/forest.theme` - Missing 28 properties
- `assets/themes/midnight.theme` - Missing 28 properties
- `assets/themes/sunset.theme` - Missing 35 properties

**Missing Property Categories:**
- Borders: `border`, `border_shadow`
- Separators: `separator`, `separator_hovered`, `separator_active`
- Scrollbars: `scrollbar_bg`, `scrollbar_grab`, `scrollbar_grab_hovered`, `scrollbar_grab_active`
- Resize grips: `resize_grip`, `resize_grip_hovered`, `resize_grip_active`
- Controls: `check_mark`, `slider_grab`, `slider_grab_active`
- Tables: `table_header_bg`, `table_border_strong`, `table_border_light`, `table_row_bg`, `table_row_bg_alt`
- Links: `text_link`
- Navigation: `input_text_cursor`, `nav_cursor`, `nav_windowing_highlight`, `nav_windowing_dim_bg`, `modal_window_dim_bg`, `text_selected_bg`, `drag_drop_target`
- Docking: `docking_preview`, `docking_empty_bg`
- Tree: `tree_lines`
- Tabs: `tab_dimmed`, `tab_dimmed_selected`, `tab_dimmed_selected_overline`, `tab_selected_overline`

### Task 1.2: Add Smart Default Generation

**File:** `src/app/gui/core/theme_manager.cc`

Add `ApplySmartDefaults(Theme& theme)` method that fills missing properties based on primary colors:

```cpp
void ThemeManager::ApplySmartDefaults(Theme& theme) {
  auto is_unset = [](const Color& c) {
    return c.red == 0.0f && c.green == 0.0f &&
           c.blue == 0.0f && c.alpha == 0.0f;
  };

  auto with_alpha = [](const Color& c, float alpha) {
    return Color{c.red, c.green, c.blue, alpha};
  };

  if (is_unset(theme.border))
    theme.border = theme.primary;
  if (is_unset(theme.scrollbar_bg))
    theme.scrollbar_bg = with_alpha(theme.surface, 0.6f);
  // ... continue for all properties
}
```

### Task 1.3: Theme Validation

**Files:** `src/app/gui/core/theme_manager.h/cc`

Add validation method to report missing properties:

```cpp
struct ThemeValidationResult {
  std::vector<std::string> missing_colors;
  bool is_complete() const { return missing_colors.empty(); }
};
ThemeValidationResult ValidateTheme(const Theme& theme) const;
```

---

## Phase 2: Theme Preview and Accent Color System

**Goal:** Enable theme preview on hover and user-customizable accent colors.

**Dependencies:** Phase 1

### Task 2.1: Theme Preview on Hover

**Files:** `src/app/gui/core/theme_manager.h/cc`

Add preview state:

```cpp
class ThemeManager {
public:
  void StartPreview(const std::string& theme_name);
  void EndPreview();
  bool IsPreviewActive() const { return preview_active_; }

private:
  bool preview_active_ = false;
  Theme original_theme_;
  std::string original_name_;
};
```

Modify `ShowThemeSelector()` to preview on hover.

### Task 2.2: Accent Color System

**Files:**
- New: `src/app/gui/core/color_utils.h/cc`
- Modify: `src/app/gui/core/theme_manager.cc`
- Modify: `src/app/editor/ui/popup_manager.cc`

Add HSL color manipulation:

```cpp
struct HSL { float h, s, l; };
HSL RGBToHSL(const Color& c);
Color HSLToRGB(const HSL& hsl);

struct DerivedColorScheme {
  Color primary, secondary, accent;
  Color error, warning, success, info;
};
DerivedColorScheme DeriveColorsFromAccent(const Color& accent);
```

Add UI in Display Settings:
- Color picker for custom accent
- Quick preset buttons (Forest, Ocean, Sunset, Cyberpunk)

### Task 2.3: Persist Accent Preferences

**Files:** `src/app/editor/system/user_settings.h/cc`

```cpp
struct Preferences {
  std::string current_theme_name = "YAZE Tre";
  bool use_custom_accent = false;
  uint32_t custom_accent_rgba = 0;
};
```

---

## Phase 3: Typography and Micro-interactions

**Goal:** Expand typography controls and add panel transition animations.

**Dependencies:** Phase 1

### Task 3.1: Enhanced Typography UI

**Files:**
- `src/app/editor/system/user_settings.h`
- `src/app/gui/core/style.cc`
- `src/app/editor/ui/popup_manager.cc`

Add UI density presets:

```cpp
struct Preferences {
  float font_global_scale = 1.0f;
  float compact_factor = 1.0f;
  float spacing_multiplier = 1.0f;
  int ui_density = 1;  // 0=Compact, 1=Normal, 2=Comfortable
};
```

UI with density combo and fine-tuning sliders.

### Task 3.2: Panel Transition Animations

**Files:** `src/app/gui/animation/animator.h/cc`

Add new methods:

```cpp
float AnimatePanelVisibility(const std::string& panel_id, bool visible);
float AnimatePanelExpand(const std::string& panel_id, bool expanded);
ImVec2 AnimatePanelSize(const std::string& panel_id, ImVec2 target_size);

static float EaseInOutQuad(float t);
static float EaseOutBack(float t);
```

### Task 3.3: Button Press Feedback

**File:** `src/app/gui/widgets/themed_widgets.cc`

Add press animation (scale down on press) and optional ripple effect.

### Task 3.4: Selection Pulse Animation

**File:** `src/app/gui/widgets/themed_widgets.cc`

Enhance PaletteColorButton selection with pulsing border.

---

## Phase 4: Welcome Screen Polish

**Goal:** Enhance visual hierarchy and add entry animations.

**Dependencies:** Phase 3 (animations)

### Task 4.1: Visual Hierarchy

**File:** `src/app/editor/ui/welcome_screen.cc`

Add section cards with:
- Subtle shadow
- Accent stripe on left
- Gradient hover backgrounds

### Task 4.2: Entry Animations

**Files:** `src/app/editor/ui/welcome_screen.h/cc`

Add staggered entry animations:

```cpp
struct EntryAnimation {
  float progress = 0.0f;
  float delay = 0.0f;
  bool complete = false;
};

EntryAnimation header_anim_;
EntryAnimation quick_actions_anim_;
EntryAnimation recent_projects_anim_;
EntryAnimation templates_anim_;
```

Each section slides/fades in with 50ms stagger.

### Task 4.3: Interactive Enhancement

**File:** `src/app/editor/ui/welcome_screen.cc`

- Triforce attraction toward hovered buttons
- Sparkle effect on close triforce hover

---

## File Change Summary

| File | P1 | P2 | P3 | P4 |
|------|:--:|:--:|:--:|:--:|
| `assets/themes/*.theme` | X | | | |
| `theme_manager.h` | X | X | | |
| `theme_manager.cc` | X | X | | |
| `color_utils.h/cc` (new) | | X | | |
| `user_settings.h/cc` | | X | X | |
| `popup_manager.cc` | | X | X | |
| `animator.h/cc` | | | X | |
| `ui_helpers.h/cc` | | | X | |
| `themed_widgets.cc` | | | X | |
| `welcome_screen.h/cc` | | | | X |

---

## Testing Checklist

- [ ] All theme files load without errors
- [ ] Missing properties get reasonable defaults
- [ ] Theme preview activates on hover, restores on leave
- [ ] Accent color changes propagate to derived colors
- [ ] Typography density presets apply correctly
- [ ] Panel animations are smooth (60fps)
- [ ] Welcome screen animations stagger properly
- [ ] All features work with animations disabled
