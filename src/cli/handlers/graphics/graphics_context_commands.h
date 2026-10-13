#ifndef YAZE_CLI_HANDLERS_GRAPHICS_GRAPHICS_CONTEXT_COMMANDS_H_
#define YAZE_CLI_HANDLERS_GRAPHICS_GRAPHICS_CONTEXT_COMMANDS_H_

#include "cli/service/resources/command_handler.h"

namespace yaze::cli {

class GraphicsContextCommandHandler : public resources::CommandHandler {
 public:
  std::string GetName() const override { return "graphics-context"; }

  std::string GetUsage() const override {
    return "graphics-context (--map <hex> | --room <hex>) [--format json|text]";
  }

  std::string GetDefaultFormat() const override { return "json"; }

  std::string GetOutputTitle() const override { return "Graphics Context"; }

  bool RequiresRom() const override { return true; }

  absl::Status ValidateArgs(const resources::ArgumentParser& parser) override;

  absl::Status Execute(Rom* rom, const resources::ArgumentParser& parser,
                       resources::OutputFormatter& formatter) override;
};

}  // namespace yaze::cli

#endif  // YAZE_CLI_HANDLERS_GRAPHICS_GRAPHICS_CONTEXT_COMMANDS_H_
