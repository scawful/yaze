#include "canvas_usage_tracker.h"

#include <algorithm>
#include <sstream>
#include <iomanip>
#include <chrono>

#include "util/log.h"

namespace yaze {
namespace gui {
namespace canvas {

void CanvasUsageTracker::Initialize(const std::string& canvas_id) {
  canvas_id_ = canvas_id;
  current_stats_.Reset();
  current_stats_.session_start = std::chrono::steady_clock::now();
  last_activity_ = current_stats_.session_start;
  session_start_ = current_stats_.session_start;
}

void CanvasUsageTracker::SetUsageMode(CanvasUsage usage) {
  if (current_stats_.usage_mode != usage) {
    // Save current stats before changing mode
    SaveCurrentStats();
    
    // Update usage mode
    current_stats_.usage_mode = usage;
    current_stats_.mode_changes++;
    
    // Record mode change interaction
    RecordInteraction(CanvasInteraction::kModeChange, GetUsageModeName(usage));
    
  LOG_INFO("CanvasUsage", "Canvas %s: Usage mode changed to %s", 
       canvas_id_.c_str(), GetUsageModeName(usage).c_str());
  }
}

void CanvasUsageTracker::RecordInteraction(CanvasInteraction interaction, 
                                          const std::string& details) {
  interaction_history_.push_back({interaction, details});
  
  // Update activity time
  last_activity_ = std::chrono::steady_clock::now();
  
  // Update interaction counts
  switch (interaction) {
    case CanvasInteraction::kMouseClick:
      current_stats_.mouse_clicks++;
      break;
    case CanvasInteraction::kMouseDrag:
      current_stats_.mouse_drags++;
      break;
    case CanvasInteraction::kContextMenu:
      current_stats_.context_menu_opens++;
      break;
    case CanvasInteraction::kModalOpen:
      current_stats_.modal_opens++;
      break;
    case CanvasInteraction::kToolChange:
      current_stats_.tool_changes++;
      break;
    case CanvasInteraction::kModeChange:
      current_stats_.mode_changes++;
      break;
    default:
      break;
  }
}

void CanvasUsageTracker::RecordOperation(const std::string& operation_name, 
                                        double time_ms) {
  operation_times_[operation_name].push_back(time_ms);
  current_stats_.total_operations++;
  
  // Update average operation time
  double total_time = 0.0;
  int total_ops = 0;
  for (const auto& [name, times] : operation_times_) {
    for (double t : times) {
      total_time += t;
      total_ops++;
    }
  }
  
  if (total_ops > 0) {
    current_stats_.average_operation_time_ms = total_time / total_ops;
  }
  
  // Update max operation time
  if (time_ms > current_stats_.max_operation_time_ms) {
    current_stats_.max_operation_time_ms = time_ms;
  }
  
  // Record as interaction
  RecordInteraction(CanvasInteraction::kKeyboardInput, operation_name);
}

void CanvasUsageTracker::UpdateCanvasState(const ImVec2& canvas_size,
                                          const ImVec2& content_size,
                                          float global_scale,
                                          float grid_step,
                                          bool enable_grid,
                                          bool enable_hex_labels,
                                          bool enable_custom_labels) {
  current_stats_.canvas_size = canvas_size;
  current_stats_.content_size = content_size;
  current_stats_.global_scale = global_scale;
  current_stats_.grid_step = grid_step;
  current_stats_.enable_grid = enable_grid;
  current_stats_.enable_hex_labels = enable_hex_labels;
  current_stats_.enable_custom_labels = enable_custom_labels;
  
  // Update activity time
  last_activity_ = std::chrono::steady_clock::now();
}

// These methods are already defined in the header as inline, removing duplicates

std::string CanvasUsageTracker::GetUsageModeName(CanvasUsage usage) const {
  switch (usage) {
    case CanvasUsage::kTilePainting: return "Tile Painting";
    case CanvasUsage::kTileSelecting: return "Tile Selecting";
    case CanvasUsage::kSelectRectangle: return "Rectangle Selection";
    case CanvasUsage::kColorPainting: return "Color Painting";
    case CanvasUsage::kBitmapEditing: return "Bitmap Editing";
    case CanvasUsage::kPaletteEditing: return "Palette Editing";
    case CanvasUsage::kBppConversion: return "BPP Conversion";
    case CanvasUsage::kPerformanceMode: return "Performance Mode";
    case CanvasUsage::kUnknown: return "Unknown";
    default: return "Unknown";
  }
}

ImVec4 CanvasUsageTracker::GetUsageModeColor(CanvasUsage usage) const {
  switch (usage) {
    case CanvasUsage::kTilePainting: return ImVec4(0.2F, 1.0F, 0.2F, 1.0F); // Green
    case CanvasUsage::kTileSelecting: return ImVec4(0.2F, 0.8F, 1.0F, 1.0F); // Blue
    case CanvasUsage::kSelectRectangle: return ImVec4(1.0F, 0.8F, 0.2F, 1.0F); // Yellow
    case CanvasUsage::kColorPainting: return ImVec4(1.0F, 0.2F, 1.0F, 1.0F); // Magenta
    case CanvasUsage::kBitmapEditing: return ImVec4(1.0F, 0.5F, 0.2F, 1.0F); // Orange
    case CanvasUsage::kPaletteEditing: return ImVec4(0.8F, 0.2F, 1.0F, 1.0F); // Purple
    case CanvasUsage::kBppConversion: return ImVec4(0.2F, 1.0F, 1.0F, 1.0F); // Cyan
    case CanvasUsage::kPerformanceMode: return ImVec4(1.0F, 0.2F, 0.2F, 1.0F); // Red
    case CanvasUsage::kUnknown: return ImVec4(0.7F, 0.7F, 0.7F, 1.0F); // Gray
    default: return ImVec4(0.7F, 0.7F, 0.7F, 1.0F); // Gray
  }
}

std::vector<std::string> CanvasUsageTracker::GetUsageRecommendations() const {
  std::vector<std::string> recommendations;
  
  // Analyze usage patterns and provide recommendations
  if (current_stats_.mouse_clicks > 100) {
    recommendations.push_back("Consider using keyboard shortcuts to reduce mouse usage");
  }
  
  if (current_stats_.context_menu_opens > 20) {
    recommendations.push_back("Frequent context menu usage - consider adding toolbar buttons");
  }
  
  if (current_stats_.modal_opens > 10) {
    recommendations.push_back("Many modal dialogs opened - consider persistent panels");
  }
  
  if (current_stats_.average_operation_time_ms > 100.0) {
    recommendations.push_back("Operations are slow - check performance optimization");
  }
  
  if (current_stats_.mode_changes > 5) {
    recommendations.push_back("Frequent mode switching - consider mode-specific toolbars");
  }
  
  return recommendations;
}

std::string CanvasUsageTracker::ExportUsageReport() const {
  std::ostringstream report;
  
  report << "Canvas Usage Report for: " << canvas_id_ << "\n";
  report << "==========================================\n\n";
  
  // Session information
  auto now = std::chrono::steady_clock::now();
  auto session_duration = std::chrono::duration_cast<std::chrono::milliseconds>(
      now - session_start_);
  
  report << "Session Information:\n";
  report << "  Duration: " << FormatDuration(session_duration) << "\n";
  report << "  Current Mode: " << GetUsageModeName(current_stats_.usage_mode) << "\n";
  report << "  Mode Changes: " << current_stats_.mode_changes << "\n\n";
  
  // Interaction statistics
  report << "Interaction Statistics:\n";
  report << "  Mouse Clicks: " << current_stats_.mouse_clicks << "\n";
  report << "  Mouse Drags: " << current_stats_.mouse_drags << "\n";
  report << "  Context Menu Opens: " << current_stats_.context_menu_opens << "\n";
  report << "  Modal Opens: " << current_stats_.modal_opens << "\n";
  report << "  Tool Changes: " << current_stats_.tool_changes << "\n\n";
  
  // Performance statistics
  report << "Performance Statistics:\n";
  report << "  Total Operations: " << current_stats_.total_operations << "\n";
  report << "  Average Operation Time: " << std::fixed << std::setprecision(2) 
         << current_stats_.average_operation_time_ms << " ms\n";
  report << "  Max Operation Time: " << std::fixed << std::setprecision(2) 
         << current_stats_.max_operation_time_ms << " ms\n\n";
  
  // Canvas state
  report << "Canvas State:\n";
  report << "  Canvas Size: " << static_cast<int>(current_stats_.canvas_size.x) 
         << " x " << static_cast<int>(current_stats_.canvas_size.y) << "\n";
  report << "  Content Size: " << static_cast<int>(current_stats_.content_size.x) 
         << " x " << static_cast<int>(current_stats_.content_size.y) << "\n";
  report << "  Global Scale: " << std::fixed << std::setprecision(2) 
         << current_stats_.global_scale << "\n";
  report << "  Grid Step: " << std::fixed << std::setprecision(1) 
         << current_stats_.grid_step << "\n";
  report << "  Grid Enabled: " << (current_stats_.enable_grid ? "Yes" : "No") << "\n";
  report << "  Hex Labels: " << (current_stats_.enable_hex_labels ? "Yes" : "No") << "\n";
  report << "  Custom Labels: " << (current_stats_.enable_custom_labels ? "Yes" : "No") << "\n\n";
  
  // Operation breakdown
  if (!operation_times_.empty()) {
    report << "Operation Breakdown:\n";
    for (const auto& [operation, times] : operation_times_) {
      double avg_time = CalculateAverageOperationTime(operation);
      report << "  " << operation << ": " << times.size() << " operations, "
             << "avg " << std::fixed << std::setprecision(2) << avg_time << " ms\n";
    }
    report << "\n";
  }
  
  // Recommendations
  auto recommendations = GetUsageRecommendations();
  if (!recommendations.empty()) {
    report << "Recommendations:\n";
    for (const auto& rec : recommendations) {
      report << "  • " << rec << "\n";
    }
  }
  
  return report.str();
}

void CanvasUsageTracker::ClearHistory() {
  usage_history_.clear();
  interaction_history_.clear();
  operation_times_.clear();
  current_stats_.Reset();
  current_stats_.session_start = std::chrono::steady_clock::now();
  last_activity_ = current_stats_.session_start;
  session_start_ = current_stats_.session_start;
}

void CanvasUsageTracker::StartSession() {
  session_start_ = std::chrono::steady_clock::now();
  current_stats_.session_start = session_start_;
  last_activity_ = session_start_;
}

void CanvasUsageTracker::EndSession() {
  // Update final statistics
  UpdateActiveTime();
  UpdateIdleTime();
  
  // Save final stats
  SaveCurrentStats();
  
  LOG_INFO("CanvasUsage", "Canvas %s: Session ended. Duration: %s, Operations: %d", 
       canvas_id_.c_str(), 
       FormatDuration(std::chrono::duration_cast<std::chrono::milliseconds>(
         std::chrono::steady_clock::now() - session_start_)).c_str(),
       current_stats_.total_operations);
}

void CanvasUsageTracker::UpdateActiveTime() {
  auto now = std::chrono::steady_clock::now();
  auto time_since_activity = std::chrono::duration_cast<std::chrono::milliseconds>(
      now - last_activity_);
  
  if (time_since_activity.count() < 5000) { // 5 seconds threshold
    current_stats_.active_time += time_since_activity;
  }
}

void CanvasUsageTracker::UpdateIdleTime() {
  auto now = std::chrono::steady_clock::now();
  auto time_since_activity = std::chrono::duration_cast<std::chrono::milliseconds>(
      now - last_activity_);
  
  if (time_since_activity.count() >= 5000) { // 5 seconds threshold
    current_stats_.idle_time += time_since_activity;
  }
}

void CanvasUsageTracker::SaveCurrentStats() {
  // Update final times
  UpdateActiveTime();
  UpdateIdleTime();
  
  // Calculate total time
  current_stats_.total_time = current_stats_.active_time + current_stats_.idle_time;
  
  // Save to history
  usage_history_.push_back(current_stats_);
  
  // Reset for next session
  current_stats_.Reset();
  current_stats_.session_start = std::chrono::steady_clock::now();
}

double CanvasUsageTracker::CalculateAverageOperationTime(const std::string& operation_name) const {
  auto it = operation_times_.find(operation_name);
  if (it == operation_times_.end() || it->second.empty()) {
    return 0.0;
  }
  
  double total = 0.0;
  for (double time : it->second) {
    total += time;
  }
  
  return total / it->second.size();
}

std::string CanvasUsageTracker::FormatDuration(const std::chrono::milliseconds& duration) const {
  auto total_ms = duration.count();
  auto hours = total_ms / 3600000;
  auto minutes = (total_ms % 3600000) / 60000;
  auto seconds = (total_ms % 60000) / 1000;
  
  std::ostringstream ss;
  if (hours > 0) {
    ss << hours << "h " << minutes << "m " << seconds << "s";
  } else if (minutes > 0) {
    ss << minutes << "m " << seconds << "s";
  } else {
    ss << seconds << "s";
  }
  
  return ss.str();
}

// CanvasUsageManager implementation

CanvasUsageManager& CanvasUsageManager::Get() {
  static CanvasUsageManager instance;
  return instance;
}

void CanvasUsageManager::RegisterTracker(const std::string& canvas_id, 
                                        std::shared_ptr<CanvasUsageTracker> tracker) {
  trackers_[canvas_id] = tracker;
  LOG_INFO("CanvasUsage", "Registered usage tracker for canvas: %s", canvas_id.c_str());
}

std::shared_ptr<CanvasUsageTracker> CanvasUsageManager::GetTracker(const std::string& canvas_id) {
  auto it = trackers_.find(canvas_id);
  if (it != trackers_.end()) {
    return it->second;
  }
  return nullptr;
}

CanvasUsageStats CanvasUsageManager::GetGlobalStats() const {
  CanvasUsageStats global_stats;
  
  for (const auto& [id, tracker] : trackers_) {
    const auto& stats = tracker->GetCurrentStats();
    
    global_stats.mouse_clicks += stats.mouse_clicks;
    global_stats.mouse_drags += stats.mouse_drags;
    global_stats.context_menu_opens += stats.context_menu_opens;
    global_stats.modal_opens += stats.modal_opens;
    global_stats.tool_changes += stats.tool_changes;
    global_stats.mode_changes += stats.mode_changes;
    global_stats.total_operations += stats.total_operations;
    
    // Update averages
    if (stats.average_operation_time_ms > global_stats.average_operation_time_ms) {
      global_stats.average_operation_time_ms = stats.average_operation_time_ms;
    }
    if (stats.max_operation_time_ms > global_stats.max_operation_time_ms) {
      global_stats.max_operation_time_ms = stats.max_operation_time_ms;
    }
  }
  
  return global_stats;
}

std::string CanvasUsageManager::ExportGlobalReport() const {
  std::ostringstream report;
  
  report << "Global Canvas Usage Report\n";
  report << "==========================\n\n";
  
  report << "Registered Canvases: " << trackers_.size() << "\n\n";
  
  for (const auto& [id, tracker] : trackers_) {
    report << "Canvas: " << id << "\n";
    report << "----------------------------------------\n";
    report << tracker->ExportUsageReport() << "\n\n";
  }
  
  // Global summary
  auto global_stats = GetGlobalStats();
  report << "Global Summary:\n";
  report << "  Total Mouse Clicks: " << global_stats.mouse_clicks << "\n";
  report << "  Total Operations: " << global_stats.total_operations << "\n";
  report << "  Average Operation Time: " << std::fixed << std::setprecision(2) 
         << global_stats.average_operation_time_ms << " ms\n";
  report << "  Max Operation Time: " << std::fixed << std::setprecision(2) 
         << global_stats.max_operation_time_ms << " ms\n";
  
  return report.str();
}

void CanvasUsageManager::ClearAllTrackers() {
  for (auto& [id, tracker] : trackers_) {
    tracker->ClearHistory();
  }
  trackers_.clear();
  LOG_INFO("CanvasUsage", "Cleared all canvas usage trackers");
}

}  // namespace canvas
}  // namespace gui
}  // namespace yaze
