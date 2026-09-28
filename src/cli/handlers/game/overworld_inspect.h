#ifndef YAZE_CLI_HANDLERS_OVERWORLD_INSPECT_H_
#define YAZE_CLI_HANDLERS_OVERWORLD_INSPECT_H_

#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
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

// Where one tile16 of an overworld area lives. Tile commands take x/y
// relative to the top-left of the area's parent screen: 0-31 per axis for a
// small area, 0-63 on each doubled axis of a large, wide, or tall area. The
// world blockset (Overworld::GetMapTiles) is indexed [world_x][world_y] where
// each screen owns a 32x32 block at ((local % 8) * 32, (local / 8) * 32);
// world_x/world_y are also the editor canvas tile coordinates.
struct AreaTileLocation {
  int map_id = 0;        // screen the caller named
  int parent_map = 0;    // screen at the area's top-left
  int world = 0;         // 0 light, 1 dark, 2 special
  int area_width = 32;   // tile16 columns in the area
  int area_height = 32;  // tile16 rows in the area
  int area_x = 0;        // caller coordinates, relative to parent_map
  int area_y = 0;
  int screen_id = 0;  // screen that contains the tile
  int screen_x = 0;   // 0-31 within screen_id
  int screen_y = 0;
  int world_x = 0;  // index into the world blockset
  int world_y = 0;
};

// Screens spanned by an area: {columns, rows}, each 1 or 2.
std::pair<int, int> AreaScreenSpan(const zelda3::OverworldMap& parent_map);

// Pure address math, independent of ROM loading. Fails when the parent is in
// another world, the named screen is outside the parent's span, or x/y fall
// outside the area.
absl::StatusOr<AreaTileLocation> ResolveAreaTileLocation(
    int map_id, int parent_map, int area_columns, int area_rows, int x, int y);

// Resolves the parent and span of map_id from a loaded overworld.
absl::StatusOr<AreaTileLocation> ResolveAreaTileForMap(
    const zelda3::Overworld& overworld, int map_id, int x, int y);

// Converts a screen-local tile (0-31) to its area-relative location.
absl::StatusOr<AreaTileLocation> LocateScreenTile(
    const zelda3::Overworld& overworld, int screen_id, int screen_x,
    int screen_y);

absl::StatusOr<uint16_t> ReadAreaTile(zelda3::Overworld& overworld,
                                      const AreaTileLocation& location);
absl::Status WriteAreaTile(zelda3::Overworld& overworld,
                           const AreaTileLocation& location, uint16_t tile_id);

struct OverworldSprite {
  uint8_t sprite_id;
  int map_id;
  int world;
  int x;
  int y;
  std::optional<std::string> sprite_name;
  // Game-state sprite list this entry came from (0 = beginning, 1 = first
  // part, 2 = second part) and its index within that map's list.
  int phase = 0;
  int list_index = 0;
  // Raw 16px tile coordinates within the parent area (list byte low 6 bits).
  int local_x = 0;
  int local_y = 0;
};

// Game-state (phase) names used by overworld sprite commands.
const char* SpritePhaseName(int phase);

struct SpriteQuery {
  std::optional<int> map_id;
  std::optional<int> world;
  std::optional<uint8_t> sprite_id;
  std::optional<int> phase;
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
