#include "app/gui/plots/implot_support.h"

#include "app/gui/core/color.h"
#include "app/gui/core/layout_helpers.h"
#include "app/gui/core/theme_manager.h"

namespace yaze {
namespace gui {
namespace plotting {
namespace {

ImVec4 WithAlpha(const ImVec4& color, float alpha) {
  return ImVec4(color.x, color.y, color.z, color.w * alpha);
}

ImVec2 ScaledSpacing(float multiplier) {
  const float spacing = LayoutHelpers::GetStandardSpacing();
  return ImVec2(spacing * multiplier, spacing * multiplier);
}

ImPlotStyle BuildBaseStyle(const Theme& theme,
                           const PlotStyleConfig& config) {
  ImPlotStyle style;
  style.LineWeight = config.line_weight;
  style.MarkerSize = config.marker_size;
  style.MarkerWeight = config.marker_weight;
  style.FillAlpha = config.fill_alpha;
  style.DigitalBitHeight = ImGui::GetFontSize() * 0.85f;
  style.DigitalBitGap = ImGui::GetStyle().ItemSpacing.y * 0.5f;
  style.PlotBorderSize = 1.0f;
  style.MinorAlpha = config.grid_alpha;
  style.MajorTickLen = ImVec2(6.0f, 6.0f);
  style.MinorTickLen = ImVec2(3.0f, 3.0f);
  style.MajorTickSize = ImVec2(1.0f, 1.0f);
  style.MinorTickSize = ImVec2(1.0f, 1.0f);
  style.MajorGridSize = ImVec2(1.0f, 1.0f);
  style.MinorGridSize = ImVec2(1.0f, 1.0f);
  style.PlotPadding = ScaledSpacing(1.5f);
  style.LabelPadding = ScaledSpacing(0.75f);
  style.LegendPadding = ScaledSpacing(1.0f);
  style.LegendInnerPadding = ScaledSpacing(0.5f);
  style.LegendSpacing = ImVec2(LayoutHelpers::GetStandardSpacing(),
                               LayoutHelpers::GetStandardSpacing() * 0.35f);
  style.MousePosPadding = ScaledSpacing(1.0f);
  style.AnnotationPadding = ScaledSpacing(0.35f);
  style.FitPadding = ImVec2(0.05f, 0.05f);
  style.PlotDefaultSize = ImVec2(420, 280);
  style.PlotMinSize = ImVec2(220, 160);
  style.UseISO8601 = config.use_iso_8601;
  style.Use24HourClock = config.use_24h_clock;
  style.UseLocalTime = true;

  style.Colors[ImPlotCol_Line] =
      WithAlpha(ConvertColorToImVec4(theme.plot_lines), 1.0f);
  style.Colors[ImPlotCol_Fill] =
      WithAlpha(ConvertColorToImVec4(theme.plot_histogram), config.fill_alpha);
  style.Colors[ImPlotCol_MarkerOutline] =
      WithAlpha(ConvertColorToImVec4(theme.text_primary), 1.0f);
  style.Colors[ImPlotCol_MarkerFill] =
      WithAlpha(ConvertColorToImVec4(theme.accent), 0.9f);
  style.Colors[ImPlotCol_ErrorBar] =
      WithAlpha(ConvertColorToImVec4(theme.text_secondary), 0.85f);
  style.Colors[ImPlotCol_FrameBg] =
      WithAlpha(ConvertColorToImVec4(theme.surface), config.background_alpha);
  style.Colors[ImPlotCol_PlotBg] =
      WithAlpha(ConvertColorToImVec4(theme.child_bg), config.background_alpha);
  style.Colors[ImPlotCol_PlotBorder] =
      WithAlpha(ConvertColorToImVec4(theme.border), 1.0f);
  style.Colors[ImPlotCol_LegendBg] =
      WithAlpha(ConvertColorToImVec4(theme.popup_bg), config.background_alpha);
  style.Colors[ImPlotCol_LegendBorder] =
      WithAlpha(ConvertColorToImVec4(theme.border), 1.0f);
  style.Colors[ImPlotCol_LegendText] =
      WithAlpha(ConvertColorToImVec4(theme.text_primary), 1.0f);
  style.Colors[ImPlotCol_TitleText] =
      WithAlpha(ConvertColorToImVec4(theme.text_primary), 1.0f);
  style.Colors[ImPlotCol_InlayText] =
      WithAlpha(ConvertColorToImVec4(theme.text_secondary), 1.0f);
  style.Colors[ImPlotCol_AxisText] =
      WithAlpha(ConvertColorToImVec4(theme.text_secondary), 0.95f);
  style.Colors[ImPlotCol_AxisGrid] =
      WithAlpha(ConvertColorToImVec4(theme.editor_grid), config.grid_alpha);
  style.Colors[ImPlotCol_AxisTick] =
      WithAlpha(ConvertColorToImVec4(theme.text_secondary), 0.6f);
  style.Colors[ImPlotCol_AxisBg] =
      WithAlpha(ConvertColorToImVec4(theme.child_bg), config.background_alpha);
  style.Colors[ImPlotCol_AxisBgHovered] =
      WithAlpha(ConvertColorToImVec4(theme.hover_highlight), 0.65f);
  style.Colors[ImPlotCol_AxisBgActive] =
      WithAlpha(ConvertColorToImVec4(theme.active_selection), 0.65f);
  style.Colors[ImPlotCol_Selection] =
      WithAlpha(ConvertColorToImVec4(theme.accent), 0.25f);
  style.Colors[ImPlotCol_Crosshairs] =
      WithAlpha(ConvertColorToImVec4(theme.editor_cursor), 0.75f);

  return style;
}

}  // namespace

void EnsureImPlotContext() {
  if (ImPlot::GetCurrentContext() == nullptr) {
    ImPlot::CreateContext();
  }
}

ImPlotStyle BuildStyleFromTheme(const Theme& theme,
                                const PlotStyleConfig& config) {
  EnsureImPlotContext();
  return BuildBaseStyle(theme, config);
}

PlotStyleScope::PlotStyleScope(const Theme& theme,
                               const PlotStyleConfig& config)
    : previous_style_() {
  EnsureImPlotContext();
  previous_style_ = ImPlot::GetStyle();
  ImPlot::GetStyle() = BuildBaseStyle(theme, config);
}

PlotStyleScope::~PlotStyleScope() { ImPlot::GetStyle() = previous_style_; }

PlotGuard::PlotGuard(const PlotConfig& config) {
  EnsureImPlotContext();
  should_end_ =
      ImPlot::BeginPlot(config.id.c_str(), config.size, config.flags);
  if (should_end_) {
    ImPlot::SetupAxes(config.x_label, config.y_label, config.x_axis_flags,
                      config.y_axis_flags);
  }
}

PlotGuard::~PlotGuard() {
  if (should_end_) {
    ImPlot::EndPlot();
  }
}

}  // namespace plotting
}  // namespace gui
}  // namespace yaze
