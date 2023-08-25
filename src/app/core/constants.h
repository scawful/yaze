#ifndef YAZE_APP_CORE_CONSTANTS_H
#define YAZE_APP_CORE_CONSTANTS_H

#include <vector>

#include "absl/strings/string_view.h"

#define BASIC_BUTTON(w) if (ImGui::Button(w))

#define TAB_BAR(w) if (ImGui::BeginTabBar(w)) {
#define END_TAB_BAR() \
  ImGui::EndTabBar(); \
  }

#define TAB_ITEM(w) if (ImGui::BeginTabItem(w)) {
#define END_TAB_ITEM() \
  ImGui::EndTabItem(); \
  }

#define MENU_BAR() if (ImGui::BeginMenuBar()) {
#define END_MENU_BAR() \
  ImGui::EndMenuBar(); \
  }

#define MENU_ITEM(w) if (ImGui::MenuItem(w))
#define MENU_ITEM2(w, v) if (ImGui::MenuItem(w, v))

#define BUTTON_COLUMN(w)    \
  ImGui::TableNextColumn(); \
  ImGui::Button(w);

#define TEXT_COLUMN(w)      \
  ImGui::TableNextColumn(); \
  ImGui::Text(w);

#define BEGIN_TABLE(l, n, f) if (ImGui::BeginTable(l, n, f, ImVec2(0, 0))) {
#define SETUP_COLUMN(l) ImGui::TableSetupColumn(l);

#define TABLE_HEADERS()     \
  ImGui::TableHeadersRow(); \
  ImGui::TableNextRow();

#define NEXT_COLUMN() ImGui::TableNextColumn();

#define END_TABLE()  \
  ImGui::EndTable(); \
  }

#define PRINT_IF_ERROR(expression)                \
  {                                               \
    auto error = expression;                      \
    if (!error.ok()) {                            \
      std::cout << error.ToString() << std::endl; \
    }                                             \
  }

#define EXIT_IF_ERROR(expression)                 \
  {                                               \
    auto error = expression;                      \
    if (!error.ok()) {                            \
      std::cout << error.ToString() << std::endl; \
      return EXIT_FAILURE;                        \
    }                                             \
  }

#define RETURN_IF_ERROR(expression) \
  {                                 \
    auto error = expression;        \
    if (!error.ok()) {              \
      return error;                 \
    }                               \
  }

#define ASSIGN_OR_RETURN(type_variable_name, expression)         \
  ASSIGN_OR_RETURN_IMPL(APPEND_NUMBER(error_or_value, __LINE__), \
                        type_variable_name, expression)

#define ASSIGN_OR_RETURN_IMPL(error_or_value, type_variable_name, expression) \
  auto error_or_value = expression;                                           \
  if (!error_or_value.ok()) {                                                 \
    return error_or_value.status();                                           \
  }                                                                           \
  type_variable_name = std::move(*error_or_value);

#define APPEND_NUMBER(expression, number) \
  APPEND_NUMBER_INNER(expression, number)

#define APPEND_NUMBER_INNER(expression, number) expression##number

#define TEXT_WITH_SEPARATOR(text) \
  ImGui::Text(text);              \
  ImGui::Separator();

#define CLEAR_AND_RETURN_STATUS(status) \
  if (!status.ok()) {                   \
    auto temp = status;                 \
    status = absl::OkStatus();          \
    return temp;                        \
  }

using ushort = unsigned short;
using uint = unsigned int;
using uchar = unsigned char;
using Bytes = std::vector<uchar>;

using OWBlockset = std::vector<std::vector<ushort>>;
struct OWMapTiles {
  OWBlockset light_world;    // 64 maps
  OWBlockset dark_world;     // 64 maps
  OWBlockset special_world;  // 32 maps
};
using OWMapTiles = struct OWMapTiles;

