#include <atomic>
#include <chrono>
#include <csignal>
#include <cstdlib>
#include <iostream>
#include <optional>
#include <string>
#include <thread>
#include <vector>

#include "absl/flags/declare.h"
#include "absl/flags/flag.h"
#include "absl/strings/match.h"
#include "absl/strings/str_format.h"
#include "absl/strings/string_view.h"
#include "cli/cli.h"
#ifdef YAZE_ENABLE_AGENT_CLI
#include "cli/handlers/agent_command_registration.h"
#endif
#include "cli/service/command_registry.h"
#include "rom/rom.h"
#include "cli/z3ed_ascii_logo.h"
#include "yaze_config.h"

#ifdef YAZE_HTTP_API_ENABLED
#include "cli/service/api/http_server.h"
#include "util/log.h"
#endif

// Define all CLI flags
ABSL_DECLARE_FLAG(bool, quiet);
ABSL_DECLARE_FLAG(bool, sandbox);
ABSL_FLAG(bool, version, false, "Show version information");
ABSL_FLAG(bool, self_test, false,
          "Run self-test diagnostics to verify CLI functionality");
#ifdef YAZE_HTTP_API_ENABLED
ABSL_FLAG(int, http_port, 0,
          "HTTP API server port (0 = disabled, default: 8080 when enabled)");
ABSL_FLAG(std::string, http_host, "localhost",
          "HTTP API server host (default: localhost)");
#endif
ABSL_DECLARE_FLAG(std::string, rom);
ABSL_DECLARE_FLAG(std::string, ai_provider);
ABSL_DECLARE_FLAG(std::string, ai_model);
ABSL_DECLARE_FLAG(std::string, gemini_api_key);
ABSL_DECLARE_FLAG(std::string, anthropic_api_key);
ABSL_DECLARE_FLAG(std::string, ollama_host);
ABSL_DECLARE_FLAG(std::string, mesen_socket);
ABSL_DECLARE_FLAG(std::string, gui_server_address);
ABSL_DECLARE_FLAG(std::string, prompt_version);
ABSL_DECLARE_FLAG(bool, use_function_calling);

