#ifndef YAZE_SRC_CLI_HANDLERS_GAME_OVERWORLD_TILE_REFERENCE_H_
#define YAZE_SRC_CLI_HANDLERS_GAME_OVERWORLD_TILE_REFERENCE_H_

#include <array>
#include <cstdint>
#include <functional>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "cli/handlers/game/overworld_inspect.h"
#include "cli/service/resources/command_handler.h"
#include "zelda3/overworld/overworld.h"

namespace yaze {
namespace cli {
namespace overworld {

// Status vocabulary matches coast source_reference_policy / CLIFF_ROLE_USAGE.
enum class TileReferenceStatus {
  kObserved,
  kVerifiedValid,
  kConfirmedError,
  kDeliberateException,
  kUnresolved,
};

const char* TileReferenceStatusName(TileReferenceStatus status);

struct TileReferenceRuleCell {
  int x = 0;
  int y = 0;
  uint16_t tile16 = 0;
};

struct TileReferenceRule {
  std::string id;
  TileReferenceStatus classification = TileReferenceStatus::kUnresolved;
  int map = 0;
  std::vector<TileReferenceRuleCell> before;
  std::vector<TileReferenceRuleCell> after;
  // Exact Tile16 -> four SNES words (TL,TR,BL,BR). Empty = do not check words.
  std::vector<std::pair<uint16_t, std::array<uint16_t, 4>>> definitions;
  std::vector<TileReferenceRuleCell> preserve;
  std::string reason;
  std::string user_ruling;
};

struct TileReferencePolicy {
  std::vector<TileReferenceRule> rules;
};

absl::StatusOr<TileReferencePolicy> LoadTileReferencePolicy(
    const std::string& path);

struct TileNeighborCell {
  int dx = 0;
  int dy = 0;
  bool in_bounds = false;
  std::optional<uint16_t> tile16;
};

struct TileReferenceMatch {
  TileReferenceStatus status = TileReferenceStatus::kObserved;
  std::string reason;
  std::string rule_id;
  std::string user_ruling;
};

// Classify one occurrence against policy without inventing roles. Absent a
// covering rule the status stays observed. Mirrors
// source_reference_policy.reference_exclusions for the selected cell only.
absl::StatusOr<TileReferenceMatch> ClassifyTileReferenceOccurrence(
    const TileReferencePolicy& policy, int map_id, int x, int y,
    const std::function<absl::StatusOr<uint16_t>(int, int)>& read_cell,
    const std::function<absl::StatusOr<std::array<uint16_t, 4>>(uint16_t)>&
        read_definition);

struct TileReferenceInspection {
  int map_id = 0;
  int x = 0;
  int y = 0;
  uint16_t tile16 = 0;
  AreaTileLocation location{};
  std::array<Tile16QuadrantDetail, 4> quadrants{};
  std::vector<TileNeighborCell> neighbors;  // 3x3, center included
  TileReferenceStatus status = TileReferenceStatus::kObserved;
  std::string reason;
  std::string rule_id;
  std::string user_ruling;
};

// Read-only occurrence inspector. No classifier clone, repair, or blanking.
absl::StatusOr<TileReferenceInspection> InspectTile16Reference(
    zelda3::Overworld& overworld, int map_id, int x, int y,
    const TileReferencePolicy* policy);

}  // namespace overworld

namespace handlers {

class OverworldTileReferenceInspectCommandHandler
    : public resources::CommandHandler {
 public:
  std::string GetName() const override {
    return "overworld-tile-reference-inspect";
  }
  std::string GetDescription() const {
    return "Inspect one overworld Tile16 occurrence with neighbor context, "
           "quadrant words/flips/palette/collision, and optional "
           "source-reference policy status (no classifier or repair)";
  }
  std::string GetUsage() const override {
    return "overworld-tile-reference-inspect --map <hex> --x <area_tile_x> "
           "--y <area_tile_y> [--reference-policy <path.json>] "
           "[--allow-lyra-layout-writes|--experimental-page-tile16] [--format "
           "<json|text>]";
  }
  bool AcceptsCompiledBuildReceipt() const override { return true; }
  absl::Status ValidateArgs(const resources::ArgumentParser& parser) override {
    return parser.RequireArgs({"map", "x", "y"});
  }
  absl::Status Execute(Rom* rom, const resources::ArgumentParser& parser,
                       resources::OutputFormatter& formatter) override;
};

}  // namespace handlers
}  // namespace cli
}  // namespace yaze

#endif  // YAZE_SRC_CLI_HANDLERS_GAME_OVERWORLD_TILE_REFERENCE_H_
