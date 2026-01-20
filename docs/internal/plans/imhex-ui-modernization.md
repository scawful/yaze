# ImHex-Style UI Modernization Plan for YAZE

**Created:** 2026-01-19
**Updated:** 2026-01-20
**Status:** Phase 1-4 Complete (except 3.2, 3.3), Phase 5+ In Progress
**Reference:** `docs/internal/architecture/imhex-comparison-analysis.md`

---

## Implementation Status

| Phase | Task | Status | Notes |
|-------|------|--------|-------|
| 1.1 | List Virtualization | ✅ Done | 7+ files use ImGuiListClipper |
| 1.2 | Lazy Panel Loading | ✅ Done | OnFirstDraw hook added |
| 1.3 | Texture Queue Budget | ✅ Done | ProcessTextureQueueWithBudget exists |
| 2.1 | Animation System | ✅ Done | Animator class implemented |
| 2.2 | Hover Effects | ✅ Done | Activity bar integrated |
| 2.3 | Panel Transitions | ✅ Done | Fade-in/out via Animator in PanelManager |
| 3.1 | Command Palette | ✅ Done | Recent files + history persistence added |
| 3.2 | Contextual Help | ✅ Done | RightPanelManager help panel + quick action URLs |
| 3.3 | Shortcut Overlay | ✅ Done | Search, categorization already implemented |
| 4.1 | Core Events | ✅ Done | Zoom, Selection, PanelVisibility events exist |
| 4.2 | Migrate Callbacks | ✅ Done | PanelManager + EditorRegistry callbacks migrated |
| 4.3 | Deprecate SessionObserver | ✅ Done | No active observers remain (dual-path in place) |
| 5.1 | Panel State Caching | ✅ Done | EditorPanel caching infrastructure |
| 5.2 | Graphics Sheet Caching | ✅ Done | LRU cache in Arena |
| 5.3 | Layout State Persistence | ✅ Done | Panel visibility + ImGui INI persistence |
| 6.x | Provider Abstraction | ❌ Future | |

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

## Phase 1: Performance Foundation ✅ (Mostly Complete)
**Timeline:** Week 1-2 | **Complexity:** Medium | **Impact:** High

### 1.1 List Virtualization ✅ DONE

ImGuiListClipper implemented in:
- `src/app/editor/dungeon/dungeon_room_selector.cc` ✅
- `src/app/editor/music/song_browser_view.cc` ✅
- `src/app/editor/music/tracker_view.cc` ✅
- `src/app/editor/message/message_editor.cc` ✅
- `src/app/editor/palette/palette_group_panel.cc` ✅
- `src/app/gui/widgets/asset_browser.cc` ✅
- `src/app/emu/debug/disassembly_viewer.cc` ✅

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

## Phase 2: Modern UX Patterns ✅ (Mostly Complete)
**Timeline:** Week 2-3 | **Complexity:** Medium | **Impact:** High

### 2.1 Animation System ✅ DONE

Implemented in `src/app/gui/animation/animator.h/cc`:
- `Lerp()`, `EaseOutCubic()`, `LerpColor()` utilities
- Panel-scoped animation state management
- `ClearAnimationsForPanel()` for cleanup on unregister
- Integrated with `ThemeManager::enable_animations`

### 2.2 Hover Effects and Feedback ✅ DONE

- `src/app/gui/widgets/themed_widgets.h` - Animated button widgets
- `src/app/editor/menu/activity_bar.cc` - Hover animations integrated
- `src/app/editor/system/panel_manager.cc` - Clears animations on unregister

### 2.3 Panel Transitions ✅ DONE

Smooth panel show/hide transitions implemented using existing Animator class.

**Implementation:**
- `src/app/editor/system/panel_manager.cc` - Uses `Animator::Animate()` in `DrawAllVisiblePanels()`
- Alpha animates from 0→1 on show, 1→0 on hide
- Applied via `ImGui::PushStyleVar(ImGuiStyleVar_Alpha, current_alpha)`
- Respects `ThemeManager::enable_animations` setting

---

## Phase 3: Enhanced Discoverability
**Timeline:** Week 3-4 | **Complexity:** Low | **Impact:** High

### 3.1 Universal Command Palette ✅ DONE

CommandPalette now includes all discoverable actions with history persistence.

