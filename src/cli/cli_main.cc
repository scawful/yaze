#include <cstdlib>
#include <iostream>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include "absl/flags/declare.h"
#include "absl/flags/flag.h"
#include "absl/strings/match.h"
#include "absl/strings/str_format.h"
#include "cli/cli.h"
#include "cli/tui/tui.h"
#include "cli/z3ed_ascii_logo.h"
#include "yaze_config.h"

// Define all CLI flags
ABSL_FLAG(bool, tui, false, "Launch interactive Text User Interface");
ABSL_FLAG(bool, quiet, false, "Suppress non-essential output");
ABSL_FLAG(bool, version, false, "Show version information");
ABSL_DECLARE_FLAG(std::string, rom);
ABSL_DECLARE_FLAG(std::string, ai_provider);
ABSL_DECLARE_FLAG(std::string, ai_model);
ABSL_DECLARE_FLAG(std::string, gemini_api_key);
ABSL_DECLARE_FLAG(std::string, ollama_host);
ABSL_DECLARE_FLAG(std::string, prompt_version);
ABSL_DECLARE_FLAG(bool, use_function_calling);

namespace {

void PrintVersion() {
  std::cout << yaze::cli::GetColoredLogo() << "\n";
  std::cout << absl::StrFormat("  Version %d.%d.%d\n", 
                               YAZE_VERSION_MAJOR,
                               YAZE_VERSION_MINOR, 
                               YAZE_VERSION_PATCH);
  std::cout << "  Yet Another Zelda3 Editor - Command Line Interface\n";
  std::cout << "  https://github.com/scawful/yaze\n\n";
}

void PrintCompactHelp() {
  std::cout << yaze::cli::GetColoredLogo() << "\n";
  std::cout << "  \033[1;37mYet Another Zelda3 Editor - AI-Powered CLI\033[0m\n\n";
  
  std::cout << "\033[1;36mUSAGE:\033[0m\n";
  std::cout << "  z3ed [command] [flags]\n";
  std::cout << "  z3ed --tui              # Interactive TUI mode\n";
  std::cout << "  z3ed --version          # Show version\n";
  std::cout << "  z3ed --help <category>  # Category help\n\n";
  
  std::cout << "\033[1;36mCOMMANDS:\033[0m\n";
  std::cout << "  \033[1;33magent\033[0m       AI conversational agent for ROM inspection\n";
  std::cout << "  \033[1;33mrom\033[0m         ROM operations (info, validate, diff)\n";
  std::cout << "  \033[1;33mdungeon\033[0m     Dungeon inspection and editing\n";
  std::cout << "  \033[1;33moverworld\033[0m   Overworld inspection and editing\n";
  std::cout << "  \033[1;33mmessage\033[0m     Message/dialogue inspection\n";
  std::cout << "  \033[1;33mgfx\033[0m         Graphics operations (export, import)\n";
  std::cout << "  \033[1;33mpalette\033[0m     Palette operations\n";
  std::cout << "  \033[1;33mpatch\033[0m       Apply patches (BPS, Asar)\n";
  std::cout << "  \033[1;33mproject\033[0m     Project management (init, build)\n\n";
  
  std::cout << "\033[1;36mCOMMON FLAGS:\033[0m\n";
  std::cout << "  --rom=<path>           Path to ROM file\n";
  std::cout << "  --tui                  Launch interactive TUI\n";
  std::cout << "  --quiet, -q            Suppress output\n";
  std::cout << "  --version              Show version\n";
  std::cout << "  --help <category>      Show category help\n\n";
  
  std::cout << "\033[1;36mEXAMPLES:\033[0m\n";
  std::cout << "  z3ed agent test-conversation --rom=zelda3.sfc\n";
  std::cout << "  z3ed rom info --rom=zelda3.sfc\n";
  std::cout << "  z3ed agent message-search --rom=zelda3.sfc --query=\"Master Sword\"\n";
  std::cout << "  z3ed dungeon export --rom=zelda3.sfc --id=1\n\n";
  
  std::cout << "For detailed help: z3ed --help <command>\n";
  std::cout << "For all commands:  z3ed --list-commands\n\n";
}

struct ParsedGlobals {
  std::vector<char*> positional;
  bool show_help = false;
  bool show_version = false;
  bool list_commands = false;
  std::optional<std::string> help_category;
  std::optional<std::string> error;
};

ParsedGlobals ParseGlobalFlags(int argc, char* argv[]) {
  ParsedGlobals result;
  if (argc <= 0 || argv == nullptr) {
    result.error = "Invalid argv provided";
    return result;
  }

  result.positional.reserve(argc);
  result.positional.push_back(argv[0]);

  bool passthrough = false;
  for (int i = 1; i < argc; ++i) {
    char* current = argv[i];
    std::string_view token(current);

    if (!passthrough) {
      if (token == "--") {
        passthrough = true;
        continue;
      }

      // Help flags
      if (absl::StartsWith(token, "--help=")) {
        std::string category(token.substr(7));
        if (!category.empty()) {
          result.help_category = category;
        } else {
          result.show_help = true;
        }
        continue;
      }
      if (token == "--help" || token == "-h") {
        if (i + 1 < argc && argv[i + 1][0] != '-') {
          result.help_category = std::string(argv[++i]);
        } else {
          result.show_help = true;
        }
        continue;
      }

      // Version flag
      if (token == "--version" || token == "-v") {
        result.show_version = true;
        continue;
      }

      // List commands
      if (token == "--list-commands" || token == "--list") {
        result.list_commands = true;
        continue;
      }

      // TUI mode
      if (token == "--tui" || token == "--interactive") {
        absl::SetFlag(&FLAGS_tui, true);
        continue;
      }

      // Quiet mode
      if (token == "--quiet" || token == "-q") {
        absl::SetFlag(&FLAGS_quiet, true);
        continue;
      }
      if (absl::StartsWith(token, "--quiet=")) {
        std::string value(token.substr(8));
        absl::SetFlag(&FLAGS_quiet, value == "true" || value == "1");
        continue;
      }

      // ROM path
      if (absl::StartsWith(token, "--rom=")) {
        absl::SetFlag(&FLAGS_rom, std::string(token.substr(6)));
        continue;
      }
      if (token == "--rom") {
        if (i + 1 >= argc) {
          result.error = "--rom flag requires a value";
          return result;
        }
        absl::SetFlag(&FLAGS_rom, std::string(argv[++i]));
        continue;
      }

      // AI provider flags
      if (absl::StartsWith(token, "--ai_provider=") || 
          absl::StartsWith(token, "--ai-provider=")) {
        size_t eq_pos = token.find('=');
        absl::SetFlag(&FLAGS_ai_provider, std::string(token.substr(eq_pos + 1)));
        continue;
      }
      if (token == "--ai_provider" || token == "--ai-provider") {
        if (i + 1 >= argc) {
          result.error = "--ai-provider flag requires a value";
          return result;
        }
        absl::SetFlag(&FLAGS_ai_provider, std::string(argv[++i]));
        continue;
      }

      if (absl::StartsWith(token, "--ai_model=") || 
          absl::StartsWith(token, "--ai-model=")) {
        size_t eq_pos = token.find('=');
        absl::SetFlag(&FLAGS_ai_model, std::string(token.substr(eq_pos + 1)));
        continue;
      }
      if (token == "--ai_model" || token == "--ai-model") {
        if (i + 1 >= argc) {
          result.error = "--ai-model flag requires a value";
          return result;
        }
        absl::SetFlag(&FLAGS_ai_model, std::string(argv[++i]));
        continue;
      }

      if (absl::StartsWith(token, "--gemini_api_key=") || 
          absl::StartsWith(token, "--gemini-api-key=")) {
        size_t eq_pos = token.find('=');
        absl::SetFlag(&FLAGS_gemini_api_key, std::string(token.substr(eq_pos + 1)));
        continue;
      }
      if (token == "--gemini_api_key" || token == "--gemini-api-key") {
        if (i + 1 >= argc) {
          result.error = "--gemini-api-key flag requires a value";
          return result;
        }
        absl::SetFlag(&FLAGS_gemini_api_key, std::string(argv[++i]));
        continue;
      }

      if (absl::StartsWith(token, "--ollama_host=") || 
          absl::StartsWith(token, "--ollama-host=")) {
        size_t eq_pos = token.find('=');
        absl::SetFlag(&FLAGS_ollama_host, std::string(token.substr(eq_pos + 1)));
        continue;
      }
      if (token == "--ollama_host" || token == "--ollama-host") {
        if (i + 1 >= argc) {
          result.error = "--ollama-host flag requires a value";
          return result;
        }
        absl::SetFlag(&FLAGS_ollama_host, std::string(argv[++i]));
        continue;
      }

      if (absl::StartsWith(token, "--prompt_version=") || 
          absl::StartsWith(token, "--prompt-version=")) {
        size_t eq_pos = token.find('=');
        absl::SetFlag(&FLAGS_prompt_version, std::string(token.substr(eq_pos + 1)));
        continue;
      }
      if (token == "--prompt_version" || token == "--prompt-version") {
        if (i + 1 >= argc) {
          result.error = "--prompt-version flag requires a value";
          return result;
        }
        absl::SetFlag(&FLAGS_prompt_version, std::string(argv[++i]));
        continue;
      }

      if (absl::StartsWith(token, "--use_function_calling=") || 
          absl::StartsWith(token, "--use-function-calling=")) {
        size_t eq_pos = token.find('=');
        std::string value(token.substr(eq_pos + 1));
        absl::SetFlag(&FLAGS_use_function_calling, value == "true" || value == "1");
        continue;
      }
      if (token == "--use_function_calling" || token == "--use-function-calling") {
        if (i + 1 >= argc) {
          result.error = "--use-function-calling flag requires a value";
          return result;
        }
        std::string value(argv[++i]);
        absl::SetFlag(&FLAGS_use_function_calling, value == "true" || value == "1");
        continue;
      }
    }

    result.positional.push_back(current);
  }

  return result;
}

}  // namespace

