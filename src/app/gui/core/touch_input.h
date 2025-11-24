#ifndef YAZE_APP_GUI_CORE_TOUCH_INPUT_H
#define YAZE_APP_GUI_CORE_TOUCH_INPUT_H

#include <array>
#include <functional>
#include <memory>

#include "imgui/imgui.h"

namespace yaze {
namespace gui {

/**
 * @brief Gesture types recognized by the touch input system
 */
enum class TouchGesture {
  kNone,         // No gesture active
  kTap,          // Single finger tap (click)
  kDoubleTap,    // Double tap (context menu or zoom to fit)
  kLongPress,    // Long press (500ms+) for context menu
  kPan,          // Two-finger pan/scroll
  kPinchZoom,    // Two-finger pinch to zoom
  kRotate        // Two-finger rotation (optional)
};

/**
 * @brief Phase of a touch gesture
 */
enum class TouchPhase {
  kBegan,     // Gesture started
  kChanged,   // Gesture in progress with updated values
  kEnded,     // Gesture completed successfully
  kCancelled  // Gesture was cancelled
};

/**
 * @brief Individual touch point data
 */
struct TouchPoint {
  int id = -1;                      // Unique identifier for this touch
  ImVec2 position = ImVec2(0, 0);   // Current position
  ImVec2 start_position = ImVec2(0, 0);  // Position when touch started
  ImVec2 previous_position = ImVec2(0, 0);  // Previous frame position
  float pressure = 1.0f;            // Touch pressure (0.0 - 1.0)
  double timestamp = 0.0;           // Time when touch started
  bool active = false;              // Whether this touch is currently active
};

/**
 * @brief Gesture recognition result
 */
struct GestureState {
  TouchGesture gesture = TouchGesture::kNone;
  TouchPhase phase = TouchPhase::kBegan;

  // Position data
  ImVec2 position = ImVec2(0, 0);       // Center position of gesture
  ImVec2 start_position = ImVec2(0, 0); // Where gesture started
  ImVec2 translation = ImVec2(0, 0);    // Pan/drag offset
  ImVec2 velocity = ImVec2(0, 0);       // Movement velocity for inertia

  // Scale/rotation data
  float scale = 1.0f;           // Pinch zoom scale factor
  float scale_delta = 0.0f;     // Change in scale this frame
  float rotation = 0.0f;        // Rotation angle in radians
  float rotation_delta = 0.0f;  // Change in rotation this frame

  // Touch count
  int touch_count = 0;

  // Timing
  double duration = 0.0;  // How long the gesture has been active
};

/**
 * @brief Touch input configuration
 */
struct TouchConfig {
  // Timing thresholds (in seconds)
  float tap_max_duration = 0.3f;      // Max duration for a tap
  float double_tap_max_delay = 0.3f;  // Max delay between taps for double-tap
  float long_press_duration = 0.5f;   // Duration to trigger long press

  // Distance thresholds (in pixels)
  float tap_max_movement = 10.0f;     // Max movement for a tap
  float pan_threshold = 10.0f;        // Min movement to start pan
  float pinch_threshold = 5.0f;       // Min scale change to start pinch
  float rotation_threshold = 0.1f;    // Min rotation to start rotate (radians)

  // Feature toggles
  bool enable_pan_zoom = true;     // Enable two-finger pan/zoom
  bool enable_rotation = false;    // Enable two-finger rotation
  bool enable_inertia = true;      // Enable inertia scrolling after swipe

  // Inertia settings
  float inertia_deceleration = 0.95f;  // Velocity multiplier per frame
  float inertia_min_velocity = 0.5f;   // Minimum velocity to continue inertia

  // Scale limits
  float min_zoom = 0.25f;
  float max_zoom = 4.0f;
};

/**
 * @brief Touch input handling system for iPad and tablet browsers
 *
 * Provides gesture recognition and touch event handling for the yaze editor.
 * Integrates with ImGui's input system and the Canvas pan/zoom functionality.
 *
 * Features:
 * - Single tap: Translates to left mouse click
 * - Double tap: Context menu or zoom-to-fit
 * - Long press: Context menu activation
 * - Two-finger pan: Canvas scrolling
 * - Pinch to zoom: Canvas zoom in/out
 * - Two-finger rotate: Optional sprite rotation
 *
 * Usage:
 * @code
 * // Initialize once at startup
 * TouchInput::Initialize();
 *
 * // Call each frame before ImGui::NewFrame()
 * TouchInput::Update();
 *
 * // Check touch state in your code
 * if (TouchInput::IsTouchActive()) {
 *   auto gesture = TouchInput::GetCurrentGesture();
 *   // Handle gesture...
 * }
 *
 * // Get canvas transform values
 * ImVec2 pan = TouchInput::GetPanOffset();
 * float zoom = TouchInput::GetZoomLevel();
 * @endcode
 */
class TouchInput {
 public:
  /**
   * @brief Maximum number of simultaneous touch points supported
   */
  static constexpr int kMaxTouchPoints = 10;

