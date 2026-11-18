#ifndef YAZE_SRC_CLI_HANDLERS_DUNGEON_MAP_COMMANDS_H_
#define YAZE_SRC_CLI_HANDLERS_DUNGEON_MAP_COMMANDS_H_

#include <string>

#include "absl/status/status.h"
#include "cli/service/resources/command_handler.h"

namespace yaze {
namespace cli {
namespace handlers {

/**
 * @brief Command handler for ASCII room visualization
 *
 * Generates an ASCII representation of a dungeon room showing
 * walls, floors, pits, doors, and other tile objects. Useful for
 * quick visual inspection of room layouts.
 */
class DungeonMapCommandHandler : public resources::CommandHandler {
 public:
  std::string GetName() const override { return "dungeon-map"; }
  std::string GetDescription() const {
    return "Generate ASCII visualization of a dungeon room";
  }
  std::string GetUsage() const override {
    return "dungeon-map --rom <path> --room <room_id> [--layer <0|1|2>] "
           "[--format <ascii|json>]";
  }

  absl::Status ValidateArgs(const resources::ArgumentParser& parser) override {
    return parser.RequireArgs({"room"});
  }

  absl::Status Execute(Rom* rom, const resources::ArgumentParser& parser,
                       resources::OutputFormatter& formatter) override;
};

/**
 * @brief Export dungeon pause-map data (bank $0A + Tile16 templates) to JSON.
 *
 * Matches yaze ScreenEditor / zelda3::LoadDungeonMaps byte-for-byte, with
 * optional PNG preview rendering and room chest marker inspection.
 */
class DungeonPauseMapExportCommandHandler : public resources::CommandHandler {
 public:
  std::string GetName() const override { return "dungeon-pause-map-export"; }
  std::string GetDescription() const {
    return "Export dungeon pause-map floor grids, templates, boss metadata, "
           "and chest markers to JSON (with optional preview PNG)";
  }
  std::string GetUsage() const override {
    return "dungeon-pause-map-export [--slot <0..13|all>] [--out <path>] "
           "[--include-templates] [--include-chests] "
           "[--render-preview <path>] [--gfx-bin <path>] "
           "[--format <json|text>]";
  }

  bool AcceptsCompiledBuildReceipt() const override { return true; }

  absl::Status ValidateArgs(const resources::ArgumentParser& parser) override;

  absl::Status Execute(Rom* rom, const resources::ArgumentParser& parser,
                       resources::OutputFormatter& formatter) override;

  absl::Status ExecuteWithContext(
      Rom* rom, const resources::ArgumentParser& parser,
      resources::OutputFormatter& formatter,
      const resources::CommandInvocationContext& invocation_context) override;
};

/**
 * @brief Validate or apply dungeon pause-map JSON specs via ScreenEditor logic.
 *
 * Dry-run by default; pass --write to mutate and save with backup, write
 * fence, transaction rollback, and cold readback verification.
 */
class DungeonPauseMapApplyCommandHandler : public resources::CommandHandler {
 public:
  std::string GetName() const override { return "dungeon-pause-map-apply"; }
  std::string GetDescription() const {
    return "Validate (dry-run by default) or apply (--write) dungeon "
           "pause-map JSON specs matching ScreenEditor serialization";
  }
  std::string GetUsage() const override {
    return "dungeon-pause-map-apply --in <path> [--slot <0..13>] [--write] "
           "[--dry-run] [--allow-template-overwrite] "
           "[--allow-lyra-layout-writes|--experimental-page-tile16] "
           "[--report <path>] [--sandbox | --mock-rom] [--format <json|text>]";
  }

  absl::Status ValidateArgs(const resources::ArgumentParser& parser) override;

  absl::Status Execute(Rom* rom, const resources::ArgumentParser& parser,
                       resources::OutputFormatter& formatter) override;

  absl::Status ExecuteWithContext(
      Rom* rom, const resources::ArgumentParser& parser,
      resources::OutputFormatter& formatter,
      const resources::CommandInvocationContext& invocation_context) override;
};

}  // namespace handlers
}  // namespace cli
}  // namespace yaze

#endif  // YAZE_SRC_CLI_HANDLERS_DUNGEON_MAP_COMMANDS_H_