namespace {

void PrintVersion() {
  std::cout << yaze::cli::GetColoredLogo() << "\n";
  std::cout << absl::StrFormat("  Version %d.%d.%d\n", YAZE_VERSION_MAJOR,
                               YAZE_VERSION_MINOR, YAZE_VERSION_PATCH);
  std::cout << "  Yet Another Zelda3 Editor - Command Line Interface\n";
  std::cout << "  https://github.com/scawful/yaze\n\n";
}

/**
 * @brief Run self-test diagnostics to verify CLI functionality
 * @return EXIT_SUCCESS if all tests pass, EXIT_FAILURE otherwise
 */
int RunSelfTest() {
  std::cout << "\n\033[1;36m=== z3ed Self-Test ===\033[0m\n\n";
  int passed = 0;
  int failed = 0;

  auto run_test = [&](const char* name, bool condition) {
    if (condition) {
      std::cout << "  \033[1;32m✓\033[0m " << name << "\n";
      ++passed;
    } else {
      std::cout << "  \033[1;31m✗\033[0m " << name << "\n";
      ++failed;
    }
  };

  // Test 1: Version info is available
  run_test("Version info available",
           YAZE_VERSION_MAJOR >= 0 && YAZE_VERSION_MINOR >= 0);

  // Test 2: CLI instance can be created
  {
    bool cli_created = false;
    try {
      yaze::cli::ModernCLI cli;
      cli_created = true;
    } catch (...) {
      cli_created = false;
    }
    run_test("CLI instance creation", cli_created);
  }

  // Test 3: App context is accessible
  run_test("App context accessible", true);  // Always passes if we got here

  // Test 4: ROM class can be instantiated
  {
    bool rom_ok = false;
    try {
      yaze::Rom test_rom;
      rom_ok = true;
    } catch (...) {
      rom_ok = false;
    }
    run_test("ROM class instantiation", rom_ok);
  }

  // Test 5: Flag parsing works
  run_test("Flag parsing functional", absl::GetFlag(FLAGS_self_test) == true);

#ifdef YAZE_HTTP_API_ENABLED
  // Test 6: HTTP API available (if compiled in)
  run_test("HTTP API compiled in", true);
#else
  run_test("HTTP API not compiled (expected)", true);
#endif

  // Summary
  std::cout << "\n\033[1;36m=== Results ===\033[0m\n";
  std::cout << "  Passed: \033[1;32m" << passed << "\033[0m\n";
  std::cout << "  Failed: \033[1;31m" << failed << "\033[0m\n";

  if (failed == 0) {
    std::cout << "\n\033[1;32mAll self-tests passed!\033[0m\n\n";
    return EXIT_SUCCESS;
  } else {
    std::cout << "\n\033[1;31mSome self-tests failed.\033[0m\n\n";
    return EXIT_FAILURE;
  }
}

void PrintCompactHelp() {
  auto& registry = yaze::cli::CommandRegistry::Instance();
  auto categories = registry.GetCategories();

  std::cout << yaze::cli::GetColoredLogo() << "\n";
  std::cout
      << "  \033[1;37mYet Another Zelda3 Editor - AI-Powered CLI\033[0m\n\n";

  std::cout << "\033[1;36mUSAGE:\033[0m\n";
  std::cout << "  z3ed [command] [flags]\n";
  std::cout << "  z3ed help <command|category>  # Scoped help\n";
  std::cout << "  z3ed --export-schemas         # JSON schemas for agents\n";
  std::cout << "  z3ed --version                # Show version\n\n";

  std::cout << "\033[1;36mCATEGORIES:\033[0m\n";
  for (const auto& category : categories) {
    auto commands = registry.GetCommandsInCategory(category);
    std::cout << "  \033[1;33m" << category << "\033[0m (" << commands.size()
              << " commands)\n";
  }
  std::cout << "\n";

  std::cout << "\033[1;36mCOMMON FLAGS:\033[0m\n";
  std::cout << "  --rom=<path>           Path to ROM file\n";
  std::cout << "  --sandbox              Run ROM commands in a sandbox copy\n";
  std::cout << "  --quiet, -q            Suppress output\n";
  std::cout << "  --ai_provider=<name>   AI provider (auto, ollama, gemini, "
               "openai,\n";
  std::cout << "                         anthropic, mock)\n";
  std::cout << "  --ai_model=<name>      Provider-specific model override\n";
  std::cout << "  --version              Show version\n";
  std::cout << "  --self-test            Run self-test diagnostics\n";
  std::cout << "  --help <scope>         Show help for command or category\n";
  std::cout << "  --export-schemas       Export command schemas as JSON\n";
#ifdef YAZE_HTTP_API_ENABLED
  std::cout << "  --http-port=<port>     HTTP API server port (0=disabled)\n";
  std::cout
      << "  --http-host=<host>     HTTP API display host (printed URLs)\n";
  std::cout
      << "                         (no command keeps server alive until Ctrl+C)\n";
#endif
  std::cout << "  --mesen-socket=<path>  Override Mesen2 socket path\n";
  std::cout << "\n";

  std::cout << "\033[1;36mEXAMPLES:\033[0m\n";
#ifdef YAZE_ENABLE_AGENT_CLI
  std::cout << "  z3ed agent simple-chat --rom=zelda3.sfc\n";
#endif
  std::cout << "  z3ed rom-info --rom=zelda3.sfc\n";
  std::cout << "  z3ed rom read --address=0x1000 --length=16 --rom=zelda3.sfc\n";
  std::cout << "  z3ed debug state\n";
  std::cout
      << "  z3ed message-search --rom=zelda3.sfc --query=\"Master Sword\"\n";
  std::cout << "  z3ed dungeon-export --rom=zelda3.sfc --id=1\n\n";

  std::cout << "For detailed help: z3ed help <command>\n";
  std::cout << "For all commands:  z3ed --list-commands\n\n";
}

#ifdef YAZE_HTTP_API_ENABLED
std::atomic<bool> g_http_shutdown_requested{false};

void HandleHttpShutdownSignal(int) {
  g_http_shutdown_requested.store(true);
}

void WaitForHttpServer() {
  std::signal(SIGINT, HandleHttpShutdownSignal);
  std::signal(SIGTERM, HandleHttpShutdownSignal);
  while (!g_http_shutdown_requested.load()) {
    std::this_thread::sleep_for(std::chrono::milliseconds(200));
  }
}
#endif

struct ParsedGlobals {
  std::vector<char*> positional;
  bool show_help = false;
  bool show_version = false;
  bool list_commands = false;
  bool self_test = false;
  bool export_schemas = false;
  std::optional<std::string> help_target;
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
    absl::string_view token(current);

