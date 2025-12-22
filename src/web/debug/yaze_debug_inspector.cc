/**
 * @file yaze_debug_inspector.cc
 * @brief WASM debug inspector for yaze - palette, overworld, and emulator access
 *
 * This file provides JavaScript bindings for debugging infrastructure used by
 * Gemini/Antigravity AI integration to analyze rendering issues and game state.
 */

#include <emscripten.h>
#include <emscripten/bind.h>

#include <atomic>
#include <iomanip>
#include <sstream>
#include <unordered_map>

#include "yaze.h"  // For YAZE_VERSION_STRING
#include "app/emu/emulator.h"
#include "app/emu/snes.h"
#include "app/emu/video/ppu.h"
#include "app/gfx/resource/arena.h"
#include "rom/rom.h"
#include "zelda3/dungeon/palette_debug.h"
#include "zelda3/game_data.h"

#include "app/editor/editor_manager.h"
#include "app/editor/editor.h"
#include "app/editor/agent/agent_session.h"
#include "app/editor/agent/agent_editor.h"
#include "app/editor/agent/agent_chat.h"
#include "cli/service/agent/conversational_agent_service.h"
#include "cli/service/ai/common.h"
#include "nlohmann/json.hpp"

using namespace emscripten;

// External function to get the global ROM (defined in wasm_terminal_bridge.cc)
namespace yaze::cli {
extern Rom* GetGlobalRom();
}

// External function to get the global emulator (defined in main.cc)
namespace yaze::app {
extern emu::Emulator* GetGlobalEmulator();
extern editor::EditorManager* GetGlobalEditorManager();
}

extern "C" {
// Forward declaration of Z3edProcessCommand from wasm_terminal_bridge.cc
const char* Z3edProcessCommand(const char* command);
}

// Helper function to get the emulator for this file
namespace {
yaze::emu::Emulator* GetGlobalEmulator() {
  return yaze::app::GetGlobalEmulator();
}

// Helper to access the live EditorManager without repeating the namespace
yaze::editor::EditorManager* GetEditorManager() {
  return yaze::app::GetGlobalEditorManager();
}

// =============================================================================
// AI Driver Bridge (WASM <-> JS)
// =============================================================================

// JS function to call the AI Manager
EM_JS(void, CallJsAiDriver, (const char* history_json), {
  if (window.yaze && window.yaze.ai && window.yaze.ai.processAgentRequest) {
    window.yaze.ai.processAgentRequest(UTF8ToString(history_json));
  } else {
    console.error("AI Driver not found in window.yaze.ai.processAgentRequest");
    // Try to recover or notify C++
  }
});

// Helper to serialize ChatMessage
nlohmann::json MessageToJson(const yaze::cli::agent::ChatMessage& msg) {
  nlohmann::json j;
  j["role"] = (msg.sender == yaze::cli::agent::ChatMessage::Sender::kUser) ? "user" : "model";
  
  // Convert parts (text, internal logs)
  // Simplification: just send message content for now
  j["parts"] = nlohmann::json::array({ {{"text", msg.message}} });
  
  return j;
}

// Register the external driver for the active session
std::string registerExternalAiDriver() {
  auto* manager = GetEditorManager();
  if (!manager) return "{\"error\":\"EditorManager not available\"}";

  auto* agent_ui = manager->GetAgentUiController();
  if (!agent_ui) return "{\"error\":\"AgentUiController not available\"}";

#if defined(YAZE_BUILD_AGENT_UI)
  auto& sessions = agent_ui->GetSessionManager();
  
  // Get the active session (AgentSessionManager creates a session on startup)
  // This is more reliable than looking for a "default" session ID
  yaze::editor::AgentSession* session = sessions.GetActiveSession();
  if (!session) {
    // Fallback: if no active session but sessions exist, use the first one
    // This shouldn't happen normally since the constructor creates an active session
    if (sessions.GetSessionCount() == 0) {
      return "{\"error\":\"No agent sessions available\"}";
    }
    // Get the first session as fallback
    auto& all_sessions = sessions.GetAllSessions();
    if (all_sessions.empty()) {
      return "{\"error\":\"No agent sessions available\"}";
    }
    session = &all_sessions[0];
  }
  
  // Access agent service through AgentEditor -> AgentChat
  // The new architecture doesn't store agent_service directly in AgentSession
  auto* agent_editor = manager->GetAgentEditor();
  if (!agent_editor) {
    return "{\"error\":\"AgentEditor not available\"}";
  }

  auto* agent_chat = agent_editor->GetAgentChat();
  if (!agent_chat) {
    return "{\"error\":\"AgentChat not available\"}";
  }

  auto* agent_service = agent_chat->GetAgentService();
  if (!agent_service) {
    return "{\"error\":\"AgentService not available\"}";
  }
  
  agent_service->SetExternalDriver([](const std::vector<yaze::cli::agent::ChatMessage>& history) {
    nlohmann::json j_history = nlohmann::json::array();
    for (const auto& msg : history) {
      j_history.push_back(MessageToJson(msg));
    }
    CallJsAiDriver(j_history.dump().c_str());
  });
  
  return "{\"success\":true}";
#else
  return "{\"error\":\"Agent UI disabled in build\"}";
#endif
}

// Handle response from JS
void onExternalAiResponse(std::string response_json) {
  auto* manager = GetEditorManager();
  if (!manager) return;
  auto* agent_ui = manager->GetAgentUiController();
  if (!agent_ui) return;

#if defined(YAZE_BUILD_AGENT_UI)
  auto& sessions = agent_ui->GetSessionManager();
  auto* session = sessions.GetSession("default");
  if (!session) return;

  // Access agent service through AgentEditor -> AgentChat
  auto* agent_editor = manager->GetAgentEditor();
  if (!agent_editor) return;

  auto* agent_chat = agent_editor->GetAgentChat();
  if (!agent_chat) return;

  auto* agent_service = agent_chat->GetAgentService();
  if (!agent_service) return;

  try {
    auto j = nlohmann::json::parse(response_json);
    yaze::cli::AgentResponse response;
    
    if (j.contains("text")) response.text_response = j["text"].get<std::string>();
    
    // Parse tool calls
    if (j.contains("tool_calls")) {
        for (const auto& tc : j["tool_calls"]) {
            yaze::cli::ToolCall call;
            call.tool_name = tc["name"].get<std::string>();
            if (tc.contains("args")) {
                for (const auto& [k, v] : tc["args"].items()) {
                    call.args[k] = v.is_string() ? v.get<std::string>() : v.dump();
                }
            }
            response.tool_calls.push_back(call);
        }
    }
    
    agent_service->HandleExternalResponse(response);
    
  } catch (const std::exception& e) {
    printf("Error parsing AI response: %s\n", e.what());
  }
#endif
}

// =============================================================================
// Async Operation Tracking
// =============================================================================

// Operation status for async API calls
std::atomic<uint32_t> g_operation_counter{0};
std::unordered_map<uint32_t, std::string> g_pending_operations;

// Parse editor name to EditorType - supports all editor types
yaze::editor::EditorType ParseEditorType(const std::string& name) {
  static const std::unordered_map<std::string, yaze::editor::EditorType>
      kEditorMap = {
          {"Assembly", yaze::editor::EditorType::kAssembly},
          {"Dungeon", yaze::editor::EditorType::kDungeon},
          {"Emulator", yaze::editor::EditorType::kEmulator},
          {"Graphics", yaze::editor::EditorType::kGraphics},
          {"Music", yaze::editor::EditorType::kMusic},
          {"Overworld", yaze::editor::EditorType::kOverworld},
          {"Palette", yaze::editor::EditorType::kPalette},
          {"Screen", yaze::editor::EditorType::kScreen},
          {"Sprite", yaze::editor::EditorType::kSprite},
          {"Message", yaze::editor::EditorType::kMessage},
          {"Text", yaze::editor::EditorType::kMessage},  // Alias
          {"Hex", yaze::editor::EditorType::kHex},
          {"Agent", yaze::editor::EditorType::kAgent},
          {"Settings", yaze::editor::EditorType::kSettings},
      };
  auto it = kEditorMap.find(name);
  return it != kEditorMap.end() ? it->second
                                : yaze::editor::EditorType::kUnknown;
}

}  // namespace

