#ifndef YAZE_CLI_HANDLERS_TOOLS_HEX_INSPECTOR_COMMANDS_H
#define YAZE_CLI_HANDLERS_TOOLS_HEX_INSPECTOR_COMMANDS_H

#include "cli/service/resources/command_handler.h"

namespace yaze {
namespace cli {

/**
 * @brief Dump a hex view of a ROM file
 *
 * Usage: z3ed hex-dump <rom_path> <offset> [size]
 */
class HexDumpCommandHandler : public resources::CommandHandler {
 public:
  std::string GetName() const override { return "hex-dump"; }

  std::string GetUsage() const override {
    return "hex-dump <rom_path> <offset> [size] [--mode snes|pc]";
  }

  std::string GetDefaultFormat() const override { return "text"; }

  std::string GetOutputTitle() const override { return "Hex Dump"; }

  bool RequiresRom() const override { return false; }

  Descriptor Describe() const override {
    Descriptor desc;
    desc.display_name = "hex-dump";
    desc.summary = "Dump a hex view of a ROM file at a specific offset.";
    desc.todo_reference = "todo#hex-dump";
    return desc;
  }

  absl::Status ValidateArgs(const resources::ArgumentParser& parser) override;

  absl::Status Execute(Rom* rom, const resources::ArgumentParser& parser,
                       resources::OutputFormatter& formatter) override;
};

/**
 * @brief Compare two ROM files byte-by-byte
 *
 * Usage: z3ed hex-compare <rom1> <rom2> [--start <offset>] [--end <offset>]
 */
class HexCompareCommandHandler : public resources::CommandHandler {
 public:
  std::string GetName() const override { return "hex-compare"; }

  std::string GetUsage() const override {
    return "hex-compare <rom1> <rom2> [--start <offset>] [--end <offset>] "
           "[--mode snes|pc]";
  }

  std::string GetDefaultFormat() const override { return "text"; }

  std::string GetOutputTitle() const override { return "Hex Compare"; }

  bool RequiresRom() const override { return false; }

  Descriptor Describe() const override {
    Descriptor desc;
    desc.display_name = "hex-compare";
    desc.summary =
        "Compare two ROM files byte-by-byte with optional region filtering.";
    return desc;
  }

  absl::Status ValidateArgs(const resources::ArgumentParser& parser) override;

  absl::Status Execute(Rom* rom, const resources::ArgumentParser& parser,
                       resources::OutputFormatter& formatter) override;
};

/**
 * @brief Show known data structure annotations at a ROM offset
 *
 * Usage: z3ed hex-annotate <rom_path> <offset> [--type <type>]
 */
class HexAnnotateCommandHandler : public resources::CommandHandler {
 public:
  std::string GetName() const override { return "hex-annotate"; }

  std::string GetUsage() const override {
    return "hex-annotate <rom_path> <offset> "
           "[--type auto|room_header|sprite|tile16|message|snes_header]";
  }

  std::string GetDefaultFormat() const override { return "text"; }

  std::string GetOutputTitle() const override { return "Hex Annotate"; }

  bool RequiresRom() const override { return false; }

  Descriptor Describe() const override {
    Descriptor desc;
    desc.display_name = "hex-annotate";
    desc.summary = "Show known data structure annotations at a ROM offset.";
    return desc;
  }

  absl::Status ValidateArgs(const resources::ArgumentParser& parser) override;

  absl::Status Execute(Rom* rom, const resources::ArgumentParser& parser,
                       resources::OutputFormatter& formatter) override;
};

}  // namespace cli
}  // namespace yaze

#endif  // YAZE_CLI_HANDLERS_TOOLS_HEX_INSPECTOR_COMMANDS_H
