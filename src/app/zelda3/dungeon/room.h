#ifndef YAZE_APP_ZELDA3_DUNGEON_ROOM_H
#define YAZE_APP_ZELDA3_DUNGEON_ROOM_H

#include <yaze.h>

#include <cstdint>
#include <string_view>
#include <vector>

#include "app/rom.h"
#include "app/gfx/background_buffer.h"
#include "app/zelda3/dungeon/room_layout.h"
#include "app/zelda3/dungeon/room_object.h"
#include "app/zelda3/sprite/sprite.h"

namespace yaze {
namespace zelda3 {

// room_object_layout_pointer   0x882D
// room_object_pointer          0x874C
// 0x882D -> readlong() -> 2FEF04 (04EF2F -> toPC->026F2F) ->

// 47EF04 ; layout00 ptr
// AFEF04 ; layout01 ptr
// F0EF04 ; layout02 ptr
// 4CF004 ; layout03 ptr
// A8F004 ; layout04 ptr
// ECF004 ; layout05 ptr
// 48F104 ; layout06 ptr
// A4F104 ; layout07 ptr
// also they are not exactly the same as rooms
// the object array is terminated by a 0xFFFF there's no layers
// in normal room when you encounter a 0xFFFF it goes to the next layer

constexpr int room_object_layout_pointer = 0x882D;
constexpr int room_object_pointer = 0x874C;  // Long pointer

constexpr int dungeons_main_bg_palette_pointers = 0xDEC4B;  // JP Same
constexpr int dungeons_palettes = 0xDD734;
constexpr int room_items_pointers = 0xDB69;     // JP 0xDB67
constexpr int rooms_sprite_pointer = 0x4C298;   // JP Same //2byte bank 09D62E
constexpr int kRoomHeaderPointer = 0xB5DD;      // LONG
constexpr int kRoomHeaderPointerBank = 0xB5E7;  // JP Same
constexpr int gfx_groups_pointer = 0x6237;
constexpr int chests_length_pointer = 0xEBF6;
constexpr int chests_data_pointer1 = 0xEBFB;

constexpr int messages_id_dungeon = 0x3F61D;

constexpr int blocks_length = 0x8896;  // Word value
constexpr int blocks_pointer1 = 0x15AFA;
constexpr int blocks_pointer2 = 0x15B01;
constexpr int blocks_pointer3 = 0x15B08;
constexpr int blocks_pointer4 = 0x15B0F;
constexpr int torch_data = 0x2736A;  // JP 0x2704A
constexpr int torches_length_pointer = 0x88C1;
constexpr int sprite_blockset_pointer = 0x5B57;

constexpr int sprites_data = 0x4D8B0;
constexpr int sprites_data_empty_room = 0x4D8AE;
constexpr int sprites_end_data = 0x4EC9E;
constexpr int pit_pointer = 0x394AB;
constexpr int pit_count = 0x394A6;
constexpr int doorPointers = 0xF83C0;

// doors
constexpr int door_gfx_up = 0x4D9E;
constexpr int door_gfx_down = 0x4E06;
constexpr int door_gfx_cavexit_down = 0x4E06;
constexpr int door_gfx_left = 0x4E66;
constexpr int door_gfx_right = 0x4EC6;
constexpr int door_pos_up = 0x197E;
constexpr int door_pos_down = 0x1996;
constexpr int door_pos_left = 0x19AE;
constexpr int door_pos_right = 0x19C6;

constexpr int dungeon_spr_ptrs = 0x090000;

constexpr int NumberOfRooms = 296;

constexpr uint16_t stairsObjects[] = {0x139, 0x138, 0x13B, 0x12E, 0x12D};

constexpr int tile_address = 0x001B52;
constexpr int tile_address_floor = 0x001B5A;

struct LayerMergeType {
  uint8_t ID;
  std::string Name;
  bool Layer2OnTop;
  bool Layer2Translucent;
  bool Layer2Visible;
  LayerMergeType() = default;
  LayerMergeType(uint8_t id, std::string name, bool see, bool top, bool trans) {
    ID = id;
    Name = name;
    Layer2OnTop = top;
    Layer2Translucent = trans;
    Layer2Visible = see;
  }
};

const static LayerMergeType LayerMerge00{0x00, "Off", true, false, false};
const static LayerMergeType LayerMerge01{0x01, "Parallax", true, false, false};
const static LayerMergeType LayerMerge02{0x02, "Dark", true, true, true};
const static LayerMergeType LayerMerge03{0x03, "On top", true, true, false};
const static LayerMergeType LayerMerge04{0x04, "Translucent", true, true, true};
const static LayerMergeType LayerMerge05{0x05, "Addition", true, true, true};
const static LayerMergeType LayerMerge06{0x06, "Normal", true, false, false};
const static LayerMergeType LayerMerge07{0x07, "Transparent", true, true, true};
const static LayerMergeType LayerMerge08{0x08, "Dark room", true, true, true};

const static LayerMergeType kLayerMergeTypeList[] = {
    LayerMerge00, LayerMerge01, LayerMerge02, LayerMerge03, LayerMerge04,
    LayerMerge05, LayerMerge06, LayerMerge07, LayerMerge08};

enum CollisionKey {
  One_Collision,
  Both,
  Both_With_Scroll,
  Moving_Floor_Collision,
  Moving_Water_Collision,
};

enum EffectKey {
  Effect_Nothing,
  One,
  Moving_Floor,
  Moving_Water,
  Four,
  Red_Flashes,
  Torch_Show_Floor,
  Ganon_Room,
};

enum TagKey {
  Nothing,
  NW_Kill_Enemy_to_Open,
  NE_Kill_Enemy_to_Open,
  SW_Kill_Enemy_to_Open,
  SE_Kill_Enemy_to_Open,
  W_Kill_Enemy_to_Open,
  E_Kill_Enemy_to_Open,
  N_Kill_Enemy_to_Open,
  S_Kill_Enemy_to_Open,
  Clear_Quadrant_to_Open,
  Clear_Room_to_Open,
  NW_Push_Block_to_Open,
  NE_Push_Block_to_Open,
  SW_Push_Block_to_Open,
  SE_Push_Block_to_Open,
  W_Push_Block_to_Open,
  E_Push_Block_to_Open,
  N_Push_Block_to_Open,
  S_Push_Block_to_Open,
  Push_Block_to_Open,
  Pull_Lever_to_Open,
  Clear_Level_to_Open,
  Switch_Open_Door_Hold,
  Switch_Open_Door_Toggle,
  Turn_off_Water,
  Turn_on_Water,
  Water_Gate,
  Water_Twin,
  Secret_Wall_Right,
  Secret_Wall_Left,
  Crash1,
  Crash2,
  Pull_Switch_to_bomb_Wall,
  Holes_0,
  Open_Chest_Activate_Holes_0,
  Holes_1,
  Holes_2,
  Kill_Enemy_to_clear_level,
  SE_Kill_Enemy_to_Move_Block,
  Trigger_activated_Chest,
  Pull_lever_to_Bomb_Wall,
  NW_Kill_Enemy_for_Chest,
  NE_Kill_Enemy_for_Chest,
  SW_Kill_Enemy_for_Chest,
  SE_Kill_Enemy_for_Chest,
  W_Kill_Enemy_for_Chest,
  E_Kill_Enemy_for_Chest,
  N_Kill_Enemy_for_Chest,
  S_Kill_Enemy_for_Chest,
  Clear_Quadrant_for_Chest,
  Clear_Room_for_Chest,
  Light_Torches_to_Open,
  Holes_3,
  Holes_4,
  Holes_5,
  Holes_6,
  Agahnim_Room,
  Holes_7,
  Holes_8,
  Open_Chest_for_Holes_8,
  Push_Block_for_Chest,
  Kill_to_open_Ganon_Door,
  Light_Torches_to_get_Chest,
  Kill_boss_Again
};

class Room {
 public:
  Room() = default;
  Room(int room_id, Rom* rom) : room_id_(room_id), rom_(rom), layout_(rom) {}

