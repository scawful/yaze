#ifndef YAZE_APP_EDITOR_AGENT_AGENT_CHAT_WIDGET_H_
#define YAZE_APP_EDITOR_AGENT_AGENT_CHAT_WIDGET_H_

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
    struct SessionContext {
      std::string session_id;
      std::string session_name;
      std::vector<std::string> participants;
    };

    std::function<absl::StatusOr<SessionContext>(const std::string&)> host_session;
    std::function<absl::StatusOr<SessionContext>(const std::string&)> join_session;
    std::function<absl::Status()> leave_session;
    std::function<absl::StatusOr<SessionContext>()> refresh_session;
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

public:
  enum class CollaborationMode {
    kLocal = 0,    // Filesystem-based collaboration
    kNetwork = 1   // WebSocket-based collaboration
  };

  struct CollaborationState {
    bool active = false;
    CollaborationMode mode = CollaborationMode::kLocal;
    std::string session_id;
    std::string session_name;
    std::string server_url = "ws://localhost:8765";
    bool server_connected = false;
    std::vector<std::string> participants;
    absl::Time last_synced = absl::InfinitePast();
  };

  enum class CaptureMode {
    kFullWindow = 0,
    kActiveEditor = 1,
    kSpecificWindow = 2
  };

  struct MultimodalState {
    std::optional<std::filesystem::path> last_capture_path;
    std::string status_message;
    absl::Time last_updated = absl::InfinitePast();
    CaptureMode capture_mode = CaptureMode::kActiveEditor;
    char specific_window_buffer[128] = {};
  };

  // Accessors for capture settings
  CaptureMode capture_mode() const { return multimodal_state_.capture_mode; }
  const char* specific_window_name() const { 
    return multimodal_state_.specific_window_buffer; 
  }

  // Collaboration history management (public so EditorManager can call them)
  void SwitchToSharedHistory(const std::string& session_id);
  void SwitchToLocalHistory();

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
  void RenderCollaborationPanel();
  void RenderMultimodalPanel();
  void RefreshCollaboration();
  void ApplyCollaborationSession(
      const CollaborationCallbacks::SessionContext& context,
      bool update_action_timestamp);
  void MarkHistoryDirty();
  void PollSharedHistory();  // For real-time collaboration sync

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
  char server_url_buffer_[256] = "ws://localhost:8765";
  char multimodal_prompt_buffer_[256] = {};
  absl::Time last_collaboration_action_ = absl::InfinitePast();
  absl::Time last_shared_history_poll_ = absl::InfinitePast();
  size_t last_known_history_size_ = 0;
};

}  // namespace editor
}  // namespace yaze

#endif  // YAZE_APP_EDITOR_AGENT_AGENT_CHAT_WIDGET_H_
