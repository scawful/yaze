#ifndef YAZE_APP_GUI_CORE_DRAW_SAMPLING_H_
#define YAZE_APP_GUI_CORE_DRAW_SAMPLING_H_

#include "imgui/imgui.h"

namespace yaze {
namespace gui {

/**
 * @brief RAII scope that samples the images drawn inside it with
 * nearest-neighbor filtering.
 *
 * Renderer backends sample textures with linear filtering by default, which
 * blurs pixel-art bitmaps when a canvas zooms them. The scope adds the
 * backend's DrawCallback_SetSamplerNearest to the draw list. On destruction it
 * adds DrawCallback_SetSamplerLinear, or DrawCallback_ResetRenderState when
 * the backend has no linear callback, so text and lines drawn later keep
 * linear sampling.
 *
 * When the backend does not provide DrawCallback_SetSamplerNearest, the scope
 * adds no callbacks. SDLRenderer2 is such a backend: it cannot change the
 * scale mode while rendering, so its textures keep the SDL texture setting.
 *
 * A scope created while another scope is active on the same draw list adds
 * nothing. Otherwise the inner scope would switch the outer scope's remaining
 * images back to linear. Create scopes on the stack only.
 *
 * Usage:
 *   {
 *     ScopedNearestSampling nearest_sampling(draw_list);
 *     draw_list->AddImage(texture, p_min, p_max);
 *   }
 */
class ScopedNearestSampling {
 public:
  explicit ScopedNearestSampling(ImDrawList* draw_list) {
    if (draw_list == nullptr || IsActiveOn(draw_list)) {
      return;
    }
    ImDrawCallback set_nearest =
        ImGui::GetPlatformIO().DrawCallback_SetSamplerNearest;
    if (set_nearest == nullptr) {
      return;
    }
    draw_list->AddCallback(set_nearest, nullptr);
    draw_list_ = draw_list;
    outer_ = innermost_;
    innermost_ = this;
  }

  ~ScopedNearestSampling() {
    if (draw_list_ == nullptr) {
      return;
    }
    innermost_ = outer_;
    const ImGuiPlatformIO& platform_io = ImGui::GetPlatformIO();
    if (platform_io.DrawCallback_SetSamplerLinear != nullptr) {
      draw_list_->AddCallback(platform_io.DrawCallback_SetSamplerLinear,
                              nullptr);
    } else if (platform_io.DrawCallback_ResetRenderState != nullptr) {
      draw_list_->AddCallback(platform_io.DrawCallback_ResetRenderState,
                              nullptr);
    }
  }

  ScopedNearestSampling(const ScopedNearestSampling&) = delete;
  ScopedNearestSampling& operator=(const ScopedNearestSampling&) = delete;

  // True when this scope added the nearest callback and will restore it.
  bool active() const { return draw_list_ != nullptr; }

 private:
  static bool IsActiveOn(const ImDrawList* draw_list) {
    for (const ScopedNearestSampling* scope = innermost_; scope != nullptr;
         scope = scope->outer_) {
      if (scope->draw_list_ == draw_list) {
        return true;
      }
    }
    return false;
  }

  ImDrawList* draw_list_ = nullptr;
  const ScopedNearestSampling* outer_ = nullptr;

  // Innermost active scope on this thread; scopes link to the one they nest
  // in through outer_.
  static inline thread_local const ScopedNearestSampling* innermost_ = nullptr;
};

}  // namespace gui
}  // namespace yaze

#endif  // YAZE_APP_GUI_CORE_DRAW_SAMPLING_H_
