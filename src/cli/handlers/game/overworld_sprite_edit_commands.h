#ifndef YAZE_SRC_CLI_HANDLERS_GAME_OVERWORLD_SPRITE_EDIT_COMMANDS_H_
#define YAZE_SRC_CLI_HANDLERS_GAME_OVERWORLD_SPRITE_EDIT_COMMANDS_H_

#include <filesystem>

#include "cli/service/resources/command_handler.h"

namespace yaze {
namespace cli {
namespace handlers {

enum class OverworldSpriteEditAction { kAdd, kMove, kRemove };

// True when `rom_path` is a ROM inside an Oracle of Secrets source checkout
// (<checkout>/Roms/<rom> next to <checkout>/Oracle_main.asm). Overworld sprite
// writes refuse such ROMs unless --allow-project-rom is passed.
bool IsOracleProjectRomPath(const std::filesystem::path& rom_path);

// Shared implementation for overworld-add-sprite / -move-sprite /
// -remove-sprite. Dry-run by default: prints the list before/after, the
// minimal-diff strategy, and every PC/SNES byte range the write would change.
// --write applies through the fenced sprite writer, saves with a required
// backup, then reopens the saved file and verifies the change.
class OverworldSpriteEditCommandHandler : public resources::CommandHandler {
 public:
  explicit OverworldSpriteEditCommandHandler(OverworldSpriteEditAction action)
      : action_(action) {}

  std::string GetName() const override;
  std::string GetDescription() const;
  std::string GetUsage() const override;

  absl::Status ValidateArgs(const resources::ArgumentParser& parser) override;
  absl::Status Execute(Rom* rom, const resources::ArgumentParser& parser,
                       resources::OutputFormatter& formatter) override;
  absl::Status ExecuteWithContext(
      Rom* rom, const resources::ArgumentParser& parser,
      resources::OutputFormatter& formatter,
      const resources::CommandInvocationContext& invocation_context) override;

 private:
  std::string GetDefaultFormat() const override { return "json"; }
  std::string GetOutputTitle() const override {
    return "Overworld Sprite Edit";
  }
  OverworldSpriteEditAction action_;
};

class OverworldAddSpriteCommandHandler
    : public OverworldSpriteEditCommandHandler {
 public:
  OverworldAddSpriteCommandHandler()
      : OverworldSpriteEditCommandHandler(OverworldSpriteEditAction::kAdd) {}
};
class OverworldMoveSpriteCommandHandler
    : public OverworldSpriteEditCommandHandler {
 public:
  OverworldMoveSpriteCommandHandler()
      : OverworldSpriteEditCommandHandler(OverworldSpriteEditAction::kMove) {}
};
class OverworldRemoveSpriteCommandHandler
    : public OverworldSpriteEditCommandHandler {
 public:
  OverworldRemoveSpriteCommandHandler()
      : OverworldSpriteEditCommandHandler(OverworldSpriteEditAction::kRemove) {}
};

}  // namespace handlers
}  // namespace cli
}  // namespace yaze

#endif  // YAZE_SRC_CLI_HANDLERS_GAME_OVERWORLD_SPRITE_EDIT_COMMANDS_H_
