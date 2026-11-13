#ifndef YAZE_APP_GUI_PLOTS_IMPLOT_SUPPORT_H
#define YAZE_APP_GUI_PLOTS_IMPLOT_SUPPORT_H

#include <string>

#include "app/gui/core/theme_manager.h"
#include "imgui/imgui.h"
#include "implot.h"

namespace yaze {
namespace gui {
namespace plotting {

struct PlotStyleConfig {
  float line_weight = 2.0f;  // also the marker edge weight in ImPlot 1.x
  float marker_size = 4.0f;
  float fill_alpha = 0.35f;
  float grid_alpha = 0.35f;
  float background_alpha = 0.9f;
  bool use_iso_8601 = true;
  bool use_24h_clock = true;
};

struct PlotConfig {
  std::string id;
  const char* x_label = nullptr;
  const char* y_label = nullptr;
  ImVec2 size = ImVec2(0, 0);
  ImPlotFlags flags = ImPlotFlags_NoLegend;
  ImPlotAxisFlags x_axis_flags = ImPlotAxisFlags_AutoFit;
  ImPlotAxisFlags y_axis_flags = ImPlotAxisFlags_AutoFit;
};

void EnsureImPlotContext();
ImPlotStyle BuildStyleFromTheme(const Theme& theme,
                                const PlotStyleConfig& config = {});
// ImPlot 1.x styles plot items only through the ImPlotSpec passed to each
// PlotX call; ImPlotStyle no longer carries line, fill, or marker defaults.
ImPlotSpec BuildItemSpecFromTheme(const Theme& theme,
                                  const PlotStyleConfig& config = {});

// Applies the themed ImPlotStyle for its lifetime. Pass item_spec() to each
// PlotX call inside the scope so items match the theme too.
class PlotStyleScope {
 public:
  PlotStyleScope(const Theme& theme, const PlotStyleConfig& config = {});
  ~PlotStyleScope();

  const ImPlotSpec& item_spec() const { return item_spec_; }

 private:
  ImPlotStyle previous_style_{};
  ImPlotSpec item_spec_{};
};

class PlotGuard {
 public:
  explicit PlotGuard(const PlotConfig& config);
  ~PlotGuard();

  bool IsOpen() const { return should_end_; }
  explicit operator bool() const { return IsOpen(); }

 private:
  bool should_end_ = false;
};

}  // namespace plotting
}  // namespace gui
}  // namespace yaze

#endif  // YAZE_APP_GUI_PLOTS_IMPLOT_SUPPORT_H
