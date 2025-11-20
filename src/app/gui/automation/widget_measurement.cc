#include "app/gui/automation/widget_measurement.h"

#include "absl/strings/str_format.h"
#include "imgui/imgui.h"
#include "imgui/imgui_internal.h"

namespace yaze {
namespace gui {

WidgetMetrics WidgetMeasurement::MeasureLastItem(const std::string& widget_id,
                                                 const std::string& type) {
  if (!enabled_) {
    return WidgetMetrics{};
  }

  WidgetMetrics metrics;
  metrics.widget_id = widget_id;
  metrics.type = type;

  // Get item rect (bounding box)
  metrics.rect_min = ImGui::GetItemRectMin();
  metrics.rect_max = ImGui::GetItemRectMax();

  // Calculate size
  metrics.size = ImVec2(metrics.rect_max.x - metrics.rect_min.x,
                        metrics.rect_max.y - metrics.rect_min.y);

  // Store position
  metrics.position = metrics.rect_min;

  // Get content region
  metrics.content_size = ImGui::GetContentRegionAvail();

  // Get cursor position after item
  metrics.cursor_pos_x = ImGui::GetCursorPosX();

  // Store in current toolbar if active
  if (!current_toolbar_id_.empty()) {
    toolbar_metrics_[current_toolbar_id_].push_back(metrics);
  }

  // Store in frame metrics
  frame_metrics_.push_back(metrics);

  return metrics;
}

void WidgetMeasurement::BeginToolbarMeasurement(const std::string& toolbar_id) {
  current_toolbar_id_ = toolbar_id;
  current_toolbar_start_x_ = ImGui::GetCursorPosX();
  current_toolbar_width_ = 0.0f;

  // Clear previous measurements for this toolbar
  toolbar_metrics_[toolbar_id].clear();
}

void WidgetMeasurement::EndToolbarMeasurement() {
  if (current_toolbar_id_.empty()) return;

  // Calculate total width from cursor movement
  float end_x = ImGui::GetCursorPosX();
  current_toolbar_width_ = end_x - current_toolbar_start_x_;

  // Store the width
  toolbar_widths_[current_toolbar_id_] = current_toolbar_width_;

  // Reset state
  current_toolbar_id_.clear();
  current_toolbar_width_ = 0.0f;
  current_toolbar_start_x_ = 0.0f;
}

float WidgetMeasurement::GetToolbarWidth(const std::string& toolbar_id) const {
  auto it = toolbar_widths_.find(toolbar_id);
  if (it != toolbar_widths_.end()) {
    return it->second;
  }
  return 0.0f;
}

bool WidgetMeasurement::WouldToolbarOverflow(const std::string& toolbar_id,
                                             float available_width) const {
  float toolbar_width = GetToolbarWidth(toolbar_id);
  return toolbar_width > available_width;
}

const std::vector<WidgetMetrics>& WidgetMeasurement::GetToolbarMetrics(
    const std::string& toolbar_id) const {
  static const std::vector<WidgetMetrics> empty;
  auto it = toolbar_metrics_.find(toolbar_id);
  if (it != toolbar_metrics_.end()) {
    return it->second;
  }
  return empty;
}

void WidgetMeasurement::ClearFrame() {
  frame_metrics_.clear();
  // Don't clear toolbar metrics - they persist across frames for debugging
}

std::string WidgetMeasurement::ExportMetricsJSON() const {
  std::string json = "{\n  \"toolbars\": {\n";

  bool first_toolbar = true;
  for (const auto& [toolbar_id, metrics] : toolbar_metrics_) {
    if (!first_toolbar) json += ",\n";
    first_toolbar = false;

    json += absl::StrFormat("    \"%s\": {\n", toolbar_id);
    json += absl::StrFormat("      \"total_width\": %.1f,\n",
                            GetToolbarWidth(toolbar_id));
    json += "      \"widgets\": [\n";

    bool first_widget = true;
    for (const auto& metric : metrics) {
      if (!first_widget) json += ",\n";
      first_widget = false;

      json += "        {\n";
      json += absl::StrFormat("          \"id\": \"%s\",\n", metric.widget_id);
      json += absl::StrFormat("          \"type\": \"%s\",\n", metric.type);
      json += absl::StrFormat("          \"width\": %.1f,\n", metric.size.x);
      json += absl::StrFormat("          \"height\": %.1f,\n", metric.size.y);
      json += absl::StrFormat("          \"x\": %.1f,\n", metric.position.x);
      json += absl::StrFormat("          \"y\": %.1f\n", metric.position.y);
      json += "        }";
    }

    json += "\n      ]\n";
    json += "    }";
  }

  json += "\n  }\n}\n";
  return json;
}

}  // namespace gui
}  // namespace yaze
