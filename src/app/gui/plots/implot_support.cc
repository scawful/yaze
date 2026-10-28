#include "app/gui/plots/implot_support.h"

#include <vector>

#include "absl/strings/str_format.h"
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

bool IsCloseColor(const ImVec4& a, const ImVec4& b) {
  const float dr = a.x - b.x;
  const float dg = a.y - b.y;
  const float db = a.z - b.z;
  return dr * dr + dg * dg + db * db < 0.15f * 0.15f;
}

// Series 0 keeps the theme's plot_lines color; later series take ImPlot's
// Deep palette so multi-series plots stay distinguishable. ImPlot keeps
// colormaps for the life of the context and asserts on a reused name, and
// the list depends only on plot_lines, so the name is keyed on that color.
ImPlotColormap ThemeColormap(const Theme& theme) {
  const ImVec4 primary =
      WithAlpha(ConvertColorToImVec4(theme.plot_lines), 1.0f);
  const std::string name = absl::StrFormat(
      "yaze_theme_%08X", ImGui::ColorConvertFloat4ToU32(primary));
  const ImPlotColormap existing = ImPlot::GetColormapIndex(name.c_str());
  if (existing != -1) {
    return existing;
  }

  std::vector<ImVec4> colors = {primary};
  const int deep_size = ImPlot::GetColormapSize(ImPlotColormap_Deep);
  for (int i = 0; i < deep_size; ++i) {
    const ImVec4 color = ImPlot::GetColormapColor(i, ImPlotColormap_Deep);
    if (!IsCloseColor(color, primary)) {
      colors.push_back(color);
    }
  }
  return ImPlot::AddColormap(name.c_str(), colors.data(),
                             static_cast<int>(colors.size()), true);
}

ImVec2 ScaledSpacing(float multiplier) {
  const float spacing = LayoutHelpers::GetStandardSpacing();
  return ImVec2(spacing * multiplier, spacing * multiplier);
}

ImPlotStyle BuildBaseStyle(const Theme& theme, const PlotStyleConfig& config) {
  ImPlotStyle style;
  style.DigitalSpacing = ImGui::GetStyle().ItemSpacing.y * 0.5f;
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

  style.Colormap = ThemeColormap(theme);
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

ImPlotSpec BuildBaseItemSpec(const Theme& /*theme*/,
                             const PlotStyleConfig& config) {
  // Line, fill and marker colors stay IMPLOT_AUTO_COL so each series takes the
  // next entry of the theme colormap. Forcing them made every series in a plot
  // the same color. Items that need a fixed color copy this spec and set it.
  ImPlotSpec spec;
  spec.LineWeight = config.line_weight;
  spec.FillAlpha = config.fill_alpha;
  spec.MarkerSize = config.marker_size;
  spec.Size = ImGui::GetFontSize() * 0.85f;  // digital bar height
  return spec;
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

ImPlotSpec BuildItemSpecFromTheme(const Theme& theme,
                                  const PlotStyleConfig& config) {
  return BuildBaseItemSpec(theme, config);
}

PlotStyleScope::PlotStyleScope(const Theme& theme,
                               const PlotStyleConfig& config)
    : previous_style_(), item_spec_(BuildBaseItemSpec(theme, config)) {
  EnsureImPlotContext();
  previous_style_ = ImPlot::GetStyle();
  ImPlot::GetStyle() = BuildBaseStyle(theme, config);
}

PlotStyleScope::~PlotStyleScope() {
  ImPlot::GetStyle() = previous_style_;
}

PlotGuard::PlotGuard(const PlotConfig& config) {
  EnsureImPlotContext();
  should_end_ = ImPlot::BeginPlot(config.id.c_str(), config.size, config.flags);
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
