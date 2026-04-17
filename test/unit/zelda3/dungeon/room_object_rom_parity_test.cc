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
