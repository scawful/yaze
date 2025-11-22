#ifndef YAZE_CLI_TUI_ENHANCED_CHAT_COMPONENT_H
#define YAZE_CLI_TUI_ENHANCED_CHAT_COMPONENT_H

#include <ftxui/component/component.hpp>
#include <ftxui/dom/elements.hpp>
#include <memory>
#include <string>
#include <vector>

#include "app/rom.h"
#include "cli/service/agent/conversational_agent_service.h"
#include "cli/util/autocomplete.h"

namespace yaze {
namespace cli {

// Enhanced chat component that integrates with the unified layout
class EnhancedChatComponent {
 public:
  explicit EnhancedChatComponent(Rom* rom_context = nullptr);

  // Component interface
  ftxui::Component GetComponent();
  void SetRomContext(Rom* rom_context);

  // Chat functionality
  void SendMessage(const std::string& message);
  void ClearHistory();
  void ResetConversation();

  // State management
  bool IsFocused() const { return focused_; }
  void SetFocused(bool focused) { focused_ = focused; }

  // Configuration
  void SetMaxHistoryLines(int lines) { max_history_lines_ = lines; }
  int GetMaxHistoryLines() const { return max_history_lines_; }

 private:
  // Component creation
  ftxui::Component CreateInputComponent();
  ftxui::Component CreateHistoryComponent();
  ftxui::Component CreateChatContainer();

  // Event handling
  bool HandleInputEvents(const ftxui::Event& event);
  bool HandleHistoryEvents(const ftxui::Event& event);

  // Message processing
  void ProcessMessage(const std::string& message);
  void AddMessageToHistory(const std::string& sender,
                           const std::string& message);
  void UpdateHistoryDisplay();

  // Rendering
  ftxui::Element RenderChatMessage(const std::string& sender,
                                   const std::string& message);
  ftxui::Element RenderInputArea();
  ftxui::Element RenderHistoryArea();

  // State
  Rom* rom_context_;
  agent::ConversationalAgentService agent_service_;
  AutocompleteEngine autocomplete_engine_;

  // UI State
  std::string input_message_;
  std::vector<std::pair<std::string, std::string>> chat_history_;
  int selected_history_index_ = 0;
  bool focused_ = false;
  int max_history_lines_ = 20;

  // Components
  ftxui::Component input_component_;
  ftxui::Component history_component_;
  ftxui::Component chat_container_;

  // Event handlers
  std::function<bool(const ftxui::Event&)> input_event_handler_;
  std::function<bool(const ftxui::Event&)> history_event_handler_;
};

}  // namespace cli
}  // namespace yaze

#endif  // YAZE_CLI_TUI_ENHANCED_CHAT_COMPONENT_H
