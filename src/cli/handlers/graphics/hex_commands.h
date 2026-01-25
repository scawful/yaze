#ifndef YAZE_SRC_CLI_HANDLERS_HEX_COMMANDS_H_
#define YAZE_SRC_CLI_HANDLERS_HEX_COMMANDS_H_

#include "cli/service/resources/command_handler.h"

namespace yaze {
namespace cli {
namespace handlers {

/**
 * @brief Command handler for reading hex data from ROM
 */
class HexReadCommandHandler : public resources::CommandHandler {
 public:
  std::string GetName() const { return "hex-read"; }
  std::string GetDescription() const {
    return "Read hex data from ROM at specified address";
  }
  std::string GetUsage() const {
    return "hex-read --address <address> [--length <length>] "
           "[--data-format <hex|ascii|both>] [--format <json|text>]";
  }

  absl::Status ValidateArgs(const resources::ArgumentParser& parser) override {
    return parser.RequireArgs({"address"});
  }

  absl::Status Execute(Rom* rom, const resources::ArgumentParser& parser,
                       resources::OutputFormatter& formatter) override;
};

/**
 * @brief Command handler for writing hex data to ROM
 */
class HexWriteCommandHandler : public resources::CommandHandler {
 public:
  std::string GetName() const { return "hex-write"; }
  std::string GetDescription() const {
    return "Write hex data to ROM at specified address";
  }
  std::string GetUsage() const {
    return "hex-write --address <address> --data <data>";
  }

  absl::Status ValidateArgs(const resources::ArgumentParser& parser) override {
    return parser.RequireArgs({"address", "data"});
  }

  absl::Status Execute(Rom* rom, const resources::ArgumentParser& parser,
                       resources::OutputFormatter& formatter) override;
};

/**
 * @brief Command handler for searching hex patterns in ROM
 */
class HexSearchCommandHandler : public resources::CommandHandler {
 public:
  std::string GetName() const { return "hex-search"; }
  std::string GetDescription() const {
    return "Search for hex patterns in ROM";
  }
  std::string GetUsage() const {
    return "hex-search --pattern <pattern> [--start <start>] [--end <end>]";
  }

  absl::Status ValidateArgs(const resources::ArgumentParser& parser) override {
    return parser.RequireArgs({"pattern"});
  }

  absl::Status Execute(Rom* rom, const resources::ArgumentParser& parser,
                       resources::OutputFormatter& formatter) override;
};

}  // namespace handlers
}  // namespace cli
}  // namespace yaze

#endif  // YAZE_SRC_CLI_HANDLERS_HEX_COMMANDS_H_
