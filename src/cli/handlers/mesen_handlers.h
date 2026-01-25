#ifndef YAZE_CLI_HANDLERS_MESEN_HANDLERS_H_
#define YAZE_CLI_HANDLERS_MESEN_HANDLERS_H_

#include <string>
#include <vector>

#include "cli/service/resources/command_handler.h"

namespace yaze {
namespace cli {
namespace handlers {

// Handler implementations for Mesen2 commands

class MesenGamestateCommandHandler : public resources::CommandHandler {
 public:
  std::string GetName() const override { return "mesen-gamestate"; }
  std::string GetDescription() const {
    return "Get ALTTP game state from running Mesen2";
  }
  std::string GetUsage() const override {
    return "mesen-gamestate [--format <json|text>]";
  }
  bool RequiresRom() const override { return false; }
  absl::Status ValidateArgs(const resources::ArgumentParser& parser) override;
  absl::Status Execute(Rom* rom, const resources::ArgumentParser& parser,
                       resources::OutputFormatter& formatter) override;
};

class MesenSpritesCommandHandler : public resources::CommandHandler {
 public:
  std::string GetName() const override { return "mesen-sprites"; }
  std::string GetDescription() const {
    return "Get active sprites from running Mesen2";
  }
  std::string GetUsage() const override {
    return "mesen-sprites [--all] [--format <json|text>]";
  }
  bool RequiresRom() const override { return false; }
  absl::Status ValidateArgs(const resources::ArgumentParser& parser) override;
  absl::Status Execute(Rom* rom, const resources::ArgumentParser& parser,
                       resources::OutputFormatter& formatter) override;
};

class MesenCpuCommandHandler : public resources::CommandHandler {
 public:
  std::string GetName() const override { return "mesen-cpu"; }
  std::string GetDescription() const {
    return "Get CPU register state from Mesen2";
  }
  std::string GetUsage() const override {
    return "mesen-cpu [--format <json|text>]";
  }
  bool RequiresRom() const override { return false; }
  absl::Status ValidateArgs(const resources::ArgumentParser& parser) override;
  absl::Status Execute(Rom* rom, const resources::ArgumentParser& parser,
                       resources::OutputFormatter& formatter) override;
};

class MesenMemoryReadCommandHandler : public resources::CommandHandler {
 public:
  std::string GetName() const override { return "mesen-memory-read"; }
  std::string GetDescription() const {
    return "Read memory from Mesen2 emulator";
  }
  std::string GetUsage() const override {
    return "mesen-memory-read --address <hex> [--length <n>] [--format <json|text>]";
  }
  bool RequiresRom() const override { return false; }
  absl::Status ValidateArgs(const resources::ArgumentParser& parser) override;
  absl::Status Execute(Rom* rom, const resources::ArgumentParser& parser,
                       resources::OutputFormatter& formatter) override;
};

class MesenMemoryWriteCommandHandler : public resources::CommandHandler {
 public:
  std::string GetName() const override { return "mesen-memory-write"; }
  std::string GetDescription() const {
    return "Write memory in Mesen2 emulator";
  }
  std::string GetUsage() const override {
    return "mesen-memory-write --address <hex> --data <hexbytes> [--format <json|text>]";
  }
  bool RequiresRom() const override { return false; }
  absl::Status ValidateArgs(const resources::ArgumentParser& parser) override;
  absl::Status Execute(Rom* rom, const resources::ArgumentParser& parser,
                       resources::OutputFormatter& formatter) override;
};

class MesenDisasmCommandHandler : public resources::CommandHandler {
 public:
  std::string GetName() const override { return "mesen-disasm"; }
  std::string GetDescription() const {
    return "Disassemble code at address in Mesen2";
  }
  std::string GetUsage() const override {
    return "mesen-disasm --address <hex> [--count <n>] [--format <json|text>]";
  }
  bool RequiresRom() const override { return false; }
  absl::Status ValidateArgs(const resources::ArgumentParser& parser) override;
  absl::Status Execute(Rom* rom, const resources::ArgumentParser& parser,
                       resources::OutputFormatter& formatter) override;
};

class MesenTraceCommandHandler : public resources::CommandHandler {
 public:
  std::string GetName() const override { return "mesen-trace"; }
  std::string GetDescription() const {
    return "Get execution trace from Mesen2";
  }
  std::string GetUsage() const override {
    return "mesen-trace [--count <n>] [--format <json|text>]";
  }
  bool RequiresRom() const override { return false; }
  absl::Status ValidateArgs(const resources::ArgumentParser& parser) override;
  absl::Status Execute(Rom* rom, const resources::ArgumentParser& parser,
                       resources::OutputFormatter& formatter) override;
};

class MesenBreakpointCommandHandler : public resources::CommandHandler {
 public:
  std::string GetName() const override { return "mesen-breakpoint"; }
  std::string GetDescription() const {
    return "Manage breakpoints in Mesen2";
  }
  std::string GetUsage() const override {
    return "mesen-breakpoint --action <add|remove|clear|list> [--address <hex>] [--id <n>] [--type <exec|read|write|rw>]";
  }
  bool RequiresRom() const override { return false; }
  absl::Status ValidateArgs(const resources::ArgumentParser& parser) override;
  absl::Status Execute(Rom* rom, const resources::ArgumentParser& parser,
                       resources::OutputFormatter& formatter) override;
};

class MesenControlCommandHandler : public resources::CommandHandler {
 public:
  std::string GetName() const override { return "mesen-control"; }
  std::string GetDescription() const {
    return "Control Mesen2 emulation state";
  }
  std::string GetUsage() const override {
    return "mesen-control --action <pause|resume|step|frame|reset>";
  }
  bool RequiresRom() const override { return false; }
  absl::Status ValidateArgs(const resources::ArgumentParser& parser) override;
  absl::Status Execute(Rom* rom, const resources::ArgumentParser& parser,
                       resources::OutputFormatter& formatter) override;
};

/**
 * @brief Factory function to create Mesen2 command handlers
 */
std::vector<std::unique_ptr<resources::CommandHandler>>
CreateMesenCommandHandlers();

}  // namespace handlers
}  // namespace cli
}  // namespace yaze

#endif  // YAZE_CLI_HANDLERS_MESEN_HANDLERS_H_
