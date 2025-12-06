#ifndef YAZE_APP_EDITOR_AGENT_AGENT_CHAT_H_
#define YAZE_APP_EDITOR_AGENT_AGENT_CHAT_H_

#include <filesystem>
#include <string>
#include <vector>

#include "absl/status/status.h"
#include "absl/time/time.h"
#include "app/editor/agent/agent_state.h"
#include "cli/service/agent/conversational_agent_service.h"
#include "app/gui/widgets/text_editor.h"

namespace yaze {
class Rom;
namespace editor {

class ToastManager;
class ProposalDrawer;

/**
 * @class AgentChat
 * @brief Unified Agent Chat Component
 * 
 * Handles:
 * - Chat History Display
 * - User Input
 * - Tool/Proposal rendering within chat
 * - Interaction with AgentService
 */
class AgentChat {
 public:
  AgentChat();
  ~AgentChat() = default;

  // Initialization
  void Initialize(ToastManager* toast_manager, ProposalDrawer* proposal_drawer);
  void SetRomContext(Rom* rom);
  void SetContext(AgentUIContext* context);

  // Main Draw Loop
  // available_height: 0 = auto-resize to fit container
  void Draw(float available_height = 0.0f);

  // Actions
  void SendMessage(const std::string& message);
  void ClearHistory();
  void ScrollToBottom();

  // History Persistence
  absl::Status LoadHistory(const std::string& filepath);
  absl::Status SaveHistory(const std::string& filepath);

  // Accessors
  cli::agent::ConversationalAgentService* GetAgentService();

  // UI Options
  bool auto_scroll() const { return auto_scroll_; }
  void set_auto_scroll(bool v) { auto_scroll_ = v; }
  bool show_timestamps() const { return show_timestamps_; }
  void set_show_timestamps(bool v) { show_timestamps_ = v; }
  bool show_reasoning() const { return show_reasoning_; }
  void set_show_reasoning(bool v) { show_reasoning_ = v; }

  // State
  bool* active() { return &active_; }
  void set_active(bool active) { active_ = active; }

  // Automation Telemetry Support
  struct AutomationTelemetry {
    std::string test_id;
    std::string name;
    std::string status;
    std::string message;
    absl::Time updated_at;
  };

  void UpdateHarnessTelemetry(const AutomationTelemetry& telemetry);
  void SetLastPlanSummary(const std::string& summary);
  const std::vector<AutomationTelemetry>& GetTelemetryHistory() const { return telemetry_history_; }
  const std::string& GetLastPlanSummary() const { return last_plan_summary_; }

 private:
  // UI Rendering
  void RenderToolbar();
  void RenderHistory();
  void RenderInputBox();
  void RenderMessage(const cli::agent::ChatMessage& msg, int index);
  void RenderThinkingIndicator();
  void RenderProposalQuickActions(const cli::agent::ChatMessage& msg, int index);
  void RenderCodeBlock(const std::string& code, const std::string& language, int msg_index);
  void RenderTableData(const cli::agent::ChatMessage::TableData& table);
  void RenderToolTimeline(const cli::agent::ChatMessage& msg);

  // Helpers
  struct ContentBlock {
    enum class Type { kText, kCode };
    Type type;
    std::string content;
    std::string language;
  };
  std::vector<ContentBlock> ParseMessageContent(const std::string& content);
  void HandleAgentResponse(const absl::StatusOr<cli::agent::ChatMessage>& response);

  // Dependencies
  AgentUIContext* context_ = nullptr;
  ToastManager* toast_manager_ = nullptr;
  ProposalDrawer* proposal_drawer_ = nullptr;
  Rom* rom_ = nullptr;

  // Internal State
  cli::agent::ConversationalAgentService agent_service_;
  bool active_ = false;
  bool waiting_for_response_ = false;
  float thinking_animation_ = 0.0f;
  char input_buffer_[4096] = {};
  bool scroll_to_bottom_ = false;
  bool history_loaded_ = false;

  // UI Options (from legacy AgentChatWidget)
  bool auto_scroll_ = true;
  bool show_timestamps_ = true;
  bool show_reasoning_ = false;
  float message_spacing_ = 12.0f;

  // Automation Telemetry State
  std::vector<AutomationTelemetry> telemetry_history_;
  std::string last_plan_summary_;
};

}  // namespace editor
}  // namespace yaze

#endif  // YAZE_APP_EDITOR_AGENT_AGENT_CHAT_H_
