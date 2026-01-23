#ifndef YAZE_APP_EDITOR_AGENT_AGENT_STATE_H_
#define YAZE_APP_EDITOR_AGENT_AGENT_STATE_H_

#include <filesystem>
#include <functional>
#include <optional>
#include <string>
#include <vector>

#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "absl/time/time.h"
#include "imgui/imgui.h"
#include "core/project.h"
#include "core/asar_wrapper.h"

namespace yaze {

class Rom;

namespace editor {

// ============================================================================
// Collaboration State
// ============================================================================

/**
 * @brief Collaboration mode for multi-user sessions
 */
enum class CollaborationMode {
  kLocal = 0,   // Filesystem-based collaboration
  kNetwork = 1  // WebSocket-based collaboration
};

/**
 * @brief State for collaborative editing sessions
 */
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

// ============================================================================
// Multimodal State
// ============================================================================

/**
 * @brief Screenshot capture mode
 */
enum class CaptureMode {
  kFullWindow = 0,
  kActiveEditor = 1,
  kSpecificWindow = 2,
  kRegionSelect = 3
};

/**
 * @brief Preview state for captured screenshots
 */
struct ScreenshotPreviewState {
  void* texture_id = nullptr;  // ImTextureID
  int width = 0;
  int height = 0;
  bool loaded = false;
  float preview_scale = 1.0f;
  bool show_preview = true;
};

/**
 * @brief Region selection state for screenshot cropping
 */
struct RegionSelectionState {
  bool active = false;
  bool dragging = false;
  ImVec2 start_pos;
  ImVec2 end_pos;
  ImVec2 selection_min;
  ImVec2 selection_max;
};

/**
 * @brief State for multimodal/vision features
 */
struct MultimodalState {
  std::optional<std::filesystem::path> last_capture_path;
  std::string status_message;
  absl::Time last_updated = absl::InfinitePast();
  CaptureMode capture_mode = CaptureMode::kActiveEditor;
  char specific_window_buffer[128] = {};
  ScreenshotPreviewState preview;
  RegionSelectionState region_selection;
};

// ============================================================================
// Automation State
// ============================================================================

/**
 * @brief Telemetry from automation/test harness
 */
struct AutomationTelemetry {
  std::string test_id;
  std::string name;
  std::string status;
  std::string message;
  absl::Time updated_at = absl::InfinitePast();
};

/**
 * @brief State for automation/test harness integration
 */
struct AutomationState {
  std::vector<AutomationTelemetry> recent_tests;
  bool harness_connected = false;
  absl::Time last_poll = absl::InfinitePast();
  bool auto_refresh_enabled = true;
  float refresh_interval_seconds = 2.0f;
  float pulse_animation = 0.0f;
  float scanline_offset = 0.0f;
  int connection_attempts = 0;
  absl::Time last_connection_attempt = absl::InfinitePast();
  std::string grpc_server_address = "localhost:50052";
  bool auto_run_plan = false;
  bool auto_sync_rom = true;
  bool auto_focus_proposals = true;
};

// ============================================================================
// Agent Configuration
// ============================================================================

/**
 * @brief Model preset for quick switching
 */
struct ModelPreset {
  std::string name;
  std::string model;
  std::string host;
  std::vector<std::string> tags;
  bool pinned = false;
  absl::Time last_used = absl::InfinitePast();
};

/**
 * @brief Tool enablement configuration
 */
struct ToolConfig {
  bool resources = true;
  bool dungeon = true;
  bool overworld = true;
  bool dialogue = true;
  bool messages = true;
  bool gui = true;
  bool music = true;
  bool sprite = true;
  bool emulator = true;
};

/**
 * @brief Model chain mode for multi-model responses
 */
enum class ChainMode {
  kDisabled = 0,
  kRoundRobin = 1,
  kConsensus = 2,
};

/**
 * @brief Agent configuration state
 */
struct AgentConfigState {
  std::string ai_provider = "mock";  // mock, ollama, gemini, openai
  std::string ai_model;
  std::string ollama_host = "http://localhost:11434";
  std::string gemini_api_key;
  std::string openai_api_key;
  std::string openai_base_url = "https://api.openai.com";
  std::string host_id;
  bool verbose = false;
  bool show_reasoning = true;
  int max_tool_iterations = 4;
  int max_retry_attempts = 3;
  float temperature = 0.25f;
  float top_p = 0.95f;
  int max_output_tokens = 2048;
  bool stream_responses = false;
  std::vector<std::string> favorite_models;
  std::vector<std::string> model_chain;
  ChainMode chain_mode = ChainMode::kDisabled;
  std::vector<ModelPreset> model_presets;
  ToolConfig tool_config;

