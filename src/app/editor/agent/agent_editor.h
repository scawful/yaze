#ifndef YAZE_APP_EDITOR_AGENT_AGENT_EDITOR_H_
#define YAZE_APP_EDITOR_AGENT_AGENT_EDITOR_H_

#include <filesystem>
#include <functional>
#include <memory>
#include <optional>
#include <string>
#include <vector>

#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "absl/time/clock.h"
#include "absl/time/time.h"
#include "app/editor/agent/agent_state.h"
#include "app/editor/agent/panels/agent_editor_panels.h"
#include "app/editor/editor.h"
#include "app/gui/widgets/text_editor.h"
#include "cli/service/agent/conversational_agent_service.h"
#ifdef YAZE_WITH_GRPC
#include "app/editor/agent/automation_bridge.h"
#endif

namespace yaze {

class Rom;
namespace cli {
class AIService;
}  // namespace cli

namespace editor {

class ToastManager;
class ProposalDrawer;
class AgentChat;
class AgentCollaborationCoordinator;
class AgentConfigPanel;
class FeatureFlagEditorPanel;
class ManifestPanel;
class MesenDebugPanel;
class MesenScreenshotPanel;
class OracleStateLibraryPanel;
class SramViewerPanel;

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
  absl::Status Cut() override {
    return absl::UnimplementedError("Not applicable");
  }
  absl::Status Copy() override {
    return absl::UnimplementedError("Not applicable");
  }
  absl::Status Paste() override {
    return absl::UnimplementedError("Not applicable");
  }
  absl::Status Undo() override {
    return absl::UnimplementedError("Not applicable");
  }
  absl::Status Redo() override {
    return absl::UnimplementedError("Not applicable");
  }
  absl::Status Find() override {
    return absl::UnimplementedError("Not applicable");
  }

  // Initialization with dependencies
  void InitializeWithDependencies(ToastManager* toast_manager,
                                  ProposalDrawer* proposal_drawer, Rom* rom);
  void SetRomContext(Rom* rom);
  void SetContext(AgentUIContext* context);

  // Main rendering (called by Update())
  void DrawDashboard();

  // Bot Configuration & Profile Management
  struct BotProfile {
    std::string name = "Default Bot";
    std::string description;
    std::string provider = "mock";
    std::string host_id;
    std::string model;
    std::string ollama_host = "http://localhost:11434";
    std::string gemini_api_key;
    std::string anthropic_api_key;
    std::string openai_api_key;
    std::string openai_base_url = "https://api.openai.com";
    std::string system_prompt;
    bool verbose = false;
    bool show_reasoning = true;
    int max_tool_iterations = 4;
    int max_retry_attempts = 3;
    float temperature = 0.25f;
    float top_p = 0.95f;
    int max_output_tokens = 2048;
    bool stream_responses = false;
    std::vector<std::string> tags;
    absl::Time created_at = absl::Now();
    absl::Time modified_at = absl::Now();
  };

  // Profile accessor for external sync (used by AgentUiController)
  const BotProfile& GetCurrentProfile() const { return current_profile_; }
  BotProfile& GetCurrentProfile() { return current_profile_; }

  // Legacy support
  struct AgentConfig {
    std::string provider = "mock";
    std::string model;
    std::string ollama_host = "http://localhost:11434";
    std::string gemini_api_key;
    std::string anthropic_api_key;
    std::string openai_api_key;
    std::string openai_base_url = "https://api.openai.com";
    bool verbose = false;
    bool show_reasoning = true;
    int max_tool_iterations = 4;
    int max_retry_attempts = 3;
    float temperature = 0.25f;
    float top_p = 0.95f;
    int max_output_tokens = 2048;
    bool stream_responses = false;
  };

  struct AgentBuilderState {
    struct Stage {
      std::string name;
      std::string summary;
      bool completed = false;
    };
    std::vector<Stage> stages;
    int active_stage = 0;
    std::vector<std::string> goals;
    std::string persona_notes;
    struct ToolPlan {
      bool resources = true;
      bool dungeon = true;
      bool overworld = true;
      bool dialogue = true;
      bool gui = false;
      bool music = false;
      bool sprite = false;
      bool emulator = false;
      bool memory_inspector = false;
    } tools;
    bool auto_run_tests = false;
    bool auto_sync_rom = true;
    bool auto_focus_proposals = true;
    std::string blueprint_path;
    bool ready_for_e2e = false;
  };

  // Retro hacker animation state
  float pulse_animation_ = 0.0f;
  float scanline_offset_ = 0.0f;
  float glitch_timer_ = 0.0f;
  int blink_counter_ = 0;

  AgentConfig GetCurrentConfig() const;
  void ApplyConfig(const AgentConfig& config);
  void ApplyUserSettingsDefaults(bool force = false);

  // Bot Profile Management
  absl::Status SaveBotProfile(const BotProfile& profile);
  absl::Status LoadBotProfile(const std::string& name);
  absl::Status DeleteBotProfile(const std::string& name);
  std::vector<BotProfile> GetAllProfiles() const;
  void SetCurrentProfile(const BotProfile& profile);
  absl::Status ExportProfile(const BotProfile& profile,
                             const std::filesystem::path& path);
  absl::Status ImportProfile(const std::filesystem::path& path);

  // Chat widget access (for EditorManager)
  AgentChat* GetAgentChat() { return agent_chat_.get(); }
  bool IsChatActive() const;
  void SetChatActive(bool active);
  void ToggleChat();
  void OpenChatWindow();

  // Knowledge panel callback (set by AgentUiController)
  using KnowledgePanelCallback = std::function<void()>;
  void SetKnowledgePanelCallback(KnowledgePanelCallback callback) {
    knowledge_panel_callback_ = std::move(callback);
  }

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

