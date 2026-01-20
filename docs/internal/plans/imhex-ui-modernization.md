# ImHex-Style UI Modernization Plan for YAZE

**Created:** 2026-01-19
**Status:** Planning Complete
**Reference:** `docs/internal/architecture/imhex-comparison-analysis.md`

---

## Executive Summary

This plan outlines a phased approach to modernizing YAZE's UI to match ImHex's performance and user experience standards. The current architecture already includes ContentRegistry, EventBus, and PanelManager foundations - this plan focuses on expanding their usage and adding missing capabilities.

---

## Current Architecture Assessment

### Strengths (Already Implemented)
1. **ContentRegistry pattern** - Panel factories, Context, Shortcuts, Settings registries
2. **EventBus** - Publish-subscribe system with core events
3. **PanelManager** - Session-aware panel management with visibility control
4. **Arena** - Deferred texture command system for async loading
5. **ThemeManager** - Comprehensive theming with Material Design colors
6. **CommandPalette** - Basic fuzzy search implementation
7. **ToastManager** - Notification system with history
8. **KeyboardShortcuts** - Context-aware shortcut system

### Gaps vs ImHex
1. **Performance**: No virtualization in most lists, limited lazy loading
2. **Modern UX**: No animations/transitions, limited hover effects
3. **Discoverability**: Command palette exists but not integrated everywhere
4. **Caching**: Limited state caching for expensive computations

---

## Lifecycle Semantics Addendum

This addendum clarifies lifecycle rules that cut across the phases below.

- **Lazy init scope**: `EditorPanel::OnFirstDraw()` runs once per session. Reset lazy-init on `SessionSwitchedEvent` (or `OnSessionSwitched()` helper) and expose an `InvalidateLazyInit()` path for panels that subscribe to session changes.
- **Animation lifecycle**: Animation state is panel-scoped. Panels must clear their animator state on teardown (`ClearAnimationsForPanel(panel_id)`) and all animations must be gated by `ThemeManager::enable_animations` (headless builds default to disabled).
- **Panel transitions**: Fade-out should disable input and panel transition state must be cleaned up on panel unregister/destruction to avoid hidden interactive panels.
- **Cache invalidation**: Cached panel data must be invalidated on session switches and data mutations; prefer value returns or shared ownership (`std::shared_ptr`) over dangling references.
- **Layout persistence**: Global panels (`PanelScope::kGlobal`) persist to `layouts/global.json`; session-scoped panels persist to `layouts/session_{id}.json`.

---

## Phase 1: Performance Foundation
**Timeline:** Week 1-2 | **Complexity:** Medium | **Impact:** High

### 1.1 List Virtualization (P0)
Add ImGuiListClipper to all large lists.

**Files to modify:**
- `src/app/editor/dungeon/panels/dungeon_room_panel.cc` - Room list (296 rooms)
- `src/app/editor/music/song_browser_view.cc` - Song list
- `src/app/editor/message/message_editor.cc` - Message list (200+ messages)
- `src/app/editor/palette/palette_group_panel.cc` - Palette entries

**Reference implementation:** `src/app/gui/widgets/asset_browser.cc`

```cpp
// Pattern to follow
ImGuiListClipper clipper;
clipper.Begin(items.size(), item_height);
while (clipper.Step()) {
  for (int i = clipper.DisplayStart; i < clipper.DisplayEnd; i++) {
    DrawItem(items[i]);
  }
}
```

### 1.2 Lazy Panel Loading (P2)
Extend EditorPanel with lazy initialization.

**Files to modify:**
- `src/app/editor/system/editor_panel.h` - Add `OnFirstDraw()` hook
- `src/app/editor/system/panel_manager.cc` - Track first-draw state

**API additions:**
```cpp
class EditorPanel {
  virtual void OnFirstDraw() {}  // Called once before first Draw()
  virtual bool RequiresLazyInit() const { return false; }
};
```

### 1.3 Texture Queue Budget (P1)
Implement frame-budget aware texture loading.

**Files to modify:**
- `src/app/gfx/resource/arena.h/cc` - Add `ProcessTextureQueueWithBudget()`

```cpp
bool Arena::ProcessTextureQueueWithBudget(IRenderer* renderer, float budget_ms) {
  auto start = std::chrono::high_resolution_clock::now();
  while (!texture_command_queue_.empty()) {
    ProcessSingleTexture(renderer);
    auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::high_resolution_clock::now() - start);
    if (elapsed.count() > budget_ms) break;
  }
  return texture_command_queue_.empty();
}
```

