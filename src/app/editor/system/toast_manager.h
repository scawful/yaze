#ifndef YAZE_APP_EDITOR_SYSTEM_TOAST_MANAGER_H
#define YAZE_APP_EDITOR_SYSTEM_TOAST_MANAGER_H

#include <deque>
#include <string>

#include "imgui/imgui.h"

namespace yaze {
namespace editor {

enum class ToastType { kInfo, kSuccess, kWarning, kError };

struct Toast {
  std::string message;
  ToastType type = ToastType::kInfo;
  float ttl_seconds = 3.0f;
};

class ToastManager {
 public:
  void Show(const std::string &message, ToastType type = ToastType::kInfo,
            float ttl_seconds = 3.0f) {
    toasts_.push_back({message, type, ttl_seconds});
  }

  void Draw() {
    if (toasts_.empty()) return;
    ImGuiIO &io = ImGui::GetIO();
    ImVec2 pos(io.DisplaySize.x - 10.f, 40.f);

    // Iterate copy so we can mutate ttl while drawing ordered from newest.
    for (auto it = toasts_.begin(); it != toasts_.end();) {
      Toast &t = *it;
      ImVec4 bg;
      switch (t.type) {
        case ToastType::kInfo: bg = ImVec4(0.10f, 0.10f, 0.10f, 0.85f); break;
        case ToastType::kSuccess: bg = ImVec4(0.10f, 0.30f, 0.10f, 0.85f); break;
        case ToastType::kWarning: bg = ImVec4(0.30f, 0.25f, 0.05f, 0.90f); break;
        case ToastType::kError: bg = ImVec4(0.40f, 0.10f, 0.10f, 0.90f); break;
      }
      ImGui::SetNextWindowBgAlpha(bg.w);
      ImGui::SetNextWindowPos(pos, ImGuiCond_Always, ImVec2(1.f, 0.f));
      ImGuiWindowFlags flags = ImGuiWindowFlags_NoDecoration |
                               ImGuiWindowFlags_AlwaysAutoResize |
                               ImGuiWindowFlags_NoSavedSettings |
                               ImGuiWindowFlags_NoNav;
      ImGui::PushStyleColor(ImGuiCol_WindowBg, bg);
      if (ImGui::Begin("##toast", nullptr, flags)) {
        ImGui::TextUnformatted(t.message.c_str());
      }
      ImGui::End();
      ImGui::PopStyleColor(1);

      // Decrease TTL
      t.ttl_seconds -= io.DeltaTime;
      if (t.ttl_seconds <= 0.f) {
        it = toasts_.erase(it);
      } else {
        // Next toast stacks below
        pos.y += ImGui::GetItemRectSize().y + 6.f;
        ++it;
      }
    }
  }

 private:
  std::deque<Toast> toasts_;
};

}  // namespace editor
}  // namespace yaze

#endif  // YAZE_APP_EDITOR_SYSTEM_TOAST_MANAGER_H


