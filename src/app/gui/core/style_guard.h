#ifndef YAZE_APP_GUI_CORE_STYLE_GUARD_H_
#define YAZE_APP_GUI_CORE_STYLE_GUARD_H_

#include <initializer_list>
#include <optional>
#include <variant>

#include "app/gui/core/color.h"
#include "imgui/imgui.h"
// ImGuiItemFlags_ReadOnly (used by ReadOnlyScope) is declared only here.
#include "imgui/imgui_internal.h"

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
    if (count_ > 0)
      ImGui::PopStyleColor(count_);
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
    if (count_ > 0)
      ImGui::PopStyleVar(count_);
  }

  StyleVarGuard(const StyleVarGuard&) = delete;
  StyleVarGuard& operator=(const StyleVarGuard&) = delete;

 private:
  int count_;
};

// ============================================================================
// RAII Scope Guards: ID, Disabled, and Item Flag Stacks
// ============================================================================
// Each guard pushes in its constructor and pops in its destructor, so an early
// return, break, or continue inside the scope cannot leave an ImGui stack
// unbalanced.

/**
 * @brief RAII guard for ImGui::PushID / ImGui::PopID.
 *
 * Usage:
 *   for (int i = 0; i < count; ++i) {
 *     IdScope id(i);
 *     ImGui::Button("Delete");  // A different ID on every row.
 *   }
 */
class IdScope {
 public:
  explicit IdScope(const char* str_id) { ImGui::PushID(str_id); }
  explicit IdScope(int int_id) { ImGui::PushID(int_id); }
  explicit IdScope(const void* ptr_id) { ImGui::PushID(ptr_id); }

  ~IdScope() { ImGui::PopID(); }

  IdScope(const IdScope&) = delete;
  IdScope& operator=(const IdScope&) = delete;
};

/**
 * @brief RAII guard for ImGui::BeginDisabled / ImGui::EndDisabled.
 *
 * DisabledScope(false) still pushes and pops, so the argument can be a
 * runtime condition with no if around the scope. A DisabledScope(false)
 * nested inside a disabled scope does not re-enable items; ImGui keeps them
 * disabled.
 *
 * Usage:
 *   DisabledScope disabled(!rom_loaded);
 *   ImGui::Button("Save");
 */
class DisabledScope {
 public:
  explicit DisabledScope(bool disabled = true) {
    ImGui::BeginDisabled(disabled);
  }

  ~DisabledScope() { ImGui::EndDisabled(); }

  DisabledScope(const DisabledScope&) = delete;
  DisabledScope& operator=(const DisabledScope&) = delete;
};

/**
 * @brief RAII guard for ImGui::PushItemFlag / ImGui::PopItemFlag.
 *
 * Usage:
 *   ItemFlagScope no_tab(ImGuiItemFlags_NoTabStop, true);
 *   ItemFlagScope repeat(ImGuiItemFlags_ButtonRepeat, true);
 */
class ItemFlagScope {
 public:
  ItemFlagScope(ImGuiItemFlags option, bool enabled) {
    ImGui::PushItemFlag(option, enabled);
  }

  ~ItemFlagScope() { ImGui::PopItemFlag(); }

  ItemFlagScope(const ItemFlagScope&) = delete;
  ItemFlagScope& operator=(const ItemFlagScope&) = delete;
};

/**
 * @brief RAII guard that pushes ImGuiItemFlags_ReadOnly.
 *
 * Read-only items still hover and show tooltips, and are not dimmed, but
 * their value does not change. ImGui 1.93 marks the flag [ALPHA]. Only
 * Drag and Slider widgets, InputText, ColorEdit4, and ColorPicker4 honor it;
 * Button, Checkbox, and Combo still react to clicks, so use DisabledScope
 * for those.
 *
 * Usage:
 *   ReadOnlyScope read_only(!can_edit);
 *   ImGui::DragInt("Room", &room_id);
 */
class ReadOnlyScope {
 public:
  explicit ReadOnlyScope(bool read_only = true) {
    ImGui::PushItemFlag(ImGuiItemFlags_ReadOnly, read_only);
  }

  ~ReadOnlyScope() { ImGui::PopItemFlag(); }

  ReadOnlyScope(const ReadOnlyScope&) = delete;
  ReadOnlyScope& operator=(const ReadOnlyScope&) = delete;
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
  ImVec2 padding = {-1, -1};  // -1 means "don't push"
  ImVec2 spacing = {-1, -1};  // -1 means "don't push"
  float border_size = -1.0f;  // -1 means "don't push"
  float rounding = -1.0f;     // -1 means "don't push"
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
    if (var_count_ > 0)
      ImGui::PopStyleVar(var_count_);
    if (color_count_ > 0)
      ImGui::PopStyleColor(color_count_);
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
             const StyledWindowConfig& config, ImGuiWindowFlags extra_flags = 0)
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

// ============================================================================
// StyledChild — RAII for BeginChild/EndChild with optional styling
// ============================================================================

/**
 * @brief Configuration for styled child windows.
 *
 * Uses C++20 designated initializers:
 *   StyledChildConfig{.bg = color, .border = border_color, .rounding = 4.0f}
 */
struct StyledChildConfig {
  std::optional<ImVec4> bg;      // ImGuiCol_ChildBg
  std::optional<ImVec4> border;  // ImGuiCol_Border
  float rounding = -1.0f;        // ImGuiStyleVar_ChildRounding
  float border_size = -1.0f;     // ImGuiStyleVar_ChildBorderSize
};

/**
 * @brief RAII guard for ImGui child windows with optional styling.
 *
 * Replaces the common pattern of PushStyleColor(ChildBg) + BeginChild +
 * EndChild + PopStyleColor.
 *
 * Usage:
 *   StyledChild child("##list", {0, 200}, {.bg = dark_bg}, true);
 *   if (child) { // draw contents }
 */
class StyledChild {
 public:
  StyledChild(const char* id, const ImVec2& size,
              const StyledChildConfig& config, bool has_border = false,
              ImGuiWindowFlags flags = 0)
      : color_count_(0), var_count_(0) {
    if (config.bg.has_value()) {
      ImGui::PushStyleColor(ImGuiCol_ChildBg, *config.bg);
      ++color_count_;
    }
    if (config.border.has_value()) {
      ImGui::PushStyleColor(ImGuiCol_Border, *config.border);
      ++color_count_;
    }
    if (config.rounding >= 0.0f) {
      ImGui::PushStyleVar(ImGuiStyleVar_ChildRounding, config.rounding);
      ++var_count_;
    }
    if (config.border_size >= 0.0f) {
      ImGui::PushStyleVar(ImGuiStyleVar_ChildBorderSize, config.border_size);
      ++var_count_;
    }
    is_open_ = ImGui::BeginChild(id, size, has_border, flags);
  }

  ~StyledChild() {
    ImGui::EndChild();
    if (var_count_ > 0)
      ImGui::PopStyleVar(var_count_);
    if (color_count_ > 0)
      ImGui::PopStyleColor(color_count_);
  }

  explicit operator bool() const { return is_open_; }

  StyledChild(const StyledChild&) = delete;
  StyledChild& operator=(const StyledChild&) = delete;

 private:
  int color_count_;
  int var_count_;
  bool is_open_;
};

}  // namespace gui
}  // namespace yaze

#endif  // YAZE_APP_GUI_CORE_STYLE_GUARD_H_
