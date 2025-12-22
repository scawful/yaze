/**
 * @file test_helpers_commands.h
 * @brief CLI command handlers for test helper tools
 *
 * Provides z3ed subcommands that wrap the standalone test helper tools:
 * - tools harness-state: Generate WRAM state for test harnesses
 * - tools extract-values: Extract vanilla ROM values
 * - tools extract-golden: Extract comprehensive golden data
 * - tools patch-v3: Create v3 ZSCustomOverworld patched ROM
 */

#ifndef YAZE_SRC_CLI_HANDLERS_TOOLS_TEST_HELPERS_COMMANDS_H_
#define YAZE_SRC_CLI_HANDLERS_TOOLS_TEST_HELPERS_COMMANDS_H_

#include <fstream>
#include <iomanip>
#include <sstream>
#include <string>
#include <vector>

#include "absl/status/status.h"
#include "app/emu/snes.h"
#include "rom/rom.h"
#include "cli/service/resources/command_handler.h"
#include "zelda3/overworld/overworld.h"
#include "zelda3/overworld/overworld_map.h"

namespace yaze {
namespace cli {
namespace handlers {

/**
 * @brief Generate WRAM state for test harnesses
 *
 * Runs the emulator to the main game loop and dumps the state.
 * Usage: z3ed tools harness-state --rom=<path> --output=<path>
 */
class ToolsHarnessStateCommandHandler : public resources::CommandHandler {
 public:
  std::string GetName() const override { return "tools-harness-state"; }
  std::string GetDescription() const {
    return "Generate WRAM state for test harnesses by running emulator to "
           "game loop";
  }
  std::string GetUsage() const override {
    return "tools-harness-state --rom=<path> --output=<path>";
  }

  absl::Status ValidateArgs(const resources::ArgumentParser& parser) override {
    return parser.RequireArgs({"rom", "output"});
  }

  absl::Status Execute(Rom* rom, const resources::ArgumentParser& parser,
                       resources::OutputFormatter& formatter) override;

 private:
  absl::Status GenerateHarnessState(const std::string& rom_path,
                                    const std::string& output_path);
};

/**
 * @brief Extract vanilla ROM values to stdout
 *
 * Extracts key ROM constants for test reference.
 * Usage: z3ed tools extract-values --rom=<path>
 */
class ToolsExtractValuesCommandHandler : public resources::CommandHandler {
 public:
  std::string GetName() const override { return "tools-extract-values"; }
  std::string GetDescription() const {
    return "Extract vanilla ROM values for test reference";
  }
  std::string GetUsage() const override {
    return "tools-extract-values --rom=<path> [--format=<json|cpp>]";
  }

  absl::Status ValidateArgs(const resources::ArgumentParser& parser) override {
    return parser.RequireArgs({"rom"});
  }

  absl::Status Execute(Rom* rom, const resources::ArgumentParser& parser,
                       resources::OutputFormatter& formatter) override;
};

/**
 * @brief Extract comprehensive golden data from ROM
 *
 * Extracts all overworld-related values for golden data testing.
 * Usage: z3ed tools extract-golden --rom=<path> --output=<path>
 */
class ToolsExtractGoldenCommandHandler : public resources::CommandHandler {
 public:
  std::string GetName() const override { return "tools-extract-golden"; }
  std::string GetDescription() const {
    return "Extract comprehensive golden data from ROM for E2E testing";
  }
  std::string GetUsage() const override {
    return "tools-extract-golden --rom=<path> --output=<path>";
  }

  absl::Status ValidateArgs(const resources::ArgumentParser& parser) override {
    return parser.RequireArgs({"rom", "output"});
  }

  absl::Status Execute(Rom* rom, const resources::ArgumentParser& parser,
                       resources::OutputFormatter& formatter) override;

 private:
  void WriteHeader(std::ofstream& out, const std::string& rom_path);
  void WriteBasicROMInfo(std::ofstream& out, Rom& rom);
  void WriteASMVersionInfo(std::ofstream& out, Rom& rom);
  void WriteOverworldMapsData(std::ofstream& out,
                              zelda3::Overworld& overworld);
  void WriteTileData(std::ofstream& out, zelda3::Overworld& overworld);
  void WriteEntranceData(std::ofstream& out, zelda3::Overworld& overworld);
  void WriteFooter(std::ofstream& out);
};

/**
 * @brief Create v3 ZSCustomOverworld patched ROM
 *
 * Applies v3 patches to a ROM for testing.
 * Usage: z3ed tools patch-v3 --rom=<input> --output=<output>
 */
class ToolsPatchV3CommandHandler : public resources::CommandHandler {
 public:
  std::string GetName() const override { return "tools-patch-v3"; }
  std::string GetDescription() const {
    return "Create v3 ZSCustomOverworld patched ROM for testing";
  }
  std::string GetUsage() const override {
    return "tools-patch-v3 --rom=<input> --output=<output>";
  }

  absl::Status ValidateArgs(const resources::ArgumentParser& parser) override {
    return parser.RequireArgs({"rom", "output"});
  }

  absl::Status Execute(Rom* rom, const resources::ArgumentParser& parser,
                       resources::OutputFormatter& formatter) override;

 private:
  absl::Status ApplyV3Patch(Rom& rom);
};

/**
 * @brief List available test helper tools
 *
 * Usage: z3ed tools list
 */
class ToolsListCommandHandler : public resources::CommandHandler {
 public:
  std::string GetName() const override { return "tools-list"; }
  std::string GetDescription() const {
    return "List available test helper tools";
  }
  std::string GetUsage() const override { return "tools-list"; }

  absl::Status ValidateArgs(
      const resources::ArgumentParser& /*parser*/) override {
    return absl::OkStatus();
  }

  absl::Status Execute(Rom* rom, const resources::ArgumentParser& parser,
                       resources::OutputFormatter& formatter) override;

  bool RequiresRom() const override { return false; }
};

}  // namespace handlers
}  // namespace cli
}  // namespace yaze

#endif  // YAZE_SRC_CLI_HANDLERS_TOOLS_TEST_HELPERS_COMMANDS_H_

