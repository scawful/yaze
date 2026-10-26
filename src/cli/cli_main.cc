#include <cstdio>
#include <cstdlib>
#include <iostream>
#include <iterator>
#include <optional>
#include <sstream>
#include <string>
#include <vector>

#include "absl/flags/declare.h"
#include "absl/flags/flag.h"
#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "absl/strings/str_format.h"
#include "cli/cli.h"
#include "cli/global_flags.h"
#include "cli/service/command_registry.h"
#include "cli/util/ansi_style.h"
#include "cli/z3ed_ascii_logo.h"
#include "util/platform_paths.h"
#include "yaze_config.h"

#ifdef YAZE_ENABLE_AGENT_CLI
#include "cli/handlers/agent_command_registration.h"
#endif

#ifdef YAZE_HTTP_API_ENABLED
#include <chrono>
#include <csignal>
#include <memory>
#include <thread>

#include "app/service/render_service.h"
#include "cli/service/api/http_server.h"
#include "rom/rom.h"
#include "util/macro.h"
#include "zelda3/game_data.h"
#endif

ABSL_DECLARE_FLAG(bool, quiet);
ABSL_DECLARE_FLAG(std::string, rom);

#ifdef YAZE_HTTP_API_ENABLED
ABSL_FLAG(int, http_port, 0, "HTTP API server port (0 = disabled)");
ABSL_FLAG(std::string, http_host, "localhost",
          "Host shown in printed HTTP API URLs");
#endif

