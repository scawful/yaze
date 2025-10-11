#include "cli/cli.h"

#include <iostream>
#include <optional>

#include "absl/flags/flag.h"
#include "absl/flags/declare.h"
#include "absl/strings/ascii.h"
#include "absl/strings/match.h"
#include "absl/strings/str_cat.h"
#include "absl/strings/str_format.h"
#include "absl/strings/str_join.h"

#include "app/core/asar_wrapper.h"
#include "app/rom.h"
#include "cli/z3ed_ascii_logo.h"
#include "cli/tui/tui.h"
#include "app/snes.h"
#include "util/macro.h"
#include "cli/handlers/commands.h"
#include "cli/service/resources/command_context.h"

// Define additional z3ed-specific flags
ABSL_DECLARE_FLAG(std::string, rom);
ABSL_DECLARE_FLAG(bool, quiet);
ABSL_DECLARE_FLAG(std::string, ai_provider);
ABSL_DECLARE_FLAG(std::string, ai_model);
ABSL_DECLARE_FLAG(std::string, gemini_api_key);
ABSL_DECLARE_FLAG(std::string, ollama_host);
ABSL_DECLARE_FLAG(std::string, prompt_version);
ABSL_DECLARE_FLAG(bool, use_function_calling);

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

    commands_["chat"] = {
      .name = "chat",
      .description =
          "Unified chat entrypoint with text, markdown, or JSON output",
      .usage =
          "z3ed chat [--mode=simple|gui|test] [--format=text|markdown|json|compact]"
          " [--prompt \"<message>\"] [--file=<questions.txt>] [--quiet]",
      .handler = [this](const std::vector<std::string>& args) -> absl::Status {
        return HandleChatEntryCommand(args);
      }
    };

    commands_["proposal"] = {
      .name = "proposal",
      .description = "Review and manage AI-generated change proposals",
      .usage =
          "z3ed proposal <run|list|diff|show|accept|commit|revert> [options]",
      .handler = [this](const std::vector<std::string>& args) -> absl::Status {
        return HandleProposalCommand(args);
      }
    };

    commands_["collab"] = {
      .name = "collab",
      .description = "🌐 Collaboration Server Management\n"
                     "   Launch and manage the WebSocket collaboration server for networked sessions",
      .usage = "\n"
               "╔═══════════════════════════════════════════════════════════════════════════╗\n"
               "║                 🌐 COLLABORATION SERVER COMMANDS                          ║\n"
               "╚═══════════════════════════════════════════════════════════════════════════╝\n"
               "\n"
               "🚀 SERVER MANAGEMENT:\n"
               "  z3ed collab start [--port=<port>]\n"
               "    → Start the WebSocket collaboration server\n"
               "    → Default port: 8765\n"
               "    → Server will be accessible at ws://localhost:<port>\n"
               "\n"
               "📊 SERVER STATUS:\n"
               "  z3ed collab status\n"
               "    → Check if collaboration server is running\n"
               "    → Show active sessions and participants\n"
               "\n"
               "💡 USAGE:\n"
               "  1. Start server: z3ed collab start\n"
               "  2. In YAZE GUI: Debug → Agent Chat\n"
               "  3. Select 'Network' mode and connect to ws://localhost:8765\n"
               "  4. Host or join a session to collaborate!\n",
      .handler = [this](const std::vector<std::string>& args) -> absl::Status {
        return HandleCollabCommand(args);
      }
    };

    commands_["widget"] = {
      .name = "widget",
      .description = "Discover GUI widgets exposed through automation APIs",
      .usage = "z3ed widget discover [--window=<name>] [--type=<widget>]",
      .handler = [this](const std::vector<std::string>& args) -> absl::Status {
        return HandleWidgetCommand(args);
      }
    };

    commands_["widget discover"] = {
      .name = "widget discover",
      .description = "Inspect UI widgets using the automation service",
      .usage =
          "z3ed widget discover [--window=<name>] [--type=<widget>]"
          " [--format=text|json]",
      .handler = [this](const std::vector<std::string>& args) -> absl::Status {
        return HandleWidgetCommand(args);
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

    commands_["overworld select-rect"] = {
        .name = "overworld select-rect",
        .description = "Select a rectangular region of tiles",
        .usage = "z3ed overworld select-rect --map <map_id> --x1 <x1> --y1 <y1> --x2 <x2> --y2 <y2>",
        .handler = [this](const std::vector<std::string>& args) -> absl::Status {
            return HandleOverworldSelectRectCommand(args);
        }
    };

    commands_["overworld scroll-to"] = {
        .name = "overworld scroll-to",
        .description = "Scroll canvas to show specific tile",
        .usage = "z3ed overworld scroll-to --map <map_id> --x <x> --y <y> [--center]",
        .handler = [this](const std::vector<std::string>& args) -> absl::Status {
            return HandleOverworldScrollToCommand(args);
        }
    };

    commands_["overworld set-zoom"] = {
        .name = "overworld set-zoom",
        .description = "Set canvas zoom level",
        .usage = "z3ed overworld set-zoom --zoom <level> (0.25-4.0)",
        .handler = [this](const std::vector<std::string>& args) -> absl::Status {
            return HandleOverworldSetZoomCommand(args);
        }
    };

    commands_["overworld get-visible-region"] = {
        .name = "overworld get-visible-region",
        .description = "Get currently visible tile region",
        .usage = "z3ed overworld get-visible-region --map <map_id> [--format json|text]",
        .handler = [this](const std::vector<std::string>& args) -> absl::Status {
            return HandleOverworldGetVisibleRegionCommand(args);
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
  std::cout << "    chat                  Unified chat entrypoint (text/json/markdown)" << std::endl;
  std::cout << "    agent simple-chat     Natural language ROM queries" << std::endl;
  std::cout << "    agent test-conversation  Interactive testing mode" << std::endl;
  std::cout << "    \033[90m→ z3ed help chat, z3ed help agent\033[0m" << std::endl;
    std::cout << std::endl;

  std::cout << "  \033[1m🧠 Proposals\033[0m" << std::endl;
  std::cout << "    proposal run          Execute AI-driven sandbox plan" << std::endl;
  std::cout << "    proposal list         Show pending proposals" << std::endl;
  std::cout << "    proposal diff         Review latest sandbox diff" << std::endl;
  std::cout << "    proposal accept       Apply sandbox changes" << std::endl;
  std::cout << "    \033[90m→ z3ed help proposal\033[0m" << std::endl;
  std::cout << std::endl;

  std::cout << "  \033[1m🪟 GUI Automation\033[0m" << std::endl;
  std::cout << "    widget discover       Inspect GUI widgets via automation" << std::endl;
  std::cout << "    \033[90m→ z3ed help widget\033[0m" << std::endl;
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
  std::cout << "  z3ed chat \"What is room 5?\" --rom=zelda3.sfc --format=markdown" << std::endl;
    std::cout << "  z3ed patch apply-asar patch.asm --rom=zelda3.sfc" << std::endl;
    std::cout << std::endl;
    
    std::cout << "\033[90mFor detailed help: z3ed help <category>\033[0m" << std::endl;
}

void ModernCLI::PrintTopLevelHelp() const {
  const_cast<ModernCLI*>(this)->ShowHelp();
}

void ModernCLI::PrintCategoryHelp(const std::string& category) const {
  const_cast<ModernCLI*>(this)->ShowCategoryHelp(category);
}

void ModernCLI::PrintCommandSummary() const {
  const_cast<ModernCLI*>(this)->ShowCommandSummary();
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
    
  } else if (category == "chat") {
    std::cout << "\033[1m\033[36m💬 CHAT ENTRYPOINT\033[0m" << std::endl;
    std::cout << std::endl;
    std::cout << "\033[1mDESCRIPTION:\033[0m" << std::endl;
    std::cout << "  Launch the embedded agent in text, markdown, or JSON-friendly modes." << std::endl;
    std::cout << std::endl;
    std::cout << "\033[1mMODES:\033[0m" << std::endl;
    std::cout << "  chat --mode=simple        Quick REPL with --format=text|markdown|json|compact" << std::endl;
    std::cout << "  chat --mode=batch --file=F   Run prompts from file (one per line)" << std::endl;
    std::cout << "  chat --mode=gui            Launch the full FTXUI conversation experience" << std::endl;
    std::cout << "  chat --mode=test           Execute scripted agent conversation for QA" << std::endl;
    std::cout << std::endl;
    std::cout << "\033[1mOPTIONS:\033[0m" << std::endl;
    std::cout << "  --format=text|markdown|json|compact    Control response formatting" << std::endl;
    std::cout << "  --prompt \"<msg>\"                  Send a single message and exit" << std::endl;
    std::cout << "  --file questions.txt                  Batch mode input" << std::endl;
    std::cout << "  --quiet                                Suppress extra banners" << std::endl;
    std::cout << std::endl;

  } else if (category == "proposal") {
    std::cout << "\033[1m\033[36m🧠 PROPOSAL WORKFLOWS\033[0m" << std::endl;
    std::cout << std::endl;
    std::cout << "\033[1mCOMMANDS:\033[0m" << std::endl;
    std::cout << "  proposal run --prompt \"<desc>\"        Plan and execute changes in sandbox" << std::endl;
    std::cout << "  proposal list                          Show pending proposals" << std::endl;
    std::cout << "  proposal diff [--proposal-id=X]        Inspect latest diff/log" << std::endl;
    std::cout << "  proposal accept --proposal-id=X        Apply sandbox changes to main ROM" << std::endl;
    std::cout << "  proposal commit | proposal revert      Persist or undo sandbox changes" << std::endl;
    std::cout << std::endl;
    std::cout << "\033[1mTIPS:\033[0m" << std::endl;
    std::cout << "  • Run `z3ed proposal list` frequently to monitor progress" << std::endl;
    std::cout << "  • Use `--prompt` to describe tasks in natural language" << std::endl;
    std::cout << "  • Sandbox artifacts live alongside proposal logs" << std::endl;
    std::cout << std::endl;

  } else if (category == "widget") {
    std::cout << "\033[1m\033[36m🪟 GUI WIDGET DISCOVERY\033[0m" << std::endl;
    std::cout << std::endl;
    std::cout << "\033[1mCOMMANDS:\033[0m" << std::endl;
  std::cout << "  widget discover [--window=<name>] [--type=<widget>]" << std::endl;
  std::cout << "    Enumerate UI widgets available through automation hooks" << std::endl;
  std::cout << "    Options: --format=table|json, --limit <n>, --include-invisible, --include-disabled" << std::endl;
    std::cout << std::endl;
    std::cout << "\033[1mTIPS:\033[0m" << std::endl;
    std::cout << "  • Requires the YAZE GUI to be running locally" << std::endl;
    std::cout << "  • Combine with `z3ed proposal run` for automated UI tests" << std::endl;
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
    std::cout << "  \033[1moverworld select-rect\033[0m" << std::endl;
    std::cout << "    Select a rectangular region of tiles" << std::endl;
    std::cout << "    Example: z3ed overworld select-rect --map=0 --x1=5 --y1=5 --x2=10 --y2=10" << std::endl;
    std::cout << std::endl;
    std::cout << "  \033[1moverworld scroll-to\033[0m" << std::endl;
    std::cout << "    Scroll canvas to show specific tile" << std::endl;
    std::cout << "    Example: z3ed overworld scroll-to --map=0 --x=10 --y=10 --center" << std::endl;
    std::cout << std::endl;
    std::cout << "  \033[1moverworld set-zoom\033[0m" << std::endl;
    std::cout << "    Set canvas zoom level (0.25-4.0)" << std::endl;
    std::cout << "    Example: z3ed overworld set-zoom --zoom=1.5" << std::endl;
    std::cout << std::endl;
    std::cout << "  \033[1moverworld get-visible-region\033[0m" << std::endl;
    std::cout << "    Get currently visible tile region" << std::endl;
    std::cout << "    Example: z3ed overworld get-visible-region --map=0 --format=json" << std::endl;
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
  std::cout << "Available categories: agent, chat, proposal, widget, patch, rom, overworld, dungeon, gfx, palette" << std::endl;
    std::cout << std::endl;
    std::cout << "Use 'z3ed --help' to see all commands." << std::endl;
  }
}

void ModernCLI::ShowCommandSummary() const {
  std::cout << GetColoredLogo() << std::endl;
  std::cout << std::endl;
  std::cout << "\033[1m\033[36mCOMMANDS OVERVIEW\033[0m" << std::endl;
  std::cout << std::endl;

  for (const auto& [key, info] : commands_) {
    std::string headline = info.description;
    const size_t newline_pos = headline.find('\n');
    if (newline_pos != std::string::npos) {
      headline = headline.substr(0, newline_pos);
    }
    if (headline.empty()) {
      headline = info.usage;
    }
    if (headline.size() > 80) {
      headline = absl::StrCat(headline.substr(0, 77), "…");
    }

    std::string label = info.name;
    if (label.size() > 24) {
      label = absl::StrCat(label.substr(0, 23), "…");
    }

  std::cout << "  "
        << absl::StrFormat("%-24s%s", label, headline) << std::endl;
  }

  std::cout << std::endl;
  std::cout << "Use \033[90mz3ed help <topic>\033[0m for detailed information." << std::endl;
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
    // TODO: Implement using new CommandHandler system
    // This method should return CommandHandler instances from the new system
    // Reference: cli/handlers/command_handlers.h (factory functions)
    return nullptr;
}

absl::Status ModernCLI::HandleAsarPatchCommand(const std::vector<std::string>& args) {
    // TODO: Implement using AsarPatchCommandHandler
    // Reference: src/app/core/asar_wrapper.cc (AsarWrapper class)
    return absl::UnimplementedError("AsarPatchCommandHandler not yet implemented");
}

absl::Status ModernCLI::HandleBpsPatchCommand(const std::vector<std::string>& args) {
    // TODO: Implement using BpsPatchCommandHandler  
    // Reference: src/app/core/asar_wrapper.cc (for patch application logic)
    return absl::UnimplementedError("BpsPatchCommandHandler not yet implemented");
}

absl::Status ModernCLI::HandleRomInfoCommand(const std::vector<std::string>& args) {
    // TODO: Implement using RomInfoCommandHandler
    // Reference: src/app/rom.cc (Rom::GetInfo methods)
    return absl::UnimplementedError("RomInfoCommandHandler not yet implemented");
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
    // Use new CommandHandler system
    return yaze::cli::handlers::HandleAgentCommand(args);
}

absl::Status ModernCLI::HandleCollabCommand(const std::vector<std::string>& args) {
  if (args.empty()) {
    return absl::InvalidArgumentError(
        "Usage: z3ed collab <start|status> [options]\n"
        "  start  - Start the collaboration server\n"
        "  status - Check server status");
  }

  const std::string& subcommand = args[0];

  if (subcommand == "start") {
    std::string port = "8765";
    
    // Parse port argument
    for (size_t i = 1; i < args.size(); ++i) {
      if (absl::StartsWith(args[i], "--port=")) {
        port = args[i].substr(7);
      } else if (args[i] == "--port" && i + 1 < args.size()) {
        port = args[++i];
      }
    }

    // Determine server directory
    std::string server_dir;
    if (const char* yaze_root = std::getenv("YAZE_ROOT")) {
      server_dir = std::string(yaze_root);
    } else {
      // Assume we're in build directory, server is ../yaze-server
      server_dir = "..";
    }
    
    std::cout << "🚀 Starting collaboration server on port " << port << "...\n";
    std::cout << "   Server will be accessible at ws://localhost:" << port << "\n\n";
    
    // Build platform-specific command
    std::string command;
#ifdef _WIN32
    // Windows: Use cmd.exe to run npm start
    command = "cd /D \"" + server_dir + "\\..\\yaze-server\" && set PORT=" + 
              port + " && npm start";
#else
    // Unix: Use bash script
    command = "cd \"" + server_dir + "/../yaze-server\" && PORT=" + 
              port + " node server.js &";
#endif
    
    int result = std::system(command.c_str());
    
    if (result != 0) {
      std::cout << "⚠️  Note: Server may not be installed. To install:\n";
      std::cout << "   cd yaze-server && npm install\n";
      return absl::InternalError("Failed to start collaboration server");
    }
    
    std::cout << "✓ Server started (may take a few seconds to initialize)\n";
    return absl::OkStatus();
  }
  
  if (subcommand == "status") {
    // Check if Node.js process is running (platform-specific)
    int result;
#ifdef _WIN32
    // Windows: Use tasklist to find node.exe
    result = std::system("tasklist /FI \"IMAGENAME eq node.exe\" 2>NUL | find /I \"node.exe\" >NUL");
#else
    // Unix: Use pgrep
    result = std::system("pgrep -f 'node.*server.js' > /dev/null 2>&1");
#endif
    
    if (result == 0) {
      std::cout << "✓ Collaboration server appears to be running\n";
      std::cout << "  Connect from YAZE: Debug → Agent Chat → Network mode\n";
    } else {
      std::cout << "○ Collaboration server is not running\n";
      std::cout << "  Start with: z3ed collab start\n";
    }
    
    return absl::OkStatus();
  }

  return absl::InvalidArgumentError(absl::StrFormat("Unknown subcommand: %s", subcommand));
}

absl::Status ModernCLI::HandleProjectBuildCommand(const std::vector<std::string>& args) {
    // TODO: Implement using ProjectBuildCommandHandler
    // Reference: src/app/core/project.cc (Project::Build method)
    return absl::UnimplementedError("ProjectBuildCommandHandler not yet implemented");
}

absl::Status ModernCLI::HandleProjectInitCommand(const std::vector<std::string>& args) {
    // TODO: Implement using ProjectInitCommandHandler
    // Reference: src/app/core/project.cc (Project class)
    return absl::UnimplementedError("ProjectInitCommandHandler not yet implemented");
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
    // Use new CommandHandler system
    return yaze::cli::handlers::HandleDungeonExportRoomCommand(args, nullptr);
}

absl::Status ModernCLI::HandleDungeonListObjectsCommand(const std::vector<std::string>& args) {
    // Use new CommandHandler system
    return yaze::cli::handlers::HandleDungeonListObjectsCommand(args, nullptr);
}

absl::Status ModernCLI::HandleGfxExportCommand(const std::vector<std::string>& args) {
    // TODO: Implement using GfxExportCommandHandler
    // Reference: src/app/editor/graphics/ (graphics editor classes)
    return absl::UnimplementedError("GfxExportCommandHandler not yet implemented");
}

absl::Status ModernCLI::HandleGfxImportCommand(const std::vector<std::string>& args) {
    // TODO: Implement using GfxImportCommandHandler
    // Reference: src/app/editor/graphics/ (graphics editor classes)
    return absl::UnimplementedError("GfxImportCommandHandler not yet implemented");
}

absl::Status ModernCLI::HandleCommandPaletteCommand(const std::vector<std::string>& args) {
    // TODO: Implement using CommandPaletteCommandHandler
    // Reference: src/app/editor/system/command_palette.cc
    return absl::UnimplementedError("CommandPaletteCommandHandler not yet implemented");
}

absl::Status ModernCLI::HandlePaletteExportCommand(const std::vector<std::string>& args) {
    // TODO: Implement using PaletteExportCommandHandler
    // Reference: src/app/editor/graphics/palette_editor.cc
    return absl::UnimplementedError("PaletteExportCommandHandler not yet implemented");
}

absl::Status ModernCLI::HandlePaletteCommand(const std::vector<std::string>& args) {
    // Use new CommandHandler system for palette operations
    if (args.empty()) {
        return yaze::cli::handlers::HandlePaletteGetColors(args, nullptr);
    }
    
    // Parse subcommand
    std::string subcommand = absl::AsciiStrToLower(args[0]);
    if (subcommand == "get" || subcommand == "list") {
        return yaze::cli::handlers::HandlePaletteGetColors(args, nullptr);
    } else if (subcommand == "set") {
        return yaze::cli::handlers::HandlePaletteSetColor(args, nullptr);
    } else if (subcommand == "analyze") {
        return yaze::cli::handlers::HandlePaletteAnalyze(args, nullptr);
    }
    
    return absl::InvalidArgumentError("Unknown palette subcommand. Use: get, set, analyze");
}

absl::Status ModernCLI::HandlePaletteImportCommand(const std::vector<std::string>& args) {
    // TODO: Implement using PaletteImportCommandHandler
    // Reference: src/app/editor/graphics/palette_editor.cc
    return absl::UnimplementedError("PaletteImportCommandHandler not yet implemented");
}

absl::Status ModernCLI::HandleRomDiffCommand(const std::vector<std::string>& args) {
    // TODO: Implement using RomDiffCommandHandler
    // Reference: src/app/rom.cc (Rom comparison methods)
    return absl::UnimplementedError("RomDiffCommandHandler not yet implemented");
}

absl::Status ModernCLI::HandleRomValidateCommand(const std::vector<std::string>& args) {
    // TODO: Implement using RomValidateCommandHandler
    // Reference: src/app/rom.cc (Rom class validation methods)
    return absl::UnimplementedError("RomValidateCommandHandler not yet implemented");
}

absl::Status ModernCLI::HandleOverworldGetTileCommand(const std::vector<std::string>& args) {
    // TODO: Implement using OverworldGetTileCommandHandler
    // Reference: src/app/editor/overworld/ (overworld editor classes)
    return absl::UnimplementedError("OverworldGetTileCommandHandler not yet implemented");
}

absl::Status ModernCLI::HandleOverworldFindTileCommand(const std::vector<std::string>& args) {
    // Use new CommandHandler system
    return yaze::cli::handlers::HandleOverworldFindTileCommand(args, nullptr);
}

absl::Status ModernCLI::HandleOverworldDescribeMapCommand(const std::vector<std::string>& args) {
    // Use new CommandHandler system
    return yaze::cli::handlers::HandleOverworldDescribeMapCommand(args, nullptr);
}

absl::Status ModernCLI::HandleOverworldListWarpsCommand(const std::vector<std::string>& args) {
    // Use new CommandHandler system
    return yaze::cli::handlers::HandleOverworldListWarpsCommand(args, nullptr);
}

absl::Status ModernCLI::HandleOverworldSetTileCommand(const std::vector<std::string>& args) {
    // TODO: Implement using OverworldSetTileCommandHandler
    // Reference: src/app/editor/overworld/ (overworld editor classes)
    return absl::UnimplementedError("OverworldSetTileCommandHandler not yet implemented");
}

absl::Status ModernCLI::HandleOverworldSelectRectCommand(const std::vector<std::string>& args) {
    // TODO: Implement using OverworldSelectRectCommandHandler
    // Reference: src/app/editor/overworld/ (overworld editor classes)
    return absl::UnimplementedError("OverworldSelectRectCommandHandler not yet implemented");
}

absl::Status ModernCLI::HandleOverworldScrollToCommand(const std::vector<std::string>& args) {
    // TODO: Implement using OverworldScrollToCommandHandler
    // Reference: src/app/editor/overworld/ (overworld editor classes)
    return absl::UnimplementedError("OverworldScrollToCommandHandler not yet implemented");
}

absl::Status ModernCLI::HandleOverworldSetZoomCommand(const std::vector<std::string>& args) {
    // TODO: Implement using OverworldSetZoomCommandHandler
    // Reference: src/app/editor/overworld/ (overworld editor classes)
    return absl::UnimplementedError("OverworldSetZoomCommandHandler not yet implemented");
}

absl::Status ModernCLI::HandleOverworldGetVisibleRegionCommand(const std::vector<std::string>& args) {
    // TODO: Implement using OverworldGetVisibleRegionCommandHandler
    // Reference: src/app/editor/overworld/ (overworld editor classes)
    return absl::UnimplementedError("OverworldGetVisibleRegionCommandHandler not yet implemented");
}

absl::Status ModernCLI::HandleSpriteCreateCommand(const std::vector<std::string>& args) {
    // TODO: Implement using SpriteCreateCommandHandler
    // Reference: src/app/zelda3/sprite/ (sprite management classes)
    return absl::UnimplementedError("SpriteCreateCommandHandler not yet implemented");
}

absl::Status ModernCLI::HandleChatEntryCommand(
  const std::vector<std::string>& args) {
  std::string mode = "simple";
  std::optional<std::string> prompt;
  std::vector<std::string> forwarded;

  for (size_t i = 0; i < args.size(); ++i) {
    const std::string& token = args[i];
    if (absl::StartsWith(token, "--mode=")) {
      mode = absl::AsciiStrToLower(token.substr(7));
      continue;
    }
    if (token == "--mode" && i + 1 < args.size()) {
      mode = absl::AsciiStrToLower(args[i + 1]);
      ++i;
      continue;
    }
    if (absl::StartsWith(token, "--prompt=")) {
      prompt = token.substr(9);
      continue;
    }
    if (token == "--prompt" && i + 1 < args.size()) {
      prompt = args[i + 1];
      ++i;
      continue;
    }
    if (token == "--quiet" || token == "-q") {
      absl::SetFlag(&FLAGS_quiet, true);
      continue;
    }
    if (!absl::StartsWith(token, "--") && !prompt.has_value()) {
      prompt = token;
      continue;
    }
    forwarded.push_back(token);
  }

  const std::string normalized_mode = absl::AsciiStrToLower(mode);

  auto has_batch_file = [&forwarded]() {
    for (const auto& token : forwarded) {
      if (absl::StartsWith(token, "--file") || token == "--file") {
        return true;
      }
    }
    return false;
  };

  std::vector<std::string> agent_args;
  if (normalized_mode == "gui" || normalized_mode == "visual" ||
    normalized_mode == "tui") {
    if (prompt.has_value()) {
      return absl::InvalidArgumentError(
        "GUI chat mode launches the interactive TUI and does not accept a --prompt value.");
    }
    agent_args.push_back("chat");
  } else if (normalized_mode == "test" || normalized_mode == "qa") {
    if (prompt.has_value()) {
      return absl::InvalidArgumentError(
        "Test conversation mode does not accept an inline prompt.");
    }
    agent_args.push_back("test-conversation");
  } else {
    if (normalized_mode == "batch" && !has_batch_file()) {
      return absl::InvalidArgumentError(
        "Batch chat mode requires a --file=<path> option.");
    }
    agent_args.push_back("simple-chat");
    if (prompt.has_value()) {
      agent_args.push_back(*prompt);
    }
  }

  agent_args.insert(agent_args.end(), forwarded.begin(), forwarded.end());
  return HandleAgentCommand(agent_args);
}

absl::Status ModernCLI::HandleProposalCommand(
  const std::vector<std::string>& args) {
  if (args.empty()) {
    ShowCategoryHelp("proposal");
    return absl::OkStatus();
  }

  std::string subcommand = absl::AsciiStrToLower(args[0]);
  std::vector<std::string> forwarded(args.begin() + 1, args.end());
  std::vector<std::string> agent_args;

  if (subcommand == "run" || subcommand == "plan") {
    agent_args.push_back(subcommand);
  } else if (subcommand == "list") {
    agent_args.push_back("list");
  } else if (subcommand == "diff" || subcommand == "show") {
    agent_args.push_back("diff");
  } else if (subcommand == "accept") {
    agent_args.push_back("accept");
  } else if (subcommand == "commit") {
    agent_args.push_back("commit");
  } else if (subcommand == "revert" || subcommand == "reject") {
    agent_args.push_back("revert");
  } else if (subcommand == "test") {
    agent_args.push_back("test");
  } else {
    return absl::InvalidArgumentError(
      absl::StrCat("Unknown proposal command: ", subcommand,
              ". Valid actions: run, plan, list, diff, show, accept, commit, revert."));
  }

  agent_args.insert(agent_args.end(), forwarded.begin(), forwarded.end());
  return HandleAgentCommand(agent_args);
}

absl::Status ModernCLI::HandleWidgetCommand(
  const std::vector<std::string>& args) {
  if (args.empty()) {
    ShowCategoryHelp("widget");
    return absl::OkStatus();
  }

  std::vector<std::string> forwarded(args.begin(), args.end());
  std::string subcommand = absl::AsciiStrToLower(forwarded[0]);
  std::vector<std::string> agent_args;

  if (subcommand == "discover") {
    agent_args.push_back("gui");
    agent_args.insert(agent_args.end(), forwarded.begin(), forwarded.end());
  } else {
    return absl::InvalidArgumentError(
      absl::StrCat("Unknown widget command: ", forwarded[0],
              ". Try 'z3ed widget discover'."));
  }

  return HandleAgentCommand(agent_args);
}

// TODO: Implement remaining legacy commands using new CommandHandler system
// 
// PENDING IMPLEMENTATIONS:
// 
// 1. ApplyPatch - Apply BPS patches to ROM
//    Reference: src/app/core/asar_wrapper.cc (for patch application logic)
//    TODO: Create BpsPatchCommandHandler in cli/handlers/rom/
//
// 2. AsarPatch - Apply ASAR assembly patches to ROM  
//    Reference: src/app/core/asar_wrapper.cc (AsarWrapper class)
//    TODO: Create AsarPatchCommandHandler in cli/handlers/rom/
//
// 3. ProjectInit - Initialize new Yaze projects
//    Reference: src/app/core/project.cc (Project class)
//    TODO: Implement ProjectInitCommandHandler in cli/handlers/rom/project_commands.cc
//
// 4. ProjectBuild - Build Yaze projects
//    Reference: src/app/core/project.cc (Project::Build method)
//    TODO: Implement ProjectBuildCommandHandler in cli/handlers/rom/project_commands.cc
//
// 5. RomValidate - Validate ROM integrity
//    Reference: src/app/rom.cc (Rom class validation methods)
//    TODO: Create RomValidateCommandHandler in cli/handlers/rom/
//
// 6. RomDiff - Compare ROM files
//    Reference: src/app/rom.cc (Rom comparison methods)
//    TODO: Implement RomDiffCommandHandler in cli/handlers/rom/rom_commands.cc
//
// 7. RomInfo - Display ROM information
//    Reference: src/app/rom.cc (Rom::GetInfo methods)
//    TODO: Implement RomInfoCommandHandler in cli/handlers/rom/rom_commands.cc
//
// 8. SpriteCreate - Create new sprites
//    Reference: src/app/zelda3/sprite/ (sprite management classes)
//    TODO: Create SpriteCreateCommandHandler in cli/handlers/graphics/
//
// 9. CommandPalette - Interactive command palette
//    Reference: src/app/editor/system/command_palette.cc
//    TODO: Create CommandPaletteCommandHandler in cli/handlers/tools/
//
// 10. DungeonExport - Export dungeon data
//     Reference: src/app/editor/dungeon/ (dungeon editor classes)
//     TODO: Implement DungeonExportCommandHandler in cli/handlers/game/dungeon_commands.cc
//
// 11. DungeonListObjects - List dungeon objects
//     Reference: src/app/zelda3/dungeon/ (dungeon data classes)
//     TODO: Implement DungeonListObjectsCommandHandler in cli/handlers/game/dungeon_commands.cc
//
// 12. GfxExport/GfxImport - Graphics import/export
//     Reference: src/app/editor/graphics/ (graphics editor classes)
//     TODO: Create GfxExportCommandHandler and GfxImportCommandHandler in cli/handlers/graphics/
//
// 13. PaletteExport/PaletteImport - Palette import/export
//     Reference: src/app/editor/graphics/palette_editor.cc
//     TODO: Implement PaletteExportCommandHandler and PaletteImportCommandHandler in cli/handlers/graphics/palette_commands.cc
//
// 14. Overworld commands - Overworld manipulation
//     Reference: src/app/editor/overworld/ (overworld editor classes)
//     TODO: Implement OverworldCommandHandler in cli/handlers/game/overworld_commands.cc
//
// 15. Agent command - AI agent interface
//     Reference: src/app/editor/agent/ (agent system)
//     TODO: Implement AgentCommandHandler in cli/handlers/agent/

}  // namespace cli
}  // namespace yaze
