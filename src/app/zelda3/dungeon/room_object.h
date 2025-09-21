#ifndef YAZE_APP_ZELDA3_DUNGEON_ROOM_OBJECT_H
#define YAZE_APP_ZELDA3_DUNGEON_ROOM_OBJECT_H

#include <cstdint>
#include <string>
#include <vector>

#include "absl/strings/string_view.h"
#include "app/gfx/snes_tile.h"
#include "app/rom.h"
#include "app/zelda3/dungeon/object_parser.h"

namespace yaze {
namespace zelda3 {

struct SubtypeInfo {
  uint32_t subtype_ptr;
  uint32_t routine_ptr;
};

SubtypeInfo FetchSubtypeInfo(uint16_t object_id);

enum class SpecialObjectType { Chest, BigChest, InterroomStairs };

enum Sorting {
  All = 0,
  Wall = 1,
  Horizontal = 2,
  Vertical = 4,
  NonScalable = 8,
  Dungeons = 16,
  Floors = 32,
  SortStairs = 64
};

enum class ObjectOption {
  Nothing = 0,
  Door = 1,
  Chest = 2,
  Block = 4,
  Torch = 8,
  Bgr = 16,
  Stairs = 32
};

ObjectOption operator|(ObjectOption lhs, ObjectOption rhs);
ObjectOption operator&(ObjectOption lhs, ObjectOption rhs);
ObjectOption operator^(ObjectOption lhs, ObjectOption rhs);
ObjectOption operator~(ObjectOption option);

constexpr int kRoomObjectSubtype1 = 0x8000;          // JP = Same
constexpr int kRoomObjectSubtype2 = 0x83F0;          // JP = Same
constexpr int kRoomObjectSubtype3 = 0x84F0;          // JP = Same
constexpr int kRoomObjectTileAddress = 0x1B52;       // JP = Same
constexpr int kRoomObjectTileAddressFloor = 0x1B5A;  // JP = Same

class RoomObject {
 public:
  enum LayerType { BG1 = 0, BG2 = 1, BG3 = 2 };

  RoomObject(int16_t id, uint8_t x, uint8_t y, uint8_t size, uint8_t layer = 0)
      : id_(id),
        x_(x),
        y_(y),
        size_(size),
        layer_(static_cast<LayerType>(layer)),
        nx_(x),
        ny_(y),
        ox_(x),
        oy_(y),
        width_(16),
        height_(16) {}

  void set_rom(Rom* rom) { rom_ = rom; }
  auto rom() { return rom_; }
  auto mutable_rom() { return rom_; }

  // Ensures tiles_ is populated with a basic set based on ROM tables so we can
  // preview/draw objects without needing full emulator execution.
  void EnsureTilesLoaded();
  
  // Load tiles using the new ObjectParser
  absl::Status LoadTilesWithParser();

  // Getter for tiles
  const std::vector<gfx::Tile16>& tiles() const { return tiles_; }
  std::vector<gfx::Tile16>& mutable_tiles() { return tiles_; }

  void AddTiles(int nbr, int pos) {
    for (int i = 0; i < nbr; i++) {
      ASSIGN_OR_LOG_ERROR(auto tile, rom()->ReadTile16(pos + (i * 2)));
      tiles_.push_back(tile);
    }
  }

  void DrawTile(gfx::Tile16 t, int xx, int yy,
                std::vector<uint8_t>& current_gfx16,
                std::vector<uint8_t>& tiles_bg1_buffer,
                std::vector<uint8_t>& tiles_bg2_buffer,
                uint16_t tile_under = 0xFFFF);

  auto options() const { return options_; }
  void set_options(ObjectOption options) { options_ = options; }

  bool all_bgs_ = false;
  bool lit_ = false;

  int16_t id_;
  uint8_t x_;
  uint8_t y_;
  uint8_t size_;
  uint8_t nx_;
  uint8_t ny_;
  uint8_t ox_;
  uint8_t oy_;
  uint8_t z_ = 0;
  uint8_t previous_size_ = 0;

  int width_;
  int height_;
  int offset_x_ = 0;
  int offset_y_ = 0;

  std::string name_;

  std::vector<uint8_t> preview_object_data_;
  std::vector<gfx::Tile16> tiles_;

