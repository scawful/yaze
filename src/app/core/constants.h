#ifndef YAZE_APP_CORE_CONSTANTS_H
#define YAZE_APP_CORE_CONSTANTS_H

#include <vector>

#include "absl/strings/string_view.h"

#define TAB_BAR(w) if (ImGui::BeginTabBar(w)) {
#define END_TAB_BAR() \
  ImGui::EndTabBar(); \
  }

#define TAB_ITEM(w) if (ImGui::BeginTabItem(w)) {
#define END_TAB_ITEM() \
  ImGui::EndTabItem(); \
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

#define HOVER_HINT(string) \
  if (ImGui::IsItemHovered()) ImGui::SetTooltip(string);

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

#define RETURN_VOID_IF_ERROR(expression)          \
  {                                               \
    auto error = expression;                      \
    if (!error.ok()) {                            \
      std::cout << error.ToString() << std::endl; \
      return;                                     \
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

#define ASSIGN_OR_LOG_ERROR(type_variable_name, expression)         \
  ASSIGN_OR_LOG_ERROR_IMPL(APPEND_NUMBER(error_or_value, __LINE__), \
                           type_variable_name, expression)

#define ASSIGN_OR_LOG_ERROR_IMPL(error_or_value, type_variable_name, \
                                 expression)                         \
  auto error_or_value = expression;                                  \
  if (!error_or_value.ok()) {                                        \
    std::cout << error_or_value.status().ToString() << std::endl;    \
  }                                                                  \
  type_variable_name = std::move(*error_or_value);

#define APPEND_NUMBER(expression, number) \
  APPEND_NUMBER_INNER(expression, number)

#define APPEND_NUMBER_INNER(expression, number) expression##number

#define TEXT_WITH_SEPARATOR(text) \
  ImGui::Text(text);              \
  ImGui::Separator();

#define TABLE_BORDERS_RESIZABLE \
  ImGuiTableFlags_Borders | ImGuiTableFlags_Resizable

#define CLEAR_AND_RETURN_STATUS(status) \
  if (!status.ok()) {                   \
    auto temp = status;                 \
    status = absl::OkStatus();          \
    return temp;                        \
  }

using ushort = unsigned short;
using uint = unsigned int;
using uchar = unsigned char;

namespace yaze {
namespace app {
namespace core {

constexpr uint32_t kRedPen = 0xFF0000FF;
constexpr std::string_view kYazeVersion = "0.2.0";

// ============================================================================
// Magic numbers
// ============================================================================

constexpr int UncompressedSheetSize = 0x0800;

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
// Names
// ============================================================================

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

static const std::string kSpriteDefaultNames[]{
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

static const std::string overlordnames[] = {
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