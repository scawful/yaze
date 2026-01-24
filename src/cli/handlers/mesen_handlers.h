#ifndef YAZE_CLI_HANDLERS_MESEN_HANDLERS_H_
#define YAZE_CLI_HANDLERS_MESEN_HANDLERS_H_

#include <memory>
#include <string>
#include <vector>

#include "app/emu/mesen/mesen_socket_client.h"
#include "cli/service/resources/command_handler.h"

namespace yaze {
namespace cli {
namespace handlers {

/**
 * @brief Shared Mesen2 socket client for all mesen-* handlers
 */
class MesenClientHolder {
 public:
  static std::shared_ptr<emu::mesen::MesenSocketClient>& GetClient();
  static void SetClient(std::shared_ptr<emu::mesen::MesenSocketClient> client);
};

// Handler implementations for Mesen2 commands

class MesenGamestateCommandHandler : public resources::CommandHandler {
 public:
  std::string name() const override { return "mesen-gamestate"; }
  std::string description() const override {
    return "Get ALTTP game state from running Mesen2";
  }
  absl::Status Execute(const std::vector<std::string>& args,
                       std::string* output) override;
};

class MesenSpritesCommandHandler : public resources::CommandHandler {
 public:
  std::string name() const override { return "mesen-sprites"; }
  std::string description() const override {
    return "Get active sprites from running Mesen2";
  }
  absl::Status Execute(const std::vector<std::string>& args,
                       std::string* output) override;
};

class MesenCpuCommandHandler : public resources::CommandHandler {
 public:
  std::string name() const override { return "mesen-cpu"; }
  std::string description() const override {
    return "Get CPU register state from Mesen2";
  }
  absl::Status Execute(const std::vector<std::string>& args,
                       std::string* output) override;
};

class MesenMemoryReadCommandHandler : public resources::CommandHandler {
 public:
  std::string name() const override { return "mesen-memory-read"; }
  std::string description() const override {
    return "Read memory from Mesen2 emulator";
  }
  absl::Status Execute(const std::vector<std::string>& args,
                       std::string* output) override;
};

class MesenMemoryWriteCommandHandler : public resources::CommandHandler {
 public:
  std::string name() const override { return "mesen-memory-write"; }
  std::string description() const override {
    return "Write memory in Mesen2 emulator";
  }
  absl::Status Execute(const std::vector<std::string>& args,
                       std::string* output) override;
};

class MesenDisasmCommandHandler : public resources::CommandHandler {
 public:
  std::string name() const override { return "mesen-disasm"; }
  std::string description() const override {
    return "Disassemble code at address in Mesen2";
  }
  absl::Status Execute(const std::vector<std::string>& args,
                       std::string* output) override;
};

class MesenTraceCommandHandler : public resources::CommandHandler {
 public:
  std::string name() const override { return "mesen-trace"; }
  std::string description() const override {
    return "Get execution trace from Mesen2";
  }
  absl::Status Execute(const std::vector<std::string>& args,
                       std::string* output) override;
};

class MesenBreakpointCommandHandler : public resources::CommandHandler {
 public:
  std::string name() const override { return "mesen-breakpoint"; }
  std::string description() const override {
    return "Manage breakpoints in Mesen2";
  }
  absl::Status Execute(const std::vector<std::string>& args,
                       std::string* output) override;
};

class MesenControlCommandHandler : public resources::CommandHandler {
 public:
  std::string name() const override { return "mesen-control"; }
  std::string description() const override {
    return "Control Mesen2 emulation state";
  }
  absl::Status Execute(const std::vector<std::string>& args,
                       std::string* output) override;
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
