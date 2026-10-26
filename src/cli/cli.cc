#include "cli/cli.h"

#include <ostream>
#include <utility>

#include "absl/status/statusor.h"
#include "absl/strings/str_cat.h"
#include "cli/service/command_registry.h"
#include "cli/subcommand_routes.h"
#include "cli/z3ed_ascii_logo.h"

namespace yaze::cli {
namespace {

// Word groups that expand to registered commands, e.g. `rom read` runs
// `rom-read`.
struct AliasGroup {
  absl::string_view name;
  absl::string_view title;
  absl::StatusOr<CommandInvocation> (*resolve)(absl::Span<const std::string>);
  std::string (*usage)();
};

constexpr AliasGroup kAliasGroups[] = {
    {"rom", "ROM subcommands", &ResolveRomSubcommand, &RomSubcommandUsage},
    {"debug", "Debug subcommands", &ResolveDebugSubcommand,
     &DebugSubcommandUsage},
};

const AliasGroup* FindAliasGroup(absl::string_view name) {
  for (const AliasGroup& group : kAliasGroups) {
    if (group.name == name) {
      return &group;
    }
  }
  return nullptr;
}

bool IsHelpFlag(absl::string_view token) {
  return token == "--help" || token == "-h";
}

constexpr absl::string_view kUsageText =
    "  z3ed [command] [flags]\n"
    "  z3ed help <command|category>  # Scoped help\n"
    "  z3ed --export-schemas         # JSON schemas for agents\n"
    "  z3ed --version                # Show version\n";

constexpr absl::string_view kCommonFlagsText =
    "  --rom=<path>           Path to ROM file\n"
    "  --sandbox              Run ROM commands in a sandbox copy\n"
    "  --quiet, -q            Suppress output\n"
    "  --ai_provider=<name>   AI provider (auto, ollama, gemini, openai,\n"
    "                         anthropic, mock)\n"
    "  --ai_model=<name>      Provider-specific model override\n"
    "  --version              Show version\n"
    "  --self-test            Run self-test diagnostics\n"
    "  --help <scope>         Show help for command or category\n"
    "  --export-schemas       Export command schemas as JSON\n";

constexpr absl::string_view kHttpFlagsText =
    "  --http-port=<port>     HTTP API server port (0=disabled)\n"
    "  --http-host=<host>     HTTP API display host (printed URLs)\n"
    "                         (no command keeps server alive until Ctrl+C)\n";

constexpr absl::string_view kExamplesText =
    "  z3ed rom-info --rom=zelda3.sfc\n"
    "  z3ed rom read --address=0x1000 --length=16 --rom=zelda3.sfc\n"
    "  z3ed debug state\n"
    "  z3ed message-search --rom=zelda3.sfc --query=\"Master Sword\"\n"
    "  z3ed dungeon-export --rom=zelda3.sfc --id=1\n";

}  // namespace

Cli::Cli(CommandRegistry& registry, Options options, std::ostream& out)
    : registry_(registry), options_(std::move(options)), out_(out) {}

absl::Status Cli::Run(absl::Span<const std::string> args) const {
  if (args.empty()) {
    PrintOverview();
    return absl::OkStatus();
  }

  const std::string& command = args[0];
  const absl::Span<const std::string> rest = args.subspan(1);

  if (command == "help") {
    if (rest.empty()) {
      PrintOverview();
      return absl::OkStatus();
    }
    return PrintScopedHelp(rest);
  }
  if (command == "agent") {
    return RunAgent(rest);
  }
  if (FindAliasGroup(command) != nullptr) {
    return RunAlias(command, rest);
  }
  if (registry_.HasCommand(command)) {
    return registry_.Execute(
        command, std::vector<std::string>(rest.begin(), rest.end()), nullptr);
  }
  if (!registry_.GetCommandsInCategory(command).empty()) {
    PrintCategoryHelp(command);
    return absl::OkStatus();
  }
  return absl::NotFoundError(absl::StrCat("Unknown command: ", command));
}

absl::Status Cli::RunAgent(absl::Span<const std::string> args) const {
  if (args.empty() || IsHelpFlag(args[0])) {
    PrintAgentHelp();
    return absl::OkStatus();
  }
  if (!options_.agent_handler) {
    return absl::FailedPreconditionError("Agent CLI is disabled in this build");
  }
  return options_.agent_handler(
      std::vector<std::string>(args.begin(), args.end()));
}

absl::Status Cli::RunAlias(absl::string_view group_name,
                           absl::Span<const std::string> args) const {
  if (args.empty() || IsHelpFlag(args[0])) {
    PrintAliasHelp(group_name);
    return absl::OkStatus();
  }

  const AliasGroup& group = *FindAliasGroup(group_name);
  absl::StatusOr<CommandInvocation> invocation = group.resolve(args);
  if (!invocation.ok()) {
    if (absl::IsInvalidArgument(invocation.status())) {
      PrintAliasHelp(group_name);
    }
    return invocation.status();
  }
  if (!registry_.HasCommand(invocation->command)) {
    return absl::NotFoundError(
        absl::StrCat("Command not found for ", group.name,
                     " subcommand: ", invocation->command));
  }
  return registry_.Execute(invocation->command, invocation->args, nullptr);
}

void Cli::PrintOverview() const {
  const util::AnsiStyle& style = options_.style;

  out_ << RenderLogo(style) << "\n";
  out_ << "  " << style.Emphasis()
       << "Yet Another Zelda3 Editor - AI-Powered CLI" << style.Reset()
       << "\n\n";

  out_ << style.Heading() << "USAGE:" << style.Reset() << "\n"
       << kUsageText << "\n";

  out_ << style.Heading() << "CATEGORIES:" << style.Reset() << "\n";
  for (const std::string& category : registry_.GetCategories()) {
    out_ << "  " << style.Highlight() << category << style.Reset() << " ("
         << registry_.GetCommandsInCategory(category).size() << " commands)\n";
  }
  out_ << "\n";

  out_ << style.Heading() << "COMMON FLAGS:" << style.Reset() << "\n"
       << kCommonFlagsText;
  if (options_.http_api_available) {
    out_ << kHttpFlagsText;
  }
  out_ << "  --mesen-socket=<path>  Override Mesen2 socket path\n\n";

  out_ << style.Heading() << "EXAMPLES:" << style.Reset() << "\n";
  if (options_.agent_handler) {
    out_ << "  z3ed agent simple-chat --rom=zelda3.sfc\n";
  }
  out_ << kExamplesText << "\n";

  out_ << "For detailed help: z3ed help <command>\n";
  out_ << "For all commands:  z3ed --list-commands\n\n";
}

absl::Status Cli::PrintScopedHelp(absl::Span<const std::string> words) const {
  if (words.empty()) {
    PrintOverview();
    return absl::OkStatus();
  }

  const util::AnsiStyle& style = options_.style;
  const std::string& target = words[0];
  if (target == "all") {
    out_ << registry_.GenerateCompleteHelp(style) << "\n";
    return absl::OkStatus();
  }
  if (registry_.HasCommand(target)) {
    out_ << registry_.GenerateHelp(target, style) << "\n";
    return absl::OkStatus();
  }
  if (target == "agent") {
    if (words.size() > 1 && registry_.HasCommand(words[1])) {
      out_ << registry_.GenerateHelp(words[1], style) << "\n";
    } else {
      PrintAgentHelp();
    }
    return absl::OkStatus();
  }
  if (const AliasGroup* group = FindAliasGroup(target)) {
    if (words.size() > 1) {
      absl::StatusOr<CommandInvocation> invocation =
          group->resolve(words.subspan(1));
      if (invocation.ok() && registry_.HasCommand(invocation->command)) {
        out_ << registry_.GenerateHelp(invocation->command, style) << "\n";
        return absl::OkStatus();
      }
    }
    PrintAliasHelp(target);
    return absl::OkStatus();
  }
  if (!registry_.GetCommandsInCategory(target).empty()) {
    PrintCategoryHelp(target);
    return absl::OkStatus();
  }
  return absl::NotFoundError(
      absl::StrCat("Unknown command or category '", target,
                   "'. Use --list-commands for a full command list."));
}

void Cli::PrintAgentHelp() const {
  const util::AnsiStyle& style = options_.style;
  out_ << "\n" << style.Heading() << "Agent command:" << style.Reset() << "\n";
  if (!options_.agent_handler) {
    out_ << "  This build was compiled without agent CLI support.\n"
         << "  Rebuild with `YAZE_ENABLE_AGENT_CLI` to enable `z3ed agent`.\n";
    return;
  }
  out_ << "  z3ed agent <subcommand> [flags]\n\n";
  if (!registry_.GetCommandsInCategory("agent").empty()) {
    PrintCategoryHelp("agent");
  }
  out_ << "Use `z3ed help agent <subcommand>` for scoped details.\n";
}

void Cli::PrintAliasHelp(absl::string_view group_name) const {
  const AliasGroup& group = *FindAliasGroup(group_name);
  const util::AnsiStyle& style = options_.style;
  const std::string category(group.name);

  out_ << "\n"
       << style.Heading() << group.title << ":" << style.Reset() << "\n"
       << group.usage() << "\n";
  if (!registry_.GetCommandsInCategory(category).empty()) {
    out_ << "Equivalent direct commands:\n"
         << registry_.GenerateCategoryHelp(category, style) << "\n";
  }
  out_ << "Use `z3ed help " << group.name
       << " <subcommand>` for command-specific flags.\n";
}

void Cli::PrintCategoryHelp(const std::string& category) const {
  const std::vector<std::string> commands =
      registry_.GetCommandsInCategory(category);

  out_ << "Category: " << category << "\n\n";
  if (commands.empty()) {
    out_ << "  No commands in this category.\n";
    return;
  }

  for (const std::string& name : commands) {
    const CommandRegistry::CommandMetadata* metadata =
        registry_.GetMetadata(name);
    if (metadata == nullptr) {
      continue;
    }

    out_ << "  " << name << " - " << metadata->description << "\n";
    if (!metadata->usage.empty()) {
      out_ << "    Usage: " << metadata->usage << "\n";
    }

    std::string requirements;
    if (metadata->requires_rom) {
      requirements += "ROM ";
    }
    if (metadata->requires_grpc) {
      requirements += "gRPC ";
    }
    if (!requirements.empty()) {
      out_ << "    Requires: " << requirements << "\n";
    }
    out_ << "\n";
  }
}

void Cli::PrintCommandSummary() const {
  const std::vector<std::string> categories = registry_.GetCategories();

  out_ << "Z3ED Command Summary\n\n";
  for (const std::string& category : categories) {
    for (const std::string& name : registry_.GetCommandsInCategory(category)) {
      if (const auto* metadata = registry_.GetMetadata(name)) {
        out_ << "  " << name << " [" << metadata->category << "] - "
             << metadata->description << "\n";
      }
    }
  }
  out_ << "\nTotal: " << registry_.Count() << " commands across "
       << categories.size() << " categories\n";
}

}  // namespace yaze::cli
