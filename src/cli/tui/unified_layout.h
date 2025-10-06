#ifndef YAZE_CLI_TUI_UNIFIED_LAYOUT_H
#define YAZE_CLI_TUI_UNIFIED_LAYOUT_H

#include <ftxui/component/component.hpp>
#include <ftxui/dom/elements.hpp>
#include <ftxui/screen/screen.hpp>
#include <memory>
#include <string>
#include <vector>

#include "app/rom.h"
#include "cli/tui/chat_tui.h"

namespace yaze {
namespace cli {
namespace tui {
class ChatTUI;
}  // namespace tui
}  // namespace cli
}  // namespace yaze

namespace yaze {
namespace cli {

// Panel types for the unified layout
enum class PanelType {
  kMainMenu,
  kChat,
  kStatus,
  kTools,
  kHexViewer,
  kPaletteEditor,
  kTodoManager,
  kRomTools,
  kGraphicsTools,
  kSettings,
  kHelp
};

// Layout configuration
struct LayoutConfig {
  int left_panel_width = 30;      // Menu/Tools panel
  int right_panel_width = 40;     // Status/Info panel
  int bottom_panel_height = 15;   // Chat panel
  bool show_chat = true;
  bool show_status = true;
  bool show_tools = true;
};

// Panel state management
struct PanelState {
  PanelType active_main_panel = PanelType::kMainMenu;
  PanelType active_tool_panel = PanelType::kRomTools;
  bool chat_focused = false;
  bool status_expanded = false;
  std::string current_rom_file;
  std::string current_error;
};

class UnifiedLayout {
 public:
  explicit UnifiedLayout(Rom* rom_context = nullptr);
  
  // Main interface
  void Run();
  void SetRomContext(Rom* rom_context);
  
  // Panel management
  void SwitchMainPanel(PanelType panel);
  void SwitchToolPanel(PanelType panel);
  void ToggleChat();
  void ToggleStatus();
  
  // Configuration
  void SetLayoutConfig(const LayoutConfig& config);
  LayoutConfig GetLayoutConfig() const { return config_; }

 private:
  // Component creation
  ftxui::Component CreateMainMenuPanel();
  ftxui::Component CreateChatPanel();
  ftxui::Component CreateStatusPanel();
  ftxui::Component CreateToolsPanel();
  ftxui::Component CreateHexViewerPanel();
  ftxui::Component CreatePaletteEditorPanel();
  ftxui::Component CreateTodoManagerPanel();
  ftxui::Component CreateRomToolsPanel();
  ftxui::Component CreateGraphicsToolsPanel();
  ftxui::Component CreateSettingsPanel();
  ftxui::Component CreateHelpPanel();
  
  // Layout assembly
  ftxui::Component CreateUnifiedLayout();
  ftxui::Component CreateTopLayout();
  ftxui::Component CreateBottomLayout();
  
  // Event handling
  bool HandleGlobalEvents(const ftxui::Event& event);
  bool HandlePanelEvents(const ftxui::Event& event);
  
  // Rendering
  ftxui::Element RenderPanelHeader(PanelType panel);
  ftxui::Element RenderStatusBar();
  
  // State
  ftxui::ScreenInteractive screen_;
  Rom* rom_context_;
  LayoutConfig config_;
  PanelState state_;
  
  // Components
  std::unique_ptr<tui::ChatTUI> chat_tui_;
  
  // Panel components (cached for performance)
  ftxui::Component main_menu_panel_;
  ftxui::Component chat_panel_;
  ftxui::Component status_panel_;
  ftxui::Component tools_panel_;
  ftxui::Component hex_viewer_panel_;
  ftxui::Component palette_editor_panel_;
  ftxui::Component todo_manager_panel_;
  ftxui::Component rom_tools_panel_;
  ftxui::Component graphics_tools_panel_;
  ftxui::Component settings_panel_;
  ftxui::Component help_panel_;
  
  // Layout components
  ftxui::Component unified_layout_;
  ftxui::Component top_layout_;
  ftxui::Component bottom_layout_;
  
  // Event handlers
  std::function<bool(const ftxui::Event&)> global_event_handler_;
  std::function<bool(const ftxui::Event&)> panel_event_handler_;
};

}  // namespace cli
}  // namespace yaze

#endif  // YAZE_CLI_TUI_UNIFIED_LAYOUT_H
