# Yaze vs ImHex Architecture Analysis

**Date:** January 2026
**Purpose:** Identify usability concerns and architectural improvements by comparing with ImHex

## Executive Summary

Both yaze and [ImHex](https://github.com/WerWolv/ImHex) use ImGui with docking for specialized binary editing. However, their architectural approaches differ significantly:

| Aspect | yaze | ImHex |
|--------|------|-------|
| **Core Pattern** | Specialized Managers (delegation) | ContentRegistry (plugin-first) |
| **View Abstraction** | `EditorPanel` + `Editor` (two hierarchies) | Single `View` hierarchy (6 variants) |
| **Component Registration** | Constructor injection via `EditorDependencies` | Static registry via `ContentRegistry::*::add<T>()` |
| **Communication** | Direct references + Observer pattern | Event-driven (publish-subscribe) |
| **Extension Model** | Internal (compile-time) | Plugin (runtime loadable) |

---

## Usability Concerns in Yaze

### 1. Cognitive Overload in Core Classes

**EditorManager** has 90+ member functions and 91 includes despite delegation attempts. This creates:
- Difficulty understanding what's actually needed
- Fear of breaking things when modifying
- Long compile times

**ImHex approach**: The `ImHexApi` namespace organizes into logical subsystems (`System`, `Provider`, `HexEditor`) with focused responsibilities. No single class owns everything.

### 2. Complex Panel ID Scoping

Session-aware panel IDs like `"s0.dungeon.room_list"` require:
- Manual string concatenation
- Helper functions (`StripSessionPrefix()`)
- Easy-to-miss bugs when IDs don't match

```cpp
// Current yaze pattern - error-prone
std::string panel_id = "s" + std::to_string(session_id) + ".dungeon.room_list";
```

**ImHex approach**: Views self-register with a simple name. The system handles scoping internally:
```cpp
ContentRegistry::Views::add<ViewHexEditor>();  // That's it
```

### 3. Scattered State Management

Visibility flags are split across 3+ components:
- `EditorManager`: `session_to_rename_`, `show_workspace_layout`
- `UICoordinator`: `show_editor_selection_`, `show_welcome_screen_`
- `PanelManager`: per-panel visibility

**ImHex approach**: View state is encapsulated in the View itself (`m_windowOpen`). The system queries views, not the other way around.

### 4. Deferred Action Complexity

Two separate deferred queues exist (EditorManager + LayoutCoordinator) because ImGui state can't be modified during rendering. This creates:
- Race conditions if timing is wrong
- Silent failures if actions are dropped
- Debugging difficulty

**ImHex approach**: Three-phase rendering (`frameBegin`/`frame`/`frameEnd`) with `EventFrameBegin` for pre-frame work. Clear separation rather than queue management.

### 5. Limited Discoverability

Key functionality is hidden:
- No central "what can this editor do?" query
- Keyboard shortcuts scattered across files
- Feature availability isn't queryable

**ImHex approach**: `ContentRegistry` is a single point of truth for:
- All views (`ContentRegistry::Views`)
- All settings (`ContentRegistry::Settings`)
- All tools (`ContentRegistry::Tools`)
- All providers (`ContentRegistry::Provider`)

### 6. Startup State Machine Complexity

Three startup surfaces (`kWelcome` → `kDashboard` → `kEditor`) with transitions managed across multiple components. Racing conditions possible.

---

## Key Patterns from ImHex to Adopt

### 1. ContentRegistry Pattern (High Impact)

Instead of constructor injection and manual wiring:

```cpp
// Current yaze (verbose, coupled)
auto* panel = new RoomSelectorPanel();
panel->SetDependencies(deps);
panel_manager_->Register("dungeon.room_list", panel);
```

Adopt a registry pattern:

```cpp
// Proposed (ImHex-style)
namespace ContentRegistry {
  namespace Panels {
    template<typename T>
    void add() {
      // Auto-register, auto-wire dependencies
      s_panels.push_back(std::make_unique<T>());
    }
  }
}

// Usage
ContentRegistry::Panels::add<RoomSelectorPanel>();  // Clean!
```

### 2. View Hierarchy with Variants (Medium Impact)

ImHex has one `View` base with 6 variants:

| Variant | Purpose |
|---------|---------|
| `View::Window` | Standard dockable window |
| `View::Modal` | Blocking dialog |
| `View::Floating` | Non-dockable window |
| `View::Scrolling` | Auto-scrolling content |
| `View::Special` | Custom rendering |
| `View::FullScreen` | Full-screen exclusive |

**Yaze currently has**: `EditorPanel` (panels), `Editor` (full editors), and ad-hoc popup handling. These should be unified.

```cpp
// Proposed unified hierarchy
class Panel {
public:
  class Dockable;   // Standard sidebar panel
  class Modal;      // Popups/dialogs
  class Canvas;     // Full canvas editors (Dungeon, Overworld)
  class Floating;   // Detached windows
};
```

### 3. Event-Driven Communication (High Impact)

ImHex uses publish-subscribe events:
```cpp
// Publisher
EventProviderChanged::post(provider);

// Subscriber
EventProviderChanged::subscribe([](auto& provider) {
  // React to change
});
```

**Yaze currently has**: `EventBus` exists but is underutilized. Most communication is through direct method calls or Observer interfaces.

**Recommendation**: Expand `EventBus` usage for:
- Session changes
- ROM load/unload
- Editor switching
- Selection changes

This eliminates the need for `SessionObserver` interface implementations everywhere.

### 4. Provider Abstraction for Data Sources (Medium Impact)

ImHex's `Provider` pattern abstracts data access:
- Files, memory, disks, remote debugging all use same interface
- Enables undo/redo at the data layer
- Plugins can add new provider types

**Yaze's `Rom` class**: Currently tightly coupled. Consider extracting a `DataProvider` interface:

```cpp
class DataProvider {
public:
  virtual absl::StatusOr<std::vector<uint8_t>> Read(uint64_t offset, size_t size) = 0;
  virtual absl::Status Write(uint64_t offset, std::span<uint8_t> data) = 0;
  virtual uint64_t GetSize() const = 0;
  virtual bool IsDirty() const = 0;
};

class RomProvider : public DataProvider { /* Current Rom logic */ };
class SaveStateProvider : public DataProvider { /* Emulator save states */ };
class PatchProvider : public DataProvider { /* IPS/BPS patches */ };
```

### 5. Simplified Lifecycle Hooks (Low Impact)

ImHex View has clean lifecycle:
```cpp
virtual void onOpen();   // Called when view opens
virtual void onClose();  // Called when view closes
```

**Yaze's `EditorPanel`** already has this, which is good. But the Editor base class has more complex lifecycle that could be simplified.

---

## Concrete Improvement Recommendations

### Phase 1: Reduce Cognitive Load (Quick Wins)

1. **Split EditorManager further**: Extract `EditorLifecycleManager`, `EditorStateManager`
2. **Consolidate visibility flags**: Single `UIState` struct passed by reference
3. **Document initialization order**: Add static assert or builder pattern to enforce

### Phase 2: ContentRegistry Adoption (Medium Effort)

1. Create `ContentRegistry` namespace with:
   - `Panels::add<T>()` for panel registration
   - `Editors::add<T>()` for editor registration
   - `Shortcuts::add()` for keyboard shortcuts
   - `Settings::add()` for settings categories

2. Replace constructor injection with registry lookup:
   ```cpp
   // Before
   explicit RoomSelectorPanel(EditorDependencies deps);

   // After
   class RoomSelectorPanel : public Panel::Dockable {
     Rom& rom() { return ContentRegistry::Context::rom(); }
   };
   ```

### Phase 3: Event System Expansion (Medium Effort)

1. Define core events:
   ```cpp
   EVENT_DEF(RomLoaded, Rom&);
   EVENT_DEF(RomClosed);
   EVENT_DEF(SessionSwitched, size_t old_id, size_t new_id);
   EVENT_DEF(EditorActivated, EditorType);
   EVENT_DEF(SelectionChanged, std::vector<size_t>);
   ```

2. Replace `SessionObserver` with event subscriptions
3. Remove direct callbacks in favor of events

### Phase 4: Unified View Hierarchy (Higher Effort)

1. Create `View` base class with variants (Window, Modal, Canvas, etc.)
2. Merge `EditorPanel` and `Editor` concepts
3. Standardize `drawContent()` / `drawMenu()` pattern

---

## Architecture Comparison Diagram

```
┌─────────────────────────────────────────────────────────────────────┐
│                          IMHEX PATTERN                              │
├─────────────────────────────────────────────────────────────────────┤
│  ContentRegistry  ───────────────┐                                  │
│    ├── Views::add<T>()           │                                  │
│    ├── Providers::add<T>()       ├──► EventManager ──► Components   │
│    ├── Settings::add()           │    (pub-sub)                     │
│    └── Tools::add()              │                                  │
│                                  │                                  │
│  View (base)                     │                                  │
│    ├── Window     (dockable)     │                                  │
│    ├── Modal      (dialog)       │                                  │
│    ├── Floating   (detached)     │                                  │
│    └── Special    (custom)       │                                  │
└─────────────────────────────────────────────────────────────────────┘

┌─────────────────────────────────────────────────────────────────────┐
│                          YAZE CURRENT                               │
├─────────────────────────────────────────────────────────────────────┤
│  EditorManager (90+ methods) ────┐                                  │
│    ├── PanelManager              │                                  │
│    ├── LayoutManager             ├──► Direct calls + SessionObserver│
│    ├── UICoordinator             │                                  │
│    ├── PopupManager              │                                  │
│    └── SessionCoordinator        │                                  │
│                                  │                                  │
│  EditorPanel (panels)            │                                  │
│  Editor (full editors)           │   ◄── Two separate hierarchies   │
│  Popups (ad-hoc)                 │                                  │
└─────────────────────────────────────────────────────────────────────┘

┌─────────────────────────────────────────────────────────────────────┐
│                          YAZE PROPOSED                              │
├─────────────────────────────────────────────────────────────────────┤
│  ContentRegistry ────────────────┐                                  │
│    ├── Panels::add<T>()          │                                  │
│    ├── Editors::add<T>()         ├──► EventBus ──► Components       │
│    ├── Shortcuts::add()          │    (expanded)                    │
│    └── Settings::add()           │                                  │
│                                  │                                  │
│  View (unified base)             │                                  │
│    ├── View::Dockable (panels)   │                                  │
│    ├── View::Canvas   (editors)  │   ◄── Single hierarchy           │
│    ├── View::Modal    (popups)   │                                  │
│    └── View::Floating (detached) │                                  │
└─────────────────────────────────────────────────────────────────────┘
```

---

## Summary

**ImHex's strengths:**
- Plugin-first design with clear extension points
- Single ContentRegistry for all registration
- Event-driven, loosely coupled communication
- Clean View hierarchy with variants
- Provider abstraction for data access

**Yaze's current pain points:**
- EditorManager bloat despite delegation
- Complex session-aware panel IDs
- Scattered state management
- Two deferred action queues
- Two-hierarchy (Panel vs Editor) complexity

**Priority improvements:**
1. **ContentRegistry pattern** - biggest bang for buck
2. **Expand EventBus** - reduce coupling
3. **Unify View hierarchy** - eliminate Panel/Editor split
4. **Provider abstraction** - enable new data sources

---

## References

- [ImHex GitHub Repository](https://github.com/WerWolv/ImHex)
- [ImHex Provider System (DeepWiki)](https://deepwiki.com/WerWolv/ImHex/2.1-provider-system)
- [ImHex Plugins Development Guide](https://github.com/WerWolv/ImHex/wiki/Plugins-Development-Guide)
- [ImHex Architecture Overview (DeepWiki)](https://deepwiki.com/WerWolv/ImHex/1-overview)