// =============================================================================
// Editor State Functions
// =============================================================================

std::string getEditorState() {
  std::ostringstream json;
  auto* manager = yaze::app::GetGlobalEditorManager();
  
  if (!manager) {
    return "{\"error\":\"EditorManager not available\"}";
  }
  
  auto* editor = manager->GetCurrentEditor();
  
  json << "{";
  json << "\"active_editor\":\"" << (editor ? yaze::editor::kEditorNames[(int)editor->type()] : "None") << "\",";
  json << "\"session_id\":" << manager->GetCurrentSessionId() << ",";
  json << "\"rom_loaded\":" << (manager->GetCurrentRom() && manager->GetCurrentRom()->is_loaded() ? "true" : "false");
  
  if (editor && editor->type() == yaze::editor::EditorType::kDungeon) {
     // We can't easily cast to DungeonEditorV2 here without circular deps or massive includes
     // But we can check if it exposes state via base class if we added virtuals? No.
     // For now, just knowing it's the Dungeon Editor is a big help.
  }
  
  json << "}";
  return json.str();
}

std::string executeCommand(std::string command) {
  // Wrapper around Z3edProcessCommand for easier JS usage
  const char* result = Z3edProcessCommand(command.c_str());
  return result ? std::string(result) : "";
}

std::string switchToEditor(std::string editor_name) {
  auto* manager = yaze::app::GetGlobalEditorManager();
  if (!manager) {
    return "{\"error\":\"EditorManager not available\"}";
  }

  // Parse editor type using the helper that supports all editors
  auto editor_type = ParseEditorType(editor_name);
  if (editor_type == yaze::editor::EditorType::kUnknown) {
    return "{\"error\":\"Unknown editor name. Valid names: Assembly, Dungeon, "
           "Emulator, Graphics, Music, Overworld, Palette, Screen, Sprite, "
           "Message, Text, Hex, Agent, Settings\"}";
  }

  // Check if ROM is loaded (required for most editors)
  auto* rom = manager->GetCurrentRom();
  if (!rom || !rom->is_loaded()) {
    // Some editors don't require ROM (Settings, Agent)
    if (editor_type != yaze::editor::EditorType::kSettings &&
        editor_type != yaze::editor::EditorType::kAgent) {
      return "{\"error\":\"ROM not loaded\",\"editor\":\"" + editor_name +
             "\"}";
    }
  }

  manager->SwitchToEditor(editor_type);
  return "{\"success\":true,\"editor\":\"" + editor_name +
         "\",\"note\":\"Action may be deferred to next frame\"}";
}

// =============================================================================
// Async Editor Switching API
// =============================================================================

/**
 * @brief Switch to an editor asynchronously with operation tracking
 *
 * Returns an operation ID that can be polled with getOperationStatus().
 * The operation completes on the next ImGui frame when the deferred action
 * executes.
 *
 * @param editor_name Name of the editor to switch to
 * @return JSON with op_id and initial status, or error
 */
std::string switchToEditorAsync(std::string editor_name) {
  uint32_t op_id = g_operation_counter++;

  auto* manager = yaze::app::GetGlobalEditorManager();
  if (!manager) {
    return "{\"error\":\"EditorManager not available\",\"op_id\":" +
           std::to_string(op_id) + "}";
  }

  // Parse editor type
  auto editor_type = ParseEditorType(editor_name);
  if (editor_type == yaze::editor::EditorType::kUnknown) {
    return "{\"error\":\"Unknown editor name\",\"op_id\":" +
           std::to_string(op_id) + "}";
  }

  // Check ROM loaded (except for Settings/Agent)
  auto* rom = manager->GetCurrentRom();
  if (!rom || !rom->is_loaded()) {
    if (editor_type != yaze::editor::EditorType::kSettings &&
        editor_type != yaze::editor::EditorType::kAgent) {
      return "{\"error\":\"ROM not loaded\",\"op_id\":" +
             std::to_string(op_id) + "}";
    }
  }

  // Mark operation as pending
  g_pending_operations[op_id] = "pending";

  // Queue the deferred action with completion callback
  manager->QueueDeferredAction([op_id, editor_type, editor_name, manager]() {
    // Check EditorSet exists
    auto* editor_set = manager->GetCurrentEditorSet();
    if (!editor_set) {
      g_pending_operations[op_id] = "error:No editor set available";
      return;
    }

    // Perform the switch
    manager->SwitchToEditor(editor_type);
    g_pending_operations[op_id] = "completed:" + editor_name;
  });

  return "{\"op_id\":" + std::to_string(op_id) +
         ",\"status\":\"pending\",\"editor\":\"" + editor_name + "\"}";
}

/**
 * @brief Get the status of an async operation
 *
 * @param op_id The operation ID returned by switchToEditorAsync
 * @return JSON with status: "pending", "completed:EditorName", or "error:message"
 */
