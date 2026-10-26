/**
 * @file wasm_terminal_bridge.cc
 * @brief C++ bridge for web terminal integration in WASM builds
 *
 * Exposes z3ed command processing, completion and ROM queries to the browser
 * terminal. Every exported function that returns `const char*` returns a
 * pointer into bridge-owned storage that stays valid until the next call.
 */

#ifdef __EMSCRIPTEN__

#include <cstdint>
#include <fstream>
#include <memory>
#include <optional>
#include <set>
#include <string>
#include <vector>

#include <emscripten.h>
#include <emscripten/bind.h>

#include "absl/status/status.h"
#include "absl/strings/ascii.h"
#include "absl/strings/match.h"
#include "absl/strings/numbers.h"
#include "absl/strings/str_cat.h"
#include "absl/strings/str_join.h"
#include "absl/strings/str_split.h"
#include "absl/strings/string_view.h"
#include "absl/types/span.h"
#include "app/editor/dungeon/dungeon_editor_v2.h"
#include "app/editor/editor_manager.h"
#include "app/net/wasm/emscripten_http_client.h"
#include "app/platform/wasm/wasm_bootstrap.h"
#include "cli/handlers/command_handlers.h"
#include "cli/service/ai/browser_ai_service.h"
#include "cli/service/command_registry.h"
#include "cli/subcommand_routes.h"
#include "nlohmann/json.hpp"
#include "rom/rom.h"

namespace yaze::app {
extern editor::EditorManager* GetGlobalEditorManager();
}

namespace {

constexpr char kTerminalUploadPath[] = "/.yaze/roms/terminal_upload.sfc";

struct BridgeState {
  std::unique_ptr<yaze::cli::BrowserAIService> ai_service;
  std::string last_output;
  std::string provider = yaze::cli::kProviderGemini;
  std::string model = "gemini-2.5-flash";
  std::string api_base;
  std::string api_key;
  bool initialized = false;

  void Initialize() {
    if (!initialized) {
      yaze::cli::CommandRegistry::Instance();
      initialized = true;
    }
  }

  void SetupAIService() {
    yaze::cli::BrowserAIConfig config;
    config.provider = provider.empty() ? yaze::cli::kProviderGemini : provider;
    config.api_key = api_key;
    config.model = model;
    config.api_base = api_base;
    config.verbose = false;
    auto http_client = std::make_unique<yaze::net::EmscriptenHttpClient>();
    ai_service = std::make_unique<yaze::cli::BrowserAIService>(
        config, std::move(http_client));
    if (yaze::Rom* rom = GetActiveRom()) {
      ai_service->SetRomContext(rom);
    }
  }

  void ConfigureAI(const char* provider_value, const char* model_value,
                   const char* api_base_value, const char* api_key_value) {
    if (provider_value) {
      provider = provider_value;
    }
    if (model_value) {
      model = model_value;
    }
    if (api_base_value) {
      api_base = api_base_value;
    }
    if (api_key_value) {
      api_key = api_key_value;
    }
    SetupAIService();
  }

  // The ROM open in the editor, which the terminal operates on.
  yaze::Rom* GetActiveRom() const {
    auto* manager = yaze::app::GetGlobalEditorManager();
    return manager ? manager->GetCurrentRom() : nullptr;
  }

  yaze::Rom* GetLoadedRom() const {
    yaze::Rom* rom = GetActiveRom();
    return rom && rom->is_loaded() ? rom : nullptr;
  }

  const char* Return(std::string output) {
    last_output = std::move(output);
    return last_output.c_str();
  }
};

BridgeState g_bridge;

}  // namespace

namespace yaze {
namespace cli {

BrowserAIService* GetGlobalBrowserAIService() {
  return g_bridge.ai_service.get();
}

yaze::Rom* GetGlobalRom() {
  return g_bridge.GetActiveRom();
}

}  // namespace cli
}  // namespace yaze

