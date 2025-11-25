#ifndef YAZE_APP_EDITOR_SYSTEM_TOAST_MANAGER_H
#define YAZE_APP_EDITOR_SYSTEM_TOAST_MANAGER_H

#include <chrono>
#include <deque>
#include <string>

// Must define before including imgui.h
#ifndef IMGUI_DEFINE_MATH_OPERATORS
#define IMGUI_DEFINE_MATH_OPERATORS
#endif

#include "imgui/imgui.h"

namespace yaze {
namespace editor {

enum class ToastType { kInfo, kSuccess, kWarning, kError };

struct Toast {
  std::string message;
  ToastType type = ToastType::kInfo;
  float ttl_seconds = 3.0f;
};

/**
 * @brief Entry in the notification history with timestamp
 */
struct NotificationEntry {
  std::string message;
  ToastType type;
  std::chrono::system_clock::time_point timestamp;
  bool read = false;
};

class ToastManager {
 public:
  static constexpr size_t kMaxHistorySize = 50;

  void Show(const std::string& message, ToastType type = ToastType::kInfo,
            float ttl_seconds = 3.0f) {
    toasts_.push_back({message, type, ttl_seconds});

    // Also add to notification history
    NotificationEntry entry;
    entry.message = message;
    entry.type = type;
    entry.timestamp = std::chrono::system_clock::now();
    entry.read = false;

    notification_history_.push_front(entry);

    // Trim history if too large
    while (notification_history_.size() > kMaxHistorySize) {
      notification_history_.pop_back();
    }
  }

  void Draw() {
    if (toasts_.empty())
      return;
    ImGuiIO& io = ImGui::GetIO();
    ImVec2 pos(io.DisplaySize.x - 10.f, 40.f);

    // Iterate copy so we can mutate ttl while drawing ordered from newest.
    for (auto it = toasts_.begin(); it != toasts_.end();) {
      Toast& t = *it;
      ImVec4 bg;
      switch (t.type) {
        case ToastType::kInfo:
          bg = ImVec4(0.10f, 0.10f, 0.10f, 0.85f);
          break;
        case ToastType::kSuccess:
          bg = ImVec4(0.10f, 0.30f, 0.10f, 0.85f);
          break;
        case ToastType::kWarning:
          bg = ImVec4(0.30f, 0.25f, 0.05f, 0.90f);
          break;
        case ToastType::kError:
          bg = ImVec4(0.40f, 0.10f, 0.10f, 0.90f);
          break;
      }
      ImGui::SetNextWindowBgAlpha(bg.w);
      ImGui::SetNextWindowPos(pos, ImGuiCond_Always, ImVec2(1.f, 0.f));
      ImGuiWindowFlags flags =
          ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_AlwaysAutoResize |
          ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoNav;
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

  // Notification history methods
  size_t GetUnreadCount() const {
    size_t count = 0;
    for (const auto& entry : notification_history_) {
      if (!entry.read) ++count;
    }
    return count;
  }

  const std::deque<NotificationEntry>& GetHistory() const {
    return notification_history_;
  }

  void MarkAllRead() {
    for (auto& entry : notification_history_) {
      entry.read = true;
    }
  }

  void ClearHistory() { notification_history_.clear(); }

 private:
  std::deque<Toast> toasts_;
  std::deque<NotificationEntry> notification_history_;
};

}  // namespace editor
}  // namespace yaze

#endif  // YAZE_APP_EDITOR_SYSTEM_TOAST_MANAGER_H