std::string getOperationStatus(uint32_t op_id) {
  auto it = g_pending_operations.find(op_id);
  if (it == g_pending_operations.end()) {
    return "{\"error\":\"Unknown operation ID\",\"op_id\":" +
           std::to_string(op_id) + "}";
  }

  const std::string& status = it->second;
  std::ostringstream json;
  json << "{\"op_id\":" << op_id << ",";

  if (status == "pending") {
    json << "\"status\":\"pending\"}";
  } else if (status.rfind("completed:", 0) == 0) {
    // Extract editor name from "completed:EditorName"
    std::string editor = status.substr(10);
    json << "\"status\":\"completed\",\"editor\":\"" << editor << "\"}";
    // Clean up completed operations (keep last 100)
    if (g_pending_operations.size() > 100) {
      // Simple cleanup: remove this one since it's done
      g_pending_operations.erase(it);
    }
  } else if (status.rfind("error:", 0) == 0) {
    // Extract error message from "error:message"
    std::string error_msg = status.substr(6);
    json << "\"status\":\"error\",\"error\":\"" << error_msg << "\"}";
    g_pending_operations.erase(it);
  } else {
    json << "\"status\":\"" << status << "\"}";
  }

  return json.str();
}

// =============================================================================
// Panel Control API
// =============================================================================

/**
 * @brief Predefined card groups for common workflows
 */
static const std::unordered_map<std::string, std::vector<std::string>>
    kPanelGroups = {
        {"dungeon_editing",
         {"dungeon.room_selector", "dungeon.object_editor",
          "dungeon.tile_selector"}},
        {"dungeon_debug",
         {"dungeon.room_selector", "dungeon.palette_debug"}},
        {"overworld_editing",
         {"overworld.map_selector", "overworld.tile_selector",
          "overworld.entity_editor"}},
        {"graphics_editing",
         {"graphics.sheet_viewer", "graphics.tile_editor",
          "graphics.palette_editor"}},
        {"minimal", {}},  // Empty = hide all
};

/**
 * @brief Show a card by ID
 * @param card_id Panel identifier (e.g., "dungeon.room_selector")
 * @return JSON with success status
 */
std::string showPanel(std::string card_id) {
  auto* manager = yaze::app::GetGlobalEditorManager();
  if (!manager) {
    return "{\"error\":\"EditorManager not available\"}";
  }

  // Access card registry through the member function
  // Note: We need to use the public interface
  auto& card_registry = manager->card_registry();
  bool success = card_registry.ShowPanel(card_id);

  if (success) {
    return "{\"success\":true,\"card\":\"" + card_id + "\"}";
  } else {
    return "{\"error\":\"Panel not found\",\"card\":\"" + card_id + "\"}";
  }
}

/**
 * @brief Hide a card by ID
 */
std::string hidePanel(std::string card_id) {
  auto* manager = yaze::app::GetGlobalEditorManager();
  if (!manager) {
    return "{\"error\":\"EditorManager not available\"}";
  }

  auto& card_registry = manager->card_registry();
  bool success = card_registry.HidePanel(card_id);

  if (success) {
    return "{\"success\":true,\"card\":\"" + card_id + "\"}";
  } else {
    return "{\"error\":\"Panel not found\",\"card\":\"" + card_id + "\"}";
  }
}

/**
 * @brief Toggle a card's visibility
 */
std::string togglePanel(std::string card_id) {
  auto* manager = yaze::app::GetGlobalEditorManager();
  if (!manager) {
    return "{\"error\":\"EditorManager not available\"}";
  }

  auto& card_registry = manager->card_registry();

  // Get current visibility first
  bool was_visible = card_registry.IsPanelVisible(card_id);

  // Toggle
  bool success;
  if (was_visible) {
    success = card_registry.HidePanel(card_id);
  } else {
    success = card_registry.ShowPanel(card_id);
  }

  if (success) {
    return "{\"success\":true,\"card\":\"" + card_id +
           "\",\"visible\":" + (was_visible ? "false" : "true") + "}";
  } else {
    return "{\"error\":\"Panel not found\",\"card\":\"" + card_id + "\"}";
  }
}

/**
 * @brief Get the visibility state of all cards
 * @return JSON with all cards, their visibility, and categories
 */
std::string getPanelState() {
  auto* manager = yaze::app::GetGlobalEditorManager();
  if (!manager) {
    return "{\"error\":\"EditorManager not available\"}";
  }

  auto& card_registry = manager->card_registry();
  size_t session_id = manager->GetCurrentSessionId();

  std::ostringstream json;
  json << "{\"session_id\":" << session_id << ",";
  json << "\"active_category\":\"" << card_registry.GetActiveCategory() << "\",";
  json << "\"cards\":[";

  // Get all categories and iterate through cards
  auto categories = card_registry.GetAllCategories(session_id);
  bool first_card = true;

  for (const auto& category : categories) {
    auto cards = card_registry.GetPanelsInCategory(session_id, category);
    for (const auto& card : cards) {
      if (!first_card) json << ",";
      first_card = false;

      json << "{";
      json << "\"id\":\"" << card.card_id << "\",";
      json << "\"name\":\"" << card.display_name << "\",";
      json << "\"category\":\"" << card.category << "\",";
      json << "\"visible\":"
           << (card_registry.IsPanelVisible(session_id, card.card_id) ? "true"
                                                                     : "false");
      json << "}";
    }
  }

  json << "]}";
  return json.str();
}

/**
 * @brief Get cards in a specific category
 */
std::string getPanelsInCategory(std::string category) {
  auto* manager = yaze::app::GetGlobalEditorManager();
  if (!manager) {
    return "{\"error\":\"EditorManager not available\"}";
  }

  auto& card_registry = manager->card_registry();
  size_t session_id = manager->GetCurrentSessionId();
  auto cards = card_registry.GetPanelsInCategory(session_id, category);

  std::ostringstream json;
  json << "{\"category\":\"" << category << "\",\"cards\":[";

  bool first = true;
  for (const auto& card : cards) {
    if (!first) json << ",";
    first = false;

    json << "{";
    json << "\"id\":\"" << card.card_id << "\",";
    json << "\"name\":\"" << card.display_name << "\",";
    json << "\"visible\":"
         << (card_registry.IsPanelVisible(session_id, card.card_id) ? "true"
                                                                   : "false");
    json << "}";
  }

  json << "]}";
  return json.str();
}

/**
 * @brief Show a predefined group of cards
 */
