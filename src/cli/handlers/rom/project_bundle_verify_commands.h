#ifndef YAZE_SRC_CLI_HANDLERS_ROM_PROJECT_BUNDLE_VERIFY_COMMANDS_H_
#define YAZE_SRC_CLI_HANDLERS_ROM_PROJECT_BUNDLE_VERIFY_COMMANDS_H_

#include "cli/service/resources/command_handler.h"

namespace yaze::cli::handlers {

// Verify the structural integrity and portability of a .yaze project file or
// .yazeproj bundle directory.
//
// Checks performed:
//   - Path exists and is a recognized project format
//   - For .yazeproj bundles: directory structure and project.yaze presence
//   - project.yaze parses successfully
//   - Referenced paths are structurally sane (portability warnings for
//     absolute paths)
//   - ROM file referenced by project exists and is readable (if present)
//
// Exit codes:
//   0  All checks pass (warnings are allowed)
//   1  Any check failed, or invalid arguments
class ProjectBundleVerifyCommandHandler : public resources::CommandHandler {
 public:
  std::string GetName() const override { return "project-bundle-verify"; }
  std::string GetUsage() const override {
    return "project-bundle-verify --project <path> "
           "[--check-rom-hash] [--format <json|text>] [--report <path>]";
  }

  bool RequiresRom() const override { return false; }

  Descriptor Describe() const override;

  absl::Status ValidateArgs(
      const resources::ArgumentParser& parser) override;

  absl::Status Execute(Rom* rom, const resources::ArgumentParser& parser,
                       resources::OutputFormatter& formatter) override;
};

}  // namespace yaze::cli::handlers

#endif  // YAZE_SRC_CLI_HANDLERS_ROM_PROJECT_BUNDLE_VERIFY_COMMANDS_H_
