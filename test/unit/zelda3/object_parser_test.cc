#include "zelda3/dungeon/object_parser.h"

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include <vector>

#include "core/features.h"
#include "mocks/mock_rom.h"
#include "zelda3/dungeon/draw_routines/draw_routine_registry.h"

namespace yaze {
namespace test {

namespace {

struct ScopedCustomObjectsFlag {
  bool previous = false;

  explicit ScopedCustomObjectsFlag(bool enabled) {
    previous = core::FeatureFlags::get().kEnableCustomObjects;
    core::FeatureFlags::get().kEnableCustomObjects = enabled;
    zelda3::DrawRoutineRegistry::Get().RefreshFeatureFlagMappings();
  }

  ~ScopedCustomObjectsFlag() {
    core::FeatureFlags::get().kEnableCustomObjects = previous;
    zelda3::DrawRoutineRegistry::Get().RefreshFeatureFlagMappings();
  }
};

}  // namespace

class ObjectParserTest : public ::testing::Test {
 protected:
  void SetUp() override {
    mock_rom_ = std::make_unique<MockRom>();
    SetupMockData();
    parser_ = std::make_unique<zelda3::ObjectParser>(mock_rom_.get());
  }

  void SetupMockData() {
    std::vector<uint8_t> mock_data(0x100000, 0x00);

    // Set up object subtype tables
    SetupSubtypeTable(mock_data, 0x8000, 0x100);  // Subtype 1 table
    SetupSubtypeTable(mock_data, 0x83F0, 0x80);   // Subtype 2 table
    SetupSubtypeTable(mock_data, 0x84F0, 0x100);  // Subtype 3 table

    // Set up tile data
    SetupTileData(mock_data, 0x1B52, 0x1000);

    static_cast<MockRom*>(mock_rom_.get())->SetTestData(mock_data);
  }

  void SetupSubtypeTable(std::vector<uint8_t>& data, int base_addr, int count) {
    for (int i = 0; i < count; i++) {
      int addr = base_addr + (i * 2);
      if (addr + 1 < (int)data.size()) {
        // Point to tile data at 0x1B52 + (i * 8)
        int tile_offset = (i * 8) & 0xFFFF;
        data[addr] = tile_offset & 0xFF;
        data[addr + 1] = (tile_offset >> 8) & 0xFF;
      }
    }
  }

  void SetupTileData(std::vector<uint8_t>& data, int base_addr, int size) {
    for (int i = 0; i < size; i += 8) {
      int addr = base_addr + i;
      if (addr + 7 < (int)data.size()) {
        // Create simple tile data (4 words per tile)
        for (int j = 0; j < 8; j++) {
          data[addr + j] = (i + j) & 0xFF;
        }
      }
    }
  }

