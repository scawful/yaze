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
// Golden checksums regenerated 2026-07-22 after the USDASM-correct Left/Right
// dungeon palette-slot mapping fix (62f954b68).
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
            .composite_checksum = 5018637587913985457ull,
            .object_bg1_checksum = 7942374673971519889ull,
            .object_bg2_checksum = 16314113468206397296ull,
            .layout_bg1_checksum = 16155382640141831219ull,
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
            .composite_checksum = 7186032506148411670ull,
            .object_bg1_checksum = 12203971081140519348ull,
            .object_bg2_checksum = 4665773132931031645ull,
            .layout_bg1_checksum = 14374714413720404755ull,
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
            .composite_checksum = 7928503047124983480ull,
            .object_bg1_checksum = 10501821798088277057ull,
            .object_bg2_checksum = 14741702422515376347ull,
            .layout_bg1_checksum = 3904745168084633499ull,
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
            .composite_checksum = 2251720105443116807ull,
            .object_bg1_checksum = 38343009025063977ull,
            .object_bg2_checksum = 9908005591592637895ull,
            .layout_bg1_checksum = 6452462266518031539ull,
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
            .composite_checksum = 11931349271745880540ull,
            .object_bg1_checksum = 3494573777721387843ull,
            .object_bg2_checksum = 11028269878064776067ull,
            .layout_bg1_checksum = 8128105348724780643ull,
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
