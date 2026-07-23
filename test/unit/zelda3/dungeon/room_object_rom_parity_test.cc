// ROM-data-backed parity tests for the dungeon object rendering pipeline.
//
// Existing tests either use synthetic tile data (object_drawer_registry_replay_test)
// or assert drawer/parser self-consistency (dungeon_object_rom_validation_test).
// This file adds the missing dimension: "does ObjectParser faithfully reproduce
// the raw bytes at `kRoomObjectTileAddress + offset`?" and "does
// Room::ResolveDungeonPaletteId match a direct two-level ROM lookup?"
//
// Concretely, for each checked object we:
//   1. Read the tile pointer from the subtype table directly.
//   2. Read N raw 16-bit words from ROM at the computed tile-data address.
//   3. Decode each word via gfx::WordToTileInfo.
//   4. Compare the decoded TileInfos against ObjectParser::ParseObject's output.
//   5. For corner objects, run ObjectDrawer and verify the drawer places each
//      parsed tile at the USDASM column-major position on both BG1 and BG2.
//
// The goal is to detect divergences between the ObjectParser / ObjectDrawer
// pipeline and what the ROM actually contains, without depending on a golden
// snapshot or an emulator.
//
// Safety: this test never checks ROM fixtures into the repo. It only reads ROMs
// from TestRomManager-managed local paths and skips cleanly when they are absent.

#include <gtest/gtest.h>

#include <cstdint>
#include <memory>
#include <vector>

#include "absl/strings/str_format.h"
#include "app/gfx/render/background_buffer.h"
#include "app/gfx/types/snes_tile.h"
#include "rom/rom.h"
#include "test_utils.h"
#include "zelda3/dungeon/dungeon_rom_addresses.h"
#include "zelda3/dungeon/dungeon_state.h"
#include "zelda3/dungeon/object_drawer.h"
#include "zelda3/dungeon/object_parser.h"
#include "zelda3/dungeon/room.h"
#include "zelda3/dungeon/room_object.h"
#include "zelda3/game_data.h"