  void LoadRoomGraphics(uint8_t entrance_blockset = 0xFF);
  void CopyRoomGraphicsToBuffer();
  void LoadGraphicsSheetsIntoArena();
  void RenderRoomGraphics();
  void RenderObjectsToBackground();
  void LoadAnimatedGraphics();
  void LoadObjects();
  void LoadSprites();
  void LoadChests();
  void LoadRoomLayout();
  void LoadDoors();
  void LoadTorches();
  void LoadBlocks();
  void LoadPits();

  const RoomLayout& GetLayout() const { return layout_; }
  RoomLayout& GetLayout() { return layout_; }

  // Public getters and manipulators for sprites
  const std::vector<zelda3::Sprite>& GetSprites() const { return sprites_; }
  std::vector<zelda3::Sprite>& GetSprites() { return sprites_; }

  // Public getters and manipulators for chests
  const std::vector<chest_data>& GetChests() const { return chests_in_room_; }
  std::vector<chest_data>& GetChests() { return chests_in_room_; }

  // Public getters and manipulators for stairs
  const std::vector<staircase>& GetStairs() const { return z3_staircases_; }
  std::vector<staircase>& GetStairs() { return z3_staircases_; }


  // Public getters and manipulators for tile objects
  const std::vector<RoomObject>& GetTileObjects() const {
    return tile_objects_;
  }
  std::vector<RoomObject>& GetTileObjects() { return tile_objects_; }

