#include "cli/modern_cli.h"

#include <iostream>

#include "absl/flags/flag.h"
#include "absl/flags/declare.h"
#include "absl/strings/str_cat.h"
#include "absl/strings/str_format.h"
#include "absl/strings/str_join.h"

#include "app/core/asar_wrapper.h"
#include "app/rom.h"

ABSL_DECLARE_FLAG(std::string, rom);

namespace yaze {
namespace cli {

ModernCLI::ModernCLI() {
  SetupCommands();
}

void ModernCLI::SetupCommands() {
    commands_["patch apply-asar"] = {
      .name = "patch apply-asar",
      .description = "Apply Asar 65816 assembly patch to ROM",
      .usage = "z3ed patch apply-asar <patch.asm> [--rom=<rom_file>] [--output=<output_file>]",
      .handler = [this](const std::vector<std::string>& args) -> absl::Status {
        return HandleAsarPatchCommand(args);
      }
    };

    commands_["patch apply-bps"] = {
      .name = "patch apply-bps", 
      .description = "Apply BPS patch to ROM",
      .usage = "z3ed patch apply-bps <patch.bps> [--rom=<rom_file>] [--output=<output_file>]",
      .handler = [this](const std::vector<std::string>& args) -> absl::Status {
        return HandleBpsPatchCommand(args);
      }
    };

    commands_["patch extract-symbols"] = {
      .name = "patch extract-symbols",
      .description = "Extract symbols from assembly file",
      .usage = "z3ed patch extract-symbols <patch.asm>",
      .handler = [this](const std::vector<std::string>& args) -> absl::Status {
        return HandleExtractSymbolsCommand(args);
      }
    };

    commands_["rom info"] = {
      .name = "rom info",
      .description = "Show ROM information",
      .usage = "z3ed rom info [--rom=<rom_file>]",
      .handler = [this](const std::vector<std::string>& args) -> absl::Status {
        return HandleRomInfoCommand(args);
      }
    };

    commands_["agent"] = {
      .name = "agent",
      .description = "Interact with the AI agent",
      .usage = "z3ed agent <run|plan|diff|test|gui|list|learn|commit|revert|describe> [options]\n"
               "  test run:     --prompt \"<description>\" [--host <host>] [--port <port>] [--timeout <sec>]\n"
               "  test status:  status --test-id <id> [--follow] [--host <host>] [--port <port>]\n"
               "  test list:    list [--category <name>] [--status <state>] [--limit <n>] [--host <host>] [--port <port>]\n"
               "  test results: results --test-id <id> [--include-logs] [--format yaml|json] [--host <host>] [--port <port>]\n"
               "  test suite:   suite <run|validate|create> [options]\n"
               "  gui discover: discover [--window <name>] [--type <widget>] [--path-prefix <path>]\n"
               "                 [--include-invisible] [--include-disabled] [--format table|json] [--limit <n>]\n"
               "  describe options: [--resource <name>] [--format json|yaml] [--output <path>]\n"
               "                     [--version <value>] [--last-updated <date>]",
      .handler = [this](const std::vector<std::string>& args) -> absl::Status {
        return HandleAgentCommand(args);
      }
    };

    commands_["project build"] = {
      .name = "project build",
      .description = "Build the project and create a new ROM file",
      .usage = "z3ed project build",
      .handler = [this](const std::vector<std::string>& args) -> absl::Status {
        return HandleProjectBuildCommand(args);
      }
    };

    commands_["project init"] = {
      .name = "project init",
      .description = "Initialize a new z3ed project",
      .usage = "z3ed project init <project_name>",
      .handler = [this](const std::vector<std::string>& args) -> absl::Status {
        return HandleProjectInitCommand(args);
      }
    };

    commands_["rom generate-golden"] = {
      .name = "rom generate-golden",
      .description = "Generate a golden file from a ROM for regression testing",
      .usage = "z3ed rom generate-golden <rom_file> <golden_file>",
      .handler = [this](const std::vector<std::string>& args) -> absl::Status {
        return HandleRomGenerateGoldenCommand(args);
      }
    };

    commands_["rom diff"] = {
      .name = "rom diff",
      .description = "Compare two ROM files and show the differences",
      .usage = "z3ed rom diff <rom_a> <rom_b>",
      .handler = [this](const std::vector<std::string>& args) -> absl::Status {
        return HandleRomDiffCommand(args);
      }
    };

    commands_["rom validate"] = {
      .name = "rom validate",
      .description = "Validate the ROM file",
      .usage = "z3ed rom validate [--rom=<rom_file>]",
      .handler = [this](const std::vector<std::string>& args) -> absl::Status {
        return HandleRomValidateCommand(args);
      }
    };

    commands_["dungeon export"] = {
      .name = "dungeon export",
      .description = "Export dungeon data to a file",
      .usage = "z3ed dungeon export <room_id> --to <file>",
      .handler = [this](const std::vector<std::string>& args) -> absl::Status {
        return HandleDungeonExportCommand(args);
      }
    };

    commands_["dungeon list-objects"] = {
      .name = "dungeon list-objects",
      .description = "List all objects in a dungeon room",
      .usage = "z3ed dungeon list-objects <room_id>",
      .handler = [this](const std::vector<std::string>& args) -> absl::Status {
        return HandleDungeonListObjectsCommand(args);
      }
    };

    commands_["gfx export-sheet"] = {
      .name = "gfx export-sheet",
      .description = "Export a graphics sheet to a file",
      .usage = "z3ed gfx export-sheet <sheet_id> --to <file>",
      .handler = [this](const std::vector<std::string>& args) -> absl::Status {
        return HandleGfxExportCommand(args);
      }
    };

    commands_["gfx import-sheet"] = {
      .name = "gfx import-sheet",
      .description = "Import a graphics sheet from a file",
      .usage = "z3ed gfx import-sheet <sheet_id> --from <file>",
      .handler = [this](const std::vector<std::string>& args) -> absl::Status {
        return HandleGfxImportCommand(args);
      }
    };

    commands_["command-palette"] = {
      .name = "command-palette",
      .description = "Show the command palette",
      .usage = "z3ed command-palette",
      .handler = [this](const std::vector<std::string>& args) -> absl::Status {
        return HandleCommandPaletteCommand(args);
      }
    };

    commands_["palette"] = {
      .name = "palette",
      .description = "Manage palette data (export/import)",
      .usage = "z3ed palette <export|import> [options]",
      .handler = [this](const std::vector<std::string>& args) -> absl::Status {
        return HandlePaletteCommand(args);
      }
    };

    commands_["palette export"] = {
      .name = "palette export",
      .description = "Export a palette to a file",
      .usage = "z3ed palette export --group <group> --id <id> --to <file>",
      .handler = [this](const std::vector<std::string>& args) -> absl::Status {
        return HandlePaletteExportCommand(args);
      }
    };

    commands_["palette import"] = {
      .name = "palette import",
      .description = "Import a palette from a file",
      .usage = "z3ed palette import --group <group> --id <id> --from <file>",
      .handler = [this](const std::vector<std::string>& args) -> absl::Status {
        return HandlePaletteImportCommand(args);
      }
    };

    commands_["overworld get-tile"] = {
        .name = "overworld get-tile",
        .description = "Get a tile from the overworld",
        .usage = "z3ed overworld get-tile --map <map_id> --x <x> --y <y>",
        .handler = [this](const std::vector<std::string>& args) -> absl::Status {
            return HandleOverworldGetTileCommand(args);
        }
    };

    commands_["overworld set-tile"] = {
        .name = "overworld set-tile",
        .description = "Set a tile in the overworld",
        .usage = "z3ed overworld set-tile --map <map_id> --x <x> --y <y> --tile <tile_id>",
        .handler = [this](const std::vector<std::string>& args) -> absl::Status {
            return HandleOverworldSetTileCommand(args);
        }
    };

    commands_["sprite create"] = {
        .name = "sprite create",
        .description = "Create a new sprite",
        .usage = "z3ed sprite create --name <name> [options]",
        .handler = [this](const std::vector<std::string>& args) -> absl::Status {
            return HandleSpriteCreateCommand(args);
        }
    };
}

void ModernCLI::ShowHelp() {
    std::cout << "z3ed - Yet Another Zelda3 Editor CLI Tool" << std::endl;
    std::cout << std::endl;
    std::cout << "USAGE:" << std::endl;
    std::cout << "  z3ed [--tui] <resource> <action> [arguments]" << std::endl;
    std::cout << std::endl;
    std::cout << "GLOBAL FLAGS:" << std::endl;
    std::cout << "  --tui              Launch Text User Interface" << std::endl;
    std::cout << "  --rom=<file>       Specify ROM file to use" << std::endl;
    std::cout << "  --version          Show version information" << std::endl;
    std::cout << "  --help             Show this help message" << std::endl;
    std::cout << std::endl;
    std::cout << "COMMANDS:" << std::endl;
    
    for (const auto& [name, info] : commands_) {
      std::cout << absl::StrFormat("  %-25s %s", name, info.description) << std::endl;
    }
    
    std::cout << std::endl;
    std::cout << "EXAMPLES:" << std::endl;
    std::cout << "  z3ed --tui                                    # Launch TUI" << std::endl;
    std::cout << "  z3ed patch apply-asar patch.asm --rom=zelda3.sfc # Apply Asar patch" << std::endl;
    std::cout << "  z3ed patch apply-bps changes.bps --rom=zelda3.sfc # Apply BPS patch" << std::endl;
    std::cout << "  z3ed patch extract-symbols patch.asm          # Extract symbols" << std::endl;
    std::cout << "  z3ed rom info --rom=zelda3.sfc                   # Show ROM info" << std::endl;
    std::cout << std::endl;
    std::cout << "For more information on a specific command:" << std::endl;
    std::cout << "  z3ed help <resource> <action>" << std::endl;
}

void ModernCLI::PrintTopLevelHelp() const {
  const_cast<ModernCLI*>(this)->ShowHelp();
}

absl::Status ModernCLI::Run(int argc, char* argv[]) {
  if (argc < 2) {
    ShowHelp();
    return absl::OkStatus();
  }

  std::vector<std::string> args;
  args.reserve(argc - 1);
  for (int i = 1; i < argc; ++i) {
    args.emplace_back(argv[i]);
  }

  const CommandInfo* command_info = nullptr;
  size_t consumed_tokens = 0;

  if (args.size() >= 2) {
    std::string candidate = absl::StrCat(args[0], " ", args[1]);
    auto it = commands_.find(candidate);
    if (it != commands_.end()) {
      command_info = &it->second;
      consumed_tokens = 2;
    }
  }

  if (command_info == nullptr && !args.empty()) {
    auto it = commands_.find(args[0]);
    if (it != commands_.end()) {
      command_info = &it->second;
      consumed_tokens = 1;
    }
  }

  if (command_info == nullptr) {
    ShowHelp();
    std::string joined = args.empty() ? std::string() : absl::StrJoin(args, " ");
    return absl::NotFoundError(
        absl::StrCat("Unknown command: ", joined.empty() ? "<none>" : joined));
  }

  std::vector<std::string> command_args(args.begin() + consumed_tokens, args.end());
  return command_info->handler(command_args);
}

CommandHandler* ModernCLI::GetCommandHandler(const std::string& name) {
    // This is not ideal, but it will work for now.
    if (name == "patch apply-asar") {
        static AsarPatch handler;
        return &handler;
    }
    if (name == "palette") {
        static Palette handler;
        return &handler;
    }
    if (name == "command-palette") {
        static CommandPalette handler;
        return &handler;
    }
    return nullptr;
}

absl::Status ModernCLI::HandleAsarPatchCommand(const std::vector<std::string>& args) {
    AsarPatch handler;
    return handler.Run(args);
}

absl::Status ModernCLI::HandleBpsPatchCommand(const std::vector<std::string>& args) {
    ApplyPatch handler;
    return handler.Run(args);
}

absl::Status ModernCLI::HandleRomInfoCommand(const std::vector<std::string>& args) {
    RomInfo handler;
    return handler.Run(args);
}

absl::Status ModernCLI::HandleExtractSymbolsCommand(const std::vector<std::string>& args) {
    // Use the AsarWrapper to extract symbols
    yaze::core::AsarWrapper wrapper;
    RETURN_IF_ERROR(wrapper.Initialize());
    
    auto symbols_result = wrapper.ExtractSymbols(args[0]);
    if (!symbols_result.ok()) {
      return symbols_result.status();
    }

    const auto& symbols = symbols_result.value();
    std::cout << "🏷️  Extracted " << symbols.size() << " symbols from " << args[0] << ":" << std::endl;
    std::cout << std::endl;
    
    for (const auto& symbol : symbols) {
      std::cout << absl::StrFormat("  %-20s @ $%06X", symbol.name, symbol.address) << std::endl;
    }
    
    return absl::OkStatus();
}

absl::Status ModernCLI::HandleAgentCommand(const std::vector<std::string>& args) {
    Agent handler;
    return handler.Run(args);
}

absl::Status ModernCLI::HandleProjectBuildCommand(const std::vector<std::string>& args) {
    ProjectBuild handler;
    return handler.Run(args);
}

absl::Status ModernCLI::HandleProjectInitCommand(const std::vector<std::string>& args) {
    ProjectInit handler;
    return handler.Run(args);
}

absl::Status ModernCLI::HandleRomGenerateGoldenCommand(const std::vector<std::string>& args) {
    std::string rom_file = absl::GetFlag(FLAGS_rom);
    if (!args.empty()) {
      rom_file = args[0];
    }
    
    if (rom_file.empty()) {
      return absl::InvalidArgumentError("ROM file required (use --rom=<file> or provide as argument)");
    }

    Open handler;
    return handler.Run({rom_file});
}

absl::Status ModernCLI::HandleDungeonExportCommand(const std::vector<std::string>& args) {
    DungeonExport handler;
    return handler.Run(args);
}

absl::Status ModernCLI::HandleDungeonListObjectsCommand(const std::vector<std::string>& args) {
    DungeonListObjects handler;
    return handler.Run(args);
}

absl::Status ModernCLI::HandleGfxExportCommand(const std::vector<std::string>& args) {
    GfxExport handler;
    return handler.Run(args);
}

absl::Status ModernCLI::HandleGfxImportCommand(const std::vector<std::string>& args) {
    GfxImport handler;
    return handler.Run(args);
}

absl::Status ModernCLI::HandleCommandPaletteCommand(const std::vector<std::string>& args) {
    CommandPalette handler;
    return handler.Run(args);
}

absl::Status ModernCLI::HandlePaletteExportCommand(const std::vector<std::string>& args) {
    PaletteExport handler;
    return handler.Run(args);
}

absl::Status ModernCLI::HandlePaletteCommand(const std::vector<std::string>& args) {
    Palette handler;
    return handler.Run(args);
}

absl::Status ModernCLI::HandlePaletteImportCommand(const std::vector<std::string>& args) {
    PaletteImport handler;
    return handler.Run(args);
}

absl::Status ModernCLI::HandleRomDiffCommand(const std::vector<std::string>& args) {
    RomDiff handler;
    return handler.Run(args);
}

absl::Status ModernCLI::HandleRomValidateCommand(const std::vector<std::string>& args) {
    RomValidate handler;
    return handler.Run(args);
}

absl::Status ModernCLI::HandleOverworldGetTileCommand(const std::vector<std::string>& args) {
    OverworldGetTile handler;
    return handler.Run(args);
}

absl::Status ModernCLI::HandleOverworldSetTileCommand(const std::vector<std::string>& args) {
    OverworldSetTile handler;
    return handler.Run(args);
}

absl::Status ModernCLI::HandleSpriteCreateCommand(const std::vector<std::string>& args) {
    SpriteCreate handler;
    return handler.Run(args);
}

}  // namespace cli
}  // namespace yaze
