#ifndef YAZE_CLI_TUI_ENHANCED_STATUS_PANEL_H
#define YAZE_CLI_TUI_ENHANCED_STATUS_PANEL_H

#include <ftxui/component/component.hpp>
#include <ftxui/dom/elements.hpp>
#include <memory>
#include <string>
#include <vector>

#include "app/rom.h"

namespace yaze {
namespace cli {

// Enhanced status panel with comprehensive system information
class EnhancedStatusPanel {
 public:
  explicit EnhancedStatusPanel(Rom* rom_context = nullptr);

  // Component interface
  ftxui::Component GetComponent();
  void SetRomContext(Rom* rom_context);

  // Status updates
  void UpdateRomInfo();
  void UpdateSystemInfo();
  void UpdateLayoutInfo();
  void SetError(const std::string& error);
  void ClearError();

  // Configuration
  void SetShowDetailedInfo(bool show) { show_detailed_info_ = show; }
  bool GetShowDetailedInfo() const { return show_detailed_info_; }

  // State management
  bool IsExpanded() const { return expanded_; }
  void SetExpanded(bool expanded) { expanded_ = expanded; }

 private:
  // Component creation
  ftxui::Component CreateStatusContainer();
  ftxui::Component CreateRomInfoSection();
  ftxui::Component CreateSystemInfoSection();
  ftxui::Component CreateLayoutInfoSection();
  ftxui::Component CreateErrorSection();

  // Event handling
  bool HandleStatusEvents(const ftxui::Event& event);

  // Rendering
  ftxui::Element RenderRomInfo();
  ftxui::Element RenderSystemInfo();
  ftxui::Element RenderLayoutInfo();
  ftxui::Element RenderErrorInfo();
  ftxui::Element RenderStatusBar();

  // Data collection
  void CollectRomInfo();
  void CollectSystemInfo();
  void CollectLayoutInfo();

  // State
  Rom* rom_context_;
  bool expanded_ = false;
  bool show_detailed_info_ = true;

  // Status data
  struct RomInfo {
    std::string title;
    std::string size;
    std::string checksum;
    std::string region;
    bool loaded = false;
  } rom_info_;

  struct SystemInfo {
    std::string current_time;
    std::string memory_usage;
    std::string cpu_usage;
    std::string disk_space;
  } system_info_;

  struct LayoutInfo {
    std::string active_panel;
    std::string panel_count;
    std::string layout_mode;
    std::string focus_state;
  } layout_info_;

  std::string current_error_;

  // Components
  ftxui::Component status_container_;
  ftxui::Component rom_info_section_;
  ftxui::Component system_info_section_;
  ftxui::Component layout_info_section_;
  ftxui::Component error_section_;

  // Event handlers
  std::function<bool(const ftxui::Event&)> status_event_handler_;
};

}  // namespace cli
}  // namespace yaze

#endif  // YAZE_CLI_TUI_ENHANCED_STATUS_PANEL_H
