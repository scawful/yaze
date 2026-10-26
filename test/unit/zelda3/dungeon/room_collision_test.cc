#include "zelda3/dungeon/room_collision.h"

#include <algorithm>
#include <cstdint>
#include <iterator>
#include <vector>

#include "gtest/gtest.h"
#include "rom/rom.h"
#include "zelda3/dungeon/room.h"
#include "zelda3/dungeon/room_object.h"

namespace yaze::zelda3 {
namespace {

// PC offsets of the vanilla tables room_collision.cc reads.
constexpr int kLoadCustomTileTypes = 0x07142A;  // $0E:942A
constexpr int kUnderworldTileTypes = 0x071659;
constexpr int kCustomTileTypesOffset = 0x071000;
constexpr int kCustomUnderworldTileTypes = 0x07102A;
constexpr int kDungeonMask = 0x0018C0;
constexpr int kDoorPositionsNorthWall = 0x00197E;
constexpr int kDoorwayTileProperties = 0x001A52;

void PutWord(std::vector<uint8_t>& data, int pc, uint16_t value) {
  data[pc] = static_cast<uint8_t>(value & 0xFF);
  data[pc + 1] = static_cast<uint8_t>(value >> 8);
}

// Underworld_LoadCustomTileTypes as assembled, with its two table operands
// (16-bit addresses in the routine's bank).
void PutLoadCustomTileTypes(std::vector<uint8_t>& data, int pc,
                            uint16_t offsets, uint16_t types) {
  const uint8_t code[] = {
      0x8B, 0x4B, 0xAB, 0xC2, 0x30, 0xAD, 0xA2, 0x0A, 0x29, 0xFF, 0x00, 0x0A,
      0xAA, 0xBD, 0x00, 0x00, 0xA8, 0xA2, 0x00, 0x00, 0xB9, 0x00, 0x00, 0x9F,
      0x40, 0xFF, 0x7E, 0xB9, 0x00, 0x00, 0x9F, 0x80, 0xFF, 0x7E, 0xC8, 0xC8,
      0xE8, 0xE8, 0xE0, 0x40, 0x00, 0xD0, 0xE9, 0xE2, 0x30, 0xAB, 0x6B};
  std::copy(std::begin(code), std::end(code), data.begin() + pc);
  PutWord(data, pc + 14, offsets);
  PutWord(data, pc + 21, types);
  PutWord(data, pc + 28, static_cast<uint16_t>(types + 0x40));
}

// A zero-filled ROM with just enough of the tables for these tests.
class RoomCollisionTest : public ::testing::Test {
 protected:
  void SetUp() override {
    std::vector<uint8_t> data(0x100000, 0);
    data[kUnderworldTileTypes + 0x005] = 0x10;  // flippable (0x10-0x1B)
    data[kUnderworldTileTypes + 0x006] = 0x27;  // not flippable
    data[kUnderworldTileTypes + 0x13F] = 0x09;
    data[kUnderworldTileTypes + 0x140] = 0x44;  // -> TILEATTR[0x1C0]
    PutWord(data, kCustomTileTypesOffset + 3 * 2, 0x0080);
    data[kCustomUnderworldTileTypes + 0x00] = 0x55;  // blockset 0 -> [0x140]
    data[kCustomUnderworldTileTypes + 0x80] = 0x66;  // blockset 3 -> [0x140]
    const uint16_t masks[16] = {0x8000, 0x4000, 0x2000, 0x1000, 0x0800, 0x0400,
                                0x0200, 0x0100, 0x0080, 0x0040, 0x0020, 0x0010,
                                0x0008, 0x0004, 0x0002, 0x0001};
    for (int i = 0; i < 16; ++i) {
      PutWord(data, kDungeonMask + i * 2, masks[i]);
    }
    PutWord(data, kDoorPositionsNorthWall, 0x021C);      // tile (14, 4)
    PutWord(data, kDoorPositionsNorthWall + 2, 0x023C);  // tile (30, 4)
    data[kDoorwayTileProperties + 0x00] = 0x80;
    data[kDoorwayTileProperties + 0x01] = 0x80;
    ASSERT_TRUE(rom_.LoadFromData(data).ok());
  }

