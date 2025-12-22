#ifndef YAZE_SRC_CLI_HANDLERS_TOOLS_MESSAGE_DOCTOR_COMMANDS_H_
#define YAZE_SRC_CLI_HANDLERS_TOOLS_MESSAGE_DOCTOR_COMMANDS_H_

#include "cli/service/resources/command_handler.h"

namespace yaze {
namespace cli {

class MessageDoctorCommandHandler : public resources::CommandHandler {
 public:
  MessageDoctorCommandHandler() = default;
  ~MessageDoctorCommandHandler() override = default;

  std::string GetName() const override { return "message-doctor"; }
  std::string GetUsage() const override { return "message-doctor [flags]"; }
  
  resources::CommandHandler::Descriptor Describe() const override {
    resources::CommandHandler::Descriptor desc;
    desc.summary = "Scan and validate in-game messages/dialogue";
    return desc;
  }

  bool RequiresRom() const override { return true; }

  absl::Status ValidateArgs(const resources::ArgumentParser& parser) override {
    // No required arguments other than ROM which is handled by base class
    return absl::OkStatus();
  }

  absl::Status Execute(Rom* rom, const resources::ArgumentParser& args,
                       resources::OutputFormatter& formatter) override;
};

}  // namespace cli
}  // namespace yaze

#endif  // YAZE_SRC_CLI_HANDLERS_TOOLS_MESSAGE_DOCTOR_COMMANDS_H_
