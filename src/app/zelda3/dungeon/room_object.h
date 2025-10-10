#ifndef YAZE_APP_ZELDA3_DUNGEON_ROOM_OBJECT_H
#define YAZE_APP_ZELDA3_DUNGEON_ROOM_OBJECT_H

#include <cstdint>
#include <string>
#include <vector>

#include "app/gfx/snes_tile.h"
#include "app/rom.h"
#include "app/zelda3/dungeon/object_parser.h"

namespace yaze {
namespace zelda3 {

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
        height_(16),
        rom_(nullptr) {}

  void set_rom(Rom* rom) { rom_ = rom; }
  auto rom() { return rom_; }
  auto mutable_rom() { return rom_; }
  
  // Position setters and getters
  void set_x(uint8_t x) { x_ = x; }
  void set_y(uint8_t y) { y_ = y; }
  void set_size(uint8_t size) { size_ = size; }
  uint8_t x() const { return x_; }
  uint8_t y() const { return y_; }
  uint8_t size() const { return size_; }

  // Ensures tiles_ is populated with a basic set based on ROM tables so we can
  // preview/draw objects without needing full emulator execution.
  void EnsureTilesLoaded();
  
  // Load tiles using the new ObjectParser
  absl::Status LoadTilesWithParser();

  // Getter for tiles
  const std::vector<gfx::TileInfo>& tiles() const { return tiles_; }
  std::vector<gfx::TileInfo>& mutable_tiles() { return tiles_; }

  // Get tile data through Arena system - returns references, not copies
  absl::StatusOr<std::span<const gfx::TileInfo>> GetTiles() const;
  
  // Get individual tile by index - uses Arena lookup
  absl::StatusOr<const gfx::TileInfo*> GetTile(int index) const;
  
  // Get tile count without loading all tiles
  int GetTileCount() const;

  // ============================================================================
  // Object Encoding/Decoding (Phase 1, Task 1.1)
  // ============================================================================
  
  // 3-byte object encoding structure
  struct ObjectBytes {
    uint8_t b1;
    uint8_t b2;
    uint8_t b3;
  };
  
  // Decode object from 3-byte ROM format
  // Type1: xxxxxxss yyyyyyss iiiiiiii (ID 0x00-0xFF)
  // Type2: 111111xx xxxxyyyy yyiiiiii (ID 0x100-0x1FF)
  // Type3: xxxxxxii yyyyyyii 11111iii (ID 0xF00-0xFFF)
  static RoomObject DecodeObjectFromBytes(uint8_t b1, uint8_t b2, uint8_t b3,
                                          uint8_t layer);
  
  // Encode object to 3-byte ROM format
  ObjectBytes EncodeObjectToBytes() const;
  
  // Determine object type from bytes (1, 2, or 3)
  static int DetermineObjectType(uint8_t b1, uint8_t b3);
  
  // Get layer from LayerType enum
  uint8_t GetLayerValue() const { return static_cast<uint8_t>(layer_); }
  
  // ============================================================================

  // NOTE: Legacy ZScream methods removed. Modern rendering uses:
  // - ObjectParser for loading tiles from ROM
  // - ObjectDrawer for rendering tiles to BackgroundBuffer

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
  // Size nibble bits captured from object encoding (0..3 each) for heuristic
  // orientation and sizing decisions.
  uint8_t size_x_bits_ = 0;
  uint8_t size_y_bits_ = 0;

  int width_;
  int height_;
  int offset_x_ = 0;
  int offset_y_ = 0;

  std::string name_;

  std::vector<uint8_t> preview_object_data_;
  
  // Tile data storage - using Arena system for efficient memory management
  // Instead of copying Tile16 vectors, we store references to Arena-managed data
  mutable std::vector<gfx::TileInfo> tiles_; // Individual tiles like ZScream
  mutable bool tiles_loaded_ = false;
  mutable int tile_count_ = 0;
  mutable int tile_data_ptr_ = -1; // Pointer to tile data in ROM

  LayerType layer_;
  ObjectOption options_ = ObjectOption::Nothing;

  Rom* rom_;
};

// NOTE: Legacy Subtype1, Subtype2, Subtype3 classes removed.
// These were ported from ZScream but are no longer used.
// Modern code uses: ObjectParser + ObjectDrawer + ObjectRenderer

constexpr static inline const char* Type1RoomObjectNames[] = {
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

constexpr static inline const char* Type2RoomObjectNames[] = {
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

constexpr static inline const char* Type3RoomObjectNames[] = {
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