std::string showPanelGroup(std::string group_name) {
  auto* manager = yaze::app::GetGlobalEditorManager();
  if (!manager) {
    return "{\"error\":\"EditorManager not available\"}";
  }

  auto it = kPanelGroups.find(group_name);
  if (it == kPanelGroups.end()) {
    std::ostringstream groups;
    groups << "dungeon_editing, dungeon_debug, overworld_editing, "
              "graphics_editing, minimal";
    return "{\"error\":\"Unknown group. Available: " + groups.str() + "\"}";
  }

  auto& card_registry = manager->card_registry();
  const auto& card_ids = it->second;

  // Show all cards in the group
  int shown = 0;
  for (const auto& card_id : card_ids) {
    if (card_registry.ShowPanel(card_id)) {
      shown++;
    }
  }

  return "{\"success\":true,\"group\":\"" + group_name +
         "\",\"cards_shown\":" + std::to_string(shown) + "}";
}

/**
 * @brief Hide a predefined group of cards
 */
std::string hidePanelGroup(std::string group_name) {
  auto* manager = yaze::app::GetGlobalEditorManager();
  if (!manager) {
    return "{\"error\":\"EditorManager not available\"}";
  }

  auto it = kPanelGroups.find(group_name);
  if (it == kPanelGroups.end()) {
    return "{\"error\":\"Unknown group\"}";
  }

  auto& card_registry = manager->card_registry();
  const auto& card_ids = it->second;

  // Special case: "minimal" hides all cards in active session
  if (group_name == "minimal") {
    card_registry.HideAll();
    return "{\"success\":true,\"group\":\"minimal\",\"action\":\"hid_all\"}";
  }

  // Hide all cards in the group
  int hidden = 0;
  for (const auto& card_id : card_ids) {
    if (card_registry.HidePanel(card_id)) {
      hidden++;
    }
  }

  return "{\"success\":true,\"group\":\"" + group_name +
         "\",\"cards_hidden\":" + std::to_string(hidden) + "}";
}

/**
 * @brief Get available card groups
 */
std::string getPanelGroups() {
  std::ostringstream json;
  json << "{\"groups\":[";

  bool first = true;
  for (const auto& [name, cards] : kPanelGroups) {
    if (!first) json << ",";
    first = false;

    json << "{\"name\":\"" << name << "\",\"cards\":[";
    bool first_card = true;
    for (const auto& card_id : cards) {
      if (!first_card) json << ",";
      first_card = false;
      json << "\"" << card_id << "\"";
    }
    json << "]}";
  }

  json << "]}";
  return json.str();
}

// =============================================================================
// Sidebar View Mode Functions
// =============================================================================

/**
 * @brief Check if tree view mode is enabled
 */
bool isTreeViewMode() {
  auto* editor_manager = GetEditorManager();
  if (!editor_manager) return false;
  // Map legacy \"tree\" terminology to the new expanded side panel state
  return editor_manager->card_registry().IsPanelExpanded();
}

/**
 * @brief Set tree view mode
 */
std::string setTreeViewMode(bool enabled) {
  auto* editor_manager = GetEditorManager();
  if (!editor_manager) {
    return R"({"success":false,"error":"Editor manager not available"})";
  }
  // Tree mode previously meant expanded sidebar; map to panel expansion
  editor_manager->card_registry().SetPanelExpanded(enabled);
  return enabled ? R"({"success":true,"mode":"tree"})"
                 : R"({"success":true,"mode":"icon"})";
}

/**
 * @brief Toggle tree view mode
 */
std::string toggleTreeViewMode() {
  auto* editor_manager = GetEditorManager();
  if (!editor_manager) {
    return R"({"success":false,"error":"Editor manager not available"})";
  }
  auto& registry = editor_manager->card_registry();
  registry.TogglePanelExpanded();
  bool is_expanded = registry.IsPanelExpanded();
  return is_expanded ? R"({"success":true,"mode":"tree"})"
                     : R"({"success":true,"mode":"icon"})";
}

/**
 * @brief Get sidebar state including view mode and width
 */
std::string getSidebarState() {
  auto* editor_manager = GetEditorManager();
  if (!editor_manager) {
    return R"({"available":false})";
  }

  auto& registry = editor_manager->card_registry();
  bool is_expanded = registry.IsPanelExpanded();
  bool is_visible = registry.IsSidebarVisible();

  float width = 0.0f;
  if (is_visible) {
    width = yaze::editor::PanelManager::GetSidebarWidth();
    if (is_expanded) {
      width += yaze::editor::PanelManager::GetSidePanelWidth();
    }
  }

  std::ostringstream json;
  json << "{\"available\":true,";
  json << "\"mode\":\"" << (is_expanded ? "tree" : "icon") << "\",";
  json << "\"width\":" << width << ",";
  json << "\"collapsed\":" << (is_visible ? "false" : "true") << "}";
  return json.str();
}

// =============================================================================
// Right Panel Functions
// =============================================================================

/**
 * @brief Open a specific right panel
 */
std::string openRightPanel(std::string panel_name) {
  auto* editor_manager = GetEditorManager();
  if (!editor_manager) {
    return R"({"success":false,"error":"Editor manager not available"})";
  }

  auto* right_panel = editor_manager->right_panel_manager();
  if (!right_panel) {
    return R"({"success":false,"error":"Right panel manager not available"})";
  }

  using PanelType = yaze::editor::RightPanelManager::PanelType;
  PanelType type = PanelType::kNone;

  if (panel_name == "properties") {
    type = PanelType::kProperties;
  } else if (panel_name == "agent" || panel_name == "chat") {
    type = PanelType::kAgentChat;
  } else if (panel_name == "proposals") {
    type = PanelType::kProposals;
  } else if (panel_name == "settings") {
    type = PanelType::kSettings;
  } else if (panel_name == "help") {
    type = PanelType::kHelp;
  } else {
    return R"({"success":false,"error":"Unknown panel type"})";
  }

  right_panel->OpenPanel(type);
  return R"({"success":true,"panel":")" + panel_name + R"("})";
}

/**
 * @brief Close the currently open right panel
 */
std::string closeRightPanel() {
  auto* editor_manager = GetEditorManager();
  if (!editor_manager) {
    return R"({"success":false,"error":"Editor manager not available"})";
  }

  auto* right_panel = editor_manager->right_panel_manager();
  if (!right_panel) {
    return R"({"success":false,"error":"Right panel manager not available"})";
  }

  right_panel->ClosePanel();
  return R"({"success":true})";
}

/**
 * @brief Toggle a specific right panel
 */
