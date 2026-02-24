#ifndef YAZE_SRC_CLI_HANDLERS_ROM_PROJECT_BUNDLE_ARCHIVE_COMMANDS_H_
#define YAZE_SRC_CLI_HANDLERS_ROM_PROJECT_BUNDLE_ARCHIVE_COMMANDS_H_

#include "cli/service/resources/command_handler.h"

namespace yaze::cli::handlers {

// Pack a .yazeproj bundle directory into a .zip archive for cross-platform
// sharing. Preserves the bundle root folder name inside the archive.
//
// Exit 0 on success, 1 on validation failure or write error.
class ProjectBundlePackCommandHandler : public resources::CommandHandler {
 public:
  std::string GetName() const override { return "project-bundle-pack"; }
  std::string GetUsage() const override {
    return "project-bundle-pack --project <path.yazeproj> --out <archive.zip> "
           "[--overwrite] [--format <json|text>]";
  }
  bool RequiresRom() const override { return false; }
  Descriptor Describe() const override;
  absl::Status ValidateArgs(
      const resources::ArgumentParser& parser) override;
  absl::Status Execute(Rom* rom, const resources::ArgumentParser& parser,
                       resources::OutputFormatter& formatter) override;
};

// Unpack a .zip archive into a .yazeproj bundle directory. Enforces path
// traversal safety (rejects ".." and absolute-path entries).
//
// Exit 0 on success, 1 on validation failure or extraction error.
class ProjectBundleUnpackCommandHandler : public resources::CommandHandler {
 public:
  std::string GetName() const override { return "project-bundle-unpack"; }
  std::string GetUsage() const override {
    return "project-bundle-unpack --archive <archive.zip> "
           "--out <directory> [--overwrite] [--dry-run] "
           "[--keep-partial-output] [--format <json|text>]";
  }
  bool RequiresRom() const override { return false; }
  Descriptor Describe() const override;
  absl::Status ValidateArgs(
      const resources::ArgumentParser& parser) override;
  absl::Status Execute(Rom* rom, const resources::ArgumentParser& parser,
                       resources::OutputFormatter& formatter) override;
};

}  // namespace yaze::cli::handlers

#endif  // YAZE_SRC_CLI_HANDLERS_ROM_PROJECT_BUNDLE_ARCHIVE_COMMANDS_H_
