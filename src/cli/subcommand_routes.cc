#include "cli/subcommand_routes.h"

#include <cstddef>
#include <iterator>
#include <optional>

#include "absl/status/status.h"
#include "absl/strings/match.h"
#include "absl/strings/str_cat.h"
#include "absl/strings/str_join.h"

namespace yaze::cli {
namespace {

struct RomSubcommand {
  absl::string_view name;
  absl::string_view usage;
};

// `z3ed rom <name>` runs the registered command `rom-<name>`.
constexpr RomSubcommand kRomSubcommands[] = {
    {"info", "z3ed rom info --rom=<path>"},
    {"validate", "z3ed rom validate --rom=<path>"},
    {"read", "z3ed rom read --address=0x1000 --length=16 --rom=<path>"},
    {"write", "z3ed rom write --address=0x1000 --value=0xFF --rom=<path>"},
    {"diff", "z3ed rom diff --rom_a=<a> --rom_b=<b>"},
    {"compare", "z3ed rom compare --rom=<path> --baseline=<path>"},
    {"doctor", "z3ed rom doctor --rom=<path>"},
};

enum class DebugBackend { kMesen, kGrpc };

struct DebugRoute {
  absl::string_view topic;
  absl::string_view alias = {};
  // Required second word, e.g. "read" in `debug mem read`.
  absl::string_view action = {};
  absl::string_view mesen_command;
  // Empty when the topic has no gRPC equivalent.
  absl::string_view grpc_command = {};
  // Appended to the arguments when routed to Mesen2.
  absl::string_view mesen_extra_arg = {};
  absl::string_view usage;
};

constexpr DebugRoute kDebugRoutes[] = {
    {.topic = "state",
     .alias = "gamestate",
     .mesen_command = "mesen-gamestate",
     .grpc_command = "emulator-get-state",
     .usage = "z3ed debug state [--backend=mesen|grpc]"},
    {.topic = "sprites",
     .mesen_command = "mesen-sprites",
     .usage = "z3ed debug sprites [--backend=mesen]"},
    {.topic = "cpu",
     .mesen_command = "mesen-cpu",
     .usage = "z3ed debug cpu [--backend=mesen]"},
    {.topic = "mem",
     .alias = "memory",
     .action = "read",
     .mesen_command = "mesen-memory-read",
     .grpc_command = "emulator-read-memory",
     .usage = "z3ed debug mem read --address=<addr> [--length=<n>]"},
    {.topic = "mem",
     .alias = "memory",
     .action = "write",
     .mesen_command = "mesen-memory-write",
     .grpc_command = "emulator-write-memory",
     .usage = "z3ed debug mem write --address=<addr> --value=<hex>"},
    {.topic = "disasm",
     .mesen_command = "mesen-disasm",
     .usage = "z3ed debug disasm --address=<addr> [--count=<n>]"},
    {.topic = "trace",
     .mesen_command = "mesen-trace",
     .usage = "z3ed debug trace --address=<addr> [--count=<n>]"},
    {.topic = "breakpoint",
     .alias = "bp",
     .mesen_command = "mesen-breakpoint",
     .usage = "z3ed debug breakpoint --address=<addr>"},
    {.topic = "control",
     .mesen_command = "mesen-control",
     .usage = "z3ed debug control --action=<pause|resume|step|reset>"},
    {.topic = "reset",
     .mesen_command = "mesen-control",
     .grpc_command = "emulator-reset",
     .mesen_extra_arg = "--action=reset",
     .usage = "z3ed debug reset [--backend=mesen|grpc]"},
};

bool MatchesTopic(const DebugRoute& route, absl::string_view word) {
  return word == route.topic || (!route.alias.empty() && word == route.alias);
}

struct DebugWords {
  DebugBackend backend = DebugBackend::kMesen;
  std::vector<std::string> words;
};

// Removes --backend / --debug-backend from `args`, wherever they appear.
absl::StatusOr<DebugWords> ExtractDebugBackend(
    absl::Span<const std::string> args) {
  DebugWords result;
  result.words.reserve(args.size());
  for (size_t i = 0; i < args.size(); ++i) {
    const std::string& token = args[i];
    std::optional<std::string> value;
    if (absl::StartsWith(token, "--backend=") ||
        absl::StartsWith(token, "--debug-backend=")) {
      value = token.substr(token.find('=') + 1);
    } else if (token == "--backend" || token == "--debug-backend") {
      if (i + 1 >= args.size()) {
        return absl::InvalidArgumentError(
            absl::StrCat(token, " requires a value (mesen or grpc)"));
      }
      value = args[++i];
    } else {
      result.words.push_back(token);
      continue;
    }

    if (*value == "mesen") {
      result.backend = DebugBackend::kMesen;
    } else if (*value == "grpc") {
      result.backend = DebugBackend::kGrpc;
    } else {
      return absl::InvalidArgumentError(absl::StrCat(
          "Unknown debug backend '", *value, "' (expected mesen or grpc)"));
    }
  }
  return result;
}

}  // namespace

absl::StatusOr<CommandInvocation> ResolveRomSubcommand(
    absl::Span<const std::string> args) {
  if (args.empty()) {
    return absl::InvalidArgumentError("rom requires a subcommand");
  }
  for (const RomSubcommand& subcommand : kRomSubcommands) {
    if (args[0] == subcommand.name) {
      return CommandInvocation{
          .command = absl::StrCat("rom-", subcommand.name),
          .args = {args.begin() + 1, args.end()},
      };
    }
  }
  return absl::InvalidArgumentError(
      absl::StrCat("Unknown rom subcommand: ", args[0]));
}

std::vector<absl::string_view> RomSubcommandNames() {
  std::vector<absl::string_view> names;
  names.reserve(std::size(kRomSubcommands));
  for (const RomSubcommand& subcommand : kRomSubcommands) {
    names.push_back(subcommand.name);
  }
  return names;
}

std::string RomSubcommandUsage() {
  std::string usage;
  for (const RomSubcommand& subcommand : kRomSubcommands) {
    absl::StrAppend(&usage, "  ", subcommand.usage, "\n");
  }
  return usage;
}

absl::StatusOr<CommandInvocation> ResolveDebugSubcommand(
    absl::Span<const std::string> args) {
  absl::StatusOr<DebugWords> parsed = ExtractDebugBackend(args);
  if (!parsed.ok()) {
    return parsed.status();
  }
  const DebugBackend backend = parsed->backend;
  const std::vector<std::string>& words = parsed->words;
  if (words.empty()) {
    return absl::InvalidArgumentError("debug requires a subcommand");
  }

  const std::string& topic = words[0];
  std::vector<absl::string_view> actions;
  for (const DebugRoute& route : kDebugRoutes) {
    if (!MatchesTopic(route, topic)) {
      continue;
    }
    size_t consumed_words = 1;
    if (!route.action.empty()) {
      actions.push_back(route.action);
      if (words.size() < 2 || words[1] != route.action) {
        continue;
      }
      consumed_words = 2;
    }

    const absl::string_view command = backend == DebugBackend::kGrpc
                                          ? route.grpc_command
                                          : route.mesen_command;
    if (command.empty()) {
      return absl::FailedPreconditionError(absl::StrCat(
          "debug ", topic, " has no gRPC backend; use --backend=mesen"));
    }

    CommandInvocation invocation{
        .command = std::string(command),
        .args = {words.begin() + consumed_words, words.end()},
    };
    if (backend == DebugBackend::kMesen && !route.mesen_extra_arg.empty()) {
      invocation.args.emplace_back(route.mesen_extra_arg);
    }
    return invocation;
  }

  if (!actions.empty()) {
    return absl::InvalidArgumentError(absl::StrCat(
        "debug ", topic, " requires one of: ", absl::StrJoin(actions, ", ")));
  }
  return absl::InvalidArgumentError(
      absl::StrCat("Unknown debug subcommand: ", topic));
}

std::string DebugSubcommandUsage() {
  std::string usage;
  for (const DebugRoute& route : kDebugRoutes) {
    absl::StrAppend(&usage, "  ", route.usage, "\n");
  }
  return usage;
}

}  // namespace yaze::cli