std::string toggleRightPanel(std::string panel_name) {
  auto* editor_manager = GetEditorManager();
  if (!editor_manager) {
    return R"({"success":false,"error":"Editor manager not available"})";
  }

  auto* right_panel = editor_manager->right_panel_manager();
  if (!right_panel) {
    return R"({"success":false,"error":"Right panel manager not available"})";
  }

  using PanelType = yaze::editor::RightPanelManager::PanelType;
  PanelType type = PanelType::kNone;

  if (panel_name == "properties") {
    type = PanelType::kProperties;
  } else if (panel_name == "agent" || panel_name == "chat") {
    type = PanelType::kAgentChat;
  } else if (panel_name == "proposals") {
    type = PanelType::kProposals;
  } else if (panel_name == "settings") {
    type = PanelType::kSettings;
  } else if (panel_name == "help") {
    type = PanelType::kHelp;
  } else {
    return R"({"success":false,"error":"Unknown panel type"})";
  }

  right_panel->TogglePanel(type);
  bool is_open = right_panel->IsPanelActive(type);
  return is_open ? R"({"success":true,"state":"open","panel":")" + panel_name +
                       R"("})"
                 : R"({"success":true,"state":"closed"})";
}

/**
 * @brief Get the state of the right panel
 */
std::string getRightPanelState() {
  auto* editor_manager = GetEditorManager();
  if (!editor_manager) {
    return R"({"available":false})";
  }

  auto* right_panel = editor_manager->right_panel_manager();
  if (!right_panel) {
    return R"({"available":false})";
  }

  using PanelType = yaze::editor::RightPanelManager::PanelType;
  auto active = right_panel->GetActivePanel();

  std::string active_name = "none";
  switch (active) {
    case PanelType::kProperties:
      active_name = "properties";
      break;
    case PanelType::kAgentChat:
      active_name = "agent";
      break;
    case PanelType::kProposals:
      active_name = "proposals";
      break;
    case PanelType::kSettings:
      active_name = "settings";
      break;
    case PanelType::kHelp:
      active_name = "help";
      break;
    default:
      break;
  }

  std::ostringstream json;
  json << "{\"available\":true,";
  json << "\"active\":\"" << active_name << "\",";
  json << "\"expanded\":" << (right_panel->IsPanelExpanded() ? "true" : "false")
       << ",";
  json << "\"width\":" << right_panel->GetPanelWidth() << "}";
  return json.str();
}

// =============================================================================
// Palette Debug Functions
// =============================================================================

std::string getDungeonPaletteEvents() {
  return yaze::zelda3::PaletteDebugger::Get().ExportToJSON();
}

std::string getColorComparisons() {
  return yaze::zelda3::PaletteDebugger::Get().ExportColorComparisonsJSON();
}

std::string samplePixelAt(int x, int y) {
  return yaze::zelda3::PaletteDebugger::Get().SamplePixelJSON(x, y);
}

void clearPaletteDebugEvents() {
  yaze::zelda3::PaletteDebugger::Get().Clear();
}

// AI analysis functions for Gemini/Antigravity integration
std::string getFullPaletteState() {
  return yaze::zelda3::PaletteDebugger::Get().ExportFullStateJSON();
}

std::string getPaletteData() {
  return yaze::zelda3::PaletteDebugger::Get().ExportPaletteDataJSON();
}

std::string getEventTimeline() {
  return yaze::zelda3::PaletteDebugger::Get().ExportTimelineJSON();
}

std::string getDiagnosticSummary() {
  return yaze::zelda3::PaletteDebugger::Get().GetDiagnosticSummary();
}

std::string getHypothesisAnalysis() {
  return yaze::zelda3::PaletteDebugger::Get().GetHypothesisAnalysis();
}

// =============================================================================
// Graphics Arena Debug Functions
// =============================================================================

std::string getArenaStatus() {
  std::ostringstream json;
  auto& arena = yaze::gfx::Arena::Get();

  json << "{";
  json << "\"texture_queue_size\":" << arena.texture_command_queue_size()
       << ",";

  // Get info about graphics sheets
  json << "\"gfx_sheets\":[";
  bool first = true;
  for (int i = 0; i < 223; i++) {
    auto sheet = arena.gfx_sheet(i);  // Returns by value
    if (sheet.is_active()) {
      if (!first) json << ",";
      json << "{\"index\":" << i << ",\"width\":" << sheet.width()
           << ",\"height\":" << sheet.height()
           << ",\"has_texture\":" << (sheet.texture() != nullptr ? "true" : "false")
           << ",\"has_surface\":" << (sheet.surface() != nullptr ? "true" : "false")
           << "}";
      first = false;
    }
  }
  json << "]";
  json << "}";

  return json.str();
}

std::string getGfxSheetInfo(int index) {
  if (index < 0 || index >= 223) {
    return "{\"error\": \"Invalid sheet index\"}";
  }

  std::ostringstream json;
  auto& arena = yaze::gfx::Arena::Get();
  auto sheet = arena.gfx_sheet(index);  // Returns by value

  json << "{";
  json << "\"index\":" << index << ",";
  json << "\"active\":" << (sheet.is_active() ? "true" : "false") << ",";
  json << "\"width\":" << sheet.width() << ",";
  json << "\"height\":" << sheet.height() << ",";
  json << "\"has_texture\":" << (sheet.texture() != nullptr ? "true" : "false")
       << ",";
  json << "\"has_surface\":" << (sheet.surface() != nullptr ? "true" : "false");

  // If surface exists, get palette info
  if (sheet.surface() && sheet.surface()->format) {
    auto* fmt = sheet.surface()->format;
    json << ",\"surface_format\":" << fmt->format;
    if (fmt->palette) {
      json << ",\"palette_colors\":" << fmt->palette->ncolors;
    }
  }

  json << "}";
  return json.str();
}

// =============================================================================
// ROM Debug Functions
// =============================================================================

std::string getRomStatus() {
  std::ostringstream json;
  auto* rom = yaze::cli::GetGlobalRom();

  json << "{";

  if (!rom) {
    json << "\"loaded\":false,\"error\":\"No ROM loaded\"";
  } else {
    json << "\"loaded\":" << (rom->is_loaded() ? "true" : "false") << ",";
    json << "\"size\":" << rom->size() << ",";
    json << "\"title\":\"" << rom->title() << "\"";
  }

  json << "}";
  return json.str();
}

