#include "zelda3/dungeon/room_collision.h"

#include <algorithm>
#include <array>
#include <cstdint>
#include <vector>

#include "rom/rom.h"
#include "zelda3/dungeon/object_layer_semantics.h"
#include "zelda3/dungeon/room.h"
#include "zelda3/dungeon/room_object.h"

namespace yaze::zelda3 {
namespace {

// ROM tables (PC offsets, vanilla addresses from usdasm).
constexpr int kUnderworldTileTypes = 0x071659;        // $0E:9659, 0x180 bytes
constexpr int kCustomTileTypesOffset = 0x071000;      // $0E:9000, 21 words
constexpr int kCustomUnderworldTileTypes = 0x07102A;  // $0E:902A
constexpr int kDungeonMask = 0x0018C0;                // $00:98C0, 16 words
constexpr int kRoomFlagMask = 0x001900;               // $00:9900
constexpr int kDoorPositionsNorthWall = 0x00197E;     // $00:997E
constexpr int kDoorPositionsNorthMiddle = 0x00198A;   // $00:998A
constexpr int kDoorPositionsSouthMiddle = 0x001996;   // $00:9996
constexpr int kDoorPositionsWestWall = 0x0019AE;      // $00:99AE
constexpr int kDoorPositionsWestMiddle = 0x0019BA;    // $00:99BA
constexpr int kDoorPositionsEastMiddle = 0x0019C6;    // $00:99C6
constexpr int kExplodingWallPositions = 0x0019DE;     // $00:99DE
constexpr int kDoorwayReplacementDoorGfx = 0x001A02;  // $00:9A02
constexpr int kDoorwayTileProperties = 0x001A52;      // $00:9A52

uint8_t RomByte(const Rom& rom, int pc) {
  if (pc < 0 || static_cast<size_t>(pc) >= rom.size()) {
    return 0;
  }
  return rom.data()[pc];
}

uint16_t RomWord(const Rom& rom, int pc) {
  return static_cast<uint16_t>(RomByte(rom, pc) | (RomByte(rom, pc + 1) << 8));
}

// The low WRAM ($0000-$1FFF) the draw routines and attribute passes share.
// Lists overlap in the real game ($06B0 stairs run into $06B8, chests at
// $06E0 into $06EC), so they live in one byte array like the game's.
class LowWram {
 public:
  uint16_t Word(int address) const {
    return static_cast<uint16_t>(bytes_[address & 0x1FFF] |
                                 (bytes_[(address + 1) & 0x1FFF] << 8));
  }
  void SetWord(int address, uint16_t value) {
    bytes_[address & 0x1FFF] = static_cast<uint8_t>(value & 0xFF);
    bytes_[(address + 1) & 0x1FFF] = static_cast<uint8_t>(value >> 8);
  }

 private:
  std::array<uint8_t, 0x2000> bytes_{};
};

// Low WRAM addresses (usdasm wram.asm / bank_01 comments).
constexpr int kDoorListIndex = 0x0460;
constexpr int kDoorTypes = 0x1980;      // (slot << 8) | type
constexpr int kDoorPositions = 0x19A0;  // tilemap offset (+$2000 = BG2)
constexpr int kDoorDirections = 0x19C0;
constexpr int kExitDoorCount = 0x19E0;
constexpr int kExitDoorPositions = 0x19E2;  // 4 words
constexpr int kDoorOpenMask = 0x068C;
constexpr int kRoomFlags = 0x0402;
constexpr int kShutterController = 0x0468;
constexpr int kLayerToggleCount = 0x044E;    // list at $06C0
constexpr int kDungeonToggleCount = 0x0450;  // list at $06D0
constexpr int kDoorSwitch = 0x04B0;
constexpr int kStarCount = 0x0432;         // list at $06A0
constexpr int kManipulableCount = 0x042C;  // $0500 type, $0540 position
constexpr int kTorchEnd = 0x042E;
constexpr int kChestCount = 0x0496;  // list at $06E0
constexpr int kBigKeyLockEnd = 0x0498;

struct RomTables {
  explicit RomTables(const Rom& rom) : rom(rom) {}
  uint16_t DungeonMask(int x) const {
    return RomWord(rom, kDungeonMask + (x & 0xFFFF));
  }
  const Rom& rom;
};

// ---------------------------------------------------------------------------
// Replays the list bookkeeping of the object and door draw routines.

class RoomLoadReplay {
 public:
  RoomLoadReplay(const Rom& rom, const RoomCollisionInput& input,
                 const UnderworldRoomLoadState& state)
      : rom_(rom), tables_(rom), input_(input), state_(state) {
    tag1_ = input.tag1;
    tag2_ = input.tag2;
    // Underworld_LoadHeader (#_01B6D1).
    const uint16_t save = state.room_save_flags;
    w_.SetWord(kDoorOpenMask,
               state.door_open_mask.value_or(
                   static_cast<uint16_t>((save & 0xF000) | 0x0F00)));
    w_.SetWord(kRoomFlags, static_cast<uint16_t>((save & 0x0FF0) << 4));
    w_.SetWord(kShutterController, state.shutter_controller);
  }

  LowWram& wram() { return w_; }
  uint8_t tag1() const { return tag1_; }
  uint8_t tag2() const { return tag2_; }

  // Underworld_LoadRoom (#_01873A): room object lists 0, 1, 2 (doors follow
  // the list that holds the $FFF0 marker, the last one in vanilla), then
  // pushable blocks, then torches.
  void Run() {
    for (int list = 0; list < 3; ++list) {
      for (const auto& object : input_.objects) {
        if (!UsesRoomObjectStream(object) || object.GetLayerValue() != list) {
          continue;
        }
        RecordObject(object, /*lower_layer=*/list == 1);
      }
    }
    for (const auto& door : input_.doors) {
      RecordDoor(door);
    }
    for (const auto& object : input_.objects) {
      if ((object.options() & ObjectOption::Block) != ObjectOption::Nothing) {
        RecordPushableBlock(object);
      }
    }
    // $042E = $042C before the torches draw (#_01889C).
    w_.SetWord(kTorchEnd, w_.Word(kManipulableCount));
    for (const auto& object : input_.objects) {
      if ((object.options() & ObjectOption::Torch) != ObjectOption::Nothing) {
        RecordTorch(object);
      }
    }
  }

 private:
  // ---- objects -----------------------------------------------------------

  static uint16_t TilemapOffset(const RoomObject& object) {
    return static_cast<uint16_t>((object.y() & 0x3F) * 0x80 +
                                 (object.x() & 0x3F) * 2);
  }

  // Sets each counter in `counters` to `value` (the stairs routines keep
  // every later stairs list's end index in step).
  void SetCounters(std::initializer_list<int> counters, uint16_t value) {
    for (int counter : counters) {
      w_.SetWord(counter, value);
    }
  }

  // Appends `value` to a list at `list` whose end index lives at `counter`.
  uint16_t Append(int counter, int list, uint16_t value) {
    const uint16_t x = w_.Word(counter);
    w_.SetWord(list + x, value);
    return static_cast<uint16_t>(x + 2);
  }

  // DrawBigGraySegment / RoomDraw_SinglePot / RoomDraw_HammerPegSingle
  // (#_01B33A, #_01B395, #_01B493): $0500 = type, $0540 = position.
  void AddManipulable(uint16_t type, uint16_t y, bool lower_layer) {
    const uint16_t x = w_.Word(kManipulableCount);
    w_.SetWord(0x0500 + x, type);
    w_.SetWord(kManipulableCount, static_cast<uint16_t>(x + 2));
    w_.SetWord(0x0520 + x, 0);  // $BA, the object stream offset (unused)
    w_.SetWord(0x0540 + x, lower_layer ? (y | 0x2000) : y);
  }