namespace yaze {
namespace app {
namespace core {

// ============================================================================
// Window Variables
// ============================================================================

constexpr int kScreenWidth = 1200;
constexpr int kScreenHeight = 800;

// ============================================================================
// Magic numbers
// ============================================================================

/// Bit set for object priority
constexpr ushort TilePriorityBit = 0x2000;

/// Bit set for object hflip
constexpr ushort TileHFlipBit = 0x4000;

/// Bit set for object vflip
constexpr ushort TileVFlipBit = 0x8000;

/// Bits used for tile name
constexpr ushort TileNameMask = 0x03FF;

constexpr int Uncompressed3BPPSize = 0x0600;
constexpr int UncompressedSheetSize = 0x0800;

constexpr int NumberOfSheets = 223;
constexpr int NumberOfRooms = 296;

constexpr int NumberOfColors = 3143;

// ============================================================================
//  Game Graphics
// ============================================================================

constexpr int tile_address = 0x1B52;           // JP = Same
constexpr int tile_address_floor = 0x1B5A;     // JP = Same
constexpr int subtype1_tiles = 0x8000;         // JP = Same
constexpr int subtype2_tiles = 0x83F0;         // JP = Same
constexpr int subtype3_tiles = 0x84F0;         // JP = Same
constexpr int gfx_animated_pointer = 0x10275;  // JP 0x10624 //long pointer

constexpr int hud_palettes = 0xDD660;
constexpr int maxGfx = 0xC3FB5;

constexpr int kTilesheetWidth = 128;
constexpr int kTilesheetHeight = 32;
constexpr int kTilesheetDepth = 8;

// ============================================================================
// Dungeon Related Variables
// ============================================================================

// That could be turned into a pointer :
constexpr int dungeons_palettes_groups = 0x75460;           // JP 0x67DD0
constexpr int dungeons_main_bg_palette_pointers = 0xDEC4B;  // JP Same
constexpr int dungeons_palettes =
    0xDD734;  // JP Same (where all dungeons palettes are)

// That could be turned into a pointer :
constexpr int room_items_pointers = 0xDB69;  // JP 0xDB67

constexpr int rooms_sprite_pointer = 0x4C298;  // JP Same //2byte bank 09D62E
constexpr int room_header_pointer = 0xB5DD;    // LONG
constexpr int room_header_pointers_bank = 0xB5E7;  // JP Same

constexpr int gfx_groups_pointer = 0x6237;
constexpr int room_object_layout_pointer = 0x882D;

constexpr int room_object_pointer = 0x874C;  // Long pointer

constexpr int chests_length_pointer = 0xEBF6;
constexpr int chests_data_pointer1 = 0xEBFB;
// constexpr int chests_data_pointer2 = 0xEC0A; //Disabled for now could be used
// for expansion constexpr int chests_data_pointer3 = 0xEC10; //Disabled for now
// could be used for expansion

constexpr int blocks_length = 0x8896;  // word value
constexpr int blocks_pointer1 = 0x15AFA;
constexpr int blocks_pointer2 = 0x15B01;
constexpr int blocks_pointer3 = 0x15B08;
constexpr int blocks_pointer4 = 0x15B0F;

constexpr int torch_data = 0x2736A;  // JP 0x2704A
constexpr int torches_length_pointer = 0x88C1;

constexpr int sprites_data =
    0x4D8B0;  // It use the unused pointers to have more space //Save purpose
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

// TEXT EDITOR RELATED CONSTANTS
constexpr int gfx_font = 0x70000;  // 2bpp format
constexpr int text_data = 0xE0000;
constexpr int text_data2 = 0x75F40;
constexpr int pointers_dictionaries = 0x74703;
constexpr int characters_width = 0x74ADF;

// ============================================================================
// Dungeon Entrances Related Variables
// ============================================================================

// 0x14577 word value for each room
constexpr int entrance_room = 0x14813;

// 8 bytes per room, HU, FU, HD, FD, HL, FL, HR, FR
constexpr int entrance_scrolledge = 0x1491D;       // 0x14681
constexpr int entrance_yscroll = 0x14D45;          // 0x14AA9 2 bytes each room
constexpr int entrance_xscroll = 0x14E4F;          // 0x14BB3 2 bytes
constexpr int entrance_yposition = 0x14F59;        // 0x14CBD 2bytes
constexpr int entrance_xposition = 0x15063;        // 0x14DC7 2bytes
constexpr int entrance_camerayposition = 0x1516D;  // 0x14ED1 2bytes
constexpr int entrance_cameraxposition = 0x15277;  // 0x14FDB 2bytes

constexpr int entrance_gfx_group = 0x5D97;
constexpr int entrance_blockset = 0x15381;  // 0x150E5 1byte
constexpr int entrance_floor = 0x15406;     // 0x1516A 1byte
constexpr int entrance_dungeon = 0x1548B;   // 0x151EF 1byte (dungeon id)
constexpr int entrance_door = 0x15510;      // 0x15274 1byte

// 1 byte, ---b ---a b = bg2, a = need to check
constexpr int entrance_ladderbg = 0x15595;        // 0x152F9
constexpr int entrance_scrolling = 0x1561A;       // 0x1537E 1byte --h- --v-
constexpr int entrance_scrollquadrant = 0x1569F;  // 0x15403 1byte
constexpr int entrance_exit = 0x15724;            // 0x15488 2byte word
constexpr int entrance_music = 0x1582E;           // 0x15592

// word value for each room
constexpr int startingentrance_room = 0x15B6E;  // 0x158D2

// 8 bytes per room, HU, FU, HD, FD, HL, FL, HR, FR
constexpr int startingentrance_scrolledge = 0x15B7C;  // 0x158E0
constexpr int startingentrance_yscroll = 0x15BB4;  // 0x14AA9 //2bytes each room
constexpr int startingentrance_xscroll = 0x15BC2;  // 0x14BB3 //2bytes
constexpr int startingentrance_yposition = 0x15BD0;        // 0x14CBD 2bytes
constexpr int startingentrance_xposition = 0x15BDE;        // 0x14DC7 2bytes
constexpr int startingentrance_camerayposition = 0x15BEC;  // 0x14ED1 2bytes
constexpr int startingentrance_cameraxposition = 0x15BFA;  // 0x14FDB 2bytes

constexpr int startingentrance_blockset = 0x15C08;  // 0x150E5 1byte
constexpr int startingentrance_floor = 0x15C0F;     // 0x1516A 1byte
constexpr int startingentrance_dungeon = 0x15C16;  // 0x151EF 1byte (dungeon id)

constexpr int startingentrance_door = 0x15C2B;  // 0x15274 1byte

// 1 byte, ---b ---a b = bg2, a = need to check
constexpr int startingentrance_ladderbg = 0x15C1D;  // 0x152F9
// 1byte --h- --v-
constexpr int startingentrance_scrolling = 0x15C24;       // 0x1537E
constexpr int startingentrance_scrollquadrant = 0x15C2B;  // 0x15403 1byte
constexpr int startingentrance_exit = 0x15C32;   // 0x15488 //2byte word
constexpr int startingentrance_music = 0x15C4E;  // 0x15592
constexpr int startingentrance_entrance = 0x15C40;

constexpr int items_data_start = 0xDDE9;  // save purpose
constexpr int items_data_end = 0xE6B2;    // save purpose
constexpr int initial_equipement = 0x271A6;
constexpr int messages_id_dungeon = 0x3F61D;

// item id you get instead if you already have that item
constexpr int chests_backupitems = 0x3B528;
constexpr int chests_yoffset = 0x4836C;
constexpr int chests_xoffset = 0x4836C + (76 * 1);
constexpr int chests_itemsgfx = 0x4836C + (76 * 2);
constexpr int chests_itemswide = 0x4836C + (76 * 3);
constexpr int chests_itemsproperties = 0x4836C + (76 * 4);
constexpr int chests_sramaddress = 0x4836C + (76 * 5);
constexpr int chests_sramvalue = 0x4836C + (76 * 7);
constexpr int chests_msgid = 0x442DD;

constexpr int dungeons_startrooms = 0x7939;
constexpr int dungeons_endrooms = 0x792D;
constexpr int dungeons_bossrooms = 0x10954;  // short value

// Bed Related Values (Starting location)
constexpr int bedPositionX = 0x039A37;  // short value
constexpr int bedPositionY = 0x039A32;  // short value

// short value (on 2 different bytes)
constexpr int bedPositionResetXLow = 0x02DE53;
constexpr int bedPositionResetXHigh = 0x02DE58;

// short value (on 2 different bytes)
constexpr int bedPositionResetYLow = 0x02DE5D;
constexpr int bedPositionResetYHigh = 0x02DE62;

constexpr int bedSheetPositionX = 0x0480BD;  // short value
constexpr int bedSheetPositionY = 0x0480B8;  // short value

// ============================================================================
// Gravestones related variables
// ============================================================================

constexpr int GravesYTilePos = 0x49968;    // short (0x0F entries)
constexpr int GravesXTilePos = 0x49986;    // short (0x0F entries)
constexpr int GravesTilemapPos = 0x499A4;  // short (0x0F entries)
constexpr int GravesGFX = 0x499C2;         // short (0x0F entries)

constexpr int GravesXPos = 0x4994A;      // short (0x0F entries)
constexpr int GravesYLine = 0x4993A;     // short (0x08 entries)
constexpr int GravesCountOnY = 0x499E0;  // Byte 0x09 entries

constexpr int GraveLinkSpecialHole = 0x46DD9;    // short
constexpr int GraveLinkSpecialStairs = 0x46DE0;  // short

// ============================================================================
// Palettes Related Variables - This contain all the palettes of the game
// ============================================================================
constexpr int overworldPaletteMain = 0xDE6C8;
constexpr int overworldPaletteAuxialiary = 0xDE86C;
constexpr int overworldPaletteAnimated = 0xDE604;
constexpr int globalSpritePalettesLW = 0xDD218;
constexpr int globalSpritePalettesDW = 0xDD290;
// Green, Blue, Red, Bunny, Electrocuted (15 colors each)
constexpr int armorPalettes = 0xDD308;
constexpr int spritePalettesAux1 = 0xDD39E;  // 7 colors each
constexpr int spritePalettesAux2 = 0xDD446;  // 7 colors each
constexpr int spritePalettesAux3 = 0xDD4E0;  // 7 colors each
constexpr int swordPalettes = 0xDD630;       // 3 colors each - 4 entries
constexpr int shieldPalettes = 0xDD648;      // 4 colors each - 3 entries
constexpr int hudPalettes = 0xDD660;
constexpr int dungeonMapPalettes = 0xDD70A;    // 21 colors
constexpr int dungeonMainPalettes = 0xDD734;   //(15*6) colors each - 20 entries
constexpr int dungeonMapBgPalettes = 0xDE544;  // 16*6
// Mirrored Value at 0x75645 : 0x75625
constexpr int hardcodedGrassLW = 0x5FEA9;
constexpr int hardcodedGrassDW = 0x05FEB3;  // 0x7564F
constexpr int hardcodedGrassSpecial = 0x75640;
constexpr int overworldMiniMapPalettes = 0x55B27;
constexpr int triforcePalette = 0x64425;
constexpr int crystalPalette = 0xF4CD3;
constexpr int customAreaSpecificBGPalette =
    0x140000;  // 2 bytes for each overworld area (320)
constexpr int customAreaSpecificBGASM = 0x140150;
constexpr int customAreaSpecificBGEnabled =
    0x140140;  // 1 byte, not 0 if enabled

// ============================================================================
// Dungeon Map Related Variables
// ============================================================================

constexpr int dungeonMap_rooms_ptr = 0x57605;  // 14 pointers of map data
constexpr int dungeonMap_floors = 0x575D9;     // 14 words values

constexpr int dungeonMap_gfx_ptr = 0x57BE4;  // 14 pointers of gfx data

// data start for floors/gfx MUST skip 575D9 to 57621 (pointers)
constexpr int dungeonMap_datastart = 0x57039;

// IF Byte = 0xB9 dungeon maps are not expanded
constexpr int dungeonMap_expCheck = 0x56652;
constexpr int dungeonMap_tile16 = 0x57009;
constexpr int dungeonMap_tile16Exp = 0x109010;

// 14 words values 0x000F = no boss
constexpr int dungeonMap_bossrooms = 0x56807;
constexpr int triforceVertices = 0x04FFD2;  // group of 3, X, Y ,Z
constexpr int TriforceFaces = 0x04FFE4;     // group of 5

constexpr int crystalVertices = 0x04FF98;

// ============================================================================
// Names
// ============================================================================

static const absl::string_view RoomEffect[] = {"Nothing",
                                               "Nothing",
                                               "Moving Floor",
                                               "Moving Water",
                                               "Trinexx Shell",
                                               "Red Flashes",
                                               "Light Torch to See Floor",
                                               "Ganon's Darkness"};

static const absl::string_view RoomTag[] = {"Nothing",

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

static const absl::string_view SecretItemNames[] = {
    "Nothing",    "Green Rupee", "Rock hoarder",  "Bee",       "Health pack",
    "Bomb",       "Heart ",      "Blue Rupee",

    "Key",        "Arrow",       "Bomb",          "Heart",     "Magic",
    "Full Magic", "Cucco",       "Green Soldier", "Bush Stal", "Blue Soldier",

    "Landmine",   "Heart",       "Fairy",         "Heart",
    "Nothing ",  // 22

    "Hole",       "Warp",        "Staircase",     "Bombable",  "Switch"};

static const absl::string_view TileTypeNames[] = {
    "$00 Nothing (standard floor)",
    "$01 Collision",
    "$02 Collision",
    "$03 Collision",
    "$04 Collision",
    "$05 Nothing (unused?)",
    "$06 Nothing (unused?)",
    "$07 Nothing (unused?)",
    "$08 Deep water",
    "$09 Shallow water",
    "$0A Unknown? Possibly unused",
    "$0B Collision (different in Overworld and unknown)",
    "$0C Overlay mask",
    "$0D Spike floor",
    "$0E GT ice",
    "$0F Ice palace ice",
    "$10 Slope ◤",
    "$11 Slope ◥",
    "$12 Slope ◣",
    "$13 Slope ◢",
    "$14 Nothing (unused?)",
    "$15 Nothing (unused?)",
    "$16 Nothing (unused?)",
    "$17 Nothing (unused?)",
    "$18 Slope ◤",
    "$19 Slope ◥",
    "$1A Slope ◣",
    "$1B Slope ◢",
    "$1C Layer 2 overlay",
    "$1D North single-layer auto stairs",
    "$1E North layer-swap auto stairs",
    "$1F North layer-swap auto stairs",
    "$20 Pit",
    "$21 Nothing (unused?)",
    "$22 Manual stairs",
    "$23 Pot switch",
    "$24 Pressure switch",
    "$25 Nothing (unused but referenced by somaria blocks)",
    "$26 Collision (near stairs?)",
    "$27 Brazier/Fence/Statue/Block/General hookable things",
    "$28 North ledge",
    "$29 South ledge",
    "$2A East ledge",
    "$2B West ledge",
    "$2C ◤ ledge",
    "$2D ◣ ledge",
    "$2E ◥ ledge",
    "$2F ◢ ledge",
    "$30 Straight inter-room stairs south/up 0",
    "$31 Straight inter-room stairs south/up 1",
    "$32 Straight inter-room stairs south/up 2",
    "$33 Straight inter-room stairs south/up 3",
    "$34 Straight inter-room stairs north/down 0",
    "$35 Straight inter-room stairs north/down 1",
    "$36 Straight inter-room stairs north/down 2",
    "$37 Straight inter-room stairs north/down 3",
    "$38 Straight inter-room stairs north/down edge",
    "$39 Straight inter-room stairs south/up edge",
    "$3A Star tile (inactive on load)",
    "$3B Star tile (active on load)",
    "$3C Nothing (unused?)",
    "$3D South single-layer auto stairs",
    "$3E South layer-swap auto stairs",
    "$3F South layer-swap auto stairs",
    "$40 Thick grass",
    "$41 Nothing (unused?)",
    "$42 Gravestone / Tower of hera ledge shadows??",
    "$43 Skull Woods entrance/Hera columns???",
    "$44 Spike",
    "$45 Nothing (unused?)",
    "$46 Desert Tablet",
    "$47 Nothing (unused?)",
    "$48 Diggable ground",
    "$49 Nothing (unused?)",
    "$4A Diggable ground",
    "$4B Warp tile",
    "$4C Nothing (unused?) | Something unknown in overworld",
    "$4D Nothing (unused?) | Something unknown in overworld",
    "$4E Square corners in EP overworld",
    "$4F Square corners in EP overworld",
    "$50 Green bush",
    "$51 Dark bush",
    "$52 Gray rock",
    "$53 Black rock",
    "$54 Hint tile/Sign",
    "$55 Big gray rock",
    "$56 Big black rock",
    "$57 Bonk rocks",
    "$58 Chest 0",
    "$59 Chest 1",
    "$5A Chest 2",
    "$5B Chest 3",
    "$5C Chest 4",
    "$5D Chest 5",
    "$5E Spiral stairs",
    "$5F Spiral stairs",
    "$60 Rupee tile",
    "$61 Nothing (unused?)",
    "$62 Bombable floor",
    "$63 Minigame chest",
    "$64 Nothing (unused?)",
    "$65 Nothing (unused?)",
    "$66 Crystal peg down",
    "$67 Crystal peg up",
    "$68 Upwards conveyor",
    "$69 Downwards conveyor",
    "$6A Leftwards conveyor",
    "$6B Rightwards conveyor",
    "$6C North vines",
    "$6D South vines",
    "$6E West vines",
    "$6F East vines",
    "$70 Pot/Hammer peg/Push block 00",
    "$71 Pot/Hammer peg/Push block 01",
    "$72 Pot/Hammer peg/Push block 02",
    "$73 Pot/Hammer peg/Push block 03",
    "$74 Pot/Hammer peg/Push block 04",
    "$75 Pot/Hammer peg/Push block 05",
    "$76 Pot/Hammer peg/Push block 06",
    "$77 Pot/Hammer peg/Push block 07",
    "$78 Pot/Hammer peg/Push block 08",
    "$79 Pot/Hammer peg/Push block 09",
    "$7A Pot/Hammer peg/Push block 0A",
    "$7B Pot/Hammer peg/Push block 0B",
    "$7C Pot/Hammer peg/Push block 0C",
    "$7D Pot/Hammer peg/Push block 0D",
    "$7E Pot/Hammer peg/Push block 0E",
    "$7F Pot/Hammer peg/Push block 0F",
    "$80 North/South door",
    "$81 East/West door",
    "$82 North/South shutter door",
    "$83 East/West shutter door",
    "$84 North/South layer 2 door",
    "$85 East/West layer 2 door",
    "$86 North/South layer 2 shutter door",
    "$87 East/West layer 2 shutter door",
    "$88 Some type of door (?)",
    "$89 East/West transport door",
    "$8A Some type of door (?)",
    "$8B Some type of door (?)",
    "$8C Some type of door (?)",
    "$8D Some type of door (?)",
    "$8E Entrance door",
    "$8F Entrance door",
    "$90 Layer toggle shutter door (?)",
    "$91 Layer toggle shutter door (?)",
    "$92 Layer toggle shutter door (?)",
    "$93 Layer toggle shutter door (?)",
    "$94 Layer toggle shutter door (?)",
    "$95 Layer toggle shutter door (?)",
    "$96 Layer toggle shutter door (?)",
    "$97 Layer toggle shutter door (?)",
    "$98 Layer+Dungeon toggle shutter door (?)",
    "$99 Layer+Dungeon toggle shutter door (?)",
    "$9A Layer+Dungeon toggle shutter door (?)",
    "$9B Layer+Dungeon toggle shutter door (?)",
    "$9C Layer+Dungeon toggle shutter door (?)",
    "$9D Layer+Dungeon toggle shutter door (?)",
    "$9E Layer+Dungeon toggle shutter door (?)",
    "$9F Layer+Dungeon toggle shutter door (?)",
    "$A0 North/South Dungeon swap door",
    "$A1 Dungeon toggle door (?)",
    "$A2 Dungeon toggle door (?)",
    "$A3 Dungeon toggle door (?)",
    "$A4 Dungeon toggle door (?)",
    "$A5 Dungeon toggle door (?)",
    "$A6 Nothing (unused?)",
    "$A7 Nothing (unused?)",
    "$A8 Layer+Dungeon toggle shutter door (?)",
    "$A9 Layer+Dungeon toggle shutter door (?)",
    "$AA Layer+Dungeon toggle shutter door (?)",
    "$AB Layer+Dungeon toggle shutter door (?)",
    "$AC Layer+Dungeon toggle shutter door (?)",
    "$AD Layer+Dungeon toggle shutter door (?)",
    "$AE Layer+Dungeon toggle shutter door (?)",
    "$AF Layer+Dungeon toggle shutter door (?)",
    "$B0 Somaria ─",
    "$B1 Somaria │",
    "$B2 Somaria ┌",
    "$B3 Somaria └",
    "$B4 Somaria ┐",
    "$B5 Somaria ┘",
    "$B6 Somaria ⍰ 1 way",
    "$B7 Somaria ┬",
    "$B8 Somaria ┴",
    "$B9 Somaria ├",
    "$BA Somaria ┤",
    "$BB Somaria ┼",
    "$BC Somaria ⍰ 2 way",
    "$BD Somaria ┼ crossover",
    "$BE Pipe entrance",
    "$BF Nothing (unused?)",
    "$C0 Torch 00",
    "$C1 Torch 01",
    "$C2 Torch 02",
    "$C3 Torch 03",
    "$C4 Torch 04",
    "$C5 Torch 05",
    "$C6 Torch 06",
    "$C7 Torch 07",
    "$C8 Torch 08",
    "$C9 Torch 09",
    "$CA Torch 0A",
    "$CB Torch 0B",
    "$CC Torch 0C",
    "$CD Torch 0D",
    "$CE Torch 0E",
    "$CF Torch 0F",
    "$D0 Nothing (unused?)",
    "$D1 Nothing (unused?)",
    "$D2 Nothing (unused?)",
    "$D3 Nothing (unused?)",
    "$D4 Nothing (unused?)",
    "$D5 Nothing (unused?)",
    "$D6 Nothing (unused?)",
    "$D7 Nothing (unused?)",
    "$D8 Nothing (unused?)",
    "$D9 Nothing (unused?)",
    "$DA Nothing (unused?)",
    "$DB Nothing (unused?)",
    "$DC Nothing (unused?)",
    "$DD Nothing (unused?)",
    "$DE Nothing (unused?)",
    "$DF Nothing (unused?)",
    "$E0 Nothing (unused?)",
    "$E1 Nothing (unused?)",
    "$E2 Nothing (unused?)",
    "$E3 Nothing (unused?)",
    "$E4 Nothing (unused?)",
    "$E5 Nothing (unused?)",
    "$E6 Nothing (unused?)",
    "$E7 Nothing (unused?)",
    "$E8 Nothing (unused?)",
    "$E9 Nothing (unused?)",
    "$EA Nothing (unused?)",
    "$EB Nothing (unused?)",
    "$EC Nothing (unused?)",
    "$ED Nothing (unused?)",
    "$EE Nothing (unused?)",
    "$EF Nothing (unused?)",
    "$F0 Door 0 bottom",
    "$F1 Door 1 bottom",
    "$F2 Door 2 bottom",
    "$F3 Door 3 bottom",
    "$F4 Door X bottom? (unused?)",
    "$F5 Door X bottom? (unused?)",
    "$F6 Door X bottom? (unused?)",
    "$F7 Door X bottom? (unused?)",
    "$F8 Door 0 top",
    "$F9 Door 1 top",
    "$FA Door 2 top",
    "$FB Door 3 top",
    "$FC Door X top? (unused?)",
    "$FD Door X top? (unused?)",
    "$FE Door X top? (unused?)",
    "$FF Door X top? (unused?)"};

static const absl::string_view kSpriteDefaultNames[]{
    "00 Raven",
    "01 Vulture",
    "02 Flying Stalfos Head",
    "03 No Pointer (Empty",
    "04 Pull Switch (good",
    "05 Pull Switch (unused",
    "06 Pull Switch (bad",
    "07 Pull Switch (unused",
    "08 Octorock (one way",
    "09 Moldorm (Boss",
    "0A Octorock (four way",
    "0B Chicken",
    "0C Octorock (?",
    "0D Buzzblock",
    "0E Snapdragon",
    "0F Octoballoon",
    "10 Octoballon Hatchlings",
    "11 Hinox",
    "12 Moblin",
    "13 Mini Helmasaure",
    "14 Gargoyle's Domain Gate",
    "15 Antifairy",
    "16 Sahasrahla / Aginah",
    "17 Bush Hoarder",
    "18 Mini Moldorm",
    "19 Poe",
    "1A Dwarves",
    "1B Arrow in wall",
    "1C Statue",
    "1D Weathervane",
    "1E Crystal Switch",
    "1F Bug-Catching Kid",
    "20 Sluggula",
    "21 Push Switch",
    "22 Ropa",
    "23 Red Bari",
    "24 Blue Bari",
    "25 Talking Tree",
    "26 Hardhat Beetle",
    "27 Deadrock",
    "28 Storytellers",
    "29 Blind Hideout attendant",
    "2A Sweeping Lady",
    "2B Storytellers",
    "2C Lumberjacks",
    "2D Telepathic Stones",
    "2E Multipurpose Sprite",
    "2F Race Npc",
    "30 Person?",
    "31 Fortune Teller",
    "32 Angry Brothers",
    "33 Pull for items",
    "34 Scared Girl",
    "35 Innkeeper",
    "36 Witch",
    "37 Waterfall",
    "38 Arrow Target",
    "39 Average Middle",
    "3A Half Magic Bat",
    "3B Dash Item",
    "3C Village Kid",
    "3D Signs? Chicken lady also showed up / Scared ladies outside houses.",
    "3E Rock Hoarder",
    "3F Tutorial Soldier",
    "40 Lightning Lock",
    "41 Blue Sword Soldier / Used by guards to detect player",
    "42 Green Sword Soldier",
    "43 Red Spear Soldier",
    "44 Assault Sword Soldier",
    "45 Green Spear Soldier",
    "46 Blue Archer",
    "47 Green Archer",
    "48 Red Javelin Soldier",
    "49 Red Javelin Soldier 2",
    "4A Red Bomb Soldiers",
    "4B Green Soldier Recruits",
    "4C Geldman",
    "4D Rabbit",
    "4E Popo",
    "4F Popo 2",
    "50 Cannon Balls",
    "51 Armos",
    "52 Giant Zora",
    "53 Armos Knights (Boss",
    "54 Lanmolas (Boss",
    "55 Fireball Zora",
    "56 Walking Zora",
    "57 Desert Palace Barriers",
    "58 Crab",
    "59 Bird",
    "5A Squirrel",
    "5B Spark (Left to Right",
    "5C Spark (Right to Left",
    "5D Roller (vertical moving",
    "5E Roller (vertical moving",
    "5F Roller",
    "60 Roller (horizontal moving",
    "61 Beamos",
    "62 Master Sword",
    "63 Devalant (Non",
    "64 Devalant (Shooter",
    "65 Shooting Gallery Proprietor",
    "66 Moving Cannon Ball Shooters (Right",
    "67 Moving Cannon Ball Shooters (Left",
    "68 Moving Cannon Ball Shooters (Down",
    "69 Moving Cannon Ball Shooters (Up",
    "6A Ball N' Chain Trooper",
    "6B Cannon Soldier",
    "6C Mirror Portal",
    "6D Rat",
    "6E Rope",
    "6F Keese",
    "70 Helmasaur King Fireball",
    "71 Leever",
    "72 Activator for the ponds (where you throw in items",
    "73 Uncle / Priest",
    "74 Running Man",
    "75 Bottle Salesman",
    "76 Princess Zelda",
    "77 Antifairy (Alternate",
    "78 Village Elder",
    "79 Bee",
    "7A Agahnim",
    "7B Agahnim Energy Ball",
    "7C Hyu",
    "7D Big Spike Trap",
    "7E Guruguru Bar (Clockwise",
    "7F Guruguru Bar (Counter Clockwise",
    "80 Winder",
    "81 Water Tektite",
    "82 Antifairy Circle",
    "83 Green Eyegore",
    "84 Red Eyegore",
    "85 Yellow Stalfos",
    "86 Kodongos",
    "87 Flames",
    "88 Mothula (Boss",
    "89 Mothula's Beam",
    "8A Spike Trap",
    "8B Gibdo",
    "8C Arrghus (Boss",
    "8D Arrghus spawn",
    "8E Terrorpin",
    "8F Slime",
    "90 Wallmaster",
    "91 Stalfos Knight",
    "92 Helmasaur King",
    "93 Bumper",
    "94 Swimmers",
    "95 Eye Laser (Right",
    "96 Eye Laser (Left",
    "97 Eye Laser (Down",
    "98 Eye Laser (Up",
    "99 Pengator",
    "9A Kyameron",
    "9B Wizzrobe",
    "9C Tadpoles",
    "9D Tadpoles",
    "9E Ostrich (Haunted Grove",
    "9F Flute",
    "A0 Birds (Haunted Grove",
    "A1 Freezor",
    "A2 Kholdstare (Boss",
    "A3 Kholdstare's Shell",
    "A4 Falling Ice",
    "A5 Zazak Fireball",
    "A6 Red Zazak",
    "A7 Stalfos",
    "A8 Bomber Flying Creatures from Darkworld",
    "A9 Bomber Flying Creatures from Darkworld",
    "AA Pikit",
    "AB Maiden",
    "AC Apple",
    "AD Lost Old Man",
    "AE Down Pipe",
    "AF Up Pipe",
    "B0 Right Pip",
    "B1 Left Pipe",
    "B2 Good bee again?",
    "B3 Hylian Inscription",
    "B4 Thief?s chest (not the one that follows you",
    "B5 Bomb Salesman",
    "B6 Kiki",
    "B7 Maiden following you in Blind Dungeon",
    "B8 Monologue Testing Sprite",
    "B9 Feuding Friends on Death Mountain",
    "BA Whirlpool",
    "BB Salesman / chestgame guy / 300 rupee giver guy / Chest game thief",
    "BC Drunk in the inn",
    "BD Vitreous (Large Eyeball",
    "BE Vitreous (Small Eyeball",
    "BF Vitreous' Lightning",
    "C0 Monster in Lake of Ill Omen / Quake Medallion",
    "C1 Agahnim teleporting Zelda to dark world",
    "C2 Boulders",
    "C3 Gibo",
    "C4 Thief",
    "C5 Medusa",
    "C6 Four Way Fireball Spitters (spit when you use your sword",
    "C7 Hokku",
    "C8 Big Fairy who heals you",
    "C9 Tektite",
    "CA Chain Chomp",
    "CB Trinexx",
    "CC Another part of trinexx",
    "CD Yet another part of trinexx",
    "CE Blind The Thief (Boss)",
    "CF Swamola",
    "D0 Lynel",
    "D1 Bunny Beam",
    "D2 Flopping fish",
    "D3 Stal",
    "D4 Landmine",
    "D5 Digging Game Proprietor",
    "D6 Ganon",
    "D7 Copy of Ganon",
    "D8 Heart",
    "D9 Green Rupee",
    "DA Blue Rupee",
    "DB Red Rupee",
    "DC Bomb Refill (1)",
    "DD Bomb Refill (4)",
    "DE Bomb Refill (8)",
    "DF Small Magic Refill",
    "E0 Full Magic Refill",
    "E1 Arrow Refill (5)",
    "E2 Arrow Refill (10)",
    "E3 Fairy",
    "E4 Key",
    "E5 Big Key",
    "E6 Shield",
    "E7 Mushroom",
    "E8 Fake Master Sword",
    "E9 Magic Shop dude / His items",
    "EA Heart Container",
    "EB Heart Piece",
    "EC Bushes",
    "ED Cane Of Somaria Platform",
    "EE Mantle",
    "EF Cane of Somaria Platform (Unused)",
    "F0 Cane of Somaria Platform (Unused)",
    "F1 Cane of Somaria Platform (Unused)",
    "F2 Medallion Tablet",
    "F3",
    "F4 Falling Rocks",
    "F5",
    "F6",
    "F7",
    "F8",
    "F9",
    "FA",
    "FB",
    "FC",
    "FD",
    "FE",
    "FF",
};

static const absl::string_view overlordnames[] = {
    "Overlord_SpritePositionTarget",
    "Overlord_AllDirectionMetalBallFactory",
    "Overlord_CascadeMetalBallFactory",
    "Overlord_StalfosFactory",
    "Overlord_StalfosTrap",
    "Overlord_SnakeTrap",
    "Overlord_MovingFloor",
    "Overlord_ZolFactory",
    "Overlord_WallMasterFactory",
    "Overlord_CrumbleTilePath 1",
    "Overlord_CrumbleTilePath 2",
    "Overlord_CrumbleTilePath 3",
    "Overlord_CrumbleTilePath 4",
    "Overlord_CrumbleTilePath 5",
    "Overlord_CrumbleTilePath 6",
    "Overlord_PirogusuFactory 1",
    "Overlord_PirogusuFactory 2",
    "Overlord_PirogusuFactory 3",
    "Overlord_PirogusuFactory 4",
    "Overlord_FlyingTileFactory",
    "Overlord_WizzrobeFactory",
    "Overlord_ZoroFactory",
    "Overlord_StalfosTrapTriggerWindow",
    "Overlord_RedStalfosTrap",
    "Overlord_ArmosCoordinator",
    "Overlord_BombTrap",
};

}  // namespace core
}  // namespace app
}  // namespace yaze

#endif