  // Methods for modifying tile objects
  void ClearTileObjects() { tile_objects_.clear(); }
  void AddTileObject(const RoomObject& object) {
    tile_objects_.push_back(object);
  }
  
  // Enhanced object manipulation (Phase 3)
  absl::Status AddObject(const RoomObject& object);
  absl::Status RemoveObject(size_t index);
  absl::Status UpdateObject(size_t index, const RoomObject& object);
  absl::StatusOr<size_t> FindObjectAt(int x, int y, int layer) const;
  bool ValidateObject(const RoomObject& object) const;
  void RemoveTileObject(size_t index) {
    if (index < tile_objects_.size()) {
      tile_objects_.erase(tile_objects_.begin() + index);
    }
  }
  size_t GetTileObjectCount() const { return tile_objects_.size(); }
  RoomObject& GetTileObject(size_t index) { return tile_objects_[index]; }
  const RoomObject& GetTileObject(size_t index) const {
    return tile_objects_[index];
  }

  // For undo/redo functionality
  void SetTileObjects(const std::vector<RoomObject>& objects) {
    tile_objects_ = objects;
  }

  // Public setters for LoadRoomFromRom function
  void SetBg2(background2 bg2) { bg2_ = bg2; }
  void SetCollision(CollisionKey collision) { collision_ = collision; }
  void SetIsLight(bool is_light) { is_light_ = is_light; }
  void SetPalette(uint8_t palette) { this->palette = palette; }
  void SetBlockset(uint8_t blockset) { this->blockset = blockset; }
  void SetSpriteset(uint8_t spriteset) { this->spriteset = spriteset; }
  void SetEffect(EffectKey effect) { effect_ = effect; }
  void SetTag1(TagKey tag1) { tag1_ = tag1; }
  void SetTag2(TagKey tag2) { tag2_ = tag2; }
  void SetStaircasePlane(int index, uint8_t plane) {
    if (index >= 0 && index < 4) staircase_plane_[index] = plane;
  }
  void SetHolewarp(uint8_t holewarp) { this->holewarp = holewarp; }
  void SetStaircaseRoom(int index, uint8_t room) {
    if (index >= 0 && index < 4) staircase_rooms_[index] = room;
  }
  // SetFloor1/SetFloor2 removed - use set_floor1()/set_floor2() instead (defined above)
  void SetMessageId(uint16_t message_id) { message_id_ = message_id; }

  // Getters for LoadRoomFromRom function
  bool IsLight() const { return is_light_; }