namespace {

using yaze::cli::Cli;
using yaze::cli::CommandRegistry;
using yaze::cli::GlobalOptions;
using yaze::cli::util::AnsiStyle;

int ReportFailure(const absl::Status& status, const AnsiStyle& style) {
  std::cerr << "\n"
            << style.Failure() << "Error:" << style.Reset() << " "
            << status.message() << "\n";
  std::cerr << "Use --help for usage information.\n";
  return EXIT_FAILURE;
}

int ExitCode(const absl::Status& status, const AnsiStyle& style) {
  return status.ok() ? EXIT_SUCCESS : ReportFailure(status, style);
}

void PrintVersion(const AnsiStyle& style) {
  std::cout << yaze::cli::RenderLogo(style) << "\n";
  std::cout << absl::StrFormat("  Version %d.%d.%d\n", YAZE_VERSION_MAJOR,
                               YAZE_VERSION_MINOR, YAZE_VERSION_PATCH);
  std::cout << "  Yet Another Zelda3 Editor - Command Line Interface\n";
  std::cout << "  https://github.com/scawful/yaze\n\n";
}

Cli::Options MakeCliOptions(const AnsiStyle& style) {
  Cli::Options options;
  options.style = style;
#ifdef YAZE_ENABLE_AGENT_CLI
  options.agent_handler = &yaze::cli::handlers::HandleAgentCommand;
#endif
#ifdef YAZE_HTTP_API_ENABLED
  options.http_api_available = true;
#endif
  return options;
}

// The command line help was requested for, or an empty list for the general
// overview. std::nullopt when no help was requested.
std::optional<std::vector<std::string>> HelpRequest(
    const GlobalOptions& options) {
  if (options.help_target.has_value()) {
    return std::vector<std::string>{*options.help_target};
  }
  if (!options.show_help) {
    return std::nullopt;
  }
  if (options.positional.empty() || options.positional[0] == "help") {
    return std::vector<std::string>{};
  }
  return options.positional;
}

bool GlobalFlagParsingWorks() {
  const std::string saved_rom = absl::GetFlag(FLAGS_rom);
  const std::vector<std::string> args = {"--rom=self-test.sfc", "rom-info",
                                         "--format=json"};
  absl::StatusOr<GlobalOptions> options = yaze::cli::ParseGlobalFlags(args);
  const bool works = options.ok() &&
                     absl::GetFlag(FLAGS_rom) == "self-test.sfc" &&
                     options->positional ==
                         std::vector<std::string>{"rom-info", "--format=json"};
  absl::SetFlag(&FLAGS_rom, saved_rom);
  return works;
}

bool OverviewRenders(CommandRegistry& registry) {
  std::ostringstream out;
  Cli(registry, Cli::Options{}, out).PrintOverview();
  return !out.str().empty();
}

int RunSelfTest(const AnsiStyle& style) {
  CommandRegistry& registry = CommandRegistry::Instance();
  struct Check {
    const char* name;
    bool passed;
  };
  // Asset checks must pass from portable, bundle and FHS package layouts,
  // without depending on the source checkout.
  const Check checks[] = {
      {"Command registry populated", registry.Count() > 0},
      {"Global flag parsing", GlobalFlagParsingWorks()},
      {"Help overview renders", OverviewRenders(registry)},
      {"Agent catalogue asset available",
       yaze::util::PlatformPaths::FindAsset("agent/prompt_catalogue.yaml")
           .ok()},
      {"Overworld patch asset available",
       yaze::util::PlatformPaths::FindAsset(
           "patches/Overworld/TailMapExpansion.asm")
           .ok()},
  };

  std::cout << "\n"
            << style.Heading() << "=== z3ed Self-Test ===" << style.Reset()
            << "\n\n";
  int failed = 0;
  for (const Check& check : checks) {
    if (check.passed) {
      std::cout << "  " << style.Success() << "✓" << style.Reset();
    } else {
      std::cout << "  " << style.Failure() << "✗" << style.Reset();
      ++failed;
    }
    std::cout << " " << check.name << "\n";
  }
#ifdef YAZE_HTTP_API_ENABLED
  std::cout << "  HTTP API: compiled in\n";
#else
  std::cout << "  HTTP API: not compiled in\n";
#endif

  const int passed = static_cast<int>(std::size(checks)) - failed;
  std::cout << "\n"
            << style.Heading() << "=== Results ===" << style.Reset() << "\n";
  std::cout << "  Passed: " << style.Success() << passed << style.Reset()
            << "\n";
  std::cout << "  Failed: " << style.Failure() << failed << style.Reset()
            << "\n";

  if (failed == 0) {
    std::cout << "\n"
              << style.Success() << "All self-tests passed!" << style.Reset()
              << "\n\n";
    return EXIT_SUCCESS;
  }
  std::cout << "\n"
            << style.Failure() << "Some self-tests failed." << style.Reset()
            << "\n\n";
  return EXIT_FAILURE;
}

#ifdef YAZE_HTTP_API_ENABLED
volatile std::sig_atomic_t g_shutdown_requested = 0;

void RequestShutdown(int) {
  g_shutdown_requested = 1;
}

void WaitForShutdownSignal() {
  std::signal(SIGINT, RequestShutdown);
  std::signal(SIGTERM, RequestShutdown);
  while (g_shutdown_requested == 0) {
    std::this_thread::sleep_for(std::chrono::milliseconds(200));
  }
}

// Owns the HTTP API server and the ROM-backed services its endpoints read.
class HttpApiSession {
 public:
  absl::Status Start(int port) {
    if (port <= 0 || port > 65535) {
      return absl::InvalidArgumentError(
          absl::StrFormat("--http-port must be 1-65535, got %d", port));
    }
    server_ = std::make_unique<yaze::cli::api::HttpServer>();
    if (render_service_) {
      server_->SetRenderServiceSource(
          [service = render_service_.get()] { return service; });
    }
    absl::Status status = server_->Start(port);
    if (!status.ok()) {
      server_.reset();
    }
    return status;
  }

  // Serves headless dungeon renders for `rom_path`. Call before Start().
  absl::Status LoadRenderService(const std::string& rom_path) {
    auto rom = std::make_unique<yaze::Rom>();
    RETURN_IF_ERROR(rom->LoadFromFile(rom_path));
    auto game_data = std::make_unique<yaze::zelda3::GameData>();
    RETURN_IF_ERROR(yaze::zelda3::LoadGameData(*rom, *game_data));
    rom_ = std::move(rom);
    game_data_ = std::move(game_data);
    render_service_ = std::make_unique<yaze::app::service::RenderService>(
        rom_.get(), game_data_.get());
    return absl::OkStatus();
  }

