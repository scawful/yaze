#ifndef YAZE_SRC_CLI_HANDLERS_OVERWORLD_COMMANDS_H_
#define YAZE_SRC_CLI_HANDLERS_OVERWORLD_COMMANDS_H_

#include "cli/service/resources/command_handler.h"

namespace yaze {
namespace cli {
namespace handlers {

class OverworldGetTileCommandHandler : public resources::CommandHandler {
 public:
  std::string GetName() const { return "overworld-get-tile"; }
  std::string GetDescription() const {
    return "Read the tile16 id at an overworld area position. --map is a hex "
           "screen id; x/y are decimal (or 0x-hex) tile16 coordinates "
           "relative to the top-left of the screen's parent area: 0-31 for a "
           "small area, 0-63 on each doubled axis of a large/wide/tall area";
  }
  std::string GetUsage() const {
    return "overworld-get-tile --map <hex> --x <area_tile_x> --y <area_tile_y> "
           "[--format <json|text>]";
  }

  absl::Status ValidateArgs(const resources::ArgumentParser& parser) override {
    return parser.RequireArgs({"map", "x", "y"});
  }

  absl::Status Execute(Rom* rom, const resources::ArgumentParser& parser,
                       resources::OutputFormatter& formatter) override;
};

class OverworldInspectSpecialEntrancesCommandHandler
    : public resources::CommandHandler {
 public:
  std::string GetName() const { return "overworld-inspect-special-entrances"; }
  std::string GetDescription() const {
    return "Inspect verified four-row special-area tile triggers in the loaded "
           "ROM";
  }
  std::string GetUsage() const {
    return "overworld-inspect-special-entrances --map <hex> [--format "
           "<json|text>]";
  }
  absl::Status ValidateArgs(const resources::ArgumentParser& parser) override {
    return parser.RequireArgs({"map"});
  }
  absl::Status Execute(Rom* rom, const resources::ArgumentParser& parser,
                       resources::OutputFormatter& formatter) override;
};

class OverworldSetTileCommandHandler : public resources::CommandHandler {
 public:
  std::string GetName() const { return "overworld-set-tile"; }
  std::string GetDescription() const {
    return "Write one tile16 id at an overworld area position (dry-run by "
           "default). Coordinates match overworld-get-tile. --write rebuilds "
           "tile32/map data, verifies a fresh reload, saves with a required "
           "backup, and refuses ROMs inside an Oracle checkout unless "
           "--allow-project-rom";
  }
  std::string GetUsage() const {
    return "overworld-set-tile --map <hex> --x <area_tile_x> --y <area_tile_y> "
           "--tile <hex> [--write] [--allow-project-rom] [--format "
           "<json|text>]";
  }

  absl::Status ValidateArgs(const resources::ArgumentParser& parser) override {
    return parser.RequireArgs({"map", "x", "y", "tile"});
  }

  absl::Status Execute(Rom* rom, const resources::ArgumentParser& parser,
                       resources::OutputFormatter& formatter) override;
  absl::Status ExecuteWithContext(
      Rom* rom, const resources::ArgumentParser& parser,
      resources::OutputFormatter& formatter,
      const resources::CommandInvocationContext& invocation_context) override;
};

// Hash-bound relocation of existing live native entrance/return rows.
class OverworldApplyEndpointPlanCommandHandler
    : public resources::CommandHandler {
 public:
  std::string GetName() const override {
    return "overworld-apply-endpoint-plan";
  }
  Descriptor Describe() const override {
    Descriptor descriptor;
    descriptor.display_name = GetName();
    descriptor.summary =
        "Apply a hash-bound native endpoint plan through the model; dry-run by "
        "default";
    return descriptor;
  }
  std::string GetUsage() const override {
    return "overworld-apply-endpoint-plan --plan <json> --plan-sha256 <sha256> "
           "--output <new-rom> [--experimental-page-tile16] [--write] "
           "[--format <json|text>]";
  }
  absl::Status ValidateArgs(const resources::ArgumentParser& parser) override;
  absl::Status Execute(Rom* rom, const resources::ArgumentParser& parser,
                       resources::OutputFormatter& formatter) override;
  absl::Status ExecuteWithContext(
      Rom* rom, const resources::ArgumentParser& parser,
      resources::OutputFormatter& formatter,
      const resources::CommandInvocationContext& invocation_context) override;
};

// Imports physical pages into an already-loaded native overworld. Publication
// is deliberately separate from the caller's ROM and is exclusive-create only.
// --native-source consumes a hash-bound native-source export instead of a
// donor ROM: selected special pages' grids and page Tile16 definitions, plus
// their appearance profiles when --apply-native-profiles is set.
class OverworldImportMapsCommandHandler : public resources::CommandHandler {
 public:
  std::string GetName() const override { return "overworld-import-maps"; }
  std::string GetDescription() const {
    return "Stage selected physical map grids, referenced Tile16 definitions, "
           "ordinary graphics assignments and palette dependencies from a "
           "donor (or selected special-page grids and page Tile16 definitions "
           "from a native-source export) into a native overworld; reject "
           "foreign owners; dry-run by default";
  }
  Descriptor Describe() const override {
    Descriptor descriptor;
    descriptor.display_name = GetName();
    descriptor.summary = GetDescription();
    return descriptor;
  }
  std::string GetUsage() const override {
    return "overworld-import-maps (--donor <rom> | --native-source <json>) "
           "[--native-source-sha256 <sha256>] [--experimental-page-tile16] "
           "[--apply-native-profiles] "
           "--maps <hex,hex,...> --output <new-rom> [--write] "
           "[--format <json|text>]";
  }
  absl::Status ValidateArgs(const resources::ArgumentParser& parser) override;
  absl::Status Execute(Rom* rom, const resources::ArgumentParser& parser,
                       resources::OutputFormatter& formatter) override;
  absl::Status ExecuteWithContext(
      Rom* rom, const resources::ArgumentParser& parser,
      resources::OutputFormatter& formatter,
      const resources::CommandInvocationContext& invocation_context) override;
};

/**
 * @brief Command handler for finding tiles in overworld
 */
class OverworldFindTileCommandHandler : public resources::CommandHandler {
 public:
  std::string GetName() const { return "overworld-find-tile"; }
  std::string GetDescription() const {
    return "Find tiles by ID in overworld maps";
  }
  std::string GetUsage() const {
    return "overworld-find-tile --tile <hex> [--map <hex>] "
           "[--world <light|dark|special>] [--format <json|text>]";
  }

