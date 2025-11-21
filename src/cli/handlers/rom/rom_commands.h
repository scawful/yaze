#ifndef YAZE_SRC_CLI_HANDLERS_ROM_COMMANDS_H_
#define YAZE_SRC_CLI_HANDLERS_ROM_COMMANDS_H_

#include "cli/service/resources/command_handler.h"

namespace yaze {
namespace cli {
namespace handlers {

/**
 * @brief Command handler for displaying ROM information
 */
class RomInfoCommandHandler : public resources::CommandHandler {
 public:
  std::string GetName() const { return "rom-info"; }
  std::string GetDescription() const { return "Display ROM information"; }
  std::string GetUsage() const { return "rom-info"; }

  absl::Status ValidateArgs(const resources::ArgumentParser& parser) override {
    return absl::OkStatus();
  }

  absl::Status Execute(Rom* rom, const resources::ArgumentParser& parser,
                       resources::OutputFormatter& formatter) override;
};

/**
 * @brief Command handler for validating ROM files
 */
class RomValidateCommandHandler : public resources::CommandHandler {
 public:
  std::string GetName() const { return "rom-validate"; }
  std::string GetDescription() const { return "Validate ROM file integrity"; }
  std::string GetUsage() const { return "rom-validate"; }

  absl::Status ValidateArgs(const resources::ArgumentParser& parser) override {
    return absl::OkStatus();
  }

  absl::Status Execute(Rom* rom, const resources::ArgumentParser& parser,
                       resources::OutputFormatter& formatter) override;
};

/**
 * @brief Command handler for comparing ROM files
 */
class RomDiffCommandHandler : public resources::CommandHandler {
 public:
  std::string GetName() const { return "rom-diff"; }
  std::string GetDescription() const { return "Compare two ROM files"; }
  std::string GetUsage() const {
    return "rom-diff --rom_a <file> --rom_b <file>";
  }

  absl::Status ValidateArgs(const resources::ArgumentParser& parser) override {
    return parser.RequireArgs({"rom_a", "rom_b"});
  }

  absl::Status Execute(Rom* rom, const resources::ArgumentParser& parser,
                       resources::OutputFormatter& formatter) override;
};

/**
 * @brief Command handler for generating golden ROM files
 */
class RomGenerateGoldenCommandHandler : public resources::CommandHandler {
 public:
  std::string GetName() const { return "rom-generate-golden"; }
  std::string GetDescription() const {
    return "Generate golden ROM file for testing";
  }
  std::string GetUsage() const {
    return "rom-generate-golden --rom_file <file> --golden_file <file>";
  }

  absl::Status ValidateArgs(const resources::ArgumentParser& parser) override {
    return parser.RequireArgs({"rom_file", "golden_file"});
  }

  absl::Status Execute(Rom* rom, const resources::ArgumentParser& parser,
                       resources::OutputFormatter& formatter) override;
};

}  // namespace handlers
}  // namespace cli
}  // namespace yaze

#endif  // YAZE_SRC_CLI_HANDLERS_ROM_COMMANDS_H_
