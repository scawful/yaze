#ifndef YAZE_APP_GUI_WIDGET_MEASUREMENT_H
#define YAZE_APP_GUI_WIDGET_MEASUREMENT_H

#include <string>
#include <unordered_map>
#include <vector>

#include "absl/strings/str_format.h"
#include "imgui/imgui.h"

namespace yaze {
namespace gui {

/**
 * @class WidgetMeasurement
 * @brief Tracks widget dimensions for debugging and test automation
 * 
 * Integrates with ImGui Test Engine to provide accurate measurements
 * of UI elements, helping prevent layout issues and enabling automated
 * testing of widget sizes and positions.
 */
struct WidgetMetrics {
  ImVec2 size;           // Width and height
  ImVec2 position;       // Screen position
  ImVec2 content_size;   // Available content region
  ImVec2 rect_min;       // Bounding box min
  ImVec2 rect_max;       // Bounding box max
  float cursor_pos_x;    // Cursor X after rendering
  std::string widget_id; // Widget identifier
  std::string type;      // Widget type (button, input, combo, etc.)
  
  std::string ToString() const {
    return absl::StrFormat(
        "Widget '%s' (%s): size=(%.1f,%.1f) pos=(%.1f,%.1f) content=(%.1f,%.1f) cursor_x=%.1f",
        widget_id, type, size.x, size.y, position.x, position.y,
        content_size.x, content_size.y, cursor_pos_x);
  }
};

class WidgetMeasurement {
 public:
  static WidgetMeasurement& Instance() {
    static WidgetMeasurement instance;
    return instance;
  }

  /**
   * @brief Measure the last rendered ImGui item
   * @param widget_id Unique identifier for this widget
   * @param type Widget type (button, input, etc.)
   * @return WidgetMetrics containing all measurements
   */
  WidgetMetrics MeasureLastItem(const std::string& widget_id,
                                const std::string& type = "unknown");

  /**
   * @brief Begin measuring a toolbar section
   */
  void BeginToolbarMeasurement(const std::string& toolbar_id);

  /**
   * @brief End measuring a toolbar section and store total width
   */
  void EndToolbarMeasurement();

  /**
   * @brief Get total measured width of a toolbar
   */
  float GetToolbarWidth(const std::string& toolbar_id) const;

  /**
   * @brief Check if toolbar would overflow given window width
   */
  bool WouldToolbarOverflow(const std::string& toolbar_id,
                            float available_width) const;

  /**
   * @brief Get all measurements for a specific toolbar
   */
  const std::vector<WidgetMetrics>& GetToolbarMetrics(
      const std::string& toolbar_id) const;

  /**
   * @brief Clear all measurements (call once per frame)
   */
  void ClearFrame();

  /**
   * @brief Export measurements for test automation
   */
  std::string ExportMetricsJSON() const;

  /**
   * @brief Enable/disable measurement (performance option)
   */
  void SetEnabled(bool enabled) { enabled_ = enabled; }
  bool IsEnabled() const { return enabled_; }

 private:
  WidgetMeasurement() = default;

  bool enabled_ = true;
  std::string current_toolbar_id_;
  float current_toolbar_width_ = 0.0f;
  float current_toolbar_start_x_ = 0.0f;

  // Store measurements per toolbar
  std::unordered_map<std::string, std::vector<WidgetMetrics>> toolbar_metrics_;
  std::unordered_map<std::string, float> toolbar_widths_;

  // All measurements from current frame
  std::vector<WidgetMetrics> frame_metrics_;
};

}  // namespace gui
}  // namespace yaze

#endif  // YAZE_APP_GUI_WIDGET_MEASUREMENT_H