namespace {

using Args = std::vector<std::string>;

EM_JS(void, z3ed_print_to_terminal, (const char* text), {
  if (window.z3edTerminal) {
    window.z3edTerminal.print(UTF8ToString(text));
  } else {
    console.log(UTF8ToString(text));
  }
});

EM_JS(void, z3ed_error_to_terminal, (const char* text), {
  if (window.z3edTerminal) {
    window.z3edTerminal.printError(UTF8ToString(text));
  } else {
    console.error(UTF8ToString(text));
  }
});

// Whitespace tokenization; quoted arguments are not supported.
Args ParseCommand(absl::string_view command) {
  return absl::StrSplit(command, absl::ByAnyChar(" \t"), absl::SkipEmpty());
}

std::string ErrorJson(absl::string_view message) {
  return nlohmann::json{{"error", message}}.dump();
}

std::optional<int> ParseHexId(absl::string_view text) {
  int value = 0;
  if (!absl::SimpleHexAtoi(text, &value)) {
    return std::nullopt;
  }
  return value;
}

std::string ExecuteRegistryCommand(const std::string& name, const Args& args) {
  std::string output;
  absl::Status status = yaze::cli::CommandRegistry::Instance().Execute(
      name, args, g_bridge.GetActiveRom(), &output);
  if (!status.ok()) {
    return absl::StrCat("Command failed: ", status.message());
  }
  return output.empty() ? "Command executed successfully" : output;
}

std::string HandleHelp(const Args& args) {
  auto& registry = yaze::cli::CommandRegistry::Instance();
  if (args.size() < 2) {
    return registry.GenerateCompleteHelp();
  }
  if (registry.HasCommand(args[1])) {
    return registry.GenerateHelp(args[1]);
  }
  return registry.GenerateCategoryHelp(args[1]);
}

std::string HandleRom(const Args& args) {
  if (args.size() < 2) {
    return absl::StrCat("Usage: rom load <path> | rom info\n",
                        yaze::cli::RomSubcommandUsage());
  }
  const std::string& subcommand = args[1];
  if (subcommand == "load") {
    if (args.size() < 3) {
      return "Usage: rom load <path>";
    }
    yaze::app::wasm::TriggerRomLoad(args[2]);
    return absl::StrCat("Requesting load for: ", args[2],
                        ". Check application log.");
  }
  if (subcommand == "info") {
    yaze::Rom* rom = g_bridge.GetLoadedRom();
    if (rom == nullptr) {
      return "No ROM loaded.";
    }
    return absl::StrCat("ROM Info:\n  Size: ", rom->size(),
                        " bytes\n  Title: ", rom->title(), "\n");
  }

  auto invocation =
      yaze::cli::ResolveRomSubcommand(absl::MakeConstSpan(args).subspan(1));
  if (!invocation.ok()) {
    return std::string(invocation.status().message());
  }
  return ExecuteRegistryCommand(invocation->command, invocation->args);
}

std::string HandleAi(const Args& args) {
  if (!g_bridge.ai_service) {
    g_bridge.SetupAIService();
  }
  if (!g_bridge.ai_service) {
    return "AI service unavailable";
  }
  if (args.size() < 2) {
    return "AI command requires a prompt. Usage: ai <prompt>";
  }

  const std::string prompt = absl::StrJoin(args.begin() + 1, args.end(), " ");
  auto response = g_bridge.ai_service->GenerateResponse(prompt);
  if (!response.ok()) {
    return absl::StrCat("AI error: ", response.status().message());
  }
  return response->text_response;
}

std::string HandleEditor(const Args& args) {
  auto* editor_manager = yaze::app::GetGlobalEditorManager();
  if (!editor_manager) {
    return "Error: Editor manager not available";
  }

  if (args.size() > 2 && args[1] == "switch") {
    const std::string target = absl::AsciiStrToLower(args[2]);
    for (size_t i = 0; i < yaze::editor::kEditorNames.size(); ++i) {
      const std::string name =
          absl::AsciiStrToLower(yaze::editor::kEditorNames[i]);
      if (name == target) {
        editor_manager->SwitchToEditor(
            static_cast<yaze::editor::EditorType>(i));
        return absl::StrCat("Switched to ", yaze::editor::kEditorNames[i]);
      }
    }
    return absl::StrCat("Unknown editor: ", args[2]);
  }

  if (args.size() > 3 && args[1] == "card") {
    auto* current_editor = editor_manager->GetCurrentEditor();
    if (current_editor &&
        current_editor->type() == yaze::editor::EditorType::kDungeon) {
      return "Panel visibility is now controlled via Layout Designer. Use "
             "'editor layout' commands.";
    }
    return "Panel control not supported for current editor";
  }

  if (args.size() > 2 && args[1] == "debug" && args[2] == "toggle") {
    return "Debug controls are now accessed via the Layout Designer. "
           "Use the canvas debug panel or layout commands.";
  }

  return "Usage: editor switch <name>";
}

std::string HandleDungeon(const Args& args) {
  auto* editor_manager = yaze::app::GetGlobalEditorManager();
  if (!editor_manager) {
    return "Error: Editor manager not available";
  }

  auto* current_editor = editor_manager->GetCurrentEditor();
  if (!current_editor ||
      current_editor->type() != yaze::editor::EditorType::kDungeon) {
    return "Error: Dungeon editor is not active. Use 'editor switch dungeon' "
           "first.";
  }
  auto* dungeon_editor =
      static_cast<yaze::editor::DungeonEditorV2*>(current_editor);

  if (args.size() > 2 && args[1] == "room") {
    std::optional<int> room_id = ParseHexId(args[2]);
    if (!room_id) {
      return "Invalid room ID (hex required)";
    }
    dungeon_editor->FocusRoom(*room_id);
    return absl::StrCat("Focused room ", args[2]);
  }

  if (args.size() > 2 && args[1] == "select_object") {
    std::optional<int> object_id = ParseHexId(args[2]);
    if (!object_id) {
      return "Invalid object ID (hex required)";
    }
    dungeon_editor->SelectObject(*object_id);
    return absl::StrCat("Selected object ", args[2]);
  }

  if (args.size() > 2 && args[1] == "agent_mode") {
    const bool enabled = args[2] == "on" || args[2] == "true";
    dungeon_editor->SetAgentMode(enabled);
    return absl::StrCat("Agent mode ", enabled ? "enabled" : "disabled");
  }

  return "Usage: dungeon room <hex> | select_object <hex> | agent_mode "
         "<on|off>";
}

using BuiltinHandler = std::string (*)(const Args&);

struct BuiltinCommand {
  absl::string_view name;
  BuiltinHandler handler;
};

// Commands handled by the bridge itself rather than the command registry.
constexpr BuiltinCommand kBuiltinCommands[] = {
    {"help", &HandleHelp},     {"rom", &HandleRom},         {"ai", &HandleAi},
    {"editor", &HandleEditor}, {"dungeon", &HandleDungeon},
};

std::string ProcessCommandInternal(absl::string_view command_str) {
  const Args args = ParseCommand(command_str);
  if (args.empty()) {
    return "No command provided";
  }

  for (const BuiltinCommand& builtin : kBuiltinCommands) {
    if (args[0] == builtin.name) {
      return builtin.handler(args);
    }
  }

  auto& registry = yaze::cli::CommandRegistry::Instance();
  if (registry.HasCommand(args[0])) {
    return ExecuteRegistryCommand(args[0], Args(args.begin() + 1, args.end()));
  }
  if (!registry.GetCommandsInCategory(args[0]).empty()) {
    return registry.GenerateCategoryHelp(args[0]);
  }

  return absl::StrCat("Unknown command: ", args[0],
                      "\nType 'help' for available commands");
}

std::vector<std::string> GetCompletionsInternal(absl::string_view partial) {
  const Args words = ParseCommand(partial);
  const bool completing_first_word =
      words.empty() || (words.size() == 1 && !absl::EndsWith(partial, " "));

  std::set<std::string> completions;
  if (completing_first_word) {
    const absl::string_view prefix =
        words.empty() ? absl::string_view() : words[0];
    auto add = [&](absl::string_view candidate) {
      if (absl::StartsWith(candidate, prefix)) {
        completions.emplace(candidate);
      }
    };
    auto& registry = yaze::cli::CommandRegistry::Instance();
    for (const BuiltinCommand& builtin : kBuiltinCommands) {
      add(builtin.name);
    }
    for (const std::string& category : registry.GetCategories()) {
      add(category);
      for (const std::string& command :
           registry.GetCommandsInCategory(category)) {
        add(command);
      }
    }
  } else if (words[0] == "rom") {
    const absl::string_view prefix =
        words.size() > 1 ? absl::string_view(words[1]) : absl::string_view();
    std::vector<absl::string_view> subcommands = {"load", "info"};
    for (absl::string_view name : yaze::cli::RomSubcommandNames()) {
      subcommands.push_back(name);
    }
    for (absl::string_view subcommand : subcommands) {
      if (absl::StartsWith(subcommand, prefix)) {
        completions.insert(absl::StrCat("rom ", subcommand));
      }
    }
  }

  return {completions.begin(), completions.end()};
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
    return g_bridge.Return("Invalid command");
  }

