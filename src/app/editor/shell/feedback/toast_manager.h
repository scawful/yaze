#ifndef YAZE_APP_EDITOR_SYSTEM_TOAST_MANAGER_H
#define YAZE_APP_EDITOR_SYSTEM_TOAST_MANAGER_H

#include <algorithm>
#include <chrono>
#include <deque>
#include <functional>
#include <string>
#include <utility>

// Must define before including imgui.h
#ifndef IMGUI_DEFINE_MATH_OPERATORS
#define IMGUI_DEFINE_MATH_OPERATORS
#endif

#include "app/gui/core/style.h"
#include "app/gui/core/style_guard.h"
#include "app/gui/core/theme_manager.h"
#include "imgui/imgui.h"

namespace yaze {
namespace editor {

enum class ToastType { kInfo, kSuccess, kWarning, kError };

struct Toast {
  std::string message;
  ToastType type = ToastType::kInfo;
  float ttl_seconds = 3.0f;
  /// Optional button label (empty = message-only toast).
  std::string action_label;
  std::function<void()> on_action;
  std::chrono::steady_clock::time_point last_tick;
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
  using Clock = std::chrono::steady_clock;

  explicit ToastManager(std::function<Clock::time_point()> now = Clock::now)
      : now_(std::move(now)) {}

  static constexpr size_t kMaxHistorySize = 50;
  // Suppress duplicate (same message+type) toasts within this window.
  static constexpr float kDedupCooldownSeconds = 1.0f;

  void Show(const std::string& message, ToastType type = ToastType::kInfo,
            float ttl_seconds = 3.0f, std::string action_label = {},
            std::function<void()> on_action = nullptr) {
    // Dedup: skip if same message+type was shown within the cooldown window.
    const auto now = now_();
    if (message == last_shown_message_ && type == last_shown_type_) {
      auto elapsed = std::chrono::duration<float>(now - last_shown_time_);
      if (elapsed.count() < kDedupCooldownSeconds) {
        ++dedup_suppressed_count_;
        return;
      }
    }
    last_shown_message_ = message;
    last_shown_type_ = type;
    last_shown_time_ = now;

    toasts_.push_back({message, type, ttl_seconds, std::move(action_label),
                       std::move(on_action), now});

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

  /// Number of toasts suppressed by dedup since last reset.
  size_t dedup_suppressed_count() const { return dedup_suppressed_count_; }
  void reset_dedup_stats() { dedup_suppressed_count_ = 0; }

  /// Active on-screen toasts (not notification history). For tests / UI.
  size_t pending_count() const { return toasts_.size(); }
  const Toast* GetPendingToast(size_t index) const {
    if (index >= toasts_.size()) {
      return nullptr;
    }
    return &toasts_[index];
  }

  void Draw() {
    if (toasts_.empty())
      return;
    ImGuiIO& io = ImGui::GetIO();
    const auto& theme = gui::ThemeManager::Get().GetCurrentTheme();
    const auto now = now_();

    // Position toasts from the top-right, below menu bar
    ImVec2 pos(io.DisplaySize.x - 16.f, 48.f);

    // Iterate copy so we can mutate ttl while drawing ordered from newest.
    for (auto it = toasts_.begin(); it != toasts_.end();) {
      Toast& t = *it;

      // Use theme colors for toast backgrounds
      ImVec4 bg;
      ImVec4 text_color;
      switch (t.type) {
        case ToastType::kInfo:
          bg = gui::GetSurfaceContainerHighVec4();
          bg.w = 0.95f;
          text_color = gui::ConvertColorToImVec4(theme.text_primary);
          break;
        case ToastType::kSuccess:
          bg = gui::ConvertColorToImVec4(theme.success);
          bg.w = 0.95f;
          text_color = ImVec4(1.0f, 1.0f, 1.0f, 1.0f);
          break;
        case ToastType::kWarning:
          bg = gui::ConvertColorToImVec4(theme.warning);
          bg.w = 0.95f;
          text_color = ImVec4(0.0f, 0.0f, 0.0f, 1.0f);
          break;
        case ToastType::kError:
          bg = gui::ConvertColorToImVec4(theme.error);
          bg.w = 0.95f;
          text_color = ImVec4(1.0f, 1.0f, 1.0f, 1.0f);
          break;
      }

      ImGui::SetNextWindowBgAlpha(bg.w);
      ImGui::SetNextWindowPos(pos, ImGuiCond_Always, ImVec2(1.f, 0.f));
      ImGuiWindowFlags flags =
          ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_AlwaysAutoResize |
          ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoNav |
          ImGuiWindowFlags_NoFocusOnAppearing;

      gui::StyleColorGuard color_guard(
          {{ImGuiCol_WindowBg, bg}, {ImGuiCol_Text, text_color}});
      gui::StyleVarGuard var_guard(
          {{ImGuiStyleVar_WindowRounding, 6.0f},
           {ImGuiStyleVar_WindowPadding, ImVec2(12.0f, 8.0f)}});

      // Use unique window name per toast to allow multiple
      char window_name[32];
      snprintf(window_name, sizeof(window_name), "##toast_%p", (void*)&t);

      bool action_clicked = false;
      bool hovered = false;
      float toast_height = 0.0f;
      if (ImGui::Begin(window_name, nullptr, flags)) {
        // Constrain width so long migrant tips wrap instead of spanning the
        // display.
        const float max_width = std::min(360.f, io.DisplaySize.x * 0.4f);
        ImGui::PushTextWrapPos(ImGui::GetCursorPos().x + max_width);
        ImGui::TextUnformatted(t.message.c_str());
        ImGui::PopTextWrapPos();
        if (!t.action_label.empty()) {
          ImGui::Spacing();
          if (ImGui::SmallButton(t.action_label.c_str())) {
            action_clicked = true;
          }
        }
        hovered = ImGui::IsWindowHovered();
        toast_height = ImGui::GetWindowSize().y;
      }
      ImGui::End();

      if (action_clicked) {
        if (t.on_action) {
          t.on_action();
        }
        it = toasts_.erase(it);
        continue;
      }

      // Frame DeltaTime can include a modal dialog before this toast existed.
      const auto elapsed = std::chrono::duration<float>(now - t.last_tick);
      t.last_tick = now;
      // Decrease TTL (pause while hovered so users can reach the action).
      if (!hovered) {
        t.ttl_seconds -= elapsed.count();
      }
      if (t.ttl_seconds <= 0.f) {
        it = toasts_.erase(it);
      } else {
        // ItemRect is only the last text/button. Stack by the full toast
        // window height so actionable toasts cannot overlap the next toast.
        pos.y += std::max(toast_height, ImGui::GetFrameHeight()) + 8.f;
        ++it;
      }
    }
  }

  // Notification history methods
  size_t GetUnreadCount() const {
    size_t count = 0;
    for (const auto& entry : notification_history_) {
      if (!entry.read)
        ++count;
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
  std::function<Clock::time_point()> now_;
  std::deque<Toast> toasts_;
  std::deque<NotificationEntry> notification_history_;

  // Dedup state: suppress rapid-fire identical toasts.
  std::string last_shown_message_;
  ToastType last_shown_type_ = ToastType::kInfo;
  std::chrono::steady_clock::time_point last_shown_time_{};
  size_t dedup_suppressed_count_ = 0;
};

}  // namespace editor
}  // namespace yaze

#endif  // YAZE_APP_EDITOR_SYSTEM_TOAST_MANAGER_H
