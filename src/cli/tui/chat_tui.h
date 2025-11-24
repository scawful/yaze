#ifndef YAZE_SRC_CLI_TUI_CHAT_TUI_H_
#define YAZE_SRC_CLI_TUI_CHAT_TUI_H_

// FTXUI is not available on WASM builds
#ifndef __EMSCRIPTEN__

#include <atomic>
#include <chrono>
#include <future>
#include <mutex>
#include <optional>
#include <thread>
#include <vector>

#include "cli/service/agent/conversational_agent_service.h"
#include "cli/service/agent/todo_manager.h"
#include "cli/util/autocomplete.h"
#include "ftxui/component/component.hpp"
#include "ftxui/component/screen_interactive.hpp"

namespace yaze {

class Rom;

namespace cli {
namespace tui {

class ChatTUI {
 public:
  explicit ChatTUI(Rom* rom_context = nullptr);
  ~ChatTUI();
  void Run();
  void SetRomContext(Rom* rom_context);

 private:
  void OnSubmit(const std::string& message);
  void LaunchAgentPrompt(const std::string& prompt);
  void CleanupWorkers();
  void StopSpinner();
  void InitializeAutocomplete();

  agent::ChatMessage::SessionMetrics CurrentMetrics() const;

  // Popup state
  void ToggleTodoPopup();
  ftxui::Component CreateTodoPopup();
  ftxui::Component BuildShortcutPalette();
  bool IsPopupOpen() const;
  void ToggleShortcutPalette();

  ftxui::ScreenInteractive screen_ = ftxui::ScreenInteractive::Fullscreen();
  agent::ConversationalAgentService agent_service_;
  agent::TodoManager todo_manager_;
  Rom* rom_context_ = nullptr;
  std::optional<std::string> last_error_;
  AutocompleteEngine autocomplete_engine_;
  std::string rom_header_;

  std::atomic<bool> agent_busy_{false};
  std::atomic<bool> spinner_running_{false};
  std::atomic<int> spinner_index_{0};

  std::vector<std::future<void>> worker_futures_;
  mutable std::mutex worker_mutex_;
  std::chrono::steady_clock::time_point last_send_time_{};
  double last_response_seconds_ = 0.0;
  std::vector<double> latency_history_;

  std::vector<std::string> quick_actions_;

  std::thread spinner_thread_;

  // Popup state
  bool todo_popup_visible_ = false;
  ftxui::Component todo_popup_component_;
  ftxui::Component shortcut_palette_component_;
  bool shortcut_palette_visible_ = false;
  bool todo_manager_ready_ = false;
};

}  // namespace tui
}  // namespace cli
}  // namespace yaze

#endif  // __EMSCRIPTEN__

#endif  // YAZE_SRC_CLI_TUI_CHAT_TUI_H_