**Implemented features:**
- ✅ Panel visibility toggles: "Show/Hide Room Selector"
- ✅ Editor switches: "Switch to Dungeon Editor"
- ✅ Layout presets: "Apply Developer Layout"
- ✅ Recent files: "Open Recent: zelda3.sfc" (via `RegisterRecentFilesCommands()`)
- ✅ Frecency scoring (usage count + recency bonus)
- ✅ History persistence: `SaveHistory()`/`LoadHistory()` to JSON

**Files modified:**
- `src/app/editor/system/command_palette.h/cc` - Added `RegisterRecentFilesCommands()`, `SaveHistory()`, `LoadHistory()`
- `src/app/editor/ui/ui_coordinator.cc` - Integrated recent files and history in `InitializeCommandPalette()`

### 3.2 Contextual Help System ✅ DONE
Add context-aware help based on current editor.

**Implemented:**
- RightPanelManager help panel shows context-aware content based on active editor
- `SetActiveEditor()` called from `EditorManager::SetCurrentEditor()` to update context
- Editor-specific shortcuts displayed per editor type
- Editor-specific help content for each editor
- Quick action buttons with working URLs (docs, GitHub issues, Discord)

**Files:**
- `src/app/editor/menu/right_panel_manager.h/cc` - Full help panel implementation

### 3.3 Keyboard Shortcut Overlay ✅ DONE
KeyboardShortcuts overlay already has all features implemented.

**Implemented:**
- Search filter (`ImGui::InputTextWithHint`)
- Category sections with collapsible headers
- Table display with Shortcut, Description, Context columns
- Context-aware filtering
- '?' key toggle and Escape to close
- Semi-transparent background with centered modal

**Files:**
- `src/app/gui/keyboard_shortcuts.cc` - Full overlay implementation

**Note:** The overlay exists but is not integrated into the main application loop.
The RightPanelManager help panel provides similar functionality and is active.

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

### 4.2 Migrate Direct Callbacks ✅ DONE
Replace callback functions with EventBus where appropriate.

**Implemented:**
- Added `UIActionRequestEvent` with enum for all activity bar actions
- Added `JumpToRoomRequestEvent` and `JumpToMapRequestEvent` for navigation
- `PanelManager` publishes events via `SetEventBus()` (Trigger* methods prefer EventBus)
- `EditorManager` subscribes to `UIActionRequestEvent` via `HandleUIActionRequest()`
- `EditorActivator` subscribes to navigation events for JumpToDungeon/JumpToOverworld
- All callback setters deprecated with `[[deprecated]]` attribute
- Old callbacks kept for backward compatibility (fallback if EventBus not set)

**Files modified:**
- `src/app/editor/events/core_events.h` - Added UIActionRequestEvent, JumpTo*RequestEvent
- `src/app/editor/system/panel_manager.h` - SetEventBus(), EventBus-aware Trigger* methods
- `src/app/editor/system/editor_registry.h/cc` - Deprecated navigation callbacks
- `src/app/editor/system/editor_activator.h/cc` - EventBus subscription for navigation
- `src/app/editor/editor_manager.h/cc` - HandleUIActionRequest(), SetEventBus() call

### 4.3 Complete SessionObserver Deprecation ✅ DONE

SessionObserver interface is deprecated and no active implementations remain.

**Status:**
- Interface marked with `[[deprecated]]` attribute
- `SessionCoordinator` uses dual-path notification: notifies observers AND publishes events
- `EditorManager` fully migrated to EventBus subscriptions
- No other classes extend SessionObserver

**Migration path for future code:**
```cpp
// Old pattern (deprecated)
class MyEditor : public SessionObserver {
  void OnSessionSwitched(...) override { ... }
};

// New pattern (use this)
event_bus_->Subscribe<SessionSwitchedEvent>([this](const auto& e) { ... });
```

---

## Phase 5: Caching and State Management
**Timeline:** Week 5-6 | **Complexity:** High | **Impact:** Medium

### 5.1 Panel State Caching ✅ DONE
Cache expensive panel computations.

**Implemented in `src/app/editor/system/editor_panel.h`:**
- `InvalidateCache()` - Marks cache as stale
- `GetCached<T>(key, compute)` - Get or compute cached value
- `IsCacheValid()` - Check cache validity
- `ClearCache()` - Clear all cached values and free memory

