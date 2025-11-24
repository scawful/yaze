/**
 * @file wasm_terminal_bridge.cc
 * @brief C++ bridge for web terminal integration in WASM builds
 *
 * This file provides the C++ side of the web terminal integration,
 * exposing functions that can be called from JavaScript to process
 * z3ed commands and provide terminal functionality in the browser.
 */

#ifdef __EMSCRIPTEN__

#include <cstring>
#include <memory>
#include <sstream>
#include <string>
#include <vector>

#include <emscripten.h>
#include <emscripten/bind.h>

#include "absl/status/status.h"
#include "absl/strings/str_split.h"
#include "absl/strings/str_join.h"
#include "app/rom.h"
#include "cli/service/command_registry.h"
#include "cli/service/ai/browser_ai_service.h"
#include "cli/handlers/command_handlers.h"

namespace {

// Global state for the terminal bridge
struct TerminalBridge {
  std::unique_ptr<yaze::Rom> rom;
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
      ai_service = std::make_unique<yaze::cli::BrowserAIService>(config);
    }
  }
};

static TerminalBridge g_bridge;

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
      if (!g_bridge.rom) {
        g_bridge.rom = std::make_unique<yaze::Rom>();
      }
      auto status = g_bridge.rom->LoadFromFile(args[2]);
      if (status.ok()) {
        return "ROM loaded successfully: " + args[2];
      } else {
        return "Failed to load ROM: " + std::string(status.message());
      }
    }
    if (args[1] == "info" && g_bridge.rom && g_bridge.rom->is_loaded()) {
      output << "ROM Info:\n";
      output << "  Size: " << g_bridge.rom->size() << " bytes\n";
      output << "  Version: " << static_cast<int>(g_bridge.rom->version()) << "\n";
      output << "  Title: " << g_bridge.rom->title() << "\n";
      return output.str();
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
      return *response;
    } else {
      return "AI error: " + std::string(response.status().message());
    }
  }

  // Try command registry
  auto& registry = yaze::cli::CommandRegistry::Instance();
  if (registry.HasCommand(args[0])) {
    std::vector<std::string> cmd_args(args.begin() + 1, args.end());
    auto status = registry.Execute(args[0], cmd_args, g_bridge.rom.get());
    if (status.ok()) {
      return "Command executed successfully";
    } else {
      return "Command failed: " + std::string(status.message());
    }
  }

  return "Unknown command: " + args[0] + "\nType 'help' for available commands";
}

// Get command completions for autocomplete
std::vector<std::string> GetCompletionsInternal(const std::string& partial) {
  std::vector<std::string> completions;

  // Get all available commands from registry
  auto& registry = yaze::cli::CommandRegistry::Instance();
  auto categories = registry.GetCategories();

  for (const auto& category : categories) {
    auto commands = registry.GetCommandsInCategory(category);
    for (const auto& cmd : commands) {
      if (cmd.find(partial) == 0) {
        completions.push_back(cmd);
      }
    }
  }

  // Add special commands
  std::vector<std::string> special = {"help", "rom", "ai", "clear", "version"};
  for (const auto& cmd : special) {
    if (cmd.find(partial) == 0) {
      completions.push_back(cmd);
    }
  }

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

  if (!g_bridge.rom) {
    g_bridge.rom = std::make_unique<yaze::Rom>();
  }

  // Load ROM from memory buffer
  auto status = g_bridge.rom->LoadFromData(std::vector<uint8_t>(data, data + size));
  if (status.ok()) {
    z3ed_print_to_terminal("ROM loaded successfully");
    return 1;
  } else {
    std::string error = "Failed to load ROM: " + std::string(status.message());
    z3ed_error_to_terminal(error.c_str());
    return 0;
  }
}

/**
 * Get the current ROM info as JSON
 * @return JSON string with ROM info (caller must not free)
 */
EMSCRIPTEN_KEEPALIVE
const char* Z3edGetRomInfo() {
  if (!g_bridge.rom || !g_bridge.rom->is_loaded()) {
    g_bridge.last_output = "{\"error\": \"No ROM loaded\"}";
    return g_bridge.last_output.c_str();
  }

  std::ostringstream json;
  json << "{"
       << "\"loaded\": true,"
       << "\"size\": " << g_bridge.rom->size() << ","
       << "\"version\": " << static_cast<int>(g_bridge.rom->version()) << ","
       << "\"title\": \"" << g_bridge.rom->title() << "\""
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

  if (!g_bridge.rom || !g_bridge.rom->is_loaded()) {
    g_bridge.last_output = "{\"error\": \"No ROM loaded\"}";
    return g_bridge.last_output.c_str();
  }

  // Process resource query using command registry
  std::vector<std::string> args = {"resource", "query", query};
  auto& registry = yaze::cli::CommandRegistry::Instance();

  if (registry.HasCommand("resource")) {
    std::vector<std::string> cmd_args = {"query", query};
    auto status = registry.Execute("resource", cmd_args, g_bridge.rom.get());
    if (status.ok()) {
      // Output should be in last_output from the command handler
      return g_bridge.last_output.c_str();
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