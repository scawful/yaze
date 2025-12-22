#include "app/gui/core/touch_input.h"

#include <algorithm>
#include <chrono>
#include <cmath>

#include "imgui/imgui.h"
#include "util/log.h"

#ifdef __EMSCRIPTEN__
#include <emscripten.h>
#include <emscripten/html5.h>
#endif

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

namespace yaze {
namespace gui {

namespace {

// Static state for touch input system
struct TouchInputState {
  bool initialized = false;
  bool touch_mode = false;  // true if last input was touch
  bool pan_zoom_enabled = true;

  // Touch points
  std::array<TouchPoint, TouchInput::kMaxTouchPoints> touch_points;
  int active_touch_count = 0;

  // Gesture recognition state
  GestureState current_gesture;
  GestureState previous_gesture;

  // Tap detection state
  ImVec2 last_tap_position = ImVec2(0, 0);
  double last_tap_time = 0.0;
  bool potential_tap = false;
  bool potential_double_tap = false;

  // Long press detection
  bool long_press_detected = false;
  double touch_start_time = 0.0;

  // Canvas transform state
  ImVec2 pan_offset = ImVec2(0, 0);
  float zoom_level = 1.0f;
  float rotation = 0.0f;
  ImVec2 zoom_center = ImVec2(0, 0);

  // Inertia state
  ImVec2 inertia_velocity = ImVec2(0, 0);
  bool inertia_active = false;

  // Two-finger gesture tracking
  float initial_pinch_distance = 0.0f;
  float initial_rotation_angle = 0.0f;
  ImVec2 initial_pan_center = ImVec2(0, 0);

  // Configuration
  TouchConfig config;