  std::unique_ptr<MockRom> mock_rom_;
  std::unique_ptr<zelda3::ObjectParser> parser_;
};

TEST_F(ObjectParserTest, ParseSubtype1Object) {
  auto result = parser_->ParseObject(0x01);
  ASSERT_TRUE(result.ok());

  const auto& tiles = result.value();
  EXPECT_EQ(tiles.size(), 8);

  // Verify tile data was parsed correctly
  for (const auto& tile : tiles) {
    EXPECT_NE(tile.id_, 0);
  }
}

TEST_F(ObjectParserTest, ParseSubtype2Object) {
  auto result = parser_->ParseObject(0x101);
  ASSERT_TRUE(result.ok());

  const auto& tiles = result.value();
  EXPECT_EQ(tiles.size(), 16);
}

TEST_F(ObjectParserTest, ParseSubtype3Object) {
  auto result = parser_->ParseObject(0x201);
  ASSERT_TRUE(result.ok());

  const auto& tiles = result.value();
  EXPECT_EQ(tiles.size(), 8);
}

TEST_F(ObjectParserTest, GetObjectSubtype) {
  // Subtype 1: Object IDs 0x00-0xFF
  auto result1 = parser_->GetObjectSubtype(0x01);
  ASSERT_TRUE(result1.ok());
  EXPECT_EQ(result1->subtype, 1);

  // Subtype 2: Object IDs 0x100-0x1FF
  auto result2 = parser_->GetObjectSubtype(0x101);
  ASSERT_TRUE(result2.ok());
  EXPECT_EQ(result2->subtype, 2);

  // Subtype 3: Object IDs 0xF80-0xFFF (decoded from b3 >= 0xF8)
  // These map to table indices 0-127 via (id - 0xF80) & 0x7F
  auto result3 = parser_->GetObjectSubtype(0xF81);
  ASSERT_TRUE(result3.ok());
  EXPECT_EQ(result3->subtype, 3);
}

TEST_F(ObjectParserTest, ParseObjectSize) {
  // size_byte 0x09 = 0b00001001: size_x = 1 (bits 0-1), size_y = 2 (bits 2-3)
  auto result = parser_->ParseObjectSize(0x01, 0x09);
  ASSERT_TRUE(result.ok());

  const auto& size_info = result.value();
  EXPECT_EQ(size_info.width_tiles, 4);   // (1 + 1) * 2
  EXPECT_EQ(size_info.height_tiles, 6);  // (2 + 1) * 2
  EXPECT_TRUE(size_info.is_horizontal);
  EXPECT_TRUE(size_info.is_repeatable);
  EXPECT_EQ(size_info.repeat_count, 0x09);
}

TEST_F(ObjectParserTest, ParseObjectRoutine) {
  auto result = parser_->ParseObjectRoutine(0x01);
  ASSERT_TRUE(result.ok());

  const auto& routine_info = result.value();
  EXPECT_NE(routine_info.routine_ptr, 0);
  EXPECT_NE(routine_info.tile_ptr, 0);
  EXPECT_EQ(routine_info.tile_count, 8);
  EXPECT_TRUE(routine_info.is_repeatable);
  EXPECT_TRUE(routine_info.is_orientation_dependent);
}

TEST_F(ObjectParserTest, DrawInfoUsesSubtypeTileCountLookup) {
  auto info_subtype1 = parser_->GetObjectDrawInfo(0x33);
  EXPECT_EQ(info_subtype1.tile_count, 16);

  auto info_subtype2 = parser_->GetObjectDrawInfo(0x122);
  EXPECT_EQ(info_subtype2.tile_count, 20);

  auto info_subtype3 = parser_->GetObjectDrawInfo(0xFB1);
  EXPECT_EQ(info_subtype3.tile_count, 12);

  // 2026-04-25 audit: kSubtype1TileLengths previously stored 0 for
  // 0x47/0x48 and the parser defaulted to 8. The actual `Waterfall47`
  // routine reads 15 tiles and `Waterfall48` reads 9; the table now
  // carries those explicit counts so the parser's tile vector matches
  // what the routine consumes.
  auto info_waterfall47 = parser_->GetObjectDrawInfo(0x47);
  EXPECT_EQ(info_waterfall47.tile_count, 15);
  auto info_waterfall48 = parser_->GetObjectDrawInfo(0x48);
  EXPECT_EQ(info_waterfall48.tile_count, 9);
}

TEST_F(ObjectParserTest, DrawInfoOrientationFollowsRoutineCategory) {
  auto horizontal = parser_->GetObjectDrawInfo(0x33);
  EXPECT_TRUE(horizontal.is_horizontal);
  EXPECT_FALSE(horizontal.is_vertical);

  auto vertical = parser_->GetObjectDrawInfo(0x60);
  EXPECT_FALSE(vertical.is_horizontal);
  EXPECT_TRUE(vertical.is_vertical);

  auto diagonal = parser_->GetObjectDrawInfo(0x09);
  EXPECT_FALSE(diagonal.is_horizontal);
  EXPECT_FALSE(diagonal.is_vertical);
}

TEST_F(ObjectParserTest, DrawInfoMapsSubtype2CornerRoutinesToUsdasmParity) {
  auto corner_4x4_both = parser_->GetObjectDrawInfo(0x108);
  EXPECT_EQ(corner_4x4_both.draw_routine_id, 35);
  EXPECT_TRUE(corner_4x4_both.both_layers);
  EXPECT_EQ(corner_4x4_both.tile_count, 16);

  auto weird_bottom = parser_->GetObjectDrawInfo(0x110);
  EXPECT_EQ(weird_bottom.draw_routine_id, 36);
  EXPECT_TRUE(weird_bottom.both_layers);
  EXPECT_EQ(weird_bottom.tile_count, 12);

  auto weird_top = parser_->GetObjectDrawInfo(0x114);
  EXPECT_EQ(weird_top.draw_routine_id, 37);
  EXPECT_TRUE(weird_top.both_layers);
  EXPECT_EQ(weird_top.tile_count, 12);
}

TEST_F(ObjectParserTest,
       DrawInfoUsesRegistryRoutineMappingForSubtype1Midrange) {
  auto& registry = zelda3::DrawRoutineRegistry::Get();

  for (int id : {0x40, 0x47, 0x48, 0x4C, 0x4D, 0x50, 0x5D, 0x5F}) {
    SCOPED_TRACE(id);
    auto info = parser_->GetObjectDrawInfo(id);
    EXPECT_EQ(info.draw_routine_id, registry.GetRoutineIdForObject(id));
  }
}

TEST_F(ObjectParserTest,
       DrawInfoMatchesRegistryForAllSubtype1Objects_CustomFlagOnOff) {
  auto& registry = zelda3::DrawRoutineRegistry::Get();

  for (bool enabled : {false, true}) {
    ScopedCustomObjectsFlag scoped_flag(enabled);

    const int expected_custom_routine =
        enabled ? zelda3::DrawRoutineIds::kCustomObject
                : zelda3::DrawRoutineIds::kNothing;

    for (int id = 0x00; id <= 0xF7; ++id) {
      SCOPED_TRACE(::testing::Message() << "custom=" << enabled
                                        << " object_id=0x" << std::hex << id);

      const int expected_routine = registry.GetRoutineIdForObject(id);
      ASSERT_GE(expected_routine, 0);

      auto info = parser_->GetObjectDrawInfo(id);
      EXPECT_EQ(info.draw_routine_id, expected_routine);

      const zelda3::DrawRoutineInfo* routine =
          registry.GetRoutineInfo(expected_routine);
      if (routine != nullptr) {
        EXPECT_EQ(info.routine_name, routine->name);
        EXPECT_EQ(info.both_layers, routine->draws_to_both_bgs);
      } else {
        EXPECT_EQ(info.routine_name, "DefaultSolid");
        EXPECT_FALSE(info.both_layers);
      }

      auto subtype = parser_->GetObjectSubtype(id);
      ASSERT_TRUE(subtype.ok());
      EXPECT_EQ(info.tile_count, subtype->max_tile_count);
    }

    EXPECT_EQ(parser_->GetObjectDrawInfo(0x31).draw_routine_id,
              expected_custom_routine);
    EXPECT_EQ(parser_->GetObjectDrawInfo(0x32).draw_routine_id,
              expected_custom_routine);
  }
}

// 2026-04-25 audit follow-up: pin the registry mappings for the
// subtype-2 / subtype-3 over-fetch clusters identified by
// `zelda3-hacking-expert`. These IDs currently take the parser's
// 8-tile fallback even though their bound routines read fewer tiles
// (`Rightwards2x2_1to16` reads 4, `Rightwards1x1Solid_1to16_plus3`
// reads 1, `RightwardsStatue2x3spaced2_1to16` reads 6, `DrawSingle2x2`
// reads 4). The over-fetch is harmless — the routines reference fixed
// indices below their span and ignore the trailing slots — so this
// test does NOT mirror the parser's tile-count table; it pins the
// audit's id→routine mapping at the registry level so a future
// routing refactor can't silently invalidate the audit's premise.
//
// If/when the production tile-count fallback tightens, the parser's
// existing `DetectsType2Objects` / `DetectsType3Objects` tests will
// surface that change directly.
TEST_F(ObjectParserTest, OverFetchClusterIdsRouteToAuditedRoutines) {
  auto& registry = zelda3::DrawRoutineRegistry::Get();

  // Subtype-2: routine 4 is `Rightwards2x2_1to16` (reads tiles[0..3]).
  for (int id :
       {0x118, 0x119, 0x11A, 0x11B, 0x11E, 0x127, 0x12A, 0x12B, 0x134}) {
    SCOPED_TRACE(::testing::Message()
                 << "id=0x" << std::hex << id << " expects routine 4");
    EXPECT_EQ(registry.GetRoutineIdForObject(id), 4);
  }
  // Subtype-2: routine 28 is `RightwardsStatue2x3spaced2_1to16`
  // (reads tiles[0..5]).
  for (int id : {0x11D, 0x121, 0x126}) {
    SCOPED_TRACE(::testing::Message()
                 << "id=0x" << std::hex << id << " expects routine 28");
    EXPECT_EQ(registry.GetRoutineIdForObject(id), 28);
  }
  // Subtype-2/3: routine 25 is `Rightwards1x1Solid_1to16_plus3`
  // (reads tiles[0]).
  for (int id : {0x11F, 0x120, 0xF96}) {
    SCOPED_TRACE(::testing::Message()
                 << "id=0x" << std::hex << id << " expects routine 25");
    EXPECT_EQ(registry.GetRoutineIdForObject(id), 25);
  }
  // Subtype-3: routine 110 is `DrawSingle2x2` (reads tiles[0..3]).
  // Cluster scoped to IDs explicitly mapped in the registry; the
  // initial audit listed a broader set, but registry diff showed the
  // others fall through to a different default routine.
  for (int id : {0xF90, 0xF91, 0xF93, 0xFAB, 0xFAC, 0xFAF, 0xFB0, 0xFC9, 0xFCA,
                 0xFDE, 0xFDF, 0xFF5}) {
    SCOPED_TRACE(::testing::Message()
                 << "id=0x" << std::hex << id << " expects routine 110");
    EXPECT_EQ(registry.GetRoutineIdForObject(id), 110);
  }
  // Subtype-3: routine 39 is `DrawChest`. Audit initially proposed
  // tightening the parser fallback to 4 here, but
  // `special_routines.cc:157-167` shows the open-state branch reads
  // tiles[4..7]; tightening to 4 would force `TileAtWrapped` to
  // substitute closed-state bytes for open-chest previews. Pin the
  // registry mapping so the routine attribution stays correct even
  // if the parser fallback is later tightened to 8 with a comment.
  for (int id : {0xF99, 0xF9A}) {
    SCOPED_TRACE(::testing::Message()
                 << "id=0x" << std::hex << id << " expects routine 39");
    EXPECT_EQ(registry.GetRoutineIdForObject(id), 39);
  }
}

TEST_F(ObjectParserTest, InvalidObjectId) {
  auto result = parser_->ParseObject(-1);
  EXPECT_FALSE(result.ok());
  EXPECT_EQ(result.status().code(), absl::StatusCode::kInvalidArgument);
}

TEST_F(ObjectParserTest, NullRom) {
  zelda3::ObjectParser null_parser(nullptr);
  auto result = null_parser.ParseObject(0x01);
  EXPECT_FALSE(result.ok());
  EXPECT_EQ(result.status().code(), absl::StatusCode::kInvalidArgument);
}

}  // namespace test
}  // namespace yaze