  // Additional setters for LoadRoomFromRom function
  void SetMessageIdDirect(uint16_t message_id) { message_id_ = message_id; }
  void SetLayer2Mode(uint8_t mode) { layer2_mode_ = mode; }
  void SetLayerMerging(LayerMergeType merging) { layer_merging_ = merging; }
  void SetIsDark(bool is_dark) { is_dark_ = is_dark; }
  void SetPaletteDirect(uint8_t palette) { palette_ = palette; }
  void SetBackgroundTileset(uint8_t tileset) { background_tileset_ = tileset; }
  void SetSpriteTileset(uint8_t tileset) { sprite_tileset_ = tileset; }
  void SetLayer2Behavior(uint8_t behavior) { layer2_behavior_ = behavior; }
  void SetTag1Direct(TagKey tag1) { tag1_ = tag1; }
  void SetTag2Direct(TagKey tag2) { tag2_ = tag2; }
  void SetPitsTargetLayer(uint8_t layer) { pits_.target_layer = layer; }
  void SetStair1TargetLayer(uint8_t layer) { stair1_.target_layer = layer; }
  void SetStair2TargetLayer(uint8_t layer) { stair2_.target_layer = layer; }
  void SetStair3TargetLayer(uint8_t layer) { stair3_.target_layer = layer; }
  void SetStair4TargetLayer(uint8_t layer) { stair4_.target_layer = layer; }
  void SetPitsTarget(uint8_t target) { pits_.target = target; }
  void SetStair1Target(uint8_t target) { stair1_.target = target; }
  void SetStair2Target(uint8_t target) { stair2_.target = target; }
  void SetStair3Target(uint8_t target) { stair3_.target = target; }
  void SetStair4Target(uint8_t target) { stair4_.target = target; }
  
  // Loaded state
  bool IsLoaded() const { return is_loaded_; }
  void SetLoaded(bool loaded) { is_loaded_ = loaded; }

  // Read-only accessors for metadata
  EffectKey effect() const { return effect_; }
  TagKey tag1() const { return tag1_; }
  TagKey tag2() const { return tag2_; }
  CollisionKey collision() const { return collision_; }
  const LayerMergeType& layer_merging() const { return layer_merging_; }

  int id() const { return room_id_; }

  uint8_t blockset = 0;
  uint8_t spriteset = 0;
  uint8_t palette = 0;
  uint8_t layout = 0;
  uint8_t holewarp = 0;
  // NOTE: floor1/floor2 removed - use floor1() and floor2() accessors instead
  // Floor graphics are now private (floor1_graphics_, floor2_graphics_)
  uint16_t message_id_ = 0;
  
  // Floor graphics accessors (use these instead of direct members!)
  uint8_t floor1() const { return floor1_graphics_; }
  uint8_t floor2() const { return floor2_graphics_; }
  void set_floor1(uint8_t value) { 
    floor1_graphics_ = value;
    // TODO: Trigger re-render if needed
  }
  void set_floor2(uint8_t value) { 
    floor2_graphics_ = value;
    // TODO: Trigger re-render if needed
  }
  // Enhanced object parsing methods
  void ParseObjectsFromLocation(int objects_location);
  void HandleSpecialObjects(short oid, uint8_t posX, uint8_t posY,
                            int& nbr_of_staircase);
  
  // Object saving (Phase 1, Task 1.3)
  absl::Status SaveObjects();
  std::vector<uint8_t> EncodeObjects() const;

  auto blocks() const { return blocks_; }
  auto& mutable_blocks() { return blocks_; }
  auto rom() { return rom_; }
  auto mutable_rom() { return rom_; }
  const std::array<uint8_t, 0x4000>& get_gfx_buffer() const { return current_gfx16_; }
  
  // Per-room background buffers (not shared via arena!)
  auto& bg1_buffer() { return bg1_buffer_; }
  auto& bg2_buffer() { return bg2_buffer_; }
  const auto& bg1_buffer() const { return bg1_buffer_; }
  const auto& bg2_buffer() const { return bg2_buffer_; }

 private:
  Rom* rom_;

  std::array<uint8_t, 0x4000> current_gfx16_;
  
  // Each room has its OWN background buffers and bitmaps
  gfx::BackgroundBuffer bg1_buffer_{512, 512};
  gfx::BackgroundBuffer bg2_buffer_{512, 512};

  bool is_light_;
  bool is_loaded_ = false;
  bool is_dark_;
  bool is_floor_ = true;

  int room_id_;
  int animated_frame_;

  uint8_t staircase_plane_[4];
  uint8_t staircase_rooms_[4];

  uint8_t background_tileset_;
  uint8_t sprite_tileset_;
  uint8_t layer2_behavior_;
  uint8_t palette_;
  uint8_t floor1_graphics_;
  uint8_t floor2_graphics_;
  uint8_t layer2_mode_;