---

## Phase 2: Modern UX Patterns
**Timeline:** Week 2-3 | **Complexity:** Medium | **Impact:** High

### 2.1 Animation System (P1)
Implement the animation flags already defined in ThemeManager.

**New files:** `src/app/gui/animation/animator.h`, `src/app/gui/animation/animator.cc`

```cpp
class Animator {
public:
  static float Lerp(float a, float b, float t);
  static float EaseOutCubic(float t);
  static ImVec4 LerpColor(ImVec4 a, ImVec4 b, float t);

  // Managed animations with IDs for persistent state
  float Animate(const std::string& panel_id, const std::string& anim_id,
                float target, float speed = 5.0f);
  ImVec4 AnimateColor(const std::string& panel_id, const std::string& anim_id,
                      ImVec4 target, float speed = 5.0f);
  void ClearAnimationsForPanel(const std::string& panel_id);
  bool IsEnabled() const;
};
```

**Files to modify:**
- `src/app/gui/core/ui_helpers.h/cc` - Add animation utilities
- Wire up to `ThemeManager::enable_animations` flag

### 2.2 Hover Effects and Feedback (P1)
Add consistent hover states across all interactive elements.

**Files to modify:**
- `src/app/gui/core/style.cc` - Add hover style utilities
- `src/app/editor/menu/activity_bar.cc` - Add hover animations
- `src/app/gui/widgets/themed_widgets.h` - Create animated button widgets

### 2.3 Panel Transitions (P2)
Smooth panel show/hide transitions.

**Files to modify:**
- `src/app/editor/system/panel_manager.cc` - Add transition state tracking

```cpp
struct PanelTransitionState {
  float current_alpha = 0.0f;
  float target_alpha = 0.0f;
  bool transitioning = false;
};
std::unordered_map<std::string, PanelTransitionState> panel_transitions_;
```

---

## Phase 3: Enhanced Discoverability
**Timeline:** Week 3-4 | **Complexity:** Low | **Impact:** High

### 3.1 Universal Command Palette (P0)
Expand CommandPalette to include all discoverable actions.

**Files to modify:**
- `src/app/editor/system/command_palette.h/cc` - Add category-aware search
- `src/app/editor/ui/ui_coordinator.cc` - Enhance `DrawCommandPalette()`

**Features to add:**
- Panel visibility toggles: "Show/Hide Room Selector"
- Editor switches: "Switch to Dungeon Editor"
- Layout presets: "Apply Developer Layout"
- Recent files: "Open Recent: zelda3.sfc"
- All registered keyboard shortcuts

### 3.2 Contextual Help System (P2)
Add context-aware help based on current editor.

**Files to modify:**
- `src/app/editor/menu/right_panel_manager.h/cc` - Wire up help context

### 3.3 Keyboard Shortcut Overlay (P2)
Enhance the existing KeyboardShortcuts overlay.

**Files to modify:**
- `src/app/gui/keyboard_shortcuts.cc` - Add search, categorization

---

## Phase 4: Event System Expansion
**Timeline:** Week 4-5 | **Complexity:** Medium | **Impact:** Medium

### 4.1 Additional Core Events (P1)
Add events for more granular state changes.

**Files to modify:**
- `src/app/editor/events/core_events.h` - Add new event types

```cpp
// New events to add
struct SelectionChangedEvent : Event {
  std::string source;  // "overworld", "dungeon", etc.
  std::vector<int> selected_ids;
};

struct PanelVisibilityChangedEvent : Event {
  std::string panel_id;
  bool visible;
};

struct ZoomChangedEvent : Event {
  float old_zoom;
  float new_zoom;
};
```

### 4.2 Migrate Direct Callbacks (P1)
Replace callback functions with EventBus where appropriate.

**Files to modify:**
- `src/app/editor/system/panel_manager.cc` - Publish visibility change events
- Individual editors - Subscribe to relevant events

### 4.3 Complete SessionObserver Deprecation (P2)
Remove remaining SessionObserver usages.

**Pattern:**
```cpp
// Before
class MyEditor : public SessionObserver {
  void OnSessionSwitched(...) override { ... }
};

// After
event_bus_.Subscribe<SessionSwitchedEvent>([this](const auto& e) { ... });
```

