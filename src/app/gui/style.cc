#include "style.h"

#include "imgui/imgui.h"
#include "imgui/imgui_internal.h"

namespace yaze {
namespace app {

namespace gui {

void DrawDisplaySettings(ImGuiStyle* ref) {
  // You can pass in a reference ImGuiStyle structure to compare to, revert to
  // and save to (without a reference style pointer, we will use one compared
  // locally as a reference)
  ImGuiStyle& style = ImGui::GetStyle();
  static ImGuiStyle ref_saved_style;

  // Default to using internal storage as reference
  static bool init = true;
  if (init && ref == NULL) ref_saved_style = style;
  init = false;
  if (ref == NULL) ref = &ref_saved_style;

  ImGui::PushItemWidth(ImGui::GetWindowWidth() * 0.50f);

  if (ImGui::ShowStyleSelector("Colors##Selector")) ref_saved_style = style;
  ImGui::ShowFontSelector("Fonts##Selector");

  // Simplified Settings (expose floating-pointer border sizes as boolean
  // representing 0.0f or 1.0f)
  if (ImGui::SliderFloat("FrameRounding", &style.FrameRounding, 0.0f, 12.0f,
                         "%.0f"))
    style.GrabRounding = style.FrameRounding;  // Make GrabRounding always the
                                               // same value as FrameRounding
  {
    bool border = (style.WindowBorderSize > 0.0f);
    if (ImGui::Checkbox("WindowBorder", &border)) {
      style.WindowBorderSize = border ? 1.0f : 0.0f;
    }
  }
  ImGui::SameLine();
  {
    bool border = (style.FrameBorderSize > 0.0f);
    if (ImGui::Checkbox("FrameBorder", &border)) {
      style.FrameBorderSize = border ? 1.0f : 0.0f;
    }
  }
  ImGui::SameLine();
  {
    bool border = (style.PopupBorderSize > 0.0f);
    if (ImGui::Checkbox("PopupBorder", &border)) {
      style.PopupBorderSize = border ? 1.0f : 0.0f;
    }
  }

  // Save/Revert button
  if (ImGui::Button("Save Ref")) *ref = ref_saved_style = style;
  ImGui::SameLine();
  if (ImGui::Button("Revert Ref")) style = *ref;
  ImGui::SameLine();

  ImGui::Separator();

  if (ImGui::BeginTabBar("##tabs", ImGuiTabBarFlags_None)) {
    if (ImGui::BeginTabItem("Sizes")) {
      ImGui::SeparatorText("Main");
      ImGui::SliderFloat2("WindowPadding", (float*)&style.WindowPadding, 0.0f,
                          20.0f, "%.0f");
      ImGui::SliderFloat2("FramePadding", (float*)&style.FramePadding, 0.0f,
                          20.0f, "%.0f");
      ImGui::SliderFloat2("ItemSpacing", (float*)&style.ItemSpacing, 0.0f,
                          20.0f, "%.0f");
      ImGui::SliderFloat2("ItemInnerSpacing", (float*)&style.ItemInnerSpacing,
                          0.0f, 20.0f, "%.0f");
      ImGui::SliderFloat2("TouchExtraPadding", (float*)&style.TouchExtraPadding,
                          0.0f, 10.0f, "%.0f");
      ImGui::SliderFloat("IndentSpacing", &style.IndentSpacing, 0.0f, 30.0f,
                         "%.0f");
      ImGui::SliderFloat("ScrollbarSize", &style.ScrollbarSize, 1.0f, 20.0f,
                         "%.0f");
      ImGui::SliderFloat("GrabMinSize", &style.GrabMinSize, 1.0f, 20.0f,
                         "%.0f");

      ImGui::SeparatorText("Borders");
      ImGui::SliderFloat("WindowBorderSize", &style.WindowBorderSize, 0.0f,
                         1.0f, "%.0f");
      ImGui::SliderFloat("ChildBorderSize", &style.ChildBorderSize, 0.0f, 1.0f,
                         "%.0f");
      ImGui::SliderFloat("PopupBorderSize", &style.PopupBorderSize, 0.0f, 1.0f,
                         "%.0f");
      ImGui::SliderFloat("FrameBorderSize", &style.FrameBorderSize, 0.0f, 1.0f,
                         "%.0f");
      ImGui::SliderFloat("TabBorderSize", &style.TabBorderSize, 0.0f, 1.0f,
                         "%.0f");
      ImGui::SliderFloat("TabBarBorderSize", &style.TabBarBorderSize, 0.0f,
                         2.0f, "%.0f");

      ImGui::SeparatorText("Rounding");
      ImGui::SliderFloat("WindowRounding", &style.WindowRounding, 0.0f, 12.0f,
                         "%.0f");
      ImGui::SliderFloat("ChildRounding", &style.ChildRounding, 0.0f, 12.0f,
                         "%.0f");
      ImGui::SliderFloat("FrameRounding", &style.FrameRounding, 0.0f, 12.0f,
                         "%.0f");
      ImGui::SliderFloat("PopupRounding", &style.PopupRounding, 0.0f, 12.0f,
                         "%.0f");
      ImGui::SliderFloat("ScrollbarRounding", &style.ScrollbarRounding, 0.0f,
                         12.0f, "%.0f");
      ImGui::SliderFloat("GrabRounding", &style.GrabRounding, 0.0f, 12.0f,
                         "%.0f");
      ImGui::SliderFloat("TabRounding", &style.TabRounding, 0.0f, 12.0f,
                         "%.0f");

      ImGui::SeparatorText("Tables");
      ImGui::SliderFloat2("CellPadding", (float*)&style.CellPadding, 0.0f,
                          20.0f, "%.0f");
      ImGui::SliderAngle("TableAngledHeadersAngle",
                         &style.TableAngledHeadersAngle, -50.0f, +50.0f);

      ImGui::SeparatorText("Widgets");
      ImGui::SliderFloat2("WindowTitleAlign", (float*)&style.WindowTitleAlign,
                          0.0f, 1.0f, "%.2f");
      int window_menu_button_position = style.WindowMenuButtonPosition + 1;
      if (ImGui::Combo("WindowMenuButtonPosition",
                       (int*)&window_menu_button_position,
                       "None\0Left\0Right\0"))
        style.WindowMenuButtonPosition = window_menu_button_position - 1;
      ImGui::Combo("ColorButtonPosition", (int*)&style.ColorButtonPosition,
                   "Left\0Right\0");
      ImGui::SliderFloat2("ButtonTextAlign", (float*)&style.ButtonTextAlign,
                          0.0f, 1.0f, "%.2f");
      ImGui::SameLine();

      ImGui::SliderFloat2("SelectableTextAlign",
                          (float*)&style.SelectableTextAlign, 0.0f, 1.0f,
                          "%.2f");
      ImGui::SameLine();

      ImGui::SliderFloat("SeparatorTextBorderSize",
                         &style.SeparatorTextBorderSize, 0.0f, 10.0f, "%.0f");
      ImGui::SliderFloat2("SeparatorTextAlign",
                          (float*)&style.SeparatorTextAlign, 0.0f, 1.0f,
                          "%.2f");
      ImGui::SliderFloat2("SeparatorTextPadding",
                          (float*)&style.SeparatorTextPadding, 0.0f, 40.0f,
                          "%.0f");
      ImGui::SliderFloat("LogSliderDeadzone", &style.LogSliderDeadzone, 0.0f,
                         12.0f, "%.0f");

      ImGui::SeparatorText("Tooltips");
      for (int n = 0; n < 2; n++)
        if (ImGui::TreeNodeEx(n == 0 ? "HoverFlagsForTooltipMouse"
                                     : "HoverFlagsForTooltipNav")) {
          ImGuiHoveredFlags* p = (n == 0) ? &style.HoverFlagsForTooltipMouse
                                          : &style.HoverFlagsForTooltipNav;
          ImGui::CheckboxFlags("ImGuiHoveredFlags_DelayNone", p,
                               ImGuiHoveredFlags_DelayNone);
          ImGui::CheckboxFlags("ImGuiHoveredFlags_DelayShort", p,
                               ImGuiHoveredFlags_DelayShort);
          ImGui::CheckboxFlags("ImGuiHoveredFlags_DelayNormal", p,
                               ImGuiHoveredFlags_DelayNormal);
          ImGui::CheckboxFlags("ImGuiHoveredFlags_Stationary", p,
                               ImGuiHoveredFlags_Stationary);
          ImGui::CheckboxFlags("ImGuiHoveredFlags_NoSharedDelay", p,
                               ImGuiHoveredFlags_NoSharedDelay);
          ImGui::TreePop();
        }

      ImGui::SeparatorText("Misc");
      ImGui::SliderFloat2("DisplaySafeAreaPadding",
                          (float*)&style.DisplaySafeAreaPadding, 0.0f, 30.0f,
                          "%.0f");
      ImGui::SameLine();

      ImGui::EndTabItem();
    }

    if (ImGui::BeginTabItem("Colors")) {
      static int output_dest = 0;
      static bool output_only_modified = true;
      if (ImGui::Button("Export")) {
        if (output_dest == 0)
          ImGui::LogToClipboard();
        else
          ImGui::LogToTTY();
        ImGui::LogText("ImVec4* colors = ImGui::GetStyle().Colors;" IM_NEWLINE);
        for (int i = 0; i < ImGuiCol_COUNT; i++) {
          const ImVec4& col = style.Colors[i];
          const char* name = ImGui::GetStyleColorName(i);
          if (!output_only_modified ||
              memcmp(&col, &ref->Colors[i], sizeof(ImVec4)) != 0)
            ImGui::LogText(
                "colors[ImGuiCol_%s]%*s= ImVec4(%.2ff, %.2ff, %.2ff, "
                "%.2ff);" IM_NEWLINE,
                name, 23 - (int)strlen(name), "", col.x, col.y, col.z, col.w);
        }
        ImGui::LogFinish();
      }
      ImGui::SameLine();
      ImGui::SetNextItemWidth(120);
      ImGui::Combo("##output_type", &output_dest, "To Clipboard\0To TTY\0");
      ImGui::SameLine();
      ImGui::Checkbox("Only Modified Colors", &output_only_modified);

      static ImGuiTextFilter filter;
      filter.Draw("Filter colors", ImGui::GetFontSize() * 16);

      static ImGuiColorEditFlags alpha_flags = 0;
      if (ImGui::RadioButton("Opaque",
                             alpha_flags == ImGuiColorEditFlags_None)) {
        alpha_flags = ImGuiColorEditFlags_None;
      }
      ImGui::SameLine();
      if (ImGui::RadioButton("Alpha",
                             alpha_flags == ImGuiColorEditFlags_AlphaPreview)) {
        alpha_flags = ImGuiColorEditFlags_AlphaPreview;
      }
      ImGui::SameLine();
      if (ImGui::RadioButton(
              "Both", alpha_flags == ImGuiColorEditFlags_AlphaPreviewHalf)) {
        alpha_flags = ImGuiColorEditFlags_AlphaPreviewHalf;
      }
      ImGui::SameLine();

      ImGui::SetNextWindowSizeConstraints(
          ImVec2(0.0f, ImGui::GetTextLineHeightWithSpacing() * 10),
          ImVec2(FLT_MAX, FLT_MAX));
      ImGui::BeginChild("##colors", ImVec2(0, 0), ImGuiChildFlags_Border,
                        ImGuiWindowFlags_AlwaysVerticalScrollbar |
                            ImGuiWindowFlags_AlwaysHorizontalScrollbar |
                            ImGuiWindowFlags_NavFlattened);
      ImGui::PushItemWidth(ImGui::GetFontSize() * -12);
      for (int i = 0; i < ImGuiCol_COUNT; i++) {
        const char* name = ImGui::GetStyleColorName(i);
        if (!filter.PassFilter(name)) continue;
        ImGui::PushID(i);
        ImGui::ColorEdit4("##color", (float*)&style.Colors[i],
                          ImGuiColorEditFlags_AlphaBar | alpha_flags);
        if (memcmp(&style.Colors[i], &ref->Colors[i], sizeof(ImVec4)) != 0) {
          // Tips: in a real user application, you may want to merge and use
          // an icon font into the main font, so instead of "Save"/"Revert"
          // you'd use icons! Read the FAQ and docs/FONTS.md about using icon
          // fonts. It's really easy and super convenient!
          ImGui::SameLine(0.0f, style.ItemInnerSpacing.x);
          if (ImGui::Button("Save")) {
            ref->Colors[i] = style.Colors[i];
          }
          ImGui::SameLine(0.0f, style.ItemInnerSpacing.x);
          if (ImGui::Button("Revert")) {
            style.Colors[i] = ref->Colors[i];
          }
        }
        ImGui::SameLine(0.0f, style.ItemInnerSpacing.x);
        ImGui::TextUnformatted(name);
        ImGui::PopID();
      }
      ImGui::PopItemWidth();
      ImGui::EndChild();

      ImGui::EndTabItem();
    }

    if (ImGui::BeginTabItem("Fonts")) {
      ImGuiIO& io = ImGui::GetIO();
      ImFontAtlas* atlas = io.Fonts;
      ImGui::ShowFontAtlas(atlas);

      // Post-baking font scaling. Note that this is NOT the nice way of
      // scaling fonts, read below. (we enforce hard clamping manually as by
      // default DragFloat/SliderFloat allows CTRL+Click text to get out of
      // bounds).
      const float MIN_SCALE = 0.3f;
      const float MAX_SCALE = 2.0f;

      static float window_scale = 1.0f;
      ImGui::PushItemWidth(ImGui::GetFontSize() * 8);
      if (ImGui::DragFloat(
              "window scale", &window_scale, 0.005f, MIN_SCALE, MAX_SCALE,
              "%.2f",
              ImGuiSliderFlags_AlwaysClamp))  // Scale only this window
        ImGui::SetWindowFontScale(window_scale);
      ImGui::DragFloat("global scale", &io.FontGlobalScale, 0.005f, MIN_SCALE,
                       MAX_SCALE, "%.2f",
                       ImGuiSliderFlags_AlwaysClamp);  // Scale everything
      ImGui::PopItemWidth();

      ImGui::EndTabItem();
    }

    if (ImGui::BeginTabItem("Rendering")) {
      ImGui::Checkbox("Anti-aliased lines", &style.AntiAliasedLines);
      ImGui::SameLine();

      ImGui::Checkbox("Anti-aliased lines use texture",
                      &style.AntiAliasedLinesUseTex);
      ImGui::SameLine();

      ImGui::Checkbox("Anti-aliased fill", &style.AntiAliasedFill);
      ImGui::PushItemWidth(ImGui::GetFontSize() * 8);
      ImGui::DragFloat("Curve Tessellation Tolerance",
                       &style.CurveTessellationTol, 0.02f, 0.10f, 10.0f,
                       "%.2f");
      if (style.CurveTessellationTol < 0.10f)
        style.CurveTessellationTol = 0.10f;

      // When editing the "Circle Segment Max Error" value, draw a preview of
      // its effect on auto-tessellated circles.
      ImGui::DragFloat("Circle Tessellation Max Error",
                       &style.CircleTessellationMaxError, 0.005f, 0.10f, 5.0f,
                       "%.2f", ImGuiSliderFlags_AlwaysClamp);
      const bool show_samples = ImGui::IsItemActive();
      if (show_samples) ImGui::SetNextWindowPos(ImGui::GetCursorScreenPos());
      if (show_samples && ImGui::BeginTooltip()) {
        ImGui::TextUnformatted("(R = radius, N = number of segments)");
        ImGui::Spacing();
        ImDrawList* draw_list = ImGui::GetWindowDrawList();
        const float min_widget_width = ImGui::CalcTextSize("N: MMM\nR: MMM").x;
        for (int n = 0; n < 8; n++) {
          const float RAD_MIN = 5.0f;
          const float RAD_MAX = 70.0f;
          const float rad =
              RAD_MIN + (RAD_MAX - RAD_MIN) * (float)n / (8.0f - 1.0f);

          ImGui::BeginGroup();

          ImGui::Text("R: %.f\nN: %d", rad,
                      draw_list->_CalcCircleAutoSegmentCount(rad));

          const float canvas_width = std::max(min_widget_width, rad * 2.0f);
          const float offset_x = floorf(canvas_width * 0.5f);
          const float offset_y = floorf(RAD_MAX);

          const ImVec2 p1 = ImGui::GetCursorScreenPos();
          draw_list->AddCircle(ImVec2(p1.x + offset_x, p1.y + offset_y), rad,
                               ImGui::GetColorU32(ImGuiCol_Text));
          ImGui::Dummy(ImVec2(canvas_width, RAD_MAX * 2));

          /*
          const ImVec2 p2 = ImGui::GetCursorScreenPos();
          draw_list->AddCircleFilled(ImVec2(p2.x + offset_x, p2.y + offset_y),
          rad, ImGui::GetColorU32(ImGuiCol_Text));
          ImGui::Dummy(ImVec2(canvas_width, RAD_MAX * 2));
          */

          ImGui::EndGroup();
          ImGui::SameLine();
        }
        ImGui::EndTooltip();
      }
      ImGui::SameLine();

      ImGui::DragFloat("Global Alpha", &style.Alpha, 0.005f, 0.20f, 1.0f,
                       "%.2f");  // Not exposing zero here so user doesn't
                                 // "lose" the UI (zero alpha clips all
                                 // widgets). But application code could have a
                                 // toggle to switch between zero and non-zero.
      ImGui::DragFloat("Disabled Alpha", &style.DisabledAlpha, 0.005f, 0.0f,
                       1.0f, "%.2f");
      ImGui::SameLine();

      ImGui::PopItemWidth();

      ImGui::EndTabItem();
    }

    ImGui::EndTabBar();
  }

  ImGui::PopItemWidth();
}

void TextWithSeparators(const absl::string_view& text) {
  ImGui::Separator();
  ImGui::Text("%s", text.data());
  ImGui::Separator();
}

void ColorsYaze() {
  ImGuiStyle* style = &ImGui::GetStyle();
  ImVec4* colors = style->Colors;

  style->WindowPadding = ImVec2(10.f, 10.f);
  style->FramePadding = ImVec2(10.f, 2.f);
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
}  // namespace app
}  // namespace yaze