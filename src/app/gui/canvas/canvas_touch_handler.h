#ifndef YAZE_APP_GUI_CANVAS_CANVAS_TOUCH_HANDLER_H
#define YAZE_APP_GUI_CANVAS_CANVAS_TOUCH_HANDLER_H

#include "app/gui/core/touch_input.h"
#include "imgui/imgui.h"

namespace yaze {
namespace gui {

/**
 * @brief Handles touch input integration for Canvas
 *
 * Bridges the TouchInput system with Canvas pan/zoom/interaction.
 * Translates touch gestures into canvas operations and maintains
 * smooth integration with mouse-based controls.
 *
 * Usage:
 * @code
 * // In canvas initialization or first frame
 * canvas_touch_handler_.Initialize();
 *
 * // Each frame before canvas rendering
 * canvas_touch_handler_.Update();
 *
 * // Apply touch transforms to canvas
 * if (canvas_touch_handler_.IsTouchActive()) {
 *   scrolling = canvas_touch_handler_.GetScrollOffset();
 *   scale = canvas_touch_handler_.GetScale();
 * }
 * @endcode
 */
class CanvasTouchHandler {
 public:
  /**
   * @brief Configuration for canvas touch behavior
   */
  struct Config {
    // Scale limits
    float min_scale = 0.25f;
    float max_scale = 4.0f;

    // Whether touch pan/zoom is enabled
    bool enable_pan = true;
    bool enable_zoom = true;

    // Smooth animations
    bool enable_smooth_zoom = true;
    float zoom_smoothing = 0.2f;

    // Inertia settings
    bool enable_inertia = true;
    float inertia_deceleration = 0.92f;
    float inertia_min_velocity = 0.5f;

    // Canvas boundaries (0 = no limits)
    float max_pan_x = 0.0f;
    float max_pan_y = 0.0f;
    float min_pan_x = 0.0f;
    float min_pan_y = 0.0f;
  };

  CanvasTouchHandler() = default;

  /**
   * @brief Initialize the touch handler
   * @param canvas_id Canvas identifier for unique state tracking
   */
  void Initialize(const std::string& canvas_id = "");

  /**
   * @brief Update touch state each frame
   *
   * Call this once per frame before accessing touch state.
   * Processes touch events and updates pan/zoom values.
   */
  void Update();

  /**
   * @brief Check if touch input is currently active
   */
  bool IsTouchActive() const;

  /**
   * @brief Check if we're in touch mode (vs mouse)
   */
  bool IsTouchMode() const;

  /**
   * @brief Get the current scroll/pan offset for the canvas
   */
  ImVec2 GetScrollOffset() const { return scroll_offset_; }

  /**
   * @brief Get the current zoom scale
   */
  float GetScale() const { return current_scale_; }

  /**
   * @brief Get the zoom center point (for pivot-based zooming)
   */
  ImVec2 GetZoomCenter() const { return zoom_center_; }

  /**
   * @brief Set the scroll offset (e.g., from Canvas state)
   */
  void SetScrollOffset(ImVec2 offset);

  /**
   * @brief Set the zoom scale
   */
  void SetScale(float scale);

  /**
   * @brief Apply a scroll delta (add to current offset)
   */
  void ApplyScrollDelta(ImVec2 delta);

  /**
   * @brief Apply a zoom delta around a center point
   */
  void ApplyZoomDelta(float delta, ImVec2 center);

  /**
   * @brief Reset to default state (no pan, 1.0 scale)
   */
  void Reset();

  /**
   * @brief Get configuration reference for modification
   */
  Config& GetConfig() { return config_; }
  const Config& GetConfig() const { return config_; }

  /**
   * @brief Check if a tap gesture occurred this frame
   */
  bool WasTapped() const { return was_tapped_; }

  /**
   * @brief Check if a double-tap gesture occurred this frame
   */
  bool WasDoubleTapped() const { return was_double_tapped_; }

  /**
   * @brief Check if a long-press gesture occurred this frame
   */
  bool WasLongPressed() const { return was_long_pressed_; }

  /**
   * @brief Get the position of the last tap/gesture
   */
  ImVec2 GetGesturePosition() const { return gesture_position_; }

  /**
   * @brief Process touch gestures for the current canvas bounds
   *
   * @param canvas_p0 Canvas top-left position in screen coordinates
   * @param canvas_sz Canvas size
   * @param is_hovered Whether mouse/touch is over the canvas
   */
  void ProcessForCanvas(ImVec2 canvas_p0, ImVec2 canvas_sz, bool is_hovered);

 private:
  void ProcessGesture(const GestureState& gesture);
  void ProcessInertia();
  void ClampPanToBounds();

  std::string canvas_id_;
  Config config_;

  // Current canvas transform state
  ImVec2 scroll_offset_ = ImVec2(0, 0);
  float current_scale_ = 1.0f;
  float target_scale_ = 1.0f;
  ImVec2 zoom_center_ = ImVec2(0, 0);

  // Inertia state
  ImVec2 inertia_velocity_ = ImVec2(0, 0);
  bool inertia_active_ = false;

  // Gesture state for this frame
  bool was_tapped_ = false;
  bool was_double_tapped_ = false;
  bool was_long_pressed_ = false;
  ImVec2 gesture_position_ = ImVec2(0, 0);

  // Canvas bounds for gesture processing
  ImVec2 canvas_p0_ = ImVec2(0, 0);
  ImVec2 canvas_sz_ = ImVec2(0, 0);
  bool canvas_hovered_ = false;

  // Previous frame state for delta calculations
  ImVec2 prev_pan_offset_ = ImVec2(0, 0);
  float prev_scale_ = 1.0f;
};

}  // namespace gui
}  // namespace yaze

#endif  // YAZE_APP_GUI_CANVAS_CANVAS_TOUCH_HANDLER_H
