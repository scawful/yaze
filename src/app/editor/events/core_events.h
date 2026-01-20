#ifndef YAZE_APP_EDITOR_EVENTS_CORE_EVENTS_H_
#define YAZE_APP_EDITOR_EVENTS_CORE_EVENTS_H_

#include <cstddef>
#include <string>

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
