#include "cli/handlers/game/overworld_inspect.h"

#include <algorithm>
#include <map>
#include <optional>
#include <string>
#include <vector>

#include "absl/status/status.h"
#include "absl/strings/ascii.h"
#include "absl/strings/numbers.h"
#include "absl/strings/str_cat.h"
#include "absl/strings/str_format.h"
#include "util/macro.h"
#include "zelda3/common.h"
#include "zelda3/overworld/overworld.h"
#include "zelda3/overworld/overworld_entrance.h"
#include "zelda3/overworld/overworld_exit.h"
#include "zelda3/overworld/overworld_map.h"
#include "zelda3/sprite/sprite.h"

namespace yaze {
namespace cli {
namespace overworld {

namespace {

constexpr int kLightWorldOffset = 0x00;
constexpr int kDarkWorldOffset = 0x40;
constexpr int kSpecialWorldOffset = 0x80;

int NormalizeMapId(uint16_t raw_map_id) {
  return static_cast<int>(raw_map_id & 0x00FF);
}

int WorldOffset(int world) {
  switch (world) {
    case 0:
      return kLightWorldOffset;
    case 1:
      return kDarkWorldOffset;
    case 2:
      return kSpecialWorldOffset;
    default:
      return 0;
  }
}

absl::Status ValidateMapId(int map_id) {
  if (map_id < 0 || map_id >= zelda3::kNumOverworldMaps) {
    return absl::InvalidArgumentError(
        absl::StrFormat("Map ID out of range: 0x%02X", map_id));
  }
  return absl::OkStatus();
}

std::string AreaSizeToString(zelda3::AreaSizeEnum size) {
  switch (size) {
    case zelda3::AreaSizeEnum::SmallArea:
      return "Small";
    case zelda3::AreaSizeEnum::LargeArea:
      return "Large";
    case zelda3::AreaSizeEnum::WideArea:
      return "Wide";
    case zelda3::AreaSizeEnum::TallArea:
      return "Tall";
    default:
      return "Unknown";
  }
}

std::string EntranceLabel(uint8_t id) {
  constexpr size_t kEntranceCount =
      sizeof(zelda3::kEntranceNames) / sizeof(zelda3::kEntranceNames[0]);
  if (id < kEntranceCount) {
    return zelda3::kEntranceNames[id];
  }
  return absl::StrFormat("Entrance %d", id);
}

void PopulateCommonWarpFields(WarpEntry& entry, uint16_t raw_map_id,
                              uint16_t map_pos, int pixel_x, int pixel_y) {
  entry.raw_map_id = raw_map_id;
  entry.map_id = NormalizeMapId(raw_map_id);
  if (entry.map_id >= zelda3::kNumOverworldMaps) {
    // Some ROM hacks use sentinel values. Clamp to valid range for reporting
    entry.map_id %= zelda3::kNumOverworldMaps;
  }
  entry.world = (entry.map_id >= kSpecialWorldOffset)
                    ? 2
                    : (entry.map_id >= kDarkWorldOffset ? 1 : 0);
  entry.local_index = entry.map_id - WorldOffset(entry.world);
  entry.map_x = entry.local_index % 8;
  entry.map_y = entry.local_index / 8;
  entry.map_pos = map_pos;
  entry.pixel_x = pixel_x;
  entry.pixel_y = pixel_y;
  int tile_index = static_cast<int>(map_pos >> 1);
  entry.tile16_x = tile_index & 0x3F;
  entry.tile16_y = tile_index >> 6;
}

}  // namespace

absl::StatusOr<int> ParseNumeric(std::string_view value, int base) {
  try {
    size_t processed = 0;
    int result = std::stoi(std::string(value), &processed, base);
    if (processed != value.size()) {
      return absl::InvalidArgumentError(
          absl::StrCat("Invalid numeric value: ", std::string(value)));
    }
    return result;
  } catch (const std::exception&) {
    return absl::InvalidArgumentError(
        absl::StrCat("Invalid numeric value: ", std::string(value)));
  }
}

absl::StatusOr<int> ParseWorldSpecifier(std::string_view value) {
  std::string lower = absl::AsciiStrToLower(std::string(value));
  if (lower == "0" || lower == "light") {
    return 0;
  }
  if (lower == "1" || lower == "dark") {
    return 1;
  }
  if (lower == "2" || lower == "special") {
    return 2;
  }
  return absl::InvalidArgumentError(
      absl::StrCat("Unknown world value: ", std::string(value)));
}

absl::StatusOr<int> InferWorldFromMapId(int map_id) {
  RETURN_IF_ERROR(ValidateMapId(map_id));
  if (map_id < kDarkWorldOffset) {
    return 0;
  }
  if (map_id < kSpecialWorldOffset) {
    return 1;
  }
  return 2;
}

std::string WorldName(int world) {
  switch (world) {
    case 0:
      return "Light";
    case 1:
      return "Dark";
    case 2:
      return "Special";
    default:
      return absl::StrCat("Unknown(", world, ")");
  }
}

std::string WarpTypeName(WarpType type) {
  switch (type) {
    case WarpType::kEntrance:
      return "entrance";
    case WarpType::kHole:
      return "hole";
    case WarpType::kExit:
      return "exit";
    default:
      return "unknown";
  }
}

absl::StatusOr<MapSummary> BuildMapSummary(zelda3::Overworld& overworld,
                                           int map_id) {
  RETURN_IF_ERROR(ValidateMapId(map_id));
  ASSIGN_OR_RETURN(int world, InferWorldFromMapId(map_id));

  // Ensure map data is built before accessing metadata.
  RETURN_IF_ERROR(overworld.EnsureMapBuilt(map_id));

  const auto* map = overworld.overworld_map(map_id);
  if (map == nullptr) {
    return absl::InternalError(
        absl::StrFormat("Failed to retrieve overworld map 0x%02X", map_id));
  }

  MapSummary summary;
  summary.map_id = map_id;
  summary.world = world;
  summary.local_index = map_id - WorldOffset(world);
  summary.map_x = summary.local_index % 8;
  summary.map_y = summary.local_index / 8;
  summary.is_large_map = map->is_large_map();
  summary.parent_map = map->parent();
  summary.large_quadrant = map->large_index();
  summary.area_size = AreaSizeToString(map->area_size());
  summary.message_id = map->message_id();
  summary.area_graphics = map->area_graphics();
  summary.area_palette = map->area_palette();
  summary.main_palette = map->main_palette();
  summary.animated_gfx = map->animated_gfx();
  summary.subscreen_overlay = map->subscreen_overlay();
  summary.area_specific_bg_color = map->area_specific_bg_color();

  summary.sprite_graphics.clear();
  summary.sprite_palettes.clear();
  summary.area_music.clear();
  summary.static_graphics.clear();

  for (int i = 0; i < 3; ++i) {
    summary.sprite_graphics.push_back(map->sprite_graphics(i));
    summary.sprite_palettes.push_back(map->sprite_palette(i));
  }

  for (int i = 0; i < 4; ++i) {
    summary.area_music.push_back(map->area_music(i));
  }

  for (int i = 0; i < 16; ++i) {
    summary.static_graphics.push_back(map->static_graphics(i));
  }

  summary.has_overlay = map->has_overlay();
  summary.overlay_id = map->overlay_id();

  return summary;
}

absl::StatusOr<std::vector<WarpEntry>> CollectWarpEntries(
    const zelda3::Overworld& overworld, const WarpQuery& query) {
  std::vector<WarpEntry> entries;

  const auto& entrances = overworld.entrances();
  for (const auto& entrance : entrances) {
    WarpEntry entry;
    entry.type = WarpType::kEntrance;
    entry.deleted = entrance.deleted;
    entry.is_hole = entrance.is_hole_;
    entry.entrance_id = entrance.entrance_id_;
    entry.entrance_name = EntranceLabel(entrance.entrance_id_);
    PopulateCommonWarpFields(entry, entrance.map_id_, entrance.map_pos_,
                             entrance.x_, entrance.y_);

    if (query.type.has_value() && *query.type != entry.type) {
      continue;
    }
    if (query.world.has_value() && *query.world != entry.world) {
      continue;
    }
    if (query.map_id.has_value() && *query.map_id != entry.map_id) {
      continue;
    }

    entries.push_back(std::move(entry));
  }

  const auto& holes = overworld.holes();
  for (const auto& hole : holes) {
    WarpEntry entry;
    entry.type = WarpType::kHole;
    entry.deleted = false;
    entry.is_hole = true;
    entry.entrance_id = hole.entrance_id_;
    entry.entrance_name = EntranceLabel(hole.entrance_id_);
    PopulateCommonWarpFields(entry, hole.map_id_, hole.map_pos_, hole.x_,
                             hole.y_);

    if (query.type.has_value() && *query.type != entry.type) {
      continue;
    }
    if (query.world.has_value() && *query.world != entry.world) {
      continue;
    }
    if (query.map_id.has_value() && *query.map_id != entry.map_id) {
      continue;
    }

    entries.push_back(std::move(entry));
  }

  std::sort(entries.begin(), entries.end(),
            [](const WarpEntry& a, const WarpEntry& b) {
              if (a.world != b.world) {
                return a.world < b.world;
              }
              if (a.map_id != b.map_id) {
                return a.map_id < b.map_id;
              }
              if (a.tile16_y != b.tile16_y) {
                return a.tile16_y < b.tile16_y;
              }
              if (a.tile16_x != b.tile16_x) {
                return a.tile16_x < b.tile16_x;
              }
              return static_cast<int>(a.type) < static_cast<int>(b.type);
            });

  return entries;
}

std::pair<int, int> AreaScreenSpan(const zelda3::OverworldMap& parent_map) {
  switch (parent_map.area_size()) {
    case zelda3::AreaSizeEnum::LargeArea:
      return {2, 2};
    case zelda3::AreaSizeEnum::WideArea:
      return {2, 1};
    case zelda3::AreaSizeEnum::TallArea:
      return {1, 2};
    default:
      // Pre-v3 ROMs only record large vs small.
      return parent_map.is_large_map() ? std::pair<int, int>{2, 2}
                                       : std::pair<int, int>{1, 1};
  }
}

absl::StatusOr<AreaTileLocation> ResolveAreaTileLocation(
    int map_id, int parent_map, int area_columns, int area_rows, int x, int y) {
  RETURN_IF_ERROR(ValidateMapId(map_id));
  RETURN_IF_ERROR(ValidateMapId(parent_map));
  if (area_columns < 1 || area_columns > 2 || area_rows < 1 || area_rows > 2) {
    return absl::InvalidArgumentError(
        absl::StrFormat("Area span must be 1 or 2 screens per axis, got %dx%d",
                        area_columns, area_rows));
  }
  ASSIGN_OR_RETURN(const int world, InferWorldFromMapId(map_id));
  ASSIGN_OR_RETURN(const int parent_world, InferWorldFromMapId(parent_map));
  if (world != parent_world) {
    return absl::FailedPreconditionError(absl::StrFormat(
        "Map 0x%02X is in the %s World but its parent 0x%02X is in the %s "
        "World",
        map_id, WorldName(world), parent_map, WorldName(parent_world)));
  }

  const int parent_local = parent_map - WorldOffset(world);
  const int parent_column = parent_local % 8;
  const int parent_row = parent_local / 8;
  const int map_local = map_id - WorldOffset(world);
  const int map_column = map_local % 8;
  const int map_row = map_local / 8;
  if (map_column < parent_column ||
      map_column >= parent_column + area_columns || map_row < parent_row ||
      map_row >= parent_row + area_rows) {
    return absl::FailedPreconditionError(absl::StrFormat(
        "Map 0x%02X is outside the %dx%d-screen area of its parent 0x%02X",
        map_id, area_columns, area_rows, parent_map));
  }
  if (parent_column + area_columns > 8) {
    return absl::FailedPreconditionError(absl::StrFormat(
        "Area 0x%02X (%d screens wide) crosses the world's east edge",
        parent_map, area_columns));
  }

  const int width = area_columns * 32;
  const int height = area_rows * 32;
  if (x < 0 || y < 0 || x >= width || y >= height) {
    return absl::OutOfRangeError(absl::StrFormat(
        "Tile (%d,%d) is outside area 0x%02X: x must be 0-%d and y 0-%d "
        "(tile16 units relative to the area's top-left; decimal, or hex with "
        "a 0x prefix)",
        x, y, parent_map, width - 1, height - 1));
  }

  AreaTileLocation location;
  location.map_id = map_id;
  location.parent_map = parent_map;
  location.world = world;
  location.area_width = width;
  location.area_height = height;
  location.area_x = x;
  location.area_y = y;
  location.screen_id = parent_map + (x / 32) + (y / 32) * 8;
  location.screen_x = x % 32;
  location.screen_y = y % 32;
  location.world_x = parent_column * 32 + x;
  location.world_y = parent_row * 32 + y;
  RETURN_IF_ERROR(ValidateMapId(location.screen_id));
  return location;
}

absl::StatusOr<AreaTileLocation> ResolveAreaTileForMap(
    const zelda3::Overworld& overworld, int map_id, int x, int y) {
  RETURN_IF_ERROR(ValidateMapId(map_id));
  const auto* map = overworld.overworld_map(map_id);
  if (map == nullptr) {
    return absl::FailedPreconditionError(
        absl::StrFormat("Overworld map 0x%02X is not loaded", map_id));
  }
  const int parent = map->parent();
  RETURN_IF_ERROR(ValidateMapId(parent));
  const auto* parent_map = overworld.overworld_map(parent);
  if (parent_map == nullptr) {
    return absl::FailedPreconditionError(
        absl::StrFormat("Overworld map 0x%02X is not loaded", parent));
  }
  const auto [columns, rows] = AreaScreenSpan(*parent_map);
  return ResolveAreaTileLocation(map_id, parent, columns, rows, x, y);
}

absl::StatusOr<AreaTileLocation> LocateScreenTile(
    const zelda3::Overworld& overworld, int screen_id, int screen_x,
    int screen_y) {
  RETURN_IF_ERROR(ValidateMapId(screen_id));
  if (screen_x < 0 || screen_x >= 32 || screen_y < 0 || screen_y >= 32) {
    return absl::OutOfRangeError(absl::StrFormat(
        "Screen tile (%d,%d) is outside 0-31", screen_x, screen_y));
  }
  const auto* map = overworld.overworld_map(screen_id);
  if (map == nullptr) {
    return absl::FailedPreconditionError(
        absl::StrFormat("Overworld map 0x%02X is not loaded", screen_id));
  }
  const int parent = map->parent();
  RETURN_IF_ERROR(ValidateMapId(parent));
  ASSIGN_OR_RETURN(const int world, InferWorldFromMapId(screen_id));
  const int screen_local = screen_id - WorldOffset(world);
  const int parent_local = parent - WorldOffset(world);
  const int x = (screen_local % 8 - parent_local % 8) * 32 + screen_x;
  const int y = (screen_local / 8 - parent_local / 8) * 32 + screen_y;
  return ResolveAreaTileForMap(overworld, screen_id, x, y);
}

absl::StatusOr<uint16_t> ReadAreaTile(zelda3::Overworld& overworld,
                                      const AreaTileLocation& location) {
  const auto& tiles = overworld.GetMapTiles(location.world);
  if (location.world_x < 0 ||
      location.world_x >= static_cast<int>(tiles.size()) ||
      location.world_y < 0 ||
      location.world_y >= static_cast<int>(tiles[location.world_x].size())) {
    return absl::FailedPreconditionError(
        "Overworld tile data is not loaded for this world");
  }
  return tiles[location.world_x][location.world_y];
}

absl::Status WriteAreaTile(zelda3::Overworld& overworld,
                           const AreaTileLocation& location, uint16_t tile_id) {
  RETURN_IF_ERROR(ReadAreaTile(overworld, location).status());
  overworld.GetMapTiles(location.world)[location.world_x][location.world_y] =
      tile_id;
  return absl::OkStatus();
}

absl::StatusOr<std::vector<TileMatch>> FindTileMatches(
    zelda3::Overworld& overworld, uint16_t tile_id,
    const TileSearchOptions& options) {
  if (options.map_id.has_value()) {
    RETURN_IF_ERROR(ValidateMapId(*options.map_id));
  }
  if (options.world.has_value()) {
    if (*options.world < 0 || *options.world > 2) {
      return absl::InvalidArgumentError(
          absl::StrFormat("Unknown world index: %d", *options.world));
    }
  }

  if (options.map_id.has_value() && options.world.has_value()) {
    ASSIGN_OR_RETURN(int inferred_world, InferWorldFromMapId(*options.map_id));
    if (inferred_world != *options.world) {
      return absl::InvalidArgumentError(absl::StrFormat(
          "Map 0x%02X belongs to the %s World but --world requested %s",
          *options.map_id, WorldName(inferred_world),
          WorldName(*options.world)));
    }
  }

  std::vector<int> worlds;
  if (options.world.has_value()) {
    worlds.push_back(*options.world);
  } else if (options.map_id.has_value()) {
    ASSIGN_OR_RETURN(int inferred_world, InferWorldFromMapId(*options.map_id));
    worlds.push_back(inferred_world);
  } else {
    worlds = {0, 1, 2};
  }

  std::vector<TileMatch> matches;

  for (int world : worlds) {
    int world_start = 0;
    int world_maps = 0;
    switch (world) {
      case 0:
        world_start = 0x00;
        world_maps = 0x40;
        break;
      case 1:
        world_start = 0x40;
        world_maps = 0x40;
        break;
      case 2:
        world_start = 0x80;
        world_maps = 0x20;
        break;
      default:
        return absl::InvalidArgumentError(
            absl::StrFormat("Unknown world index: %d", world));
    }

    overworld.set_current_world(world);

    for (int local_map = 0; local_map < world_maps; ++local_map) {
      int map_id = world_start + local_map;
      if (options.map_id.has_value() && map_id != *options.map_id) {
        continue;
      }

      int map_x_index = local_map % 8;
      int map_y_index = local_map / 8;

      int global_x_start = map_x_index * 32;
      int global_y_start = map_y_index * 32;

      for (int local_y = 0; local_y < 32; ++local_y) {
        for (int local_x = 0; local_x < 32; ++local_x) {
          int global_x = global_x_start + local_x;
          int global_y = global_y_start + local_y;

          uint16_t current_tile = overworld.GetTile(global_x, global_y);
          if (current_tile == tile_id) {
            matches.push_back(
                {map_id, world, local_x, local_y, global_x, global_y});
          }
        }
      }
    }
  }

  return matches;
}

const char* SpritePhaseName(int phase) {
  switch (phase) {
    case 0:
      return "beginning";
    case 1:
      return "first_part";
    case 2:
      return "second_part";
  }
  return "unknown";
}

absl::StatusOr<std::vector<OverworldSprite>> CollectOverworldSprites(
    const zelda3::Overworld& overworld, const SpriteQuery& query) {
  std::vector<OverworldSprite> results;

  // Iterate through all 3 game states (beginning, zelda, agahnim)
  for (int game_state = 0; game_state < 3; ++game_state) {
    if (query.phase.has_value() && *query.phase != game_state) {
      continue;
    }
    const auto sprites = overworld.sprites(game_state);
    std::map<int, int> next_index_by_map;

    for (const auto& sprite : sprites) {
      const int list_index = next_index_by_map[sprite.map_id()]++;
      // Apply filters
      if (query.sprite_id.has_value() && sprite.id() != *query.sprite_id) {
        continue;
      }

      int map_id = sprite.map_id();
      if (query.map_id.has_value() && map_id != *query.map_id) {
        continue;
      }

      // Determine world from map_id
      int world = (map_id >= kSpecialWorldOffset)
                      ? 2
                      : (map_id >= kDarkWorldOffset ? 1 : 0);

      if (query.world.has_value() && world != *query.world) {
        continue;
      }

      OverworldSprite entry;
      entry.sprite_id = sprite.id();
      entry.map_id = map_id;
      entry.world = world;
      entry.x = sprite.x();
      entry.y = sprite.y();
      entry.phase = game_state;
      entry.list_index = list_index;
      entry.local_x = (sprite.x() - (map_id % 8) * 512) / 16;
      entry.local_y = (sprite.y() - ((map_id % 64) / 8) * 512) / 16;
      entry.sprite_name = zelda3::ResolveSpriteName(sprite.id());

      results.push_back(entry);
    }
  }

  return results;
}

absl::StatusOr<EntranceDetails> GetEntranceDetails(
    const zelda3::Overworld& overworld, uint8_t entrance_id) {
  const auto& entrances = overworld.entrances();

  if (entrance_id >= entrances.size()) {
    return absl::NotFoundError(absl::StrFormat(
        "Entrance %d not found (max: %d)", entrance_id, entrances.size() - 1));
  }

  const auto& entrance = entrances[entrance_id];

  EntranceDetails details;
  details.entrance_id = entrance_id;
  details.map_id = entrance.map_id_;

  // Determine world from map_id
  details.world = (details.map_id >= kSpecialWorldOffset)
                      ? 2
                      : (details.map_id >= kDarkWorldOffset ? 1 : 0);

  details.x = entrance.x_;
  details.y = entrance.y_;
  details.area_x = entrance.game_x_;
  details.area_y = entrance.game_y_;
  details.map_pos = entrance.map_pos_;
  details.is_hole = entrance.is_hole_;

  // Get entrance name if available
  details.entrance_name = EntranceLabel(entrance_id);

  return details;
}

absl::StatusOr<TileStatistics> AnalyzeTileUsage(
    zelda3::Overworld& overworld, uint16_t tile_id,
    const TileSearchOptions& options) {
  // Use FindTileMatches to get all occurrences
  ASSIGN_OR_RETURN(auto matches, FindTileMatches(overworld, tile_id, options));

  TileStatistics stats;
  stats.tile_id = tile_id;
  stats.count = static_cast<int>(matches.size());

  // If scoped to a specific map, store that info
  if (options.map_id.has_value()) {
    stats.map_id = *options.map_id;
    if (options.world.has_value()) {
      stats.world = *options.world;
    } else {
      ASSIGN_OR_RETURN(stats.world, InferWorldFromMapId(*options.map_id));
    }
  } else {
    stats.map_id = -1;  // Indicates all maps
    stats.world = -1;
  }

  // Store positions (convert from TileMatch to pair)
  stats.positions.reserve(matches.size());
  for (const auto& match : matches) {
    stats.positions.emplace_back(match.local_x, match.local_y);
  }

  return stats;
}

}  // namespace overworld
}  // namespace cli
}  // namespace yaze
