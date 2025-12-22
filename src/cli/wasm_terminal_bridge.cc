/**
 * @file wasm_terminal_bridge.cc
 * @brief C++ bridge for web terminal integration in WASM builds
 *
 * This file provides the C++ side of the web terminal integration,
 * exposing functions that can be called from JavaScript to process
 * z3ed commands and provide terminal functionality in the browser.
 */

#ifdef __EMSCRIPTEN__

#include <algorithm>
#include <cstring>
#include <memory>
#include <set>
#include <sstream>
#include <string>
#include <vector>
#include <fstream>

#include <emscripten.h>
#include <emscripten/bind.h>

#include "absl/status/status.h"
#include "absl/strings/str_split.h"
#include "absl/strings/str_join.h"
#include "rom/rom.h"
#include "app/net/wasm/emscripten_http_client.h"
#include "app/platform/wasm/wasm_bootstrap.h"
#include "cli/service/command_registry.h"
#include "cli/service/ai/browser_ai_service.h"
#include "cli/handlers/command_handlers.h"
#include "app/editor/editor_manager.h"
#include "app/editor/dungeon/dungeon_editor_v2.h"

namespace yaze::app {
extern editor::EditorManager* GetGlobalEditorManager();
}

namespace {

struct BridgeState {
  std::unique_ptr<yaze::cli::BrowserAIService> ai_service;
  std::string last_output;
  std::string api_key;
  bool initialized = false;

  void Initialize() {
    if (!initialized) {
      // Ensure command registry is initialized
      yaze::cli::CommandRegistry::Instance();
      initialized = true;
    }
  }

  void SetupAIService() {
    if (!api_key.empty() && !ai_service) {
      yaze::cli::BrowserAIConfig config;
      config.api_key = api_key;
      config.model = "gemini-2.0-flash-exp";
      config.verbose = false;
      auto http_client = std::make_unique<yaze::net::EmscriptenHttpClient>();
      ai_service = std::make_unique<yaze::cli::BrowserAIService>(
          config, std::move(http_client));
    }
  }

  // Helper to get the REAL active ROM from the application controller
  yaze::Rom* GetActiveRom() {
    auto* manager = yaze::app::GetGlobalEditorManager();
    if (manager) return manager->GetCurrentRom();
    return nullptr;
  }
};

static BridgeState g_bridge;

}  // namespace

// Global accessor for command handlers
namespace yaze {
namespace cli {
BrowserAIService* GetGlobalBrowserAIService() {
  return g_bridge.ai_service.get();
}

Rom* GetGlobalRom() {
  return g_bridge.GetActiveRom();
}

}  // namespace cli
}  // namespace yaze