  // Callback
  std::function<void(const GestureState&)> gesture_callback;
};

TouchInputState g_touch_state;

// Helper functions
float Distance(ImVec2 a, ImVec2 b) {
  float dx = b.x - a.x;
  float dy = b.y - a.y;
  return std::sqrt(dx * dx + dy * dy);
}

float Angle(ImVec2 a, ImVec2 b) {
  return std::atan2(b.y - a.y, b.x - a.x);
}

ImVec2 Midpoint(ImVec2 a, ImVec2 b) {
  return ImVec2((a.x + b.x) * 0.5f, (a.y + b.y) * 0.5f);
}

double GetTime() {
#ifdef __EMSCRIPTEN__
  return emscripten_get_now() / 1000.0;  // Convert ms to seconds
#else
  auto now = std::chrono::steady_clock::now();
  auto duration = now.time_since_epoch();
  return std::chrono::duration<double>(duration).count();
#endif
}

TouchPoint* FindTouchById(int id) {
  for (auto& touch : g_touch_state.touch_points) {
    if (touch.active && touch.id == id) {
      return &touch;
    }
  }
  return nullptr;
}

TouchPoint* FindInactiveSlot() {
  for (auto& touch : g_touch_state.touch_points) {
    if (!touch.active) {
      return &touch;
    }
  }
  return nullptr;
}

void CountActiveTouches() {
  g_touch_state.active_touch_count = 0;
  for (const auto& touch : g_touch_state.touch_points) {
    if (touch.active) {
      g_touch_state.active_touch_count++;
    }
  }
}

// Get the first N active touch points
std::array<TouchPoint*, 2> GetFirstTwoTouches() {
  std::array<TouchPoint*, 2> result = {nullptr, nullptr};
  int found = 0;
  for (auto& touch : g_touch_state.touch_points) {
    if (touch.active && found < 2) {
      result[found++] = &touch;
    }
  }
  return result;
}

}  // namespace

// === Public API Implementation ===

void TouchInput::Initialize() {
  if (g_touch_state.initialized) {
    return;
  }

  // Reset all state
  g_touch_state = TouchInputState();
  g_touch_state.initialized = true;

  // Initialize touch points
  for (auto& touch : g_touch_state.touch_points) {
    touch = TouchPoint();
  }

  // Platform-specific initialization
  InitializePlatform();

  LOG_INFO("TouchInput", "Touch input system initialized");
}

void TouchInput::Shutdown() {
  if (!g_touch_state.initialized) {
    return;
  }

  ShutdownPlatform();
  g_touch_state.initialized = false;

  LOG_INFO("TouchInput", "Touch input system shut down");
}

void TouchInput::Update() {
  if (!g_touch_state.initialized) {
    return;
  }

  // Store previous gesture for phase detection
  g_touch_state.previous_gesture = g_touch_state.current_gesture;

  // Update gesture recognition
  UpdateGestureRecognition();

  // Process inertia if active
  if (g_touch_state.inertia_active) {
    ProcessInertia();
  }

  // Translate to ImGui events
  TranslateToImGuiEvents();

  // Invoke callback if gesture state changed
  if (g_touch_state.gesture_callback &&
      (g_touch_state.current_gesture.gesture != TouchGesture::kNone ||
       g_touch_state.previous_gesture.gesture != TouchGesture::kNone)) {
    g_touch_state.gesture_callback(g_touch_state.current_gesture);
  }
}

bool TouchInput::IsTouchActive() {
  return g_touch_state.active_touch_count > 0;
}

bool TouchInput::IsTouchMode() {
  return g_touch_state.touch_mode;
}

GestureState TouchInput::GetCurrentGesture() {
  return g_touch_state.current_gesture;
}

TouchPoint TouchInput::GetTouchPoint(int index) {
  if (index >= 0 && index < kMaxTouchPoints) {
    return g_touch_state.touch_points[index];
  }
  return TouchPoint();
}

int TouchInput::GetActiveTouchCount() {
  return g_touch_state.active_touch_count;
}

void TouchInput::SetPanZoomEnabled(bool enabled) {
  g_touch_state.pan_zoom_enabled = enabled;
}

bool TouchInput::IsPanZoomEnabled() {
  return g_touch_state.pan_zoom_enabled;
}

ImVec2 TouchInput::GetPanOffset() {
  return g_touch_state.pan_offset;
}

float TouchInput::GetZoomLevel() {
  return g_touch_state.zoom_level;
}

float TouchInput::GetRotation() {
  return g_touch_state.rotation;
}

ImVec2 TouchInput::GetZoomCenter() {
  return g_touch_state.zoom_center;
}

void TouchInput::ApplyPanOffset(ImVec2 offset) {
  g_touch_state.pan_offset.x += offset.x;
  g_touch_state.pan_offset.y += offset.y;
}

void TouchInput::SetZoomLevel(float zoom) {
  auto& config = g_touch_state.config;
  g_touch_state.zoom_level = std::clamp(zoom, config.min_zoom, config.max_zoom);
}

void TouchInput::SetPanOffset(ImVec2 offset) {
  g_touch_state.pan_offset = offset;
}

void TouchInput::ResetCanvasState() {
  g_touch_state.pan_offset = ImVec2(0, 0);
  g_touch_state.zoom_level = 1.0f;
  g_touch_state.rotation = 0.0f;
  g_touch_state.inertia_velocity = ImVec2(0, 0);
  g_touch_state.inertia_active = false;
}

TouchConfig& TouchInput::GetConfig() {
  return g_touch_state.config;
}

void TouchInput::SetGestureCallback(
    std::function<void(const GestureState&)> callback) {
  g_touch_state.gesture_callback = callback;
}

// === Internal Implementation ===

void TouchInput::UpdateGestureRecognition() {
  auto& state = g_touch_state;
  auto& gesture = state.current_gesture;
  auto& config = state.config;
  double current_time = GetTime();

  CountActiveTouches();

  // Update touch count in gesture state
  gesture.touch_count = state.active_touch_count;

  if (state.active_touch_count == 0) {
    // No touches - check for completed gestures

    // Check if we had a tap
    if (state.potential_tap && !state.long_press_detected) {
      double tap_duration = current_time - state.touch_start_time;
      if (tap_duration <= config.tap_max_duration) {
        // Check for double tap
        double time_since_last_tap = current_time - state.last_tap_time;
        if (state.potential_double_tap &&
            time_since_last_tap <= config.double_tap_max_delay) {
          gesture.gesture = TouchGesture::kDoubleTap;
          gesture.phase = TouchPhase::kEnded;
          state.potential_double_tap = false;
        } else {
          gesture.gesture = TouchGesture::kTap;
          gesture.phase = TouchPhase::kEnded;
          state.last_tap_time = current_time;
          state.last_tap_position = gesture.position;
          state.potential_double_tap = true;
        }
      }
    }

    // Clear gesture if it was in progress
    if (gesture.gesture != TouchGesture::kTap &&
        gesture.gesture != TouchGesture::kDoubleTap &&
        gesture.gesture != TouchGesture::kLongPress) {
      // Start inertia for pan gestures
      if (gesture.gesture == TouchGesture::kPan && config.enable_inertia) {
        state.inertia_active = true;
        // Velocity was already being tracked during pan
      }

      if (gesture.gesture != TouchGesture::kNone) {
        gesture.phase = TouchPhase::kEnded;
      }
    }

    state.potential_tap = false;
    state.long_press_detected = false;

    // Only clear gesture after it's been reported as ended
    if (gesture.phase == TouchPhase::kEnded) {
      // Keep gesture info for one frame so consumers can see it ended
      // It will be cleared next frame
    } else {
      gesture.gesture = TouchGesture::kNone;
    }

    return;
  }

  // Clear potential double tap if too much time has passed
  if (state.potential_double_tap) {
    double time_since_last_tap = current_time - state.last_tap_time;
    if (time_since_last_tap > config.double_tap_max_delay) {
      state.potential_double_tap = false;
    }
  }

  // === Single Touch Processing ===
  if (state.active_touch_count == 1) {
    auto touches = GetFirstTwoTouches();
    TouchPoint* touch = touches[0];
    if (!touch) return;

    gesture.position = touch->position;
    gesture.start_position = touch->start_position;

    float movement = Distance(touch->position, touch->start_position);
    double duration = current_time - touch->timestamp;

    // Check for long press
    if (!state.long_press_detected && duration >= config.long_press_duration &&
        movement <= config.tap_max_movement) {
      state.long_press_detected = true;
      gesture.gesture = TouchGesture::kLongPress;
      gesture.phase = TouchPhase::kBegan;
      gesture.duration = duration;
      return;
    }

    // If already detected long press, keep it
    if (state.long_press_detected) {
      gesture.gesture = TouchGesture::kLongPress;
      gesture.phase = TouchPhase::kChanged;
      gesture.duration = duration;
      return;
    }

    // Check for potential tap (small movement, short duration still possible)
    if (movement <= config.tap_max_movement &&
        duration <= config.tap_max_duration) {
      state.potential_tap = true;
      // Don't set gesture yet - wait for release
      return;
    }

    // Too much movement - not a tap, this is a drag
    // Single finger drag acts as mouse drag (already handled by ImGui)
    state.potential_tap = false;
  }

  // === Two Touch Processing ===
  if (state.active_touch_count >= 2 && state.pan_zoom_enabled) {
    auto touches = GetFirstTwoTouches();
    TouchPoint* touch1 = touches[0];
    TouchPoint* touch2 = touches[1];
    if (!touch1 || !touch2) return;

    // Cancel any tap detection
    state.potential_tap = false;
    state.long_press_detected = false;

    // Calculate two-finger geometry
    ImVec2 center = Midpoint(touch1->position, touch2->position);
    float distance = Distance(touch1->position, touch2->position);
    float angle = Angle(touch1->position, touch2->position);

    // Initialize reference values on gesture start
    if (gesture.gesture != TouchGesture::kPan &&
        gesture.gesture != TouchGesture::kPinchZoom &&
        gesture.gesture != TouchGesture::kRotate) {
      state.initial_pinch_distance = distance;
      state.initial_rotation_angle = angle;
      state.initial_pan_center = center;
      gesture.scale = 1.0f;
      gesture.rotation = 0.0f;
    }

    // Calculate deltas
    float scale_ratio =
        (state.initial_pinch_distance > 0.0f)
            ? distance / state.initial_pinch_distance
            : 1.0f;

    float rotation_delta = angle - state.initial_rotation_angle;
    // Normalize rotation delta to [-PI, PI]
    while (rotation_delta > M_PI) rotation_delta -= 2.0f * M_PI;
    while (rotation_delta < -M_PI) rotation_delta += 2.0f * M_PI;

    ImVec2 pan_delta = ImVec2(center.x - state.initial_pan_center.x,
                              center.y - state.initial_pan_center.y);

    // Determine dominant gesture
    float scale_change = std::abs(scale_ratio - 1.0f);
    float pan_distance = Distance(center, state.initial_pan_center);
    float rotation_change = std::abs(rotation_delta);

    bool is_pinch = scale_change > config.pinch_threshold / 100.0f;
    bool is_pan = pan_distance > config.pan_threshold;
    bool is_rotate = config.enable_rotation &&
                     rotation_change > config.rotation_threshold;

    // Prioritize: pinch > rotate > pan
    if (is_pinch) {
      gesture.gesture = TouchGesture::kPinchZoom;
      gesture.phase = (gesture.gesture == TouchGesture::kPinchZoom)
                          ? TouchPhase::kChanged
                          : TouchPhase::kBegan;
      gesture.scale = scale_ratio;
      gesture.scale_delta = scale_ratio - gesture.scale;

      // Apply zoom
      float new_zoom = state.zoom_level * scale_ratio;
      new_zoom = std::clamp(new_zoom, config.min_zoom, config.max_zoom);

      // Calculate zoom delta to apply
      float zoom_delta = new_zoom / state.zoom_level;
      if (std::abs(zoom_delta - 1.0f) > 0.001f) {
        state.zoom_level = new_zoom;
        state.zoom_center = center;

        // Reset reference for continuous zooming
        state.initial_pinch_distance = distance;
      }
    } else if (is_rotate) {
      gesture.gesture = TouchGesture::kRotate;
      gesture.phase = (gesture.gesture == TouchGesture::kRotate)
                          ? TouchPhase::kChanged
                          : TouchPhase::kBegan;
      gesture.rotation_delta = rotation_delta;
      gesture.rotation += rotation_delta;
      state.rotation = gesture.rotation;
      state.initial_rotation_angle = angle;
    } else if (is_pan) {
      gesture.gesture = TouchGesture::kPan;
      gesture.phase = (gesture.gesture == TouchGesture::kPan)
                          ? TouchPhase::kChanged
                          : TouchPhase::kBegan;
      gesture.translation = pan_delta;

      // Apply pan offset
      state.pan_offset.x += pan_delta.x;
      state.pan_offset.y += pan_delta.y;

      // Track velocity for inertia
      ImVec2 prev_center = Midpoint(touch1->previous_position,
                                    touch2->previous_position);
      state.inertia_velocity.x = center.x - prev_center.x;
      state.inertia_velocity.y = center.y - prev_center.y;

      // Reset reference for continuous panning
      state.initial_pan_center = center;
    }

    gesture.position = center;
    gesture.start_position = state.initial_pan_center;
  }
}

void TouchInput::ProcessInertia() {
  auto& state = g_touch_state;
  auto& config = state.config;

  if (!config.enable_inertia || !state.inertia_active) {
    return;
  }

  float velocity_magnitude = Distance(ImVec2(0, 0), state.inertia_velocity);
  if (velocity_magnitude < config.inertia_min_velocity) {
    state.inertia_active = false;
    state.inertia_velocity = ImVec2(0, 0);
    return;
  }

  // Apply inertia to pan offset
  state.pan_offset.x += state.inertia_velocity.x;
  state.pan_offset.y += state.inertia_velocity.y;

  // Decay velocity
  state.inertia_velocity.x *= config.inertia_deceleration;
  state.inertia_velocity.y *= config.inertia_deceleration;
}

void TouchInput::TranslateToImGuiEvents() {
  auto& state = g_touch_state;

  if (!state.touch_mode && state.active_touch_count == 0) {
    return;
  }

  ImGuiIO& io = ImGui::GetIO();

  // Always indicate touch source when in touch mode
  if (state.active_touch_count > 0) {
    io.AddMouseSourceEvent(ImGuiMouseSource_TouchScreen);
    state.touch_mode = true;
  }

  // Handle single touch as mouse
  if (state.active_touch_count == 1) {
    auto touches = GetFirstTwoTouches();
    TouchPoint* touch = touches[0];
    if (touch) {
      io.AddMousePosEvent(touch->position.x, touch->position.y);
      io.AddMouseButtonEvent(0, true);  // Left button down
    }
  } else if (state.active_touch_count == 0 &&
             state.previous_gesture.touch_count > 0) {
    // Touch ended - release mouse button
    io.AddMouseButtonEvent(0, false);

    // Handle tap as click
    if (state.current_gesture.gesture == TouchGesture::kTap ||
        state.current_gesture.gesture == TouchGesture::kDoubleTap) {
      // Position should already be set from the touch
    }

    // Handle long press as right click
    if (state.previous_gesture.gesture == TouchGesture::kLongPress) {
      io.AddMouseButtonEvent(1, true);   // Right click down
      io.AddMouseButtonEvent(1, false);  // Right click up
    }
  }

  // For two-finger gestures, don't send mouse events
  // The pan/zoom is handled separately via GetPanOffset()/GetZoomLevel()
}

double TouchInput::GetCurrentTime() {
  return GetTime();
}

// === Platform-Specific Implementation ===

#ifdef __EMSCRIPTEN__

// clang-format off
EM_JS(void, yaze_setup_touch_handlers, (), {
  // Check if touch gestures script is loaded
  if (typeof window.YazeTouchGestures !== 'undefined') {
    window.YazeTouchGestures.initialize();
    console.log('Touch gesture handlers initialized via YazeTouchGestures');
  } else {
    console.log('YazeTouchGestures not loaded - using basic touch handling');

    // Basic fallback touch handling
    const canvas = document.getElementById('canvas');
    if (!canvas) {
      console.error('Canvas element not found for touch handling');
      return;
    }

    // Prevent default touch behaviors
    canvas.style.touchAction = 'none';

    function handleTouch(event, type) {
      event.preventDefault();

      const rect = canvas.getBoundingClientRect();
      const scaleX = canvas.width / rect.width;
      const scaleY = canvas.height / rect.height;

      for (let i = 0; i < event.changedTouches.length; i++) {
        const touch = event.changedTouches[i];
        const x = (touch.clientX - rect.left) * scaleX;
        const y = (touch.clientY - rect.top) * scaleY;
        const pressure = touch.force || 1.0;
        const timestamp = event.timeStamp / 1000.0;

        // Call C++ function
        if (typeof Module._OnTouchEvent === 'function') {
          Module._OnTouchEvent(type, touch.identifier, x, y, pressure, timestamp);
        } else if (typeof Module.ccall === 'function') {
          Module.ccall('OnTouchEvent', null,
            ['number', 'number', 'number', 'number', 'number', 'number'],
            [type, touch.identifier, x, y, pressure, timestamp]);
        }
      }
    }

    canvas.addEventListener('touchstart', (e) => handleTouch(e, 0), { passive: false });
    canvas.addEventListener('touchmove', (e) => handleTouch(e, 1), { passive: false });
    canvas.addEventListener('touchend', (e) => handleTouch(e, 2), { passive: false });
    canvas.addEventListener('touchcancel', (e) => handleTouch(e, 3), { passive: false });
  }
});

EM_JS(void, yaze_cleanup_touch_handlers, (), {
  if (typeof window.YazeTouchGestures !== 'undefined') {
    window.YazeTouchGestures.shutdown();
  }
});
// clang-format on

void TouchInput::InitializePlatform() {
  yaze_setup_touch_handlers();
}

void TouchInput::ShutdownPlatform() {
  yaze_cleanup_touch_handlers();
}

// Exported functions for JavaScript callbacks
extern "C" {

EMSCRIPTEN_KEEPALIVE
void OnTouchEvent(int type, int id, float x, float y, float pressure,
                  double timestamp) {
  TouchInput::OnTouchEvent(type, id, x, y, pressure, timestamp);
}

EMSCRIPTEN_KEEPALIVE
void OnGestureEvent(int type, int phase, float x, float y, float scale,
                    float rotation) {
  TouchInput::OnGestureEvent(type, phase, x, y, scale, rotation);
}

}  // extern "C"

void TouchInput::OnTouchEvent(int type, int id, float x, float y,
                              float pressure, double timestamp) {
  auto& state = g_touch_state;

  switch (type) {
    case 0: {  // Touch start
      TouchPoint* touch = FindInactiveSlot();
      if (touch) {
        touch->id = id;
        touch->position = ImVec2(x, y);
        touch->start_position = ImVec2(x, y);
        touch->previous_position = ImVec2(x, y);
        touch->pressure = pressure;
        touch->timestamp = timestamp;
        touch->active = true;

        // Track touch start time for tap detection
        if (state.active_touch_count == 0) {
          state.touch_start_time = timestamp;
        }
      }
      break;
    }
    case 1: {  // Touch move
      TouchPoint* touch = FindTouchById(id);
      if (touch) {
        touch->previous_position = touch->position;
        touch->position = ImVec2(x, y);
        touch->pressure = pressure;
      }
      break;
    }
    case 2:    // Touch end
    case 3: {  // Touch cancel
      TouchPoint* touch = FindTouchById(id);
      if (touch) {
        touch->active = false;
      }
      break;
    }
  }

  CountActiveTouches();
  state.touch_mode = true;
}

void TouchInput::OnGestureEvent(int type, int phase, float x, float y,
                                float scale, float rotation) {
  auto& state = g_touch_state;
  auto& gesture = state.current_gesture;

  gesture.gesture = static_cast<TouchGesture>(type);
  gesture.phase = static_cast<TouchPhase>(phase);
  gesture.position = ImVec2(x, y);
  gesture.scale = scale;
  gesture.rotation = rotation;

  // Apply gesture effects
  if (gesture.gesture == TouchGesture::kPinchZoom) {
    auto& config = state.config;
    float new_zoom = state.zoom_level * scale;
    state.zoom_level = std::clamp(new_zoom, config.min_zoom, config.max_zoom);
    state.zoom_center = ImVec2(x, y);
  } else if (gesture.gesture == TouchGesture::kRotate) {
    state.rotation = rotation;
  }
}

#else  // Non-Emscripten (desktop) builds

void TouchInput::InitializePlatform() {
  // On desktop, touch events come through SDL which is already handled
  // by the ImGui SDL backend. We may need to register for SDL_FINGERDOWN etc.
  // events if we want more control.
  LOG_INFO("TouchInput", "Touch input: Desktop mode (SDL touch passthrough)");
}

void TouchInput::ShutdownPlatform() {
  // Nothing to clean up on desktop
}

#endif  // __EMSCRIPTEN__

}  // namespace gui
}  // namespace yaze
