#ifndef YAZE_SRC_CLI_HANDLERS_EMULATOR_COMMANDS_H_
#define YAZE_SRC_CLI_HANDLERS_EMULATOR_COMMANDS_H_

#include "cli/service/resources/command_handler.h"

namespace yaze {
namespace cli {
namespace handlers {

class EmulatorStepCommandHandler : public resources::CommandHandler {
 public:
  std::string GetName() const { return "emulator-step"; }
  std::string GetDescription() const {
    return "Step emulator execution by one instruction";
  }
  std::string GetUsage() const {
    return "emulator-step [--count <count>] [--format <json|text>]";
  }

  absl::Status ValidateArgs(const resources::ArgumentParser& parser) override {
    return absl::OkStatus();
  }

  absl::Status Execute(Rom* rom, const resources::ArgumentParser& parser,
                       resources::OutputFormatter& formatter) override;
};

class EmulatorRunCommandHandler : public resources::CommandHandler {
 public:
  std::string GetName() const { return "emulator-run"; }
  std::string GetDescription() const { return "Run emulator execution"; }
  std::string GetUsage() const {
    return "emulator-run [--until <breakpoint>] [--format <json|text>]";
  }

  absl::Status ValidateArgs(const resources::ArgumentParser& parser) override {
    return absl::OkStatus();
  }

  absl::Status Execute(Rom* rom, const resources::ArgumentParser& parser,
                       resources::OutputFormatter& formatter) override;
};

class EmulatorPauseCommandHandler : public resources::CommandHandler {
 public:
  std::string GetName() const { return "emulator-pause"; }
  std::string GetDescription() const { return "Pause emulator execution"; }
  std::string GetUsage() const {
    return "emulator-pause [--format <json|text>]";
  }

  absl::Status ValidateArgs(const resources::ArgumentParser& parser) override {
    return absl::OkStatus();
  }

  absl::Status Execute(Rom* rom, const resources::ArgumentParser& parser,
                       resources::OutputFormatter& formatter) override;
};

class EmulatorResetCommandHandler : public resources::CommandHandler {
 public:
  std::string GetName() const { return "emulator-reset"; }
  std::string GetDescription() const {
    return "Reset emulator to initial state";
  }
  std::string GetUsage() const {
    return "emulator-reset [--format <json|text>]";
  }

  absl::Status ValidateArgs(const resources::ArgumentParser& parser) override {
    return absl::OkStatus();
  }

  absl::Status Execute(Rom* rom, const resources::ArgumentParser& parser,
                       resources::OutputFormatter& formatter) override;
};

class EmulatorGetStateCommandHandler : public resources::CommandHandler {
 public:
  std::string GetName() const { return "emulator-get-state"; }
  std::string GetDescription() const { return "Get current emulator state"; }
  std::string GetUsage() const {
    return "emulator-get-state [--format <json|text>]";
  }

  absl::Status ValidateArgs(const resources::ArgumentParser& parser) override {
    return absl::OkStatus();
  }

  absl::Status Execute(Rom* rom, const resources::ArgumentParser& parser,
                       resources::OutputFormatter& formatter) override;
};

class EmulatorSetBreakpointCommandHandler : public resources::CommandHandler {
 public:
  std::string GetName() const { return "emulator-set-breakpoint"; }
  std::string GetDescription() const {
    return "Set a breakpoint at specified address";
  }
  std::string GetUsage() const {
    return "emulator-set-breakpoint --address <address> [--condition "
           "<condition>] [--format <json|text>]";
  }

  absl::Status ValidateArgs(const resources::ArgumentParser& parser) override {
    return parser.RequireArgs({"address"});
  }

  absl::Status Execute(Rom* rom, const resources::ArgumentParser& parser,
                       resources::OutputFormatter& formatter) override;
};

class EmulatorClearBreakpointCommandHandler : public resources::CommandHandler {
 public:
  std::string GetName() const { return "emulator-clear-breakpoint"; }
  std::string GetDescription() const {
    return "Clear a breakpoint at specified address";
  }
  std::string GetUsage() const {
    return "emulator-clear-breakpoint --address <address> [--format "
           "<json|text>]";
  }

  absl::Status ValidateArgs(const resources::ArgumentParser& parser) override {
    return parser.RequireArgs({"address"});
  }

  absl::Status Execute(Rom* rom, const resources::ArgumentParser& parser,
                       resources::OutputFormatter& formatter) override;
};

class EmulatorListBreakpointsCommandHandler : public resources::CommandHandler {
 public:
  std::string GetName() const { return "emulator-list-breakpoints"; }
  std::string GetDescription() const { return "List all active breakpoints"; }
  std::string GetUsage() const {
    return "emulator-list-breakpoints [--format <json|text>]";
  }

