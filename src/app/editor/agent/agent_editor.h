#ifndef YAZE_APP_EDITOR_AGENT_AGENT_EDITOR_H_
#define YAZE_APP_EDITOR_AGENT_AGENT_EDITOR_H_

#include <memory>
#include <optional>
#include <string>
#include <filesystem>
#include <vector>

#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "app/editor/editor.h"
#include "app/gui/modules/text_editor.h"
#include "cli/service/agent/conversational_agent_service.h"

namespace yaze {

class Rom;

namespace editor {

class ToastManager;
class ProposalDrawer;
class AgentChatWidget;
class AgentCollaborationCoordinator;

#ifdef YAZE_WITH_GRPC
class NetworkCollaborationCoordinator;
#endif

/**
 * @class AgentEditor
 * @brief Comprehensive AI Agent Platform & Bot Creator
 * 
 * A full-featured bot creation and management platform:
 * - Agent provider configuration (Ollama, Gemini, Mock)
 * - Model selection and parameters  
 * - System prompt editing with live syntax highlighting
 * - Bot profile management (create, save, load custom bots)
 * - Chat history viewer and management
 * - Session metrics and analytics dashboard
 * - Collaboration settings (Local/Network)
 * - Z3ED command automation presets
 * - Multimodal/vision configuration
 * - Export/Import bot configurations
 * 
 * The chat widget is separate and managed by EditorManager, with
 * a dense/compact mode for focused conversations.
 */
class AgentEditor : public Editor {
 public:
  AgentEditor();
  ~AgentEditor() override;

  // Editor interface implementation
  void Initialize() override;
  absl::Status Load() override;
  absl::Status Save() override;
  absl::Status Update() override;
  absl::Status Cut() override { return absl::UnimplementedError("Not applicable"); }
  absl::Status Copy() override { return absl::UnimplementedError("Not applicable"); }
  absl::Status Paste() override { return absl::UnimplementedError("Not applicable"); }
  absl::Status Undo() override { return absl::UnimplementedError("Not applicable"); }
  absl::Status Redo() override { return absl::UnimplementedError("Not applicable"); }
  absl::Status Find() override { return absl::UnimplementedError("Not applicable"); }

  // Initialization with dependencies
  void InitializeWithDependencies(ToastManager* toast_manager, 
                                   ProposalDrawer* proposal_drawer,
                                   Rom* rom);
  void SetRomContext(Rom* rom);


  // Main rendering (called by Update())
  void DrawDashboard();

  // Bot Configuration & Profile Management
  struct BotProfile {
    std::string name = "Default Bot";
    std::string description;
    std::string provider = "mock";
    std::string model;
    std::string ollama_host = "http://localhost:11434";
    std::string gemini_api_key;
    std::string system_prompt;
    bool verbose = false;
    bool show_reasoning = true;
    int max_tool_iterations = 4;
    int max_retry_attempts = 3;
    std::vector<std::string> tags;
    absl::Time created_at = absl::Now();
    absl::Time modified_at = absl::Now();
  };

  // Legacy support
  struct AgentConfig {
    std::string provider = "mock";
    std::string model;
    std::string ollama_host = "http://localhost:11434";
    std::string gemini_api_key;
    bool verbose = false;
    bool show_reasoning = true;
    int max_tool_iterations = 4;
  };
  
  AgentConfig GetCurrentConfig() const;
  void ApplyConfig(const AgentConfig& config);

  // Bot Profile Management
  absl::Status SaveBotProfile(const BotProfile& profile);
  absl::Status LoadBotProfile(const std::string& name);
  absl::Status DeleteBotProfile(const std::string& name);
  std::vector<BotProfile> GetAllProfiles() const;
  BotProfile GetCurrentProfile() const { return current_profile_; }
  void SetCurrentProfile(const BotProfile& profile);
  absl::Status ExportProfile(const BotProfile& profile, const std::filesystem::path& path);
  absl::Status ImportProfile(const std::filesystem::path& path);

  // Chat widget access (for EditorManager)
  AgentChatWidget* GetChatWidget() { return chat_widget_.get(); }
  bool IsChatActive() const;
  void SetChatActive(bool active);
  void ToggleChat();
  void OpenChatWindow();
  
  // Collaboration and session management  
  enum class CollaborationMode {
    kLocal,   // Filesystem-based collaboration
    kNetwork  // WebSocket-based collaboration
  };

