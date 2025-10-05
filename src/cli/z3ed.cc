#include "cli/z3ed.h"

#include <cstring>
#include <iostream>
#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

#include "absl/flags/flag.h"
#include "absl/flags/parse.h"
#include "absl/flags/usage.h"
#include "cli/modern_cli.h"
#include "cli/tui.h"
#include "util/macro.h"

// Define additional z3ed-specific flags
ABSL_FLAG(bool, quiet, false, "Suppress non-essential output");
ABSL_FLAG(bool, interactive, false, "Launch interactive TUI mode");
ABSL_FLAG(bool, version, false, "Show version information");

#ifdef _WIN32
extern "C" int SDL_main(int argc, char *argv[]) {
#else
int main(int argc, char *argv[]) {
#endif
  // Set up usage message
  absl::SetProgramUsageMessage(R"(
z3ed - Yet Another Zelda3 Editor CLI

A command-line interface for inspecting and modifying Zelda 3: A Link to the
Past ROM files. Supports both interactive commands and batch processing.

USAGE:
  z3ed [command] [flags]
  z3ed --rom=<path> [command]
  z3ed --interactive  # Launch TUI mode

COMMANDS:
  agent              AI-powered conversational agent for ROM inspection
  rom                ROM file operations (info, validate, diff, etc.)
  dungeon            Dungeon inspection and editing
  overworld          Overworld inspection and editing
  message            Message/dialogue inspection and editing
  gfx                Graphics operations (export, import)
  palette            Palette operations
  patch              Apply patches (BPS, Asar)
  project            Project management (init, build)

FLAGS:
  --rom=<path>       Path to the ROM file
  --quiet            Suppress non-essential output
  --interactive      Launch interactive TUI mode
  --version          Show version information
  --help             Show this help message

EXAMPLES:
  # Interactive TUI mode
  z3ed --interactive

  # Get ROM information
  z3ed rom info --rom=zelda3.sfc

  # AI agent conversation
  z3ed agent test-conversation --rom=zelda3.sfc

  # List all messages
  z3ed agent message-list --rom=zelda3.sfc --format=json

  # Search for specific message text
  z3ed agent message-search --rom=zelda3.sfc --query="Master Sword"

  # Describe dungeon room
  z3ed agent dungeon-describe-room --rom=zelda3.sfc --room=0x02A

For more information about each command, run:
  z3ed [command] --help
)");

  // Parse command line flags
  std::vector<char*> remaining = absl::ParseCommandLine(argc, argv);

  // Handle version flag
  if (absl::GetFlag(FLAGS_version)) {
    std::cout << "z3ed version 0.4.0\n";
    std::cout << "Yet Another Zelda3 Editor - Command Line Interface\n";
    return EXIT_SUCCESS;
  }

  // Handle interactive TUI mode
  if (absl::GetFlag(FLAGS_interactive)) {
    yaze::cli::ShowMain();
    return EXIT_SUCCESS;
  }

  // If no commands specified, show usage
  if (remaining.size() <= 1) {
    std::cout << absl::ProgramUsageMessage() << std::endl;
    return EXIT_SUCCESS;
  }

  // Use modern CLI for command dispatching
  yaze::cli::ModernCLI cli;
  auto status = cli.Run(argc, argv);

  if (!status.ok()) {
    std::cerr << "Error: " << status.message() << std::endl;
    return EXIT_FAILURE;
  }

  return EXIT_SUCCESS;
}