std::string readRomBytes(int address, int count) {
  std::ostringstream json;
  auto* rom = yaze::cli::GetGlobalRom();

  if (!rom || !rom->is_loaded()) {
    return "{\"error\":\"No ROM loaded\"}";
  }

  if (count > 256) count = 256;  // Limit to prevent huge responses
  if (count < 1) count = 1;

  json << "{\"address\":" << address << ",\"count\":" << count << ",\"bytes\":[";

  for (int i = 0; i < count; i++) {
    if (i > 0) json << ",";
    auto byte_result = rom->ReadByte(address + i);
    if (byte_result.ok()) {
      json << static_cast<int>(*byte_result);
    } else {
      json << "null";
    }
  }

  json << "]}";
  return json.str();
}

std::string getRomPaletteGroup(const std::string& group_name, int palette_index) {
  std::ostringstream json;
  auto* rom = yaze::cli::GetGlobalRom();

  if (!rom || !rom->is_loaded()) {
    return "{\"error\":\"No ROM loaded\"}";
  }

  json << "{\"group_name\":\"" << group_name << "\",\"palette_index\":" << palette_index;

  // Get palette colors from GameData
  try {
    yaze::zelda3::GameData game_data;
    auto load_status = yaze::zelda3::LoadGameData(*rom, game_data);
    if (!load_status.ok()) {
      return "{\"error\":\"Failed to load game data\"}";
    }
    auto* group = game_data.palette_groups.get_group(group_name);
    if (group) {
      if (palette_index >= 0 && palette_index < static_cast<int>(group->size())) {
        auto palette = (*group)[palette_index];
        json << ",\"size\":" << palette.size();
        json << ",\"colors\":[";
        for (size_t i = 0; i < palette.size(); i++) {
          if (i > 0) json << ",";
          auto rgb = palette[i].rgb();
          json << "{\"r\":" << static_cast<int>(rgb.x)
               << ",\"g\":" << static_cast<int>(rgb.y)
               << ",\"b\":" << static_cast<int>(rgb.z) << "}";
        }
        json << "]";
      } else {
        json << ",\"error\":\"Invalid palette index\"";
      }
    } else {
      json << ",\"error\":\"Invalid group name. Valid names: ow_main, ow_aux, ow_animated, hud, global_sprites, armors, swords, shields, sprites_aux1, sprites_aux2, sprites_aux3, dungeon_main, grass, 3d_object, ow_mini_map\"";
    }
  } catch (...) {
    json << ",\"error\":\"Exception accessing palette\"";
  }

  json << "}";
  return json.str();
}

// =============================================================================
// Overworld Debug Functions
// =============================================================================

std::string getOverworldMapInfo(int map_id) {
  std::ostringstream json;
  auto* rom = yaze::cli::GetGlobalRom();

  if (!rom || !rom->is_loaded()) {
    return "{\"error\":\"No ROM loaded\"}";
  }

  // Overworld map constants
  constexpr int kNumOverworldMaps = 160;
  if (map_id < 0 || map_id >= kNumOverworldMaps) {
    return "{\"error\":\"Invalid map ID (0-159)\"}";
  }

  json << "{\"map_id\":" << map_id;

  // Read map properties from known ROM addresses
  // Map size: 0x12844 + map_id
  auto size_byte = rom->ReadByte(0x12844 + map_id);
  if (size_byte.ok()) {
    json << ",\"size_flag\":" << static_cast<int>(*size_byte);
    json << ",\"is_large\":" << (*size_byte == 0x20 ? "true" : "false");
  }

  // Parent ID: 0x125EC + map_id
  auto parent_byte = rom->ReadByte(0x125EC + map_id);
  if (parent_byte.ok()) {
    json << ",\"parent_id\":" << static_cast<int>(*parent_byte);
  }

  // Determine world type
  if (map_id < 64) {
    json << ",\"world\":\"light\"";
  } else if (map_id < 128) {
    json << ",\"world\":\"dark\"";
  } else {
    json << ",\"world\":\"special\"";
  }

  json << "}";
  return json.str();
}

std::string getOverworldTileInfo(int map_id, int tile_x, int tile_y) {
  std::ostringstream json;
  auto* rom = yaze::cli::GetGlobalRom();

  if (!rom || !rom->is_loaded()) {
    return "{\"error\":\"No ROM loaded\"}";
  }

  json << "{\"map_id\":" << map_id
       << ",\"tile_x\":" << tile_x
       << ",\"tile_y\":" << tile_y;

  // Note: Full tile data access would require loading the overworld
  // For now, provide basic info that can be accessed without full load
  json << ",\"note\":\"Full tile data requires overworld to be loaded in editor\"";

  json << "}";
  return json.str();
}

// =============================================================================
// Emulator Debug Functions
// =============================================================================

/**
 * @brief Get the current emulator status including CPU state
 *
 * Returns JSON with:
 * - initialized: whether the emulator is ready
 * - running: whether the emulator is currently executing
 * - cpu: register values (A, X, Y, SP, PC, D, DB, PB, P/status)
 * - cycles: total cycles executed
 * - fps: current frames per second
 *
 * CPU status flags (P register):
 * - N (0x80): Negative
 * - V (0x40): Overflow
 * - M (0x20): Accumulator size (0=16-bit, 1=8-bit)
 * - X (0x10): Index size (0=16-bit, 1=8-bit)
 * - D (0x08): Decimal mode
 * - I (0x04): IRQ disable
 * - Z (0x02): Zero
 * - C (0x01): Carry
 * - E: Emulation mode (6502 compatibility)
 */
