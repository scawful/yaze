#include "cli/service/command_registry.h"

#include "absl/strings/str_cat.h"
#include "absl/strings/str_format.h"
#include "absl/strings/str_join.h"
#include "cli/handlers/command_handlers.h"

namespace yaze {
namespace cli {

CommandRegistry& CommandRegistry::Instance() {
  static CommandRegistry instance;
  static bool initialized = false;
  if (!initialized) {
    instance.RegisterAllCommands();
    initialized = true;
  }
  return instance;
}

void CommandRegistry::Register(std::unique_ptr<resources::CommandHandler> handler,
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
    if (std::find(categories.begin(), categories.end(), metadata.category) == categories.end()) {
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
  // TODO: Generate JSON function schemas from metadata
  // This would replace manual function_schemas.json maintenance
  return "{}";  // Placeholder
}

std::string CommandRegistry::GenerateHelp(const std::string& name) const {
  auto* metadata = GetMetadata(name);
  if (!metadata) {
    return absl::StrFormat("Command '%s' not found", name);
  }
  
  std::ostringstream help;
  help << "\n\033[1;36m" << metadata->name << "\033[0m - " << metadata->description << "\n\n";
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
    help << "\n\033[1;33mAliases:\033[0m " << absl::StrJoin(metadata->aliases, ", ") << "\n";
  }
  
  return help.str();
}

std::string CommandRegistry::GenerateCategoryHelp(const std::string& category) const {
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
                                       Rom* rom_context) {
  auto* handler = Get(name);
  if (!handler) {
    return absl::NotFoundError(absl::StrFormat("Command '%s' not found", name));
  }
  
  return handler->Run(args, rom_context);
}

bool CommandRegistry::HasCommand(const std::string& name) const {
  return Get(name) != nullptr;
}

void CommandRegistry::RegisterAllCommands() {
  // Get all command handlers from factory
  auto all_handlers = handlers::CreateAllCommandHandlers();
  
  for (auto& handler : all_handlers) {
    std::string name = handler->GetName();
    
    // Build metadata from handler
    CommandMetadata metadata;
    metadata.name = name;
    metadata.usage = handler->GetUsage();
    metadata.available_to_agent = true;  // Most commands available to agent
    metadata.requires_rom = true;  // Most commands need ROM
    metadata.requires_grpc = false;
    
    // Categorize and enhance metadata based on command type
    if (name.find("resource-") == 0) {
      metadata.category = "resource";
      metadata.description = "Resource inspection and search";
      if (name == "resource-list") {
        metadata.examples = {
          "z3ed resource-list --type=dungeon --format=json",
          "z3ed resource-list --type=sprite --format=table"
        };
      }
    } else if (name.find("dungeon-") == 0) {
      metadata.category = "dungeon";
      metadata.description = "Dungeon inspection and editing";
      if (name == "dungeon-describe-room") {
        metadata.examples = {
          "z3ed dungeon-describe-room --room=5 --format=json"
        };
      }
    } else if (name.find("overworld-") == 0) {
      metadata.category = "overworld";
      metadata.description = "Overworld inspection and editing";
      if (name == "overworld-find-tile") {
        metadata.examples = {
          "z3ed overworld-find-tile --tile=0x42 --format=json"
        };
      }
    } else if (name.find("emulator-") == 0) {
      metadata.category = "emulator";
      metadata.description = "Emulator control and debugging";
      metadata.requires_grpc = true;
      if (name == "emulator-set-breakpoint") {
        metadata.examples = {
          "z3ed emulator-set-breakpoint --address=0x83D7 --description='NMI handler'"
        };
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
      metadata.description = name.find("message-") == 0 ? "Message inspection" : "Dialogue inspection";
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
        "z3ed simple-chat \"What dungeons exist?\" --rom=zelda3.sfc"
      };
    } else {
      metadata.category = "misc";
      metadata.description = "Miscellaneous command";
    }
    
    Register(std::move(handler), metadata);
  }
}

}  // namespace cli
}  // namespace yaze

