#ifndef YAZE_APP_EDITOR_EVENTS_CORE_EVENTS_H_
#define YAZE_APP_EDITOR_EVENTS_CORE_EVENTS_H_

#include <cstddef>
#include <string>
#include <vector>

#include "app/editor/core/event_bus.h"

namespace yaze {
class Rom;

namespace editor {

class RomSession;

// =============================================================================
// ROM Lifecycle Events
// =============================================================================

/**
 * @brief Published when a ROM is successfully loaded into a session.
 *
 * Subscribers can use this to initialize ROM-dependent state, update UI,
 * or trigger data loading operations.
 */
struct RomLoadedEvent : public Event {
  Rom* rom = nullptr;
  std::string filename;
  size_t session_id = 0;

  static RomLoadedEvent Create(Rom* r, const std::string& file, size_t session) {
    RomLoadedEvent e;
    e.rom = r;
    e.filename = file;
    e.session_id = session;
    return e;
  }
};

/**
 * @brief Published when a ROM is unloaded from a session.
 *
 * Subscribers should release any ROM-dependent resources and reset state.
 */
struct RomUnloadedEvent : public Event {
  size_t session_id = 0;

  static RomUnloadedEvent Create(size_t session) {
    RomUnloadedEvent e;
    e.session_id = session;
    return e;
  }
};

/**
 * @brief Published when ROM data is modified.
 *
 * Used for dirty flag tracking, cache invalidation, and WASM synchronization.
 * The address field is optional - set to 0 for bulk/unknown changes.
 */
struct RomModifiedEvent : public Event {
  Rom* rom = nullptr;
  uint32_t address = 0;
  size_t byte_count = 0;
  size_t session_id = 0;

  static RomModifiedEvent Create(Rom* r, size_t session, uint32_t addr = 0,
                                  size_t bytes = 0) {
    RomModifiedEvent e;
    e.rom = r;
    e.session_id = session;
    e.address = addr;
    e.byte_count = bytes;
    return e;
  }
};

// =============================================================================
// Session Lifecycle Events
// =============================================================================

/**
 * @brief Published when the active session changes.
 *
 * Use this to update cross-component state without tight coupling.
 */
struct SessionSwitchedEvent : public Event {
  size_t old_index = 0;
  size_t new_index = 0;
  RomSession* session = nullptr;

  static SessionSwitchedEvent Create(size_t old_idx, size_t new_idx,
                                     RomSession* sess) {
    SessionSwitchedEvent e;
    e.old_index = old_idx;
    e.new_index = new_idx;
    e.session = sess;
    return e;
  }
};

/**
 * @brief Published when a new session is created.
 */
struct SessionCreatedEvent : public Event {
  size_t index = 0;
  RomSession* session = nullptr;

  static SessionCreatedEvent Create(size_t idx, RomSession* sess) {
    SessionCreatedEvent e;
    e.index = idx;
    e.session = sess;
    return e;
  }
};

/**
 * @brief Published when a session is closed.
 */
struct SessionClosedEvent : public Event {
  size_t index = 0;

  static SessionClosedEvent Create(size_t idx) {
    SessionClosedEvent e;
    e.index = idx;
    return e;
  }
};

// =============================================================================
// Editor State Events
// =============================================================================

/**
 * @brief Published when the active editor changes.
 */
struct EditorSwitchedEvent : public Event {
  int editor_type = 0;  // EditorType enum value
  void* editor = nullptr;

  static EditorSwitchedEvent Create(int type, void* ed) {
    EditorSwitchedEvent e;
    e.editor_type = type;
    e.editor = ed;
    return e;
  }
};

// =============================================================================
// UI State Events
// =============================================================================

/**
 * @brief Published when selection changes in any editor.
 *
 * Subscribers can respond to selection changes for cross-component updates,
 * status bar updates, or property panel refreshes.
 */
struct SelectionChangedEvent : public Event {
  std::string source;              // Source editor: "overworld", "dungeon", "graphics", etc.
  std::vector<int> selected_ids;   // IDs of selected items (tiles, rooms, sprites, etc.)
  int primary_id = -1;             // Primary selection (first or focused item)
  size_t session_id = 0;