std::string getEmulatorStatus() {
  std::ostringstream json;
  auto* emulator = GetGlobalEmulator();

  json << "{";

  if (!emulator) {
    json << "\"initialized\":false,\"error\":\"Emulator not available\"";
    json << "}";
    return json.str();
  }

  bool is_initialized = emulator->is_snes_initialized();
  bool is_running = emulator->running();

  json << "\"initialized\":" << (is_initialized ? "true" : "false") << ",";
  json << "\"running\":" << (is_running ? "true" : "false") << ",";

  if (is_initialized) {
    auto& snes = emulator->snes();
    auto& cpu = snes.cpu();

    // CPU registers
    json << "\"cpu\":{";
    json << "\"A\":" << cpu.A << ",";
    json << "\"X\":" << cpu.X << ",";
    json << "\"Y\":" << cpu.Y << ",";
    json << "\"SP\":" << cpu.SP() << ",";
    json << "\"PC\":" << cpu.PC << ",";
    json << "\"D\":" << cpu.D << ",";       // Direct page register
    json << "\"DB\":" << (int)cpu.DB << ","; // Data bank register
    json << "\"PB\":" << (int)cpu.PB << ","; // Program bank register
    json << "\"P\":" << (int)cpu.status << ","; // Processor status
    json << "\"E\":" << (int)cpu.E << ",";   // Emulation mode flag

    // Decode status flags for convenience
    json << "\"flags\":{";
    json << "\"N\":" << (cpu.GetNegativeFlag() ? "true" : "false") << ",";
    json << "\"V\":" << (cpu.GetOverflowFlag() ? "true" : "false") << ",";
    json << "\"M\":" << (cpu.GetAccumulatorSize() ? "true" : "false") << ",";
    json << "\"X\":" << (cpu.GetIndexSize() ? "true" : "false") << ",";
    json << "\"D\":" << (cpu.GetDecimalFlag() ? "true" : "false") << ",";
    json << "\"I\":" << (cpu.GetInterruptFlag() ? "true" : "false") << ",";
    json << "\"Z\":" << (cpu.GetZeroFlag() ? "true" : "false") << ",";
    json << "\"C\":" << (cpu.GetCarryFlag() ? "true" : "false");
    json << "}";
    json << "},";

    // Full 24-bit PC address
    uint32_t full_pc = ((uint32_t)cpu.PB << 16) | cpu.PC;
    json << "\"full_pc\":" << full_pc << ",";
    json << "\"full_pc_hex\":\"$" << std::hex << std::uppercase
         << std::setfill('0') << std::setw(6) << full_pc << std::dec << "\",";

    // Timing information
    json << "\"cycles\":" << snes.mutable_cycles() << ",";
    json << "\"fps\":" << emulator->GetCurrentFPS();
  }

  json << "}";
  return json.str();
}

/**
 * @brief Read memory from the emulator's WRAM
 *
 * @param address Starting address (SNES address space, e.g., 0x7E0000 for WRAM)
 * @param count Number of bytes to read (max 256)
 * @return JSON with address, count, and bytes array
 *
 * Memory map reference:
 * - $7E0000-$7E1FFF: Low RAM (first 8KB, mirrored in banks $00-$3F)
 * - $7E2000-$7FFFFF: High RAM (additional ~120KB)
 * - $7F0000-$7FFFFF: Extended RAM (second 64KB bank)
 */
std::string readEmulatorMemory(int address, int count) {
  std::ostringstream json;
  auto* emulator = GetGlobalEmulator();

  if (!emulator || !emulator->is_snes_initialized()) {
    return "{\"error\":\"Emulator not initialized\"}";
  }

  // Clamp count to prevent huge responses
  if (count > 256) count = 256;
  if (count < 1) count = 1;

  auto& snes = emulator->snes();

  json << "{\"address\":" << address << ",";
  json << "\"address_hex\":\"$" << std::hex << std::uppercase
       << std::setfill('0') << std::setw(6) << address << std::dec << "\",";
  json << "\"count\":" << count << ",";
  json << "\"bytes\":[";

  // Read bytes from emulator memory using Snes::Read
  // This respects SNES memory mapping
  for (int i = 0; i < count; i++) {
    if (i > 0) json << ",";
    uint8_t byte = snes.Read(address + i);
    json << static_cast<int>(byte);
  }

  json << "],\"hex\":\"";
  // Also provide hex string representation
  for (int i = 0; i < count; i++) {
    uint8_t byte = snes.Read(address + i);
    json << std::hex << std::uppercase << std::setfill('0') << std::setw(2)
         << static_cast<int>(byte);
  }
  json << std::dec << "\"}";

  return json.str();
}

/**
 * @brief Get PPU (video) state from the emulator
 *
 * Returns JSON with:
 * - current_scanline: Current rendering scanline
 * - h_pos / v_pos: Horizontal/vertical position
 * - mode: Current BG mode (0-7)
 * - brightness: Screen brightness level
 * - forced_blank: Whether screen is blanked
 * - overscan: Whether overscan is enabled
 * - interlace: Whether interlacing is enabled
 */
std::string getEmulatorVideoState() {
  std::ostringstream json;
  auto* emulator = GetGlobalEmulator();

  if (!emulator || !emulator->is_snes_initialized()) {
    return "{\"error\":\"Emulator not initialized\"}";
  }

  auto& snes = emulator->snes();
  auto& ppu = snes.ppu();
  auto& memory = snes.memory();

  json << "{";

  // Scanline and position info from memory interface
  json << "\"h_pos\":" << memory.h_pos() << ",";
  json << "\"v_pos\":" << memory.v_pos() << ",";
  json << "\"current_scanline\":" << ppu.current_scanline_ << ",";

  // PPU mode and settings
  json << "\"mode\":" << (int)ppu.mode << ",";
  json << "\"brightness\":" << (int)ppu.brightness << ",";
  json << "\"forced_blank\":" << (ppu.forced_blank_ ? "true" : "false") << ",";
  json << "\"overscan\":" << (ppu.overscan_ ? "true" : "false") << ",";
  json << "\"frame_overscan\":" << (ppu.frame_overscan_ ? "true" : "false") << ",";
  json << "\"interlace\":" << (ppu.interlace ? "true" : "false") << ",";
  json << "\"frame_interlace\":" << (ppu.frame_interlace ? "true" : "false") << ",";
  json << "\"pseudo_hires\":" << (ppu.pseudo_hires_ ? "true" : "false") << ",";
  json << "\"direct_color\":" << (ppu.direct_color_ ? "true" : "false") << ",";
  json << "\"bg3_priority\":" << (ppu.bg3priority ? "true" : "false") << ",";
  json << "\"even_frame\":" << (ppu.even_frame ? "true" : "false") << ",";

  // VRAM pointer info
  json << "\"vram_pointer\":" << ppu.vram_pointer << ",";
  json << "\"vram_increment\":" << ppu.vram_increment_ << ",";
  json << "\"vram_increment_on_high\":" << (ppu.vram_increment_on_high_ ? "true" : "false");

  json << "}";
  return json.str();
}

// =============================================================================
// Version and Session Management
// =============================================================================

std::string getYazeVersion() {
  return YAZE_VERSION_STRING;
}

