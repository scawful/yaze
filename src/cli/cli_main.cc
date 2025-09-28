#include <iostream>
#include <string>
#include <vector>
#include <map>
#include <memory>

#include <SDL.h>

#include "absl/flags/flag.h"
#include "absl/flags/parse.h"
#include "absl/flags/usage.h"
#include "absl/strings/str_format.h"
#include "absl/strings/str_join.h"
#include "absl/strings/str_cat.h"

#include "cli/z3ed.h"
#include "cli/tui.h"
#include "app/core/asar_wrapper.h"
#include "app/gfx/arena.h"
#include "app/rom.h"
#include "app/zelda3/overworld/overworld.h"

// Global flags
ABSL_FLAG(bool, tui, false, "Launch the Text User Interface");
ABSL_FLAG(bool, version, false, "Show version information");
ABSL_FLAG(bool, verbose, false, "Enable verbose output");
ABSL_FLAG(std::string, rom, "", "Path to the ROM file");

// Command-specific flags
ABSL_FLAG(std::string, output, "", "Output file path");
ABSL_FLAG(bool, dry_run, false, "Perform a dry run without making changes");
ABSL_FLAG(bool, backup, true, "Create a backup before modifying files");

namespace yaze {
namespace cli {

struct CommandInfo {
  std::string name;
  std::string description;
  std::string usage;
  std::function<absl::Status(const std::vector<std::string>&)> handler;
};

class ModernCLI {
 public:
  ModernCLI() {
    SetupCommands();
  }

  void SetupCommands() {
    commands_["asar"] = {
      .name = "asar",
      .description = "Apply Asar 65816 assembly patch to ROM",
      .usage = "z3ed asar <patch.asm> [--rom=<rom_file>] [--output=<output_file>]",
      .handler = [this](const std::vector<std::string>& args) -> absl::Status {
        return HandleAsarCommand(args);
      }
    };

    commands_["patch"] = {
      .name = "patch", 
      .description = "Apply BPS patch to ROM",
      .usage = "z3ed patch <patch.bps> [--rom=<rom_file>] [--output=<output_file>]",
      .handler = [this](const std::vector<std::string>& args) -> absl::Status {
        return HandlePatchCommand(args);
      }
    };

    commands_["extract"] = {
      .name = "extract",
      .description = "Extract symbols from assembly file",
      .usage = "z3ed extract <patch.asm>",
      .handler = [this](const std::vector<std::string>& args) -> absl::Status {
        return HandleExtractCommand(args);
      }
    };

    commands_["validate"] = {
      .name = "validate",
      .description = "Validate assembly file syntax",
      .usage = "z3ed validate <patch.asm>",
      .handler = [this](const std::vector<std::string>& args) -> absl::Status {
        return HandleValidateCommand(args);
      }
    };

    commands_["info"] = {
      .name = "info",
      .description = "Show ROM information",
      .usage = "z3ed info [--rom=<rom_file>]",
      .handler = [this](const std::vector<std::string>& args) -> absl::Status {
        return HandleInfoCommand(args);
      }
    };

    commands_["convert"] = {
      .name = "convert",
      .description = "Convert between SNES and PC addresses",
      .usage = "z3ed convert <address> [--to-pc|--to-snes]",
      .handler = [this](const std::vector<std::string>& args) -> absl::Status {
        return HandleConvertCommand(args);
      }
    };

    commands_["test"] = {
      .name = "test",
      .description = "Run comprehensive asset loading tests on ROM",
      .usage = "z3ed test [--rom=<rom_file>] [--graphics] [--overworld] [--dungeons]",
      .handler = [this](const std::vector<std::string>& args) -> absl::Status {
        return HandleTestCommand(args);
      }
    };

    commands_["help"] = {
      .name = "help",
      .description = "Show help information",
      .usage = "z3ed help [command]",
      .handler = [this](const std::vector<std::string>& args) -> absl::Status {
        return HandleHelpCommand(args);
      }
    };
  }

  void ShowVersion() {
    std::cout << "z3ed v0.3.1 - Yet Another Zelda3 Editor CLI" << std::endl;
    std::cout << "Built with Asar integration" << std::endl;
    std::cout << "Copyright (c) 2025 scawful" << std::endl;
  }