  bool WaterFlagSet() const {
    // LDA $0403 : AND DoorFlagMasks+1 (8-bit) = $0402 & 0x0800.
    return (w_.Word(kRoomFlags) & 0x0800) != 0;
  }
  bool DamGateUnopened() const {
    // $AF == 0x1B and the room's save word bit 0x0100 clear.
    return tag2_ == 0x1B && (state_.room_save_flags & 0x0100) == 0;
  }
  // RoomDraw_WaterOverlayA/B: turns the "merged" water stairs into the
  // regular ones.
  void MoveWaterStairLists() {
    w_.SetWord(0x0440, w_.Word(0x0442));
    w_.SetWord(0x0448, w_.Word(0x0444));
    w_.SetWord(0x0444, 0);
    w_.SetWord(0x0442, 0);
    w_.SetWord(0x049E, w_.Word(0x04AE));
    w_.SetWord(0x04AE, 0);
  }

  void RecordObject(const RoomObject& object, bool lower_layer) {
    const uint16_t y = TilemapOffset(object);
    const uint16_t layer_y = lower_layer ? (y | 0x2000) : y;
    const int count = (object.size() & 0x0F) + 1;  // RoomDraw_GetSize_1to16
    switch (object.id_) {
      case 0x035: {  // RoomDraw_DoorSwitcherer (#_01913F)
        uint16_t value = layer_y;
        if ((w_.Word(kRoomFlags) & 0x1000) == 0) {
          value |= 0x8000;  // not drawn: 5 columns instead of 6
        }
        w_.SetWord(kDoorSwitch, value);
        break;
      }
      case 0x095:  // RoomDraw_DownwardsPots2x2_1to16
        for (int i = 0; i < count; ++i) {
          AddManipulable(0x1111, static_cast<uint16_t>(y + i * 0x100),
                         lower_layer);
        }
        break;
      case 0x0BC:  // RoomDraw_RightwardsPots2x2_1to16
        for (int i = 0; i < count; ++i) {
          AddManipulable(0x1111, static_cast<uint16_t>(y + i * 4), lower_layer);
        }
        break;
      case 0x096:  // RoomDraw_DownwardsHammerPegs2x2_1to16
        for (int i = 0; i < count; ++i) {
          AddManipulable(0x4040, static_cast<uint16_t>(y + i * 0x100),
                         lower_layer);
        }
        break;
      case 0x0BD:  // RoomDraw_RightwardsHammerPegs2x2_1to16
        for (int i = 0; i < count; ++i) {
          AddManipulable(0x4040, static_cast<uint16_t>(y + i * 4), lower_layer);
        }
        break;
      case 0xF96:  // RoomDraw_HammerPegSingle (0x216)
        AddManipulable(0x4040, y, lower_layer);
        break;
      case 0xFAF:  // RoomDraw_SinglePot (0x22F)
        AddManipulable(0x1111, y, lower_layer);
        break;
      case 0xFAB:  // RoomDraw_WeirdGloveRequiredPot (0x22B)
        AddManipulable(0x1010, y, lower_layer);
        break;
      case 0xFB0:  // RoomDraw_WeirdUglyPot (0x230)
        AddManipulable(0x1212, y, lower_layer);
        break;
      case 0xFAC:  // RoomDraw_BigGrayRock (0x22C): four 2x2 segments
        AddManipulable(0x2020, y, lower_layer);
        AddManipulable(0x2121, static_cast<uint16_t>(y + 4), lower_layer);
        AddManipulable(0x2222, static_cast<uint16_t>(y + 0x100), lower_layer);
        AddManipulable(0x2323, static_cast<uint16_t>(y + 0x104), lower_layer);
        break;
      case 0xFC7:  // RoomDraw_BombableFloor (0x247)
        if (input_.room_id == 0x65 && (w_.Word(kRoomFlags) & 0x1000) != 0) {
          break;  // open Thieves' Town attic floor: a plain hole
        }
        AddManipulable(0x3030, y, lower_layer);
        AddManipulable(0x3131, static_cast<uint16_t>(y + 4), lower_layer);
        AddManipulable(0x3232, static_cast<uint16_t>(y + 0x100), lower_layer);
        AddManipulable(0x3333, static_cast<uint16_t>(y + 0x104), lower_layer);
        break;
      case 0x0D8:  // RoomDraw_WaterOverlayA8x8_1to16 (#_019501)
        if (WaterFlagSet()) {
          tag2_ = 0;
          MoveWaterStairLists();
        }
        break;
      case 0x0DA:  // RoomDraw_WaterOverlayB8x8_1to16 (#_0195EF)
        if (WaterFlagSet()) {
          tag2_ = 0;
        } else {
          MoveWaterStairLists();
        }
        break;
      case 0x11F:  // RoomDraw_EnabledStarSwitch (#_019A6F)
        w_.SetWord(kStarCount, Append(kStarCount, 0x06A0, layer_y >> 1));
        break;
      case 0x12D:  // RoomDraw_InterRoomFatStairsUp (#_01A41B)
        SetCounters({0x0438, 0x047E, 0x0482, 0x04A2, 0x04A4, 0x043A, 0x0480,
                     0x0484, 0x04A6, 0x04A8},
                    Append(0x0438, 0x06B0, layer_y >> 1));
        break;
      case 0x12E:  // RoomDraw_InterRoomFatStairsDown_A
      case 0x12F:  // RoomDraw_InterRoomFatStairsDown_B
        SetCounters({0x043A, 0x0480, 0x0484, 0x04A6, 0x04A8},
                    Append(0x043A, 0x06B0, layer_y >> 1));
        break;
      case 0x138: {  // RoomDraw_SpiralStairsGoingUpUpper (#_01A4B4)
        const uint16_t v = static_cast<uint16_t>(
            (static_cast<uint16_t>(y - 0x80) | (lower_layer ? 0x2000 : 0)) >>
            1);
        SetCounters({0x047E, 0x0482, 0x04A2, 0x04A4, 0x043A, 0x0480, 0x0484,
                     0x04A6, 0x04A8},
                    Append(0x047E, 0x06B0, v));
        break;
      }
      case 0x13A: {  // RoomDraw_SpiralStairsGoingUpLower
        const uint16_t v = static_cast<uint16_t>(
            (static_cast<uint16_t>(y - 0x80) | (lower_layer ? 0x2000 : 0)) >>
            1);
        SetCounters(
            {0x0482, 0x04A2, 0x04A4, 0x043A, 0x0480, 0x0484, 0x04A6, 0x04A8},
            Append(0x0482, 0x06B0, v));
        break;
      }
      case 0x139: {  // RoomDraw_SpiralStairsGoingDownUpper
        const uint16_t v = static_cast<uint16_t>(
            (static_cast<uint16_t>(y - 0x80) | (lower_layer ? 0x2000 : 0)) >>
            1);
        SetCounters({0x0480, 0x0484, 0x04A6, 0x04A8},
                    Append(0x0480, 0x06B0, v));
        break;
      }
      case 0x13B: {  // RoomDraw_SpiralStairsGoingDownLower
        const uint16_t v = static_cast<uint16_t>(
            (static_cast<uint16_t>(y - 0x80) | (lower_layer ? 0x2000 : 0)) >>
            1);
        SetCounters({0x0484, 0x04A6, 0x04A8}, Append(0x0484, 0x06B0, v));
        break;
      }
      // Straight inter-room stairs. The "Upper" variants ignore the layer.
      case 0xF9E:  // RoomDraw_StraightInterroomStairsGoingUpNorthUpper
        SetCounters({0x04A2, 0x04A4, 0x043A, 0x0480, 0x0484, 0x04A6, 0x04A8},
                    Append(0x04A2, 0x06B0, y >> 1));
        break;
      case 0xFA6:  // RoomDraw_StraightInterroomStairsGoingUpNorthLower
        SetCounters({0x04A2, 0x04A4, 0x043A, 0x0480, 0x0484, 0x04A6, 0x04A8},
                    Append(0x04A2, 0x06B0, layer_y >> 1));
        break;
      case 0xF9F:  // RoomDraw_StraightInterroomStairsGoingDownNorthUpper
        SetCounters({0x04A6, 0x04A8}, Append(0x04A6, 0x06B0, y >> 1));
        break;
      case 0xFA7:  // RoomDraw_StraightInterroomStairsGoingDownNorthLower
        SetCounters({0x04A6, 0x04A8}, Append(0x04A6, 0x06B0, layer_y >> 1));
        break;
      case 0xFA0:  // RoomDraw_StraightInterroomStairsGoingUpSouthUpper
        SetCounters({0x04A4, 0x043A, 0x0480, 0x0484, 0x04A6, 0x04A8},
                    Append(0x04A4, 0x06B0, y >> 1));
        break;
      case 0xFA8:  // RoomDraw_StraightInterroomStairsGoingUpSouthLower
        SetCounters({0x04A4, 0x043A, 0x0480, 0x0484, 0x04A6, 0x04A8},
                    Append(0x04A4, 0x06B0, layer_y >> 1));
        break;
      case 0xFA1:  // RoomDraw_StraightInterroomStairsGoingDownSouthUpper
        SetCounters({0x04A8}, Append(0x04A8, 0x06B0, y >> 1));
        break;
      case 0xFA9:  // RoomDraw_StraightInterroomStairsGoingDownSouthLower
        SetCounters({0x04A8}, Append(0x04A8, 0x06B0, layer_y >> 1));
        break;
      // Auto stairs record Y >> 1 without the layer bit.
      case 0x130: {  // RoomDraw_AutoStairs_North_MultiLayer_A (#_01A25D)
        const uint16_t end = Append(0x043C, 0x06B8, y >> 1);
        SetCounters({0x043C, 0x0446, 0x0448}, end);
        break;
      }
      case 0x131: {  // RoomDraw_AutoStairs_North_MultiLayer_B
        const uint16_t end = Append(0x043E, 0x06B8, y >> 1);
        SetCounters({0x043E, 0x0446, 0x0448}, end);
        break;
      }
      case 0x133:  // RoomDraw_AutoStairs_North_MergedLayer_B (#_01A2DF)
        if (!DamGateUnopened()) {
          SetCounters({0x0442, 0x0444}, Append(0x0442, 0x06B8, y >> 1));
          break;
        }
        [[fallthrough]];  // AutoStairsNorthMergedStart
      case 0x132:         // RoomDraw_AutoStairs_North_MergedLayer_A (#_01A2C7)
        SetCounters({0x0440, 0x0446, 0x0448}, Append(0x0440, 0x06B8, y >> 1));
        break;
      case 0x135:  // RoomDraw_WaterHopStairs_A (#_019B1E)
        if (!DamGateUnopened()) {
          w_.SetWord(0x0444, Append(0x0444, 0x06B8, y >> 1));
          break;
        }
        [[fallthrough]];
      case 0x136:  // RoomDraw_WaterHopStairs_B (#_01A3AE)
        SetCounters({0x0446, 0x0448}, Append(0x0446, 0x06B8, y >> 1));
        break;
      case 0xF9B:  // RoomDraw_AutoStairs_South_MultiLayer_A (#_01A30C)
        // Vanilla stores this one in the north list ($06B8) but counts it in
        // $049A, which the attribute pass reads against $06EC.
        w_.SetWord(0x049A, Append(0x049A, 0x06B8, y >> 1));
        break;
      case 0xF9C:  // RoomDraw_AutoStairs_South_MultiLayer_B
        w_.SetWord(0x049C, Append(0x049C, 0x06EC, y >> 1));
        break;
      case 0xFB3:  // RoomDraw_AutoStairs_South_MergedLayer (#_01A380)
        if (!DamGateUnopened()) {
          w_.SetWord(0x04AE, Append(0x04AE, 0x06EC, y >> 1));
          break;
        }
        [[fallthrough]];  // South_MergedStairs_BecomeMultiC
      case 0xF9D:         // RoomDraw_AutoStairs_South_MultiLayer_C
        w_.SetWord(0x049E, Append(0x049E, 0x06EC, y >> 1));
        break;
      case 0xF98: {  // RoomDraw_BigKeyLock (#_0198AE)
        const uint16_t x = w_.Word(kBigKeyLockEnd);
        const bool opened =
            (w_.Word(kRoomFlags) & RomWord(rom_, kRoomFlagMask + x)) != 0;
        w_.SetWord(0x06E0 + x, opened ? 0 : y);
        w_.SetWord(kBigKeyLockEnd, static_cast<uint16_t>(x + 2));
        break;
      }
      case 0xF99:    // RoomDraw_Chest (#_0198D0)
      case 0xFB1: {  // RoomDraw_BigChest (#_0199BB)
        const uint16_t x = w_.Word(kChestCount);
        uint16_t value = layer_y;
        if (object.id_ == 0xFB1) {
          value |= 0x8000;
        }
        const bool opened =
            (w_.Word(kRoomFlags) & RomWord(rom_, kRoomFlagMask + x)) != 0;
        w_.SetWord(0x06E0 + x, opened ? 0 : value);
        SetCounters({kChestCount, kBigKeyLockEnd},
                    static_cast<uint16_t>(x + 2));
        break;
      }
      default:
        break;
    }
  }

