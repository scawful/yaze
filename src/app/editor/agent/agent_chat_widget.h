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
#include "app/gui/modules/text_editor.h"
#include "cli/service/agent/conversational_agent_service.h"
#include "cli/service/agent/advanced_routing.h"
#include "cli/service/agent/agent_pretraining.h"
#include "cli/service/agent/prompt_manager.h"
#include "core/project.h"

namespace yaze {

class Rom;

namespace editor {

class ProposalDrawer;
class ToastManager;
class AgentChatHistoryPopup;

/**
 * @class AgentChatWidget
 * @brief Modern AI chat widget with comprehensive z3ed and yaze-server integration
 * 
 * Features:
 * - AI Provider Configuration (Ollama, Gemini, Mock)
 * - Z3ED Command Palette (run, plan, diff, accept, test)
 * - Real-time Collaboration (Local & Network modes)
 * - ROM Synchronization and Diff Broadcasting
 * - Multimodal Vision (Screenshot Analysis)
 * - Snapshot Sharing with Preview
 * - Collaborative Proposal Management
 * - Tabbed Interface with Modern ImGui patterns
 */
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

  // Z3ED Command Callbacks
  struct Z3EDCommandCallbacks {
    std::function<absl::Status(const std::string&)> run_agent_task;
    std::function<absl::StatusOr<std::string>(const std::string&)> plan_agent_task;
    std::function<absl::StatusOr<std::string>(const std::string&)> diff_proposal;
    std::function<absl::Status(const std::string&)> accept_proposal;
    std::function<absl::Status(const std::string&)> reject_proposal;
    std::function<absl::StatusOr<std::vector<std::string>>()> list_proposals;
  };

  // ROM Sync Callbacks
  struct RomSyncCallbacks {
    std::function<absl::StatusOr<std::string>()> generate_rom_diff;
    std::function<absl::Status(const std::string&, const std::string&)> apply_rom_diff;
    std::function<std::string()> get_rom_hash;
  };

  void SetToastManager(ToastManager* toast_manager);

  void SetProposalDrawer(ProposalDrawer* drawer);
  
  void SetChatHistoryPopup(AgentChatHistoryPopup* popup);

  void SetCollaborationCallbacks(const CollaborationCallbacks& callbacks) {
    collaboration_callbacks_ = callbacks;
  }

  void SetMultimodalCallbacks(const MultimodalCallbacks& callbacks) {
    multimodal_callbacks_ = callbacks;
  }

  void SetZ3EDCommandCallbacks(const Z3EDCommandCallbacks& callbacks) {
    z3ed_callbacks_ = callbacks;
  }