  absl::Status ValidateArgs(const resources::ArgumentParser& parser) override {
    return parser.RequireArgs({"tile"});
  }

  absl::Status Execute(Rom* rom, const resources::ArgumentParser& parser,
                       resources::OutputFormatter& formatter) override;
};

/**
 * @brief Command handler for describing overworld maps
 */
class OverworldDescribeMapCommandHandler : public resources::CommandHandler {
 public:
  std::string GetName() const { return "overworld-describe-map"; }
  std::string GetDescription() const {
    return "Get detailed description of an overworld map";
  }
  std::string GetUsage() const {
    return "overworld-describe-map --screen <hex> [--format <json|text>]";
  }

  absl::Status ValidateArgs(const resources::ArgumentParser& parser) override {
    return parser.RequireArgs({"screen"});
  }

  absl::Status Execute(Rom* rom, const resources::ArgumentParser& parser,
                       resources::OutputFormatter& formatter) override;
};

/**
 * @brief Command handler for listing warps in overworld
 */
class OverworldListWarpsCommandHandler : public resources::CommandHandler {
 public:
  std::string GetName() const { return "overworld-list-warps"; }
  std::string GetDescription() const {
    return "List all warps in overworld maps";
  }
  std::string GetUsage() const {
    return "overworld-list-warps [--screen <screen_id>] [--format <json|text>]";
  }

  absl::Status ValidateArgs(const resources::ArgumentParser& parser) override {
    return absl::OkStatus();  // No required args
  }

  absl::Status Execute(Rom* rom, const resources::ArgumentParser& parser,
                       resources::OutputFormatter& formatter) override;
};

/**
 * @brief Command handler for listing sprites in overworld
 */
class OverworldListSpritesCommandHandler : public resources::CommandHandler {
 public:
  std::string GetName() const { return "overworld-list-sprites"; }
  std::string GetDescription() const {
    return "List all sprites in overworld maps";
  }
  std::string GetUsage() const {
    return "overworld-list-sprites [--screen <screen_id>] [--phase <0|1|2>] "
           "[--format <json|text>]";
  }

  absl::Status ValidateArgs(const resources::ArgumentParser& parser) override {
    return absl::OkStatus();  // No required args
  }

  absl::Status Execute(Rom* rom, const resources::ArgumentParser& parser,
                       resources::OutputFormatter& formatter) override;
};

/**
 * @brief Command handler for listing items in overworld
 */
class OverworldListItemsCommandHandler : public resources::CommandHandler {
 public:
  std::string GetName() const { return "overworld-list-items"; }
  std::string GetDescription() const {
    return "List all hidden item placements in overworld maps";
  }
  std::string GetUsage() const {
    return "overworld-list-items [--screen <screen_id>] [--format "
           "<json|text>]";
  }

  absl::Status ValidateArgs(const resources::ArgumentParser& parser) override {
    return absl::OkStatus();  // No required args
  }

  absl::Status Execute(Rom* rom, const resources::ArgumentParser& parser,
                       resources::OutputFormatter& formatter) override;
};

/**
 * @brief Command handler for getting entrance information
 */
class OverworldGetEntranceCommandHandler : public resources::CommandHandler {
 public:
  std::string GetName() const { return "overworld-get-entrance"; }
  std::string GetDescription() const {
    return "Get entrance information from overworld";
  }
  std::string GetUsage() const {
    return "overworld-get-entrance --entrance <entrance_id> "
           "[--experimental-page-tile16] [--format <json|text>]";
  }

  absl::Status ValidateArgs(const resources::ArgumentParser& parser) override {
    return parser.RequireArgs({"entrance"});
  }

  absl::Status Execute(Rom* rom, const resources::ArgumentParser& parser,
                       resources::OutputFormatter& formatter) override;
};

/**
 * @brief Command handler for getting tile statistics
 */
class OverworldTileStatsCommandHandler : public resources::CommandHandler {
 public:
  std::string GetName() const { return "overworld-tile-stats"; }
  std::string GetDescription() const {
    return "Count tile16 usage per screen, per world and per tile16 id";
  }
  std::string GetUsage() const {
    return "overworld-tile-stats [--screen <hex>|--map <hex>] "
           "[--world <light|dark|special>] [--tile <hex> [--details]] "
           "[--top <n>] [--format <json|text>]";
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

#endif  // YAZE_SRC_CLI_HANDLERS_OVERWORLD_COMMANDS_H_
