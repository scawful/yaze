#ifndef YAZE_APP_ZELDA3_COMMON_H
#define YAZE_APP_ZELDA3_COMMON_H

#include <vector>
#include <cstdint>
#include <string>

namespace yaze {
namespace app {
/**
 * @namespace yaze::app::zelda3
 * @brief Zelda 3 specific classes and functions.
 */
namespace zelda3 {

/**
 * @brief Represents tile32 data for the overworld.
 */
using OWBlockset = std::vector<std::vector<uint16_t>>;

/**
 * @brief Overworld map tile32 data.
 */
struct OWMapTiles {
  OWBlockset light_world;    // 64 maps
  OWBlockset dark_world;     // 64 maps
  OWBlockset special_world;  // 32 maps
};
using OWMapTiles = struct OWMapTiles;

/**
 * @class GameEntity
 * @brief Base class for all overworld and dungeon entities.
 */
class GameEntity {
 public:
  enum EntityType {
    kEntrance = 0,
    kExit = 1,
    kItem = 2,
    kSprite = 3,
    kTransport = 4,
    kMusic = 5,
    kTilemap = 6,
    kProperties = 7,
    kDungeonSprite = 8,
  } entity_type_;
  int x_;
  int y_;
  int game_x_;
  int game_y_;
  int entity_id_;
  uint16_t map_id_;

  auto set_x(int x) { x_ = x; }
  auto set_y(int y) { y_ = y; }

  GameEntity() = default;

  virtual void UpdateMapProperties(uint16_t map_id) = 0;
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

}  // namespace zelda3
}  // namespace app
}  // namespace yaze

#endif  // YAZE_APP_ZELDA3_COMMON_H