  static SelectionChangedEvent Create(const std::string& src,
                                       const std::vector<int>& ids,
                                       size_t session = 0) {
    SelectionChangedEvent e;
    e.source = src;
    e.selected_ids = ids;
    e.primary_id = ids.empty() ? -1 : ids.front();
    e.session_id = session;
    return e;
  }

  static SelectionChangedEvent CreateSingle(const std::string& src, int id,
                                             size_t session = 0) {
    SelectionChangedEvent e;
    e.source = src;
    e.selected_ids = {id};
    e.primary_id = id;
    e.session_id = session;
    return e;
  }

  static SelectionChangedEvent CreateEmpty(const std::string& src,
                                            size_t session = 0) {
    SelectionChangedEvent e;
    e.source = src;
    e.selected_ids = {};
    e.primary_id = -1;
    e.session_id = session;
    return e;
  }

  bool IsEmpty() const { return selected_ids.empty(); }
  size_t Count() const { return selected_ids.size(); }
};

/**
 * @brief Published when panel visibility changes.
 *
 * Use for layout persistence, analytics, or cross-panel coordination.
 * More granular than PanelToggleEvent - includes session context.
 */
struct PanelVisibilityChangedEvent : public Event {
  std::string panel_id;      // Panel identifier (may be session-prefixed)
  std::string base_panel_id; // Base panel ID without session prefix
  std::string category;      // Panel category ("Dungeon", "Overworld", etc.)
  bool visible = false;      // New visibility state
  size_t session_id = 0;

  static PanelVisibilityChangedEvent Create(const std::string& id,
                                             const std::string& base_id,
                                             const std::string& cat,
                                             bool vis, size_t session = 0) {
    PanelVisibilityChangedEvent e;
    e.panel_id = id;
    e.base_panel_id = base_id;
    e.category = cat;
    e.visible = vis;
    e.session_id = session;
    return e;
  }
};

/**
 * @brief Published when zoom level changes in any canvas/editor.
 *
 * Use for synchronized zoom across linked views or status bar updates.
 */
struct ZoomChangedEvent : public Event {
  std::string source;     // Source canvas: "overworld_canvas", "dungeon_canvas", etc.
  float old_zoom = 1.0f;  // Previous zoom level
  float new_zoom = 1.0f;  // New zoom level
  size_t session_id = 0;

  static ZoomChangedEvent Create(const std::string& src, float old_z,
                                  float new_z, size_t session = 0) {
    ZoomChangedEvent e;
    e.source = src;
    e.old_zoom = old_z;
    e.new_zoom = new_z;
    e.session_id = session;
    return e;
  }

  float GetZoomDelta() const { return new_zoom - old_zoom; }
  float GetZoomRatio() const {
    return old_zoom > 0.0f ? new_zoom / old_zoom : 1.0f;
  }
  bool IsZoomIn() const { return new_zoom > old_zoom; }
  bool IsZoomOut() const { return new_zoom < old_zoom; }
};

// =============================================================================
// Frame Lifecycle Events (for deferred action consolidation)
// =============================================================================

/**
 * @brief Published at the beginning of each frame (pre-ImGui).
 *
 * Use for operations that do not require a valid ImGui frame.
 */
struct FrameBeginEvent : public Event {
  float delta_time = 0.0f;

  static FrameBeginEvent Create(float dt) {
    FrameBeginEvent e;
    e.delta_time = dt;
    return e;
  }
};

/**
 * @brief Published after ImGui::NewFrame and dockspace creation.
 *
 * Use for operations that need a valid ImGui frame/window (e.g., DockBuilder).
 */
struct FrameGuiBeginEvent : public Event {
  float delta_time = 0.0f;

  static FrameGuiBeginEvent Create(float dt) {
    FrameGuiBeginEvent e;
    e.delta_time = dt;
    return e;
  }
};

/**
 * @brief Published at the end of each frame.
 *
 * Use for cleanup operations or state finalization.
 */
struct FrameEndEvent : public Event {
  float delta_time = 0.0f;

  static FrameEndEvent Create(float dt) {
    FrameEndEvent e;
    e.delta_time = dt;
    return e;
  }
};

}  // namespace editor
}  // namespace yaze

#endif  // YAZE_APP_EDITOR_EVENTS_CORE_EVENTS_H_
