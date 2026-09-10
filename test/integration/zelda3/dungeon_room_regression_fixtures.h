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
// Golden checksums regenerated 2026-09-10 against canonical US ROM
// SHA-1 6d4f10a8b10e10dbe624cb23cf03b88bb8252973 (roms/zelda3.sfc; first 1 MiB
// of padded alttp_vanilla.sfc). The fixture renderer now loads each room header
// before drawing and applies the same merge/effect configuration as the editor
// canvas. Raw object counts treat palette index 255 as transparent instead of
// counting the entire initialized buffer. Layout-stream pit/mask objects remain
// on the upper tilemap selected by the stream, matching USDASM pointer routing.
// These self-fingerprints are drift guards only — not independent visual 1:1.
// Independent truth comes from Mesen ROI fixtures for rooms 0x007, 0x012,
// 0x031, 0x065, and 0x076.
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
            .composite_checksum = 18079800016860863152ull,
            .object_bg1_checksum = 16473803172668162085ull,
            .object_bg2_checksum = 2272669664059805342ull,
            .layout_bg1_checksum = 16155382640141831219ull,
            .composite_non_backdrop_pixels = 262144,
            .object_bg1_non_backdrop_pixels = 57088,
            .object_bg2_non_backdrop_pixels = 39168,
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
            .composite_checksum = 16835019496338994218ull,
            .object_bg1_checksum = 12090658100540583468ull,
            .object_bg2_checksum = 15343382323770861576ull,
            .layout_bg1_checksum = 14374714413720404755ull,
            .composite_non_backdrop_pixels = 262144,
            .object_bg1_non_backdrop_pixels = 52608,
            .object_bg2_non_backdrop_pixels = 46464,
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
            .composite_checksum = 6950119620544630590ull,
            .object_bg1_checksum = 2789486847902414902ull,
            .object_bg2_checksum = 7686508337355327995ull,
            .layout_bg1_checksum = 7897614742461255965ull,
            .composite_non_backdrop_pixels = 262144,
            .object_bg1_non_backdrop_pixels = 75456,
            .object_bg2_non_backdrop_pixels = 11520,
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
            .composite_checksum = 17351781091166995936ull,
            .object_bg1_checksum = 18256671800728123299ull,
            .object_bg2_checksum = 2709826818660264417ull,
            .layout_bg1_checksum = 10260335362238553655ull,
            .composite_non_backdrop_pixels = 231372,
            .object_bg1_non_backdrop_pixels = 150848,
            .object_bg2_non_backdrop_pixels = 37324,
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
            .composite_checksum = 2845038811880130538ull,
            .object_bg1_checksum = 5024287954780586053ull,
            .object_bg2_checksum = 11028269878064776067ull,
            .layout_bg1_checksum = 10084805582793809819ull,
            .composite_non_backdrop_pixels = 262144,
            .object_bg1_non_backdrop_pixels = 60096,
            .object_bg2_non_backdrop_pixels = 0,
        },
};

inline constexpr int kDungeonRoomRegressionFixtureCount =
    static_cast<int>(sizeof(kDungeonRoomRegressionFixtures) /
                     sizeof(kDungeonRoomRegressionFixtures[0]));

}  // namespace yaze::zelda3::test

#endif  // YAZE_TEST_INTEGRATION_ZELDA3_DUNGEON_ROOM_REGRESSION_FIXTURES_H_