**Usage example:**
```cpp
class MyPanel : public EditorPanel {
  int GetExpensiveResult() {
    return GetCached<int>("expensive_result", [this]() {
      return ComputeExpensiveValue();
    });
  }

  void OnDataChanged() {
    InvalidateCache();  // Mark stale, recompute on next access
  }
};
```

### 5.2 Graphics Sheet Caching ✅ DONE
Optimize frequent graphics sheet access with LRU eviction.

**Implemented in `src/app/gfx/resource/arena.h/cc`:**
- `TouchSheet(sheet_index)` - Mark sheet as recently accessed
- `GetSheetWithCache(sheet_index)` - Get sheet with auto LRU tracking + texture creation
- `SetSheetCacheSize(max_size)` - Configure max cached textures (default 64)
- `EvictLRUSheets(count)` - Manual eviction for memory management
- `SheetCacheStats` struct with hit rate, evictions, current size

**Usage example:**
```cpp
// Instead of direct array access:
auto& sheet = Arena::Get().gfx_sheets()[i];

// Use cached access for automatic texture management:
auto* sheet = Arena::Get().GetSheetWithCache(i);
// - Tracks LRU order
// - Queues texture creation if needed
// - Evicts old textures when cache full
```

**Benefits:**
- Reduces GPU memory by limiting cached textures (64 of 223 sheets)
- O(1) access and LRU updates via doubly-linked list + hash map
- Automatic eviction of least-used sheet textures
- Statistics for cache performance monitoring

### 5.3 Layout State Persistence ✅ DONE
Save and restore panel states efficiently.

**Implemented across three files:**

**`src/app/editor/system/panel_manager.h/cc`:**
- `GetVisiblePanelIds(session_id)` - Get list of visible panels
- `SetVisiblePanels(session_id, panel_ids)` - Restore visible panels
- `SerializeVisibilityState(session_id)` - Export visibility map
- `RestoreVisibilityState(session_id, state)` - Import visibility map
- `SerializePinnedState()` / `RestorePinnedState()` - Pinned panel persistence

**`src/app/editor/layout/layout_manager.h/cc`:**
- `SaveCurrentLayout(name)` - Save named layout (panels + ImGui docking)
- `LoadLayout(name)` - Restore named layout
- `DeleteLayout(name)` - Remove saved layout
- `GetSavedLayoutNames()` - List all saved layouts
- `HasLayout(name)` - Check if layout exists

**`src/app/editor/system/user_settings.h/cc`:**
- `panel_visibility_state` - Per-editor panel visibility persistence
- `pinned_panels` - Pinned panels (persists across sessions)
- `saved_layouts` - Named workspace configurations
- INI serialization for all new state types

**Usage:**
```cpp
// Save current workspace
layout_manager.SaveCurrentLayout("MyWorkspace");

// Later, restore it
layout_manager.LoadLayout("MyWorkspace");

// List available layouts
auto names = layout_manager.GetSavedLayoutNames();
```

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

## Recommended Next Steps

**Phases 1-5 complete.** All performance and caching work is done.

### Future (P3)
1. **Phase 6: Provider Abstraction** - DataProvider interface for patches, network, etc.

### Reference (Already Complete)
- ✅ List Virtualization: 7+ files using ImGuiListClipper
- ✅ Lazy Panel Loading: `OnFirstDraw()` hook in EditorPanel
- ✅ Texture Budget: `ProcessTextureQueueWithBudget()` in Arena
- ✅ Animation System: Animator class with activity bar integration
- ✅ Hover Effects: Themed widgets with animation support
- ✅ Panel Transitions: Fade-in/out via Animator in PanelManager::DrawAllVisiblePanels
- ✅ Command Palette: Recent files + JSON history persistence
- ✅ Core Events: Zoom, Selection, PanelVisibility events
- ✅ Callback Migration: PanelManager + EditorRegistry migrated to EventBus
- ✅ SessionObserver Deprecated: Dual-path to EventBus, no active observers
- ✅ Contextual Help: RightPanelManager help panel with quick action URLs
- ✅ Shortcut Overlay: Search, categorization, context-aware filtering
- ✅ Panel State Caching: EditorPanel caching infrastructure
- ✅ Graphics Sheet Caching: LRU cache in Arena (64 sheet max, auto-eviction)
- ✅ Layout State Persistence: PanelManager serialization + UserSettings + LayoutManager named layouts
