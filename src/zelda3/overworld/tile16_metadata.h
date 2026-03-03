#ifndef YAZE_ZELDA3_OVERWORLD_TILE16_METADATA_H
#define YAZE_ZELDA3_OVERWORLD_TILE16_METADATA_H

#include <cstdint>

#include "app/gfx/types/snes_tile.h"

namespace yaze::zelda3 {

// Returns quadrant metadata using ALTTP tile16 ordering:
// 0=top-left, 1=top-right, 2=bottom-left, 3=bottom-right.
// Out-of-range quadrant indices intentionally fall back to 3 for compatibility
// with legacy editor callsites.
const gfx::TileInfo& Tile16QuadrantInfo(const gfx::Tile16& tile, int quadrant);
gfx::TileInfo& MutableTile16QuadrantInfo(gfx::Tile16& tile, int quadrant);

// Keeps Tile16::tiles_info synchronized with tile0_..tile3_.
void SyncTile16TilesInfo(gfx::Tile16* tile);

// Palette helpers keep quadrant fields and tiles_info synchronized.
void SetTile16AllQuadrantPalettes(gfx::Tile16* tile, uint8_t palette_id);
bool SetTile16QuadrantPalette(gfx::Tile16* tile, int quadrant,
                              uint8_t palette_id);

// Metadata-only transforms used by editor operations.
gfx::TileInfo HorizontalFlipTileInfo(gfx::TileInfo info);
gfx::TileInfo VerticalFlipTileInfo(gfx::TileInfo info);
gfx::Tile16 HorizontalFlipTile16(gfx::Tile16 tile);
gfx::Tile16 VerticalFlipTile16(gfx::Tile16 tile);
gfx::Tile16 RotateTile16Clockwise(gfx::Tile16 tile);

}  // namespace yaze::zelda3

#endif  // YAZE_ZELDA3_OVERWORLD_TILE16_METADATA_H