namespace {

// JavaScript function to print to terminal
EM_JS(void, z3ed_print_to_terminal, (const char* text), {
  if (window.z3edTerminal) {
    window.z3edTerminal.print(UTF8ToString(text));
  } else {
    console.log(UTF8ToString(text));
  }
});

// JavaScript function to print error to terminal
EM_JS(void, z3ed_error_to_terminal, (const char* text), {
  if (window.z3edTerminal) {
    window.z3edTerminal.printError(UTF8ToString(text));
  } else {
    console.error(UTF8ToString(text));
  }
});

// Parse command string into arguments
std::vector<std::string> ParseCommand(const std::string& command) {
  // Simple tokenization - could be enhanced for quoted strings
  return absl::StrSplit(command, ' ', absl::SkipEmpty());
}

// Process command and return output
std::string ProcessCommandInternal(const std::string& command_str) {
  auto args = ParseCommand(command_str);
  if (args.empty()) {
    return "No command provided";
  }

  std::ostringstream output;

  // Handle special commands
  if (args[0] == "help") {
    if (args.size() > 1) {
      auto& registry = yaze::cli::CommandRegistry::Instance();
      return registry.GenerateCategoryHelp(args[1]);
    } else {
      return yaze::cli::CommandRegistry::Instance().GenerateCompleteHelp();
    }
  }

  // Handle ROM commands
  if (args[0] == "rom" && args.size() > 1) {
    if (args[1] == "load" && args.size() > 2) {
      // Trigger full application load via bootstrap
      yaze::app::wasm::TriggerRomLoad(args[2]);
      return "Requesting load for: " + args[2] + ". Check application log.";
    }
    yaze::Rom* rom = g_bridge.GetActiveRom();
    if (args[1] == "info") {
      if (rom && rom->is_loaded()) {
        output << "ROM Info:\n";
        output << "  Size: " << rom->size() << " bytes\n";
        output << "  Title: " << rom->title() << "\n";
        return output.str();
      } else {
        return "No ROM loaded.";
      }
    }
  }

  // Handle AI commands if service is available
  if (args[0] == "ai" && g_bridge.ai_service) {
    if (args.size() < 2) {
      return "AI command requires a prompt. Usage: ai <prompt>";
    }

    // Reconstruct prompt from remaining args
    std::vector<std::string> prompt_parts(args.begin() + 1, args.end());
    std::string prompt = absl::StrJoin(prompt_parts, " ");

    // Generate response using AI service
    auto response = g_bridge.ai_service->GenerateResponse(prompt);
    if (response.ok()) {
      return response->text_response;
    } else {
      return "AI error: " + std::string(response.status().message());
    }
  }

  // Handle editor commands
  if (args[0] == "editor") {
    auto* editor_manager = yaze::app::GetGlobalEditorManager();
    if (!editor_manager) return "Error: Editor manager not available";

    if (args.size() > 2 && args[1] == "switch") {
      std::string target = args[2];
      for (size_t i = 0; i < yaze::editor::kEditorNames.size(); ++i) {
        std::string name = yaze::editor::kEditorNames[i];
        std::transform(name.begin(), name.end(), name.begin(), ::tolower);
        std::transform(target.begin(), target.end(), target.begin(), ::tolower);
        if (name == target) {
          editor_manager->SwitchToEditor(static_cast<yaze::editor::EditorType>(i));
          return "Switched to " + std::string(yaze::editor::kEditorNames[i]);
        }
      }
      return "Unknown editor: " + args[2];
    }

    if (args.size() > 3 && args[1] == "card") {
      // "editor card <name> <show/hide>"
      // Name might be multi-word if parsed poorly, but let's assume simple args or quotes handled by caller (currently not)
      // For simple support: "editor card object show"
      std::string card_key = args[2];
      std::string state = args[3];
      bool visible = (state == "show" || state == "on" || state == "true");

      auto* current_editor = editor_manager->GetCurrentEditor();
      if (current_editor && current_editor->type() == yaze::editor::EditorType::kDungeon) {
        // Panel visibility is now controlled via the Layout Designer
        // These legacy panel toggles are no longer directly accessible
        if (card_key == "object" || card_key == "objects" ||
            card_key == "room" || card_key == "selector" ||
            card_key == "graphics" || card_key == "debug") {
          return "Panel visibility is now controlled via Layout Designer. Use 'editor layout' commands.";
        }
        return "Unknown card key for Dungeon Editor: " + card_key;
      }
      return "Panel control not supported for current editor";
    }

    if (args.size() > 2 && args[1] == "debug" && args[2] == "toggle") {
      // Legacy command - debug controls are now accessed via Layout Designer
      return "Debug controls are now accessed via the Layout Designer. "
             "Use the canvas debug panel or layout commands.";
    }
  }

  // Handle dungeon commands
  if (args[0] == "dungeon") {
    auto* editor_manager = yaze::app::GetGlobalEditorManager();
    if (!editor_manager) return "Error: Editor manager not available";
    
    auto* current_editor = editor_manager->GetCurrentEditor();
    if (!current_editor || current_editor->type() != yaze::editor::EditorType::kDungeon) {
      // Auto-switch if possible? Or just fail.
      return "Error: Dungeon editor is not active. Use 'editor switch dungeon' first.";
    }
    auto* dungeon_editor = static_cast<yaze::editor::DungeonEditorV2*>(current_editor);

    if (args.size() > 2 && args[1] == "room") {
      try {
        int room_id = std::stoi(args[2], nullptr, 16);
        dungeon_editor->FocusRoom(room_id);
        return "Focused room " + args[2];
      } catch (...) { return "Invalid room ID (hex required)"; }
    }

    if (args.size() > 2 && args[1] == "select_object") {
      try {
        int obj_id = std::stoi(args[2], nullptr, 16);
        dungeon_editor->SelectObject(obj_id);
        return "Selected object " + args[2];
      } catch (...) { return "Invalid object ID (hex required)"; }
    }

    if (args.size() > 2 && args[1] == "agent_mode") {
      bool enabled = (args[2] == "on" || args[2] == "true");
      dungeon_editor->SetAgentMode(enabled);
      return "Agent mode " + std::string(enabled ? "enabled" : "disabled");
    }
  }

  // Try command registry
  auto& registry = yaze::cli::CommandRegistry::Instance();
  if (registry.HasCommand(args[0])) {
    std::vector<std::string> cmd_args(args.begin() + 1, args.end());
    // Use the REAL active ROM
    std::string cmd_output;
    auto status = registry.Execute(args[0], cmd_args, g_bridge.GetActiveRom(), &cmd_output);
    if (status.ok()) {
      return cmd_output.empty() ? "Command executed successfully" : cmd_output;
    } else {
      return "Command failed: " + std::string(status.message());
    }
  }

  return "Unknown command: " + args[0] + "\nType 'help' for available commands";
}

// Get command completions for autocomplete
std::vector<std::string> GetCompletionsInternal(const std::string& partial) {
  std::vector<std::string> completions;

  // Parse the partial command to understand context
  auto parts = absl::StrSplit(partial, ' ', absl::SkipEmpty());
  std::vector<std::string> cmd_parts(parts.begin(), parts.end());

  // If empty or single word, show top-level commands
  if (cmd_parts.empty() || (cmd_parts.size() == 1 && !partial.empty() && partial.back() != ' ')) {
    std::string prefix = cmd_parts.empty() ? "" : cmd_parts[0];

    // Get all available commands from registry
    auto& registry = yaze::cli::CommandRegistry::Instance();
    auto categories = registry.GetCategories();

    std::set<std::string> unique_commands;  // Use set to avoid duplicates

    for (const auto& category : categories) {
      auto commands = registry.GetCommandsInCategory(category);
      for (const auto& cmd : commands) {
        if (prefix.empty() || cmd.find(prefix) == 0) {
          unique_commands.insert(cmd);
        }
      }
    }

    // Add special/built-in commands
    std::vector<std::string> special = {
      "help", "rom", "ai", "clear", "version", "hex", "palette", "sprite",
      "music", "dialogue", "message", "resource", "dungeon", "overworld",
      "gui", "emulator", "query", "analyze", "catalog"
    };

    for (const auto& cmd : special) {
      if (prefix.empty() || cmd.find(prefix) == 0) {
        unique_commands.insert(cmd);
      }
    }

    // Convert set to vector
    completions.assign(unique_commands.begin(), unique_commands.end());

  } else if (cmd_parts.size() >= 1) {
    // Context-specific completions based on the command
    const std::string& command = cmd_parts[0];

    if (command == "rom") {
      // ROM subcommands
      std::vector<std::string> rom_cmds = {"load", "info", "save", "stats", "verify"};
      std::string prefix = cmd_parts.size() > 1 ? cmd_parts[1] : "";
      for (const auto& subcmd : rom_cmds) {
        if (prefix.empty() || subcmd.find(prefix) == 0) {
          completions.push_back("rom " + subcmd);
        }
      }
    } 
    // ... (other completions logic can be kept or expanded via registry in future)
  }

  // Sort completions alphabetically
  std::sort(completions.begin(), completions.end());

  return completions;
}

}  // namespace