std::string getRomSessions() {
  std::ostringstream json;
  auto* manager = yaze::app::GetGlobalEditorManager();

  if (!manager) {
    return "{\"error\":\"EditorManager not available\",\"sessions\":[]}";
  }

  json << "{";
  json << "\"current_session\":" << manager->GetCurrentSessionId() << ",";

  // Get current ROM info
  auto* current_rom = manager->GetCurrentRom();
  if (current_rom && current_rom->is_loaded()) {
    json << "\"current_rom\":{";
    json << "\"loaded\":true,";
    json << "\"title\":\"" << current_rom->title() << "\",";
    json << "\"filename\":\"" << current_rom->filename() << "\",";
    json << "\"size\":" << current_rom->size();
    json << "},";
  } else {
    json << "\"current_rom\":{\"loaded\":false},";
  }

  // Note: Full session enumeration would require exposing session_coordinator
  // For now, provide current session info
  json << "\"sessions\":[]";

  json << "}";
  return json.str();
}

std::string getFileManagerDebugInfo() {
  std::ostringstream json;
  auto* rom = yaze::cli::GetGlobalRom();

  json << "{";
  json << "\"global_rom_ptr\":" << (rom ? "true" : "false") << ",";

  if (rom) {
    json << "\"rom_loaded\":" << (rom->is_loaded() ? "true" : "false") << ",";
    json << "\"rom_size\":" << rom->size() << ",";
    json << "\"rom_filename\":\"" << rom->filename() << "\",";
    json << "\"rom_title\":\"" << rom->title() << "\",";

    // // Add diagnostics if available
    // if (rom->is_loaded()) {
    //   auto& diag = rom->GetDiagnostics();
    //   json << "\"diagnostics\":{";
    //   json << "\"header_stripped\":" << (diag.header_stripped ? "true" : "false") << ",";
    //   json << "\"checksum_valid\":" << (diag.checksum_valid ? "true" : "false") << ",";
    //   json << "\"sheets_loaded\":" << diag.sheets.size();
    //   json << "}";
    // }
  }

  // EditorManager info
  auto* manager = yaze::app::GetGlobalEditorManager();
  if (manager) {
    json << ",\"editor_manager\":{";
    json << "\"session_count\":" << manager->GetActiveSessionCount() << ",";
    json << "\"current_session\":" << manager->GetCurrentSessionId() << ",";
    json << "\"has_current_rom\":" << (manager->GetCurrentRom() ? "true" : "false");
    json << "}";
  } else {
    json << ",\"editor_manager\":null";
  }

  json << "}";
  return json.str();
}

void resumeAudioContext() {
  auto* emulator = GetGlobalEmulator();
  if (emulator) {
    emulator->ResumeAudio();
  }
}

// =============================================================================
// Combined Debug State for AI Analysis
// =============================================================================

std::string getGraphicsDiagnostics() {
  auto* rom = yaze::cli::GetGlobalRom();
  if (!rom || !rom->is_loaded()) {
    return "{\"error\":\"No ROM loaded\"}";
  }
  return "Not implemented";
  // return rom->GetDiagnostics().ToJson();
}

std::string getFullDebugState() {
  std::ostringstream json;

  json << "{";

  // Palette debug state
  json << "\"palette\":" << getFullPaletteState() << ",";

  // Arena status
  json << "\"arena\":" << getArenaStatus() << ",";

  // ROM status
  json << "\"rom\":" << getRomStatus() << ",";

  // Emulator status
  json << "\"emulator\":" << getEmulatorStatus() << ",";

  // Diagnostic summary
  json << "\"diagnostic\":\"" << getDiagnosticSummary() << "\",";

  // Hypothesis
  json << "\"hypothesis\":\"" << getHypothesisAnalysis() << "\"";

  json << "}";

  return json.str();
}

// =============================================================================
// Emscripten Bindings
// =============================================================================

EMSCRIPTEN_BINDINGS(yaze_debug_inspector) {
  // Palette debug functions
  function("getDungeonPaletteEvents", &getDungeonPaletteEvents);
  function("getColorComparisons", &getColorComparisons);
  function("samplePixelAt", &samplePixelAt);
  function("clearPaletteDebugEvents", &clearPaletteDebugEvents);

  // AI analysis functions
  function("getFullPaletteState", &getFullPaletteState);
  function("getPaletteData", &getPaletteData);
  function("getEventTimeline", &getEventTimeline);
  function("getDiagnosticSummary", &getDiagnosticSummary);
  function("getHypothesisAnalysis", &getHypothesisAnalysis);

  // Arena debug functions
  function("getArenaStatus", &getArenaStatus);
  function("getGfxSheetInfo", &getGfxSheetInfo);

  // ROM debug functions
  function("getRomStatus", &getRomStatus);
  function("readRomBytes", &readRomBytes);
  function("getRomPaletteGroup", &getRomPaletteGroup);

  // Overworld debug functions
  function("getOverworldMapInfo", &getOverworldMapInfo);
  function("getOverworldTileInfo", &getOverworldTileInfo);

  // Emulator debug functions
  function("getEmulatorStatus", &getEmulatorStatus);
  function("readEmulatorMemory", &readEmulatorMemory);
  function("getEmulatorVideoState", &getEmulatorVideoState);
  function("resumeAudioContext", &resumeAudioContext);

  // Editor state and command execution
  function("getEditorState", &getEditorState);
  function("executeCommand", &executeCommand);
  function("switchToEditor", &switchToEditor);

  // Async editor switching API
  function("switchToEditorAsync", &switchToEditorAsync);
  function("getOperationStatus", &getOperationStatus);

  // Panel control API
  function("showPanel", &showPanel);
  function("hidePanel", &hidePanel);
  function("togglePanel", &togglePanel);
  function("getPanelState", &getPanelState);
  function("getPanelsInCategory", &getPanelsInCategory);
  function("showPanelGroup", &showPanelGroup);
  function("hidePanelGroup", &hidePanelGroup);
  function("getPanelGroups", &getPanelGroups);

  // Sidebar view mode
  function("isTreeViewMode", &isTreeViewMode);
  function("setTreeViewMode", &setTreeViewMode);
  function("toggleTreeViewMode", &toggleTreeViewMode);
  function("getSidebarState", &getSidebarState);

  // Right panel control
  function("openRightPanel", &openRightPanel);
  function("closeRightPanel", &closeRightPanel);
  function("toggleRightPanel", &toggleRightPanel);
  function("getRightPanelState", &getRightPanelState);

  // Combined state for AI
  function("getFullDebugState", &getFullDebugState);
  // function("getGraphicsDiagnostics", &getGraphicsDiagnostics);

  // Version and session management
  function("getYazeVersion", &getYazeVersion);
  function("getRomSessions", &getRomSessions);
  function("getFileManagerDebugInfo", &getFileManagerDebugInfo);

  // AI Driver Bridge
  function("registerExternalAiDriver", &registerExternalAiDriver);
  function("onExternalAiResponse", &onExternalAiResponse);
}
