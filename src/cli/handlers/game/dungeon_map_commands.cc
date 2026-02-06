#include "cli/handlers/game/dungeon_map_commands.h"

#include <cstdint>
#include <string>
#include <vector>

#include "absl/strings/str_format.h"
#include "cli/util/hex_util.h"
#include "rom/rom.h"
#include "zelda3/dungeon/custom_collision.h"
#include "zelda3/dungeon/room.h"
#include "zelda3/dungeon/room_object.h"

namespace yaze {
namespace cli {
namespace handlers {

using util::ParseHexString;

namespace {

// ASCII character mapping for room tiles
constexpr char kCharWall = '#';
constexpr char kCharFloor = '.';
constexpr char kCharWater = '~';
constexpr char kCharPit = 'v';
constexpr char kCharStair = '>';
constexpr char kCharDoor = 'D';
constexpr char kCharChest = 'C';
constexpr char kCharSpike = 'X';
constexpr char kCharBlock = 'B';
constexpr char kCharTorch = 'T';
constexpr char kCharSwitch = 'S';
constexpr char kCharTrackH = '-';
constexpr char kCharTrackV = '|';
constexpr char kCharTrackX = '+';
constexpr char kCharStopN = 'N';
constexpr char kCharStopS = 's';
constexpr char kCharStopE = 'E';
constexpr char kCharStopW = 'W';
constexpr char kCharUnknown = '?';

// Room dimensions
constexpr int kRoomWidth = 64;
constexpr int kRoomHeight = 64;

// Classify an object ID to an ASCII character
char ClassifyObject(uint16_t object_id) {
  // Common object types from ALTTP (approximate ranges)
  // These are based on typical object ID patterns

  // Walls and borders (0x000-0x0FF often includes wall/floor variants)
  if (object_id >= 0x000 && object_id <= 0x00F) {
    return kCharFloor;
  }

  // Pits and holes
  if (object_id == 0x020 || object_id == 0x021 || object_id == 0x022) {
    return kCharPit;
  }

  // Water tiles
  if (object_id >= 0x030 && object_id <= 0x03F) {
    return kCharWater;
  }
  if (object_id >= 0x040 && object_id <= 0x04F) {
    return kCharWater;
  }

  // Stairs (0x138, 0x139, 0x13A, 0x13B)
  if (object_id >= 0x138 && object_id <= 0x13B) {
    return kCharStair;
  }
  if (object_id == 0x12D || object_id == 0x12E) {
    return kCharStair;
  }

  // Chests
  if (object_id >= 0x0F0 && object_id <= 0x0FF) {
    return kCharChest;
  }

  // Blocks/pushable (exclude $D0-$D3 switch tracks)
  if (object_id >= 0x0D4 && object_id <= 0x0DF) {
    return kCharBlock;
  }

  // Torches
  if (object_id >= 0x0E0 && object_id <= 0x0EF) {
    return kCharTorch;
  }

  // Spikes
  if (object_id >= 0x100 && object_id <= 0x10F) {
    return kCharSpike;
  }

  // Switches/crystals
  if (object_id >= 0x110 && object_id <= 0x11F) {
    return kCharSwitch;
  }

  // Walls (common high-range objects)
  if (object_id >= 0x200 && object_id <= 0x2FF) {
    return kCharWall;
  }

  return kCharUnknown;
}

// Map room object to grid position and character
void PlaceObject(std::vector<std::string>& grid, const zelda3::RoomObject& obj,
                 int layer_filter) {
  // Skip if wrong layer
  if (layer_filter >= 0 && static_cast<int>(obj.layer_) != layer_filter) {
    return;
  }

  int tile_x = obj.x_;
  int tile_y = obj.y_;
  int width = obj.size_ > 0 ? obj.size_ : 1;

  char ch = ClassifyObject(obj.id_);

  // Place the object character(s) on the grid
  for (int dx = 0; dx < width; ++dx) {
    int gx = tile_x + dx;
    int gy = tile_y;

    if (gx >= 0 && gx < kRoomWidth && gy >= 0 && gy < kRoomHeight) {
      // Don't overwrite more important markers with floor
      if (ch == kCharFloor && grid[gy][gx] != kCharFloor) {
        continue;
      }
      grid[gy][gx] = ch;
    }
  }
}

// Place doors on the grid
void PlaceDoors(std::vector<std::string>& grid,
                const std::vector<zelda3::Room::Door>& doors) {
  for (const auto& door : doors) {
    auto [tile_x, tile_y] = door.GetTileCoords();

    // Doors are typically 2-3 tiles wide
    for (int dx = 0; dx < 2; ++dx) {
      int gx = tile_x + dx;
      int gy = tile_y;

      if (gx >= 0 && gx < kRoomWidth && gy >= 0 && gy < kRoomHeight) {
        grid[gy][gx] = kCharDoor;
      }
    }
  }
}

// Place stairs on the grid
// Note: staircase is a C struct from zelda.h with id (x) and room (y) fields
void PlaceStairs(std::vector<std::string>& grid,
                 const std::vector<staircase>& stairs) {
  for (const auto& stair : stairs) {
    // staircase struct: id = x position index, room = y position index
    // These are tile indices
    int tile_x = stair.id;
    int tile_y = stair.room;

    if (tile_x >= 0 && tile_x < kRoomWidth && tile_y >= 0 &&
        tile_y < kRoomHeight) {
      grid[tile_y][tile_x] = kCharStair;
    }
  }
}

// Place chests on the grid
// Note: chest_data is a C struct from zelda.h with id and size fields
void PlaceChests(std::vector<std::string>& grid,
                 const std::vector<chest_data>& chests) {
  // Chest positions need to be decoded from ROM format
  // For now we just mark them if we have location data
  // (Full chest position decoding would require additional ROM reads)
  (void)grid;    // Suppress unused warning
  (void)chests;  // Suppress unused warning
}

char ClassifyCustomCollisionTile(uint8_t tile) {
  // Oracle-of-Secrets minecart tracks/stops are stored in the custom collision
  // map (ZScream format), not as tile objects.
  switch (tile) {
    case 0xB0:  // horizontal
      return kCharTrackH;
    case 0xB1:  // vertical
      return kCharTrackV;
    case 0xB7:
      return kCharStopN;
    case 0xB8:
      return kCharStopS;
    case 0xB9:
      return kCharStopW;
    case 0xBA:
      return kCharStopE;
    default:
      break;
  }

  if ((tile >= 0xB2 && tile <= 0xB6) || (tile >= 0xBB && tile <= 0xBE)) {
    return kCharTrackX;
  }
  if (tile >= 0xD0 && tile <= 0xD3) {
    return kCharSwitch;
  }
  return 0;
}

void OverlayCustomCollision(std::vector<std::string>& grid, Rom* rom,
                            int room_id) {
  auto map_or = zelda3::LoadCustomCollisionMap(rom, room_id);
  if (!map_or.ok() || !map_or.value().has_data) {
    return;
  }

  const auto& map = map_or.value().tiles;
  for (int y = 0; y < kRoomHeight; ++y) {
    for (int x = 0; x < kRoomWidth; ++x) {
      uint8_t tile = map[static_cast<size_t>(y * kRoomWidth + x)];
      char ch = ClassifyCustomCollisionTile(tile);
      if (ch == 0) {
        continue;
      }

      // Overlay tracks/stops on top of floor/unknown. Preserve higher-signal
      // markers like doors/walls.
      if (grid[y][x] == kCharFloor || grid[y][x] == kCharUnknown) {
        grid[y][x] = ch;
      }
    }
  }
}

}  // namespace

absl::Status DungeonMapCommandHandler::Execute(
    Rom* rom, const resources::ArgumentParser& parser,
    resources::OutputFormatter& formatter) {
  auto room_id_str = parser.GetString("room").value();
  auto layer_opt = parser.GetString("layer");

  int room_id;
  if (!ParseHexString(room_id_str, &room_id)) {
    return absl::InvalidArgumentError(
        "Invalid room ID format. Must be hex (e.g., 0x07).");
  }

  int layer_filter = -1;  // -1 means all layers
  if (layer_opt.has_value()) {
    if (!ParseHexString(layer_opt.value(), &layer_filter)) {
      return absl::InvalidArgumentError(
          "Invalid layer format. Must be 0, 1, or 2.");
    }
    if (layer_filter < 0 || layer_filter > 2) {
      return absl::InvalidArgumentError("Layer must be 0, 1, or 2.");
    }
  }

  // Load room with full objects
  zelda3::Room room = zelda3::LoadRoomFromRom(rom, room_id);

  // Initialize grid with floor tiles
  std::vector<std::string> grid(kRoomHeight, std::string(kRoomWidth, kCharFloor));

  // Add border walls
  for (int x = 0; x < kRoomWidth; ++x) {
    grid[0][x] = kCharWall;
    grid[kRoomHeight - 1][x] = kCharWall;
  }
  for (int y = 0; y < kRoomHeight; ++y) {
    grid[y][0] = kCharWall;
    grid[y][kRoomWidth - 1] = kCharWall;
  }

  // Place objects on the grid
  const auto& objects = room.GetTileObjects();
  for (const auto& obj : objects) {
    PlaceObject(grid, obj, layer_filter);
  }

  // Place doors
  PlaceDoors(grid, room.GetDoors());

  // Place stairs
  PlaceStairs(grid, room.GetStairs());

  // Place chests (if position data available)
  PlaceChests(grid, room.GetChests());

  // Overlay Oracle custom collision tiles (minecart tracks/stops/switches).
  OverlayCustomCollision(grid, rom, room_id);

  // Output
  formatter.BeginObject("dungeon_map");
  formatter.AddField("room_id", absl::StrFormat("0x%02X", room_id));
  formatter.AddField("room_name", std::string(zelda3::kRoomNames[room_id]));
  formatter.AddField("width", kRoomWidth);
  formatter.AddField("height", kRoomHeight);
  formatter.AddField("layer_filter",
                     layer_filter >= 0 ? absl::StrFormat("%d", layer_filter)
                                       : "all");
  formatter.AddField("object_count", static_cast<int>(objects.size()));
  formatter.AddField("door_count", static_cast<int>(room.GetDoors().size()));

  // Legend
  formatter.BeginObject("legend");
  formatter.AddField("#", "wall");
  formatter.AddField(".", "floor");
  formatter.AddField("~", "water");
  formatter.AddField("v", "pit");
  formatter.AddField(">", "stair");
  formatter.AddField("D", "door");
  formatter.AddField("C", "chest");
  formatter.AddField("X", "spike");
  formatter.AddField("B", "block");
  formatter.AddField("T", "torch");
  formatter.AddField("S", "switch");
  formatter.AddField("-", "track_h");
  formatter.AddField("|", "track_v");
  formatter.AddField("+", "track_x");
  formatter.AddField("N/s/E/W", "stop_tiles");
  formatter.EndObject();

  // ASCII map as array of strings
  formatter.BeginArray("map");
  // Header row with column numbers
  std::string header = "    ";
  for (int x = 0; x < kRoomWidth; x += 10) {
    header += absl::StrFormat("%-10d", x);
  }
  formatter.AddArrayItem(header);

  // Grid rows with row numbers
  for (int y = 0; y < kRoomHeight; ++y) {
    std::string row = absl::StrFormat("%2d  %s", y, grid[y]);
    formatter.AddArrayItem(row);
  }
  formatter.EndArray();

  formatter.EndObject();

  return absl::OkStatus();
}

}  // namespace handlers
}  // namespace cli
}  // namespace yaze