  // RoomDraw_PushableBlock (#_01B4D6): $0500 = 0, $0540 = table word.
  void RecordPushableBlock(const RoomObject& block) {
    const uint16_t x = w_.Word(kManipulableCount);
    w_.SetWord(kManipulableCount, static_cast<uint16_t>(x + 2));
    w_.SetWord(0x0500 + x, 0);
    w_.SetWord(0x0520 + x, 0);
    uint16_t word = TilemapOffset(block);
    if (block.GetLayerValue() & 1) {
      word |= 0x2000;
    }
    word |= static_cast<uint16_t>((block.block_behavior_layer() & 1) << 14);
    w_.SetWord(0x0540 + x, word);
  }

  // RoomDraw_LightableTorch (#_01B509): $0540,$042E = table word.
  void RecordTorch(const RoomObject& torch) {
    const uint16_t x = w_.Word(kTorchEnd);
    uint16_t word = TilemapOffset(torch);
    if (torch.GetLayerValue() & 1) {
      word |= 0x2000;
    }
    if (torch.lit_) {
      word |= 0x8000;
    }
    w_.SetWord(0x0540 + x, word);
    w_.SetWord(kTorchEnd, static_cast<uint16_t>(x + 2));
  }

  // ---- doors -------------------------------------------------------------

  // Door routine registers: $00 (door word), $02 (position * 2), $04 (type),
  // $0A (graphics type), $08 (tilemap position).
  struct DoorRegs {
    uint16_t word = 0;
    uint16_t position = 0;
    uint16_t type = 0;
    uint16_t gfx = 0;
  };

  struct FlagResult {
    bool carry = false;
    uint16_t gfx = 0;  // returned in Y
    uint16_t x = 0;    // X on return
  };

  // RoomDraw_FlagDoorsAndGetFinalType (#_01B0DA).
  FlagResult FlagDoors(uint16_t x, uint16_t direction, uint16_t y) {
    w_.SetWord(kDoorDirections + x, direction);
    w_.SetWord(kDoorPositions + x, y);
    w_.SetWord(kDoorTypes + x,
               static_cast<uint16_t>(((x >> 1) << 8) | (regs_.type & 0xFF)));
    const uint16_t slot = x & 0x0F;
    if (slot < 8 && (w_.Word(kDoorOpenMask) & tables_.DungeonMask(slot))) {
      const uint16_t type = w_.Word(kDoorTypes + x) & 0xFF;
      const bool shutter = type == 0x18 || type == 0x44;
      if (!(shutter && w_.Word(kShutterController) != 0)) {
        regs_.gfx = RomWord(rom_, kDoorwayReplacementDoorGfx + regs_.type);
      }
    }
    FlagResult result;
    result.gfx = regs_.gfx;
    x = static_cast<uint16_t>(x + 2);
    w_.SetWord(kDoorListIndex, x);
    result.x = x;
    if (result.gfx == 0x32 || result.gfx == 0x08) {
      return result;  // CLC_and_EXIT
    }
    if (regs_.type == 0x1A) {
      // Both compares against $2F fall through to .flag_shutter_door.
      x = static_cast<uint16_t>(x - 2);
      w_.SetWord(0x0436,
                 static_cast<uint16_t>((x << 8) | ((regs_.word & 0x03) << 1)));
      w_.SetWord(kDoorOpenMask, static_cast<uint16_t>(w_.Word(kDoorOpenMask) |
                                                      tables_.DungeonMask(x)));
      result.gfx = 0;
      result.x = x;
    }
    result.carry = true;
    return result;
  }

