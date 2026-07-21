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
  // ASM objects 0x203-0x20C, 0x20E, and 0x20F each point to one tile word;
  // RoomDraw_SomariaLine writes that word once at the object anchor.
  constexpr int16_t kObjectIds[] = {0xF83, 0xF84, 0xF85, 0xF86, 0xF87, 0xF88,
                                    0xF89, 0xF8A, 0xF8B, 0xF8C, 0xF8E, 0xF8F};
  for (int16_t object_id : kObjectIds) {
    SCOPED_TRACE(::testing::Message()
                 << "object_id=0x" << std::hex << object_id);
    auto result = parser_->ParseObject(object_id);
    ASSERT_TRUE(result.ok());
    EXPECT_EQ(result->size(), 1u);
  }
}

TEST_F(ObjectParserTest, RupeeFloorLoadsExactTwoWordPayload) {
  auto result = parser_->ParseObject(0xF92);
  ASSERT_TRUE(result.ok());
  EXPECT_EQ(result->size(), 2u);

  const auto draw_info = parser_->GetObjectDrawInfo(0xF92);
  EXPECT_EQ(draw_info.tile_count, 2);
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

TEST_F(ObjectParserTest,
       CanonicalDoubledAndBarPayloadCountsMatchDrawerContracts) {
  struct TestCase {
    int16_t object_id;
    int expected_tiles;
    int expected_routine;
  };

  for (const auto& test_case : {
           TestCase{0x3C, 8,
                    zelda3::DrawRoutineIds::kRightwardsDoubled2x2spaced2_1to16},
           TestCase{0x4C, 12, zelda3::DrawRoutineIds::kRightwardsBar4x3_1to16},
       }) {
    SCOPED_TRACE(::testing::Message()
                 << "object_id=0x" << std::hex << test_case.object_id);

    auto parsed = parser_->ParseObject(test_case.object_id);
    ASSERT_TRUE(parsed.ok()) << parsed.status();
    EXPECT_EQ(parsed->size(), static_cast<size_t>(test_case.expected_tiles));

    auto subtype = parser_->GetObjectSubtype(test_case.object_id);
    ASSERT_TRUE(subtype.ok()) << subtype.status();
    EXPECT_EQ(subtype->max_tile_count, test_case.expected_tiles);

    const auto draw_info = parser_->GetObjectDrawInfo(test_case.object_id);
    EXPECT_EQ(draw_info.tile_count, test_case.expected_tiles);
    EXPECT_EQ(draw_info.draw_routine_id, test_case.expected_routine);

    const auto* routine = zelda3::DrawRoutineRegistry::Get().GetRoutineInfo(
        test_case.expected_routine);
    ASSERT_NE(routine, nullptr);
    EXPECT_EQ(routine->min_tiles, test_case.expected_tiles);
  }
}

// 2026-04-25 ZScream parity diff. ZScream's `subtype1Lengths` array
// (`ZScreamDungeon/ZeldaFullEditor/Data/DungeonObjectData.cs:184`) is the
// upstream provenance source for `kSubtype1TileLengths` in
// `src/zelda3/dungeon/object_parser.cc`. A full byte-for-byte audit
// confirmed the original table provenance. Yaze keeps a small allowlist of
// routine-body-proven corrections where ZScream under-fetches or over-fetches.
//
// This test pins the parity result so future drift on either side is
// caught: a change in the production yaze table that contradicts ZScream
// at a non-divergent ID will fail; a new yaze divergence not in
// `kKnownDivergences` will fail; if ZScream fixes an allowlisted count
// upstream, the corresponding divergence assertion will fail and the
// allowlist can be retired entry by entry.
TEST_F(ObjectParserTest,
       Subtype1TileLengthsMatchZScreamReferenceExceptKnownDivergences) {
  // ZScream `subtype1Lengths` reference, copied verbatim from
  // ZeldaFullEditor/Data/DungeonObjectData.cs:184-202.
  // clang-format off
  static constexpr uint8_t kZScreamSubtype1Lengths[0xF8] = {
       4, 8, 8, 8, 8, 8, 8, 4, 4, 5, 5, 5, 5, 5, 5, 5,  // 0x00-0x0F
       5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5,  // 0x10-0x1F
       5, 9, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 6,  // 0x20-0x2F
       6, 1, 1,16, 1, 1,16,16, 6, 8,12,12, 4, 8, 4, 3,  // 0x30-0x3F
       3, 3, 3, 3, 3, 3, 3, 0, 0, 8, 8, 4, 9,16,16,16,  // 0x40-0x4F
       1,18,18, 4, 1, 8, 8, 1, 1, 1, 1,18,18,15, 4, 3,  // 0x50-0x5F
       4, 8, 8, 8, 8, 8, 8, 4, 4, 3, 1, 1, 6, 6, 1, 1,  // 0x60-0x6F
      16, 1, 1,16,16, 8,16,16, 4, 1, 1, 4, 1, 4, 1, 8,  // 0x70-0x7F
       8,12,12,12,12,18,18, 8,12, 4, 3, 3, 3, 1, 1, 6,  // 0x80-0x8F
       8, 8, 4, 4,16, 4, 4, 1, 1, 1, 1, 1, 1, 1, 1, 1,  // 0x90-0x9F
       1, 1, 1, 1,24, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,  // 0xA0-0xAF
       1, 1,16, 3, 3, 8, 8, 8, 4, 4,16, 4, 4, 4, 1, 1,  // 0xB0-0xBF
       1,68, 1, 1, 8, 8, 8, 8, 8, 8, 8, 1, 1,28,28, 1,  // 0xC0-0xCF
       1, 8, 8, 0, 0, 0, 0, 1, 8, 8, 8, 8,21,16, 4, 8,  // 0xD0-0xDF
       8, 8, 8, 8, 8, 8, 8, 8, 8, 1, 1, 1, 1, 1, 1, 1,  // 0xE0-0xEF
       1, 1, 1, 1, 1, 1, 1, 1                            // 0xF0-0xF7
  };
  // clang-format on

  struct Divergence {
    int object_id;
    int yaze_value;
    const char* justification;
  };
  // Allowlist of known intentional yaze corrections of upstream ZScream
  // payload counts. Add entries only when the underlying routine body
  // proves the count divergence; cite the routine source location.
  static const Divergence kKnownDivergences[] = {
      {0x3C, 8,
       "DrawRightwardsDoubled2x2spaced2_1to16 (rightwards_routines.cc) "
       "indexes tiles[0..7]; ZScream's 4 under-fetches the second 2x2 "
       "stamp."},
      {0x47, 15,
       "DrawWaterfall47 (special_routines.cc) reads tiles[0..14]; "
       "ZScream's 0->8 fallback under-fetched and TileAtWrapped "
       "substituted wrong tiles for index>=8 (commits e9938002/c12c3178)."},
      {0x48, 9, "DrawWaterfall48 reads tiles[0..8]; same pattern as 0x47."},
      {0x4C, 12,
       "DrawRightwardsBar4x3_1to16 (rightwards_routines.cc) indexes "
       "tiles[0..11]; ZScream's 9 under-fetches the fourth 3-tile column."},
      {0xCD, 24,
       "RoomDraw_MovingWallWest reads obj072A words 0..23; ZScream's 28 "
       "over-fetches four words from obj075A."},
      {0xCE, 24,
       "RoomDraw_MovingWallEast reads obj075A words 0..23; ZScream's 28 "
       "over-fetches four words from obj078A."},
      {0xD3, 0, "DrawNothing wall-moved check; no tile payload is consumed."},
      {0xD4, 0, "DrawNothing wall-moved check; no tile payload is consumed."},
      {0xD5, 0, "DrawNothing wall-moved check; no tile payload is consumed."},
      {0xD6, 0, "DrawNothing wall-moved check; no tile payload is consumed."},
  };

  auto find_divergence = [&](int id) -> const Divergence* {
    for (const auto& d : kKnownDivergences) {
      if (d.object_id == id)
        return &d;
    }
    return nullptr;
  };

  for (int id = 0x00; id <= 0xF7; ++id) {
    SCOPED_TRACE(::testing::Message() << "object_id=0x" << std::hex << id);

    auto subtype_info = parser_->GetObjectSubtype(id);
    ASSERT_TRUE(subtype_info.ok());

    if (const Divergence* d = find_divergence(id)) {
      EXPECT_EQ(subtype_info->max_tile_count, d->yaze_value)
          << "Known yaze divergence drift: " << d->justification;
      // Sanity: the divergence row must actually differ from the ZScream
      // reference, otherwise the entry is stale and should be removed.
      const int zs_raw = kZScreamSubtype1Lengths[id];
      const int zs_effective = (zs_raw > 0) ? zs_raw : 8;
      EXPECT_NE(d->yaze_value, zs_effective)
          << "Stale allowlist entry: ZScream now matches yaze at 0x" << std::hex
          << id << " — drop the divergence row.";
    } else {
      const int zs_raw = kZScreamSubtype1Lengths[id];
      const int zs_effective = (zs_raw > 0) ? zs_raw : 8;
      EXPECT_EQ(subtype_info->max_tile_count, zs_effective)
          << "Drift from ZScream parity. If intentional, add an entry "
             "to kKnownDivergences with routine-body justification.";
    }
  }
}

TEST_F(ObjectParserTest, TurtleRockPipesProvideTwentyFourTiles) {
  // Routine-body proof (Waterfall47/48-tier under-fetch):
  // - 0xFBA, 0xFBB -> kVerticalTurtleRockPipe (102):
  //     special_routines.cc:430-437 stacks two 4x3 sections, reading
  //     tiles[0..11] then tiles[12..23] -> 24 tiles.
  // - 0xFBC, 0xFBD -> kHorizontalTurtleRockPipe (103):
  //     special_routines.cc:439-441 calls DrawNx4(columns=6, start=0)
  //     which reads a 6x4 column-major block -> 24 tiles.
  // Before the GetSubtype3TileCount special case landed, the parser
  // defaulted to 8 and TileAtWrapped substituted tiles[0..7] for
  // indices 8..23, mis-tiling the pipe sections. Pin the count via
  // both the ParseObject and GetObjectDrawInfo paths so neither
  // surface can regress to the wrap-substitution failure mode.
  for (int id : {0xFBA, 0xFBB, 0xFBC, 0xFBD}) {
    SCOPED_TRACE(::testing::Message() << "object_id=0x" << std::hex << id);

    auto parsed = parser_->ParseObject(id);
    ASSERT_TRUE(parsed.ok());
    EXPECT_EQ(parsed.value().size(), 24u);
    EXPECT_NE(parsed.value().size(), 8u);

    auto info = parser_->GetObjectDrawInfo(id);
    EXPECT_EQ(info.tile_count, 24);
    EXPECT_NE(info.tile_count, 8);
  }
}

TEST_F(ObjectParserTest, HammerPegUsesUsdasmSingle2x2RoutineAndFourTiles) {
  auto& registry = zelda3::DrawRoutineRegistry::Get();
  EXPECT_EQ(registry.GetRoutineIdForObject(0xF96),
            zelda3::DrawRoutineIds::kSingle2x2);

  auto parsed = parser_->ParseObject(0xF96);
  ASSERT_TRUE(parsed.ok());
  EXPECT_EQ(parsed->size(), 4u);

  const auto info = parser_->GetObjectDrawInfo(0xF96);
  EXPECT_EQ(info.tile_count, 4);
  EXPECT_EQ(info.draw_routine_id, zelda3::DrawRoutineIds::kSingle2x2);
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
  // Subtype-2: routine 25 is `Rightwards1x1Solid_1to16_plus3`
  // (reads tiles[0]).
  for (int id : {0x11F, 0x120}) {
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