  LayerType layer_;
  ObjectOption options_ = ObjectOption::Nothing;

  Rom* rom_;
};

class Subtype1 : public RoomObject {
 public:
  bool all_bgs;
  int tile_count_;
  std::string name;
  Sorting sort;

  Subtype1(int16_t id, uint8_t x, uint8_t y, uint8_t size, uint8_t layer,
           int tile_count)
      : RoomObject(id, x, y, size, layer), tile_count_(tile_count) {
    auto rom_data = rom()->data();
    int pos = kRoomObjectTileAddress +
              static_cast<int16_t>(
                  (rom_data[kRoomObjectSubtype1 + ((id & 0xFF) * 2) + 1] << 8) +
                  rom_data[kRoomObjectSubtype1 + ((id & 0xFF) * 2)]);
    AddTiles(tile_count_, pos);
    sort = (Sorting)(Sorting::Horizontal | Sorting::Wall);
  }

  void Draw(std::vector<uint8_t>& current_gfx16,
            std::vector<uint8_t>& tiles_bg1_buffer,
            std::vector<uint8_t>& tiles_bg2_buffer) {
    for (int s = 0; s < size_ + (tile_count_ == 8 ? 1 : 0); s++) {
      for (int i = 0; i < tile_count_; i++) {
        DrawTile(tiles_[i], ((s * 2)) * 8, (i / 2) * 8, current_gfx16,
                 tiles_bg1_buffer, tiles_bg2_buffer);
      }
    }
  }
};

class Subtype2 : public RoomObject {
 public:
  std::string name;
  bool all_bgs;
  Sorting sort;

  Subtype2(int16_t id, uint8_t x, uint8_t y, uint8_t size, uint8_t layer)
      : RoomObject(id, x, y, size, layer) {
    auto rom_data = rom()->data();
    int pos = kRoomObjectTileAddress +
              static_cast<int16_t>(
                  (rom_data[kRoomObjectSubtype2 + ((id & 0x7F) * 2) + 1] << 8) +
                  rom_data[kRoomObjectSubtype2 + ((id & 0x7F) * 2)]);
    AddTiles(8, pos);
    sort = (Sorting)(Sorting::Horizontal | Sorting::Wall);
  }

  void Draw(std::vector<uint8_t>& current_gfx16,
            std::vector<uint8_t>& tiles_bg1_buffer,
            std::vector<uint8_t>& tiles_bg2_buffer) {
    for (int i = 0; i < 8; i++) {
      DrawTile(tiles_[i], x_ * 8, (y_ + i) * 8, current_gfx16, tiles_bg1_buffer,
               tiles_bg2_buffer);
    }
  }
};

class Subtype3 : public RoomObject {
 public:
  bool all_bgs;
  std::string name;
  Sorting sort;

  Subtype3(int16_t id, uint8_t x, uint8_t y, uint8_t size, uint8_t layer)
      : RoomObject(id, x, y, size, layer) {
    auto rom_data = rom()->data();
    int pos = kRoomObjectTileAddress +
              static_cast<int16_t>(
                  (rom_data[kRoomObjectSubtype3 + ((id & 0xFF) * 2) + 1] << 8) +
                  rom_data[kRoomObjectSubtype3 + ((id & 0xFF) * 2)]);
    AddTiles(8, pos);
    sort = (Sorting)(Sorting::Horizontal | Sorting::Wall);
  }

