#include "app/gui/widgets/font_picker.h"

#include <array>
#include <cstdio>

#include "imgui/imgui.h"
#include "imgui/imgui_internal.h"

namespace yaze {
namespace gui {
namespace font_picker_internal {

const char* FontNameAt(int index) {
  static thread_local std::array<char, 64> fallback_name;
  ImGuiIO& io = ImGui::GetIO();
  if (io.Fonts == nullptr || index < 0 || index >= io.Fonts->Fonts.Size) {
    std::snprintf(fallback_name.data(), fallback_name.size(), "Font #%d",
                  index);
    return fallback_name.data();
  }
  const ImFont* font = io.Fonts->Fonts[index];
  if (font == nullptr) {
    std::snprintf(fallback_name.data(), fallback_name.size(), "Font #%d",
                  index);
    return fallback_name.data();
  }
  const char* name = font->GetDebugName();
  if (name != nullptr && name[0] != '\0' &&
      // GetDebugName() returns "<unknown>" when no ImFontConfig::Name was set.
      std::string_view(name) != "<unknown>") {
    return name;
  }
  std::snprintf(fallback_name.data(), fallback_name.size(), "Font #%d", index);
  return fallback_name.data();
}

int RegisteredFontCount() {
  ImGuiContext* ctx = ImGui::GetCurrentContext();
  if (ctx == nullptr)
    return 0;
  ImGuiIO& io = ImGui::GetIO();
  if (io.Fonts == nullptr)
    return 0;
  return io.Fonts->Fonts.Size;
}

}  // namespace font_picker_internal

bool FontPicker(const char* label, int* index) {
  if (index == nullptr)
    return false;

  const int count = font_picker_internal::RegisteredFontCount();
  if (count <= 0) {
    ImGui::BeginDisabled();
    int dummy = 0;
    ImGui::Combo(label, &dummy, "(no fonts loaded)\0\0");
    ImGui::EndDisabled();
    return false;
  }

  const int current = (*index < 0 || *index >= count) ? 0 : *index;
  const char* preview_name = font_picker_internal::FontNameAt(current);

  bool changed = false;
  if (ImGui::BeginCombo(label, preview_name)) {
    ImGuiIO& io = ImGui::GetIO();
    for (int i = 0; i < count; ++i) {
      const bool is_selected = (i == current);
      const char* name = font_picker_internal::FontNameAt(i);

      ImFont* font = io.Fonts->Fonts[i];
      if (font != nullptr) {
        ImGui::PushFont(font);
      }
      const bool picked = ImGui::Selectable(name, is_selected);
      if (font != nullptr) {
        ImGui::PopFont();
      }
      if (picked && i != current) {
        *index = i;
        changed = true;
      }
      if (is_selected) {
        ImGui::SetItemDefaultFocus();
      }
    }
    ImGui::EndCombo();
  }
  return changed;
}

}  // namespace gui
}  // namespace yaze