  // Rewrites the type of the door just flagged ($197E,X after the INX INX).
  void RewriteDoorType(const FlagResult& flag, uint16_t type) {
    const int address = 0x197E + flag.x;
    w_.SetWord(address,
               static_cast<uint16_t>((w_.Word(address) & 0xFF00) | type));
  }

  // RoomDraw_ChangeTilemapAddressToLowerLayer (#_01A8FA).
  void MoveLastDoorToLowerLayer() {
    const int address = 0x199E + w_.Word(kDoorListIndex);
    w_.SetWord(address, static_cast<uint16_t>(w_.Word(address) | 0x2000));
  }

  // RoomDraw_MarkLayerToggleDoor / RoomDraw_MarkDungeonToggleDoor.
  void MarkToggleDoor(int counter, int list, uint16_t position) {
    w_.SetWord(counter, Append(counter, list, position >> 1));
  }

  void AddExitDoor(uint16_t y) {
    const uint16_t x = w_.Word(kExitDoorCount);
    w_.SetWord(kExitDoorPositions + x, y);
    w_.SetWord(kExitDoorCount, static_cast<uint16_t>(x + 2));
  }

  uint16_t Slot() const { return w_.Word(kDoorListIndex); }

  // The far side of a door on a middle seam goes in slot | 0x10 and does not
  // advance $0460.
  template <typename Fn>
  void WithPartnerSlot(Fn fn) {
    const uint16_t saved = Slot();
    w_.SetWord(kDoorListIndex, static_cast<uint16_t>(saved | 0x10));
    fn();
    w_.SetWord(kDoorListIndex, saved);
  }

  void OneSidedShuttersNorth(uint16_t y) {
    const auto flag = FlagDoors(Slot(), 0, y);
    if (flag.carry) {
      if (flag.gfx == 0x36) {
        RewriteDoorType(flag, 0x18);
      } else if (flag.gfx == 0x38) {
        RewriteDoorType(flag, 0x00);
      }
    }
  }
  void OneSidedShuttersSouth(uint16_t y) {
    const auto flag = FlagDoors(Slot(), 1, y);
    if (flag.carry) {
      if (flag.gfx == 0x1E || flag.gfx == 0x36) {
        RewriteDoorType(flag, 0x00);
      } else if (flag.gfx == 0x38) {
        RewriteDoorType(flag, 0x18);
      }
    }
  }
  void OneSidedShuttersWest(uint16_t y) {
    const auto flag = FlagDoors(Slot(), 2, y);
    if (flag.carry) {
      if (flag.gfx == 0x36) {
        RewriteDoorType(flag, 0x18);
      } else if (flag.gfx == 0x38) {
        RewriteDoorType(flag, 0x00);
      }
    }
  }
  void OneSidedShuttersEast(uint16_t y) {
    const auto flag = FlagDoors(Slot(), 3, y);
    if (flag.carry) {
      if (flag.gfx == 0x36) {
        RewriteDoorType(flag, 0x00);
      } else if (flag.gfx == 0x38) {
        RewriteDoorType(flag, 0x18);
      }
    }
  }
  // RoomDraw_OneSidedLowerShutters_*: the rewrite ignores the carry.
  void OneSidedLowerShutters(uint16_t direction, uint16_t y) {
    const auto flag = FlagDoors(Slot(), direction, y);
    const bool north_or_west = direction == 0 || direction == 2;
    if (flag.gfx == 0x48) {
      RewriteDoorType(flag, north_or_west ? 0x44 : 0x40);
    } else if (flag.gfx == 0x4A) {
      RewriteDoorType(flag, north_or_west ? 0x40 : 0x44);
    }
    MoveLastDoorToLowerLayer();
  }

  // RoomDraw_CheckIfLowerLayerDoors_Vertical (#_01AA66).
  void CheckIfLowerLayerDoorsVertical(uint16_t type, uint16_t y) {
    OneSidedShuttersSouth(y);
    if (type == 0x08) {
      MoveLastDoorToLowerLayer();
    }
  }

  // RoomDraw_NormalRangedDoors_North (#_01A90F).
  void NormalRangedDoorsNorth(uint16_t y) {
    if (regs_.position >= 0x0C) {
      WithPartnerSlot([&] {
        CheckIfLowerLayerDoorsVertical(
            regs_.type,
            RomWord(rom_, kDoorPositionsNorthMiddle + regs_.position));
      });
      regs_.gfx = regs_.type;
    }
    OneSidedShuttersNorth(y);
  }

  // RoomDraw_NormalRangedDoors_East (#_01ABC8).
  void NormalRangedDoorsEast(uint16_t type, uint16_t y) {
    OneSidedShuttersEast(y);
    if (type == 0x08) {
      MoveLastDoorToLowerLayer();
    }
  }

  // RoomDraw_NormalRangedDoors_West (#_01AB1F).
  void NormalRangedDoorsWest(uint16_t y) {
    if (regs_.position >= 0x0C) {
      WithPartnerSlot([&] {
        NormalRangedDoorsEast(
            regs_.type,
            RomWord(rom_, kDoorPositionsWestMiddle + regs_.position));
      });
      regs_.gfx = regs_.type;
    }
    OneSidedShuttersWest(y);
  }

  // RoomDraw_Door_ExplodingWall (#_01AC70).
  void ExplodingWall() {
    const uint16_t y = RomWord(rom_, kExplodingWallPositions + regs_.position);
    const uint16_t x = Slot();
    w_.SetWord(kDoorPositions + x, static_cast<uint16_t>(y + 0x14));
    w_.SetWord(kDoorTypes + x, static_cast<uint16_t>(((x >> 1) << 8) | 0x30));
    if ((w_.Word(kDoorOpenMask) & tables_.DungeonMask(x & 0x0F)) == 0) {
      w_.SetWord(kDoorDirections + x, 0);
    } else if (tag1_ == 0x20 || tag1_ == 0x25 || tag1_ == 0x28) {
      tag1_ = 0;
    } else {
      tag2_ = 0;
    }
    w_.SetWord(kDoorListIndex, static_cast<uint16_t>(x + 2));
  }

