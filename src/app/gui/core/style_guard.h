#ifndef YAZE_APP_GUI_CORE_STYLE_GUARD_H_
#define YAZE_APP_GUI_CORE_STYLE_GUARD_H_

#include <initializer_list>
#include <optional>
#include <variant>

#include "app/gui/core/color.h"
#include "imgui/imgui.h"

namespace yaze {
namespace gui {

// ============================================================================
// RAII Style Guards
// ============================================================================
// Replace manual PushStyleColor/PopStyleColor and PushStyleVar/PopStyleVar
// patterns with scope-based guards that automatically clean up.

/**
 * @brief RAII guard for ImGui style colors.
 *
 * Usage:
 *   StyleColorGuard guard(ImGuiCol_Text, color);
 *   StyleColorGuard guard({{ImGuiCol_Header, c1}, {ImGuiCol_HeaderHovered, c2}});
 */
class StyleColorGuard {
 public:
  struct Entry {
    ImGuiCol idx;
    ImVec4 color;
  };

  StyleColorGuard(ImGuiCol idx, const ImVec4& color) : count_(1) {
    ImGui::PushStyleColor(idx, color);
  }

  StyleColorGuard(ImGuiCol idx, const Color& color) : count_(1) {
    ImGui::PushStyleColor(idx, static_cast<ImVec4>(color));
  }

  StyleColorGuard(std::initializer_list<Entry> entries)
      : count_(static_cast<int>(entries.size())) {
    for (const auto& e : entries) {
      ImGui::PushStyleColor(e.idx, e.color);
    }
  }

  ~StyleColorGuard() {
    if (count_ > 0) ImGui::PopStyleColor(count_);
  }

  StyleColorGuard(const StyleColorGuard&) = delete;
  StyleColorGuard& operator=(const StyleColorGuard&) = delete;

 private:
  int count_;
};

/**
 * @brief RAII guard for ImGui style vars.
 *
 * Usage:
 *   StyleVarGuard guard(ImGuiStyleVar_WindowPadding, ImVec2(0, 8));
 *   StyleVarGuard guard({{ImGuiStyleVar_WindowPadding, ImVec2(0, 8)},
 *                        {ImGuiStyleVar_WindowBorderSize, 1.0f}});
 */
class StyleVarGuard {
 public:
  struct Entry {
    ImGuiStyleVar idx;
    std::variant<float, ImVec2> value;
  };

  StyleVarGuard(ImGuiStyleVar idx, float val) : count_(1) {
    ImGui::PushStyleVar(idx, val);
  }

  StyleVarGuard(ImGuiStyleVar idx, const ImVec2& val) : count_(1) {
    ImGui::PushStyleVar(idx, val);
  }

  StyleVarGuard(std::initializer_list<Entry> entries)
      : count_(static_cast<int>(entries.size())) {
    for (const auto& e : entries) {
      if (auto* f = std::get_if<float>(&e.value)) {
        ImGui::PushStyleVar(e.idx, *f);
      } else {
        ImGui::PushStyleVar(e.idx, std::get<ImVec2>(e.value));
      }
    }
  }

  ~StyleVarGuard() {
    if (count_ > 0) ImGui::PopStyleVar(count_);
  }

  StyleVarGuard(const StyleVarGuard&) = delete;
  StyleVarGuard& operator=(const StyleVarGuard&) = delete;

 private:
  int count_;
};

// ============================================================================
// Compound RAII Helpers
// ============================================================================

/**
 * @brief Configuration for styled windows and panels.
 *
 * Uses C++20 designated initializers for readable setup:
 *   StyledWindowConfig{.bg = color, .padding = {12, 12}, .border_size = 1.0f}
 */
struct StyledWindowConfig {
  std::optional<ImVec4> bg;
  std::optional<ImVec4> border;
  ImVec2 padding = {-1, -1};    // -1 means "don't push"
  ImVec2 spacing = {-1, -1};    // -1 means "don't push"
  float border_size = -1.0f;    // -1 means "don't push"
  float rounding = -1.0f;       // -1 means "don't push"
};

/**
 * @brief RAII compound guard for window-level style setup.
 *
 * Replaces the extremely common pattern of 2-3 PushStyleColor +
 * 2-3 PushStyleVar + Begin/End + manual Pop counting.
 *
 * Usage:
 *   StyledWindow bar("##ActivityBar", {.bg = bg, .padding = {0, 8}},
 *                    nullptr, flags);
 *   if (bar) { // draw contents }
 */
class StyledWindow {
 public:
  StyledWindow(const char* name, const StyledWindowConfig& config,
               bool* p_open = nullptr, ImGuiWindowFlags flags = 0)
      : color_count_(0), var_count_(0) {
    // Push colors
    if (config.bg.has_value()) {
      ImGui::PushStyleColor(ImGuiCol_WindowBg, *config.bg);
      ++color_count_;
    }
    if (config.border.has_value()) {
      ImGui::PushStyleColor(ImGuiCol_Border, *config.border);
      ++color_count_;
    }

    // Push vars
    if (config.padding.x >= 0.0f) {
      ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, config.padding);
      ++var_count_;
    }
    if (config.spacing.x >= 0.0f) {
      ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, config.spacing);
      ++var_count_;
    }
    if (config.border_size >= 0.0f) {
      ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, config.border_size);
      ++var_count_;
    }
    if (config.rounding >= 0.0f) {
      ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, config.rounding);
      ++var_count_;
    }

    is_open_ = ImGui::Begin(name, p_open, flags);
  }

  ~StyledWindow() {
    ImGui::End();
    if (var_count_ > 0) ImGui::PopStyleVar(var_count_);
    if (color_count_ > 0) ImGui::PopStyleColor(color_count_);
  }

  explicit operator bool() const { return is_open_; }

  StyledWindow(const StyledWindow&) = delete;
  StyledWindow& operator=(const StyledWindow&) = delete;

 private:
  int color_count_;
  int var_count_;
  bool is_open_;
};

/**
 * @brief RAII for fixed-position panels (activity bar, side panel, status bar).
 *
 * Handles NoTitleBar | NoResize | NoMove | NoCollapse | NoDocking + pos/size
 * automatically.
 *
 * Usage:
 *   FixedPanel panel("##SidePanel", pos, size, {.bg = bg, .padding = {12, 12}});
 *   if (panel) { // draw contents }
 */
class FixedPanel {
 public:
  FixedPanel(const char* name, const ImVec2& pos, const ImVec2& size,
             const StyledWindowConfig& config,
             ImGuiWindowFlags extra_flags = 0)
      : window_(std::nullopt) {
    ImGui::SetNextWindowPos(pos);
    ImGui::SetNextWindowSize(size);

    constexpr ImGuiWindowFlags kFixedFlags =
        ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize |
        ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoCollapse |
        ImGuiWindowFlags_NoDocking;

    window_.emplace(name, config, nullptr, kFixedFlags | extra_flags);
  }

  explicit operator bool() const {
    return window_.has_value() && static_cast<bool>(*window_);
  }

  FixedPanel(const FixedPanel&) = delete;
  FixedPanel& operator=(const FixedPanel&) = delete;

 private:
  std::optional<StyledWindow> window_;
};

}  // namespace gui
}  // namespace yaze

#endif  // YAZE_APP_GUI_CORE_STYLE_GUARD_H_