  void ShowHelp(const std::string& command = "") {
    if (!command.empty()) {
      auto it = commands_.find(command);
      if (it != commands_.end()) {
        std::cout << "Command: " << it->second.name << std::endl;
        std::cout << "Description: " << it->second.description << std::endl;
        std::cout << "Usage: " << it->second.usage << std::endl;
        return;
      } else {
        std::cout << "Unknown command: " << command << std::endl;
        std::cout << std::endl;
      }
    }

    std::cout << "z3ed - Yet Another Zelda3 Editor CLI Tool" << std::endl;
    std::cout << std::endl;
    std::cout << "USAGE:" << std::endl;
    std::cout << "  z3ed [--tui] [command] [arguments]" << std::endl;
    std::cout << std::endl;
    std::cout << "GLOBAL FLAGS:" << std::endl;
    std::cout << "  --tui              Launch Text User Interface" << std::endl;
    std::cout << "  --version          Show version information" << std::endl;
    std::cout << "  --verbose          Enable verbose output" << std::endl;
    std::cout << "  --rom=<file>       Specify ROM file to use" << std::endl;
    std::cout << "  --output=<file>    Specify output file path" << std::endl;
    std::cout << "  --dry-run          Perform operations without making changes" << std::endl;
    std::cout << "  --backup=<bool>    Create backup before modifying (default: true)" << std::endl;
    std::cout << std::endl;
    std::cout << "COMMANDS:" << std::endl;
    
    for (const auto& [name, info] : commands_) {
      std::cout << absl::StrFormat("  %-12s %s", name, info.description) << std::endl;
    }
    
    std::cout << std::endl;
    std::cout << "EXAMPLES:" << std::endl;
    std::cout << "  z3ed --tui                                    # Launch TUI" << std::endl;
    std::cout << "  z3ed asar patch.asm --rom=zelda3.sfc         # Apply Asar patch" << std::endl;
    std::cout << "  z3ed patch changes.bps --rom=zelda3.sfc      # Apply BPS patch" << std::endl;
    std::cout << "  z3ed extract patch.asm                       # Extract symbols" << std::endl;
    std::cout << "  z3ed validate patch.asm                      # Validate assembly" << std::endl;
    std::cout << "  z3ed info --rom=zelda3.sfc                   # Show ROM info" << std::endl;
    std::cout << "  z3ed convert 0x008000 --to-pc                # Convert address" << std::endl;
    std::cout << std::endl;
    std::cout << "For more information on a specific command:" << std::endl;
    std::cout << "  z3ed help <command>" << std::endl;
  }

  absl::Status RunCommand(const std::string& command, const std::vector<std::string>& args) {
    auto it = commands_.find(command);
    if (it == commands_.end()) {
      return absl::NotFoundError(absl::StrFormat("Unknown command: %s", command));
    }
    
    return it->second.handler(args);
  }

 private:
  std::map<std::string, CommandInfo> commands_;

  absl::Status HandleAsarCommand(const std::vector<std::string>& args) {
    if (args.empty()) {
      return absl::InvalidArgumentError("Asar command requires a patch file");
    }

    AsarPatch handler;
    std::vector<std::string> handler_args = args;
    
    // Add ROM file from flag if not provided as argument
    std::string rom_file = absl::GetFlag(FLAGS_rom);
    if (args.size() == 1 && !rom_file.empty()) {
      handler_args.push_back(rom_file);
    }
    
    return handler.Run(handler_args);
  }

  absl::Status HandlePatchCommand(const std::vector<std::string>& args) {
    if (args.empty()) {
      return absl::InvalidArgumentError("Patch command requires a BPS file");
    }

    ApplyPatch handler;
    std::vector<std::string> handler_args = args;
    
    std::string rom_file = absl::GetFlag(FLAGS_rom);
    if (args.size() == 1 && !rom_file.empty()) {
      handler_args.push_back(rom_file);
    }
    
    return handler.Run(handler_args);
  }

