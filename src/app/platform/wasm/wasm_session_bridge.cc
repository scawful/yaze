// clang-format off
#ifdef __EMSCRIPTEN__

#include "app/platform/wasm/wasm_session_bridge.h"

#include <emscripten.h>
#include <emscripten/bind.h>

#include "absl/strings/str_format.h"
#include "app/editor/editor.h"
#include "app/editor/editor_manager.h"
#include "rom/rom.h"
#include "nlohmann/json.hpp"
#include "util/log.h"

namespace yaze {
namespace app {
namespace platform {

// Static member initialization
editor::EditorManager* WasmSessionBridge::editor_manager_ = nullptr;
bool WasmSessionBridge::initialized_ = false;
SharedSessionState WasmSessionBridge::current_state_;
std::mutex WasmSessionBridge::state_mutex_;
std::vector<WasmSessionBridge::StateChangeCallback> WasmSessionBridge::state_callbacks_;
WasmSessionBridge::CommandCallback WasmSessionBridge::command_handler_;
std::string WasmSessionBridge::pending_command_;
std::string WasmSessionBridge::pending_result_;
bool WasmSessionBridge::command_pending_ = false;

// ============================================================================
// SharedSessionState Implementation
// ============================================================================

std::string SharedSessionState::ToJson() const {
  nlohmann::json j;
  
  // ROM state
  j["rom"]["loaded"] = rom_loaded;
  j["rom"]["filename"] = rom_filename;
  j["rom"]["title"] = rom_title;
  j["rom"]["size"] = rom_size;
  j["rom"]["dirty"] = rom_dirty;
  
  // Editor state
  j["editor"]["current"] = current_editor;
  j["editor"]["type"] = current_editor_type;
  j["editor"]["visible_cards"] = visible_cards;
  
  // Session info
  j["session"]["id"] = session_id;
  j["session"]["count"] = session_count;
  j["session"]["name"] = session_name;
  
  // Feature flags
  j["flags"]["save_all_palettes"] = flag_save_all_palettes;
  j["flags"]["save_gfx_groups"] = flag_save_gfx_groups;
  j["flags"]["save_overworld_maps"] = flag_save_overworld_maps;
  j["flags"]["load_custom_overworld"] = flag_load_custom_overworld;
  j["flags"]["apply_zscustom_asm"] = flag_apply_zscustom_asm;
  
  // Project info
  j["project"]["name"] = project_name;
  j["project"]["path"] = project_path;
  j["project"]["has_project"] = has_project;
  
  // Z3ed state
  j["z3ed"]["last_command"] = last_z3ed_command;
  j["z3ed"]["last_result"] = last_z3ed_result;
  j["z3ed"]["command_pending"] = z3ed_command_pending;
  
  return j.dump();
}

SharedSessionState SharedSessionState::FromJson(const std::string& json) {
  SharedSessionState state;
  
  try {
    auto j = nlohmann::json::parse(json);
    
    // ROM state
    if (j.contains("rom")) {
      state.rom_loaded = j["rom"].value("loaded", false);
      state.rom_filename = j["rom"].value("filename", "");
      state.rom_title = j["rom"].value("title", "");
      state.rom_size = j["rom"].value("size", 0);
      state.rom_dirty = j["rom"].value("dirty", false);
    }
    
    // Editor state
    if (j.contains("editor")) {
      state.current_editor = j["editor"].value("current", "");
      state.current_editor_type = j["editor"].value("type", 0);
      if (j["editor"].contains("visible_cards")) {
        state.visible_cards = j["editor"]["visible_cards"].get<std::vector<std::string>>();
      }
    }
    
    // Session info
    if (j.contains("session")) {
      state.session_id = j["session"].value("id", 0);
      state.session_count = j["session"].value("count", 1);
      state.session_name = j["session"].value("name", "");
    }
    
    // Feature flags
    if (j.contains("flags")) {
      state.flag_save_all_palettes = j["flags"].value("save_all_palettes", false);
      state.flag_save_gfx_groups = j["flags"].value("save_gfx_groups", false);
      state.flag_save_overworld_maps = j["flags"].value("save_overworld_maps", true);
      state.flag_load_custom_overworld = j["flags"].value("load_custom_overworld", false);
      state.flag_apply_zscustom_asm = j["flags"].value("apply_zscustom_asm", false);
    }
    
    // Project info
    if (j.contains("project")) {
      state.project_name = j["project"].value("name", "");
      state.project_path = j["project"].value("path", "");
      state.has_project = j["project"].value("has_project", false);
    }
    
  } catch (const std::exception& e) {
    LOG_ERROR("SharedSessionState", "Failed to parse JSON: %s", e.what());
  }
  
  return state;
}

void SharedSessionState::UpdateFromEditor(editor::EditorManager* manager) {
  if (!manager) return;
  
  // ROM state
  auto* rom = manager->GetCurrentRom();
  if (rom && rom->is_loaded()) {
    rom_loaded = true;
    rom_filename = rom->filename();
    rom_title = rom->title();
    rom_size = rom->size();
    rom_dirty = rom->dirty();
  } else {
    rom_loaded = false;
    rom_filename = "";
    rom_title = "";
    rom_size = 0;
    rom_dirty = false;
  }
  
  // Editor state
  auto* current = manager->GetCurrentEditor();
  if (current) {
    current_editor_type = static_cast<int>(current->type());
    if (current_editor_type >= 0 && 
        current_editor_type < static_cast<int>(editor::kEditorNames.size())) {
      current_editor = editor::kEditorNames[current_editor_type];
    }
  }
  
  // Session info
  session_id = manager->GetCurrentSessionIndex();
  session_count = manager->GetActiveSessionCount();
  
  // Feature flags from global
  auto& flags = core::FeatureFlags::get();
  flag_save_all_palettes = flags.kSaveAllPalettes;
  flag_save_gfx_groups = flags.kSaveGfxGroups;
  flag_save_overworld_maps = flags.overworld.kSaveOverworldMaps;
  flag_load_custom_overworld = flags.overworld.kLoadCustomOverworld;
  flag_apply_zscustom_asm = flags.overworld.kApplyZSCustomOverworldASM;
}

absl::Status SharedSessionState::ApplyToEditor(editor::EditorManager* manager) {
  if (!manager) {
    return absl::InvalidArgumentError("EditorManager is null");
  }
  
  // Apply feature flags to global
  auto& flags = core::FeatureFlags::get();
  flags.kSaveAllPalettes = flag_save_all_palettes;
  flags.kSaveGfxGroups = flag_save_gfx_groups;
  flags.overworld.kSaveOverworldMaps = flag_save_overworld_maps;
  flags.overworld.kLoadCustomOverworld = flag_load_custom_overworld;
  flags.overworld.kApplyZSCustomOverworldASM = flag_apply_zscustom_asm;
  
  // Switch editor if changed
  if (!current_editor.empty()) {
    for (size_t i = 0; i < editor::kEditorNames.size(); ++i) {
      if (editor::kEditorNames[i] == current_editor) {
        manager->SwitchToEditor(static_cast<editor::EditorType>(i));
        break;
      }
    }
  }
  
  return absl::OkStatus();
}

// ============================================================================
// JavaScript Bindings Setup
// ============================================================================

EM_JS(void, SetupYazeSessionApi, (), {
  if (typeof Module === 'undefined') return;
  
  // Create unified window.yaze namespace if not exists
  if (!window.yaze) {
    window.yaze = {};
  }
  
  // Session API namespace
  window.yaze.session = {
    // State management
    getState: function() {
      if (Module.sessionGetState) {
        try { return JSON.parse(Module.sessionGetState()); }
        catch(e) { return {error: e.message}; }
      }
      return {error: "API not ready"};
    },
    
    setState: function(state) {
      if (Module.sessionSetState) {
        try { return JSON.parse(Module.sessionSetState(JSON.stringify(state))); }
        catch(e) { return {error: e.message}; }
      }
      return {error: "API not ready"};
    },
    
    getProperty: function(name) {
      if (Module.sessionGetProperty) {
        try { return JSON.parse(Module.sessionGetProperty(name)); }
        catch(e) { return {error: e.message}; }
      }
      return {error: "API not ready"};
    },
    
    setProperty: function(name, value) {
      if (Module.sessionSetProperty) {
        try { return JSON.parse(Module.sessionSetProperty(name, JSON.stringify(value))); }
        catch(e) { return {error: e.message}; }
      }
      return {error: "API not ready"};
    },
    
    refresh: function() {
      if (Module.sessionRefreshState) {
        try { return JSON.parse(Module.sessionRefreshState()); }
        catch(e) { return {error: e.message}; }
      }
      return {error: "API not ready"};
    },
    
    // Feature flags
    getFlags: function() {
      if (Module.sessionGetFeatureFlags) {
        try { return JSON.parse(Module.sessionGetFeatureFlags()); }
        catch(e) { return {error: e.message}; }
      }
      return {error: "API not ready"};
    },
    
    setFlag: function(name, value) {
      if (Module.sessionSetFeatureFlag) {
        try { return JSON.parse(Module.sessionSetFeatureFlag(name, value)); }
        catch(e) { return {error: e.message}; }
      }
      return {error: "API not ready"};
    },
    
    getAvailableFlags: function() {
      if (Module.sessionGetAvailableFlags) {
        try { return JSON.parse(Module.sessionGetAvailableFlags()); }
        catch(e) { return {error: e.message}; }
      }
      return {error: "API not ready"};
    },
    
    // Z3ed integration
    executeCommand: function(command) {
      if (Module.sessionExecuteZ3edCommand) {
        try { return JSON.parse(Module.sessionExecuteZ3edCommand(command)); }
        catch(e) { return {error: e.message}; }
      }
      return {error: "API not ready"};
    },
    
    getPendingCommand: function() {
      if (Module.sessionGetPendingCommand) {
        try { return JSON.parse(Module.sessionGetPendingCommand()); }
        catch(e) { return {error: e.message}; }
      }
      return {error: "API not ready"};
    },
    
    setCommandResult: function(result) {
      if (Module.sessionSetCommandResult) {
        try { return JSON.parse(Module.sessionSetCommandResult(JSON.stringify(result))); }
        catch(e) { return {error: e.message}; }
      }
      return {error: "API not ready"};
    },
    
    isReady: function() {
      return Module.sessionIsReady ? Module.sessionIsReady() : false;
    }
  };
  
  console.log("[yaze] window.yaze.session API initialized");
});

// ============================================================================
// WasmSessionBridge Implementation
// ============================================================================

void WasmSessionBridge::Initialize(editor::EditorManager* editor_manager) {
  editor_manager_ = editor_manager;
  initialized_ = (editor_manager_ != nullptr);
  
  if (initialized_) {
    SetupJavaScriptBindings();
    
    // Initialize state from editor
    std::lock_guard<std::mutex> lock(state_mutex_);
    current_state_.UpdateFromEditor(editor_manager_);
    
    LOG_INFO("WasmSessionBridge", "Session bridge initialized");
  }
}

bool WasmSessionBridge::IsReady() {
  return initialized_ && editor_manager_ != nullptr;
}

void WasmSessionBridge::SetupJavaScriptBindings() {
  SetupYazeSessionApi();
}

std::string WasmSessionBridge::GetState() {
  if (!IsReady()) {
    return R"({"error": "Session bridge not initialized"})";
  }
  
  std::lock_guard<std::mutex> lock(state_mutex_);
  current_state_.UpdateFromEditor(editor_manager_);
  return current_state_.ToJson();
}

std::string WasmSessionBridge::SetState(const std::string& state_json) {
  nlohmann::json result;
  
  if (!IsReady()) {
    result["success"] = false;
    result["error"] = "Session bridge not initialized";
    return result.dump();
  }
  
  try {
    std::lock_guard<std::mutex> lock(state_mutex_);
    auto new_state = SharedSessionState::FromJson(state_json);
    auto status = new_state.ApplyToEditor(editor_manager_);
    
    if (status.ok()) {
      current_state_ = new_state;
      result["success"] = true;
    } else {
      result["success"] = false;
      result["error"] = status.ToString();
    }
  } catch (const std::exception& e) {
    result["success"] = false;
    result["error"] = e.what();
  }
  
  return result.dump();
}

std::string WasmSessionBridge::GetProperty(const std::string& property_name) {
  nlohmann::json result;
  
  if (!IsReady()) {
    result["error"] = "Session bridge not initialized";
    return result.dump();
  }
  
  std::lock_guard<std::mutex> lock(state_mutex_);
  current_state_.UpdateFromEditor(editor_manager_);
  
  if (property_name == "rom.loaded") {
    result["value"] = current_state_.rom_loaded;
  } else if (property_name == "rom.filename") {
    result["value"] = current_state_.rom_filename;
  } else if (property_name == "rom.title") {
    result["value"] = current_state_.rom_title;
  } else if (property_name == "rom.size") {
    result["value"] = current_state_.rom_size;
  } else if (property_name == "rom.dirty") {
    result["value"] = current_state_.rom_dirty;
  } else if (property_name == "editor.current") {
    result["value"] = current_state_.current_editor;
  } else if (property_name == "session.id") {
    result["value"] = current_state_.session_id;
  } else if (property_name == "session.count") {
    result["value"] = current_state_.session_count;
  } else {
    result["error"] = "Unknown property: " + property_name;
  }
  
  return result.dump();
}

std::string WasmSessionBridge::SetProperty(const std::string& property_name,
                                            const std::string& value) {
  nlohmann::json result;
  
  if (!IsReady()) {
    result["success"] = false;
    result["error"] = "Session bridge not initialized";
    return result.dump();
  }
  
  // Most properties are read-only from external sources
  // Only feature flags can be set
  if (property_name.find("flags.") == 0) {
    std::string flag_name = property_name.substr(6);
    try {
      bool flag_value = nlohmann::json::parse(value).get<bool>();
      return SetFeatureFlag(flag_name, flag_value);
    } catch (const std::exception& e) {
      result["success"] = false;
      result["error"] = "Invalid boolean value";
    }
  } else {
    result["success"] = false;
    result["error"] = "Property is read-only: " + property_name;
  }
  
  return result.dump();
}

// ============================================================================
// Feature Flags
// ============================================================================

std::string WasmSessionBridge::GetFeatureFlags() {
  nlohmann::json result;
  
  auto& flags = core::FeatureFlags::get();
  
  result["save_all_palettes"] = flags.kSaveAllPalettes;
  result["save_gfx_groups"] = flags.kSaveGfxGroups;
  result["save_with_change_queue"] = flags.kSaveWithChangeQueue;
  result["save_dungeon_maps"] = flags.kSaveDungeonMaps;
  result["save_graphics_sheet"] = flags.kSaveGraphicsSheet;
  result["log_to_console"] = flags.kLogToConsole;
  result["enable_performance_monitoring"] = flags.kEnablePerformanceMonitoring;
  result["enable_tiered_gfx_architecture"] = flags.kEnableTieredGfxArchitecture;
  result["use_native_file_dialog"] = flags.kUseNativeFileDialog;
  
  // Overworld flags
  result["overworld"]["draw_sprites"] = flags.overworld.kDrawOverworldSprites;
  result["overworld"]["save_maps"] = flags.overworld.kSaveOverworldMaps;
  result["overworld"]["save_entrances"] = flags.overworld.kSaveOverworldEntrances;
  result["overworld"]["save_exits"] = flags.overworld.kSaveOverworldExits;
  result["overworld"]["save_items"] = flags.overworld.kSaveOverworldItems;
  result["overworld"]["save_properties"] = flags.overworld.kSaveOverworldProperties;
  result["overworld"]["load_custom"] = flags.overworld.kLoadCustomOverworld;
  result["overworld"]["apply_zscustom_asm"] = flags.overworld.kApplyZSCustomOverworldASM;
  
  return result.dump();
}

std::string WasmSessionBridge::SetFeatureFlag(const std::string& flag_name, bool value) {
  nlohmann::json result;
  
  auto& flags = core::FeatureFlags::get();
  bool found = true;
  
  if (flag_name == "save_all_palettes") {
    flags.kSaveAllPalettes = value;
  } else if (flag_name == "save_gfx_groups") {
    flags.kSaveGfxGroups = value;
  } else if (flag_name == "save_with_change_queue") {
    flags.kSaveWithChangeQueue = value;
  } else if (flag_name == "save_dungeon_maps") {
    flags.kSaveDungeonMaps = value;
  } else if (flag_name == "save_graphics_sheet") {
    flags.kSaveGraphicsSheet = value;
  } else if (flag_name == "log_to_console") {
    flags.kLogToConsole = value;
  } else if (flag_name == "enable_performance_monitoring") {
    flags.kEnablePerformanceMonitoring = value;
  } else if (flag_name == "overworld.draw_sprites") {
    flags.overworld.kDrawOverworldSprites = value;
  } else if (flag_name == "overworld.save_maps") {
    flags.overworld.kSaveOverworldMaps = value;
  } else if (flag_name == "overworld.save_entrances") {
    flags.overworld.kSaveOverworldEntrances = value;
  } else if (flag_name == "overworld.save_exits") {
    flags.overworld.kSaveOverworldExits = value;
  } else if (flag_name == "overworld.save_items") {
    flags.overworld.kSaveOverworldItems = value;
  } else if (flag_name == "overworld.save_properties") {
    flags.overworld.kSaveOverworldProperties = value;
  } else if (flag_name == "overworld.load_custom") {
    flags.overworld.kLoadCustomOverworld = value;
  } else if (flag_name == "overworld.apply_zscustom_asm") {
    flags.overworld.kApplyZSCustomOverworldASM = value;
  } else {
    found = false;
  }
  
  if (found) {
    result["success"] = true;
    result["flag"] = flag_name;
    result["value"] = value;
    LOG_INFO("WasmSessionBridge", "Set flag %s = %s", flag_name.c_str(), value ? "true" : "false");
  } else {
    result["success"] = false;
    result["error"] = "Unknown flag: " + flag_name;
  }
  
  return result.dump();
}

std::string WasmSessionBridge::GetAvailableFlags() {
  nlohmann::json result = nlohmann::json::array();
  
  result.push_back("save_all_palettes");
  result.push_back("save_gfx_groups");
  result.push_back("save_with_change_queue");
  result.push_back("save_dungeon_maps");
  result.push_back("save_graphics_sheet");
  result.push_back("log_to_console");
  result.push_back("enable_performance_monitoring");
  result.push_back("enable_tiered_gfx_architecture");
  result.push_back("overworld.draw_sprites");
  result.push_back("overworld.save_maps");
  result.push_back("overworld.save_entrances");
  result.push_back("overworld.save_exits");
  result.push_back("overworld.save_items");
  result.push_back("overworld.save_properties");
  result.push_back("overworld.load_custom");
  result.push_back("overworld.apply_zscustom_asm");
  
  return result.dump();
}

// ============================================================================
// Z3ed Command Integration
// ============================================================================

std::string WasmSessionBridge::ExecuteZ3edCommand(const std::string& command) {
  nlohmann::json result;
  
  if (!IsReady()) {
    result["success"] = false;
    result["error"] = "Session bridge not initialized";
    return result.dump();
  }
  
  // If we have a command handler, use it directly
  if (command_handler_) {
    std::string output = command_handler_(command);
    result["success"] = true;
    result["output"] = output;
    
    std::lock_guard<std::mutex> lock(state_mutex_);
    current_state_.last_z3ed_command = command;
    current_state_.last_z3ed_result = output;
    return result.dump();
  }
  
  // Otherwise, queue for external CLI
  {
    std::lock_guard<std::mutex> lock(state_mutex_);
    pending_command_ = command;
    command_pending_ = true;
    current_state_.z3ed_command_pending = true;
    current_state_.last_z3ed_command = command;
  }
  
  result["success"] = true;
  result["queued"] = true;
  result["command"] = command;
  
  LOG_INFO("WasmSessionBridge", "Queued z3ed command: %s", command.c_str());
  return result.dump();
}

std::string WasmSessionBridge::GetPendingCommand() {
  nlohmann::json result;
  
  std::lock_guard<std::mutex> lock(state_mutex_);
  
  if (command_pending_) {
    result["pending"] = true;
    result["command"] = pending_command_;
  } else {
    result["pending"] = false;
  }
  
  return result.dump();
}

std::string WasmSessionBridge::SetCommandResult(const std::string& result_str) {
  nlohmann::json result;
  
  std::lock_guard<std::mutex> lock(state_mutex_);
  
  pending_result_ = result_str;
  command_pending_ = false;
  current_state_.z3ed_command_pending = false;
  current_state_.last_z3ed_result = result_str;
  
  result["success"] = true;
  
  return result.dump();
}

void WasmSessionBridge::SetCommandHandler(CommandCallback handler) {
  command_handler_ = handler;
}

// ============================================================================
// Event System
// ============================================================================

void WasmSessionBridge::OnStateChange(StateChangeCallback callback) {
  state_callbacks_.push_back(callback);
}

void WasmSessionBridge::NotifyStateChange() {
  std::lock_guard<std::mutex> lock(state_mutex_);
  current_state_.UpdateFromEditor(editor_manager_);
  
  for (const auto& callback : state_callbacks_) {
    if (callback) {
      callback(current_state_);
    }
  }
}

std::string WasmSessionBridge::RefreshState() {
  if (!IsReady()) {
    return R"({"error": "Session bridge not initialized"})";
  }
  
  std::lock_guard<std::mutex> lock(state_mutex_);
  current_state_.UpdateFromEditor(editor_manager_);
  
  nlohmann::json result;
  result["success"] = true;
  result["state"] = nlohmann::json::parse(current_state_.ToJson());
  
  return result.dump();
}

// ============================================================================
// Emscripten Bindings
// ============================================================================

EMSCRIPTEN_BINDINGS(wasm_session_bridge) {
  emscripten::function("sessionIsReady", &WasmSessionBridge::IsReady);
  emscripten::function("sessionGetState", &WasmSessionBridge::GetState);
  emscripten::function("sessionSetState", &WasmSessionBridge::SetState);
  emscripten::function("sessionGetProperty", &WasmSessionBridge::GetProperty);
  emscripten::function("sessionSetProperty", &WasmSessionBridge::SetProperty);
  emscripten::function("sessionGetFeatureFlags", &WasmSessionBridge::GetFeatureFlags);
  emscripten::function("sessionSetFeatureFlag", &WasmSessionBridge::SetFeatureFlag);
  emscripten::function("sessionGetAvailableFlags", &WasmSessionBridge::GetAvailableFlags);
  emscripten::function("sessionExecuteZ3edCommand", &WasmSessionBridge::ExecuteZ3edCommand);
  emscripten::function("sessionGetPendingCommand", &WasmSessionBridge::GetPendingCommand);
  emscripten::function("sessionSetCommandResult", &WasmSessionBridge::SetCommandResult);
  emscripten::function("sessionRefreshState", &WasmSessionBridge::RefreshState);
}

}  // namespace platform
}  // namespace app
}  // namespace yaze

#endif  // __EMSCRIPTEN__

