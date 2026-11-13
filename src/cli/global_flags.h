#ifndef YAZE_CLI_GLOBAL_FLAGS_H_
#define YAZE_CLI_GLOBAL_FLAGS_H_

#include <optional>
#include <string>
#include <vector>

#include "absl/status/statusor.h"
#include "absl/types/span.h"

namespace yaze::cli {

// Process-level options z3ed handles before command dispatch.
struct GlobalOptions {
  // The command name followed by its arguments, with global flags removed.
  std::vector<std::string> positional;
  bool show_help = false;
  bool show_version = false;
  bool list_commands = false;
  bool self_test = false;
  bool export_schemas = false;
  // Set by `--help=<target>` or `--help <target>`.
  std::optional<std::string> help_target;
};

// Parses the z3ed command line, excluding the program name.
//
// Global flags such as --rom and --ai-provider may appear anywhere before
// "--" and are written to their absl flags; dash and underscore spellings are
// equivalent. Every other token, and everything after "--", is forwarded to
// the command in `positional`.
absl::StatusOr<GlobalOptions> ParseGlobalFlags(
    absl::Span<const std::string> args);

}  // namespace yaze::cli

#endif  // YAZE_CLI_GLOBAL_FLAGS_H_