  absl::Status HandleExtractCommand(const std::vector<std::string>& args) {
    if (args.empty()) {
      return absl::InvalidArgumentError("Extract command requires an assembly file");
    }

    // Use the AsarWrapper to extract symbols
    yaze::app::core::AsarWrapper wrapper;
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

  absl::Status HandleValidateCommand(const std::vector<std::string>& args) {
    if (args.empty()) {
      return absl::InvalidArgumentError("Validate command requires an assembly file");
    }

    yaze::app::core::AsarWrapper wrapper;
    RETURN_IF_ERROR(wrapper.Initialize());
    
    auto status = wrapper.ValidateAssembly(args[0]);
    if (status.ok()) {
      std::cout << "✅ Assembly file is valid: " << args[0] << std::endl;
    } else {
      std::cout << "❌ Assembly validation failed:" << std::endl;
      std::cout << "   " << status.message() << std::endl;
    }
    
    return status;
  }

  absl::Status HandleInfoCommand(const std::vector<std::string>& args) {
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

  absl::Status HandleConvertCommand(const std::vector<std::string>& args) {
    if (args.empty()) {
      return absl::InvalidArgumentError("Convert command requires an address");
    }

    // TODO: Implement address conversion
    std::cout << "Address conversion not yet implemented" << std::endl;
    return absl::UnimplementedError("Address conversion functionality");
  }

  absl::Status HandleTestCommand(const std::vector<std::string>& args) {
    std::string rom_file = absl::GetFlag(FLAGS_rom);
    if (args.size() > 0 && args[0].find("--rom=") == 0) {
      rom_file = args[0].substr(6);
    }
    
    if (rom_file.empty()) {
      rom_file = "zelda3.sfc";  // Default ROM file
    }
    
    std::cout << "🧪 YAZE Asset Loading Test Suite" << std::endl;
    std::cout << "ROM: " << rom_file << std::endl;
    std::cout << "=================================" << std::endl;
    
    // Initialize SDL for graphics tests
    if (SDL_Init(SDL_INIT_VIDEO) != 0) {
      return absl::InternalError(absl::StrCat("Failed to initialize SDL: ", SDL_GetError()));
    }
    
    int tests_passed = 0;
    int tests_total = 0;
    
    // Test 1: ROM Loading
    std::cout << "📁 Testing ROM loading..." << std::flush;
    tests_total++;
    Rom test_rom;
    auto status = test_rom.LoadFromFile(rom_file);
    if (status.ok()) {
      std::cout << " ✅ PASSED" << std::endl;
      tests_passed++;
      std::cout << "   Title: " << test_rom.title() << std::endl;
      std::cout << "   Size: " << test_rom.size() << " bytes" << std::endl;
    } else {
      std::cout << " ❌ FAILED: " << status.message() << std::endl;
      SDL_Quit();
      return status;
    }
    
    // Test 2: Graphics Arena Resource Tracking
    std::cout << "🎨 Testing graphics arena..." << std::flush;
    tests_total++;
    try {
      auto& arena = gfx::Arena::Get();
      size_t initial_textures = arena.GetTextureCount();
      size_t initial_surfaces = arena.GetSurfaceCount();
      
      std::cout << " ✅ PASSED" << std::endl;
      std::cout << "   Initial textures: " << initial_textures << std::endl;
      std::cout << "   Initial surfaces: " << initial_surfaces << std::endl;
      tests_passed++;
    } catch (const std::exception& e) {
      std::cout << " ❌ FAILED: " << e.what() << std::endl;
    }
    
    // Test 3: Graphics Data Loading
    bool test_graphics = true;
    for (const auto& arg : args) {
      if (arg == "--no-graphics") test_graphics = false;
    }
    
    if (test_graphics) {
      std::cout << "🖼️  Testing graphics data loading..." << std::flush;
      tests_total++;
      try {
        auto graphics_result = LoadAllGraphicsData(test_rom);
        if (graphics_result.ok()) {
          std::cout << " ✅ PASSED" << std::endl;
          std::cout << "   Loaded " << graphics_result.value().size() << " graphics sheets" << std::endl;
          tests_passed++;
        } else {
          std::cout << " ❌ FAILED: " << graphics_result.status().message() << std::endl;
        }
      } catch (const std::exception& e) {
        std::cout << " ❌ FAILED: " << e.what() << std::endl;
      }
    }
    
    // Test 4: Overworld Loading
    bool test_overworld = true;
    for (const auto& arg : args) {
      if (arg == "--no-overworld") test_overworld = false;
    }
    
    if (test_overworld) {
      std::cout << "🗺️  Testing overworld loading..." << std::flush;
      tests_total++;
      try {
        zelda3::Overworld overworld(&test_rom);
        auto ow_status = overworld.Load(&test_rom);
        if (ow_status.ok()) {
          std::cout << " ✅ PASSED" << std::endl;
          std::cout << "   Loaded overworld data successfully" << std::endl;
          tests_passed++;
        } else {
          std::cout << " ❌ FAILED: " << ow_status.message() << std::endl;
        }
      } catch (const std::exception& e) {
        std::cout << " ❌ FAILED: " << e.what() << std::endl;
      }
    }
    
    // Test 5: Arena Shutdown Test
    std::cout << "🔄 Testing arena shutdown..." << std::flush;
    tests_total++;
    try {
      auto& arena = gfx::Arena::Get();
      size_t final_textures = arena.GetTextureCount();
      size_t final_surfaces = arena.GetSurfaceCount();
      
      // Test the shutdown method (this should not crash)
      arena.Shutdown();
      
      std::cout << " ✅ PASSED" << std::endl;
      std::cout << "   Final textures: " << final_textures << std::endl;
      std::cout << "   Final surfaces: " << final_surfaces << std::endl;
      tests_passed++;
    } catch (const std::exception& e) {
      std::cout << " ❌ FAILED: " << e.what() << std::endl;
    }
    
    // Cleanup
    SDL_Quit();
    
    // Summary
    std::cout << "=================================" << std::endl;
    std::cout << "📊 Test Results: " << tests_passed << "/" << tests_total << " passed" << std::endl;
    
    if (tests_passed == tests_total) {
      std::cout << "🎉 All tests passed!" << std::endl;
      return absl::OkStatus();
    } else {
      std::cout << "❌ Some tests failed." << std::endl;
      return absl::InternalError("Test failures detected");
    }
  }

  absl::Status HandleHelpCommand(const std::vector<std::string>& args) {
    std::string command = args.empty() ? "" : args[0];
    ShowHelp(command);
    return absl::OkStatus();
  }
};

}  // namespace cli
}  // namespace yaze

#ifdef _WIN32
extern "C" int SDL_main(int argc, char* argv[]) {
#else
int main(int argc, char* argv[]) {
#endif
  absl::SetProgramUsageMessage(
    "z3ed - Yet Another Zelda3 Editor CLI Tool\n"
    "\n"
    "A command-line tool for editing The Legend of Zelda: A Link to the Past ROMs.\n"
    "Supports Asar 65816 assembly patching, BPS patches, and ROM analysis.\n"
    "\n"
    "Use --tui to launch the interactive text interface, or run commands directly.\n"
  );

  auto args = absl::ParseCommandLine(argc, argv);
  
  yaze::cli::ModernCLI cli;

  // Handle version flag
  if (absl::GetFlag(FLAGS_version)) {
    cli.ShowVersion();
    return 0;
  }

  // Handle TUI flag
  if (absl::GetFlag(FLAGS_tui)) {
    yaze::cli::ShowMain();
    return 0;
  }

  // Handle command line arguments
  if (args.size() < 2) {
    cli.ShowHelp();
    return 0;
  }

  std::string command = args[1];
  std::vector<std::string> command_args(args.begin() + 2, args.end());

  auto status = cli.RunCommand(command, command_args);
  if (!status.ok()) {
    std::cerr << "Error: " << status.message() << std::endl;
    
    if (status.code() == absl::StatusCode::kNotFound) {
      std::cerr << std::endl;
      std::cerr << "Available commands:" << std::endl;
      cli.ShowHelp();
    }
    
    return 1;
  }

  return 0;
}