  bool IsRunning() const { return server_ && server_->IsRunning(); }

 private:
  std::unique_ptr<yaze::Rom> rom_;
  std::unique_ptr<yaze::zelda3::GameData> game_data_;
  std::unique_ptr<yaze::app::service::RenderService> render_service_;
  // Declared last so it is destroyed first: the server thread must stop
  // before the services it reads are freed.
  std::unique_ptr<yaze::cli::api::HttpServer> server_;
};
#endif

}  // namespace

int main(int argc, char* argv[]) {
  const AnsiStyle out_style = AnsiStyle::ForStream(stdout);
  const AnsiStyle err_style = AnsiStyle::ForStream(stderr);

  std::vector<std::string> args;
  if (argc > 1) {
    args.assign(argv + 1, argv + argc);
  }
  absl::StatusOr<GlobalOptions> parsed = yaze::cli::ParseGlobalFlags(args);
  if (!parsed.ok()) {
    return ReportFailure(parsed.status(), err_style);
  }
  const GlobalOptions& options = *parsed;

  if (options.show_version) {
    PrintVersion(out_style);
    return EXIT_SUCCESS;
  }
  if (options.self_test) {
    return RunSelfTest(out_style);
  }

  CommandRegistry& registry = CommandRegistry::Instance();
#ifdef YAZE_ENABLE_AGENT_CLI
  yaze::cli::handlers::RegisterAgentCommandHandlers();
#endif

  if (options.export_schemas) {
    std::cout << registry.ExportFunctionSchemas() << "\n";
    return EXIT_SUCCESS;
  }

  const Cli cli(registry, MakeCliOptions(out_style), std::cout);

  if (std::optional<std::vector<std::string>> words = HelpRequest(options)) {
    return ExitCode(cli.PrintScopedHelp(*words), err_style);
  }
  if (options.list_commands) {
    cli.PrintCommandSummary();
    return EXIT_SUCCESS;
  }

#ifdef YAZE_HTTP_API_ENABLED
  HttpApiSession http_api;
  if (const int port = absl::GetFlag(FLAGS_http_port); port != 0) {
    const bool quiet = absl::GetFlag(FLAGS_quiet);
    const std::string rom_path = absl::GetFlag(FLAGS_rom);
    if (!rom_path.empty()) {
      absl::Status status = http_api.LoadRenderService(rom_path);
      if (!status.ok() && !quiet) {
        std::cerr << err_style.Highlight() << "Warning:" << err_style.Reset()
                  << " HTTP render endpoints disabled: " << status.message()
                  << "\n";
      }
    }

    absl::Status status = http_api.Start(port);
    if (!status.ok()) {
      std::cerr << "\n"
                << err_style.Failure() << "Warning:" << err_style.Reset()
                << " Failed to start HTTP API server: " << status.message()
                << "\nContinuing without HTTP API...\n\n";
    } else if (!quiet) {
      const std::string host = absl::GetFlag(FLAGS_http_host);
      std::cout << out_style.Success() << "✓" << out_style.Reset()
                << " HTTP API server started on " << host << ":" << port
                << "\n";
      std::cout << "  Health check: http://" << host << ":" << port
                << "/api/v1/health\n";
      std::cout << "  Models list:  http://" << host << ":" << port
                << "/api/v1/models\n\n";
    }

    if (options.positional.empty() && http_api.IsRunning()) {
      if (!quiet) {
        std::cout << "HTTP API server running. Press Ctrl+C to exit.\n";
      }
      WaitForShutdownSignal();
      return EXIT_SUCCESS;
    }
  }
#endif

  return ExitCode(cli.Run(options.positional), err_style);
}
