#ifndef YAZE_SRC_CLI_HANDLERS_PROJECT_COMMANDS_H_
#define YAZE_SRC_CLI_HANDLERS_PROJECT_COMMANDS_H_

#include "cli/service/resources/command_handler.h"

namespace yaze {
namespace cli {
namespace handlers {

/**
 * @brief Command handler for initializing new projects
 */
class ProjectInitCommandHandler : public resources::CommandHandler {
 public:
  std::string GetName() const { return "project-init"; }
  std::string GetDescription() const { return "Initialize a new Yaze project"; }
  std::string GetUsage() const { return "project-init --project_name <name>"; }

  absl::Status ValidateArgs(const resources::ArgumentParser& parser) override {
    return parser.RequireArgs({"project_name"});
  }

  absl::Status Execute(Rom* rom, const resources::ArgumentParser& parser,
                      resources::OutputFormatter& formatter) override;
};

/**
 * @brief Command handler for building projects
 */
class ProjectBuildCommandHandler : public resources::CommandHandler {
 public:
  std::string GetName() const { return "project-build"; }
  std::string GetDescription() const { return "Build a Yaze project"; }
  std::string GetUsage() const { return "project-build"; }

  absl::Status ValidateArgs(const resources::ArgumentParser& parser) override {
    return absl::OkStatus();
  }

  absl::Status Execute(Rom* rom, const resources::ArgumentParser& parser,
                      resources::OutputFormatter& formatter) override;
};

}  // namespace handlers
}  // namespace cli
}  // namespace yaze

#endif  // YAZE_SRC_CLI_HANDLERS_PROJECT_COMMANDS_H_