  struct SessionInfo {
    std::string session_id;
    std::string session_name;
    std::vector<std::string> participants;
  };

  absl::StatusOr<SessionInfo> HostSession(const std::string& session_name,
                                          CollaborationMode mode = CollaborationMode::kLocal);
  absl::StatusOr<SessionInfo> JoinSession(const std::string& session_code,
                                          CollaborationMode mode = CollaborationMode::kLocal);
  absl::Status LeaveSession();
  absl::StatusOr<SessionInfo> RefreshSession();
  
  struct CaptureConfig {
    enum class CaptureMode {
      kFullWindow,
      kActiveEditor,
      kSpecificWindow
    };
    CaptureMode mode = CaptureMode::kActiveEditor;
    std::string specific_window_name;
  };

  absl::Status CaptureSnapshot(std::filesystem::path* output_path,
                                const CaptureConfig& config);
  absl::Status SendToGemini(const std::filesystem::path& image_path,
                            const std::string& prompt);

#ifdef YAZE_WITH_GRPC
  absl::Status ConnectToServer(const std::string& server_url);
  void DisconnectFromServer();
  bool IsConnectedToServer() const;
#endif

  bool IsInSession() const;
  CollaborationMode GetCurrentMode() const;
  std::optional<SessionInfo> GetCurrentSession() const;

  // Access to underlying components
  AgentCollaborationCoordinator* GetLocalCoordinator() { return local_coordinator_.get(); }
#ifdef YAZE_WITH_GRPC
  NetworkCollaborationCoordinator* GetNetworkCoordinator() { return network_coordinator_.get(); }
#endif

 private:
  // Dashboard panel rendering
  void DrawConfigurationPanel();
  void DrawStatusPanel();
  void DrawMetricsPanel();
  void DrawPromptEditorPanel();
  void DrawBotProfilesPanel();
  void DrawChatHistoryViewer();
  void DrawAdvancedMetricsPanel();
  void DrawCommonTilesEditor();
  void DrawNewPromptCreator();

  // Setup callbacks
  void SetupChatWidgetCallbacks();
  void SetupMultimodalCallbacks();

  // Bot profile helpers
  std::filesystem::path GetProfilesDirectory() const;
  absl::Status EnsureProfilesDirectory();
  std::string ProfileToJson(const BotProfile& profile) const;
  absl::StatusOr<BotProfile> JsonToProfile(const std::string& json) const;

  // Internal state
  std::unique_ptr<AgentChatWidget> chat_widget_;  // Owned by AgentEditor
  std::unique_ptr<AgentCollaborationCoordinator> local_coordinator_;
#ifdef YAZE_WITH_GRPC
  std::unique_ptr<NetworkCollaborationCoordinator> network_coordinator_;
#endif

  ToastManager* toast_manager_ = nullptr;
  ProposalDrawer* proposal_drawer_ = nullptr;
  Rom* rom_ = nullptr;

  // Configuration state (legacy)
  AgentConfig current_config_;
  
  // Bot Profile System
  BotProfile current_profile_;
  std::vector<BotProfile> loaded_profiles_;
  
  // System Prompt Editor
  std::unique_ptr<TextEditor> prompt_editor_;
  std::unique_ptr<TextEditor> common_tiles_editor_;
  bool prompt_editor_initialized_ = false;
  bool common_tiles_initialized_ = false;
  std::string active_prompt_file_ = "system_prompt_v3.txt";
  char new_prompt_name_[128] = {};
  
  // Collaboration state
  CollaborationMode current_mode_ = CollaborationMode::kLocal;
  bool in_session_ = false;
  std::string current_session_id_;
  std::string current_session_name_;
  std::vector<std::string> current_participants_;
  
  // UI state
  bool show_advanced_settings_ = false;
  bool show_prompt_editor_ = false;
  bool show_bot_profiles_ = false;
  bool show_chat_history_ = false;
  bool show_metrics_dashboard_ = false;
  int selected_tab_ = 0;  // 0=Config, 1=Prompts, 2=Bots, 3=History, 4=Metrics
  
  // Chat history viewer state
  std::vector<cli::agent::ChatMessage> cached_history_;
  bool history_needs_refresh_ = true;
};

}  // namespace editor
}  // namespace yaze

#endif  // YAZE_APP_EDITOR_AGENT_AGENT_EDITOR_H_