  // RoomDraw_Door_North (#_01A81C).
  void DoorNorth() {
    const uint16_t y = RomWord(rom_, kDoorPositionsNorthWall + regs_.position);
    const uint16_t t = regs_.type;
    if (t == 0x30) {
      ExplodingWall();
    } else if (t == 0x16) {
      MarkToggleDoor(kLayerToggleCount, 0x06C0,
                     static_cast<uint16_t>(y - 0xFE));
    } else if (t == 0x32) {  // RoomDraw_NorthCurtainDoor
      FlagDoors(Slot(), 0, y);
    } else if (t == 0x06) {
      // RoomDraw_MakeDoorHighPriorityLowerLayer_North flags the slot in X,
      // which still holds the position index here, not $0460.
      FlagDoors(regs_.position, 0, y);
    } else if (t == 0x14) {
      MarkToggleDoor(kDungeonToggleCount, 0x06D0,
                     static_cast<uint16_t>(y - 0xFE));
    } else if (t == 0x02) {
      NormalRangedDoorsNorth(y);
    } else if (t == 0x12) {
      AddExitDoor(y);
    } else if (t == 0x08) {
      NormalRangedDoorsNorth(y);
      MoveLastDoorToLowerLayer();
    } else if (t == 0x20 || t == 0x22 || t == 0x24 || t == 0x26) {
      const uint16_t x = Slot();
      w_.SetWord(kDoorDirections + x, 0);
      w_.SetWord(kDoorPositions + x, y);
      w_.SetWord(kDoorTypes + x, static_cast<uint16_t>(((x >> 1) << 8) | t));
      if (tables_.DungeonMask(x & 0x0F) & w_.Word(kDoorOpenMask)) {
        w_.SetWord(kDoorListIndex, static_cast<uint16_t>(x + 2));
      } else if (t < 0x24) {
        OneSidedShuttersNorth(y);
      } else {
        FlagDoors(Slot(), 0, y);
        MoveLastDoorToLowerLayer();
      }
    } else if (t >= 0x40) {  // RoomDraw_HighRangeDoor_North (#_01AD41)
      if (regs_.position >= 0x0C && t != 0x46) {
        WithPartnerSlot([&] {
          OneSidedLowerShutters(
              1, RomWord(rom_, kDoorPositionsNorthMiddle + regs_.position));
        });
      }
      OneSidedLowerShutters(0, y);
    } else {
      NormalRangedDoorsNorth(y);
    }
  }

  // RoomDraw_Door_South (#_01A984).
  void DoorSouth() {
    uint16_t y = RomWord(rom_, kDoorPositionsSouthMiddle + regs_.position);
    const uint16_t t = regs_.type;
    if (t == 0x16) {
      MarkToggleDoor(kLayerToggleCount, 0x06C0,
                     static_cast<uint16_t>(y + 0x202));
    } else if (t == 0x06) {
      FlagDoors(regs_.position, 1, y);
    } else if (t == 0x14) {
      MarkToggleDoor(kDungeonToggleCount, 0x06D0,
                     static_cast<uint16_t>(y + 0x202));
    } else if (t == 0x12) {
      AddExitDoor(y);
    } else if (t >= 0x40) {
      OneSidedLowerShutters(1, y);
    } else if (t == 0x0A || t == 0x0E || t == 0x10) {
      // Fancy dungeon exit / RoomDraw_CaveExitLight /
      // RoomDraw_HighPriorityExitLight.
      FlagDoors(Slot(), 1, y);
    } else if (t == 0x0C || t == 0x04) {
      // Lower-layer fancy exit / RoomDraw_CheckIfExitDoor: Y | $2000.
      y |= 0x2000;
      FlagDoors(Slot(), 1, y);
    } else {
      CheckIfLowerLayerDoorsVertical(t, y);
    }
  }

  // RoomDraw_Door_West (#_01AAD7).
  void DoorWest() {
    const uint16_t y = RomWord(rom_, kDoorPositionsWestWall + regs_.position);
    const uint16_t t = regs_.type;
    if (t == 0x16) {
      MarkToggleDoor(kLayerToggleCount, 0x06C0,
                     static_cast<uint16_t>(y + 0x7C));
    } else if (t == 0x06) {
      FlagDoors(regs_.position, 2, y);
    } else if (t == 0x14) {
      MarkToggleDoor(kDungeonToggleCount, 0x06D0,
                     static_cast<uint16_t>(y + 0x7C));
    } else if (t == 0x08) {
      NormalRangedDoorsWest(y);
      MoveLastDoorToLowerLayer();
    } else if (t >= 0x40) {  // RoomDraw_HighRangeDoor_West (#_01AE40)
      if (regs_.position >= 0x0C) {
        WithPartnerSlot([&] {
          OneSidedLowerShutters(
              3, RomWord(rom_, kDoorPositionsWestMiddle + regs_.position));
        });
      }
      OneSidedLowerShutters(2, y);
    } else {
      NormalRangedDoorsWest(y);
    }
  }

  // RoomDraw_Door_East (#_01AB99).
  void DoorEast() {
    const uint16_t y = RomWord(rom_, kDoorPositionsEastMiddle + regs_.position);
    const uint16_t t = regs_.type;
    if (t == 0x16) {
      MarkToggleDoor(kLayerToggleCount, 0x06C0,
                     static_cast<uint16_t>(y + 0x88));
    } else if (t == 0x06) {
      FlagDoors(regs_.position, 3, y);
    } else if (t == 0x14) {
      MarkToggleDoor(kDungeonToggleCount, 0x06D0,
                     static_cast<uint16_t>(y + 0x88));
    } else if (t >= 0x40) {
      OneSidedLowerShutters(3, y);
    } else {
      NormalRangedDoorsEast(t, y);
    }
  }

  // RoomDraw_DoorObject (#_018916).
  void RecordDoor(const Room::Door& door) {
    regs_.word = static_cast<uint16_t>(door.byte1 | (door.byte2 << 8));
    regs_.position = static_cast<uint16_t>((door.byte1 & 0xF0) >> 3);
    regs_.type = door.byte2;
    regs_.gfx = door.byte2;
    switch (door.byte1 & 0x03) {
      case 0:
        DoorNorth();
        break;
      case 1:
        DoorSouth();
        break;
      case 2:
        DoorWest();
        break;
      default:
        DoorEast();
        break;
    }
  }

  const Rom& rom_;
  RomTables tables_;
  const RoomCollisionInput& input_;
  const UnderworldRoomLoadState& state_;
  LowWram w_;
  DoorRegs regs_;
  uint8_t tag1_ = 0;
  uint8_t tag2_ = 0;
};

// ---------------------------------------------------------------------------
// Attribute passes over the recorded lists.

class AttributeWriter {
 public:
  explicit AttributeWriter(RoomCollisionMaps& maps) : maps_(maps) {}

  // STA.l $7F2000,X (16-bit): two bytes. Bytes outside COLMAPA/B belong to
  // other WRAM and are dropped.
  void Store(int x, uint16_t value, CollisionSource source) {
    StoreByte(x, static_cast<uint8_t>(value & 0xFF), source);
    StoreByte(x + 1, static_cast<uint8_t>(value >> 8), source);
  }
  void StoreByte(int x, uint8_t value, CollisionSource source) {
    if (x < 0 || x >= static_cast<int>(kRoomCollisionBytes)) {
      return;
    }
    maps_.attributes[x] = value;
    maps_.sources[x] = source;
  }
  uint16_t Word(int x) const {
    auto byte = [&](int i) -> uint16_t {
      return (i >= 0 && i < static_cast<int>(kRoomCollisionBytes))
                 ? maps_.attributes[i]
                 : 0;
    };
    return static_cast<uint16_t>(byte(x) | (byte(x + 1) << 8));
  }