int main(int argc, char* argv[]) {
  // Parse global flags
  ParsedGlobals globals = ParseGlobalFlags(argc, argv);

  if (globals.error.has_value()) {
    std::cerr << "Error: " << *globals.error << "\n";
    std::cerr << "Use --help for usage information.\n";
    return EXIT_FAILURE;
  }

  // Handle version flag
  if (globals.show_version) {
    PrintVersion();
    return EXIT_SUCCESS;
  }

  // Handle TUI mode
  if (absl::GetFlag(FLAGS_tui)) {
    // Load ROM if specified before launching TUI
    std::string rom_path = absl::GetFlag(FLAGS_rom);
    if (!rom_path.empty()) {
      auto status = yaze::cli::app_context.rom.LoadFromFile(rom_path);
      if (!status.ok()) {
        std::cerr << "\n\033[1;31mError:\033[0m Failed to load ROM: " 
                  << status.message() << "\n";
        // Continue to TUI anyway, user can load ROM from there
      }
    }
    yaze::cli::ShowMain();
    return EXIT_SUCCESS;
  }

  // Create CLI instance
  yaze::cli::ModernCLI cli;

  // Handle category-specific help
  if (globals.help_category.has_value()) {
    cli.PrintCategoryHelp(*globals.help_category);
    return EXIT_SUCCESS;
  }

  // Handle list commands
  if (globals.list_commands) {
    cli.PrintCommandSummary();
    return EXIT_SUCCESS;
  }

  // Handle general help or no arguments
  if (globals.show_help || globals.positional.size() <= 1) {
    PrintCompactHelp();
    return EXIT_SUCCESS;
  }

  // Run CLI commands
  auto status = cli.Run(static_cast<int>(globals.positional.size()),
                        globals.positional.data());

  if (!status.ok()) {
    std::cerr << "\n\033[1;31mError:\033[0m " << status.message() << "\n";
    std::cerr << "Use --help for usage information.\n";
    return EXIT_FAILURE;
  }

  return EXIT_SUCCESS;
}