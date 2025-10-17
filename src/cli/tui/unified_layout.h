#ifndef YAZE_CLI_TUI_UNIFIED_LAYOUT_H
#define YAZE_CLI_TUI_UNIFIED_LAYOUT_H

#include <ftxui/component/component.hpp>
#include <ftxui/dom/elements.hpp>
#include <ftxui/screen/screen.hpp>
#include <functional>
#include <memory>
#include <string>
#include <vector>

#include "app/rom.h"
#include "cli/tui/chat_tui.h"
#include "cli/tui/hex_viewer.h"

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
  std::string command_palette_hint;
  std::string todo_summary;
  std::vector<std::string> active_workflows;
  double last_tool_latency = 0.0;
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
  void ToggleTodoOverlay();
  
  // Configuration
  void SetLayoutConfig(const LayoutConfig& config);
  LayoutConfig GetLayoutConfig() const { return config_; }
  void SetStatusProvider(std::function<ftxui::Element()> provider);
  void SetCommandSummaryProvider(std::function<std::vector<std::string>()> provider);
  void SetTodoProvider(std::function<std::vector<std::string>()> provider);

 private:
  void InitializeTheme();
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
  
  // Event handling
  bool HandleGlobalEvents(const ftxui::Event& event);
  bool HandlePanelEvents(const ftxui::Event& event);
  
  // Rendering
  ftxui::Element RenderPanelHeader(PanelType panel);
  ftxui::Element RenderStatusBar();
  ftxui::Element RenderAnimatedBanner();
  ftxui::Element RenderWorkflowLane();
  ftxui::Element RenderCommandHints();
  ftxui::Element RenderTodoStack();
  ftxui::Element RenderResponsiveGrid(const std::vector<ftxui::Element>& tiles);
  
  // State
  ftxui::ScreenInteractive screen_;
  Rom* rom_context_;
  LayoutConfig config_;
  PanelState state_;
  
  // Components
  std::unique_ptr<tui::ChatTUI> chat_tui_;
  std::unique_ptr<HexViewerComponent> hex_viewer_component_;
  
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
  
  // Event handlers
  std::function<bool(const ftxui::Event&)> global_event_handler_;
  std::function<bool(const ftxui::Event&)> panel_event_handler_;

  // External providers
  std::function<ftxui::Element()> status_provider_;
  std::function<std::vector<std::string>()> command_summary_provider_;
  std::function<std::vector<std::string>()> todo_provider_;

  bool todo_overlay_visible_ = false;
  ftxui::Component todo_overlay_component_;
};

}  // namespace cli
}  // namespace yaze

#endif  // YAZE_CLI_TUI_UNIFIED_LAYOUT_H
