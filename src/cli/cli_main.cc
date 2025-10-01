#include <iostream>
#include <string>

#include "absl/flags/flag.h"
#include "absl/flags/parse.h"
#include "absl/status/status.h"
#include "absl/strings/str_format.h"

#include "cli/modern_cli.h"
#include "cli/tui.h"

ABSL_FLAG(bool, tui, false, "Launch Text User Interface");
ABSL_FLAG(std::string, rom, "", "Path to the ROM file");
ABSL_FLAG(std::string, output, "", "Output file path");
ABSL_FLAG(bool, verbose, false, "Enable verbose output");
ABSL_FLAG(bool, dry_run, false, "Perform operations without making changes");
ABSL_FLAG(bool, backup, true, "Create a backup before modifying files");
ABSL_FLAG(std::string, test, "", "Name of the test to run");
ABSL_FLAG(bool, show_gui, false, "Show the test engine GUI");

int main(int argc, char* argv[]) {
  // Parse command line flags
  absl::ParseCommandLine(argc, argv);

  // Check if TUI mode is requested
  if (absl::GetFlag(FLAGS_tui)) {
    yaze::cli::ShowMain();
    return 0;
  }

  // Run CLI commands
  yaze::cli::ModernCLI cli;
  auto status = cli.Run(argc, argv);
  
  if (!status.ok()) {
    std::cerr << "Error: " << status.message() << std::endl;
    return 1;
  }

  return 0;
}