  RoomCollisionInput EmptyRoom() const {
    RoomCollisionInput input;
    input.room_id = 0x010;
    input.tilemaps.bg1.assign(kRoomTilemapWords, 0);
    input.tilemaps.bg2.assign(kRoomTilemapWords, 0);
    return input;
  }

  static RoomObject Special(int id, int x, int y, int layer,
                            ObjectOption option) {
    RoomObject object(static_cast<int16_t>(id), static_cast<uint8_t>(x),
                      static_cast<uint8_t>(y), 0, static_cast<uint8_t>(layer));
    object.set_options(option);
    return object;
  }

  Rom rom_;
};

TEST_F(RoomCollisionTest, AttributeTableCombinesDefaultAndCustomTypes) {
  const auto table = LoadUnderworldTileAttributeTable(rom_, 0);
  EXPECT_EQ(table[0x005], 0x10);
  EXPECT_EQ(table[0x13F], 0x09);
  EXPECT_EQ(table[0x140], 0x55);  // CustomUnderworldTileTypes + 0
  EXPECT_EQ(table[0x1C0], 0x44);  // UnderworldTileTypes[0x140]
  EXPECT_EQ(LoadUnderworldTileAttributeTable(rom_, 3)[0x140], 0x66);
}

TEST_F(RoomCollisionTest, VanillaRoutineResolvesToVanillaTables) {
  EXPECT_EQ(ResolveCustomTileTypeTable(rom_).source,
            CustomTileTypeSource::kVanilla);  // zero bytes: default addresses
  std::vector<uint8_t> data(rom_.vector());
  PutLoadCustomTileTypes(data, kLoadCustomTileTypes, 0x9000, 0x902A);
  Rom rom;
  ASSERT_TRUE(rom.LoadFromData(data).ok());
  const CustomTileTypeTable table = ResolveCustomTileTypeTable(rom);
  EXPECT_EQ(table.source, CustomTileTypeSource::kVanilla);
  EXPECT_EQ(table.offsets_pc, kCustomTileTypesOffset);
  EXPECT_EQ(table.data_pc, kCustomUnderworldTileTypes);
  EXPECT_EQ(LoadUnderworldTileAttributeTable(rom, 3)[0x140], 0x66);
}

// Oracle of Secrets: $0E:942A is JSL to a copy of the routine whose tables
// hold 24 groups (blocksets 0-23) in another bank.
TEST_F(RoomCollisionTest, HookedRoutineReadsItsOwnTablesForBlocksets0To23) {
  constexpr int kHookPc = 0x0F0000;     // $1E:8000
  constexpr int kOffsetsPc = 0x0F1000;  // $1E:9000, 24 words
  constexpr int kTypesPc = 0x0F1030;    // $1E:9030
  std::vector<uint8_t> data(rom_.vector());
  const uint8_t jsl[] = {0x22, 0x00, 0x80, 0x1E, 0x6B};
  std::copy(std::begin(jsl), std::end(jsl),
            data.begin() + kLoadCustomTileTypes);
  PutLoadCustomTileTypes(data, kHookPc, 0x9000, 0x9030);
  PutWord(data, kOffsetsPc + 4 * 2, 0x0200);
  PutWord(data, kOffsetsPc + 23 * 2, 0x0B80);
  data[kTypesPc + 0x200 + 0x71] = 0xB1;  // blockset 4 -> [0x1B1]
  data[kTypesPc + 0xB80 + 0x00] = 0x5A;  // blockset 23 -> [0x140]
  data[kTypesPc + 0xB80 + 0x7F] = 0x5B;  // blockset 23 -> [0x1BF]
  Rom rom;
  ASSERT_TRUE(rom.LoadFromData(data).ok());

  const CustomTileTypeTable table = ResolveCustomTileTypeTable(rom);
  EXPECT_EQ(table.source, CustomTileTypeSource::kHook);
  EXPECT_EQ(table.hook_target, 0x1E8000u);
  EXPECT_EQ(table.offsets_pc, kOffsetsPc);
  EXPECT_EQ(table.data_pc, kTypesPc);

  const auto blockset4 = LoadUnderworldTileAttributeTable(rom, 4);
  EXPECT_EQ(blockset4[0x1B1], 0xB1);
  EXPECT_EQ(blockset4[0x140], 0x00);  // not the vanilla 0x55
  EXPECT_EQ(blockset4[0x005], 0x10);  // shared types still vanilla
  EXPECT_EQ(blockset4[0x1C0], 0x44);
  const auto blockset23 = LoadUnderworldTileAttributeTable(rom, 23);
  EXPECT_EQ(blockset23[0x140], 0x5A);
  EXPECT_EQ(blockset23[0x1BF], 0x5B);
}

TEST_F(RoomCollisionTest, UndecodableHookFallsBackToVanillaTables) {
  std::vector<uint8_t> data(rom_.vector());
  const uint8_t jsl[] = {0x22, 0x00, 0x80, 0x1E, 0x6B};  // to zero bytes
  std::copy(std::begin(jsl), std::end(jsl),
            data.begin() + kLoadCustomTileTypes);
  Rom rom;
  ASSERT_TRUE(rom.LoadFromData(data).ok());
  const CustomTileTypeTable table = ResolveCustomTileTypeTable(rom);
  EXPECT_EQ(table.source, CustomTileTypeSource::kUnrecognizedHook);
  EXPECT_EQ(table.offsets_pc, kCustomTileTypesOffset);
  EXPECT_EQ(LoadUnderworldTileAttributeTable(rom, 3)[0x140], 0x66);
}

// Oracle tile type names follow the same $0E:942A hook signal.
TEST_F(RoomCollisionTest, TileTypeNameSetIsOracleOnlyForDecodedHook) {
  EXPECT_EQ(DetectTileTypeNameSet(rom_), TileTypeNameSet::kVanilla);

  std::vector<uint8_t> data(rom_.vector());
  const uint8_t jsl[] = {0x22, 0x00, 0x80, 0x1E, 0x6B};
  std::copy(std::begin(jsl), std::end(jsl),
            data.begin() + kLoadCustomTileTypes);
  Rom unrecognized;
  ASSERT_TRUE(unrecognized.LoadFromData(data).ok());
  EXPECT_EQ(DetectTileTypeNameSet(unrecognized), TileTypeNameSet::kVanilla);

  PutLoadCustomTileTypes(data, 0x0F0000, 0x9000, 0x9030);  // $1E:8000
  Rom hooked;
  ASSERT_TRUE(hooked.LoadFromData(data).ok());
  EXPECT_EQ(DetectTileTypeNameSet(hooked), TileTypeNameSet::kOracleOfSecrets);
}

TEST_F(RoomCollisionTest, BasicPassAddsFlipBitsOnlyToTypes10To1B) {
  auto input = EmptyRoom();
  input.tilemaps.bg1[0] = 0x0005;                // no flip
  input.tilemaps.bg1[1] = 0x4005;                // h flip -> bit 0
  input.tilemaps.bg1[2] = 0x8005;                // v flip -> bit 1
  input.tilemaps.bg1[3] = 0xC005 | 0x2000;       // both, plus priority
  input.tilemaps.bg1[4] = 0xC006;                // 0x27 ignores flips
  input.tilemaps.bg2[64 + 2] = 0x1C00 | 0x0140;  // palette bits ignored
  const auto maps = ComputeRoomCollisionMaps(rom_, input);
  EXPECT_EQ(maps.upper(0, 0), 0x10);
  EXPECT_EQ(maps.upper(1, 0), 0x11);
  EXPECT_EQ(maps.upper(2, 0), 0x12);
  EXPECT_EQ(maps.upper(3, 0), 0x13);
  EXPECT_EQ(maps.upper(4, 0), 0x27);
  EXPECT_EQ(maps.lower(2, 1), 0x55);
  EXPECT_EQ(maps.sources[0], CollisionSource::kTilemap);
}

TEST_F(RoomCollisionTest, StarTilesFollowTheObjectsLayer) {
  auto input = EmptyRoom();
  input.objects.push_back(RoomObject(0x11F, 10, 20, 0, 0));  // upper
  input.objects.push_back(RoomObject(0x11F, 30, 40, 0, 1));  // lower
  const auto maps = ComputeRoomCollisionMaps(rom_, input);
  for (int dy = 0; dy < 2; ++dy) {
    for (int dx = 0; dx < 2; ++dx) {
      EXPECT_EQ(maps.upper(10 + dx, 20 + dy), 0x3B);
      EXPECT_EQ(maps.lower(30 + dx, 40 + dy), 0x3B);
      EXPECT_EQ(maps.lower(10 + dx, 20 + dy), 0x00);
    }
  }
  EXPECT_EQ(maps.sources[20 * 64 + 10], CollisionSource::kStar);
}

// Pots, then pushable blocks, then torches share one numbered list:
// 0x70, 0x71, ... then 0xC0, 0xC1, ...
TEST_F(RoomCollisionTest, PotsBlocksAndTorchesAreNumberedInLoadOrder) {
  auto input = EmptyRoom();
  // Rightwards pots, size 1 -> two pots at (4,4) and (6,4).
  input.objects.push_back(RoomObject(0x0BC, 4, 4, 1, 0));
  input.objects.push_back(Special(0x150, 20, 30, 1, ObjectOption::Torch));
  input.objects.push_back(Special(0x0E00, 12, 12, 0, ObjectOption::Block));
  const auto maps = ComputeRoomCollisionMaps(rom_, input);
  EXPECT_EQ(maps.upper(4, 4), 0x70);
  EXPECT_EQ(maps.upper(5, 5), 0x70);
  EXPECT_EQ(maps.upper(6, 4), 0x71);
  EXPECT_EQ(maps.upper(7, 5), 0x71);
  EXPECT_EQ(maps.upper(12, 12), 0x72);  // block after the stream pots
  EXPECT_EQ(maps.lower(20, 30), 0xC0);  // lower-layer torch
  EXPECT_EQ(maps.lower(21, 31), 0xC0);
  EXPECT_EQ(maps.sources[kCollisionMapTiles + 30 * 64 + 20],
            CollisionSource::kTorch);
}

TEST_F(RoomCollisionTest, ChestsAndBigChestsGetNumberedChestTiles) {
  auto input = EmptyRoom();
  input.objects.push_back(RoomObject(0xF99, 8, 8, 0, 0));
  input.objects.push_back(RoomObject(0xFB1, 20, 8, 0, 0));
  auto maps = ComputeRoomCollisionMaps(rom_, input);
  EXPECT_EQ(maps.upper(8, 8), 0x58);
  EXPECT_EQ(maps.upper(9, 9), 0x58);
  EXPECT_EQ(maps.upper(10, 8), 0x00);
  // Big chest: words at x, x+0x40, x+0x42, x+0x80, x+0x82, so the top row's
  // right half keeps its tilemap attribute.
  for (int dy = 0; dy < 3; ++dy) {
    for (int dx = 0; dx < 4; ++dx) {
      const uint8_t expected = (dy == 0 && dx >= 2) ? 0x00 : 0x59;
      EXPECT_EQ(maps.upper(20 + dx, 8 + dy), expected) << dx << "," << dy;
    }
  }

  // Tag 0x27 hides closed chests on load: no chest tiles at all.
  input.tag2 = 0x27;
  maps = ComputeRoomCollisionMaps(rom_, input);
  EXPECT_EQ(maps.upper(8, 8), 0x00);
  EXPECT_EQ(maps.upper(20, 8), 0x00);
}

// Spiral stairs record the tile one row above the object.
TEST_F(RoomCollisionTest, SpiralStairsMarkTheirColumnAndNumberTheExit) {
  auto input = EmptyRoom();
  input.objects.push_back(RoomObject(0x138, 10, 10, 0, 0));
  const auto maps = ComputeRoomCollisionMaps(rom_, input);
  // Recorded tile (10, 9); marks column x+1..x+2 on rows +0, +2, +3.
  EXPECT_EQ(maps.upper(11, 9), 0x5E);
  EXPECT_EQ(maps.upper(12, 9), 0x5E);
  EXPECT_EQ(maps.upper(11, 10), 0x30);
  EXPECT_EQ(maps.upper(12, 10), 0x30);
  EXPECT_EQ(maps.upper(11, 11), 0x5E);
  EXPECT_EQ(maps.upper(11, 12), 0x5E);
  EXPECT_EQ(maps.sources[10 * 64 + 11], CollisionSource::kStairs);
}

TEST_F(RoomCollisionTest, NorthDoorwayWritesItsColumnAndLockedDoorsItsSlot) {
  auto input = EmptyRoom();
  // Slot 0: normal north doorway (type 0x00) at position 0 -> tile (14, 4).
  input.doors.push_back(Room::Door::FromRomBytes(0x00, 0x00));
  // Slot 1: a type-0x1C door at position 1; slot 1 (mask 0x4000) is closed
  // with the default $068C = 0x0F00.
  input.doors.push_back(Room::Door::FromRomBytes(0x10, 0x1C));
  const auto maps = ComputeRoomCollisionMaps(rom_, input);
  // (0x021C >> 1) & 0x783F snaps the doorway to row 0, column 14; it writes
  // columns x+1, x+2 on rows 0-6 and zero on row 7.
  for (int row = 0; row < 7; ++row) {
    EXPECT_EQ(maps.upper(15, row), 0x80) << row;
    EXPECT_EQ(maps.upper(16, row), 0x80) << row;
  }
  EXPECT_EQ(maps.upper(15, 7), 0x00);
  EXPECT_EQ(maps.sources[7 * 64 + 15], CollisionSource::kDoor);
  // Locked: F0 | slot on rows +1 and +2 of (0x023C >> 1).
  EXPECT_EQ(maps.upper(31, 5), 0xF1);
  EXPECT_EQ(maps.upper(32, 6), 0xF1);
  EXPECT_EQ(maps.sources[5 * 64 + 31], CollisionSource::kLockedDoor);
}

TEST_F(RoomCollisionTest, CrystalPegSwapFlips66And67) {
  auto input = EmptyRoom();
  const auto table = LoadUnderworldTileAttributeTable(rom_, 0);
  ASSERT_NE(table[0x140], 0x66);
  // Blockset 3 maps tile 0x140 to 0x66.
  input.blockset = 3;
  input.tilemaps.bg1[0] = 0x0140;
  UnderworldRoomLoadState state;
  EXPECT_EQ(ComputeRoomCollisionMaps(rom_, input, state).upper(0, 0), 0x66);
  state.crystal_pegs_swapped = true;
  const auto swapped = ComputeRoomCollisionMaps(rom_, input, state);
  EXPECT_EQ(swapped.upper(0, 0), 0x67);
  EXPECT_EQ(swapped.sources[0], CollisionSource::kCrystalPeg);
}

TEST_F(RoomCollisionTest, StairSlotResolverUsesSerializedLayerOrder) {
  auto input = EmptyRoom();
  input.objects = {RoomObject(0x138, 10, 10, 0, 1),
                   RoomObject(0x138, 20, 20, 0, 0)};
  const auto slots = ResolveVanillaStaircaseSlots(rom_, input);
  ASSERT_EQ(slots.size(), 2u);
  EXPECT_EQ(slots[0].object_index, 0u);
  EXPECT_EQ(slots[0].slot, 1);
  EXPECT_EQ(slots[1].slot, 0);
}

TEST_F(RoomCollisionTest, StairSlotResolverAgreesWithEncodedStreamOrder) {
  Room room;
  ASSERT_TRUE(room.AddObject(RoomObject(0x138, 10, 10, 0, 1)).ok());
  ASSERT_TRUE(room.AddObject(RoomObject(0x138, 20, 20, 0, 0)).ok());
  const auto bytes = room.EncodeObjects();
  ASSERT_GE(bytes.size(), 10u);
  // Stable layer bucketing emits primary object, FFFF, then BG2 object.
  EXPECT_EQ(bytes[3], 0xFF);
  EXPECT_EQ(bytes[4], 0xFF);
  auto input = EmptyRoom();
  input.objects = {
      RoomObject::DecodeObjectFromBytes(bytes[0], bytes[1], bytes[2], 0),
      RoomObject::DecodeObjectFromBytes(bytes[5], bytes[6], bytes[7], 1)};
  EXPECT_EQ(input.objects[0].x(), 20);
  EXPECT_EQ(input.objects[1].x(), 10);
  const auto slots = ResolveVanillaStaircaseSlots(rom_, input);
  ASSERT_EQ(slots.size(), 2u);
  EXPECT_EQ(slots[0].slot, 0);
  EXPECT_EQ(slots[1].slot, 1);
}

TEST_F(RoomCollisionTest, StairSlotResolverDetectsCounterOverwrite) {
  auto input = EmptyRoom();
  input.objects = {RoomObject(0x139, 10, 10, 0), RoomObject(0x138, 20, 20, 0)};
  const auto slots = ResolveVanillaStaircaseSlots(rom_, input);
  ASSERT_EQ(slots.size(), 2u);
  EXPECT_FALSE(slots[0].slot);
  EXPECT_EQ(slots[0].status, StaircaseSlotStatus::kTriggerUnavailable);
  EXPECT_EQ(slots[1].slot, 0);
}

TEST_F(RoomCollisionTest, StairSlotResolverHandlesEveryVanillaFamily) {
  for (int id : {0x12D, 0x12E, 0x12F, 0x138, 0x139, 0x13A, 0x13B, 0xF9E, 0xF9F,
                 0xFA0, 0xFA1, 0xFA6, 0xFA7, 0xFA8, 0xFA9}) {
    for (int layer = 0; layer < 3; ++layer) {
      SCOPED_TRACE(::testing::Message() << "id=" << id << " layer=" << layer);
      auto input = EmptyRoom();
      input.objects.emplace_back(id, 10, 10, 0, layer);
      const auto slots = ResolveVanillaStaircaseSlots(rom_, input);
      ASSERT_EQ(slots.size(), 1u);
      EXPECT_EQ(slots[0].slot, 0);
      EXPECT_EQ(slots[0].status, StaircaseSlotStatus::kResolved);
    }
  }
}

TEST_F(RoomCollisionTest, StairSlotResolverKeepsUpAndDownIndices) {
  auto input = EmptyRoom();
  input.objects = {RoomObject(0x138, 10, 10, 0), RoomObject(0x139, 20, 20, 0)};
  const auto slots = ResolveVanillaStaircaseSlots(rom_, input);
  ASSERT_EQ(slots.size(), 2u);
  EXPECT_EQ(slots[0].slot, 0);
  EXPECT_EQ(slots[1].slot, 1);
}

TEST_F(RoomCollisionTest,
       StairSlotResolverRejectsOverlappingAndOverflowingStairs) {
  auto input = EmptyRoom();
  input.objects = {RoomObject(0x138, 10, 10, 0), RoomObject(0x138, 11, 10, 0)};
  auto slots = ResolveVanillaStaircaseSlots(rom_, input);
  ASSERT_EQ(slots.size(), 2u);
  for (const auto& slot : slots) {
    EXPECT_FALSE(slot.slot);
    EXPECT_EQ(slot.status, StaircaseSlotStatus::kOverlappingStairs);
  }
  for (int i = 0; i < 3; ++i)
    input.objects.emplace_back(0x138, 20 + i * 8, 20, 0);
  slots = ResolveVanillaStaircaseSlots(rom_, input);
  ASSERT_EQ(slots.size(), 5u);
  for (const auto& slot : slots) {
    EXPECT_FALSE(slot.slot);
    EXPECT_EQ(slot.status, StaircaseSlotStatus::kTooManyStairs);
  }
}

TEST_F(RoomCollisionTest, StairSlotResolverRejectsBoundsAndNonStairs) {
  auto input = EmptyRoom();
  input.objects = {RoomObject(0x138, 10, 0, 0), RoomObject(0x138, 62, 20, 0),
                   RoomObject(0xFA0, 20, 62, 0), RoomObject(0x130, 10, 10, 0)};
  const auto slots = ResolveVanillaStaircaseSlots(rom_, input);
  ASSERT_EQ(slots.size(), 3u);
  for (const auto& slot : slots) {
    EXPECT_FALSE(slot.slot);
    EXPECT_EQ(slot.status, StaircaseSlotStatus::kOutsideRoom);
  }
}

TEST_F(RoomCollisionTest, StairSlotResolverRejectsTriggerOverwrittenByPot) {
  auto input = EmptyRoom();
  input.objects = {RoomObject(0x138, 10, 10, 0), RoomObject(0xFAF, 11, 10, 0)};
  const auto slots = ResolveVanillaStaircaseSlots(rom_, input);
  ASSERT_EQ(slots.size(), 1u);
  EXPECT_FALSE(slots[0].slot);
  EXPECT_EQ(slots[0].status, StaircaseSlotStatus::kTriggerUnavailable);
}

}  // namespace
}  // namespace yaze::zelda3
