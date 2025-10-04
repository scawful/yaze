#ifndef YAZE_SRC_APP_EDITOR_SYSTEM_AGENT_CHAT_WIDGET_H_
#define YAZE_SRC_APP_EDITOR_SYSTEM_AGENT_CHAT_WIDGET_H_

#include <filesystem>
#include <string>

#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "absl/time/time.h"
#include "cli/service/agent/conversational_agent_service.h"

namespace yaze {

class Rom;

namespace editor {

class ProposalDrawer;
class ToastManager;

class AgentChatWidget {
 public:
  AgentChatWidget();
 
  void Draw();

  void SetRomContext(Rom* rom);

  void SetToastManager(ToastManager* toast_manager) {
    toast_manager_ = toast_manager;
  }

  void SetProposalDrawer(ProposalDrawer* drawer) {
    proposal_drawer_ = drawer;
  }

  bool* active() { return &active_; }
  bool is_active() const { return active_; }
  void set_active(bool active) { active_ = active; }

 private:
    void EnsureHistoryLoaded();
    void PersistHistory();
    void RenderHistory();
    void RenderMessage(const cli::agent::ChatMessage& msg, int index);
    void RenderProposalQuickActions(const cli::agent::ChatMessage& msg,
                                    int index);
    void RenderInputBox();
    void HandleAgentResponse(
        const absl::StatusOr<cli::agent::ChatMessage>& response);
    int CountKnownProposals() const;
    void FocusProposalDrawer(const std::string& proposal_id);
    void NotifyProposalCreated(const cli::agent::ChatMessage& msg,
                               int new_total_proposals);

  cli::agent::ConversationalAgentService agent_service_;
  char input_buffer_[1024];
  bool active_ = false;
  std::string title_;
  size_t last_history_size_ = 0;
  bool history_loaded_ = false;
  bool history_dirty_ = false;
  std::filesystem::path history_path_;
  int last_proposal_count_ = 0;
  ToastManager* toast_manager_ = nullptr;
  ProposalDrawer* proposal_drawer_ = nullptr;
  std::string pending_focus_proposal_id_;
  absl::Time last_persist_time_ = absl::InfinitePast();
};

}  // namespace editor
}  // namespace yaze

#endif  // YAZE_SRC_APP_EDITOR_SYSTEM_AGENT_CHAT_WIDGET_H_
