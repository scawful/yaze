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
