#include "app/gui/animation/animator.h"

#include <algorithm>
#include <cmath>

#include "app/gui/core/theme_manager.h"
#include "imgui/imgui.h"

namespace yaze {
namespace gui {

float Animator::Lerp(float a, float b, float t) {
  return a + (b - a) * t;
}

float Animator::EaseOutCubic(float t) {
  float inv = 1.0f - t;
  return 1.0f - (inv * inv * inv);
}

float Animator::EaseInOutCubic(float t) {
  return t < 0.5f ? 4.0f * t * t * t
                  : 1.0f - std::pow(-2.0f * t + 2.0f, 3.0f) / 2.0f;
}

float Animator::EaseOutElastic(float t) {
  const float c4 = (2.0f * 3.14159265f) / 3.0f;
  if (t <= 0.0f) return 0.0f;
  if (t >= 1.0f) return 1.0f;
  return std::pow(2.0f, -10.0f * t) * std::sin((t * 10.0f - 0.75f) * c4) + 1.0f;
}

float Animator::EaseOutBack(float t) {
  const float c1 = 1.70158f;
  const float c3 = c1 + 1.0f;
  float tm1 = t - 1.0f;
  return 1.0f + c3 * tm1 * tm1 * tm1 + c1 * tm1 * tm1;
}

ImVec4 Animator::LerpColor(ImVec4 a, ImVec4 b, float t) {
  return ImVec4(Lerp(a.x, b.x, t), Lerp(a.y, b.y, t), Lerp(a.z, b.z, t),
                Lerp(a.w, b.w, t));
}

float Animator::Animate(const std::string& panel_id,
                        const std::string& anim_id, float target,
                        float speed) {
  auto& state = GetState(panel_id, anim_id);
  if (!state.has_value) {
    state.value = target;
    state.has_value = true;
  }

  if (!IsEnabled()) {
    state.value = target;
    return target;
  }

  float t = ComputeStep(speed);
  state.value = Lerp(state.value, target, t);
  return state.value;
}

ImVec4 Animator::AnimateColor(const std::string& panel_id,
                              const std::string& anim_id, ImVec4 target,
                              float speed) {
  auto& state = GetState(panel_id, anim_id);
  if (!state.has_color) {
    state.color = target;
    state.has_color = true;
  }

  if (!IsEnabled()) {
    state.color = target;
    return target;
  }

  float t = ComputeStep(speed);
  state.color = LerpColor(state.color, target, t);
  return state.color;
}

void Animator::ClearAnimationsForPanel(const std::string& panel_id) {
  panels_.erase(panel_id);
  panel_transitions_.erase(panel_id);
}

void Animator::BeginPanelTransition(const std::string& panel_id,
                                    TransitionType type) {
  if (!IsEnabled() || type == TransitionType::kNone) {
    return;
  }

  auto& state = panel_transitions_[panel_id];
  state.type = type;
  state.progress = 0.0f;
  state.active = true;

  // Set initial offsets based on transition type
  switch (type) {
    case TransitionType::kSlideLeft:
      state.initial_offset_x = -50.0f;
      state.initial_offset_y = 0.0f;
      break;
    case TransitionType::kSlideRight:
      state.initial_offset_x = 50.0f;
      state.initial_offset_y = 0.0f;
      break;
    case TransitionType::kSlideUp:
      state.initial_offset_x = 0.0f;
      state.initial_offset_y = 50.0f;
      break;
    case TransitionType::kSlideDown:
      state.initial_offset_x = 0.0f;
      state.initial_offset_y = -50.0f;
      break;
    default:
      state.initial_offset_x = 0.0f;
      state.initial_offset_y = 0.0f;
      break;
  }
}

Animator::PanelTransition Animator::UpdatePanelTransition(
    const std::string& panel_id, float speed) {
  PanelTransition result;

  if (!IsEnabled()) {
    return result;  // Return default (fully visible, no offset)
  }

  auto iter = panel_transitions_.find(panel_id);
  if (iter == panel_transitions_.end() || !iter->second.active) {
    return result;  // No active transition
  }

  auto& state = iter->second;

  // Update progress
  float step = ComputeStep(speed);
  state.progress = std::min(state.progress + step, 1.0f);

  // Apply easing
  float eased = EaseOutCubic(state.progress);

  // Calculate current values based on transition type
  switch (state.type) {
    case TransitionType::kFade:
      result.alpha = eased;
      break;

    case TransitionType::kSlideLeft:
    case TransitionType::kSlideRight:
    case TransitionType::kSlideUp:
    case TransitionType::kSlideDown:
      result.alpha = eased;
      result.offset_x = state.initial_offset_x * (1.0f - eased);
      result.offset_y = state.initial_offset_y * (1.0f - eased);
      break;

    case TransitionType::kScale:
      result.alpha = eased;
      result.scale = 0.8f + (0.2f * eased);  // Scale from 0.8 to 1.0
      break;

    case TransitionType::kExpand:
      result.alpha = eased;
      result.scale = eased;  // Scale from 0 to 1
      break;

    case TransitionType::kNone:
    default:
      break;
  }

  // Mark as complete when done
  if (state.progress >= 1.0f) {
    state.active = false;
    result.is_complete = true;
  } else {
    result.is_complete = false;
  }

  return result;
}

bool Animator::IsPanelTransitioning(const std::string& panel_id) const {
  auto iter = panel_transitions_.find(panel_id);
  return iter != panel_transitions_.end() && iter->second.active;
}

void Animator::ApplyPanelTransitionPre(const std::string& panel_id) {
  auto transition = UpdatePanelTransition(panel_id);

  if (transition.is_complete) {
    return;
  }

  // Set window position offset for slide transitions
  if (transition.offset_x != 0.0f || transition.offset_y != 0.0f) {
    ImVec2 window_pos = ImGui::GetCursorScreenPos();
    ImGui::SetNextWindowPos(
        ImVec2(window_pos.x + transition.offset_x,
               window_pos.y + transition.offset_y),
        ImGuiCond_Always);
  }

  // Apply alpha
  ImGui::PushStyleVar(ImGuiStyleVar_Alpha, transition.alpha);
}

void Animator::ApplyPanelTransitionPost(const std::string& panel_id) {
  auto iter = panel_transitions_.find(panel_id);
  if (iter == panel_transitions_.end() || !iter->second.active) {
    return;
  }

  // Pop the alpha style var pushed in ApplyPanelTransitionPre
  ImGui::PopStyleVar();
}

bool Animator::IsEnabled() const {
  if (ImGui::GetCurrentContext() == nullptr) {
    return false;
  }
  const auto& theme = ThemeManager::Get().GetCurrentTheme();
  return theme.enable_animations;
}

Animator::AnimationState& Animator::GetState(const std::string& panel_id,
                                             const std::string& anim_id) {
  return panels_[panel_id][anim_id];
}

float Animator::ComputeStep(float speed) const {
  if (ImGui::GetCurrentContext() == nullptr) {
    return 1.0f;
  }

  const auto& theme = ThemeManager::Get().GetCurrentTheme();
  const float scaled_speed = speed * theme.animation_speed;
  const float delta = ImGui::GetIO().DeltaTime;
  const float t = 1.0f - std::exp(-scaled_speed * delta);
  return std::clamp(t, 0.0f, 1.0f);
}

Animator& GetAnimator() {
  static Animator animator;
  return animator;
}

}  // namespace gui
}  // namespace yaze