  absl::StatusOr<SessionInfo> HostSession(
      const std::string& session_name,
      CollaborationMode mode = CollaborationMode::kLocal);
  absl::StatusOr<SessionInfo> JoinSession(
      const std::string& session_code,
      CollaborationMode mode = CollaborationMode::kLocal);
  absl::Status LeaveSession();
  absl::StatusOr<SessionInfo> RefreshSession();

  struct CaptureConfig {
    enum class CaptureMode { kFullWindow, kActiveEditor, kSpecificWindow };
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
  AgentCollaborationCoordinator* GetLocalCoordinator() {
    return local_coordinator_.get();
  }
#ifdef YAZE_WITH_GRPC
  NetworkCollaborationCoordinator* GetNetworkCoordinator() {
    return network_coordinator_.get();
  }
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
  void DrawAgentBuilderPanel();
  void DrawFeatureFlagPanel();
  void DrawManifestPanel();
  void DrawMesenDebugPanel();
  void DrawMesenScreenshotPanel();
  void DrawOracleStatePanel();
  void DrawSramViewerPanel();
  void SyncContextFromProfile();
  void SyncConfigFromProfile();
  void ApplyConfigFromContext(const AgentConfigState& config);
  void ApplyToolPreferencesFromContext();
  void RefreshModelCache(bool force);
  void ApplyModelPreset(const ModelPreset& preset);
  bool MaybeAutoDetectLocalProviders();

  // Setup callbacks
  void SetupMultimodalCallbacks();
  void SetupAutomationCallbacks();

  // Bot profile helpers
  std::filesystem::path GetProfilesDirectory() const;
  absl::Status EnsureProfilesDirectory();
  std::string ProfileToJson(const BotProfile& profile) const;
  absl::StatusOr<BotProfile> JsonToProfile(const std::string& json) const;
  absl::Status SaveBuilderBlueprint(const std::filesystem::path& path);
  absl::Status LoadBuilderBlueprint(const std::filesystem::path& path);

  // Profile UI state
  void MarkProfileUiDirty();
  void SyncProfileUiState();

  // Internal state
  std::unique_ptr<AgentChat> agent_chat_;  // Owned by AgentEditor
  std::unique_ptr<AgentCollaborationCoordinator> local_coordinator_;
  std::unique_ptr<AgentConfigPanel> config_panel_;
  std::unique_ptr<FeatureFlagEditorPanel> feature_flag_panel_;
  std::unique_ptr<ManifestPanel> manifest_panel_;
  std::unique_ptr<MesenDebugPanel> mesen_debug_panel_;
  std::unique_ptr<MesenScreenshotPanel> mesen_screenshot_panel_;
  std::unique_ptr<OracleStateLibraryPanel> oracle_state_panel_;
  std::unique_ptr<SramViewerPanel> sram_viewer_panel_;
#ifdef YAZE_WITH_GRPC
  std::unique_ptr<NetworkCollaborationCoordinator> network_coordinator_;
  AutomationBridge harness_telemetry_bridge_;
#endif

  ToastManager* toast_manager_ = nullptr;
  ProposalDrawer* proposal_drawer_ = nullptr;
  Rom* rom_ = nullptr;
  AgentUIContext* context_ = nullptr;
  // Note: Config syncing is managed by AgentUiController

  // Configuration state (legacy)
  AgentConfig current_config_;

  struct ModelServiceKey {
    std::string provider;
    std::string model;
    std::string ollama_host;
    std::string gemini_api_key;
    std::string anthropic_api_key;
    std::string openai_api_key;
    std::string openai_base_url;
    bool verbose = false;
  };
  ModelServiceKey last_model_service_key_;
  std::unique_ptr<cli::AIService> model_service_;
  std::vector<std::string> last_local_model_paths_;
  absl::Time last_local_model_scan_ = absl::InfinitePast();

  // Bot Profile System
  BotProfile current_profile_;
  std::vector<BotProfile> loaded_profiles_;
  AgentBuilderState builder_state_;

  struct ProfileUiState {
    bool dirty = true;
    char model_buf[128] = {};
    char ollama_host_buf[256] = {};
    char gemini_key_buf[256] = {};
    char anthropic_key_buf[256] = {};
    char openai_key_buf[256] = {};
    char openai_base_buf[256] = {};
    char name_buf[128] = {};
    char desc_buf[256] = {};
    char tags_buf[256] = {};
  };
  ProfileUiState profile_ui_state_;

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

  // UI state (legacy)
  bool show_advanced_settings_ = false;
  bool show_prompt_editor_ = false;
  bool show_bot_profiles_ = false;
  bool show_chat_history_ = false;
  bool show_metrics_dashboard_ = false;
  int selected_tab_ = 0;  // 0=Config, 1=Prompts, 2=Bots, 3=History, 4=Metrics

  // Panel-based UI visibility flags
  bool show_config_card_ = true;
  bool show_status_card_ = true;
  bool show_prompt_editor_card_ = false;
  bool show_profiles_card_ = false;
  bool show_history_card_ = false;
  bool show_metrics_card_ = false;
  bool show_builder_card_ = false;
  bool show_chat_card_ = true;
  bool auto_probe_done_ = false;

  // Panel registration helper
  void RegisterPanels();

  // Chat history viewer state
  std::vector<cli::agent::ChatMessage> cached_history_;
  bool history_needs_refresh_ = true;

  // Knowledge panel callback (set by AgentUiController)
  KnowledgePanelCallback knowledge_panel_callback_;
};

}  // namespace editor
}  // namespace yaze

#endif  // YAZE_APP_EDITOR_AGENT_AGENT_EDITOR_H_
