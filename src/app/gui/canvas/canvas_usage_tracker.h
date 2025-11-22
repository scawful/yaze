#ifndef YAZE_APP_GUI_CANVAS_CANVAS_USAGE_TRACKER_H
#define YAZE_APP_GUI_CANVAS_CANVAS_USAGE_TRACKER_H

#include <chrono>
#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

#include "imgui/imgui.h"

namespace yaze {
namespace gui {

/**
 * @brief Canvas usage patterns and tracking
 */
enum class CanvasUsage {
  kTilePainting,        // Drawing tiles on canvas
  kTileSelecting,       // Selecting tiles from canvas
  kSelectRectangle,     // Rectangle selection mode
  kColorPainting,       // Color painting mode
  kBitmapEditing,       // Direct bitmap editing
  kPaletteEditing,      // Palette editing mode
  kBppConversion,       // BPP format conversion
  kPerformanceMode,     // Performance monitoring mode
  kEntityManipulation,  // Generic entity manipulation
                        // (insertion/editing/deletion)
  kUnknown              // Unknown or mixed usage
};

/**
 * @brief Canvas interaction types
 */
enum class CanvasInteraction {
  kMouseClick,
  kMouseDrag,
  kMouseRelease,
  kKeyboardInput,
  kContextMenu,
  kModalOpen,
  kModalClose,
  kToolChange,
  kModeChange
};

/**
 * @brief Canvas usage statistics
 */
struct CanvasUsageStats {
  CanvasUsage usage_mode = CanvasUsage::kUnknown;
  std::chrono::steady_clock::time_point session_start;
  std::chrono::milliseconds total_time{0};
  std::chrono::milliseconds active_time{0};
  std::chrono::milliseconds idle_time{0};

  // Interaction counts
  int mouse_clicks = 0;
  int mouse_drags = 0;
  int context_menu_opens = 0;
  int modal_opens = 0;
  int tool_changes = 0;
  int mode_changes = 0;

  // Performance metrics
  double average_operation_time_ms = 0.0;
  double max_operation_time_ms = 0.0;
  int total_operations = 0;

  // Canvas state
  ImVec2 canvas_size = ImVec2(0, 0);
  ImVec2 content_size = ImVec2(0, 0);
  float global_scale = 1.0F;
  float grid_step = 32.0F;
  bool enable_grid = true;
  bool enable_hex_labels = false;
  bool enable_custom_labels = false;

  void Reset() {
    usage_mode = CanvasUsage::kUnknown;
    session_start = std::chrono::steady_clock::now();
    total_time = std::chrono::milliseconds{0};
    active_time = std::chrono::milliseconds{0};
    idle_time = std::chrono::milliseconds{0};
    mouse_clicks = 0;
    mouse_drags = 0;
    context_menu_opens = 0;
    modal_opens = 0;
    tool_changes = 0;
    mode_changes = 0;
    average_operation_time_ms = 0.0;
    max_operation_time_ms = 0.0;
    total_operations = 0;
  }
};

/**
 * @brief Canvas usage tracking and analysis system
 */
class CanvasUsageTracker {
 public:
  CanvasUsageTracker() = default;

  /**
   * @brief Initialize the usage tracker
   */
  void Initialize(const std::string& canvas_id);

  /**
   * @brief Set the current usage mode
   */
  void SetUsageMode(CanvasUsage usage);

  /**
   * @brief Record an interaction
   */
  void RecordInteraction(CanvasInteraction interaction,
                         const std::string& details = "");

  /**
   * @brief Record operation timing
   */
  void RecordOperation(const std::string& operation_name, double time_ms);

  /**
   * @brief Update canvas state
   */
  void UpdateCanvasState(const ImVec2& canvas_size, const ImVec2& content_size,
                         float global_scale, float grid_step, bool enable_grid,
                         bool enable_hex_labels, bool enable_custom_labels);

  /**
   * @brief Get current usage statistics
   */
  const CanvasUsageStats& GetCurrentStats() const { return current_stats_; }

  /**
   * @brief Get usage history
   */
  const std::vector<CanvasUsageStats>& GetUsageHistory() const {
    return usage_history_;
  }

  /**
   * @brief Get usage mode name
   */
  std::string GetUsageModeName(CanvasUsage usage) const;

  /**
   * @brief Get usage mode color for UI
   */
  ImVec4 GetUsageModeColor(CanvasUsage usage) const;

  /**
   * @brief Get usage recommendations
   */
  std::vector<std::string> GetUsageRecommendations() const;

  /**
   * @brief Export usage report
   */
  std::string ExportUsageReport() const;

  /**
   * @brief Clear usage history
   */
  void ClearHistory();

  /**
   * @brief Start session
   */
  void StartSession();

  /**
   * @brief End session
   */
  void EndSession();

 private:
  std::string canvas_id_;
  CanvasUsageStats current_stats_;
  std::vector<CanvasUsageStats> usage_history_;
  std::chrono::steady_clock::time_point last_activity_;
  std::chrono::steady_clock::time_point session_start_;

  // Interaction history
  std::vector<std::pair<CanvasInteraction, std::string>> interaction_history_;
  std::unordered_map<std::string, std::vector<double>> operation_times_;

  // Helper methods
  void UpdateActiveTime();
  void UpdateIdleTime();
  void SaveCurrentStats();
  double CalculateAverageOperationTime(const std::string& operation_name) const;
  std::string FormatDuration(const std::chrono::milliseconds& duration) const;
};

/**
 * @brief Global canvas usage tracker manager
 */
class CanvasUsageManager {
 public:
  static CanvasUsageManager& Get();

  /**
   * @brief Register a canvas tracker
   */
  void RegisterTracker(const std::string& canvas_id,
                       std::shared_ptr<CanvasUsageTracker> tracker);

  /**
   * @brief Get tracker for canvas
   */
  std::shared_ptr<CanvasUsageTracker> GetTracker(const std::string& canvas_id);

  /**
   * @brief Get all trackers
   */
  const std::unordered_map<std::string, std::shared_ptr<CanvasUsageTracker>>&
  GetAllTrackers() const {
    return trackers_;
  }

  /**
   * @brief Get global usage statistics
   */
  CanvasUsageStats GetGlobalStats() const;

  /**
   * @brief Export global usage report
   */
  std::string ExportGlobalReport() const;

  /**
   * @brief Clear all trackers
   */
  void ClearAllTrackers();

 private:
  CanvasUsageManager() = default;
  ~CanvasUsageManager() = default;

  std::unordered_map<std::string, std::shared_ptr<CanvasUsageTracker>>
      trackers_;
};

}  // namespace gui
}  // namespace yaze

#endif  // YAZE_APP_GUI_CANVAS_CANVAS_USAGE_TRACKER_H