extern "C" {

/**
 * Process a z3ed command and return the result
 * @param command The command string to process
 * @return Output string (caller must not free)
 */
EMSCRIPTEN_KEEPALIVE
const char* Z3edProcessCommand(const char* command) {
  g_bridge.Initialize();

  if (!command) {
    g_bridge.last_output = "Invalid command";
    return g_bridge.last_output.c_str();
  }

  std::string command_str(command);
  g_bridge.last_output = ProcessCommandInternal(command_str);

  // Also print to terminal for visual feedback
  z3ed_print_to_terminal(g_bridge.last_output.c_str());

  return g_bridge.last_output.c_str();
}

/**
 * Get command completions for partial input
 * @param partial The partial command to complete
 * @return JSON array of completions (caller must not free)
 */
EMSCRIPTEN_KEEPALIVE
const char* Z3edGetCompletions(const char* partial) {
  g_bridge.Initialize();

  if (!partial) {
    g_bridge.last_output = "[]";
    return g_bridge.last_output.c_str();
  }

  auto completions = GetCompletionsInternal(std::string(partial));

  // Convert to JSON array
  std::ostringstream json;
  json << "[";
  for (size_t i = 0; i < completions.size(); ++i) {
    if (i > 0) json << ",";
    json << "\"" << completions[i] << "\"";
  }
  json << "]";

  g_bridge.last_output = json.str();
  return g_bridge.last_output.c_str();
}

/**
 * Set the API key for AI services
 * @param api_key The API key to use
 */
EMSCRIPTEN_KEEPALIVE
void Z3edSetApiKey(const char* api_key) {
  if (api_key) {
    g_bridge.api_key = std::string(api_key);
    g_bridge.SetupAIService();
  }
}

/**
 * Check if the terminal bridge is ready
 * @return 1 if ready, 0 otherwise
 */
EMSCRIPTEN_KEEPALIVE
int Z3edIsReady() {
  if (!g_bridge.initialized) {
    g_bridge.Initialize();
  }
  return g_bridge.initialized ? 1 : 0;
}

/**
 * Load a ROM file from a URL or ArrayBuffer
 * @param data The ROM data as bytes
 * @param size The size of the ROM data
 * @return 1 on success, 0 on failure
 */
EMSCRIPTEN_KEEPALIVE
int Z3edLoadRomData(const uint8_t* data, size_t size) {
  if (!data || size == 0) {
    z3ed_error_to_terminal("Invalid ROM data");
    return 0;
  }

  // Write to a temporary file
  std::string temp_path = "/roms/terminal_upload.sfc";
  std::ofstream file(temp_path, std::ios::binary);
  if (!file) {
      z3ed_error_to_terminal("Failed to write to VFS");
      return 0;
  }
  file.write(reinterpret_cast<const char*>(data), size);
  file.close();

  // Trigger load via bootstrap (which calls Application::LoadRom)
  yaze::app::wasm::TriggerRomLoad(temp_path);
  
  z3ed_print_to_terminal("ROM uploaded to VFS. Loading...");
  return 1;
}

/**
 * Get the current ROM info as JSON
 * @return JSON string with ROM info (caller must not free)
 */
EMSCRIPTEN_KEEPALIVE
const char* Z3edGetRomInfo() {
  yaze::Rom* rom = g_bridge.GetActiveRom();
  
  if (!rom || !rom->is_loaded()) {
    g_bridge.last_output = "{\"error\": \"No ROM loaded\"}";
    return g_bridge.last_output.c_str();
  }

  std::ostringstream json;
  json << "{"
       << "\"loaded\": true,"
       << "\"size\": " << rom->size() << ","
       << "\"title\": \"" << rom->title() << "\""
       << "}";

  g_bridge.last_output = json.str();
  return g_bridge.last_output.c_str();
}

/**
 * Execute a resource query command
 * @param query The resource query (e.g., "dungeon.rooms", "overworld.maps")
 * @return JSON result (caller must not free)
 */
EMSCRIPTEN_KEEPALIVE
const char* Z3edQueryResource(const char* query) {
  g_bridge.Initialize();

  if (!query) {
    g_bridge.last_output = "{\"error\": \"Invalid query\"}";
    return g_bridge.last_output.c_str();
  }
  
  yaze::Rom* rom = g_bridge.GetActiveRom();

  if (!rom || !rom->is_loaded()) {
    g_bridge.last_output = "{\"error\": \"No ROM loaded\"}";
    return g_bridge.last_output.c_str();
  }

  // Process resource query using command registry
  // Map to 'resource-search' which is the actual registered command
  std::string cmd_name = "resource-search";
  auto& registry = yaze::cli::CommandRegistry::Instance();

  if (registry.HasCommand(cmd_name)) {
    // Construct args: resource-search --query <query> --format json
    std::vector<std::string> cmd_args = {"--query", query, "--format", "json"};
    
    std::string cmd_output;
    auto status = registry.Execute(cmd_name, cmd_args, rom, &cmd_output);
    if (status.ok()) {
       // If output captured, return it directly
       if (!cmd_output.empty()) {
           // We might want to update last_output too as a side effect if the bridge uses it
           g_bridge.last_output = cmd_output;
           return g_bridge.last_output.c_str();
       }
       return "{\"status\":\"success\", \"message\":\"Query executed but no output returned.\"}";
    }
  }

  g_bridge.last_output = "{\"error\": \"Resource query failed\"}";
  return g_bridge.last_output.c_str();
}

}  // extern "C" 

// Emscripten module initialization
EMSCRIPTEN_BINDINGS(z3ed_terminal) {
  emscripten::function("processCommand", &Z3edProcessCommand,
                       emscripten::allow_raw_pointers());
  emscripten::function("getCompletions", &Z3edGetCompletions,
                       emscripten::allow_raw_pointers());
  emscripten::function("setApiKey", &Z3edSetApiKey,
                       emscripten::allow_raw_pointers());
  emscripten::function("isReady", &Z3edIsReady);
  emscripten::function("getRomInfo", &Z3edGetRomInfo,
                       emscripten::allow_raw_pointers());
  emscripten::function("queryResource", &Z3edQueryResource,
                       emscripten::allow_raw_pointers());
}

#endif  // __EMSCRIPTEN__