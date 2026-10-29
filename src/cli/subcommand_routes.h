#ifndef YAZE_CLI_SUBCOMMAND_ROUTES_H_
#define YAZE_CLI_SUBCOMMAND_ROUTES_H_

#include <string>
#include <vector>

#include "absl/status/statusor.h"
#include "absl/strings/string_view.h"
#include "absl/types/span.h"

namespace yaze::cli {

// A registered command name and the arguments to execute it with.
struct CommandInvocation {
  std::string command;
  std::vector<std::string> args;

  bool operator==(const CommandInvocation&) const = default;
};

// `z3ed rom <subcommand> ...` aliases for the rom-* commands. `args` starts
// at the subcommand.
absl::StatusOr<CommandInvocation> ResolveRomSubcommand(
    absl::Span<const std::string> args);
std::vector<absl::string_view> RomSubcommandNames();
std::string RomSubcommandUsage();

// `z3ed debug <topic> ...` aliases. The command is chosen by
// --backend=mesen|grpc (default mesen), which may appear anywhere in `args`.
absl::StatusOr<CommandInvocation> ResolveDebugSubcommand(
    absl::Span<const std::string> args);
std::string DebugSubcommandUsage();

}  // namespace yaze::cli

#endif  // YAZE_CLI_SUBCOMMAND_ROUTES_H_