  void SetRomSyncCallbacks(const RomSyncCallbacks& callbacks) {
    rom_sync_callbacks_ = callbacks;
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

  // Agent Configuration State
  struct AgentConfigState {
    std::string ai_provider = "mock";  // mock, ollama, gemini
    std::string ai_model;
    std::string ollama_host = "http://localhost:11434";
    std::string gemini_api_key;
    bool verbose = false;
    bool show_reasoning = true;
    int max_tool_iterations = 4;
    int max_retry_attempts = 3;
    char provider_buffer[32] = "mock";
    char model_buffer[128] = {};
    char ollama_host_buffer[256] = "http://localhost:11434";
    char gemini_key_buffer[256] = {};
  };

  // ROM Sync State
  struct RomSyncState {
    std::string current_rom_hash;
    absl::Time last_sync_time = absl::InfinitePast();
    bool auto_sync_enabled = false;
    int sync_interval_seconds = 30;
    std::vector<std::string> pending_syncs;
  };

  // Z3ED Command State
  struct Z3EDCommandState {
    std::string last_command;
    std::string command_output;
    bool command_running = false;
    char command_input_buffer[512] = {};
  };
  
  void SetPromptMode(cli::agent::PromptMode mode) { prompt_mode_ = mode; }
  cli::agent::PromptMode GetPromptMode() const { return prompt_mode_; }

  // Accessors for capture settings
  CaptureMode capture_mode() const { return multimodal_state_.capture_mode; }
  const char* specific_window_name() const { 
    return multimodal_state_.specific_window_buffer; 
  }

  // Agent configuration accessors
  const AgentConfigState& GetAgentConfig() const { return agent_config_; }
  void UpdateAgentConfig(const AgentConfigState& config);
  
  // Load agent settings from project
  void LoadAgentSettingsFromProject(const core::YazeProject& project);
  void SaveAgentSettingsToProject(core::YazeProject& project);

  // Collaboration history management (public so EditorManager can call them)
  void SwitchToSharedHistory(const std::string& session_id);
  void SwitchToLocalHistory();
  
  // File editing
  void OpenFileInEditor(const std::string& filepath);
  void CreateNewFileInEditor(const std::string& filename);

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
  void RenderAgentConfigPanel();
  void RenderZ3EDCommandPanel();
  void RenderRomSyncPanel();
  void RenderSnapshotPreviewPanel();
  void RenderProposalManagerPanel();
  void RenderSystemPromptEditor();
  void RenderFileEditorTabs();
  void RefreshCollaboration();
  void ApplyCollaborationSession(
      const CollaborationCallbacks::SessionContext& context,
      bool update_action_timestamp);
  void MarkHistoryDirty();
  void PollSharedHistory();  // For real-time collaboration sync
  void HandleRomSyncReceived(const std::string& diff_data, const std::string& rom_hash);
  void HandleSnapshotReceived(const std::string& snapshot_data, const std::string& snapshot_type);
  void HandleProposalReceived(const std::string& proposal_data);
  
  // History synchronization
  void SyncHistoryToPopup();

  // AI response state
  bool waiting_for_response_ = false;
  float thinking_animation_ = 0.0f;
  std::string pending_message_;
  
  // Chat session management
  struct ChatSession {
    std::string id;
    std::string name;
    cli::agent::ConversationalAgentService agent_service;
    size_t last_history_size = 0;
    bool history_loaded = false;
    bool history_dirty = false;
    std::filesystem::path history_path;
    absl::Time last_persist_time = absl::InfinitePast();
    
    ChatSession(const std::string& session_id, const std::string& session_name)
        : id(session_id), name(session_name) {}
  };
  
  std::vector<ChatSession> chat_sessions_;
  int active_session_index_ = 0;
  
  // Legacy single session support (will migrate to sessions)
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
  AgentChatHistoryPopup* chat_history_popup_ = nullptr;
  std::string pending_focus_proposal_id_;
  absl::Time last_persist_time_ = absl::InfinitePast();
  
  // Main state
  CollaborationState collaboration_state_;
  MultimodalState multimodal_state_;
  AgentConfigState agent_config_;
  RomSyncState rom_sync_state_;
  Z3EDCommandState z3ed_command_state_;
  
  // Callbacks
  CollaborationCallbacks collaboration_callbacks_;
  MultimodalCallbacks multimodal_callbacks_;
  Z3EDCommandCallbacks z3ed_callbacks_;
  RomSyncCallbacks rom_sync_callbacks_;
  
  // Input buffers
  char session_name_buffer_[64] = {};
  char join_code_buffer_[64] = {};
  char server_url_buffer_[256] = "ws://localhost:8765";
  char multimodal_prompt_buffer_[256] = {};
  
  // Timing
  absl::Time last_collaboration_action_ = absl::InfinitePast();
  absl::Time last_shared_history_poll_ = absl::InfinitePast();
  size_t last_known_history_size_ = 0;
  
  // UI state
  int active_tab_ = 0;  // 0=Chat, 1=Config, 2=Commands, 3=Collab, 4=ROM Sync, 5=Files, 6=Prompt
  bool show_agent_config_ = false;
  cli::agent::PromptMode prompt_mode_ = cli::agent::PromptMode::kStandard;
  bool show_z3ed_commands_ = false;
  bool show_rom_sync_ = false;
  bool show_snapshot_preview_ = false;
  std::vector<uint8_t> snapshot_preview_data_;
  
  // Reactive UI colors
  ImVec4 collaboration_status_color_ = ImVec4(0.6f, 0.6f, 0.6f, 1.0f);
  
  // File editing state
  struct FileEditorTab {
    std::string filepath;
    std::string filename;
    TextEditor editor;
    bool modified = false;
    bool is_system_prompt = false;
  };
  std::vector<FileEditorTab> open_files_;
  int active_file_tab_ = -1;
};

}  // namespace editor
}  // namespace yaze

#endif  // YAZE_APP_EDITOR_AGENT_AGENT_CHAT_WIDGET_H_