  // Input buffers for UI
  char provider_buffer[32] = "mock";
  char model_buffer[128] = {};
  char ollama_host_buffer[256] = "http://localhost:11434";
  char gemini_key_buffer[256] = {};
  char openai_key_buffer[256] = {};
};

// ============================================================================
// ROM Sync State
// ============================================================================

/**
 * @brief State for ROM synchronization
 */
struct RomSyncState {
  std::string current_rom_hash;
  absl::Time last_sync_time = absl::InfinitePast();
  bool auto_sync_enabled = false;
  int sync_interval_seconds = 30;
  std::vector<std::string> pending_syncs;
};

// ============================================================================
// Z3ED Command State
// ============================================================================

/**
 * @brief State for Z3ED command palette
 */
struct Z3EDCommandState {
  std::string last_command;
  std::string command_output;
  bool command_running = false;
  char command_input_buffer[512] = {};
};

// ============================================================================
// Knowledge Base State
// ============================================================================

/**
 * @brief State for learned knowledge management (CLI integration)
 */
struct KnowledgeState {
  bool initialized = false;
  bool pretraining_enabled = false;
  bool context_injection_enabled = true;
  absl::Time last_refresh = absl::InfinitePast();

  // Cached stats for display
  int preference_count = 0;
  int pattern_count = 0;
  int project_count = 0;
  int memory_count = 0;
};

// ============================================================================
// Tool Execution State
// ============================================================================

/**
 * @brief Single tool execution entry for timeline display
 */
struct ToolExecutionEntry {
  std::string tool_name;
  std::string arguments;
  std::string result_preview;
  double duration_ms = 0.0;
  bool success = true;
  absl::Time executed_at = absl::InfinitePast();
};

/**
 * @brief State for tool execution timeline display
 */
struct ToolExecutionState {
  std::vector<ToolExecutionEntry> recent_executions;
  bool show_timeline = false;
  int max_entries = 50;
};

// ============================================================================
// Persona Profile
// ============================================================================

/**
 * @brief User persona profile for personalized AI behavior
 */
struct PersonaProfile {
  std::string notes;
  std::vector<std::string> goals;
  absl::Time applied_at = absl::InfinitePast();
  bool active = false;
};

// ============================================================================
// Chat State
// ============================================================================

/**
 * @brief State for chat UI and history
 */
struct ChatState {
  char input_buffer[1024] = {};
  bool active = false;
  bool waiting_for_response = false;
  float thinking_animation = 0.0f;
  std::string pending_message;
  size_t last_history_size = 0;
  bool history_loaded = false;
  bool history_dirty = false;
  bool history_supported = true;
  bool history_warning_displayed = false;
  std::filesystem::path history_path;
  int last_proposal_count = 0;
  absl::Time last_persist_time = absl::InfinitePast();

  // Session management
  std::string active_session_id;
  absl::Time last_shared_history_poll = absl::InfinitePast();
  size_t last_known_history_size = 0;

  // UI state
  int active_tab = 0;  // 0=Chat, 1=Config, 2=Commands, etc.
  bool scroll_to_bottom = false;
};

// ============================================================================
// Proposal State
// ============================================================================

/**
 * @brief State for proposal management
 */
struct ProposalState {
  std::string focused_proposal_id;
  std::string pending_focus_proposal_id;
  int total_proposals = 0;
  int pending_proposals = 0;
  int accepted_proposals = 0;
  int rejected_proposals = 0;

  enum class FilterMode { kAll, kPending, kAccepted, kRejected };
  FilterMode filter_mode = FilterMode::kAll;
};

// ============================================================================
// Unified Agent Context (for UI components)
// ============================================================================

/**
 * @class AgentUIContext
 * @brief Unified context for agent UI components
 *
 * This class serves as the single source of truth for agent-related UI state.
 * It's shared among AgentChatView, AgentProposalsPanel, AgentSidebar, etc.
 */
class AgentUIContext {
 public:
  AgentUIContext() = default;

  // State accessors (mutable for UI updates)
  ChatState& chat_state() { return chat_state_; }
  const ChatState& chat_state() const { return chat_state_; }

  CollaborationState& collaboration_state() { return collaboration_state_; }
  const CollaborationState& collaboration_state() const {
    return collaboration_state_;
  }

  MultimodalState& multimodal_state() { return multimodal_state_; }
  const MultimodalState& multimodal_state() const { return multimodal_state_; }

  AutomationState& automation_state() { return automation_state_; }
  const AutomationState& automation_state() const { return automation_state_; }

  AgentConfigState& agent_config() { return agent_config_; }
  const AgentConfigState& agent_config() const { return agent_config_; }

  RomSyncState& rom_sync_state() { return rom_sync_state_; }
  const RomSyncState& rom_sync_state() const { return rom_sync_state_; }

  Z3EDCommandState& z3ed_command_state() { return z3ed_command_state_; }
  const Z3EDCommandState& z3ed_command_state() const {
    return z3ed_command_state_;
  }

  PersonaProfile& persona_profile() { return persona_profile_; }
  const PersonaProfile& persona_profile() const { return persona_profile_; }