  absl::Status ValidateArgs(const resources::ArgumentParser& parser) override {
    return absl::OkStatus();
  }

  absl::Status Execute(Rom* rom, const resources::ArgumentParser& parser,
                       resources::OutputFormatter& formatter) override;
};

class EmulatorReadMemoryCommandHandler : public resources::CommandHandler {
 public:
  std::string GetName() const { return "emulator-read-memory"; }
  std::string GetDescription() const { return "Read memory from emulator"; }
  std::string GetUsage() const {
    return "emulator-read-memory --address <address> [--length <length>] "
           "[--format <json|text>]";
  }

  absl::Status ValidateArgs(const resources::ArgumentParser& parser) override {
    return parser.RequireArgs({"address"});
  }

  absl::Status Execute(Rom* rom, const resources::ArgumentParser& parser,
                       resources::OutputFormatter& formatter) override;
};

class EmulatorWriteMemoryCommandHandler : public resources::CommandHandler {
 public:
  std::string GetName() const { return "emulator-write-memory"; }
  std::string GetDescription() const { return "Write memory to emulator"; }
  std::string GetUsage() const {
    return "emulator-write-memory --address <address> --data <data> [--format "
           "<json|text>]";
  }

  absl::Status ValidateArgs(const resources::ArgumentParser& parser) override {
    return parser.RequireArgs({"address", "data"});
  }

  absl::Status Execute(Rom* rom, const resources::ArgumentParser& parser,
                       resources::OutputFormatter& formatter) override;
};

class EmulatorGetRegistersCommandHandler : public resources::CommandHandler {
 public:
  std::string GetName() const { return "emulator-get-registers"; }
  std::string GetDescription() const { return "Get emulator register values"; }
  std::string GetUsage() const {
    return "emulator-get-registers [--format <json|text>]";
  }

  absl::Status ValidateArgs(const resources::ArgumentParser& parser) override {
    return absl::OkStatus();
  }

  absl::Status Execute(Rom* rom, const resources::ArgumentParser& parser,
                       resources::OutputFormatter& formatter) override;
};

class EmulatorGetMetricsCommandHandler : public resources::CommandHandler {
 public:
  std::string GetName() const { return "emulator-get-metrics"; }
  std::string GetDescription() const {
    return "Get emulator performance metrics";
  }
  std::string GetUsage() const {
    return "emulator-get-metrics [--format <json|text>]";
  }

  absl::Status ValidateArgs(const resources::ArgumentParser& parser) override {
    return absl::OkStatus();
  }

  absl::Status Execute(Rom* rom, const resources::ArgumentParser& parser,
                       resources::OutputFormatter& formatter) override;
};

class EmulatorPressButtonsCommandHandler : public resources::CommandHandler {
 public:
  std::string GetName() const { return "emulator-press-buttons"; }
  std::string GetDescription() const {
    return "Press and release emulator buttons";
  }
  std::string GetUsage() const {
    return "emulator-press-buttons --buttons <button1>,<button2>,...";
  }
  absl::Status ValidateArgs(const resources::ArgumentParser& parser) override {
    return parser.RequireArgs({"buttons"});
  }
  absl::Status Execute(Rom* rom, const resources::ArgumentParser& parser,
                       resources::OutputFormatter& formatter) override;
};

class EmulatorReleaseButtonsCommandHandler : public resources::CommandHandler {
 public:
  std::string GetName() const { return "emulator-release-buttons"; }
  std::string GetDescription() const {
    return "Release currently held emulator buttons";
  }
  std::string GetUsage() const {
    return "emulator-release-buttons --buttons <button1>,<button2>,...";
  }
  absl::Status ValidateArgs(const resources::ArgumentParser& parser) override {
    return parser.RequireArgs({"buttons"});
  }
  absl::Status Execute(Rom* rom, const resources::ArgumentParser& parser,
                       resources::OutputFormatter& formatter) override;
};

class EmulatorHoldButtonsCommandHandler : public resources::CommandHandler {
 public:
  std::string GetName() const { return "emulator-hold-buttons"; }
  std::string GetDescription() const {
    return "Hold emulator buttons for a specified duration";
  }
  std::string GetUsage() const {
    return "emulator-hold-buttons --buttons <button1>,<button2>,... --duration "
           "<ms>";
  }
  absl::Status ValidateArgs(const resources::ArgumentParser& parser) override {
    return parser.RequireArgs({"buttons", "duration"});
  }
  absl::Status Execute(Rom* rom, const resources::ArgumentParser& parser,
                       resources::OutputFormatter& formatter) override;
};

}  // namespace handlers
}  // namespace cli
}  // namespace yaze

#endif  // YAZE_SRC_CLI_HANDLERS_EMULATOR_COMMANDS_H_