  /**
   * @brief Initialize the touch input system
   *
   * Sets up touch event handlers. On web builds, this registers JavaScript
   * callbacks for touch events. On native builds, it configures SDL touch
   * support.
   */
  static void Initialize();

  /**
   * @brief Shutdown and cleanup touch input system
   */
  static void Shutdown();

  /**
   * @brief Process touch events for the current frame
   *
   * Call this once per frame, before ImGui::NewFrame(). Updates gesture
   * recognition state and translates touch events to ImGui mouse events.
   */
  static void Update();

  /**
   * @brief Check if touch input is currently being used
   * @return true if any touch points are active
   */
  static bool IsTouchActive();

  /**
   * @brief Check if we're in touch mode (vs mouse mode)
   *
   * Returns true if the last input was from touch, even if no touches
   * are currently active. Used for UI adaptation (larger hit targets, etc.)
   * @return true if touch mode is active
   */
  static bool IsTouchMode();

  /**
   * @brief Get the current gesture state
   * @return Current gesture recognition result
   */
  static GestureState GetCurrentGesture();

  /**
   * @brief Get raw touch point data
   * @param index Touch point index (0 to kMaxTouchPoints-1)
   * @return Touch point data, or inactive touch if index invalid
   */
  static TouchPoint GetTouchPoint(int index);

  /**
   * @brief Get number of active touch points
   * @return Number of currently active touches
   */
  static int GetActiveTouchCount();

  // === Canvas Integration ===

  /**
   * @brief Enable or disable pan/zoom gestures for canvas
   * @param enabled Whether to process pan/zoom gestures
   */
  static void SetPanZoomEnabled(bool enabled);

  /**
   * @brief Check if pan/zoom is enabled
   * @return true if pan/zoom gestures are being processed
   */
  static bool IsPanZoomEnabled();

  /**
   * @brief Get cumulative pan offset from touch gestures
   *
   * This value accumulates pan translations. Reset with ResetCanvasState().
   * @return Pan offset in pixels
   */
  static ImVec2 GetPanOffset();

  /**
   * @brief Get cumulative zoom level from pinch gestures
   *
   * Starts at 1.0. Multiply your canvas scale by this value.
   * @return Zoom level (1.0 = no zoom, >1.0 = zoomed in, <1.0 = zoomed out)
   */
  static float GetZoomLevel();

  /**
   * @brief Get rotation angle from two-finger rotation
   * @return Rotation in radians
   */
  static float GetRotation();

  /**
   * @brief Get the zoom center point in screen coordinates
   *
   * Use this as the pivot point when applying zoom transformations.
   * @return Center point of the pinch gesture
   */
  static ImVec2 GetZoomCenter();

  /**
   * @brief Apply pan offset to the current value
   * @param offset Pan offset to apply
   */
  static void ApplyPanOffset(ImVec2 offset);

  /**
   * @brief Set the zoom level directly
   * @param zoom New zoom level
   */
  static void SetZoomLevel(float zoom);

  /**
   * @brief Set the pan offset directly
   * @param offset New pan offset
   */
  static void SetPanOffset(ImVec2 offset);

  /**
   * @brief Reset canvas transform state
   *
   * Resets pan offset to (0,0), zoom to 1.0, rotation to 0.0
   */
  static void ResetCanvasState();

  // === Configuration ===

  /**
   * @brief Get the current touch configuration
   * @return Reference to configuration struct
   */
  static TouchConfig& GetConfig();

  /**
   * @brief Set gesture callback
   *
   * Optional callback invoked when gestures are recognized.
   * @param callback Function to call with gesture state
   */
  static void SetGestureCallback(std::function<void(const GestureState&)> callback);

  // === Platform-Specific Hooks ===

#ifdef __EMSCRIPTEN__
  /**
   * @brief Process touch event from JavaScript
   *
   * Called from JavaScript touch event handlers.
   * @param type Event type (0=start, 1=move, 2=end, 3=cancel)
   * @param id Touch identifier
   * @param x X position
   * @param y Y position
   * @param pressure Touch pressure
   * @param timestamp Event timestamp
   */
  static void OnTouchEvent(int type, int id, float x, float y,
                           float pressure, double timestamp);

  /**
   * @brief Process gesture event from JavaScript
   *
   * For high-level gestures computed in JavaScript.
   * @param type Gesture type
   * @param phase Gesture phase
   * @param x Center X
   * @param y Center Y
   * @param scale Pinch scale
   * @param rotation Rotation angle
   */
  static void OnGestureEvent(int type, int phase, float x, float y,
                             float scale, float rotation);
#endif

 private:
  TouchInput() = delete;  // Static-only class

  // Internal state management
  static void UpdateGestureRecognition();
  static void ProcessInertia();
  static void TranslateToImGuiEvents();
  static double GetCurrentTime();

  // Platform-specific initialization
  static void InitializePlatform();
  static void ShutdownPlatform();
};

}  // namespace gui
}  // namespace yaze

#endif  // YAZE_APP_GUI_CORE_TOUCH_INPUT_H