---

## Phase 5: Caching and State Management
**Timeline:** Week 5-6 | **Complexity:** High | **Impact:** Medium

### 5.1 Panel State Caching (P2)
Cache expensive panel computations.

**Files to modify:**
- `src/app/editor/system/editor_panel.h` - Add cache invalidation hooks

```cpp
class EditorPanel {
protected:
  void InvalidateCache() { cache_valid_ = false; }

  template<typename T>
  T& GetCached(const std::string& key, std::function<T()> compute) {
    if (!cache_valid_ || !cache_.contains(key)) {
      cache_[key] = compute();
    }
    return std::any_cast<T&>(cache_[key]);
  }

private:
  bool cache_valid_ = false;
  std::unordered_map<std::string, std::any> cache_;
};
```

### 5.2 Graphics Sheet Caching (P2)
Optimize frequent graphics sheet access.

**Files to modify:**
- `src/app/gfx/resource/arena.h/cc` - Add LRU cache for rendered sheets

### 5.3 Layout State Persistence (P2)
Save and restore panel states efficiently.

**Files to modify:**
- `src/app/editor/layout/layout_manager.cc` - Add state serialization
- `src/app/editor/system/user_settings.cc` - Add panel state persistence

---

## Phase 6: Provider Abstraction (Future)
**Timeline:** Week 6-7 | **Complexity:** High | **Impact:** Low (foundational)

### 6.1 DataProvider Interface
Abstract data access for future extensibility (patches, network, etc.).

**New file:** `src/app/core/data_provider.h`

```cpp
class DataProvider {
public:
  virtual absl::StatusOr<std::vector<uint8_t>> Read(uint64_t offset, size_t size) = 0;
  virtual absl::Status Write(uint64_t offset, absl::Span<const uint8_t> data) = 0;
  virtual uint64_t GetSize() const = 0;
  virtual bool IsDirty() const = 0;
  virtual std::string GetName() const = 0;
};

class RomProvider : public DataProvider { /* Wraps Rom */ };
class PatchProvider : public DataProvider { /* IPS/BPS patches */ };
```

---

## Priority Matrix

| Task | Effort | Impact | Priority | Phase |
|------|--------|--------|----------|-------|
| List Virtualization | Low | High | **P0** | 1 |
| Command Palette Expansion | Low | High | **P0** | 3 |
| Animation System | Medium | Medium | P1 | 2 |
| Texture Budget | Low | Medium | P1 | 1 |
| Core Events Expansion | Medium | High | P1 | 4 |
| Hover Effects | Medium | Medium | P1 | 2 |
| Lazy Panel Loading | Medium | Medium | P2 | 1 |
| Panel Transitions | Medium | Medium | P2 | 2 |
| Panel State Caching | High | Medium | P2 | 5 |
| DataProvider | High | Low | P3 | 6 |

---

## Dependencies Graph

```
Phase 1 (Performance) ─────────────────────────────────────┐
    │                                                      │
    v                                                      v
Phase 2 (UX) ──────> Phase 3 (Discoverability) ──> Phase 4 (Events)
                                                           │
                                                           v
                     Phase 5 (Caching) <───────────────────┘
                            │
                            v
                     Phase 6 (Provider)
```

---

## Success Metrics

- **Frame time**: Target < 16ms (60 FPS) with 500+ items in lists
- **Startup time**: < 2s to first interactive frame
- **Memory**: < 500MB for typical ROM editing session
- **Discoverability**: All features accessible via Command Palette

---

## Risk Mitigation

1. **Backward Compatibility**: All changes are additive; existing patterns continue to work
2. **Incremental Rollout**: Each phase can be shipped independently
3. **Feature Flags**: Use compile-time or runtime flags to gate new UI behaviors
4. **Performance Testing**: Add timing measurements before/after virtualization

---

## Recommended Starting Point

**Start with Phase 1.1 (List Virtualization) and Phase 3.1 (Command Palette)** - these are P0 priorities with low effort and high impact. They can be implemented in parallel as they touch different parts of the codebase.

### First PR: List Virtualization
1. Identify the 3-4 largest lists in the codebase
2. Add ImGuiListClipper following asset_browser.cc pattern
3. Measure frame time improvement

### Second PR: Command Palette Expansion
1. Register all panel toggles in CommandPalette
2. Add editor switch commands
3. Add layout preset commands
4. Test with fuzzy search
