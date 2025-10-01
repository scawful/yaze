#include <cstdlib>
#include <iostream>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include "absl/flags/flag.h"
#include "absl/strings/str_format.h"
#include "absl/strings/match.h"

#include "cli/modern_cli.h"
#include "cli/tui.h"
#include "yaze_config.h"

ABSL_FLAG(bool, tui, false, "Launch Text User Interface");
ABSL_FLAG(std::string, rom, "", "Path to the ROM file");

namespace {

struct ParsedGlobals {
  std::vector<char*> positional;
  bool show_help = false;
  bool show_version = false;
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

      if (token == "--help" || token == "-h") {
        result.show_help = true;
        continue;
      }

      if (token == "--version") {
        result.show_version = true;
        continue;
      }

      if (token == "--tui") {
        absl::SetFlag(&FLAGS_tui, true);
        continue;
      }

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
    }

    result.positional.push_back(current);
  }

  return result;
}

void PrintVersion() {
  std::cout << absl::StrFormat("yaze %d.%d.%d", YAZE_VERSION_MAJOR,
                               YAZE_VERSION_MINOR, YAZE_VERSION_PATCH)
            << std::endl;
}

}  // namespace

int main(int argc, char* argv[]) {
  ParsedGlobals globals = ParseGlobalFlags(argc, argv);

  if (globals.error.has_value()) {
    std::cerr << "Error: " << *globals.error << std::endl;
    return EXIT_FAILURE;
  }

  if (globals.show_version) {
    PrintVersion();
    return EXIT_SUCCESS;
  }

  // Check if TUI mode is requested
  if (absl::GetFlag(FLAGS_tui)) {
    yaze::cli::ShowMain();
    return EXIT_SUCCESS;
  }

  yaze::cli::ModernCLI cli;

  if (globals.show_help) {
    cli.PrintTopLevelHelp();
    return EXIT_SUCCESS;
  }

  if (globals.positional.size() <= 1) {
    cli.PrintTopLevelHelp();
    return EXIT_SUCCESS;
  }

  // Run CLI commands
  auto status = cli.Run(static_cast<int>(globals.positional.size()),
                        globals.positional.data());
  
  if (!status.ok()) {
    std::cerr << "Error: " << status.message() << std::endl;
    return EXIT_FAILURE;
  }

  return EXIT_SUCCESS;
}