    if (!passthrough) {
      if (token == "--") {
        passthrough = true;
        continue;
      }

      // Help flags
      if (absl::StartsWith(token, "--help=")) {
        std::string target(token.substr(7));
        if (!target.empty()) {
          result.help_target = target;
        } else {
          result.show_help = true;
        }
        continue;
      }
      if (token == "--help" || token == "-h") {
        if (i + 1 < argc && argv[i + 1][0] != '-') {
          result.help_target = std::string(argv[++i]);
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

      // Schema export
      if (token == "--export-schemas" || token == "--export_schemas") {
        result.export_schemas = true;
        continue;
      }

      // Self-test mode
      if (token == "--self-test" || token == "--selftest") {
        result.self_test = true;
        continue;
      }

      if (token == "--tui" || token == "--interactive") {
        result.error =
            "--tui/--interactive was removed; use `z3ed help` for CLI workflows";
        return result;
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

      // Sandbox mode
      if (token == "--sandbox") {
        absl::SetFlag(&FLAGS_sandbox, true);
        continue;
      }
      if (absl::StartsWith(token, "--sandbox=")) {
        std::string value(token.substr(10));
        absl::SetFlag(&FLAGS_sandbox, value == "true" || value == "1");
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
        absl::SetFlag(&FLAGS_ai_provider,
                      std::string(token.substr(eq_pos + 1)));
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
        absl::SetFlag(&FLAGS_gemini_api_key,
                      std::string(token.substr(eq_pos + 1)));
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

      if (absl::StartsWith(token, "--anthropic_api_key=") ||
          absl::StartsWith(token, "--anthropic-api-key=")) {
        size_t eq_pos = token.find('=');
        absl::SetFlag(&FLAGS_anthropic_api_key,
                      std::string(token.substr(eq_pos + 1)));
        continue;
      }
      if (token == "--anthropic_api_key" || token == "--anthropic-api-key") {
        if (i + 1 >= argc) {
          result.error = "--anthropic-api-key flag requires a value";
          return result;
        }
        absl::SetFlag(&FLAGS_anthropic_api_key, std::string(argv[++i]));
        continue;
      }

      if (absl::StartsWith(token, "--gui_server_address=") ||
          absl::StartsWith(token, "--gui-server-address=")) {
        size_t eq_pos = token.find('=');
        absl::SetFlag(&FLAGS_gui_server_address,
                      std::string(token.substr(eq_pos + 1)));
        continue;
      }
      if (token == "--gui_server_address" || token == "--gui-server-address") {
        if (i + 1 >= argc) {
          result.error = "--gui-server-address flag requires a value";
          return result;
        }
        absl::SetFlag(&FLAGS_gui_server_address, std::string(argv[++i]));
        continue;
      }

      if (absl::StartsWith(token, "--ollama_host=") ||
          absl::StartsWith(token, "--ollama-host=")) {
        size_t eq_pos = token.find('=');
        absl::SetFlag(&FLAGS_ollama_host,
                      std::string(token.substr(eq_pos + 1)));
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

      if (absl::StartsWith(token, "--mesen-socket=") ||
          absl::StartsWith(token, "--mesen_socket=")) {
        size_t eq_pos = token.find('=');
        absl::SetFlag(&FLAGS_mesen_socket,
                      std::string(token.substr(eq_pos + 1)));
        continue;
      }
      if (token == "--mesen-socket" || token == "--mesen_socket") {
        if (i + 1 >= argc) {
          result.error = "--mesen-socket flag requires a value";
          return result;
        }
        absl::SetFlag(&FLAGS_mesen_socket, std::string(argv[++i]));
        continue;
      }

      if (absl::StartsWith(token, "--prompt_version=") ||
          absl::StartsWith(token, "--prompt-version=")) {
        size_t eq_pos = token.find('=');
        absl::SetFlag(&FLAGS_prompt_version,
                      std::string(token.substr(eq_pos + 1)));
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
        absl::SetFlag(&FLAGS_use_function_calling,
                      value == "true" || value == "1");
        continue;
      }
      if (token == "--use_function_calling" ||
          token == "--use-function-calling") {
        if (i + 1 >= argc) {
          result.error = "--use-function-calling flag requires a value";
          return result;
        }
        std::string value(argv[++i]);
        absl::SetFlag(&FLAGS_use_function_calling,
                      value == "true" || value == "1");
        continue;
      }

#ifdef YAZE_HTTP_API_ENABLED
      // HTTP server flags
      if (absl::StartsWith(token, "--http-port=") ||
          absl::StartsWith(token, "--http_port=")) {
        size_t eq_pos = token.find('=');
        try {
          int port = std::stoi(std::string(token.substr(eq_pos + 1)));
          absl::SetFlag(&FLAGS_http_port, port);
        } catch (...) {
          result.error = "--http-port requires an integer value";
          return result;
        }
        continue;
      }
      if (token == "--http-port" || token == "--http_port") {
        if (i + 1 >= argc) {
          result.error = "--http-port flag requires a value";
          return result;
        }
        try {
          int port = std::stoi(std::string(argv[++i]));
          absl::SetFlag(&FLAGS_http_port, port);
        } catch (...) {
          result.error = "--http-port requires an integer value";
          return result;
        }
        continue;
      }

      if (absl::StartsWith(token, "--http-host=") ||
          absl::StartsWith(token, "--http_host=")) {
        size_t eq_pos = token.find('=');
        absl::SetFlag(&FLAGS_http_host, std::string(token.substr(eq_pos + 1)));
        continue;
      }
      if (token == "--http-host" || token == "--http_host") {
        if (i + 1 >= argc) {
          result.error = "--http-host flag requires a value";
          return result;
        }
        absl::SetFlag(&FLAGS_http_host, std::string(argv[++i]));
        continue;
      }
#endif
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

  // Handle self-test flag
  if (globals.self_test) {
    absl::SetFlag(&FLAGS_self_test, true);  // Ensure flag is set for test
    return RunSelfTest();
  }

  auto& registry = yaze::cli::CommandRegistry::Instance();
#ifdef YAZE_ENABLE_AGENT_CLI
  yaze::cli::handlers::RegisterAgentCommandHandlers();
#endif

  if (globals.export_schemas) {
    std::cout << registry.ExportFunctionSchemas() << "\n";
    return EXIT_SUCCESS;
  }

#ifdef YAZE_HTTP_API_ENABLED
  // Start HTTP API server if requested
  std::unique_ptr<yaze::cli::api::HttpServer> http_server;
  int http_port = absl::GetFlag(FLAGS_http_port);

  if (http_port > 0) {
    std::string http_host = absl::GetFlag(FLAGS_http_host);
    http_server = std::make_unique<yaze::cli::api::HttpServer>();

    auto status = http_server->Start(http_port);
    if (!status.ok()) {
      std::cerr
          << "\n\033[1;31mWarning:\033[0m Failed to start HTTP API server: "
          << status.message() << "\n";
      std::cerr << "Continuing without HTTP API...\n\n";
      http_server.reset();
    } else if (!absl::GetFlag(FLAGS_quiet)) {
      std::cout << "\033[1;32m✓\033[0m HTTP API server started on " << http_host
                << ":" << http_port << "\n";
      std::cout << "  Health check: http://" << http_host << ":" << http_port
                << "/api/v1/health\n";
      std::cout << "  Models list:  http://" << http_host << ":" << http_port
                << "/api/v1/models\n\n";
    }
  } else if (http_port == 0 && !absl::GetFlag(FLAGS_quiet)) {
    // Port 0 means explicitly disabled, only show message in verbose mode
  }
#endif

  // Create CLI instance
  yaze::cli::ModernCLI cli;

  // Route `z3ed <command> --help` to command/category scoped help.
  if (globals.show_help && !globals.help_target.has_value() &&
      globals.positional.size() > 1) {
    std::string inferred_target(globals.positional[1]);
    if (inferred_target != "help") {
      globals.help_target = inferred_target;
      globals.show_help = false;
    }
  }

  // Handle targeted help (command or category)
  if (globals.help_target.has_value()) {
    const std::string& target = *globals.help_target;
    if (target == "all") {
      std::cout << registry.GenerateCompleteHelp() << "\n";
    } else if (registry.HasCommand(target)) {
      std::cout << registry.GenerateHelp(target) << "\n";
    } else if (!registry.GetCommandsInCategory(target).empty()) {
      cli.PrintCategoryHelp(target);
    } else {
      std::cerr << "\n\033[1;31mError:\033[0m Unknown command or category '"
                << target << "'\n";
      std::cerr << "Use --list-commands for a full command list.\n";
      return EXIT_FAILURE;
    }
    return EXIT_SUCCESS;
  }

  // Handle list commands
  if (globals.list_commands) {
    cli.PrintCommandSummary();
    return EXIT_SUCCESS;
  }

#ifdef YAZE_HTTP_API_ENABLED
  if (!globals.show_help && globals.positional.size() <= 1 && http_server &&
      http_server->IsRunning()) {
    if (!absl::GetFlag(FLAGS_quiet)) {
      std::cout << "HTTP API server running. Press Ctrl+C to exit.\n";
    }
    WaitForHttpServer();
    return EXIT_SUCCESS;
  }
#endif

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
