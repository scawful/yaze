#include "cli/modern_cli.h"

#include <iostream>

#include "absl/flags/flag.h"
#include "absl/flags/declare.h"
#include "absl/strings/str_cat.h"
#include "absl/strings/str_format.h"
#include "absl/strings/str_join.h"

#include "app/core/asar_wrapper.h"
#include "app/rom.h"
#include "cli/z3ed_ascii_logo.h"

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
      .description = "🤖 AI-Powered Conversational Agent for ROM Inspection\n"
                     "   Interact naturally with Zelda3 ROM data using embedded labels\n"
                     "   ✨ Features: Natural language queries, automatic label lookup, context-aware responses",
      .usage = "\n"
               "╔═══════════════════════════════════════════════════════════════════════════╗\n"
               "║                        🤖 AI AGENT COMMANDS                              ║\n"
               "╚═══════════════════════════════════════════════════════════════════════════╝\n"
               "\n"
               "💬 CONVERSATIONAL MODE (Recommended for testing):\n"
               "  z3ed agent test-conversation [--rom=<path>] [--verbose] [--file=<json>]\n"
               "    → Interactive AI testing with all embedded Zelda3 labels\n"
               "    → Ask about rooms, sprites, entrances, items naturally\n"
               "    → Example: 'What sprites are in room 5?' or 'List all dungeons'\n"
               "\n"
               "💡 SIMPLE CHAT MODE (Multiple input methods):\n"
               "  # Single message\n"
               "  z3ed agent simple-chat \"<your question>\" --rom=<path>\n"
               "  \n"
               "  # Interactive session\n"
               "  z3ed agent simple-chat --rom=<path>\n"
               "  \n"
               "  # Piped input\n"
               "  echo \"What is room 5?\" | z3ed agent simple-chat --rom=<path>\n"
               "  \n"
               "  # Batch file (one question per line)\n"
               "  z3ed agent simple-chat --file=questions.txt --rom=<path>\n"
               "\n"
               "🎯 ADVANCED CHAT MODE:\n"
               "  z3ed agent chat \"<prompt>\" [--host=<host>] [--port=<port>]\n"
               "    → Full conversational AI with Ollama/Gemini\n"
               "    → Supports multi-turn conversations\n"
               "\n"
               "📊 TEST MANAGEMENT:\n"
               "  z3ed agent test run --prompt \"<description>\" [--timeout=<sec>]\n"
               "  z3ed agent test status --test-id=<id> [--follow]\n"
               "  z3ed agent test list [--category=<name>] [--status=<state>]\n"
               "  z3ed agent test results --test-id=<id> [--format=yaml|json]\n"
               "  z3ed agent test suite <run|validate|create>\n"
               "\n"
               "🔍 OTHER COMMANDS:\n"
               "  z3ed agent gui discover [--window=<name>] [--type=<widget>]\n"
               "  z3ed agent describe [--resource=<name>] [--format=json|yaml]\n"
               "\n"
               "🏰 EMBEDDED LABELS (Always Available):\n"
               "  • 296+ room names       • 256 sprite names      • 133 entrance names\n"
               "  • 100 item names        • 160 overworld maps    • 48 music tracks\n"
               "  • 60 tile types         • 26 overlord names     • 32 graphics sheets\n"
               "\n"
               "📚 No separate labels file needed - all names built into z3ed!\n",
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

  commands_["overworld find-tile"] = {
    .name = "overworld find-tile",
    .description = "Search overworld maps for a tile ID",
    .usage = "z3ed overworld find-tile --tile <tile_id> [--map <map_id>] [--world light|dark|special] [--format json|text]",
    .handler = [this](const std::vector<std::string>& args) -> absl::Status {
      return HandleOverworldFindTileCommand(args);
    }
  };

    commands_["overworld describe-map"] = {
      .name = "overworld describe-map",
      .description = "Summarize metadata for an overworld map",
      .usage = "z3ed overworld describe-map --map <map_id> [--format json|text]",
      .handler = [this](const std::vector<std::string>& args) -> absl::Status {
        return HandleOverworldDescribeMapCommand(args);
      }
    };

    commands_["overworld list-warps"] = {
      .name = "overworld list-warps",
      .description = "List overworld entrances and holes with coordinates",
      .usage = "z3ed overworld list-warps [--map <map_id>] [--world light|dark|special] [--type entrance|hole|exit|all] [--format json|text]",
      .handler = [this](const std::vector<std::string>& args) -> absl::Status {
        return HandleOverworldListWarpsCommand(args);
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
    std::cout << GetColoredLogo() << std::endl;
    std::cout << std::endl;
    std::cout << "\033[1m\033[36mUSAGE:\033[0m" << std::endl;
    std::cout << "  z3ed [options] <command> [arguments]" << std::endl;
    std::cout << std::endl;
    
    std::cout << "\033[1m\033[36mGLOBAL OPTIONS:\033[0m" << std::endl;
    std::cout << "  --tui              Launch interactive Text User Interface" << std::endl;
    std::cout << "  --rom=<file>       Specify ROM file path" << std::endl;
    std::cout << "  --verbose, -v      Show detailed debug output" << std::endl;
    std::cout << "  --version          Show version information" << std::endl;
    std::cout << "  --help, -h         Show this help message" << std::endl;
    std::cout << std::endl;
    
    // Categorize commands
    std::cout << "\033[1m\033[36mCOMMANDS:\033[0m" << std::endl;
    std::cout << std::endl;
    
    std::cout << "  \033[1m🤖 AI Agent\033[0m" << std::endl;
    std::cout << "    agent simple-chat     Natural language ROM queries" << std::endl;
    std::cout << "    agent test-conversation  Interactive testing mode" << std::endl;
    std::cout << "    \033[90m→ z3ed help agent\033[0m" << std::endl;
    std::cout << std::endl;
    
    std::cout << "  \033[1m🔧 ROM Patching\033[0m" << std::endl;
    std::cout << "    patch apply-asar      Apply Asar 65816 assembly patch" << std::endl;
    std::cout << "    patch apply-bps       Apply BPS binary patch" << std::endl;
    std::cout << "    patch extract-symbols Extract symbols from assembly" << std::endl;
    std::cout << "    \033[90m→ z3ed help patch\033[0m" << std::endl;
    std::cout << std::endl;
    
    std::cout << "  \033[1m📦 ROM Operations\033[0m" << std::endl;
    std::cout << "    rom info              Display ROM information" << std::endl;
    std::cout << "    rom diff              Compare two ROM files" << std::endl;
    std::cout << "    rom generate-golden   Create golden test file" << std::endl;
    std::cout << "    \033[90m→ z3ed help rom\033[0m" << std::endl;
    std::cout << std::endl;
    
    std::cout << "  \033[1m🗺️  Overworld\033[0m" << std::endl;
    std::cout << "    overworld get-tile    Get tile at coordinates" << std::endl;
    std::cout << "    overworld set-tile    Place tile at coordinates" << std::endl;
    std::cout << "    overworld find-tile   Search for tile occurrences" << std::endl;
    std::cout << "    overworld describe-map  Show map metadata" << std::endl;
    std::cout << "    overworld list-warps  List entrances and exits" << std::endl;
    std::cout << "    \033[90m→ z3ed help overworld\033[0m" << std::endl;
    std::cout << std::endl;
    
    std::cout << "  \033[1m🏰 Dungeon\033[0m" << std::endl;
    std::cout << "    dungeon export        Export dungeon data" << std::endl;
    std::cout << "    dungeon import        Import dungeon data" << std::endl;
    std::cout << "    \033[90m→ z3ed help dungeon\033[0m" << std::endl;
    std::cout << std::endl;
    
    std::cout << "  \033[1m🎨 Graphics\033[0m" << std::endl;
    std::cout << "    gfx export-sheet      Export graphics sheet" << std::endl;
    std::cout << "    gfx import-sheet      Import graphics sheet" << std::endl;
    std::cout << "    palette export        Export palette data" << std::endl;
    std::cout << "    palette import        Import palette data" << std::endl;
    std::cout << "    \033[90m→ z3ed help gfx, z3ed help palette\033[0m" << std::endl;
    std::cout << std::endl;
    
    std::cout << "\033[1m\033[36mQUICK START:\033[0m" << std::endl;
    std::cout << "  z3ed --tui" << std::endl;
    std::cout << "  z3ed agent simple-chat \"What is room 5?\" --rom=zelda3.sfc" << std::endl;
    std::cout << "  z3ed patch apply-asar patch.asm --rom=zelda3.sfc" << std::endl;
    std::cout << std::endl;
    
    std::cout << "\033[90mFor detailed help: z3ed help <category>\033[0m" << std::endl;
}

void ModernCLI::PrintTopLevelHelp() const {
  const_cast<ModernCLI*>(this)->ShowHelp();
}

void ModernCLI::ShowCategoryHelp(const std::string& category) {
  std::cout << GetColoredLogo() << std::endl;
  std::cout << std::endl;
  
  if (category == "agent") {
    std::cout << "\033[1m\033[36m🤖 AI AGENT COMMANDS\033[0m" << std::endl;
    std::cout << std::endl;
    std::cout << "\033[1mDESCRIPTION:\033[0m" << std::endl;
    std::cout << "  Natural language interface for ROM inspection using embedded labels." << std::endl;
    std::cout << "  Query rooms, sprites, entrances, and game data conversationally." << std::endl;
    std::cout << std::endl;
    std::cout << "\033[1mCOMMANDS:\033[0m" << std::endl;
    std::cout << std::endl;
    std::cout << "  \033[1magent simple-chat\033[0m [\"<question>\"]" << std::endl;
    std::cout << "    Single-shot or interactive chat mode" << std::endl;
    std::cout << "    Options: --rom=<file>, --verbose" << std::endl;
    std::cout << "    Examples:" << std::endl;
    std::cout << "      z3ed agent simple-chat \"What sprites are in room 5?\" --rom=zelda3.sfc" << std::endl;
    std::cout << "      echo \"List all dungeons\" | z3ed agent simple-chat --rom=zelda3.sfc" << std::endl;
    std::cout << std::endl;
    std::cout << "  \033[1magent test-conversation\033[0m" << std::endl;
    std::cout << "    Interactive testing mode with full context" << std::endl;
    std::cout << "    Options: --rom=<file>, --verbose, --file=<json>" << std::endl;
    std::cout << std::endl;
    std::cout << "  \033[1magent chat\033[0m \"<prompt>\"" << std::endl;
    std::cout << "    Advanced multi-turn conversation mode" << std::endl;
    std::cout << "    Options: --host=<host>, --port=<port>" << std::endl;
    std::cout << std::endl;
    std::cout << "\033[1mTIPS:\033[0m" << std::endl;
    std::cout << "  • Use --verbose to see detailed API calls and responses" << std::endl;
    std::cout << "  • Set GEMINI_API_KEY environment variable for Gemini" << std::endl;
    std::cout << "  • Use --ai_provider=gemini or --ai_provider=ollama" << std::endl;
    std::cout << std::endl;
    
  } else if (category == "patch") {
    std::cout << "\033[1m\033[36m🔧 ROM PATCHING COMMANDS\033[0m" << std::endl;
    std::cout << std::endl;
    std::cout << "\033[1mDESCRIPTION:\033[0m" << std::endl;
    std::cout << "  Apply patches and extract symbols from assembly files." << std::endl;
    std::cout << std::endl;
    std::cout << "\033[1mCOMMANDS:\033[0m" << std::endl;
    std::cout << std::endl;
    std::cout << "  \033[1mpatch apply-asar\033[0m <patch.asm>" << std::endl;
    std::cout << "    Apply Asar 65816 assembly patch to ROM" << std::endl;
    std::cout << "    Options: --rom=<file>, --output=<file>" << std::endl;
    std::cout << "    Example: z3ed patch apply-asar custom.asm --rom=zelda3.sfc" << std::endl;
    std::cout << std::endl;
    std::cout << "  \033[1mpatch apply-bps\033[0m <patch.bps>" << std::endl;
    std::cout << "    Apply BPS binary patch to ROM" << std::endl;
    std::cout << "    Options: --rom=<file>, --output=<file>" << std::endl;
    std::cout << "    Example: z3ed patch apply-bps hack.bps --rom=zelda3.sfc" << std::endl;
    std::cout << std::endl;
    std::cout << "  \033[1mpatch extract-symbols\033[0m <patch.asm>" << std::endl;
    std::cout << "    Extract symbol table from assembly file" << std::endl;
    std::cout << "    Example: z3ed patch extract-symbols code.asm" << std::endl;
    std::cout << std::endl;
    std::cout << "\033[1mRELATED:\033[0m" << std::endl;
    std::cout << "  z3ed help rom         ROM operations and validation" << std::endl;
    std::cout << std::endl;
    
  } else if (category == "rom") {
    std::cout << "\033[1m\033[36m📦 ROM OPERATIONS\033[0m" << std::endl;
    std::cout << std::endl;
    std::cout << "\033[1mCOMMANDS:\033[0m" << std::endl;
    std::cout << std::endl;
    std::cout << "  \033[1mrom info\033[0m" << std::endl;
    std::cout << "    Display ROM header and metadata" << std::endl;
    std::cout << "    Example: z3ed rom info --rom=zelda3.sfc" << std::endl;
    std::cout << std::endl;
    std::cout << "  \033[1mrom diff\033[0m" << std::endl;
    std::cout << "    Compare two ROM files byte-by-byte" << std::endl;
    std::cout << "    Example: z3ed rom diff --src=original.sfc --modified=hacked.sfc" << std::endl;
    std::cout << std::endl;
    std::cout << "  \033[1mrom generate-golden\033[0m" << std::endl;
    std::cout << "    Create golden test reference file" << std::endl;
    std::cout << "    Example: z3ed rom generate-golden --rom=zelda3.sfc" << std::endl;
    std::cout << std::endl;
    std::cout << "  \033[1mrom validate\033[0m" << std::endl;
    std::cout << "    Validate ROM checksum and structure" << std::endl;
    std::cout << "    Example: z3ed rom validate --rom=zelda3.sfc" << std::endl;
    std::cout << std::endl;
    
  } else if (category == "overworld") {
    std::cout << "\033[1m\033[36m🗺️  OVERWORLD COMMANDS\033[0m" << std::endl;
    std::cout << std::endl;
    std::cout << "\033[1mDESCRIPTION:\033[0m" << std::endl;
    std::cout << "  Inspect and modify overworld map data, tiles, and warps." << std::endl;
    std::cout << std::endl;
    std::cout << "\033[1mCOMMANDS:\033[0m" << std::endl;
    std::cout << std::endl;
    std::cout << "  \033[1moverworld get-tile\033[0m" << std::endl;
    std::cout << "    Get tile ID at specific coordinates" << std::endl;
    std::cout << "    Example: z3ed overworld get-tile --x=10 --y=20 --map=0 --rom=zelda3.sfc" << std::endl;
    std::cout << std::endl;
    std::cout << "  \033[1moverworld set-tile\033[0m" << std::endl;
    std::cout << "    Place tile at coordinates" << std::endl;
    std::cout << "    Example: z3ed overworld set-tile --x=10 --y=20 --tile=0x42 --rom=zelda3.sfc" << std::endl;
    std::cout << std::endl;
    std::cout << "  \033[1moverworld find-tile\033[0m" << std::endl;
    std::cout << "    Search for all occurrences of a tile" << std::endl;
    std::cout << "    Example: z3ed overworld find-tile --tile=0x42 --rom=zelda3.sfc" << std::endl;
    std::cout << std::endl;
    std::cout << "  \033[1moverworld describe-map\033[0m" << std::endl;
    std::cout << "    Show map metadata and properties" << std::endl;
    std::cout << "    Example: z3ed overworld describe-map --map=0 --rom=zelda3.sfc" << std::endl;
    std::cout << std::endl;
    std::cout << "  \033[1moverworld list-warps\033[0m" << std::endl;
    std::cout << "    List all entrances and exits" << std::endl;
    std::cout << "    Example: z3ed overworld list-warps --rom=zelda3.sfc" << std::endl;
    std::cout << std::endl;
    
  } else if (category == "dungeon") {
    std::cout << "\033[1m\033[36m🏰 DUNGEON COMMANDS\033[0m" << std::endl;
    std::cout << std::endl;
    std::cout << "\033[1mCOMMANDS:\033[0m" << std::endl;
    std::cout << std::endl;
    std::cout << "  \033[1mdungeon export\033[0m" << std::endl;
    std::cout << "    Export dungeon room data to JSON" << std::endl;
    std::cout << "    Example: z3ed dungeon export --room=5 --rom=zelda3.sfc" << std::endl;
    std::cout << std::endl;
    std::cout << "  \033[1mdungeon import\033[0m" << std::endl;
    std::cout << "    Import dungeon data from JSON" << std::endl;
    std::cout << "    Example: z3ed dungeon import --file=room5.json --rom=zelda3.sfc" << std::endl;
    std::cout << std::endl;
    
  } else if (category == "gfx" || category == "graphics") {
    std::cout << "\033[1m\033[36m🎨 GRAPHICS COMMANDS\033[0m" << std::endl;
    std::cout << std::endl;
    std::cout << "\033[1mCOMMANDS:\033[0m" << std::endl;
    std::cout << std::endl;
    std::cout << "  \033[1mgfx export-sheet\033[0m" << std::endl;
    std::cout << "    Export graphics sheet to PNG" << std::endl;
    std::cout << "    Example: z3ed gfx export-sheet --sheet=0 --rom=zelda3.sfc" << std::endl;
    std::cout << std::endl;
    std::cout << "  \033[1mgfx import-sheet\033[0m" << std::endl;
    std::cout << "    Import graphics from PNG" << std::endl;
    std::cout << "    Example: z3ed gfx import-sheet --file=custom.png --rom=zelda3.sfc" << std::endl;
    std::cout << std::endl;
    std::cout << "\033[1mRELATED:\033[0m" << std::endl;
    std::cout << "  z3ed help palette     Palette manipulation commands" << std::endl;
    std::cout << std::endl;
    
  } else if (category == "palette") {
    std::cout << "\033[1m\033[36m🎨 PALETTE COMMANDS\033[0m" << std::endl;
    std::cout << std::endl;
    std::cout << "\033[1mCOMMANDS:\033[0m" << std::endl;
    std::cout << std::endl;
    std::cout << "  \033[1mpalette export\033[0m" << std::endl;
    std::cout << "    Export palette data" << std::endl;
    std::cout << "    Example: z3ed palette export --palette=0 --rom=zelda3.sfc" << std::endl;
    std::cout << std::endl;
    std::cout << "  \033[1mpalette import\033[0m" << std::endl;
    std::cout << "    Import palette data" << std::endl;
    std::cout << "    Example: z3ed palette import --file=colors.pal --rom=zelda3.sfc" << std::endl;
    std::cout << std::endl;
    
  } else {
    std::cout << "\033[1m\033[31mUnknown category: " << category << "\033[0m" << std::endl;
    std::cout << std::endl;
    std::cout << "Available categories: agent, patch, rom, overworld, dungeon, gfx, palette" << std::endl;
    std::cout << std::endl;
    std::cout << "Use 'z3ed --help' to see all commands." << std::endl;
  }
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

  // Handle "help <category>" command
  if (args.size() >= 1 && args[0] == "help") {
    if (args.size() == 1) {
      ShowHelp();
      return absl::OkStatus();
    }
    ShowCategoryHelp(args[1]);
    return absl::OkStatus();
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

absl::Status ModernCLI::HandleOverworldFindTileCommand(const std::vector<std::string>& args) {
  OverworldFindTile handler;
  return handler.Run(args);
}

absl::Status ModernCLI::HandleOverworldDescribeMapCommand(const std::vector<std::string>& args) {
  OverworldDescribeMap handler;
  return handler.Run(args);
}

absl::Status ModernCLI::HandleOverworldListWarpsCommand(const std::vector<std::string>& args) {
  OverworldListWarps handler;
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
