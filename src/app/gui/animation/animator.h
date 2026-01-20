#ifndef YAZE_APP_GUI_ANIMATION_ANIMATOR_H_
#define YAZE_APP_GUI_ANIMATION_ANIMATOR_H_

#include <string>
#include <unordered_map>

#include "imgui/imgui.h"

namespace yaze {
namespace gui {

class Animator {
 public:
  static float Lerp(float a, float b, float t);
  static float EaseOutCubic(float t);
  static ImVec4 LerpColor(ImVec4 a, ImVec4 b, float t);

  float Animate(const std::string& panel_id, const std::string& anim_id,
                float target, float speed = 5.0f);
  ImVec4 AnimateColor(const std::string& panel_id, const std::string& anim_id,
                      ImVec4 target, float speed = 5.0f);
  void ClearAnimationsForPanel(const std::string& panel_id);

  bool IsEnabled() const;

 private:
  struct AnimationState {
    float value = 0.0f;
    ImVec4 color = ImVec4(0.0f, 0.0f, 0.0f, 0.0f);
    bool has_value = false;
    bool has_color = false;
  };

  using AnimationMap = std::unordered_map<std::string, AnimationState>;

  AnimationState& GetState(const std::string& panel_id,
                           const std::string& anim_id);
  float ComputeStep(float speed) const;

  std::unordered_map<std::string, AnimationMap> panels_;
};

Animator& GetAnimator();

}  // namespace gui
}  // namespace yaze

#endif  // YAZE_APP_GUI_ANIMATION_ANIMATOR_H_