  ProposalState& proposal_state() { return proposal_state_; }
  const ProposalState& proposal_state() const { return proposal_state_; }

  KnowledgeState& knowledge_state() { return knowledge_state_; }
  const KnowledgeState& knowledge_state() const { return knowledge_state_; }

  ToolExecutionState& tool_execution_state() { return tool_execution_state_; }
  const ToolExecutionState& tool_execution_state() const {
    return tool_execution_state_;
  }

  // ROM context
  void SetRom(Rom* rom) { rom_ = rom; }
  Rom* GetRom() const { return rom_; }
  bool HasRom() const { return rom_ != nullptr; }

  // Project context
  void SetProject(project::YazeProject* project) { project_ = project; }
  project::YazeProject* GetProject() const { return project_; }
  bool HasProject() const { return project_ != nullptr; }

  // Asar wrapper context
  void SetAsarWrapper(core::AsarWrapper* asar_wrapper) { asar_wrapper_ = asar_wrapper; }
  core::AsarWrapper* GetAsarWrapper() const { return asar_wrapper_; }
  bool HasAsarWrapper() const { return asar_wrapper_ != nullptr; }

  // Change notification for observers
  using ChangeCallback = std::function<void()>;
  void AddChangeListener(ChangeCallback callback) {
    change_listeners_.push_back(std::move(callback));
  }
  void NotifyChanged() {
    for (auto& callback : change_listeners_) {
      callback();
    }
  }

 private:
  ChatState chat_state_;
  CollaborationState collaboration_state_;
  MultimodalState multimodal_state_;
  AutomationState automation_state_;
  AgentConfigState agent_config_;
  RomSyncState rom_sync_state_;
  Z3EDCommandState z3ed_command_state_;
  PersonaProfile persona_profile_;
  ProposalState proposal_state_;
  KnowledgeState knowledge_state_;
  ToolExecutionState tool_execution_state_;

  Rom* rom_ = nullptr;
  project::YazeProject* project_ = nullptr; // Project context
  core::AsarWrapper* asar_wrapper_ = nullptr; // AsarWrapper context
  std::vector<ChangeCallback> change_listeners_;
};

// ============================================================================
// Callback Structures (for component communication)
// ============================================================================

/**
 * @brief Callbacks for collaboration operations
 */
struct CollaborationCallbacks {
  struct SessionContext {
    std::string session_id;
    std::string session_name;
    std::vector<std::string> participants;
  };

  std::function<absl::StatusOr<SessionContext>(const std::string&)>
      host_session;
  std::function<absl::StatusOr<SessionContext>(const std::string&)>
      join_session;
  std::function<absl::Status()> leave_session;
  std::function<absl::StatusOr<SessionContext>()> refresh_session;
};

/**
 * @brief Callbacks for multimodal/vision operations
 */
struct MultimodalCallbacks {
  std::function<absl::Status(std::filesystem::path*)> capture_snapshot;
  std::function<absl::Status(const std::filesystem::path&, const std::string&)>
      send_to_gemini;
};

/**
 * @brief Callbacks for automation operations
 */
struct AutomationCallbacks {
  std::function<void()> open_harness_dashboard;
  std::function<void()> replay_last_plan;
  std::function<void(const std::string&)> focus_proposal;
  std::function<void()> show_active_tests;
  std::function<void()> poll_status;
};

/**
 * @brief Callbacks for Z3ED command operations
 */
struct Z3EDCommandCallbacks {
  std::function<absl::Status(const std::string&)> run_agent_task;
  std::function<absl::StatusOr<std::string>(const std::string&)>
      plan_agent_task;
  std::function<absl::StatusOr<std::string>(const std::string&)> diff_proposal;
  std::function<absl::Status(const std::string&)> accept_proposal;
  std::function<absl::Status(const std::string&)> reject_proposal;
  std::function<absl::StatusOr<std::vector<std::string>>()> list_proposals;
};

/**
 * @brief Callbacks for ROM sync operations
 */
struct RomSyncCallbacks {
  std::function<absl::StatusOr<std::string>()> generate_rom_diff;
  std::function<absl::Status(const std::string&, const std::string&)>
      apply_rom_diff;
  std::function<std::string()> get_rom_hash;
};

/**
 * @brief Callbacks for proposal operations
 */
struct ProposalCallbacks {
  std::function<void(const std::string&)> focus_proposal;
  std::function<absl::Status(const std::string&)> accept_proposal;
  std::function<absl::Status(const std::string&)> reject_proposal;
  std::function<void()> refresh_proposals;
};

/**
 * @brief Callbacks for chat operations
 */
struct ChatCallbacks {
  std::function<void(const std::string&)> send_message;
  std::function<void()> clear_history;
  std::function<void()> persist_history;
  std::function<void(const std::string&)> switch_session;
};

}  // namespace editor
}  // namespace yaze

#endif  // YAZE_APP_EDITOR_AGENT_AGENT_STATE_H_
