#include "zelda3/overworld/tile16_metadata.h"

namespace yaze::zelda3 {

const gfx::TileInfo& Tile16QuadrantInfo(const gfx::Tile16& tile, int quadrant) {
  switch (quadrant) {
    case 0:
      return tile.tile0_;
    case 1:
      return tile.tile1_;
    case 2:
      return tile.tile2_;
    default:
      return tile.tile3_;
  }
}

gfx::TileInfo& MutableTile16QuadrantInfo(gfx::Tile16& tile, int quadrant) {
  switch (quadrant) {
    case 0:
      return tile.tile0_;
    case 1:
      return tile.tile1_;
    case 2:
      return tile.tile2_;
    default:
      return tile.tile3_;
  }
}

void SyncTile16TilesInfo(gfx::Tile16* tile) {
  if (!tile) {
    return;
  }
  tile->tiles_info[0] = tile->tile0_;
  tile->tiles_info[1] = tile->tile1_;
  tile->tiles_info[2] = tile->tile2_;
  tile->tiles_info[3] = tile->tile3_;
}

void SetTile16AllQuadrantPalettes(gfx::Tile16* tile, uint8_t palette_id) {
  if (!tile) {
    return;
  }
  const uint8_t palette = static_cast<uint8_t>(palette_id & 0x07);
  tile->tile0_.palette_ = palette;
  tile->tile1_.palette_ = palette;
  tile->tile2_.palette_ = palette;
  tile->tile3_.palette_ = palette;
  SyncTile16TilesInfo(tile);
}

bool SetTile16QuadrantPalette(gfx::Tile16* tile, int quadrant,
                              uint8_t palette_id) {
  if (!tile || quadrant < 0 || quadrant > 3) {
    return false;
  }
  MutableTile16QuadrantInfo(*tile, quadrant).palette_ =
      static_cast<uint8_t>(palette_id & 0x07);
  SyncTile16TilesInfo(tile);
  return true;
}

gfx::TileInfo HorizontalFlipTileInfo(gfx::TileInfo info) {
  info.horizontal_mirror_ = !info.horizontal_mirror_;
  return info;
}

gfx::TileInfo VerticalFlipTileInfo(gfx::TileInfo info) {
  info.vertical_mirror_ = !info.vertical_mirror_;
  return info;
}

gfx::Tile16 HorizontalFlipTile16(gfx::Tile16 tile) {
  const gfx::Tile16 original = tile;
  tile.tile0_ = HorizontalFlipTileInfo(original.tile1_);
  tile.tile1_ = HorizontalFlipTileInfo(original.tile0_);
  tile.tile2_ = HorizontalFlipTileInfo(original.tile3_);
  tile.tile3_ = HorizontalFlipTileInfo(original.tile2_);
  SyncTile16TilesInfo(&tile);
  return tile;
}

gfx::Tile16 VerticalFlipTile16(gfx::Tile16 tile) {
  const gfx::Tile16 original = tile;
  tile.tile0_ = VerticalFlipTileInfo(original.tile2_);
  tile.tile1_ = VerticalFlipTileInfo(original.tile3_);
  tile.tile2_ = VerticalFlipTileInfo(original.tile0_);
  tile.tile3_ = VerticalFlipTileInfo(original.tile1_);
  SyncTile16TilesInfo(&tile);
  return tile;
}

gfx::Tile16 RotateTile16Clockwise(gfx::Tile16 tile) {
  const gfx::Tile16 original = tile;
  tile.tile0_ = original.tile2_;  // BL -> TL
  tile.tile1_ = original.tile0_;  // TL -> TR
  tile.tile2_ = original.tile3_;  // BR -> BL
  tile.tile3_ = original.tile1_;  // TR -> BR
  SyncTile16TilesInfo(&tile);
  return tile;
}

}  // namespace yaze::zelda3
