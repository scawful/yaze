#ifndef YAZE_APP_GUI_ANIMATION_ANIMATOR_H_
#define YAZE_APP_GUI_ANIMATION_ANIMATOR_H_

#include <string>
#include <unordered_map>
#include <unordered_set>

#include "imgui/imgui.h"

namespace yaze {
namespace gui {

// Transition types for panel animations
enum class TransitionType {
  kNone,
  kFade,       // Simple alpha fade
  kSlideLeft,  // Slide in from left
  kSlideRight, // Slide in from right
  kSlideUp,    // Slide in from bottom
  kSlideDown,  // Slide in from top
  kScale,      // Scale from center
  kExpand      // Expand from point
};

// Shared editor/workspace motion profile.
enum class MotionProfile {
  kSnappy = 0,
  kStandard = 1,
  kRelaxed = 2,
};

class Animator {
 public:
  // Static easing functions
  static float Lerp(float a, float b, float t);
  static float EaseOutCubic(float t);
  static float EaseInOutCubic(float t);
  static float EaseOutElastic(float t);
  static float EaseOutBack(float t);
  static ImVec4 LerpColor(ImVec4 a, ImVec4 b, float t);

  // Basic animations
  float Animate(const std::string& panel_id, const std::string& anim_id,
                float target, float speed = 5.0f);
  ImVec4 AnimateColor(const std::string& panel_id, const std::string& anim_id,
                      ImVec4 target, float speed = 5.0f);

  // Panel transition animations
  struct PanelTransition {
    float alpha = 1.0f;
    float offset_x = 0.0f;
    float offset_y = 0.0f;
    float scale = 1.0f;
    bool is_complete = true;
  };

  // Start a panel transition (call when panel opens)
  void BeginPanelTransition(const std::string& panel_id, TransitionType type);

  // Update and get current transition state (call each frame)
  PanelTransition UpdatePanelTransition(const std::string& panel_id,
                                        float speed = 8.0f);

  // Check if panel is currently transitioning
  bool IsPanelTransitioning(const std::string& panel_id) const;

  // Apply transition to ImGui window (call before/after Begin)
  void ApplyPanelTransitionPre(const std::string& panel_id);
  void ApplyPanelTransitionPost(const std::string& panel_id);

  void ClearAnimationsForPanel(const std::string& panel_id);
  void ClearAllAnimations();

  bool IsEnabled() const;

  void SetMotionPreferences(bool reduced_motion, MotionProfile profile);
  bool reduced_motion() const { return reduced_motion_; }
  MotionProfile motion_profile() const { return motion_profile_; }

  static MotionProfile ClampMotionProfile(int raw_profile);

 private:
  struct AnimationState {
    float value = 0.0f;
    ImVec4 color = ImVec4(0.0f, 0.0f, 0.0f, 0.0f);
    bool has_value = false;
    bool has_color = false;
  };

  struct PanelTransitionState {
    TransitionType type = TransitionType::kNone;
    float progress = 1.0f;  // 0 = start, 1 = complete
    bool active = false;
    float initial_offset_x = 0.0f;
    float initial_offset_y = 0.0f;
  };

  using AnimationMap = std::unordered_map<std::string, AnimationState>;

  AnimationState& GetState(const std::string& panel_id,
                           const std::string& anim_id);
  float ComputeStep(float speed) const;
  float GetProfileSpeedMultiplier() const;
  float ApplyTransitionEasing(float t) const;

  std::unordered_map<std::string, AnimationMap> panels_;
  std::unordered_map<std::string, PanelTransitionState> panel_transitions_;
  std::unordered_set<std::string> pushed_transition_alpha_;

  bool reduced_motion_ = false;
  MotionProfile motion_profile_ = MotionProfile::kStandard;
};

Animator& GetAnimator();

}  // namespace gui
}  // namespace yaze

#endif  // YAZE_APP_GUI_ANIMATION_ANIMATOR_H_
