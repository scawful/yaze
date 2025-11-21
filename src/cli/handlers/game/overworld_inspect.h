#ifndef YAZE_CLI_HANDLERS_OVERWORLD_INSPECT_H_
#define YAZE_CLI_HANDLERS_OVERWORLD_INSPECT_H_

#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include "absl/status/statusor.h"

namespace yaze {
class Rom;

namespace zelda3 {
class Overworld;
class OverworldEntrance;
class OverworldExit;
class OverworldMap;
}  // namespace zelda3

namespace cli {
namespace overworld {

enum class WarpType {
  kEntrance,
  kHole,
  kExit,
};

struct MapSummary {
  int map_id;
  int world;
  int local_index;
  int map_x;
  int map_y;
  bool is_large_map;
  int parent_map;
  int large_quadrant;
  std::string area_size;
  uint16_t message_id;
  uint8_t area_graphics;
  uint8_t area_palette;
  uint8_t main_palette;
  uint8_t animated_gfx;
  uint16_t subscreen_overlay;
  uint16_t area_specific_bg_color;
  std::vector<uint8_t> sprite_graphics;
  std::vector<uint8_t> sprite_palettes;
  std::vector<uint8_t> area_music;
  std::vector<uint8_t> static_graphics;
  bool has_overlay;
  uint16_t overlay_id;
};

struct WarpEntry {
  WarpType type;
  uint16_t raw_map_id;
  int map_id;
  int world;
  int local_index;
  int map_x;
  int map_y;
  int tile16_x;
  int tile16_y;
  int pixel_x;
  int pixel_y;
  uint16_t map_pos;
  bool deleted;
  bool is_hole;
  std::optional<uint8_t> entrance_id;
  std::optional<std::string> entrance_name;
  std::optional<uint16_t> room_id;
  std::optional<uint16_t> door_type_1;
  std::optional<uint16_t> door_type_2;
};

struct WarpQuery {
  std::optional<int> world;
  std::optional<int> map_id;
  std::optional<WarpType> type;
};

struct TileMatch {
  int map_id;
  int world;
  int local_x;
  int local_y;
  int global_x;
  int global_y;
};

struct TileSearchOptions {
  std::optional<int> map_id;
  std::optional<int> world;
};

struct OverworldSprite {
  uint8_t sprite_id;
  int map_id;
  int world;
  int x;
  int y;
  std::optional<std::string> sprite_name;
};

struct SpriteQuery {
  std::optional<int> map_id;
  std::optional<int> world;
  std::optional<uint8_t> sprite_id;
};

struct EntranceDetails {
  uint8_t entrance_id;
  int map_id;
  int world;
  int x;
  int y;
  uint8_t area_x;
  uint8_t area_y;
  uint16_t map_pos;
  bool is_hole;
  std::optional<std::string> entrance_name;
};

struct TileStatistics {
  int map_id;
  int world;
  uint16_t tile_id;
  int count;
  std::vector<std::pair<int, int>> positions;  // (x, y) positions
};

absl::StatusOr<int> ParseNumeric(std::string_view value, int base = 0);
absl::StatusOr<int> ParseWorldSpecifier(std::string_view value);
absl::StatusOr<int> InferWorldFromMapId(int map_id);
std::string WorldName(int world);
std::string WarpTypeName(WarpType type);

absl::StatusOr<MapSummary> BuildMapSummary(zelda3::Overworld& overworld,
                                           int map_id);

absl::StatusOr<std::vector<WarpEntry>> CollectWarpEntries(
    const zelda3::Overworld& overworld, const WarpQuery& query);

absl::StatusOr<std::vector<TileMatch>> FindTileMatches(
    zelda3::Overworld& overworld, uint16_t tile_id,
    const TileSearchOptions& options = {});

absl::StatusOr<std::vector<OverworldSprite>> CollectOverworldSprites(
    const zelda3::Overworld& overworld, const SpriteQuery& query);

absl::StatusOr<EntranceDetails> GetEntranceDetails(
    const zelda3::Overworld& overworld, uint8_t entrance_id);

absl::StatusOr<TileStatistics> AnalyzeTileUsage(
    zelda3::Overworld& overworld, uint16_t tile_id,
    const TileSearchOptions& options = {});

}  // namespace overworld
}  // namespace cli
}  // namespace yaze

#endif  // YAZE_CLI_HANDLERS_OVERWORLD_INSPECT_H_