  std::array<uint8_t, 16> blocks_;
  std::array<chest, 16> chest_list_;

  std::vector<RoomObject> tile_objects_;
  // TODO: add separate door objects list when door section (F0 FF) is parsed
  std::vector<zelda3::Sprite> sprites_;
  std::vector<staircase> z3_staircases_;
  std::vector<chest_data> chests_in_room_;

  // Room layout system for walls, floors, and structural elements
  RoomLayout layout_;

  LayerMergeType layer_merging_;
  CollisionKey collision_;
  EffectKey effect_;
  TagKey tag1_;
  TagKey tag2_;

  background2 bg2_;
  destination pits_;
  destination stair1_;
  destination stair2_;
  destination stair3_;
  destination stair4_;
};

// Loads a room from the ROM.
Room LoadRoomFromRom(Rom* rom, int room_id);

struct RoomSize {
  int64_t room_size_pointer;
  int64_t room_size;
};

// Calculates the size of a room in the ROM.
RoomSize CalculateRoomSize(Rom* rom, int room_id);

static const std::string RoomEffect[] = {"Nothing",
                                         "Nothing",
                                         "Moving Floor",
                                         "Moving Water",
                                         "Trinexx Shell",
                                         "Red Flashes",
                                         "Light Torch to See Floor",
                                         "Ganon's Darkness"};

constexpr std::string_view kRoomNames[] = {
    "Ganon",
    "Hyrule Castle (North Corridor)",
    "Behind Sanctuary (Switch)",
    "Houlihan",
    "Turtle Rock (Crysta-Roller)",
    "Empty",
    "Swamp Palace (Arrghus[Boss])",
    "Tower of Hera (Moldorm[Boss])",
    "Cave (Healing Fairy)",
    "Palace of Darkness",
    "Palace of Darkness (Stalfos Trap)",
    "Palace of Darkness (Turtle)",
    "Ganon's Tower (Entrance)",
    "Ganon's Tower (Agahnim2[Boss])",
    "Ice Palace (Entrance )",
    "Empty Clone ",
    "Ganon Evacuation Route",
    "Hyrule Castle (Bombable Stock )",
    "Sanctuary",
    "Turtle Rock (Hokku-Bokku Key 2)",
    "Turtle Rock (Big Key )",
    "Turtle Rock",
    "Swamp Palace (Swimming Treadmill)",
    "Tower of Hera (Moldorm Fall )",
    "Cave",
    "Palace of Darkness (Dark Maze)",
    "Palace of Darkness (Big Chest )",
    "Palace of Darkness (Mimics / Moving Wall )",
    "Ganon's Tower (Ice Armos)",
    "Ganon's Tower (Final Hallway)",
    "Ice Palace (Bomb Floor / Bari )",
    "Ice Palace (Pengator / Big Key )",
    "Agahnim's Tower (Agahnim[Boss])",
    "Hyrule Castle (Key-rat )",
    "Hyrule Castle (Sewer Text Trigger )",
    "Turtle Rock (West Exit to Balcony)",
    "Turtle Rock (Double Hokku-Bokku / Big chest )",
    "Empty Clone ",
    "Swamp Palace (Statue )",
    "Tower of Hera (Big Chest)",
    "Swamp Palace (Entrance )",
    "Skull Woods (Mothula[Boss])",
    "Palace of Darkness (Big Hub )",
    "Palace of Darkness (Map Chest / Fairy )",
    "Cave",
    "Empty Clone ",
    "Ice Palace (Compass )",
    "Cave (Kakariko Well HP)",
    "Agahnim's Tower (Maiden Sacrifice Chamber)",
    "Tower of Hera (Hardhat Beetles )",
    "Hyrule Castle (Sewer Key Chest )",
    "Desert Palace (Lanmolas[Boss])",
    "Swamp Palace (Push Block Puzzle / Pre-Big Key )",
    "Swamp Palace (Big Key / BS )",
    "Swamp Palace (Big Chest )",
    "Swamp Palace (Map Chest / Water Fill )",
    "Swamp Palace (Key Pot )",
    "Skull Woods (Gibdo Key / Mothula Hole )",
    "Palace of Darkness (Bombable Floor )",
    "Palace of Darkness (Spike Block / Conveyor )",
    "Cave",
    "Ganon's Tower (Torch 2)",
    "Ice Palace (Stalfos Knights / Conveyor Hellway)",
    "Ice Palace (Map Chest )",
    "Agahnim's Tower (Final Bridge )",
    "Hyrule Castle (First Dark )",
    "Hyrule Castle (6 Ropes )",
    "Desert Palace (Torch Puzzle / Moving Wall )",
    "Thieves Town (Big Chest )",
    "Thieves Town (Jail Cells )",
    "Swamp Palace (Compass Chest )",
    "Empty Clone ",
    "Empty Clone ",
    "Skull Woods (Gibdo Torch Puzzle )",
    "Palace of Darkness (Entrance )",
    "Palace of Darkness (Warps / South Mimics )",
    "Ganon's Tower (Mini-Helmasaur Conveyor )",
    "Ganon's Tower (Moldorm )",
    "Ice Palace (Bomb-Jump )",
    "Ice Palace Clone (Fairy )",
    "Hyrule Castle (West Corridor)",
    "Hyrule Castle (Throne )",
    "Hyrule Castle (East Corridor)",
    "Desert Palace (Popos 2 / Beamos Hellway )",
    "Swamp Palace (Upstairs Pits )",
    "Castle Secret Entrance / Uncle Death ",
    "Skull Woods (Key Pot / Trap )",
    "Skull Woods (Big Key )",
    "Skull Woods (Big Chest )",
    "Skull Woods (Final Section Entrance )",
    "Palace of Darkness (Helmasaur King[Boss])",
    "Ganon's Tower (Spike Pit )",
    "Ganon's Tower (Ganon-Ball Z)",
    "Ganon's Tower (Gauntlet 1/2/3)",
    "Ice Palace (Lonely Firebar)",
    "Ice Palace (Hidden Chest / Spike Floor )",
    "Hyrule Castle (West Entrance )",
    "Hyrule Castle (Main Entrance )",
    "Hyrule Castle (East Entrance )",
    "Desert Palace (Final Section Entrance )",
    "Thieves Town (West Attic )",
    "Thieves Town (East Attic )",
    "Swamp Palace (Hidden Chest / Hidden Door )",
    "Skull Woods (Compass Chest )",
    "Skull Woods (Key Chest / Trap )",
    "Empty Clone ",
    "Palace of Darkness (Rupee )",
    "Ganon's Tower (Mimics s)",
    "Ganon's Tower (Lanmolas )",
    "Ganon's Tower (Gauntlet 4/5)",
    "Ice Palace (Pengators )",
    "Empty Clone ",
    "Hyrule Castle (Small Corridor to Jail Cells)",
    "Hyrule Castle (Boomerang Chest )",
    "Hyrule Castle (Map Chest )",
    "Desert Palace (Big Chest )",
    "Desert Palace (Map Chest )",
    "Desert Palace (Big Key Chest )",
    "Swamp Palace (Water Drain )",
    "Tower of Hera (Entrance )",
    "Empty Clone ",
    "Empty Clone ",
    "Empty Clone ",
    "Ganon's Tower",
    "Ganon's Tower (East Side Collapsing Bridge / Exploding Wall )",
    "Ganon's Tower (Winder / Warp Maze )",
    "Ice Palace (Hidden Chest / Bombable Floor )",
    "Ice Palace ( Big Spike Traps )",
    "Hyrule Castle (Jail Cell )",
    "Hyrule Castle",
    "Hyrule Castle (Basement Chasm )",
    "Desert Palace (West Entrance )",
    "Desert Palace (Main Entrance )",
    "Desert Palace (East Entrance )",
    "Empty Clone ",
    "Tower of Hera (Tile )",
    "Empty Clone ",
    "Eastern Palace (Fairy )",
    "Empty Clone ",
    "Ganon's Tower (Block Puzzle / Spike Skip / Map Chest )",
    "Ganon's Tower (East and West Downstairs / Big Chest )",
    "Ganon's Tower (Tile / Torch Puzzle )",
    "Ice Palace",
    "Empty Clone ",
    "Misery Mire (Vitreous[Boss])",
    "Misery Mire (Final Switch )",
    "Misery Mire (Dark Bomb Wall / Switches )",
    "Misery Mire (Dark Cane Floor Switch Puzzle )",
    "Empty Clone ",
    "Ganon's Tower (Final Collapsing Bridge )",
    "Ganon's Tower (Torches 1 )",
    "Misery Mire (Torch Puzzle / Moving Wall )",
    "Misery Mire (Entrance )",
    "Eastern Palace (Eyegore Key )",
    "Empty Clone ",
    "Ganon's Tower (Many Spikes / Warp Maze )",
    "Ganon's Tower (Invisible Floor Maze )",
    "Ganon's Tower (Compass Chest / Invisible Floor )",
    "Ice Palace (Big Chest )",
    "Ice Palace",
    "Misery Mire (Pre-Vitreous )",
    "Misery Mire (Fish )",
    "Misery Mire (Bridge Key Chest )",
    "Misery Mire",
    "Turtle Rock (Trinexx[Boss])",
    "Ganon's Tower (Wizzrobes s)",
    "Ganon's Tower (Moldorm Fall )",
    "Tower of Hera (Fairy )",
    "Eastern Palace (Stalfos Spawn )",
    "Eastern Palace (Big Chest )",
    "Eastern Palace (Map Chest )",
    "Thieves Town (Moving Spikes / Key Pot )",
    "Thieves Town (Blind The Thief[Boss])",
    "Empty Clone ",
    "Ice Palace",
    "Ice Palace (Ice Bridge )",
    "Agahnim's Tower (Circle of Pots)",
    "Misery Mire (Hourglass )",
    "Misery Mire (Slug )",
    "Misery Mire (Spike Key Chest )",
    "Turtle Rock (Pre-Trinexx )",
    "Turtle Rock (Dark Maze)",
    "Turtle Rock (Chain Chomps )",
    "Turtle Rock (Map Chest / Key Chest / Roller )",
    "Eastern Palace (Big Key )",
    "Eastern Palace (Lobby Cannonballs )",
    "Eastern Palace (Dark Antifairy / Key Pot )",
    "Thieves Town (Hellway)",
    "Thieves Town (Conveyor Toilet)",
    "Empty Clone ",
    "Ice Palace (Block Puzzle )",
    "Ice Palace Clone (Switch )",
    "Agahnim's Tower (Dark Bridge )",
    "Misery Mire (Compass Chest / Tile )",
    "Misery Mire (Big Hub )",
    "Misery Mire (Big Chest )",
    "Turtle Rock (Final Crystal Switch Puzzle )",
    "Turtle Rock (Laser Bridge)",
    "Turtle Rock",
    "Turtle Rock (Torch Puzzle)",
    "Eastern Palace (Armos Knights[Boss])",
    "Eastern Palace (Entrance )",
    "??",
    "Thieves Town (North West Entrance )",
    "Thieves Town (North East Entrance )",
    "Empty Clone ",
    "Ice Palace (Hole to Kholdstare )",
    "Empty Clone ",
    "Agahnim's Tower (Dark Maze)",
    "Misery Mire (Conveyor Slug / Big Key )",
    "Misery Mire (Mire02 / Wizzrobes )",
    "Empty Clone ",
    "Empty Clone ",
    "Turtle Rock (Laser Key )",
    "Turtle Rock (Entrance )",
    "Empty Clone ",
    "Eastern Palace (Zeldagamer / Pre-Armos Knights )",
    "Eastern Palace (Canonball ",
    "Eastern Palace",
    "Thieves Town (Main (South West) Entrance )",
    "Thieves Town (South East Entrance )",
    "Empty Clone ",
    "Ice Palace (Kholdstare[Boss])",
    "Cave",
    "Agahnim's Tower (Entrance )",
    "Cave (Lost Woods HP)",
    "Cave (Lumberjack's Tree HP)",
    "Cave (1/2 Magic)",
    "Cave (Lost Old Man Final Cave)",
    "Cave (Lost Old Man Final Cave)",
    "Cave",
    "Cave",
    "Cave",
    "Empty Clone ",
    "Cave (Spectacle Rock HP)",
    "Cave",
    "Empty Clone ",
    "Cave",
    "Cave (Spiral Cave)",
    "Cave (Crystal Switch / 5 Chests )",
    "Cave (Lost Old Man Starting Cave)",
    "Cave (Lost Old Man Starting Cave)",
    "House",
    "House (Old Woman (Sahasrahla's Wife?))",
    "House (Angry Brothers)",
    "House (Angry Brothers)",
    "Empty Clone ",
    "Empty Clone ",
    "Cave",
    "Cave",
    "Cave",
    "Cave",
    "Empty Clone ",
    "Cave",
    "Cave",
    "Cave",

    "Chest Minigame",
    "Houses",
    "Sick Boy house",
    "Tavern",
    "Link's House",
    "Sarashrala Hut",
    "Chest Minigame",
    "Library",
    "Chicken House",
    "Witch Shop",
    "A Aginah's Cave",
    "Dam",
    "Mimic Cave",
    "Mire Shed",
    "Cave",
    "Shop",
    "Shop",
    "Archery Minigame",
    "DW Church/Shop",
    "Grave Cave",
    "Fairy Fountain",
    "Fairy Upgrade",
    "Pyramid Fairy",
    "Spike Cave",
    "Chest Minigame",
    "Blind Hut",
    "Bonzai Cave",
    "Circle of bush Cave",
    "Big Bomb Shop, C-House",
    "Blind Hut 2",
    "Hype Cave",
    "Shop",
    "Ice Cave",
    "Smith",
    "Fortune Teller",
    "MiniMoldorm Cave",
    "Under Rock Caves",
    "Smith",
    "Cave",
    "Mazeblock Cave",
    "Smith Peg Cave"};

static const std::string RoomTag[] = {"Nothing",
                                      "NW Kill Enemy to Open",
                                      "NE Kill Enemy to Open",
                                      "SW Kill Enemy to Open",
                                      "SE Kill Enemy to Open",
                                      "W Kill Enemy to Open",
                                      "E Kill Enemy to Open",
                                      "N Kill Enemy to Open",
                                      "S Kill Enemy to Open",
                                      "Clear Quadrant to Open",
                                      "Clear Full Tile to Open",

                                      "NW Push Block to Open",
                                      "NE Push Block to Open",
                                      "SW Push Block to Open",
                                      "SE Push Block to Open",
                                      "W Push Block to Open",
                                      "E Push Block to Open",
                                      "N Push Block to Open",
                                      "S Push Block to Open",
                                      "Push Block to Open",
                                      "Pull Lever to Open",
                                      "Collect Prize to Open",

                                      "Hold Switch Open Door",
                                      "Toggle Switch to Open Door",
                                      "Turn off Water",
                                      "Turn on Water",
                                      "Water Gate",
                                      "Water Twin",
                                      "Moving Wall Right",
                                      "Moving Wall Left",
                                      "Crash",
                                      "Crash",
                                      "Push Switch Exploding Wall",
                                      "Holes 0",
                                      "Open Chest (Holes 0)",
                                      "Holes 1",
                                      "Holes 2",
                                      "Defeat Boss for Dungeon Prize",

                                      "SE Kill Enemy to Push Block",
                                      "Trigger Switch Chest",
                                      "Pull Lever Exploding Wall",
                                      "NW Kill Enemy for Chest",
                                      "NE Kill Enemy for Chest",
                                      "SW Kill Enemy for Chest",
                                      "SE Kill Enemy for Chest",
                                      "W Kill Enemy for Chest",
                                      "E Kill Enemy for Chest",
                                      "N Kill Enemy for Chest",
                                      "S Kill Enemy for Chest",
                                      "Clear Quadrant for Chest",
                                      "Clear Full Tile for Chest",

                                      "Light Torches to Open",
                                      "Holes 3",
                                      "Holes 4",
                                      "Holes 5",
                                      "Holes 6",
                                      "Agahnim Room",
                                      "Holes 7",
                                      "Holes 8",
                                      "Open Chest for Holes 8",
                                      "Push Block for Chest",
                                      "Clear Room for Triforce Door",
                                      "Light Torches for Chest",
                                      "Kill Boss Again"};

}  // namespace zelda3
}  // namespace yaze

#endif
