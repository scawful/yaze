#ifndef YAZE_TEST_INTEGRATION_ZELDA3_DUNGEON_ROOM_REGRESSION_FIXTURES_H_
#define YAZE_TEST_INTEGRATION_ZELDA3_DUNGEON_ROOM_REGRESSION_FIXTURES_H_

#include <cstdint>

namespace yaze::zelda3::test {

// Symptom categories for ROM-backed room render regression fixtures.
// See docs/internal/hand-off/HANDOFF_DUNGEON_RENDERING_REGRESSION_TRIAGE_2026-04-16.md
enum class DungeonRoomRegressionCategory : uint8_t {
  kCornerAndOverlayStreams = 0,
  kBg2OverlayPlatform,
  kOpaqueBg2Control,
  kTranslucentBg2Control,
  kPitOrMask,
};

struct DungeonRoomRegressionFixture {
  int room_id;
  DungeonRoomRegressionCategory category;
  const char* name;
  const char* notes;
  // Minimum object ids that must be present in the loaded room stream.
  int required_object_id;
  // Expected layer-merge id (0-8) from room header; -1 skips the check.
  int expected_layer_merge_id;
  // Golden composite-buffer FNV-1a checksum after full RenderRoomGraphics +
  // RoomLayerManager compositing. Updated when rendering is intentionally changed.
  uint64_t composite_checksum;
  uint64_t object_bg1_checksum;
  uint64_t object_bg2_checksum;
  uint64_t layout_bg1_checksum;
  int composite_non_backdrop_pixels;
  int object_bg1_non_backdrop_pixels;
  int object_bg2_non_backdrop_pixels;
};

// Fixed vanilla-room fixtures (US 1.0). Room ids verified via
// DungeonRoomRegressionFixturesTest.ScanAllRoomsForFixtureCandidates.
// Golden checksums recorded 2026-06-28 via YAZE_RECORD_DUNGEON_ROOM_FIXTURES=1.
inline constexpr DungeonRoomRegressionFixture kDungeonRoomRegressionFixtures[] =
    {
        {
            .room_id = 0x001,
            .category = DungeonRoomRegressionCategory::kCornerAndOverlayStreams,
            .name = "HyruleCastle_Entry",
            .notes =
                "Concave corners 0x100-0x103, 4x4 corners 0x108-0x10B, three "
                "object "
                "streams with BG2 overlay platform objects 0x033/0x034/0x071.",
            .required_object_id = 0x108,
            .expected_layer_merge_id = 6,
            .composite_checksum = 3925581144764392225ull,
            .object_bg1_checksum = 13831736980008890945ull,
            .object_bg2_checksum = 3151175755251988992ull,
            .layout_bg1_checksum = 5628238167289212587ull,
            .composite_non_backdrop_pixels = 262144,
            .object_bg1_non_backdrop_pixels = 262144,
            .object_bg2_non_backdrop_pixels = 262144,
        },
        {
            .room_id = 0x050,
            .category = DungeonRoomRegressionCategory::kBg2OverlayPlatform,
            .name = "HyruleCastle_Room50_034Object",
            .notes = "Regression anchor for object 0x034 on a multi-stream "
                     "Hyrule Castle "
                     "room; exercises single-tile solid payloads on overlay "
                     "streams.",
            .required_object_id = 0x034,
            .expected_layer_merge_id = 6,
            .composite_checksum = 154053754431283302ull,
            .object_bg1_checksum = 124322299965229508ull,
            .object_bg2_checksum = 12015621994152001693ull,
            .layout_bg1_checksum = 5270035654940668381ull,
            .composite_non_backdrop_pixels = 262144,
            .object_bg1_non_backdrop_pixels = 262144,
            .object_bg2_non_backdrop_pixels = 262144,
        },
        {
            .room_id = 0x00E,
            .category = DungeonRoomRegressionCategory::kOpaqueBg2Control,
            .name = "EasternPalace_OpaqueMerge",
            .notes = "Layer merge off (id 0) — BG2 must not apply spurious "
                     "translucency; "
                     "wall/corner objects with no moving-water effect.",
            .required_object_id = 0x100,
            .expected_layer_merge_id = 0,
            .composite_checksum = 15098288716652967704ull,
            .object_bg1_checksum = 2924072550631335041ull,
            .object_bg2_checksum = 275146388502860235ull,
            .layout_bg1_checksum = 1100323173258158507ull,
            .composite_non_backdrop_pixels = 262144,
            .object_bg1_non_backdrop_pixels = 262144,
            .object_bg2_non_backdrop_pixels = 262144,
        },
        {
            .room_id = 0x016,
            .category = DungeonRoomRegressionCategory::kTranslucentBg2Control,
            .name = "SwampPalace_MovingWater",
            .notes = "Moving-water effect with translucent BG2 merge (id 4) — "
                     "control "
                     "case for intentional half-color compositing.",
            .required_object_id = 0x108,
            .expected_layer_merge_id = 4,
            // FF1 now honors the inactive room-0x065 bombed-floor state, so
            // the default room render correctly omits the big light beam.
            .composite_checksum = 7101433555545613415ull,
            .object_bg1_checksum = 3344402020867724551ull,
            .object_bg2_checksum = 10195791645045441415ull,
            .layout_bg1_checksum = 8945647139078008391ull,
            .composite_non_backdrop_pixels = 208440,
            .object_bg1_non_backdrop_pixels = 262144,
            .object_bg2_non_backdrop_pixels = 262144,
        },
        {
            .room_id = 0x004,
            .category = DungeonRoomRegressionCategory::kPitOrMask,
            .name = "DesertPalace_PitEdges",
            .notes = "Pit-edge objects on an opaque-merge room exercising BG1 "
                     "transparency "
                     "holes that reveal BG2 beneath.",
            .required_object_id = 0x022,
            .expected_layer_merge_id = 0,
            // Room 0x004 contains six F92 rupee-floor objects. Their
            // USDASM-accurate sparse five-by-eight pattern changes BG1.
            .composite_checksum = 5477542344832277612ull,
            .object_bg1_checksum = 16713002165790269043ull,
            .object_bg2_checksum = 11028269878064776067ull,
            .layout_bg1_checksum = 17333734306328304163ull,
            .composite_non_backdrop_pixels = 262144,
            .object_bg1_non_backdrop_pixels = 262144,
            .object_bg2_non_backdrop_pixels = 262144,
        },
};

inline constexpr int kDungeonRoomRegressionFixtureCount =
    static_cast<int>(sizeof(kDungeonRoomRegressionFixtures) /
                     sizeof(kDungeonRoomRegressionFixtures[0]));

}  // namespace yaze::zelda3::test

#endif  // YAZE_TEST_INTEGRATION_ZELDA3_DUNGEON_ROOM_REGRESSION_FIXTURES_H_
