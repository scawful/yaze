#include "cli/service/command_registry.h"

#include <algorithm>
#include <iostream>
#include <sstream>

#include "absl/strings/str_cat.h"
#include "absl/strings/str_format.h"
#include "absl/strings/str_join.h"
#include "cli/handlers/command_handlers.h"

namespace yaze {
namespace cli {

namespace {
std::string EscapeJson(const std::string& input) {
  std::string escaped;
  escaped.reserve(input.size());
  for (char c : input) {
    switch (c) {
      case '\\\\':
        escaped.append("\\\\");
        break;
      case '\"':
        escaped.append("\\\"");
        break;
      case '\n':
        escaped.append("\\n");
        break;
      case '\r':
        escaped.append("\\r");
        break;
      case '\t':
        escaped.append("\\t");
        break;
      default:
        escaped.push_back(c);
    }
  }
  return escaped;
}

void AppendStringArray(std::ostringstream& out,
                       const std::vector<std::string>& values) {
  out << "[";
  for (size_t i = 0; i < values.size(); ++i) {
    if (i > 0)
      out << ", ";
    out << "\"" << EscapeJson(values[i]) << "\"";
  }
  out << "]";
}
}  // namespace

CommandRegistry& CommandRegistry::Instance() {
  static CommandRegistry instance;
  static bool initialized = false;
  if (!initialized) {
    instance.RegisterCliCommands();
    initialized = true;
  }
  return instance;
}

void CommandRegistry::Register(
    std::unique_ptr<resources::CommandHandler> handler,
    const CommandMetadata& metadata) {
  std::string name = handler->GetName();

  // Store metadata
  metadata_[name] = metadata;

  // Register aliases
  for (const auto& alias : metadata.aliases) {
    aliases_[alias] = name;
  }

  // Store handler
  handlers_[name] = std::move(handler);
}

void CommandRegistry::RegisterHandlers(
    std::vector<std::unique_ptr<resources::CommandHandler>> handlers) {
  for (auto& handler : handlers) {
    std::string name = handler->GetName();

    if (handlers_.find(name) != handlers_.end()) {
      continue;
    }

    // Build metadata from handler
    auto descriptor = handler->Describe();
    CommandMetadata metadata;
    metadata.name = name;
    metadata.usage = handler->GetUsage();
    metadata.available_to_agent = true;  // Most commands available to agent
    metadata.requires_rom = handler->RequiresRom();
    metadata.requires_grpc = false;

    // Categorize and enhance metadata based on command type
    if (name.find("resource-") == 0) {
      metadata.category = "resource";
      metadata.description = "Resource inspection and search";
      if (name == "resource-list") {
        metadata.examples = {"z3ed resource-list --type=dungeon --format=json",
                             "z3ed resource-list --type=sprite --format=table"};
      }
    } else if (name.find("dungeon-") == 0) {
      metadata.category = "dungeon";
      metadata.description = "Dungeon inspection and editing";
      if (name == "dungeon-describe-room") {
        metadata.examples = {
            "z3ed dungeon-describe-room --room=5 --format=json"};
      }
    } else if (name.find("overworld-") == 0) {
      metadata.category = "overworld";
      metadata.description = "Overworld inspection and editing";
      if (name == "overworld-find-tile") {
        metadata.examples = {
            "z3ed overworld-find-tile --tile=0x42 --format=json"};
      }
    } else if (name.find("rom-") == 0) {
      metadata.category = "rom";
      metadata.description = "ROM inspection and validation";
      if (name == "rom-info") {
        metadata.examples = {"z3ed rom-info --rom=zelda3.sfc"};
      } else if (name == "rom-validate") {
        metadata.examples = {"z3ed rom-validate --rom=zelda3.sfc"};
      } else if (name == "rom-doctor") {
        metadata.examples = {"z3ed rom-doctor --rom=zelda3.sfc --format=json"};
      } else if (name == "rom-diff") {
        metadata.examples = {
            "z3ed rom-diff --rom_a=base.sfc --rom_b=target.sfc"};
      } else if (name == "rom-compare") {
        metadata.examples = {
            "z3ed rom-compare --rom=target.sfc --baseline=vanilla.sfc"};
      }
    } else if (name.find("emulator-") == 0) {
      metadata.category = "emulator";
      metadata.description = "Emulator control and debugging";
      metadata.requires_grpc = true;
      if (name == "emulator-set-breakpoint") {
        metadata.examples = {
            "z3ed emulator-set-breakpoint --address=0x83D7 --description='NMI "
            "handler'"};
      }
    } else if (name.find("gui-") == 0) {
      metadata.category = "gui";
      metadata.description = "GUI automation";
      metadata.requires_grpc = true;
    } else if (name.find("hex-") == 0) {
      metadata.category = "graphics";
      metadata.description = "Hex data manipulation";
    } else if (name.find("palette-") == 0) {
      metadata.category = "graphics";
      metadata.description = "Palette operations";
    } else if (name.find("sprite-") == 0) {
      metadata.category = "graphics";
      metadata.description = "Sprite operations";
    } else if (name.find("message-") == 0 || name.find("dialogue-") == 0) {
      metadata.category = "game";
      metadata.description = name.find("message-") == 0 ? "Message inspection"
                                                        : "Dialogue inspection";
    } else if (name.find("music-") == 0) {
      metadata.category = "game";
      metadata.description = "Music/audio inspection";
    } else if (name == "simple-chat" || name == "chat") {
      metadata.category = "agent";
      metadata.description = "AI conversational agent";
      metadata.available_to_agent = false;  // Meta-command
      metadata.requires_rom = false;
      metadata.examples = {
          "z3ed simple-chat --rom=zelda3.sfc",
          "z3ed simple-chat \"What dungeons exist?\" --rom=zelda3.sfc"};
    } else if (name.find("tools-") == 0) {
      metadata.category = "tools";
      if (name == "tools-list") {
        metadata.description = "List available test helper tools";
        metadata.requires_rom = false;
        metadata.available_to_agent = true;
      } else if (name == "tools-harness-state") {
        metadata.description = "Generate WRAM state for test harnesses";
        metadata.examples = {
            "z3ed tools-harness-state --rom=zelda3.sfc --output=state.h"};
      } else if (name == "tools-extract-values") {
        metadata.description = "Extract vanilla ROM values";
        metadata.examples = {"z3ed tools-extract-values --rom=zelda3.sfc"};
      } else if (name == "tools-extract-golden") {
        metadata.description = "Extract comprehensive golden data for testing";
        metadata.examples = {
            "z3ed tools-extract-golden --rom=zelda3.sfc --output=golden.h"};
      } else if (name == "tools-patch-v3") {
        metadata.description = "Create v3 ZSCustomOverworld patched ROM";
        metadata.examples = {
            "z3ed tools-patch-v3 --rom=zelda3.sfc --output=patched.sfc"};
      } else {
        metadata.description = "Test helper tool";
      }
    } else if (name.find("test-") == 0) {
      metadata.category = "test";
      metadata.description = "Test discovery and execution";
      if (name == "test-list") {
        metadata.requires_rom = false;
        metadata.examples = {"z3ed test-list", "z3ed test-list --format json"};
      } else if (name == "test-run") {
        metadata.examples = {"z3ed test-run --label stable",
                             "z3ed test-run --label gui"};
      } else if (name == "test-status") {
        metadata.requires_rom = false;
        metadata.examples = {"z3ed test-status --format json"};
      }
    } else {
      metadata.category = "misc";
      metadata.description = "Miscellaneous command";
    }

    // Prefer handler-provided summary if present
    if (!descriptor.summary.empty() &&
        descriptor.summary != "Command summary not provided.") {
      metadata.description = descriptor.summary;
    }

    // Keep TODO reference if supplied by handler
    if (!descriptor.todo_reference.empty() &&
        descriptor.todo_reference != "todo#unassigned") {
      metadata.todo_reference = descriptor.todo_reference;
    }

    Register(std::move(handler), metadata);
  }
}

resources::CommandHandler* CommandRegistry::Get(const std::string& name) const {
  // Check direct name
  auto it = handlers_.find(name);
  if (it != handlers_.end()) {
    return it->second.get();
  }

  // Check aliases
  auto alias_it = aliases_.find(name);
  if (alias_it != aliases_.end()) {
    auto handler_it = handlers_.find(alias_it->second);
    if (handler_it != handlers_.end()) {
      return handler_it->second.get();
    }
  }

  return nullptr;
}

const CommandRegistry::CommandMetadata* CommandRegistry::GetMetadata(
    const std::string& name) const {
  // Resolve alias first
  std::string canonical_name = name;
  auto alias_it = aliases_.find(name);
  if (alias_it != aliases_.end()) {
    canonical_name = alias_it->second;
  }

  auto it = metadata_.find(canonical_name);
  return (it != metadata_.end()) ? &it->second : nullptr;
}

std::vector<std::string> CommandRegistry::GetCommandsInCategory(
    const std::string& category) const {
  std::vector<std::string> result;
  for (const auto& [name, metadata] : metadata_) {
    if (metadata.category == category) {
      result.push_back(name);
    }
  }
  return result;
}

std::vector<std::string> CommandRegistry::GetCategories() const {
  std::vector<std::string> categories;
  for (const auto& [_, metadata] : metadata_) {
    if (std::find(categories.begin(), categories.end(), metadata.category) ==
        categories.end()) {
      categories.push_back(metadata.category);
    }
  }
  return categories;
}

std::vector<std::string> CommandRegistry::GetAgentCommands() const {
  std::vector<std::string> result;
  for (const auto& [name, metadata] : metadata_) {
    if (metadata.available_to_agent) {
      result.push_back(name);
    }
  }
  return result;
}

std::string CommandRegistry::ExportFunctionSchemas() const {
  std::ostringstream out;
  out << "{\n  \"commands\": [\n";

  bool first = true;
  for (const auto& [_, metadata] : metadata_) {
    if (!first)
      out << ",\n";
    first = false;

    out << "    {\n";
    out << "      \"name\": \"" << EscapeJson(metadata.name) << "\",\n";
    out << "      \"category\": \"" << EscapeJson(metadata.category) << "\",\n";
    out << "      \"description\": \"" << EscapeJson(metadata.description)
        << "\",\n";
    out << "      \"usage\": \"" << EscapeJson(metadata.usage) << "\",\n";
    out << "      \"available_to_agent\": "
        << (metadata.available_to_agent ? "true" : "false") << ",\n";
    out << "      \"requires_rom\": "
        << (metadata.requires_rom ? "true" : "false") << ",\n";
    out << "      \"requires_grpc\": "
        << (metadata.requires_grpc ? "true" : "false") << ",\n";
    if (!metadata.todo_reference.empty()) {
      out << "      \"todo_reference\": \""
          << EscapeJson(metadata.todo_reference) << "\",\n";
    } else {
      out << "      \"todo_reference\": \"\",\n";
    }
    out << "      \"aliases\": ";
    AppendStringArray(out, metadata.aliases);
    out << ",\n";
    out << "      \"examples\": ";
    AppendStringArray(out, metadata.examples);
    out << "\n";
    out << "    }";
  }

  out << "\n  ]\n}";
  return out.str();
}

std::string CommandRegistry::GenerateHelp(const std::string& name) const {
  auto* metadata = GetMetadata(name);
  if (!metadata) {
    return absl::StrFormat("Command '%s' not found", name);
  }

  std::ostringstream help;
  help << "\n\033[1;36m" << metadata->name << "\033[0m - "
       << metadata->description << "\n\n";
  help << "\033[1;33mUsage:\033[0m\n";
  help << "  " << metadata->usage << "\n\n";

  if (!metadata->examples.empty()) {
    help << "\033[1;33mExamples:\033[0m\n";
    for (const auto& example : metadata->examples) {
      help << "  " << example << "\n";
    }
    help << "\n";
  }

  if (metadata->requires_rom) {
    help << "\033[1;33mRequires:\033[0m ROM file (--rom=<path>)\n";
  }
  if (metadata->requires_grpc) {
    help << "\033[1;33mRequires:\033[0m YAZE running with gRPC enabled\n";
  }

  if (!metadata->aliases.empty()) {
    help << "\n\033[1;33mAliases:\033[0m "
         << absl::StrJoin(metadata->aliases, ", ") << "\n";
  }

  return help.str();
}

std::string CommandRegistry::GenerateCategoryHelp(
    const std::string& category) const {
  auto commands = GetCommandsInCategory(category);
  if (commands.empty()) {
    return absl::StrFormat("No commands in category '%s'", category);
  }

  std::ostringstream help;
  help << "\n\033[1;36m" << category << " commands:\033[0m\n\n";

  for (const auto& cmd : commands) {
    auto* metadata = GetMetadata(cmd);
    if (metadata) {
      help << "  \033[1;33m" << cmd << "\033[0m\n";
      help << "    " << metadata->description << "\n";
      if (!metadata->usage.empty()) {
        help << "    Usage: " << metadata->usage << "\n";
      }
      help << "\n";
    }
  }

  return help.str();
}

std::string CommandRegistry::GenerateCompleteHelp() const {
  std::ostringstream help;
  help << "\n\033[1;36mAll z3ed Commands:\033[0m\n\n";

  auto categories = GetCategories();
  for (const auto& category : categories) {
    help << GenerateCategoryHelp(category);
  }

  return help.str();
}

absl::Status CommandRegistry::Execute(const std::string& name,
                                      const std::vector<std::string>& args,
                                      Rom* rom_context,
                                      std::string* captured_output) {
  auto* handler = Get(name);
  if (!handler) {
    return absl::NotFoundError(absl::StrFormat("Command '%s' not found", name));
  }

  const bool command_help_requested =
      std::find(args.begin(), args.end(), "--help") != args.end() ||
      std::find(args.begin(), args.end(), "-h") != args.end();
  if (command_help_requested) {
    const std::string help = GenerateHelp(name);
    if (captured_output != nullptr) {
      *captured_output = help;
    } else {
      std::cout << help << "\n";
    }
    return absl::OkStatus();
  }

  absl::Status status = handler->Run(args, rom_context, captured_output);

  // If a command was invoked without its required arguments, surface full
  // command help in addition to the normal parser error/usage line.
  if (!status.ok() && status.code() == absl::StatusCode::kInvalidArgument &&
      args.empty()) {
    const std::string help = GenerateHelp(name);
    if (captured_output != nullptr) {
      if (!captured_output->empty()) {
        captured_output->append("\n\n");
      }
      captured_output->append(help);
    } else {
      std::cout << help << "\n";
    }
  }

  return status;
}

bool CommandRegistry::HasCommand(const std::string& name) const {
  return Get(name) != nullptr;
}

void CommandRegistry::RegisterCliCommands() {
  RegisterHandlers(handlers::CreateCliCommandHandlers());
}

}  // namespace cli
}  // namespace yaze