namespace yaze::zelda3::test {
namespace {

// Base addresses mirrored from dungeon_rom_addresses.h so the test is explicit
// about what it reads from ROM (and catches constant drift).
constexpr int kSubtype1Base = 0x8000;
constexpr int kSubtype2Base = 0x83F0;
constexpr int kSubtype3Base = 0x84F0;
constexpr int kTileDataBase = 0x1B52;
constexpr int kBombableFloorOpenTileOffset = 0x05BA;
constexpr int kPrisonCellTileOffset = 0x1488;
// Oracle of Secrets relocates this room-specific behavior from vanilla room
// 0x65 to room 0xAD. Keep the preview test keyed to the current room supplied
// to the drawer rather than baking in the vanilla room number.
constexpr int kBombableFloorPreviewRoomId = 0xAD;

// Wall corner variants: USDASM RoomDraw_4x4Corner_BothBG dispatches these.
// All 8 IDs share the same 4x4 column-major routine; they differ in flip flags.
constexpr int kFirstWallCorner = 0x108;
constexpr int kLastWallCorner = 0x10F;

// Weird corners: 3x4 bottom (0x110-0x113) and 4x3 top (0x114-0x117).
constexpr int kFirstWeirdCornerBottom = 0x110;
constexpr int kLastWeirdCornerBottom = 0x113;
constexpr int kFirstWeirdCornerTop = 0x114;
constexpr int kLastWeirdCornerTop = 0x117;

// Read 2 bytes at rom[addr] as a little-endian 16-bit word.
uint16_t ReadWordLE(const ::yaze::Rom& rom, int addr) {
  const auto& data = rom.data();
  return static_cast<uint16_t>(data[addr] |
                               (static_cast<uint16_t>(data[addr + 1]) << 8));
}

// Resolve the tile-data start address for a Subtype 2 object from ROM.
int Subtype2TileDataAddr(const ::yaze::Rom& rom, int object_id) {
  const int index = (object_id - 0x100) & 0x3F;
  const int table_addr = kSubtype2Base + index * 2;
  return kTileDataBase + ReadWordLE(rom, table_addr);
}

// Resolve the tile-data start address for a Subtype 3 object from ROM.
// Mirrors ObjectParser::ParseSubtype3 (object_parser.cc:306-326): the index
// is `(id - 0xF80) & 0x7F`, the per-id pointer lives at
// `kSubtype3Base + index*2`, and the pointer is added to `kTileDataBase`.
int Subtype3TileDataAddr(const ::yaze::Rom& rom, int object_id) {
  const int index = (object_id - 0xF80) & 0x7F;
  const int table_addr = kSubtype3Base + index * 2;
  return kTileDataBase + ReadWordLE(rom, table_addr);
}

// Decode N consecutive tile words from ROM into TileInfo structs.
std::vector<gfx::TileInfo> DecodeTilesFromRom(const ::yaze::Rom& rom, int addr,
                                              int count) {
  std::vector<gfx::TileInfo> tiles;
  tiles.reserve(count);
  for (int i = 0; i < count; ++i) {
    const uint16_t word = ReadWordLE(rom, addr + i * 2);
    tiles.push_back(gfx::WordToTileInfo(word));
  }
  return tiles;
}

// Pull BG1/BG2 writes out of a trace in emission order.
std::vector<ObjectDrawer::TileTrace> FilterByLayer(
    const std::vector<ObjectDrawer::TileTrace>& trace,
    RoomObject::LayerType layer) {
  std::vector<ObjectDrawer::TileTrace> filtered;
  for (const auto& t : trace) {
    if (t.layer == static_cast<uint8_t>(layer))
      filtered.push_back(t);
  }
  return filtered;
}

// Drive ObjectDrawer for a ROM-loaded object, capturing writes in trace-only
// mode so neither bg1 nor bg2 needs palette/gfx wiring.
std::vector<ObjectDrawer::TileTrace> ReplayRomObjectTrace(::yaze::Rom* rom,
                                                          int16_t object_id,
                                                          int x, int y,
                                                          uint8_t size) {
  RoomObject obj(object_id, x, y, size,
                 static_cast<int>(RoomObject::LayerType::BG1));
  obj.SetRom(rom);
  obj.EnsureTilesLoaded();

  ObjectDrawer drawer(rom, /*room_id=*/0);
  gfx::BackgroundBuffer bg1(512, 512);
  gfx::BackgroundBuffer bg2(512, 512);
  gfx::PaletteGroup palette_group;

  std::vector<ObjectDrawer::TileTrace> trace;
  drawer.SetTraceCollector(&trace, /*trace_only=*/true);
  EXPECT_TRUE(drawer.DrawObject(obj, bg1, bg2, palette_group).ok());
  return trace;
}

// Minimal DungeonState stub for exercising state-branching draw routines
// (BigKeyLock / BombableFloor). Defaults match the production
// "no-edit"/closed/intact behaviour; callers flip individual flags to test the
// alternate branch. All other queries return safe defaults.
struct StateStub : public DungeonState {
  bool big_key_lock_open = false;
  int bombed_floor_room_id = -1;

