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
}

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

absl::StatusOr<int> ParseNumeric(std::string_view value, int base = 0);
absl::StatusOr<int> ParseWorldSpecifier(std::string_view value);
absl::StatusOr<int> InferWorldFromMapId(int map_id);
std::string WorldName(int world);
std::string WarpTypeName(WarpType type);

absl::StatusOr<MapSummary> BuildMapSummary(zelda3::Overworld& overworld,
                                           int map_id);

absl::StatusOr<std::vector<WarpEntry>> CollectWarpEntries(
  const zelda3::Overworld& overworld, const WarpQuery& query);

}  // namespace overworld
}  // namespace cli
}  // namespace yaze

#endif  // YAZE_CLI_HANDLERS_OVERWORLD_INSPECT_H_
