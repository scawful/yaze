#include "app/gui/canvas/canvas_touch_handler.h"

#include <algorithm>
#include <cmath>

namespace yaze {
namespace gui {

void CanvasTouchHandler::Initialize(const std::string& canvas_id) {
  canvas_id_ = canvas_id;
  Reset();

  // Initialize the global touch input system if not already done
  TouchInput::Initialize();
}

void CanvasTouchHandler::Update() {
  // Clear per-frame gesture flags
  was_tapped_ = false;
  was_double_tapped_ = false;
  was_long_pressed_ = false;

  // Store previous state for delta calculations
  prev_pan_offset_ = scroll_offset_;
  prev_scale_ = current_scale_;

  // Get current gesture state from TouchInput
  auto gesture = TouchInput::GetCurrentGesture();

  // Process the gesture
  if (gesture.gesture != TouchGesture::kNone && canvas_hovered_) {
    ProcessGesture(gesture);
  }

  // Process inertia for smooth scrolling
  if (inertia_active_ && !TouchInput::IsTouchActive()) {
    ProcessInertia();
  }

  // Smooth zoom animation
  if (config_.enable_smooth_zoom && std::abs(target_scale_ - current_scale_) > 0.001f) {
    current_scale_ += (target_scale_ - current_scale_) * config_.zoom_smoothing;
  } else {
    current_scale_ = target_scale_;
  }

  // Clamp pan to bounds if configured
  if (config_.max_pan_x != 0.0f || config_.max_pan_y != 0.0f) {
    ClampPanToBounds();
  }
}

bool CanvasTouchHandler::IsTouchActive() const {
  return TouchInput::IsTouchActive();
}

bool CanvasTouchHandler::IsTouchMode() const {
  return TouchInput::IsTouchMode();
}

void CanvasTouchHandler::SetScrollOffset(ImVec2 offset) {
  scroll_offset_ = offset;
  prev_pan_offset_ = offset;
}

void CanvasTouchHandler::SetScale(float scale) {
  current_scale_ = std::clamp(scale, config_.min_scale, config_.max_scale);
  target_scale_ = current_scale_;
  prev_scale_ = current_scale_;
}

void CanvasTouchHandler::ApplyScrollDelta(ImVec2 delta) {
  scroll_offset_.x += delta.x;
  scroll_offset_.y += delta.y;
}

void CanvasTouchHandler::ApplyZoomDelta(float delta, ImVec2 center) {
  float new_scale = target_scale_ * (1.0f + delta);
  new_scale = std::clamp(new_scale, config_.min_scale, config_.max_scale);

  // Zoom towards center point
  if (std::abs(new_scale - target_scale_) > 0.001f) {
    float scale_ratio = new_scale / target_scale_;

    // Adjust pan to zoom towards the center point
    scroll_offset_.x = center.x - (center.x - scroll_offset_.x) * scale_ratio;
    scroll_offset_.y = center.y - (center.y - scroll_offset_.y) * scale_ratio;

    target_scale_ = new_scale;
    zoom_center_ = center;
  }
}

void CanvasTouchHandler::Reset() {
  scroll_offset_ = ImVec2(0, 0);
  current_scale_ = 1.0f;
  target_scale_ = 1.0f;
  zoom_center_ = ImVec2(0, 0);
  inertia_velocity_ = ImVec2(0, 0);
  inertia_active_ = false;
  prev_pan_offset_ = ImVec2(0, 0);
  prev_scale_ = 1.0f;

  // Also reset the global touch state
  TouchInput::ResetCanvasState();
}

void CanvasTouchHandler::ProcessForCanvas(ImVec2 canvas_p0, ImVec2 canvas_sz,
                                          bool is_hovered) {
  canvas_p0_ = canvas_p0;
  canvas_sz_ = canvas_sz;
  canvas_hovered_ = is_hovered;
}

void CanvasTouchHandler::ProcessGesture(const GestureState& gesture) {
  gesture_position_ = gesture.position;

  switch (gesture.gesture) {
    case TouchGesture::kTap:
      if (gesture.phase == TouchPhase::kEnded) {
        was_tapped_ = true;
      }
      break;

    case TouchGesture::kDoubleTap:
      if (gesture.phase == TouchPhase::kEnded) {
        was_double_tapped_ = true;

        // Optional: Zoom to fit on double-tap
        // This could be made configurable
      }
      break;

    case TouchGesture::kLongPress:
      if (gesture.phase == TouchPhase::kBegan) {
        was_long_pressed_ = true;
      }
      break;

    case TouchGesture::kPan:
      if (config_.enable_pan) {
        // Get pan offset from TouchInput
        ImVec2 global_pan = TouchInput::GetPanOffset();

        // Calculate delta since last frame
        ImVec2 pan_delta = ImVec2(global_pan.x - prev_pan_offset_.x,
                                  global_pan.y - prev_pan_offset_.y);

        // Apply to local scroll offset
        scroll_offset_.x += pan_delta.x;
        scroll_offset_.y += pan_delta.y;

        // Track velocity for inertia
        if (gesture.phase == TouchPhase::kChanged) {
          inertia_velocity_ = gesture.velocity;
        }

        // Start inertia when pan ends
        if (gesture.phase == TouchPhase::kEnded && config_.enable_inertia) {
          float velocity_mag = std::sqrt(
              inertia_velocity_.x * inertia_velocity_.x +
              inertia_velocity_.y * inertia_velocity_.y);
          if (velocity_mag > config_.inertia_min_velocity) {
            inertia_active_ = true;
          }
        }
      }
      break;

    case TouchGesture::kPinchZoom:
      if (config_.enable_zoom) {
        // Get zoom level from TouchInput
        float global_zoom = TouchInput::GetZoomLevel();
        zoom_center_ = TouchInput::GetZoomCenter();

        // Calculate zoom delta
        float zoom_delta = global_zoom - prev_scale_;

        if (std::abs(zoom_delta) > 0.001f) {
          // Apply zoom with pivot at gesture center
          float new_scale = current_scale_ + zoom_delta;
          new_scale = std::clamp(new_scale, config_.min_scale, config_.max_scale);

          // Calculate offset adjustment to zoom towards the pinch center
          // Convert gesture center from screen to canvas space
          ImVec2 center_in_canvas = ImVec2(
              zoom_center_.x - canvas_p0_.x - scroll_offset_.x,
              zoom_center_.y - canvas_p0_.y - scroll_offset_.y);

          float scale_ratio = new_scale / current_scale_;

          // Adjust scroll to keep the pinch center stationary
          scroll_offset_.x = zoom_center_.x - canvas_p0_.x -
                             center_in_canvas.x * scale_ratio;
          scroll_offset_.y = zoom_center_.y - canvas_p0_.y -
                             center_in_canvas.y * scale_ratio;

          target_scale_ = new_scale;
          current_scale_ = new_scale;  // Immediate update during pinch
        }
      }
      break;

    case TouchGesture::kRotate:
      // Rotation is optional and handled separately by canvas if needed
      break;

    case TouchGesture::kNone:
    default:
      break;
  }

  // Update previous values
  prev_pan_offset_ = TouchInput::GetPanOffset();
  prev_scale_ = TouchInput::GetZoomLevel();
}

void CanvasTouchHandler::ProcessInertia() {
  float velocity_mag = std::sqrt(
      inertia_velocity_.x * inertia_velocity_.x +
      inertia_velocity_.y * inertia_velocity_.y);

  if (velocity_mag < config_.inertia_min_velocity) {
    inertia_active_ = false;
    inertia_velocity_ = ImVec2(0, 0);
    return;
  }

  // Apply velocity to scroll
  scroll_offset_.x += inertia_velocity_.x;
  scroll_offset_.y += inertia_velocity_.y;

  // Decay velocity
  inertia_velocity_.x *= config_.inertia_deceleration;
  inertia_velocity_.y *= config_.inertia_deceleration;
}

void CanvasTouchHandler::ClampPanToBounds() {
  // Apply pan limits if configured
  if (config_.min_pan_x != 0.0f || config_.max_pan_x != 0.0f) {
    scroll_offset_.x = std::clamp(scroll_offset_.x,
                                  config_.min_pan_x, config_.max_pan_x);
  }

  if (config_.min_pan_y != 0.0f || config_.max_pan_y != 0.0f) {
    scroll_offset_.y = std::clamp(scroll_offset_.y,
                                  config_.min_pan_y, config_.max_pan_y);
  }
}

}  // namespace gui
}  // namespace yaze
