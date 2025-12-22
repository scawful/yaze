#include "zelda3/overworld/diggable_tiles.h"

#include <gtest/gtest.h>

#include <array>
#include <vector>

#include "app/gfx/types/snes_tile.h"

namespace yaze {
namespace zelda3 {
namespace {

// Test fixture for DiggableTiles tests
class DiggableTilesTest : public ::testing::Test {
 protected:
  void SetUp() override { diggable_tiles_.Clear(); }

  DiggableTiles diggable_tiles_;
};

// ============================================================================
// Basic Operations Tests
// ============================================================================

TEST_F(DiggableTilesTest, DefaultStateIsAllClear) {
  // All tiles should be non-diggable by default
  for (uint16_t i = 0; i < kMaxDiggableTileId; ++i) {
    EXPECT_FALSE(diggable_tiles_.IsDiggable(i))
        << "Tile " << i << " should not be diggable by default";
  }
  EXPECT_EQ(diggable_tiles_.GetDiggableCount(), 0);
}

TEST_F(DiggableTilesTest, SetDiggableBasic) {
  diggable_tiles_.SetDiggable(0x034, true);
  EXPECT_TRUE(diggable_tiles_.IsDiggable(0x034));
  EXPECT_FALSE(diggable_tiles_.IsDiggable(0x035));
  EXPECT_EQ(diggable_tiles_.GetDiggableCount(), 1);
}

TEST_F(DiggableTilesTest, ClearDiggable) {
  diggable_tiles_.SetDiggable(0x100, true);
  EXPECT_TRUE(diggable_tiles_.IsDiggable(0x100));

  diggable_tiles_.SetDiggable(0x100, false);
  EXPECT_FALSE(diggable_tiles_.IsDiggable(0x100));
}

TEST_F(DiggableTilesTest, SetMultipleDiggable) {
  diggable_tiles_.SetDiggable(0x034, true);
  diggable_tiles_.SetDiggable(0x035, true);
  diggable_tiles_.SetDiggable(0x071, true);

  EXPECT_TRUE(diggable_tiles_.IsDiggable(0x034));
  EXPECT_TRUE(diggable_tiles_.IsDiggable(0x035));
  EXPECT_TRUE(diggable_tiles_.IsDiggable(0x071));
  EXPECT_FALSE(diggable_tiles_.IsDiggable(0x072));
  EXPECT_EQ(diggable_tiles_.GetDiggableCount(), 3);
}

TEST_F(DiggableTilesTest, ClearAllTiles) {
  diggable_tiles_.SetDiggable(0x034, true);
  diggable_tiles_.SetDiggable(0x100, true);
  diggable_tiles_.SetDiggable(0x1FF, true);

  diggable_tiles_.Clear();

  EXPECT_FALSE(diggable_tiles_.IsDiggable(0x034));
  EXPECT_FALSE(diggable_tiles_.IsDiggable(0x100));
  EXPECT_FALSE(diggable_tiles_.IsDiggable(0x1FF));
  EXPECT_EQ(diggable_tiles_.GetDiggableCount(), 0);
}

// ============================================================================
// Boundary Tests
// ============================================================================

TEST_F(DiggableTilesTest, FirstTileId) {
  diggable_tiles_.SetDiggable(0, true);
  EXPECT_TRUE(diggable_tiles_.IsDiggable(0));
}

TEST_F(DiggableTilesTest, LastValidTileId) {
  diggable_tiles_.SetDiggable(511, true);
  EXPECT_TRUE(diggable_tiles_.IsDiggable(511));
}

TEST_F(DiggableTilesTest, OutOfBoundsTileIdIsNotDiggable) {
  // Tile ID 512 is out of range (max is 511)
  EXPECT_FALSE(diggable_tiles_.IsDiggable(512));
  EXPECT_FALSE(diggable_tiles_.IsDiggable(1000));
  EXPECT_FALSE(diggable_tiles_.IsDiggable(0xFFFF));
}

TEST_F(DiggableTilesTest, SetOutOfBoundsDoesNothing) {
  diggable_tiles_.SetDiggable(512, true);
  // Should not crash and count should remain 0
  EXPECT_EQ(diggable_tiles_.GetDiggableCount(), 0);
}

// ============================================================================
// Bitfield Correctness Tests
// ============================================================================

TEST_F(DiggableTilesTest, BitPositionByte0) {
  // Test bits within first byte (tiles 0-7)
  for (int i = 0; i < 8; ++i) {
    DiggableTiles tiles;
    tiles.SetDiggable(i, true);
    EXPECT_TRUE(tiles.IsDiggable(i)) << "Failed for tile " << i;
    EXPECT_EQ(tiles.GetDiggableCount(), 1);
  }
}

TEST_F(DiggableTilesTest, BitPositionAcrossBytes) {
  // Test bits that span byte boundaries
  diggable_tiles_.SetDiggable(7, true);   // Last bit of byte 0
  diggable_tiles_.SetDiggable(8, true);   // First bit of byte 1
  diggable_tiles_.SetDiggable(15, true);  // Last bit of byte 1
  diggable_tiles_.SetDiggable(16, true);  // First bit of byte 2

  EXPECT_TRUE(diggable_tiles_.IsDiggable(7));
  EXPECT_TRUE(diggable_tiles_.IsDiggable(8));
  EXPECT_TRUE(diggable_tiles_.IsDiggable(15));
  EXPECT_TRUE(diggable_tiles_.IsDiggable(16));
  EXPECT_EQ(diggable_tiles_.GetDiggableCount(), 4);
}

// ============================================================================
// Vanilla Defaults Tests
// ============================================================================

TEST_F(DiggableTilesTest, SetVanillaDefaultsMatchesKnownTiles) {
  diggable_tiles_.SetVanillaDefaults();

  // Vanilla diggable tiles from bank 1B
  EXPECT_TRUE(diggable_tiles_.IsDiggable(0x034));
  EXPECT_TRUE(diggable_tiles_.IsDiggable(0x035));
  EXPECT_TRUE(diggable_tiles_.IsDiggable(0x071));
  EXPECT_TRUE(diggable_tiles_.IsDiggable(0x0DA));
  EXPECT_TRUE(diggable_tiles_.IsDiggable(0x0E1));
  EXPECT_TRUE(diggable_tiles_.IsDiggable(0x0E2));
  EXPECT_TRUE(diggable_tiles_.IsDiggable(0x0F8));
  EXPECT_TRUE(diggable_tiles_.IsDiggable(0x10D));
  EXPECT_TRUE(diggable_tiles_.IsDiggable(0x10E));
  EXPECT_TRUE(diggable_tiles_.IsDiggable(0x10F));

  EXPECT_EQ(diggable_tiles_.GetDiggableCount(), kNumVanillaDiggableTiles);
}

TEST_F(DiggableTilesTest, SetVanillaDefaultsClearsExisting) {
  // Set some custom tiles first
  diggable_tiles_.SetDiggable(0x001, true);
  diggable_tiles_.SetDiggable(0x002, true);

  // Set vanilla defaults should clear custom and set vanilla
  diggable_tiles_.SetVanillaDefaults();

  EXPECT_FALSE(diggable_tiles_.IsDiggable(0x001));
  EXPECT_FALSE(diggable_tiles_.IsDiggable(0x002));
  EXPECT_TRUE(diggable_tiles_.IsDiggable(0x034));  // Vanilla tile
}

// ============================================================================
// GetAllDiggableTileIds Tests
// ============================================================================

TEST_F(DiggableTilesTest, GetAllDiggableTileIdsEmpty) {
  auto ids = diggable_tiles_.GetAllDiggableTileIds();
  EXPECT_TRUE(ids.empty());
}

TEST_F(DiggableTilesTest, GetAllDiggableTileIdsReturnsCorrectIds) {
  diggable_tiles_.SetDiggable(0x034, true);
  diggable_tiles_.SetDiggable(0x100, true);
  diggable_tiles_.SetDiggable(0x1FF, true);

  auto ids = diggable_tiles_.GetAllDiggableTileIds();

  EXPECT_EQ(ids.size(), 3u);
  EXPECT_EQ(ids[0], 0x034);
  EXPECT_EQ(ids[1], 0x100);
  EXPECT_EQ(ids[2], 0x1FF);
}

// ============================================================================
// Serialization Tests
// ============================================================================

TEST_F(DiggableTilesTest, SerializationRoundTrip) {
  // Set some tiles
  diggable_tiles_.SetDiggable(0x034, true);
  diggable_tiles_.SetDiggable(0x071, true);
  diggable_tiles_.SetDiggable(0x1FF, true);

  // Serialize
  std::array<uint8_t, kDiggableTilesBitfieldSize> buffer;
  diggable_tiles_.ToBytes(buffer.data());

  // Deserialize to new instance
  DiggableTiles loaded;
  loaded.FromBytes(buffer.data());

  // Verify
  EXPECT_TRUE(loaded.IsDiggable(0x034));
  EXPECT_TRUE(loaded.IsDiggable(0x071));
  EXPECT_TRUE(loaded.IsDiggable(0x1FF));
  EXPECT_FALSE(loaded.IsDiggable(0x035));
  EXPECT_EQ(loaded.GetDiggableCount(), 3);
}

TEST_F(DiggableTilesTest, GetRawDataMatchesToBytes) {
  diggable_tiles_.SetDiggable(0x000, true);  // Bit 0 of byte 0
  diggable_tiles_.SetDiggable(0x008, true);  // Bit 0 of byte 1

  const auto& raw = diggable_tiles_.GetRawData();

  EXPECT_EQ(raw[0], 0x01);  // Bit 0 set
  EXPECT_EQ(raw[1], 0x01);  // Bit 0 set
  EXPECT_EQ(raw[2], 0x00);  // No bits set
}

// ============================================================================
// IsTile16Diggable Static Method Tests
// ============================================================================

TEST_F(DiggableTilesTest, IsTile16DiggableAllDiggable) {
  // Create tile types array with diggable tiles
  std::array<uint8_t, 0x200> tile_types = {};
  tile_types[10] = kTileTypeDiggable1;  // 0x48
  tile_types[11] = kTileTypeDiggable1;
  tile_types[12] = kTileTypeDiggable2;  // 0x4A
  tile_types[13] = kTileTypeDiggable2;

  // Create Tile16 with all diggable component tiles
  gfx::TileInfo t0, t1, t2, t3;
  t0.id_ = 10;
  t1.id_ = 11;
  t2.id_ = 12;
  t3.id_ = 13;
  gfx::Tile16 tile16(t0, t1, t2, t3);

  EXPECT_TRUE(DiggableTiles::IsTile16Diggable(tile16, tile_types));
}

TEST_F(DiggableTilesTest, IsTile16DiggableOneNonDiggable) {
  // Create tile types array
  std::array<uint8_t, 0x200> tile_types = {};
  tile_types[10] = kTileTypeDiggable1;
  tile_types[11] = kTileTypeDiggable1;
  tile_types[12] = kTileTypeDiggable1;
  tile_types[13] = 0x00;  // Not diggable

  // Create Tile16 with one non-diggable component
  gfx::TileInfo t0, t1, t2, t3;
  t0.id_ = 10;
  t1.id_ = 11;
  t2.id_ = 12;
  t3.id_ = 13;  // Not diggable
  gfx::Tile16 tile16(t0, t1, t2, t3);

  EXPECT_FALSE(DiggableTiles::IsTile16Diggable(tile16, tile_types));
}

TEST_F(DiggableTilesTest, IsTile16DiggableAllNonDiggable) {
  std::array<uint8_t, 0x200> tile_types = {};
  // All tiles have type 0 (not diggable)

  gfx::TileInfo t0, t1, t2, t3;
  t0.id_ = 0;
  t1.id_ = 1;
  t2.id_ = 2;
  t3.id_ = 3;
  gfx::Tile16 tile16(t0, t1, t2, t3);

  EXPECT_FALSE(DiggableTiles::IsTile16Diggable(tile16, tile_types));
}

TEST_F(DiggableTilesTest, IsTile16DiggableMixedDiggableTypes) {
  // Test that both 0x48 and 0x4A are accepted
  std::array<uint8_t, 0x200> tile_types = {};
  tile_types[0] = kTileTypeDiggable1;  // 0x48
  tile_types[1] = kTileTypeDiggable2;  // 0x4A
  tile_types[2] = kTileTypeDiggable1;
  tile_types[3] = kTileTypeDiggable2;

  gfx::TileInfo t0, t1, t2, t3;
  t0.id_ = 0;
  t1.id_ = 1;
  t2.id_ = 2;
  t3.id_ = 3;
  gfx::Tile16 tile16(t0, t1, t2, t3);

  EXPECT_TRUE(DiggableTiles::IsTile16Diggable(tile16, tile_types));
}

}  // namespace
}  // namespace zelda3
}  // namespace yaze