 private:
  RoomCollisionMaps& maps_;
};

// Loops `index` from its current value to `end` in steps of 2 (the game's
// INY INY / CPY end / BNE). A list whose end is below the start would run
// through all of WRAM in the game; stop at 0x80 instead.
template <typename Fn>
void ForEachEntry(uint16_t& index, uint16_t end, Fn fn) {
  do {
    fn(index);
    index = static_cast<uint16_t>(index + 2);
  } while (index != end && index < 0x80);
}

// Underworld_LoadObjectAttribute (#_01B967).
void ApplyObjectAttributes(LowWram& w, uint8_t tag1, uint8_t tag2,
                           AttributeWriter& out) {
  using S = CollisionSource;
  uint16_t y = 0;
  if (w.Word(kStarCount) != 0) {
    ForEachEntry(y, w.Word(kStarCount), [&](uint16_t i) {
      const int x = w.Word(0x06A0 + i);
      out.Store(x, 0x3B3B, S::kStar);
      out.Store(x + 0x40, 0x3B3B, S::kStar);
    });
  }

  // Stairs: one list at $06B0 with running end indices; $00 counts up
  // from 0x3030 (0x34.. for the "down" lists).
  uint16_t counter = 0x3030;
  y = 0;
  auto next_counter = [&] {
    counter = static_cast<uint16_t>(counter + 0x101);
  };
  auto stairs = [&](int end_address, auto&& body) {
    const uint16_t end = w.Word(end_address);
    if (y == end) {
      return;
    }
    ForEachEntry(y, end, [&](uint16_t i) { body(w.Word(0x06B0 + i)); });
  };
  auto spiral = [&](uint16_t value) {
    return [&, value](int x) {
      out.Store(x + 0x01, value, S::kStairs);
      out.Store(x + 0x81, value, S::kStairs);
      out.Store(x + 0xC1, value, S::kStairs);
      out.Store(x + 0x41, counter, S::kStairs);
      next_counter();
    };
  };
  auto straight_north = [&](int x) {
    out.Store(x + 0x81, 0x0000, S::kStairs);
    out.Store(x + 0xC1, 0x0000, S::kStairs);
    out.Store(x + 0x01, 0x3838, S::kStairs);
    out.Store(x + 0x41, counter, S::kStairs);
    next_counter();
  };
  auto straight_south = [&](int x) {
    out.Store(x + 0x01, 0x0000, S::kStairs);
    out.Store(x + 0x41, 0x0000, S::kStairs);
    out.Store(x + 0xC1, 0x3939, S::kStairs);
    out.Store(x + 0x81, counter, S::kStairs);
    next_counter();
  };
  if (w.Word(0x0438) != 0) {
    stairs(0x0438, [&](int x) {  // .next_intraroom_stairs_up
      out.Store(x + 0x81, 0x0000, S::kStairs);
      out.Store(x + 0x01, 0x2626, S::kStairs);
      out.Store(x + 0x41, counter, S::kStairs);
      next_counter();
    });
  }
  stairs(0x047E, spiral(0x5E5E));
  stairs(0x0482, spiral(0x5F5F));
  stairs(0x04A2, straight_north);
  stairs(0x04A4, straight_south);
  counter = static_cast<uint16_t>((counter & 0x0707) | 0x3434);
  stairs(0x043A, [&](int x) {  // .next_intra_stairs_south
    out.Store(x + 0xC1, 0x2626, S::kStairs);
    out.Store(x + 0x81, counter, S::kStairs);
    next_counter();
  });
  stairs(0x0480, spiral(0x5E5E));
  stairs(0x0484, spiral(0x5F5F));
  stairs(0x04A6, straight_north);
  stairs(0x04A8, straight_south);

  // Auto north multi-layer / merged stairs ($06B8).
  y = 0;
  uint16_t end = 0;
  uint16_t value = 0;
  if (w.Word(0x043C) != 0) {
    value = 0x1F1F;
    end = w.Word(0x043C);
  } else if (w.Word(0x043E) != 0) {
    value = 0x1E1E;
    end = w.Word(0x043E);
  } else if (w.Word(0x0440) != 0) {
    value = 0x1D1D;
    end = w.Word(0x0440);
  }
  if (value != 0) {
    ForEachEntry(y, end, [&](uint16_t i) {
      const int x = w.Word(0x06B8 + i);
      out.Store(x + 0x0000, 0x0002, S::kLayerStairs);
      out.Store(x + 0x10C0, 0x0002, S::kLayerStairs);
      out.Store(x + 0x0002, 0x0200, S::kLayerStairs);
      out.Store(x + 0x10C2, 0x0200, S::kLayerStairs);
      out.Store(x + 0x0040, 0x0001, S::kLayerStairs);
      out.Store(x + 0x1080, 0x0001, S::kLayerStairs);
      out.Store(x + 0x0042, 0x0100, S::kLayerStairs);
      out.Store(x + 0x1082, 0x0100, S::kLayerStairs);
      out.Store(x + 0x0041, value, S::kLayerStairs);
      out.Store(x + 0x1041, value, S::kLayerStairs);
      out.Store(x + 0x0081, value, S::kLayerStairs);
      out.Store(x + 0x1081, value, S::kLayerStairs);
    });
  }
  if (y != w.Word(0x0448)) {  // .next_water_overlay_a
    ForEachEntry(y, w.Word(0x0448), [&](uint16_t i) {
      const int x = w.Word(0x06B8 + i);
      out.Store(x + 0x0000, 0x0A03, S::kWater);
      out.Store(x + 0x1000, 0x0A03, S::kWater);
      out.Store(x + 0x0002, 0x030A, S::kWater);
      out.Store(x + 0x1002, 0x030A, S::kWater);
      out.Store(x + 0x0040, 0x0803, S::kWater);
      out.Store(x + 0x0042, 0x0308, S::kWater);
    });
  }
  y = 0;
  if (w.Word(0x0442) != 0) {  // .next_water_overlay_a2
    ForEachEntry(y, w.Word(0x0442), [&](uint16_t i) {
      const int x = w.Word(0x06B8 + i);
      out.Store(x + 0x0000, 0x0003, S::kWater);
      out.Store(x + 0x0002, 0x0300, S::kWater);
      out.Store(x + 0x1000, 0x0A03, S::kWater);
      out.Store(x + 0x1002, 0x030A, S::kWater);
      out.Store(x + 0x0040, 0x0808, S::kWater);
      out.Store(x + 0x0042, 0x0808, S::kWater);
    });
  }
  if (y != w.Word(0x0444)) {  // .next_water_ladder
    ForEachEntry(y, w.Word(0x0444), [&](uint16_t i) {
      const int x = w.Word(0x06B8 + i);
      out.Store(x + 0x0000, 0x0003, S::kWater);
      out.Store(x + 0x0002, 0x0300, S::kWater);
      out.Store(x + 0x1000, 0x0A03, S::kWater);
      out.Store(x + 0x1002, 0x030A, S::kWater);
    });
  }

  // Manipulables ($0500 type, $0540 position): 0x70, 0x71, ... skipping
  // bombable floor segments (type 0x3X) but still counting them.
  y = 0;
  if (w.Word(kManipulableCount) != 0) {
    counter = 0x7070;
    ForEachEntry(y, w.Word(kManipulableCount), [&](uint16_t i) {
      if ((w.Word(0x0500 + i) & 0x00F0) != 0x0030) {
        const int x = (w.Word(0x0540 + i) & 0x3FFF) >> 1;
        out.Store(x, counter, S::kManipulable);
        out.Store(x + 0x40, counter, S::kManipulable);
      }
      next_counter();
    });
  }
  // Torches follow in the same $0540 list: 0xC0, 0xC1, ...
  if (y != w.Word(kTorchEnd)) {
    counter = 0xC0C0;
    ForEachEntry(y, w.Word(kTorchEnd), [&](uint16_t i) {
      const int x = (w.Word(0x0540 + i) & 0x3FFF) >> 1;
      out.Store(x, counter, S::kTorch);
      out.Store(x + 0x40, counter, S::kTorch);
      counter = static_cast<uint16_t>((counter & 0xEFEF) + 0x0101);
    });
  }

  // Chests and big key locks ($06E0): 0x58, 0x59, ...
  auto hides_chests = [](uint8_t tag) {
    return tag == 0x27 || tag == 0x3C || tag == 0x3E ||
           (tag >= 0x29 && tag < 0x33);
  };
  counter = 0x5858;
  y = 0;
  bool skip_locks = false;
  if (w.Word(kChestCount) != 0) {
    if (hides_chests(tag1) || hides_chests(tag2)) {
      skip_locks = true;  // BEQ .hidden_chests skips the locks too
    } else {
      // Underworld_SetChestAttributes (#_01BDDB).
      ForEachEntry(y, w.Word(kChestCount), [&](uint16_t i) {
        const uint16_t entry = w.Word(0x06E0 + i);
        if (entry != 0) {
          const int x = (entry & 0x7FFF) >> 1;
          out.Store(x, counter, S::kChest);
          out.Store(x + 0x40, counter, S::kChest);
          if (entry & 0x8000) {  // big chest: 4x4 tiles
            w.SetWord(0x06E0 + i, static_cast<uint16_t>(entry & 0x7FFF));
            out.Store(x + 0x42, counter, S::kChest);
            out.Store(x + 0x80, counter, S::kChest);
            out.Store(x + 0x82, counter, S::kChest);
          }
        }
        next_counter();
      });
    }
  }
  if (!skip_locks && y != w.Word(kBigKeyLockEnd)) {
    ForEachEntry(y, w.Word(kBigKeyLockEnd), [&](uint16_t i) {
      const uint16_t entry = w.Word(0x06E0 + i);
      w.SetWord(0x06E0 + i, static_cast<uint16_t>(entry | 0x8000));
      const int x = (entry & 0x7FFF) >> 1;
      out.Store(x, counter, S::kBigKeyLock);
      out.Store(x + 0x40, counter, S::kBigKeyLock);
      next_counter();
    });
  }

  // Auto south multi-layer stairs ($06EC).
  y = 0;
  value = 0;
  if (w.Word(0x049A) != 0) {
    value = 0x3F3F;
    end = w.Word(0x049A);
  } else if (w.Word(0x049C) != 0) {
    value = 0x3E3E;
    end = w.Word(0x049C);
  } else if (w.Word(0x049E) != 0) {
    value = 0x3D3D;
    end = w.Word(0x049E);
  }
  if (value != 0) {
    ForEachEntry(y, end, [&](uint16_t i) {
      const int x = w.Word(0x06EC + i);
      out.Store(x + 0x1000, 0x0002, S::kLayerStairs);
      out.Store(x + 0x00C0, 0x0002, S::kLayerStairs);
      out.Store(x + 0x1040, 0x0001, S::kLayerStairs);
      out.Store(x + 0x0080, 0x0001, S::kLayerStairs);
      out.Store(x + 0x1002, 0x0200, S::kLayerStairs);
      out.Store(x + 0x00C2, 0x0200, S::kLayerStairs);
      out.Store(x + 0x1042, 0x0100, S::kLayerStairs);
      out.Store(x + 0x0082, 0x0100, S::kLayerStairs);
      out.Store(x + 0x0041, value, S::kLayerStairs);
      out.Store(x + 0x1041, value, S::kLayerStairs);
      out.Store(x + 0x0081, value, S::kLayerStairs);
      out.Store(x + 0x1081, value, S::kLayerStairs);
    });
  }
  y = 0;
  if (w.Word(0x04AE) != 0) {  // .next_south_merged_auto_stairs
    ForEachEntry(y, w.Word(0x04AE), [&](uint16_t i) {
      const int x = w.Word(0x06EC + i);
      out.Store(x + 0x10C0, 0x0A03, S::kWater);
      out.Store(x + 0x10C2, 0x030A, S::kWater);
      out.Store(x + 0x00C0, 0x0003, S::kWater);
      out.Store(x + 0x00C2, 0x0300, S::kWater);
      out.Store(x + 0x0080, 0x0808, S::kWater);
      out.Store(x + 0x0082, 0x0808, S::kWater);
    });
  }
}

// Underworld_LoadDoorAttribute (#_01BE17) and its callees.
void ApplyDoorAttributes(const Rom& rom, LowWram& w, AttributeWriter& out) {
  using S = CollisionSource;
  RomTables tables(rom);
  auto doorway_properties = [&](uint16_t type) {
    return RomWord(rom, kDoorwayTileProperties + type);
  };
  auto is_exit_door = [&](uint16_t position) {
    for (int i = 0; i < 4; ++i) {
      if (position == w.Word(kExitDoorPositions + i * 2)) {
        return true;
      }
    }
    return false;
  };

  for (uint16_t y = 0; y < 0x20; y = static_cast<uint16_t>(y + 2)) {
    const uint16_t position = w.Word(kDoorPositions + y);
    if (position == 0) {
      continue;
    }
    const uint16_t type = w.Word(kDoorTypes + y) & 0x00FE;
    const uint16_t direction = w.Word(kDoorDirections + y) & 0x0003;

    // Closed door: F0 | slot on the two middle rows/columns.
    auto locked = [&] {
      const uint8_t v = static_cast<uint8_t>((y >> 1) | 0xF0);
      const int x = position >> 1;
      out.Store(x + 0x41, static_cast<uint16_t>(v | (v << 8)), S::kLockedDoor);
      out.Store(x + 0x81, static_cast<uint16_t>(v | (v << 8)), S::kLockedDoor);
    };

    // AddFullLongDoorDoorwayProps (#_01C0B8).
    auto full_long = [&] {
      uint16_t v = doorway_properties(type);
      if (direction == 0) {
        const int x = (position >> 1) & 0x783F;
        for (int k = 0; k < 10; ++k) {
          out.Store(x + 0x01 + 0x40 * k, v, S::kDoor);
        }
      } else if (direction == 1) {
        if (type == 0x0C || type == 0x10 || type == 0x04 ||
            is_exit_door(position & 0x1FFF)) {
          v = 0x8E8E;
        }
        const int x = (position >> 1) + 0x40;
        for (int k = 0; k < 8; ++k) {
          out.Store(x + 0x01 + 0x40 * k, v, S::kDoor);
        }
      } else {
        const int x =
            direction == 2 ? ((position >> 1) & 0xFFE0) : (position >> 1) + 1;
        const uint16_t v2 = static_cast<uint16_t>(v + 0x0101);
        for (int o : {0x40, 0x42, 0x44, 0x46, 0x80, 0x82, 0x84, 0x86}) {
          out.Store(x + o, v2, S::kDoor);
        }
      }
    };

    // Underworld_LoadSingleDoorAttribute .apply_doorway (#_01BEB8).
    auto doorway = [&] {
      if (type >= 0x20 && type < 0x28) {
        return;
      }
      uint16_t v = doorway_properties(type);
      if (direction == 0) {
        if (is_exit_door(position)) {
          v = 0x8E8E;
        }
        const int x = (position >> 1) & 0x783F;
        for (int k = 0; k < 7; ++k) {
          out.Store(x + 0x01 + 0x40 * k, v, S::kDoor);
        }
        out.Store(x + 0x1C1, 0x0000, S::kDoor);
      } else if (direction == 1) {
        if (type == 0x0A || type == 0x0E || is_exit_door(position)) {
          v = 0x8E8E;
        }
        const int x = position >> 1;
        for (int o : {0x41, 0x81, 0xC1, 0x101, 0x141}) {
          out.Store(x + o, v, S::kDoor);
        }
      } else if (direction == 2) {
        const int x = (position >> 1) & 0xFFE0;
        const uint16_t v2 = static_cast<uint16_t>(v + 0x0101);
        for (int o : {0x40, 0x42, 0x80, 0x82}) {
          out.Store(x + o, v2, S::kDoor);
        }
        out.Store(x + 0x44, v2 & 0x00FF, S::kDoor);
        out.Store(x + 0x84, v2 & 0x00FF, S::kDoor);
      } else {
        const int x = position >> 1;
        const uint16_t v2 = static_cast<uint16_t>(v + 0x0101);
        for (int o : {0x42, 0x44, 0x82, 0x84}) {
          out.Store(x + o, v2, S::kDoor);
        }
        out.Store(x + 0x40, v2 & 0xFF00, S::kDoor);
        out.Store(x + 0x80, v2 & 0xFF00, S::kDoor);
      }
    };

    const uint16_t open_mask = w.Word(kDoorOpenMask);
    if (type == 0x00 || type == 0x06 || type == 0x12 || type == 0x0A ||
        type == 0x0E) {
      doorway();
    } else if (type == 0x0C || type == 0x10 || type == 0x04 || type == 0x02 ||
               type == 0x08) {
      full_long();
    } else if (type == 0x30) {
      // Exploding wall: nothing until it is blown open.
    } else if (type >= 0x40) {  // AddDoorwayPropsForWeirdos (#_01C085)
      if (type == 0x40 || type == 0x46 ||
          (open_mask & tables.DungeonMask(y & 0xFF))) {
        full_long();
      } else {
        locked();
      }
    } else {
      const uint16_t mask_index =
          type == 0x18 ? (y & 0xFF) : (y & 0x0F);  // 0x44 is handled above.
      if (open_mask & tables.DungeonMask(mask_index)) {
        doorway();
      } else {
        locked();
      }
    }
  }

  // Underworld_LoadSingleDoorTileType (#_01D51F): layer toggle doors OR
  // 0x10, dungeon toggle doors OR 0x20, on the upper map when the tile there
  // is a doorway (0x8X), otherwise on the lower map.
  auto toggles = [&](int counter, int list, uint16_t bits) {
    const uint16_t count = w.Word(counter);
    if (count == 0) {
      return;
    }
    uint16_t y = 0;
    ForEachEntry(y, count, [&](uint16_t i) {
      const int x = w.Word(list + i);
      if ((out.Word(x) & 0x00F0) == 0x0080) {
        const uint16_t v = out.Word(x) | bits;
        out.Store(x, v, S::kDoorLayerToggle);
        out.Store(x + 0x40, v, S::kDoorLayerToggle);
      } else {
        const uint16_t v = out.Word(x + 0x1000) | bits;
        out.Store(x + 0x1000, v, S::kDoorLayerToggle);
        out.Store(x + 0x1040, v, S::kDoorLayerToggle);
      }
    });
  };
  toggles(kLayerToggleCount, 0x06C0, 0x1010);
  toggles(kDungeonToggleCount, 0x06D0, 0x2020);

  // ChangeDoorToSwitch (#_01C1BA).
  const uint16_t door_switch = w.Word(kDoorSwitch);
  if (door_switch != 0) {
    int x = (door_switch & 0x3FFF) >> 1;
    const int columns = (door_switch & 0x8000) ? 6 : 5;
    const bool open = (w.Word(kRoomFlags) & 0x1000) != 0;
    for (int i = 0; i < columns; ++i, x += 2) {
      if (open) {
        out.Store(x, 0x0101, S::kDoorSwitch);
        out.Store(x + 0x280, 0x0101, S::kDoorSwitch);
        for (int o : {0x80, 0x100, 0x180, 0x200}) {
          out.Store(x + o, 0x0000, S::kDoorSwitch);
        }
      } else {
        for (int o : {0x80, 0x100, 0x180, 0x200}) {
          out.Store(x + o, 0x2323, S::kDoorSwitch);
        }
      }
    }
  }
}

}  // namespace

const char* CollisionSourceName(CollisionSource source) {
  switch (source) {
    case CollisionSource::kTilemap:
      return "tilemap";
    case CollisionSource::kStar:
      return "star";
    case CollisionSource::kStairs:
      return "stairs";
    case CollisionSource::kLayerStairs:
      return "layer stairs";
    case CollisionSource::kWater:
      return "water stairs";
    case CollisionSource::kManipulable:
      return "pot/block";
    case CollisionSource::kTorch:
      return "torch";
    case CollisionSource::kChest:
      return "chest";
    case CollisionSource::kBigKeyLock:
      return "big key lock";
    case CollisionSource::kDoor:
      return "door";
    case CollisionSource::kLockedDoor:
      return "locked door";
    case CollisionSource::kDoorLayerToggle:
      return "toggle door";
    case CollisionSource::kDoorSwitch:
      return "door switch";
    case CollisionSource::kCrystalPeg:
      return "crystal peg";
  }
  return "?";
}

std::array<uint8_t, kTileAttributeTableSize> LoadUnderworldTileAttributeTable(
    const Rom& rom, uint8_t blockset) {
  std::array<uint8_t, kTileAttributeTableSize> table{};
  // LoadDefaultTileTypes (#_0E97D9).
  for (int i = 0; i < 0x140; ++i) {
    table[i] = RomByte(rom, kUnderworldTileTypes + i);
  }
  for (int i = 0; i < 0x40; ++i) {
    table[0x1C0 + i] = RomByte(rom, kUnderworldTileTypes + 0x140 + i);
  }
  // Underworld_LoadCustomTileTypes (#_0E942A).
  const uint16_t offset = RomWord(rom, kCustomTileTypesOffset + blockset * 2);
  for (int i = 0; i < 0x80; ++i) {
    table[0x140 + i] = RomByte(rom, kCustomUnderworldTileTypes + offset + i);
  }
  return table;
}

RoomCollisionMaps ComputeBasicCollisionMaps(
    const std::array<uint8_t, kTileAttributeTableSize>& table,
    const RoomTilemaps& tilemaps) {
  RoomCollisionMaps maps;
  // Underworld_LoadBasicAttribute_full (#_01B8F3).
  auto attribute = [&](uint16_t word) -> uint8_t {
    const uint16_t tile = word & 0x03FF;
    // Tiles 0x200+ index past TILEATTR into $7F0000 (decompression buffer);
    // no vanilla room uses them. Treat them as 0.
    uint8_t value = tile < kTileAttributeTableSize ? table[tile] : 0;
    if (value >= 0x10 && value < 0x1C) {
      // bit 1 = vertical flip (word bit 15), bit 0 = horizontal (bit 14).
      value |=
          static_cast<uint8_t>(((word >> 15) & 1) << 1 | ((word >> 14) & 1));
    }
    return value;
  };
  for (size_t i = 0; i < kCollisionMapTiles; ++i) {
    const uint16_t upper = i < tilemaps.bg1.size() ? tilemaps.bg1[i] : 0;
    const uint16_t lower = i < tilemaps.bg2.size() ? tilemaps.bg2[i] : 0;
    maps.attributes[i] = attribute(upper);
    maps.attributes[kCollisionMapTiles + i] = attribute(lower);
  }
  maps.sources.fill(CollisionSource::kTilemap);
  return maps;
}

RoomCollisionMaps ComputeRoomCollisionMaps(
    const Rom& rom, const RoomCollisionInput& input,
    const UnderworldRoomLoadState& state) {
  // Underworld_LoadAttributeTable (#_01B8BF).
  RoomCollisionMaps maps = ComputeBasicCollisionMaps(
      LoadUnderworldTileAttributeTable(rom, input.blockset), input.tilemaps);

  RoomLoadReplay replay(rom, input, state);
  replay.Run();

  AttributeWriter out(maps);
  ApplyObjectAttributes(replay.wram(), replay.tag1(), replay.tag2(), out);
  ApplyDoorAttributes(rom, replay.wram(), out);

  if (state.crystal_pegs_swapped) {  // Underworld_FlipCrystalPegAttribute
    for (size_t i = 0; i < kRoomCollisionBytes; ++i) {
      if (maps.attributes[i] == 0x66 || maps.attributes[i] == 0x67) {
        maps.attributes[i] ^= 1;
        maps.sources[i] = CollisionSource::kCrystalPeg;
      }
    }
  }
  return maps;
}

RoomCollisionInput MakeRoomCollisionInput(const Room& room,
                                          const RoomTilemaps& tilemaps) {
  RoomCollisionInput input;
  input.room_id = room.id();
  input.blockset = room.blockset();
  input.tag1 = static_cast<uint8_t>(room.tag1());
  input.tag2 = static_cast<uint8_t>(room.tag2());
  input.objects = room.GetTileObjects();
  input.doors = room.GetDoors();
  input.tilemaps = tilemaps;
  return input;
}

RoomCollisionInput MakeRoomCollisionInput(const Room& room) {
  return MakeRoomCollisionInput(room, ComposeYazeRoomTilemaps(room));
}

}  // namespace yaze::zelda3
