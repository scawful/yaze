#ifndef YAZE_APP_CORE_CONSTANTS_H
#define YAZE_APP_CORE_CONSTANTS_H

#include <array>
#include <string>

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

using ushort = unsigned short;
using uint = unsigned int;
using uchar = unsigned char;

namespace yaze {
namespace app {
namespace core {

//===========================================================================================
// 65816 LanguageDefinition
//===========================================================================================

static const char *const kKeywords[] = {
    "ADC", "AND", "ASL", "BCC", "BCS", "BEQ",   "BIT",   "BMI",       "BNE",
    "BPL", "BRA", "BRL", "BVC", "BVS", "CLC",   "CLD",   "CLI",       "CLV",
    "CMP", "CPX", "CPY", "DEC", "DEX", "DEY",   "EOR",   "INC",       "INX",
    "INY", "JMP", "JSR", "JSL", "LDA", "LDX",   "LDY",   "LSR",       "MVN",
    "NOP", "ORA", "PEA", "PER", "PHA", "PHB",   "PHD",   "PHP",       "PHX",
    "PHY", "PLA", "PLB", "PLD", "PLP", "PLX",   "PLY",   "REP",       "ROL",
    "ROR", "RTI", "RTL", "RTS", "SBC", "SEC",   "SEI",   "SEP",       "STA",
    "STP", "STX", "STY", "STZ", "TAX", "TAY",   "TCD",   "TCS",       "TDC",
    "TRB", "TSB", "TSC", "TSX", "TXA", "TXS",   "TXY",   "TYA",       "TYX",
    "WAI", "WDM", "XBA", "XCE", "ORG", "LOROM", "HIROM", "NAMESPACE", "DB"};

static const char *const kIdentifiers[] = {
    "abort",   "abs",     "acos",    "asin",     "atan",    "atexit",
    "atof",    "atoi",    "atol",    "ceil",     "clock",   "cosh",
    "ctime",   "div",     "exit",    "fabs",     "floor",   "fmod",
    "getchar", "getenv",  "isalnum", "isalpha",  "isdigit", "isgraph",
    "ispunct", "isspace", "isupper", "kbhit",    "log10",   "log2",
    "log",     "memcmp",  "modf",    "pow",      "putchar", "putenv",
    "puts",    "rand",    "remove",  "rename",   "sinh",    "sqrt",
    "srand",   "strcat",  "strcmp",  "strerror", "time",    "tolower",
    "toupper"};

//===========================================================================================
// Magic numbers
//===========================================================================================

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
constexpr int LimitOfMap32 = 8864;
constexpr int NumberOfRooms = 296;

constexpr int NumberOfOWMaps = 160;
constexpr int Map32PerScreen = 256;
constexpr int NumberOfMap16 = 3752;  // 4096
constexpr int NumberOfMap32 = Map32PerScreen * NumberOfOWMaps;
constexpr int NumberOfOWSprites = 352;
constexpr int NumberOfColors = 3143;

// ===========================================================================================
//  gfx
// ===========================================================================================

constexpr int tile_address = 0x1B52;           // JP = Same
constexpr int tile_address_floor = 0x1B5A;     // JP = Same
constexpr int subtype1_tiles = 0x8000;         // JP = Same
constexpr int subtype2_tiles = 0x83F0;         // JP = Same
constexpr int subtype3_tiles = 0x84F0;         // JP = Same
constexpr int gfx_animated_pointer = 0x10275;  // JP 0x10624 //long pointer
constexpr int overworldgfxGroups2 = 0x6073;    // 0x60B3
constexpr int gfx_1_pointer =
    0x6790;  // 2byte pointer bank 00 pc -> 0x4320  CF80  ; 004F80
constexpr int gfx_2_pointer = 0x6795;  // D05F ; 00505F
constexpr int gfx_3_pointer = 0x679A;  // D13E ; 00513E
constexpr int hud_palettes = 0xDD660;
constexpr int maxGfx = 0xC3FB5;

constexpr int kTilesheetWidth = 128;
constexpr int kTilesheetHeight = 32;
constexpr int kTilesheetDepth = 8;

// ===========================================================================================
//  Overworld Related Variables
// ===========================================================================================

constexpr int compressedAllMap32PointersHigh = 0x1794D;
constexpr int compressedAllMap32PointersLow = 0x17B2D;
constexpr int overworldgfxGroups = 0x05D97;
constexpr int map16Tiles = 0x78000;
constexpr int map32TilesTL = 0x18000;
constexpr int map32TilesTR = 0x1B400;
constexpr int map32TilesBL = 0x20000;
constexpr int map32TilesBR = 0x23400;
constexpr int overworldPalGroup1 = 0xDE6C8;
constexpr int overworldPalGroup2 = 0xDE86C;
constexpr int overworldPalGroup3 = 0xDE604;
constexpr int overworldMapPalette = 0x7D1C;
constexpr int overworldSpritePalette = 0x7B41;
constexpr int overworldMapPaletteGroup = 0x75504;
constexpr int overworldSpritePaletteGroup = 0x75580;
constexpr int overworldSpriteset = 0x7A41;
constexpr int overworldSpecialGFXGroup = 0x16821;
constexpr int overworldSpecialPALGroup = 0x16831;

constexpr int overworldSpritesBegining = 0x4C881;
constexpr int overworldSpritesAgahnim = 0x4CA21;
constexpr int overworldSpritesZelda = 0x4C901;

constexpr int overworldItemsPointers = 0xDC2F9;
constexpr int overworldItemsAddress = 0xDC8B9;  // 1BC2F9
constexpr int overworldItemsBank = 0xDC8BF;
constexpr int overworldItemsEndData = 0xDC89C;  // 0DC89E

constexpr int mapGfx = 0x7C9C;
constexpr int overlayPointers = 0x77664;
constexpr int overlayPointersBank = 0x0E;

constexpr int overworldTilesType = 0x71459;
constexpr int overworldMessages = 0x3F51D;

// TODO:
constexpr int overworldMusicBegining = 0x14303;
constexpr int overworldMusicZelda = 0x14303 + 0x40;
constexpr int overworldMusicMasterSword = 0x14303 + 0x80;
constexpr int overworldMusicAgahim = 0x14303 + 0xC0;
constexpr int overworldMusicDW = 0x14403;

constexpr int overworldEntranceAllowedTilesLeft = 0xDB8C1;
constexpr int overworldEntranceAllowedTilesRight = 0xDB917;

constexpr int overworldMapSize = 0x12844;  // 0x00 = small maps, 0x20 = large
                                           // maps
constexpr int overworldMapSizeHighByte =
    0x12884;  // 0x01 = small maps, 0x03 = large maps

// relative to the WORLD + 0x200 per map
// large map that are not == parent id = same position as their parent!
// eg for X position small maps :
// 0000, 0200, 0400, 0600, 0800, 0A00, 0C00, 0E00
// all Large map would be :
// 0000, 0000, 0400, 0400, 0800, 0800, 0C00, 0C00

constexpr int overworldTransitionPositionY = 0x128C4;
constexpr int overworldTransitionPositionX = 0x12944;

constexpr int overworldScreenSize = 0x1788D;

//===========================================================================================
// Overworld Exits/Entrances Variables
//===========================================================================================
constexpr int OWExitRoomId = 0x15D8A;  // 0x15E07 Credits sequences
// 105C2 Ending maps
// 105E2 Sprite Group Table for Ending
constexpr int OWExitMapId = 0x15E28;
constexpr int OWExitVram = 0x15E77;
constexpr int OWExitYScroll = 0x15F15;
constexpr int OWExitXScroll = 0x15FB3;
constexpr int OWExitYPlayer = 0x16051;
constexpr int OWExitXPlayer = 0x160EF;
constexpr int OWExitYCamera = 0x1618D;
constexpr int OWExitXCamera = 0x1622B;
constexpr int OWExitDoorPosition = 0x15724;
constexpr int OWExitUnk1 = 0x162C9;
constexpr int OWExitUnk2 = 0x16318;
constexpr int OWExitDoorType1 = 0x16367;
constexpr int OWExitDoorType2 = 0x16405;
constexpr int OWEntranceMap = 0xDB96F;
constexpr int OWEntrancePos = 0xDBA71;
constexpr int OWEntranceEntranceId = 0xDBB73;
constexpr int OWHolePos = 0xDB800;  //(0x13 entries, 2 bytes each) modified(less
                                    // 0x400) map16 coordinates for each hole
constexpr int OWHoleArea =
    0xDB826;  //(0x13 entries, 2 bytes each) corresponding
              // area numbers for each hole
constexpr int OWHoleEntrance =
    0xDB84C;  //(0x13 entries, 1 byte each)  corresponding entrance numbers

constexpr int OWExitMapIdWhirlpool = 0x16AE5;    //  JP = ;016849
constexpr int OWExitVramWhirlpool = 0x16B07;     //  JP = ;01686B
constexpr int OWExitYScrollWhirlpool = 0x16B29;  // JP = ;01688D
constexpr int OWExitXScrollWhirlpool = 0x16B4B;  // JP = ;016DE7
constexpr int OWExitYPlayerWhirlpool = 0x16B6D;  // JP = ;016E09
constexpr int OWExitXPlayerWhirlpool = 0x16B8F;  // JP = ;016E2B
constexpr int OWExitYCameraWhirlpool = 0x16BB1;  // JP = ;016E4D
constexpr int OWExitXCameraWhirlpool = 0x16BD3;  // JP = ;016E6F
constexpr int OWExitUnk1Whirlpool = 0x16BF5;     //    JP = ;016E91
constexpr int OWExitUnk2Whirlpool = 0x16C17;     //    JP = ;016EB3
constexpr int OWWhirlpoolPosition = 0x16CF8;     //    JP = ;016F94

//===========================================================================================
// Dungeon Related Variables
//===========================================================================================
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

constexpr int sprite_blockset_pointer = 0x5B57;
constexpr int sprites_data =
    0x4D8B0;  // It use the unused pointers to have more space //Save purpose
constexpr int sprites_data_empty_room = 0x4D8AE;
constexpr int sprites_end_data = 0x4EC9E;

constexpr int pit_pointer = 0x394AB;
constexpr int pit_count = 0x394A6;

constexpr int doorPointers = 0xF83C0;

// doors
constexpr int door_gfx_up = 0x4D9E;
//
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

//===========================================================================================
// Dungeon Entrances Related Variables
//===========================================================================================
constexpr int entrance_room = 0x14813;  // 0x14577 //word value for each room
// 8 bytes per room, HU, FU, HD, FD, HL, FL, HR, FR
constexpr int entrance_scrolledge = 0x1491D;       // 0x14681
constexpr int entrance_yscroll = 0x14D45;          // 0x14AA9 //2bytes each room
constexpr int entrance_xscroll = 0x14E4F;          // 0x14BB3 //2bytes
constexpr int entrance_yposition = 0x14F59;        // 0x14CBD 2bytes
constexpr int entrance_xposition = 0x15063;        // 0x14DC7 2bytes
constexpr int entrance_camerayposition = 0x1516D;  // 0x14ED1 2bytes
constexpr int entrance_cameraxposition = 0x15277;  // 0x14FDB 2bytes

constexpr int entrance_gfx_group = 0x5D97;
constexpr int entrance_blockset = 0x15381;  // 0x150E5 1byte
constexpr int entrance_floor = 0x15406;     // 0x1516A 1byte
constexpr int entrance_dungeon = 0x1548B;   // 0x151EF 1byte (dungeon id)
constexpr int entrance_door = 0x15510;      // 0x15274 1byte
// 1 byte, ---b ---a b = bg2, a = need to check -_-
constexpr int entrance_ladderbg = 0x15595;        // 0x152F9
constexpr int entrance_scrolling = 0x1561A;       // 0x1537E //1byte --h- --v-
constexpr int entrance_scrollquadrant = 0x1569F;  // 0x15403 1byte
constexpr int entrance_exit = 0x15724;            // 0x15488 //2byte word
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

// 1 byte, ---b ---a b = bg2, a = need to check -_-
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
constexpr int bedPositionResetXHigh = 0x02DE58;  //^^^^^^

// short value (on 2 different bytes)
constexpr int bedPositionResetYLow = 0x02DE5D;
constexpr int bedPositionResetYHigh = 0x02DE62;  //^^^^^^

constexpr int bedSheetPositionX = 0x0480BD;  // short value
constexpr int bedSheetPositionY = 0x0480B8;  // short value

//===========================================================================================
// Gravestones related variables
//===========================================================================================

constexpr int GravesYTilePos = 0x49968;    // short (0x0F entries)
constexpr int GravesXTilePos = 0x49986;    // short (0x0F entries)
constexpr int GravesTilemapPos = 0x499A4;  // short (0x0F entries)
constexpr int GravesGFX = 0x499C2;         // short (0x0F entries)

constexpr int GravesXPos = 0x4994A;      // short (0x0F entries)
constexpr int GravesYLine = 0x4993A;     // short (0x08 entries)
constexpr int GravesCountOnY = 0x499E0;  // Byte 0x09 entries

constexpr int GraveLinkSpecialHole = 0x46DD9;    // short
constexpr int GraveLinkSpecialStairs = 0x46DE0;  // short

//===========================================================================================
// Palettes Related Variables - This contain all the palettes of the game
//===========================================================================================
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

//===========================================================================================
// Dungeon Map Related Variables
//===========================================================================================
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

//===========================================================================================
// Names
//===========================================================================================
static const std::string RoomEffect[] = {"Nothing",
                                         "Nothing",
                                         "Moving Floor",
                                         "Moving Water",
                                         "Trinexx Shell",
                                         "Red Flashes",
                                         "Light Torch to See Floor",
                                         "Ganon's Darkness"};

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

static const std::string SecretItemNames[] = {
    "Nothing",    "Green Rupee", "Rock hoarder",  "Bee",       "Health pack",
    "Bomb",       "Heart ",      "Blue Rupee",

    "Key",        "Arrow",       "Bomb",          "Heart",     "Magic",
    "Full Magic", "Cucco",       "Green Soldier", "Bush Stal", "Blue Soldier",

    "Landmine",   "Heart",       "Fairy",         "Heart",
    "Nothing ",  // 22

    "Hole",       "Warp",        "Staircase",     "Bombable",  "Switch"};

static const std::string Type1RoomObjectNames[] = {
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

static const std::string Type2RoomObjectNames[] = {
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

static const std::string Type3RoomObjectNames[] = {
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

static const std::string TileTypeNames[] = {
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

}  // namespace core
}  // namespace app
}  // namespace yaze

#endif