  const char* output = g_bridge.Return(ProcessCommandInternal(command));
  z3ed_print_to_terminal(output);
  return output;
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
    return g_bridge.Return("[]");
  }
  return g_bridge.Return(
      nlohmann::json(GetCompletionsInternal(partial)).dump());
}

/**
 * Set the API key for AI services
 * @param api_key The API key to use
 */
EMSCRIPTEN_KEEPALIVE
void Z3edSetApiKey(const char* api_key) {
  g_bridge.ConfigureAI(nullptr, nullptr, nullptr, api_key ? api_key : "");
}

/**
 * Configure the browser AI bridge
 * @param provider AI provider id (gemini/openai/lmstudio/halext)
 * @param model Model name
 * @param api_base Provider base URL for OpenAI-compatible endpoints
 * @param api_key Provider API key or bearer token
 */
EMSCRIPTEN_KEEPALIVE
void Z3edConfigureAI(const char* provider, const char* model,
                     const char* api_base, const char* api_key) {
  g_bridge.Initialize();
  g_bridge.ConfigureAI(provider ? provider : yaze::cli::kProviderGemini,
                       model ? model : "", api_base ? api_base : "",
                       api_key ? api_key : "");
}

/**
 * Check if the terminal bridge is ready
 * @return 1 if ready, 0 otherwise
 */
