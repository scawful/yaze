#ifndef YAZE_APP_PLATFORM_WASM_SESSION_BRIDGE_H_
#define YAZE_APP_PLATFORM_WASM_SESSION_BRIDGE_H_

#ifdef __EMSCRIPTEN__

#include <functional>
#include <mutex>
#include <string>
#include <vector>

#include "absl/status/status.h"
#include "core/features.h"

namespace yaze {

// Forward declarations
class Rom;

namespace editor {
class EditorManager;
}  // namespace editor

namespace app {
namespace platform {

/**
 * @brief Shared session state structure for IPC between WASM and z3ed
 *
 * This structure contains all the state that needs to be synchronized
 * between the browser-based WASM app and the z3ed CLI tool.
 */
struct SharedSessionState {
  // ROM state
  bool rom_loaded = false;
  std::string rom_filename;
  std::string rom_title;
  size_t rom_size = 0;
  bool rom_dirty = false;
  
  // Editor state
  std::string current_editor;
  int current_editor_type = 0;
  std::vector<std::string> visible_cards;
  
  // Session info
  size_t session_id = 0;
  size_t session_count = 1;
  std::string session_name;
  
  // Feature flags (serializable subset)
  bool flag_save_all_palettes = false;
  bool flag_save_gfx_groups = false;
  bool flag_save_overworld_maps = true;
  bool flag_load_custom_overworld = false;
  bool flag_apply_zscustom_asm = false;
  
  // Project info
  std::string project_name;
  std::string project_path;
  bool has_project = false;
  
  // Z3ed integration
  std::string last_z3ed_command;
  std::string last_z3ed_result;
  bool z3ed_command_pending = false;
  
  // Serialize to JSON string
  std::string ToJson() const;
  
  // Deserialize from JSON string
  static SharedSessionState FromJson(const std::string& json);
  
  // Update from current EditorManager state
  void UpdateFromEditor(editor::EditorManager* manager);
  
  // Apply changes to EditorManager
  absl::Status ApplyToEditor(editor::EditorManager* manager);
};

/**
 * @brief Session bridge for bidirectional state sync
 *
 * Provides:
 * - window.yaze.session.* JavaScript API
 * - State change event notifications
 * - z3ed command execution and result retrieval
 * - Feature flag synchronization
 */
class WasmSessionBridge {
 public:
  // Callback types
  using StateChangeCallback = std::function<void(const SharedSessionState&)>;
  using CommandCallback = std::function<std::string(const std::string&)>;
  
  /**
   * @brief Initialize the session bridge
   * @param editor_manager Pointer to the main editor manager
   */
  static void Initialize(editor::EditorManager* editor_manager);
  
  /**
   * @brief Check if the session bridge is ready
   */
  static bool IsReady();
  
  /**
   * @brief Setup JavaScript bindings for window.yaze.session
   */
  static void SetupJavaScriptBindings();
  
  /**
   * @brief Get current session state as JSON
   */
  static std::string GetState();
  
  /**
   * @brief Set session state from JSON (for external updates)
   */
  static std::string SetState(const std::string& state_json);
  
  /**
   * @brief Get specific state property
   */
  static std::string GetProperty(const std::string& property_name);
  
  /**
   * @brief Set specific state property
   */
  static std::string SetProperty(const std::string& property_name,
                                  const std::string& value);
  
  // ============================================================================
  // Feature Flags
  // ============================================================================
  
  /**
   * @brief Get all feature flags as JSON
   */
  static std::string GetFeatureFlags();
  
  /**
   * @brief Set a feature flag by name
   */
  static std::string SetFeatureFlag(const std::string& flag_name, bool value);
  
  /**
   * @brief Get available feature flag names
   */
  static std::string GetAvailableFlags();
  
  // ============================================================================
  // Z3ed Command Integration
  // ============================================================================
  
  /**
   * @brief Execute a z3ed command
   * @param command The z3ed command string
   * @return JSON result with output or error
   */
  static std::string ExecuteZ3edCommand(const std::string& command);
  
  /**
   * @brief Get pending z3ed command (for CLI polling)
   */
  static std::string GetPendingCommand();
  
  /**
   * @brief Set z3ed command result (from CLI)
   */
  static std::string SetCommandResult(const std::string& result);
  
  /**
   * @brief Register callback for z3ed commands
   */
  static void SetCommandHandler(CommandCallback handler);
  
  // ============================================================================
  // Event System
  // ============================================================================
  
  /**
   * @brief Subscribe to state changes
   */
  static void OnStateChange(StateChangeCallback callback);
  
  /**
   * @brief Notify subscribers of state change
   */
  static void NotifyStateChange();
  
  /**
   * @brief Force state refresh from EditorManager
   */
  static std::string RefreshState();
  
 private:
  static editor::EditorManager* editor_manager_;
  static bool initialized_;
  static SharedSessionState current_state_;
  static std::mutex state_mutex_;
  static std::vector<StateChangeCallback> state_callbacks_;
  static CommandCallback command_handler_;
  
  // Pending z3ed command queue
  static std::string pending_command_;
  static std::string pending_result_;
  static bool command_pending_;
};

}  // namespace platform
}  // namespace app
}  // namespace yaze

#else  // !__EMSCRIPTEN__

// Stub for non-WASM builds
#include <string>
#include <functional>

namespace yaze {
namespace editor {
class EditorManager;
}

namespace app {
namespace platform {

struct SharedSessionState {
  bool rom_loaded = false;
  std::string rom_filename;
  std::string ToJson() const { return "{}"; }
  static SharedSessionState FromJson(const std::string&) { return {}; }
  void UpdateFromEditor(editor::EditorManager*) {}
};

class WasmSessionBridge {
 public:
  using StateChangeCallback = std::function<void(const SharedSessionState&)>;
  using CommandCallback = std::function<std::string(const std::string&)>;
  
  static void Initialize(editor::EditorManager*) {}
  static bool IsReady() { return false; }
  static void SetupJavaScriptBindings() {}
  static std::string GetState() { return "{}"; }
  static std::string SetState(const std::string&) { return "{}"; }
  static std::string GetProperty(const std::string&) { return "{}"; }
  static std::string SetProperty(const std::string&, const std::string&) { return "{}"; }
  static std::string GetFeatureFlags() { return "{}"; }
  static std::string SetFeatureFlag(const std::string&, bool) { return "{}"; }
  static std::string GetAvailableFlags() { return "[]"; }
  static std::string ExecuteZ3edCommand(const std::string&) { return "{}"; }
  static std::string GetPendingCommand() { return "{}"; }
  static std::string SetCommandResult(const std::string&) { return "{}"; }
  static void SetCommandHandler(CommandCallback) {}
  static void OnStateChange(StateChangeCallback) {}
  static void NotifyStateChange() {}
  static std::string RefreshState() { return "{}"; }
};

}  // namespace platform
}  // namespace app
}  // namespace yaze

#endif  // __EMSCRIPTEN__

#endif  // YAZE_APP_PLATFORM_WASM_SESSION_BRIDGE_H_

