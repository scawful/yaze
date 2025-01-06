#ifndef YAZE_APP_ZELDA3_DUNGEON_ROOM_H
#define YAZE_APP_ZELDA3_DUNGEON_ROOM_H

#include <dungeon.h>

#include <cstdint>
#include <string_view>
#include <vector>

#include "app/core/constants.h"
#include "app/gfx/bitmap.h"
#include "app/rom.h"
#include "app/zelda3/dungeon/room_object.h"
#include "app/zelda3/dungeon/room_tag.h"
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

constexpr ushort stairsObjects[] = {0x139, 0x138, 0x13B, 0x12E, 0x12D};

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

class Room : public SharedRom {
 public:
  Room() = default;
  Room(int room_id) : room_id_(room_id) {}
  ~Room() = default;

  void LoadHeader();
  void CalculateRoomSize();
  void LoadRoomFromROM();

  void LoadRoomGraphics(uchar entrance_blockset = 0xFF);
  void CopyRoomGraphicsToBuffer();
  void LoadAnimatedGraphics();

  void LoadObjects();
  void LoadSprites();
  void LoadChests();

  auto blocks() const { return blocks_; }
  auto &mutable_blocks() { return blocks_; }
  auto layer1() const { return background_bmps_[0]; }
  auto layer2() const { return background_bmps_[1]; }
  auto layer3() const { return background_bmps_[2]; }
  auto room_size() const { return room_size_; }
  auto room_size_ptr() const { return room_size_pointer_; }
  auto set_room_size(uint64_t size) { room_size_ = size; }

  uint8_t blockset = 0;
  uint8_t spriteset = 0;
  uint8_t palette = 0;
  uint8_t layout = 0;
  uint8_t holewarp = 0;
  uint8_t floor1 = 0;
  uint8_t floor2 = 0;

  uint16_t message_id_ = 0;

  gfx::Bitmap current_graphics_;
  std::vector<uint8_t> bg1_buffer_;
  std::vector<uint8_t> bg2_buffer_;
  std::vector<uint8_t> current_gfx16_;

 private:
  bool is_light_;
  bool is_loaded_;
  bool is_dark_;
  bool is_floor_;

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

  uint64_t room_size_;
  int64_t room_size_pointer_;

  std::array<uint8_t, 16> blocks_;
  std::array<chest, 16> chest_list_;

  std::array<gfx::Bitmap, 3> background_bmps_;
  std::vector<RoomObject> tile_objects_;
  std::vector<zelda3::Sprite> sprites_;
  std::vector<staircase> z3_staircases_;
  std::vector<chest_data> chests_in_room_;

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

}  // namespace zelda3
}  // namespace yaze

#endif