EMSCRIPTEN_KEEPALIVE
int Z3edIsReady() {
  g_bridge.Initialize();
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

  {
    std::ofstream file(kTerminalUploadPath, std::ios::binary);
    if (!file || !file.write(reinterpret_cast<const char*>(data),
                             static_cast<std::streamsize>(size))) {
      z3ed_error_to_terminal("Failed to write to VFS");
      return 0;
    }
  }

  yaze::app::wasm::TriggerRomLoad(kTerminalUploadPath);

  z3ed_print_to_terminal("ROM uploaded to VFS. Loading...");
  return 1;
}

/**
 * Get the current ROM info as JSON
 * @return JSON string with ROM info (caller must not free)
 */
EMSCRIPTEN_KEEPALIVE
const char* Z3edGetRomInfo() {
  yaze::Rom* rom = g_bridge.GetLoadedRom();
  if (rom == nullptr) {
    return g_bridge.Return(ErrorJson("No ROM loaded"));
  }

  return g_bridge.Return(nlohmann::json{
      {"loaded", true},
      {"size", rom->size()},
      {"title", rom->title()},
  }
                             .dump());
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
    return g_bridge.Return(ErrorJson("Invalid query"));
  }

  yaze::Rom* rom = g_bridge.GetLoadedRom();
  if (rom == nullptr) {
    return g_bridge.Return(ErrorJson("No ROM loaded"));
  }

  constexpr char kResourceSearchCommand[] = "resource-search";
  auto& registry = yaze::cli::CommandRegistry::Instance();
  if (!registry.HasCommand(kResourceSearchCommand)) {
    return g_bridge.Return(ErrorJson("Resource query failed"));
  }

  const Args args = {"--query", query, "--format", "json"};
  std::string output;
  absl::Status status =
      registry.Execute(kResourceSearchCommand, args, rom, &output);
  if (!status.ok()) {
    return g_bridge.Return(ErrorJson("Resource query failed"));
  }
  if (output.empty()) {
    return g_bridge.Return(nlohmann::json{
        {"status", "success"},
        {"message", "Query executed but no output returned."},
    }
                               .dump());
  }
  return g_bridge.Return(std::move(output));
}

}  // extern "C"

EMSCRIPTEN_BINDINGS(z3ed_terminal) {
  emscripten::function("processCommand", &Z3edProcessCommand,
                       emscripten::allow_raw_pointers());
  emscripten::function("getCompletions", &Z3edGetCompletions,
                       emscripten::allow_raw_pointers());
  emscripten::function("setApiKey", &Z3edSetApiKey,
                       emscripten::allow_raw_pointers());
  emscripten::function("configureAI", &Z3edConfigureAI,
                       emscripten::allow_raw_pointers());
  emscripten::function("isReady", &Z3edIsReady);
  emscripten::function("getRomInfo", &Z3edGetRomInfo,
                       emscripten::allow_raw_pointers());
  emscripten::function("queryResource", &Z3edQueryResource,
                       emscripten::allow_raw_pointers());
}

#endif  // __EMSCRIPTEN__