  bool IsChestOpen(int, int) const override { return false; }
  bool IsBigChestOpen() const override { return false; }
  bool IsDoorOpen(int, int) const override { return false; }
  bool IsDoorSwitchActive(int) const override { return false; }
  bool IsBigKeyLockOpen(int, int) const override { return big_key_lock_open; }
  bool IsWallMoved(int) const override { return false; }
  bool IsFloorBombable(int room_id) const override {
    return room_id == bombed_floor_room_id;
  }
  bool IsRupeeFloorCleared(int) const override { return false; }
  bool IsCrystalSwitchBlue() const override { return false; }
};

// Same as ReplayRomObjectTrace but plumbs through a DungeonState so callers
// can exercise IsDoorOpen / IsFloorBombable branches inside the routine.
std::vector<ObjectDrawer::TileTrace> ReplayRomObjectTraceWithState(
    ::yaze::Rom* rom, int16_t object_id, int x, int y, uint8_t size,
    const DungeonState* state, int room_id = 0) {
  RoomObject obj(object_id, x, y, size,
                 static_cast<int>(RoomObject::LayerType::BG1));
  obj.SetRom(rom);
  obj.EnsureTilesLoaded();

  ObjectDrawer drawer(rom, room_id);
  gfx::BackgroundBuffer bg1(512, 512);
  gfx::BackgroundBuffer bg2(512, 512);
  gfx::PaletteGroup palette_group;

  std::vector<ObjectDrawer::TileTrace> trace;
  drawer.SetTraceCollector(&trace, /*trace_only=*/true);
  EXPECT_TRUE(drawer.DrawObject(obj, bg1, bg2, palette_group, state).ok());
  return trace;
}

// -----------------------------------------------------------------------------
// Parser -> raw ROM parity
// -----------------------------------------------------------------------------

class RoomObjectRomParityTest
    : public ::testing::TestWithParam<::yaze::test::RomRole> {
 protected:
  void SetUp() override {
    const auto role = GetParam();
    YAZE_SKIP_IF_ROM_MISSING(role, "RoomObjectRomParityTest");
    rom_ = std::make_unique<::yaze::Rom>();
    const auto path = ::yaze::test::TestRomManager::GetRomPath(role);
    ASSERT_TRUE(rom_->LoadFromFile(path).ok())
        << "Failed to load "
        << ::yaze::test::TestRomManager::GetRomRoleName(role) << " ROM at "
        << path;
  }

  std::unique_ptr<::yaze::Rom> rom_;
};

TEST_P(RoomObjectRomParityTest, WallCornerParserMatchesRawRomWords) {
  SCOPED_TRACE(::yaze::test::TestRomManager::GetRomRoleName(GetParam()));
  ObjectParser parser(rom_.get());
  for (int id = kFirstWallCorner; id <= kLastWallCorner; ++id) {
    SCOPED_TRACE(absl::StrFormat("object 0x%03X", id));
    const int addr = Subtype2TileDataAddr(*rom_, id);
    const auto expected = DecodeTilesFromRom(*rom_, addr, /*count=*/16);

    auto parsed_or = parser.ParseObject(static_cast<int16_t>(id));
    ASSERT_TRUE(parsed_or.ok()) << parsed_or.status();
    const auto& parsed = parsed_or.value();
    ASSERT_EQ(parsed.size(), 16u)
        << "4x4 wall corner must parse exactly 16 tiles from ROM";

    for (size_t i = 0; i < expected.size(); ++i) {
      SCOPED_TRACE(absl::StrFormat("tile idx=%zu", i));
      EXPECT_EQ(parsed[i].id_, expected[i].id_);
      EXPECT_EQ(parsed[i].palette_, expected[i].palette_);
      EXPECT_EQ(parsed[i].horizontal_mirror_, expected[i].horizontal_mirror_);
      EXPECT_EQ(parsed[i].vertical_mirror_, expected[i].vertical_mirror_);
      EXPECT_EQ(parsed[i].over_, expected[i].over_);
    }
  }
}

TEST_P(RoomObjectRomParityTest, WeirdCornerBottomParserMatchesRawRomWords) {
  SCOPED_TRACE(::yaze::test::TestRomManager::GetRomRoleName(GetParam()));
  ObjectParser parser(rom_.get());
  for (int id = kFirstWeirdCornerBottom; id <= kLastWeirdCornerBottom; ++id) {
    SCOPED_TRACE(absl::StrFormat("object 0x%03X", id));
    const int addr = Subtype2TileDataAddr(*rom_, id);
    const auto expected = DecodeTilesFromRom(*rom_, addr, /*count=*/12);

    auto parsed_or = parser.ParseObject(static_cast<int16_t>(id));
    ASSERT_TRUE(parsed_or.ok()) << parsed_or.status();
    const auto& parsed = parsed_or.value();
    ASSERT_EQ(parsed.size(), 12u) << "3x4 weird corner uses 12 tiles";

    for (size_t i = 0; i < expected.size(); ++i) {
      EXPECT_TRUE(parsed[i] == expected[i])
          << "tile idx=" << i << " id=0x" << std::hex << parsed[i].id_
          << " vs 0x" << expected[i].id_;
    }
  }
}

TEST_P(RoomObjectRomParityTest, WeirdCornerTopParserMatchesRawRomWords) {
  SCOPED_TRACE(::yaze::test::TestRomManager::GetRomRoleName(GetParam()));
  ObjectParser parser(rom_.get());
  for (int id = kFirstWeirdCornerTop; id <= kLastWeirdCornerTop; ++id) {
    SCOPED_TRACE(absl::StrFormat("object 0x%03X", id));
    const int addr = Subtype2TileDataAddr(*rom_, id);
    const auto expected = DecodeTilesFromRom(*rom_, addr, /*count=*/12);

    auto parsed_or = parser.ParseObject(static_cast<int16_t>(id));
    ASSERT_TRUE(parsed_or.ok()) << parsed_or.status();
    const auto& parsed = parsed_or.value();
    ASSERT_EQ(parsed.size(), 12u) << "4x3 weird corner uses 12 tiles";

    for (size_t i = 0; i < expected.size(); ++i) {
      EXPECT_TRUE(parsed[i] == expected[i]) << "tile idx=" << i;
    }
  }
}

TEST_P(RoomObjectRomParityTest, Subtype1SmokeSample_ParserMatchesRawRomWords) {
  SCOPED_TRACE(::yaze::test::TestRomManager::GetRomRoleName(GetParam()));
  // Pick representative Subtype 1 objects with known tile counts. If parser
  // decoding silently changes byte endianness or palette-bit extraction, this
  // catches it for the most common object shape (horizontal/vertical walls).
  struct Sample {
    int object_id;
    int tile_count;
  };
  // Values from kSubtype1TileLengths (object_parser.cc); see
  // dungeon_object_rom_validation_test.cc:121-125 for the broader table.
  const std::vector<Sample> samples = {
      {0x00, 4},
      {0x01, 8},
      {0x10, 5},
      {0x33, 16},
  };

  ObjectParser parser(rom_.get());
  for (const auto& s : samples) {
    SCOPED_TRACE(absl::StrFormat("object 0x%02X", s.object_id));
    const int index = s.object_id;
    const int table_addr = kSubtype1Base + index * 2;
    const int addr = kTileDataBase + ReadWordLE(*rom_, table_addr);
    const auto expected = DecodeTilesFromRom(*rom_, addr, s.tile_count);

    auto parsed_or = parser.ParseObject(static_cast<int16_t>(s.object_id));
    ASSERT_TRUE(parsed_or.ok()) << parsed_or.status();
    const auto& parsed = parsed_or.value();
    ASSERT_EQ(static_cast<int>(parsed.size()), s.tile_count);
    for (size_t i = 0; i < expected.size(); ++i) {
      EXPECT_TRUE(parsed[i] == expected[i]) << "tile idx=" << i;
    }
  }
}

// -----------------------------------------------------------------------------
// Drawer places ROM-parsed tiles at the right positions on both BGs
// -----------------------------------------------------------------------------

TEST_P(RoomObjectRomParityTest,
       WallCornerDrawerPlacesRomTilesColumnMajorOnBothBGs) {
  SCOPED_TRACE(::yaze::test::TestRomManager::GetRomRoleName(GetParam()));
  for (int id = kFirstWallCorner; id <= kLastWallCorner; ++id) {
    SCOPED_TRACE(absl::StrFormat("object 0x%03X", id));
    const int base_x = 5;
    const int base_y = 7;
    const auto trace = ReplayRomObjectTrace(
        rom_.get(), static_cast<int16_t>(id), base_x, base_y, /*size=*/0);

    const auto bg1 = FilterByLayer(trace, RoomObject::LayerType::BG1);
    const auto bg2 = FilterByLayer(trace, RoomObject::LayerType::BG2);
    ASSERT_EQ(bg1.size(), 16u);
    ASSERT_EQ(bg2.size(), 16u) << "_BothBG must mirror writes to BG2";

    // Decode the 16 expected tiles from the ROM for comparison.
    const int addr = Subtype2TileDataAddr(*rom_, id);
    const auto expected_tiles = DecodeTilesFromRom(*rom_, addr, 16);

    // USDASM RoomDraw_4x4Corner_BothBG emits column-major: for each x column,
    // emit 4 rows top-to-bottom. BG1 and BG2 both get identical writes.
    int idx = 0;
    for (int dx = 0; dx < 4; ++dx) {
      for (int dy = 0; dy < 4; ++dy) {
        SCOPED_TRACE(absl::StrFormat("idx=%d (dx=%d dy=%d)", idx, dx, dy));
        EXPECT_EQ(bg1[idx].x_tile, base_x + dx);
        EXPECT_EQ(bg1[idx].y_tile, base_y + dy);
        EXPECT_EQ(bg1[idx].tile_id, expected_tiles[idx].id_);
        EXPECT_EQ(bg2[idx].x_tile, base_x + dx);
        EXPECT_EQ(bg2[idx].y_tile, base_y + dy);
        EXPECT_EQ(bg2[idx].tile_id, expected_tiles[idx].id_);
        ++idx;
      }
    }
  }
}

// -----------------------------------------------------------------------------
// Palette resolution pipeline matches a direct ROM two-level lookup
// -----------------------------------------------------------------------------

TEST_P(RoomObjectRomParityTest,
       ResolveDungeonPaletteIdMatchesDirectRomTwoLevelLookup) {
  SCOPED_TRACE(::yaze::test::TestRomManager::GetRomRoleName(GetParam()));
  // Load GameData so paletteset_ids / dungeon_main are populated from ROM.
  GameData game_data;
  ASSERT_TRUE(LoadGameData(*rom_, game_data).ok());
  const int num_palettes =
      static_cast<int>(game_data.palette_groups.dungeon_main.size());
  ASSERT_GT(num_palettes, 0) << "dungeon_main palette group must load";

  // For every paletteset slot, compute the expected dungeon-main index by
  // reading the 16-bit word at `kDungeonPalettePointerTable + entry[0]` and
  // dividing by the palette stride (180 bytes per palette). Then compare with
  // Room::ResolveDungeonPaletteId, which is the single source of truth for
  // this lookup in production code.
  for (int slot = 0; slot < static_cast<int>(kNumPalettesets); ++slot) {
    SCOPED_TRACE(absl::StrFormat("paletteset %d", slot));
    const auto& entry = game_data.paletteset_ids[slot];
    const uint8_t offset = entry[0];
    auto word_or =
        rom_->ReadWord(static_cast<int>(kDungeonPalettePointerTable + offset));
    ASSERT_TRUE(word_or.ok()) << word_or.status();
    int expected = static_cast<int>(word_or.value()) / kDungeonPaletteBytes;
    if (expected < 0 || expected >= num_palettes)
      expected = 0;

    Room room(/*room_id=*/0, rom_.get(), &game_data);
    room.SetPalette(static_cast<uint8_t>(slot));
    const int actual = room.ResolveDungeonPaletteId();
    EXPECT_EQ(actual, expected)
        << "paletteset " << slot << " offset=0x" << std::hex
        << static_cast<int>(offset) << " word=0x" << word_or.value();
  }
}

// -----------------------------------------------------------------------------
// Custom Subtype-3 routines: ROM-backed audit (PrisonCell / BigKeyLock /
// BombableFloor)
// -----------------------------------------------------------------------------
//
// These tests complement the synthetic replay coverage in
// object_drawer_registry_replay_test.cc by checking real ROM payloads. They:
//   1. Read raw bytes from the Subtype-3 tile pointer table (`kSubtype3Base`)
//      and decode its raw 16-bit words from `kTileDataBase + offset`.
//   2. Compare the decoded tiles against ObjectParser::ParseObject output.
//   3. For drawer state branches (BigKeyLock event open/closed, BombableFloor
//      intact/bombed), drive the trace path with a DungeonState stub and
//      verify the routine selects the expected sub-range of parsed tiles.
//
// Routine bodies referenced (special_routines.cc):
//   - DrawPrisonCell (97): sparse 16x4 target-layer pattern matching the long
//     tilemap writes at RoomDraw_PrisonCell ($019C44); reads tiles[0..5].
//   - DrawBigKeyLock (92): column-major 2x2 tiles[0..3] when closed; no tile
//     writes when the corresponding shared chest/lock room-event slot is set.
//   - DrawBombableFloor (93): four 2x2 quadrants from tiles[0..15], or the
//     separately stored obj05BA replacement block at tiles[16..31] when the
//     current room's selected bombed-floor preview state is active.

TEST_P(RoomObjectRomParityTest, PrisonCellParserMatchesRawRomWords) {
  SCOPED_TRACE(::yaze::test::TestRomManager::GetRomRoleName(GetParam()));
  ObjectParser parser(rom_.get());
  const auto expected = DecodeTilesFromRom(
      *rom_, kTileDataBase + kPrisonCellTileOffset, /*count=*/6);
  for (int id : {0xF8D, 0xF97}) {
    SCOPED_TRACE(absl::StrFormat("object 0x%03X (PrisonCell)", id));

    auto parsed_or = parser.ParseObject(static_cast<int16_t>(id));
    ASSERT_TRUE(parsed_or.ok()) << parsed_or.status();
    const auto& parsed = parsed_or.value();
    ASSERT_EQ(parsed.size(), 6u)
        << "Both PrisonCell aliases consume the six words at literal obj1488.";

    for (size_t i = 0; i < expected.size(); ++i) {
      SCOPED_TRACE(absl::StrFormat("tile idx=%zu", i));
      EXPECT_EQ(parsed[i].id_, expected[i].id_);
      EXPECT_EQ(parsed[i].palette_, expected[i].palette_);
      EXPECT_EQ(parsed[i].horizontal_mirror_, expected[i].horizontal_mirror_);
      EXPECT_EQ(parsed[i].vertical_mirror_, expected[i].vertical_mirror_);
      EXPECT_EQ(parsed[i].over_, expected[i].over_);
    }
  }
}

TEST_P(RoomObjectRomParityTest, BigKeyLockParserMatchesRawRomWords) {
  SCOPED_TRACE(::yaze::test::TestRomManager::GetRomRoleName(GetParam()));
  ObjectParser parser(rom_.get());
  const int id = 0xF98;
  const int addr = Subtype3TileDataAddr(*rom_, id);
  const auto expected = DecodeTilesFromRom(*rom_, addr, /*count=*/4);

  auto parsed_or = parser.ParseObject(static_cast<int16_t>(id));
  ASSERT_TRUE(parsed_or.ok()) << parsed_or.status();
  const auto& parsed = parsed_or.value();
  ASSERT_EQ(parsed.size(), 4u)
      << "BigKeyLock consumes only the four obj1494 words; the next four "
         "belong to the Chest object at obj149C.";

  for (size_t i = 0; i < expected.size(); ++i) {
    SCOPED_TRACE(absl::StrFormat("tile idx=%zu", i));
    EXPECT_TRUE(parsed[i] == expected[i])
        << "tile idx=" << i << " parsed.id=0x" << std::hex << parsed[i].id_
        << " vs expected.id=0x" << expected[i].id_;
  }
}

TEST_P(RoomObjectRomParityTest, BombableFloorParserMatchesRawRomWords) {
  SCOPED_TRACE(::yaze::test::TestRomManager::GetRomRoleName(GetParam()));
  ObjectParser parser(rom_.get());
  const int id = 0xFC7;
  const int addr = Subtype3TileDataAddr(*rom_, id);
  auto expected = DecodeTilesFromRom(*rom_, addr, /*count=*/16);
  const auto open_tiles = DecodeTilesFromRom(
      *rom_, kTileDataBase + kBombableFloorOpenTileOffset, /*count=*/16);
  expected.insert(expected.end(), open_tiles.begin(), open_tiles.end());

  auto parsed_or = parser.ParseObject(static_cast<int16_t>(id));
  ASSERT_TRUE(parsed_or.ok()) << parsed_or.status();
  const auto& parsed = parsed_or.value();
  ASSERT_EQ(parsed.size(), 32u)
      << "BombableFloor needs two non-contiguous 16-word 4x4 states";

  for (size_t i = 0; i < expected.size(); ++i) {
    SCOPED_TRACE(absl::StrFormat("tile idx=%zu", i));
    EXPECT_TRUE(parsed[i] == expected[i])
        << "tile idx=" << i << " parsed.id=0x" << std::hex << parsed[i].id_
        << " vs expected.id=0x" << expected[i].id_;
  }
}

TEST_P(RoomObjectRomParityTest,
       PrisonCellDrawerMatchesSparseTargetLayerUsdasmWrites) {
  SCOPED_TRACE(::yaze::test::TestRomManager::GetRomRoleName(GetParam()));
  // Origin chosen so the full x..x+15 span stays in-bounds.
  const int base_x = 5;
  const int base_y = 7;
  const auto rom_tiles =
      DecodeTilesFromRom(*rom_, kTileDataBase + kPrisonCellTileOffset, 6);

  struct ExpectedWrite {
    int tile_index;
    int x_offset;
    int y_offset;
    bool force_horizontal_mirror;
  };
  std::vector<ExpectedWrite> expected;
  expected.reserve(48);
  for (int col = 0; col < 5; ++col) {
    expected.push_back({1, 2 + col, 0, false});
    expected.push_back({1, 9 + col, 0, false});
    expected.push_back({2, 2 + col, 1, false});
    expected.push_back({2, 9 + col, 1, true});
    expected.push_back({4, 2 + col, 2, false});
    expected.push_back({4, 9 + col, 2, true});
    expected.push_back({5, 2 + col, 3, false});
    expected.push_back({5, 9 + col, 3, true});
  }
  expected.push_back({0, 0, 0, false});
  expected.push_back({0, 15, 0, true});
  for (int x_offset : {1, 7, 8, 14}) {
    expected.push_back({1, x_offset, 0, false});
  }
  expected.push_back({3, 1, 2, false});
  expected.push_back({3, 14, 2, true});

  for (int object_id : {0xF8D, 0xF97}) {
    SCOPED_TRACE(absl::StrFormat("object=0x%03X", object_id));
    const auto trace =
        ReplayRomObjectTrace(rom_.get(), object_id, base_x, base_y, /*size=*/0);
    const auto bg1 = FilterByLayer(trace, RoomObject::LayerType::BG1);
    const auto bg2 = FilterByLayer(trace, RoomObject::LayerType::BG2);
    ASSERT_EQ(bg1.size(), 48u);
    EXPECT_TRUE(bg2.empty())
        << "RoomDraw_PrisonCell writes only to the $BF-selected tilemap";

    ASSERT_EQ(expected.size(), bg1.size());
    for (size_t i = 0; i < expected.size(); ++i) {
      SCOPED_TRACE(absl::StrFormat("write=%zu", i));
      const auto& write = bg1[i];
      const auto& want = expected[i];
      const auto& tile = rom_tiles[want.tile_index];
      EXPECT_EQ(write.x_tile, base_x + want.x_offset);
      EXPECT_EQ(write.y_tile, base_y + want.y_offset);
      EXPECT_EQ(write.tile_id, tile.id_);
      EXPECT_EQ((write.flags & 0x1) != 0,
                want.force_horizontal_mirror || tile.horizontal_mirror_);
      EXPECT_EQ((write.flags & 0x2) != 0, tile.vertical_mirror_);
      EXPECT_EQ((write.flags & 0x4) != 0, tile.over_);
      EXPECT_EQ((write.flags >> 3) & 0x7, tile.palette_);
    }
  }
}

TEST_P(RoomObjectRomParityTest, BigKeyLockDrawerMatchesRoomEventState) {
  SCOPED_TRACE(::yaze::test::TestRomManager::GetRomRoleName(GetParam()));
  const int base_x = 6;
  const int base_y = 8;

  // ROM-parsed tiles for direct comparison against drawer trace.
  const int addr = Subtype3TileDataAddr(*rom_, 0xF98);
  const auto rom_tiles = DecodeTilesFromRom(*rom_, addr, 4);

  // Closed branch: the room-event slot is clear -> tiles[0..3].
  StateStub closed;
  closed.big_key_lock_open = false;
  const auto closed_trace = ReplayRomObjectTraceWithState(
      rom_.get(), 0xF98, base_x, base_y, /*size=*/0, &closed);
  const auto closed_bg1 =
      FilterByLayer(closed_trace, RoomObject::LayerType::BG1);
  ASSERT_EQ(closed_bg1.size(), 4u)
      << "BigKeyLock closed branch writes 4 tiles in 2x2";
  EXPECT_TRUE(FilterByLayer(closed_trace, RoomObject::LayerType::BG2).empty());
  // RoomDraw_Rightwards2x2 emits column-major: TL, BL, TR, BR.
  for (int t = 0; t < 4; ++t) {
    SCOPED_TRACE(absl::StrFormat("closed t=%d", t));
    const int dx = t >> 1;
    const int dy = t & 1;
    EXPECT_EQ(closed_bg1[t].x_tile, base_x + dx);
    EXPECT_EQ(closed_bg1[t].y_tile, base_y + dy);
    EXPECT_EQ(closed_bg1[t].tile_id, rom_tiles[t].id_)
        << "closed branch must use tiles[0..3]";
  }

  // Open branch: the room-event slot is set -> no replacement graphics.
  StateStub open;
  open.big_key_lock_open = true;
  const auto open_trace = ReplayRomObjectTraceWithState(
      rom_.get(), 0xF98, base_x, base_y, /*size=*/0, &open);
  EXPECT_TRUE(open_trace.empty())
      << "USDASM's opened BigKeyLock branch advances its event slot and RTSes";
}

TEST_P(RoomObjectRomParityTest, BombableFloorDrawerSelectsTilesByFloorState) {
  SCOPED_TRACE(::yaze::test::TestRomManager::GetRomRoleName(GetParam()));
  const int base_x = 9;
  const int base_y = 11;

  const int addr = Subtype3TileDataAddr(*rom_, 0xFC7);
  auto rom_tiles = DecodeTilesFromRom(*rom_, addr, 16);
  const auto open_tiles = DecodeTilesFromRom(
      *rom_, kTileDataBase + kBombableFloorOpenTileOffset, 16);
  rom_tiles.insert(rom_tiles.end(), open_tiles.begin(), open_tiles.end());

  auto expect_state = [&](const std::vector<ObjectDrawer::TileTrace>& trace,
                          size_t state_offset) {
    const auto bg1 = FilterByLayer(trace, RoomObject::LayerType::BG1);
    ASSERT_EQ(bg1.size(), 16u)
        << "BombableFloor state must cover one fixed 4x4 block";
    EXPECT_TRUE(FilterByLayer(trace, RoomObject::LayerType::BG2).empty());

    size_t trace_index = 0;
    for (int block_y = 0; block_y < 2; ++block_y) {
      for (int block_x = 0; block_x < 2; ++block_x) {
        const size_t block_offset =
            state_offset + static_cast<size_t>((block_y * 2 + block_x) * 4);
        for (int x = 0; x < 2; ++x) {
          for (int y = 0; y < 2; ++y) {
            SCOPED_TRACE(
                absl::StrFormat("state=%zu block=(%d,%d) local=(%d,%d)",
                                state_offset, block_x, block_y, x, y));
            ASSERT_LT(trace_index, bg1.size());
            const size_t tile_index =
                block_offset + static_cast<size_t>(x * 2 + y);
            EXPECT_EQ(bg1[trace_index].x_tile, base_x + block_x * 2 + x);
            EXPECT_EQ(bg1[trace_index].y_tile, base_y + block_y * 2 + y);
            EXPECT_EQ(bg1[trace_index].tile_id, rom_tiles[tile_index].id_);
            ++trace_index;
          }
        }
      }
    }
    EXPECT_EQ(trace_index, bg1.size());
  };

  StateStub intact;
  const auto intact_trace = ReplayRomObjectTraceWithState(
      rom_.get(), 0xFC7, base_x, base_y, /*size=*/0, &intact,
      kBombableFloorPreviewRoomId);
  expect_state(intact_trace, /*state_offset=*/0);

  StateStub bombed;
  bombed.bombed_floor_room_id = kBombableFloorPreviewRoomId;
  const auto bombed_trace = ReplayRomObjectTraceWithState(
      rom_.get(), 0xFC7, base_x, base_y, /*size=*/0, &bombed,
      kBombableFloorPreviewRoomId);
  expect_state(bombed_trace, /*state_offset=*/16);

  const auto other_room_trace = ReplayRomObjectTraceWithState(
      rom_.get(), 0xFC7, base_x, base_y, /*size=*/0, &bombed,
      kBombableFloorPreviewRoomId - 1);
  expect_state(other_room_trace, /*state_offset=*/0);
}

}  // namespace

INSTANTIATE_TEST_SUITE_P(
    SupportedRomRoles, RoomObjectRomParityTest,
    ::testing::Values(::yaze::test::RomRole::kVanilla,
                      ::yaze::test::RomRole::kExpanded),
    [](const ::testing::TestParamInfo<::yaze::test::RomRole>& info) {
      switch (info.param) {
        case ::yaze::test::RomRole::kVanilla:
          return "Vanilla";
        case ::yaze::test::RomRole::kExpanded:
          return "Expanded";
        default:
          return "Unknown";
      }
    });

}  // namespace yaze::zelda3::test
