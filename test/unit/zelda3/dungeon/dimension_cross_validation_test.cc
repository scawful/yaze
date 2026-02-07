#include "gtest/gtest.h"
#include "rom/rom.h"
#include "zelda3/dungeon/dimension_service.h"
#include "zelda3/dungeon/geometry/object_geometry.h"
#include "zelda3/dungeon/object_dimensions.h"
#include "zelda3/dungeon/room_object.h"

namespace yaze::zelda3 {

// Representative (object_id, size) pairs across subtypes.
struct CrossValidationParam {
  int16_t object_id;
  uint8_t size;
  const char* label;
};

class DimensionCrossValidationTest
    : public ::testing::TestWithParam<CrossValidationParam> {
 protected:
  void SetUp() override {
    rom_ = std::make_unique<Rom>();
    std::vector<uint8_t> mock(1024 * 1024, 0);
    rom_->LoadFromData(mock);
    ObjectDimensionTable::Get().Reset();
    ObjectDimensionTable::Get().LoadFromRom(rom_.get());
  }

  void TearDown() override {
    ObjectDimensionTable::Get().Reset();
    rom_.reset();
  }

  std::unique_ptr<Rom> rom_;
};

TEST_P(DimensionCrossValidationTest, CompareAllThreeSystems) {
  auto param = GetParam();
  RoomObject obj(param.object_id, 0, 0, param.size, 0);

  // 1. ObjectGeometry (exact buffer-replay)
  auto geo_result = ObjectGeometry::Get().MeasureByObjectId(obj);
  int geo_w = 0, geo_h = 0;
  bool geo_ok = geo_result.ok();
  if (geo_ok) {
    geo_w = geo_result->width_tiles;
    geo_h = geo_result->height_tiles;
  }

  // 2. DimensionService (facade)
  auto svc_result = DimensionService::Get().GetDimensions(obj);
  int svc_w = svc_result.width_tiles;
  int svc_h = svc_result.height_tiles;

  // DimensionService should match Geometry when available
  if (geo_ok) {
    EXPECT_EQ(svc_w, geo_w) << param.label << " Service vs Geometry width";
    EXPECT_EQ(svc_h, geo_h) << param.label << " Service vs Geometry height";
  }

  // All systems should return positive dimensions
  EXPECT_GT(svc_w, 0) << param.label;
  EXPECT_GT(svc_h, 0) << param.label;
  if (geo_ok) {
    EXPECT_GT(geo_w, 0) << param.label;
    EXPECT_GT(geo_h, 0) << param.label;
  }
}

// Subtype 1 objects (0x00-0xFF)
// Subtype 2 objects (0x100+)
// Various sizes
INSTANTIATE_TEST_SUITE_P(
    RepresentativeObjects, DimensionCrossValidationTest,
    ::testing::Values(
        // Horizontal walls
        CrossValidationParam{0x00, 0, "Wall_H_size0"},
        CrossValidationParam{0x00, 1, "Wall_H_size1"},
        CrossValidationParam{0x00, 5, "Wall_H_size5"},
        // Diagonal walls
        CrossValidationParam{0x09, 0, "Diag_Acute_size0"},
        CrossValidationParam{0x09, 3, "Diag_Acute_size3"},
        CrossValidationParam{0x0A, 0, "Diag_Obtuse_size0"},
        // Vertical walls
        CrossValidationParam{0x60, 0, "Wall_V_size0"},
        CrossValidationParam{0x60, 5, "Wall_V_size5"},
        // Corners
        CrossValidationParam{0x22, 0, "Corner_size0"},
        // Fixed objects
        CrossValidationParam{0xF9, 0, "Chest_size0"},
        CrossValidationParam{0xFB, 0, "BigChest_size0"},
        // Super squares
        CrossValidationParam{0xC0, 0, "SuperSquare_size0"},
        CrossValidationParam{0xC0, 3, "SuperSquare_size3"},
        // 4x4 blocks
        CrossValidationParam{0xA4, 0, "Block4x4_size0"},
        // Type 2 objects
        CrossValidationParam{0x100, 0, "Type2_Rightwards4x4_size0"},
        CrossValidationParam{0x100, 5, "Type2_Rightwards4x4_size5"},
        // Rails / special
        CrossValidationParam{0x34, 0, "Rail_size0"},
        CrossValidationParam{0x34, 3, "Rail_size3"}),
    [](const ::testing::TestParamInfo<CrossValidationParam>& info) {
      return info.param.label;
    });

}  // namespace yaze::zelda3