  void Draw(std::vector<uint8_t>& current_gfx16,
            std::vector<uint8_t>& tiles_bg1_buffer,
            std::vector<uint8_t>& tiles_bg2_buffer) {
    for (int i = 0; i < 8; i++) {
      DrawTile(tiles_[i], x_ * 8, (y_ + i) * 8, current_gfx16, tiles_bg1_buffer,
               tiles_bg2_buffer);
    }
  }
};

constexpr static inline absl::string_view Type1RoomObjectNames[] = {
    "Ceiling ↔",
    "Wall (top, north) ↔",
    "Wall (top, south) ↔",
    "Wall (bottom, north) ↔",
    "Wall (bottom, south) ↔",
    "Wall columns (north) ↔",
    "Wall columns (south) ↔",
    "Deep wall (north) ↔",
    "Deep wall (south) ↔",
    "Diagonal wall A ◤ (top) ↔",
    "Diagonal wall A ◣ (top) ↔",
    "Diagonal wall A ◥ (top) ↔",
    "Diagonal wall A ◢ (top) ↔",
    "Diagonal wall B ◤ (top) ↔",
    "Diagonal wall B ◣ (top) ↔",
    "Diagonal wall B ◥ (top) ↔",
    "Diagonal wall B ◢ (top) ↔",
    "Diagonal wall C ◤ (top) ↔",
    "Diagonal wall C ◣ (top) ↔",
    "Diagonal wall C ◥ (top) ↔",
    "Diagonal wall C ◢ (top) ↔",
    "Diagonal wall A ◤ (bottom) ↔",
    "Diagonal wall A ◣ (bottom) ↔",
    "Diagonal wall A ◥ (bottom) ↔",
    "Diagonal wall A ◢ (bottom) ↔",
    "Diagonal wall B ◤ (bottom) ↔",
    "Diagonal wall B ◣ (bottom) ↔",
    "Diagonal wall B ◥ (bottom) ↔",
    "Diagonal wall B ◢ (bottom) ↔",
    "Diagonal wall C ◤ (bottom) ↔",
    "Diagonal wall C ◣ (bottom) ↔",
    "Diagonal wall C ◥ (bottom) ↔",
    "Diagonal wall C ◢ (bottom) ↔",
    "Platform stairs ↔",
    "Rail ↔",
    "Pit edge ┏━┓ A (north) ↔",
    "Pit edge ┏━┓ B (north) ↔",
    "Pit edge ┏━┓ C (north) ↔",
    "Pit edge ┏━┓ D (north) ↔",
    "Pit edge ┏━┓ E (north) ↔",
    "Pit edge ┗━┛ (south) ↔",
    "Pit edge ━━━ (south) ↔",
    "Pit edge ━━━ (north) ↔",
    "Pit edge ━━┛ (south) ↔",
    "Pit edge ┗━━ (south) ↔",
    "Pit edge ━━┓ (north) ↔",
    "Pit edge ┏━━ (north) ↔",
    "Rail wall (north) ↔",
    "Rail wall (south) ↔",
    "Nothing",
    "Nothing",
    "Carpet ↔",
    "Carpet trim ↔",
    "Weird door",  // TODO: WEIRD DOOR OBJECT NEEDS INVESTIGATION
    "Drapes (north) ↔",
    "Drapes (west, odd) ↔",
    "Statues ↔",
    "Columns ↔",
    "Wall decors (north) ↔",
    "Wall decors (south) ↔",
    "Chairs in pairs ↔",
    "Tall torches ↔",
    "Supports (north) ↔",
    "Water edge ┏━┓ (concave) ↔",
    "Water edge ┗━┛ (concave) ↔",
    "Water edge ┏━┓ (convex) ↔",
    "Water edge ┗━┛ (convex) ↔",
    "Water edge ┏━┛ (concave) ↔",
    "Water edge ┗━┓ (concave) ↔",
    "Water edge ┗━┓ (convex) ↔",
    "Water edge ┏━┛ (convex) ↔",
    "Unknown",  // TODO: NEEDS IN GAME CHECKING
    "Unknown",  // TODO: NEEDS IN GAME CHECKING
    "Unknown",  // TODO: NEEDS IN GAME CHECKING
    "Unknown",  // TODO: NEEDS IN GAME CHECKING
    "Supports (south) ↔",
    "Bar ↔",
    "Shelf A ↔",
    "Shelf B ↔",
    "Shelf C ↔",
    "Somaria path ↔",
    "Cannon hole A (north) ↔",
    "Cannon hole A (south) ↔",
    "Pipe path ↔",
    "Nothing",
    "Wall torches (north) ↔",
    "Wall torches (south) ↔",
    "Nothing",
    "Nothing",
    "Nothing",
    "Nothing",
    "Cannon hole B (north) ↔",
    "Cannon hole B (south) ↔",
    "Thick rail ↔",
    "Blocks ↔",
    "Long rail ↔",
    "Ceiling ↕",
    "Wall (top, west) ↕",
    "Wall (top, east) ↕",
    "Wall (bottom, west) ↕",
    "Wall (bottom, east) ↕",
    "Wall columns (west) ↕",
    "Wall columns (east) ↕",
    "Deep wall (west) ↕",
    "Deep wall (east) ↕",
    "Rail ↕",
    "Pit edge (west) ↕",
    "Pit edge (east) ↕",
    "Rail wall (west) ↕",
    "Rail wall (east) ↕",
    "Nothing",
    "Nothing",
    "Carpet ↕",
    "Carpet trim ↕",
    "Nothing",
    "Drapes (west) ↕",
    "Drapes (east) ↕",
    "Columns ↕",
    "Wall decors (west) ↕",
    "Wall decors (east) ↕",
    "Supports (west) ↕",
    "Water edge (west) ↕",
    "Water edge (east) ↕",
    "Supports (east) ↕",
    "Somaria path ↕",
    "Pipe path ↕",
    "Nothing",
    "Wall torches (west) ↕",
    "Wall torches (east) ↕",
    "Wall decors tight A (west) ↕",
    "Wall decors tight A (east) ↕",
    "Wall decors tight B (west) ↕",
    "Wall decors tight B (east) ↕",
    "Cannon hole (west) ↕",
    "Cannon hole (east) ↕",
    "Tall torches ↕",
    "Thick rail ↕",
    "Blocks ↕",
    "Long rail ↕",
    "Jump ledge (west) ↕",
    "Jump ledge (east) ↕",
    "Rug trim (west) ↕",
    "Rug trim (east) ↕",
    "Bar ↕",
    "Wall flair (west) ↕",
    "Wall flair (east) ↕",
    "Blue pegs ↕",
    "Orange pegs ↕",
    "Invisible floor ↕",
    "Fake pots ↕",
    "Hammer pegs ↕",
    "Nothing",
    "Nothing",
    "Nothing",
    "Nothing",
    "Nothing",
    "Nothing",
    "Nothing",
    "Nothing",
    "Nothing",
    "Diagonal ceiling A ◤",
    "Diagonal ceiling A ◣",
    "Diagonal ceiling A ◥",
    "Diagonal ceiling A ◢",
    "Pit ⇲",
    "Diagonal layer 2 mask A ◤",
    "Diagonal layer 2 mask A ◣",
    "Diagonal layer 2 mask A ◥",
    "Diagonal layer 2 mask A ◢",
    "Diagonal layer 2 mask B ◤",  // TODO: VERIFY
    "Diagonal layer 2 mask B ◣",  // TODO: VERIFY
    "Diagonal layer 2 mask B ◥",  // TODO: VERIFY
    "Diagonal layer 2 mask B ◢",  // TODO: VERIFY
    "Nothing",
    "Nothing",
    "Nothing",
    "Jump ledge (north) ↔",
    "Jump ledge (south) ↔",
    "Rug ↔",
    "Rug trim (north) ↔",
    "Rug trim (south) ↔",
    "Archery game curtains ↔",
    "Wall flair (north) ↔",
    "Wall flair (south) ↔",
    "Blue pegs ↔",
    "Orange pegs ↔",
    "Invisible floor ↔",
    "Fake pressure plates ↔",
    "Fake pots ↔",
    "Hammer pegs ↔",
    "Nothing",
    "Nothing",
    "Ceiling (large) ⇲",
    "Chest platform (tall) ⇲",
    "Layer 2 pit mask (large) ⇲",
    "Layer 2 pit mask (medium) ⇲",
    "Floor 1 ⇲",
    "Floor 3 ⇲",
    "Layer 2 mask (large) ⇲",
    "Floor 4 ⇲",
    "Water floor ⇲ ",
    "Flood water (medium) ⇲ ",
    "Conveyor floor ⇲ ",
    "Nothing",
    "Nothing",
    "Moving wall (west) ⇲",
    "Moving wall (east) ⇲",
    "Nothing",
    "Nothing",
    "Icy floor A ⇲",
    "Icy floor B ⇲",
    "Moving wall flag",  // TODO: WTF IS THIS?
    "Moving wall flag",  // TODO: WTF IS THIS?
    "Moving wall flag",  // TODO: WTF IS THIS?
    "Moving wall flag",  // TODO: WTF IS THIS?
    "Layer 2 mask (medium) ⇲",
    "Flood water (large) ⇲",
    "Layer 2 swim mask ⇲",
    "Flood water B (large) ⇲",
    "Floor 2 ⇲",
    "Chest platform (short) ⇲",
    "Table / rock ⇲",
    "Spike blocks ⇲",
    "Spiked floor ⇲",
    "Floor 7 ⇲",
    "Tiled floor ⇲",
    "Rupee floor ⇲",
    "Conveyor upwards ⇲",
    "Conveyor downwards ⇲",
    "Conveyor leftwards ⇲",
    "Conveyor rightwards ⇲",
    "Heavy current water ⇲",
    "Floor 10 ⇲",
    "Nothing",
    "Nothing",
    "Nothing",
    "Nothing",
    "Nothing",
    "Nothing",
    "Nothing",
    "Nothing",
    "Nothing",
    "Nothing",
    "Nothing",
    "Nothing",
    "Nothing",
    "Nothing",
    "Nothing",
};

constexpr static inline absl::string_view Type2RoomObjectNames[] = {
    "Corner (top, concave) ▛",
    "Corner (top, concave) ▙",
    "Corner (top, concave) ▜",
    "Corner (top, concave) ▟",
    "Corner (top, convex) ▟",
    "Corner (top, convex) ▜",
    "Corner (top, convex) ▙",
    "Corner (top, convex) ▛",
    "Corner (bottom, concave) ▛",
    "Corner (bottom, concave) ▙",
    "Corner (bottom, concave) ▜",
    "Corner (bottom, concave) ▟",
    "Corner (bottom, convex) ▟",
    "Corner (bottom, convex) ▜",
    "Corner (bottom, convex) ▙",
    "Corner (bottom, convex) ▛",
    "Kinked corner north (bottom) ▜",
    "Kinked corner south (bottom) ▟",
    "Kinked corner north (bottom) ▛",
    "Kinked corner south (bottom) ▙",
    "Kinked corner west (bottom) ▙",
    "Kinked corner west (bottom) ▛",
    "Kinked corner east (bottom) ▟",
    "Kinked corner east (bottom) ▜",
    "Deep corner (concave) ▛",
    "Deep corner (concave) ▙",
    "Deep corner (concave) ▜",
    "Deep corner (concave) ▟",
    "Large brazier",
    "Statue",
    "Star tile (disabled)",
    "Star tile (enabled)",
    "Small torch (lit)",
    "Barrel",
    "Unknown",  // TODO: NEEDS IN GAME CHECKING
    "Table",
    "Fairy statue",
    "Unknown",  // TODO: NEEDS IN GAME CHECKING
    "Unknown",  // TODO: NEEDS IN GAME CHECKING
    "Chair",
    "Bed",
    "Fireplace",
    "Mario portrait",
    "Unknown",  // TODO: NEEDS IN GAME CHECKING
    "Unknown",  // TODO: NEEDS IN GAME CHECKING
    "Interroom stairs (up)",
    "Interroom stairs (down)",
    "Interroom stairs B (down)",
    "Intraroom stairs north B",  // TODO: VERIFY LAYER HANDLING
    "Intraroom stairs north (separate layers)",
    "Intraroom stairs north (merged layers)",
    "Intraroom stairs north (swim layer)",
    "Block",
    "Water ladder (north)",
    "Water ladder (south)",  // TODO: NEEDS IN GAME VERIFICATION
    "Dam floodgate",
    "Interroom spiral stairs up (top)",
    "Interroom spiral stairs down (top)",
    "Interroom spiral stairs up (bottom)",
    "Interroom spiral stairs down (bottom)",
    "Sanctuary wall (north)",
    "Unknown",  // TODO: NEEDS IN GAME CHECKING
    "Pew",
    "Magic bat altar",
};

constexpr static inline absl::string_view Type3RoomObjectNames[] = {
    "Waterfall face (empty)",
    "Waterfall face (short)",
    "Waterfall face (long)",
    "Somaria path endpoint",
    "Somaria path intersection ╋",
    "Somaria path corner ┏",
    "Somaria path corner ┗",
    "Somaria path corner ┓",
    "Somaria path corner ┛",
    "Somaria path intersection ┳",
    "Somaria path intersection ┻",
    "Somaria path intersection ┣",
    "Somaria path intersection ┫",
    "Unknown",  // TODO: NEEDS IN GAME CHECKING
    "Somaria path 2-way endpoint",
    "Somaria path crossover",
    "Babasu hole (north)",
    "Babasu hole (south)",
    "9 blue rupees",
    "Telepathy tile",
    "Warp door",  // TODO: NEEDS IN GAME VERIFICATION THAT THIS IS USELESS
    "Kholdstare's shell",
    "Hammer peg",
    "Prison cell",
    "Big key lock",
    "Chest",
    "Chest (open)",
    "Intraroom stairs south",  // TODO: VERIFY LAYER HANDLING
    "Intraroom stairs south (separate layers)",
    "Intraroom stairs south (merged layers)",
    "Interroom straight stairs up (north, top)",
    "Interroom straight stairs down (north, top)",
    "Interroom straight stairs up (south, top)",
    "Interroom straight stairs down (south, top)",
    "Deep corner (convex) ▟",
    "Deep corner (convex) ▜",
    "Deep corner (convex) ▙",
    "Deep corner (convex) ▛",
    "Interroom straight stairs up (north, bottom)",
    "Interroom straight stairs down (north, bottom)",
    "Interroom straight stairs up (south, bottom)",
    "Interroom straight stairs down (south, bottom)",
    "Lamp cones",
    "Unknown",  // TODO: NEEDS IN GAME CHECKING
    "Liftable large block",
    "Agahnim's altar",
    "Agahnim's boss room",
    "Pot",
    "Unknown",  // TODO: NEEDS IN GAME CHECKING
    "Big chest",
    "Big chest (open)",
    "Intraroom stairs south (swim layer)",
    "Unknown",  // TODO: NEEDS IN GAME CHECKING
    "Unknown",  // TODO: NEEDS IN GAME CHECKING
    "Unknown",  // TODO: NEEDS IN GAME CHECKING
    "Unknown",  // TODO: NEEDS IN GAME CHECKING
    "Unknown",  // TODO: NEEDS IN GAME CHECKING
    "Unknown",  // TODO: NEEDS IN GAME CHECKING
    "Pipe end (south)",
    "Pipe end (north)",
    "Pipe end (east)",
    "Pipe end (west)",
    "Pipe corner ▛",
    "Pipe corner ▙",
    "Pipe corner ▜",
    "Pipe corner ▟",
    "Pipe-rock intersection ⯊",
    "Pipe-rock intersection ⯋",
    "Pipe-rock intersection ◖",
    "Pipe-rock intersection ◗",
    "Pipe crossover",
    "Bombable floor",
    "Fake bombable floor",
    "Unknown",  // TODO: NEEDS IN GAME CHECKING
    "Warp tile",
    "Tool rack",
    "Furnace",
    "Tub (wide)",
    "Anvil",
    "Warp tile (disabled)",
    "Pressure plate",
    "Unknown",  // TODO: NEEDS IN GAME CHECKING
    "Blue peg",
    "Orange peg",
    "Fortune teller room",
    "Unknown",  // TODO: NEEDS IN GAME CHECKING
    "Bar corner ▛",
    "Bar corner ▙",
    "Bar corner ▜",
    "Bar corner ▟",
    "Decorative bowl",
    "Tub (tall)",
    "Bookcase",
    "Range",
    "Suitcase",
    "Bar bottles",
    "Arrow game hole (west)",
    "Arrow game hole (east)",
    "Vitreous goo gfx",
    "Fake pressure plate",
    "Medusa head",
    "4-way shooter block",
    "Pit",
    "Wall crack (north)",
    "Wall crack (south)",
    "Wall crack (west)",
    "Wall crack (east)",
    "Large decor",
    "Water grate (north)",
    "Water grate (south)",
    "Water grate (west)",
    "Water grate (east)",
    "Window sunlight",
    "Floor sunlight",
    "Trinexx's shell",
    "Layer 2 mask (full)",
    "Boss entrance",
    "Minigame chest",
    "Ganon door",
    "Triforce wall ornament",
    "Triforce floor tiles",
    "Freezor hole",
    "Pile of bones",
    "Vitreous goo damage",
    "Arrow tile ↑",
    "Arrow tile ↓",
    "Arrow tile →",
    "Nothing",
};

}  // namespace zelda3
}  // namespace yaze

#endif  // YAZE_APP_ZELDA3_DUNGEON_ROOM_OBJECT_H
