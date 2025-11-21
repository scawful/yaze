#ifndef YAZE_SRC_CLI_HANDLERS_MUSIC_COMMANDS_H_
#define YAZE_SRC_CLI_HANDLERS_MUSIC_COMMANDS_H_

#include "cli/service/resources/command_handler.h"

namespace yaze {
namespace cli {
namespace handlers {

/**
 * @brief Command handler for listing music tracks
 */
class MusicListCommandHandler : public resources::CommandHandler {
 public:
  std::string GetName() const { return "music-list"; }
  std::string GetDescription() const { return "List available music tracks"; }
  std::string GetUsage() const { return "music-list [--format <json|text>]"; }

  absl::Status ValidateArgs(const resources::ArgumentParser& parser) override {
    return absl::OkStatus();  // No required args
  }

  absl::Status Execute(Rom* rom, const resources::ArgumentParser& parser,
                       resources::OutputFormatter& formatter) override;
};

/**
 * @brief Command handler for getting music track information
 */
class MusicInfoCommandHandler : public resources::CommandHandler {
 public:
  std::string GetName() const { return "music-info"; }
  std::string GetDescription() const {
    return "Get information about a specific music track";
  }
  std::string GetUsage() const {
    return "music-info --id <track_id> [--format <json|text>]";
  }

  absl::Status ValidateArgs(const resources::ArgumentParser& parser) override {
    return parser.RequireArgs({"id"});
  }

  absl::Status Execute(Rom* rom, const resources::ArgumentParser& parser,
                       resources::OutputFormatter& formatter) override;
};

/**
 * @brief Command handler for getting detailed music track data
 */
class MusicTracksCommandHandler : public resources::CommandHandler {
 public:
  std::string GetName() const { return "music-tracks"; }
  std::string GetDescription() const {
    return "Get detailed track data for music";
  }
  std::string GetUsage() const {
    return "music-tracks [--category <category>] [--format <json|text>]";
  }

  absl::Status ValidateArgs(const resources::ArgumentParser& parser) override {
    return absl::OkStatus();  // No required args
  }

  absl::Status Execute(Rom* rom, const resources::ArgumentParser& parser,
                       resources::OutputFormatter& formatter) override;
};

}  // namespace handlers
}  // namespace cli
}  // namespace yaze

#endif  // YAZE_SRC_CLI_HANDLERS_MUSIC_COMMANDS_H_
