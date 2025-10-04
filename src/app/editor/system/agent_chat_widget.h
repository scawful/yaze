#ifndef YAZE_SRC_APP_EDITOR_SYSTEM_AGENT_CHAT_WIDGET_H_
#define YAZE_SRC_APP_EDITOR_SYSTEM_AGENT_CHAT_WIDGET_H_

#include <filesystem>
#include <functional>
#include <optional>
#include <string>
#include <vector>

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

  struct CollaborationCallbacks {
    std::function<absl::Status(const std::string&)> host_session;
    std::function<absl::Status(const std::string&)> join_session;
    std::function<absl::Status()> leave_session;
    std::function<absl::StatusOr<std::vector<std::string>>()> refresh_participants;
  };

  struct MultimodalCallbacks {
    std::function<absl::Status(std::filesystem::path*)> capture_snapshot;
    std::function<absl::Status(const std::filesystem::path&, const std::string&)> send_to_gemini;
  };

  void SetToastManager(ToastManager* toast_manager);

  void SetProposalDrawer(ProposalDrawer* drawer);

  void SetCollaborationCallbacks(const CollaborationCallbacks& callbacks) {
    collaboration_callbacks_ = callbacks;
  }

  void SetMultimodalCallbacks(const MultimodalCallbacks& callbacks) {
    multimodal_callbacks_ = callbacks;
  }

  bool* active() { return &active_; }
  bool is_active() const { return active_; }
  void set_active(bool active) { active_ = active; }

 private:
  struct CollaborationState {
    bool active = false;
    std::string session_id;
    std::vector<std::string> participants;
    absl::Time last_synced = absl::InfinitePast();
  };

  struct MultimodalState {
    std::optional<std::filesystem::path> last_capture_path;
    std::string status_message;
    absl::Time last_updated = absl::InfinitePast();
  };

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
  void RenderCollaborationPanel();
  void RenderMultimodalPanel();
  void RefreshParticipants();
  void MarkHistoryDirty();

  cli::agent::ConversationalAgentService agent_service_;
  char input_buffer_[1024];
  bool active_ = false;
  std::string title_;
  size_t last_history_size_ = 0;
  bool history_loaded_ = false;
  bool history_dirty_ = false;
  bool history_supported_ = true;
  bool history_warning_displayed_ = false;
  std::filesystem::path history_path_;
  int last_proposal_count_ = 0;
  ToastManager* toast_manager_ = nullptr;
  ProposalDrawer* proposal_drawer_ = nullptr;
  std::string pending_focus_proposal_id_;
  absl::Time last_persist_time_ = absl::InfinitePast();
  CollaborationState collaboration_state_;
  CollaborationCallbacks collaboration_callbacks_;
  MultimodalState multimodal_state_;
  MultimodalCallbacks multimodal_callbacks_;
  char session_name_buffer_[64] = {};
  char join_code_buffer_[64] = {};
  char multimodal_prompt_buffer_[256] = {};
  absl::Time last_collaboration_action_ = absl::InfinitePast();
};

}  // namespace editor
}  // namespace yaze

#endif  // YAZE_SRC_APP_EDITOR_SYSTEM_AGENT_CHAT_WIDGET_H_
