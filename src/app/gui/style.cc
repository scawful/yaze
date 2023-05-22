#include "style.h"

#include "imgui/imgui.h"
#include "imgui/imgui_internal.h"

namespace yaze {
namespace gui {

void ColorsYaze() {
  ImGuiStyle *style = &ImGui::GetStyle();
  ImVec4 *colors = style->Colors;

  style->WindowPadding = ImVec2(10.f, 10.f);
  style->FramePadding = ImVec2(10.f, 3.f);
  style->CellPadding = ImVec2(4.f, 5.f);
  style->ItemSpacing = ImVec2(10.f, 5.f);
  style->ItemInnerSpacing = ImVec2(5.f, 5.f);
  style->TouchExtraPadding = ImVec2(0.f, 0.f);
  style->IndentSpacing = 20.f;
  style->ScrollbarSize = 14.f;
  style->GrabMinSize = 15.f;

  style->WindowBorderSize = 0.f;
  style->ChildBorderSize = 1.f;
  style->PopupBorderSize = 1.f;
  style->FrameBorderSize = 0.f;
  style->TabBorderSize = 0.f;

  style->WindowRounding = 0.f;
  style->ChildRounding = 0.f;
  style->FrameRounding = 5.f;
  style->PopupRounding = 0.f;
  style->ScrollbarRounding = 5.f;

  auto alttpDarkGreen = ImVec4(0.18f, 0.26f, 0.18f, 1.0f);
  auto alttpMidGreen = ImVec4(0.28f, 0.36f, 0.28f, 1.0f);
  auto allttpLightGreen = ImVec4(0.36f, 0.45f, 0.36f, 1.0f);
  auto allttpLightestGreen = ImVec4(0.49f, 0.57f, 0.49f, 1.0f);

  colors[ImGuiCol_MenuBarBg] = alttpDarkGreen;
  colors[ImGuiCol_TitleBg] = alttpMidGreen;

  colors[ImGuiCol_Header] = alttpDarkGreen;
  colors[ImGuiCol_HeaderHovered] = allttpLightGreen;
  colors[ImGuiCol_HeaderActive] = alttpMidGreen;

  colors[ImGuiCol_TitleBgActive] = alttpDarkGreen;
  colors[ImGuiCol_TitleBgCollapsed] = alttpMidGreen;

  colors[ImGuiCol_Tab] = alttpDarkGreen;
  colors[ImGuiCol_TabHovered] = alttpMidGreen;
  colors[ImGuiCol_TabActive] = ImVec4(0.347f, 0.466f, 0.347f, 1.000f);

  colors[ImGuiCol_Button] = alttpMidGreen;
  colors[ImGuiCol_ButtonHovered] = allttpLightestGreen;
  colors[ImGuiCol_ButtonActive] = allttpLightGreen;

  colors[ImGuiCol_ScrollbarBg] = ImVec4(0.36f, 0.45f, 0.36f, 0.60f);
  colors[ImGuiCol_ScrollbarGrab] = ImVec4(0.36f, 0.45f, 0.36f, 0.30f);
  colors[ImGuiCol_ScrollbarGrabHovered] = ImVec4(0.36f, 0.45f, 0.36f, 0.40f);
  colors[ImGuiCol_ScrollbarGrabActive] = ImVec4(0.36f, 0.45f, 0.36f, 0.60f);

  colors[ImGuiCol_Text] = ImVec4(0.90f, 0.90f, 0.90f, 1.00f);
  colors[ImGuiCol_TextDisabled] = ImVec4(0.60f, 0.60f, 0.60f, 1.00f);
  colors[ImGuiCol_WindowBg] = ImVec4(0.00f, 0.00f, 0.00f, 0.85f);
  colors[ImGuiCol_ChildBg] = ImVec4(0.00f, 0.00f, 0.00f, 0.00f);
  colors[ImGuiCol_PopupBg] = ImVec4(0.11f, 0.11f, 0.14f, 0.92f);
  colors[ImGuiCol_Border] = allttpLightGreen;
  colors[ImGuiCol_BorderShadow] = ImVec4(0.00f, 0.00f, 0.00f, 0.00f);

  colors[ImGuiCol_FrameBg] = ImVec4(0.43f, 0.43f, 0.43f, 0.39f);
  colors[ImGuiCol_FrameBgHovered] = ImVec4(0.28f, 0.36f, 0.28f, 0.40f);
  colors[ImGuiCol_FrameBgActive] = ImVec4(0.28f, 0.36f, 0.28f, 0.69f);

  colors[ImGuiCol_CheckMark] = ImVec4(0.90f, 0.90f, 0.90f, 0.50f);
  colors[ImGuiCol_SliderGrab] = ImVec4(1.00f, 1.00f, 1.00f, 0.30f);
  colors[ImGuiCol_SliderGrabActive] = ImVec4(0.36f, 0.45f, 0.36f, 0.60f);

  colors[ImGuiCol_Separator] = ImVec4(0.50f, 0.50f, 0.50f, 0.60f);
  colors[ImGuiCol_SeparatorHovered] = ImVec4(0.60f, 0.60f, 0.70f, 1.00f);
  colors[ImGuiCol_SeparatorActive] = ImVec4(0.70f, 0.70f, 0.90f, 1.00f);
  colors[ImGuiCol_ResizeGrip] = ImVec4(1.00f, 1.00f, 1.00f, 0.10f);
  colors[ImGuiCol_ResizeGripHovered] = ImVec4(0.78f, 0.82f, 1.00f, 0.60f);
  colors[ImGuiCol_ResizeGripActive] = ImVec4(0.78f, 0.82f, 1.00f, 0.90f);

  colors[ImGuiCol_TabUnfocused] =
      ImLerp(colors[ImGuiCol_Tab], colors[ImGuiCol_TitleBg], 0.80f);
  colors[ImGuiCol_TabUnfocusedActive] =
      ImLerp(colors[ImGuiCol_TabActive], colors[ImGuiCol_TitleBg], 0.40f);
  colors[ImGuiCol_PlotLines] = ImVec4(1.00f, 1.00f, 1.00f, 1.00f);
  colors[ImGuiCol_PlotLinesHovered] = ImVec4(0.90f, 0.70f, 0.00f, 1.00f);
  colors[ImGuiCol_PlotHistogram] = ImVec4(0.90f, 0.70f, 0.00f, 1.00f);
  colors[ImGuiCol_PlotHistogramHovered] = ImVec4(1.00f, 0.60f, 0.00f, 1.00f);
  colors[ImGuiCol_TableHeaderBg] = alttpDarkGreen;
  colors[ImGuiCol_TableBorderStrong] = alttpMidGreen;
  colors[ImGuiCol_TableBorderLight] =
      ImVec4(0.26f, 0.26f, 0.28f, 1.00f);  // Prefer using Alpha=1.0 here
  colors[ImGuiCol_TableRowBg] = ImVec4(0.00f, 0.00f, 0.00f, 0.00f);
  colors[ImGuiCol_TableRowBgAlt] = ImVec4(1.00f, 1.00f, 1.00f, 0.07f);
  colors[ImGuiCol_TextSelectedBg] = ImVec4(0.00f, 0.00f, 1.00f, 0.35f);
  colors[ImGuiCol_DragDropTarget] = ImVec4(1.00f, 1.00f, 0.00f, 0.90f);
  colors[ImGuiCol_NavHighlight] = colors[ImGuiCol_HeaderHovered];
  colors[ImGuiCol_NavWindowingHighlight] = ImVec4(1.00f, 1.00f, 1.00f, 0.70f);
  colors[ImGuiCol_NavWindowingDimBg] = ImVec4(0.80f, 0.80f, 0.80f, 0.20f);
  colors[ImGuiCol_ModalWindowDimBg] = ImVec4(0.20f, 0.20f, 0.20f, 0.35f);
}
}  // namespace gui
}  // namespace yaze