#define IMGUI_DEFINE_MATH_OPERATORS
#include "app/gui/widgets/centered_icon_button.h"

#include <algorithm>

#include "app/gui/core/style_guard.h"
#include "imgui/imgui.h"
#include "imgui/imgui_internal.h"

namespace yaze::gui {

// Material icons are merged with a text-baseline offset. Center their visible
// glyph bounds, not the text line box, while retaining ordinary button input,
// navigation, IDs, and disabled styling. This is for one-glyph controls only.
bool CenteredIconButton(const char* label, float button_size) {
  const bool pressed = [&] {
    StyleColorGuard transparent_text(ImGuiCol_Text, ImVec4(0, 0, 0, 0));
    return ImGui::Button(label, ImVec2(button_size, button_size));
  }();
  if (!ImGui::IsItemVisible())
    return pressed;

  unsigned int codepoint = 0;
  ImTextCharFromUtf8(&codepoint, label, nullptr);
  auto* baked = ImGui::GetFontBaked();
  const auto* glyph = baked->FindGlyph(static_cast<ImWchar>(codepoint));
  if (!glyph || !glyph->Visible)
    return pressed;

  const float font_scale = ImGui::GetFontSize() / baked->Size;
  const float width = (glyph->X1 - glyph->X0) * font_scale;
  const float height = (glyph->Y1 - glyph->Y0) * font_scale;
  if (width <= 0 || height <= 0)
    return pressed;
  const ImVec2 min = ImGui::GetItemRectMin();
  const ImVec2 max = ImGui::GetItemRectMax();
  // Keep the existing compact frame size; fit oversized glyphs inside it.
  const float inset = ImGui::GetFontSize() / 8.0f;
  const float available = std::max(1.0f, button_size - 2 * inset);
  const float fit = std::min({1.0f, available / width, available / height});
  const ImVec2 glyph_size(width * fit, height * fit);
  const ImVec2 glyph_min((min.x + max.x - glyph_size.x) * 0.5f,
                         (min.y + max.y - glyph_size.y) * 0.5f);
  ImGui::GetWindowDrawList()->AddImage(
      ImGui::GetFont()->OwnerAtlas->TexRef, glyph_min,
      ImVec2(glyph_min.x + glyph_size.x, glyph_min.y + glyph_size.y),
      ImVec2(glyph->U0, glyph->V0), ImVec2(glyph->U1, glyph->V1),
      ImGui::GetColorU32(ImGuiCol_Text));
  return pressed;
}

}  // namespace yaze::gui
