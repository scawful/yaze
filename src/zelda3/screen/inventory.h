#ifndef YAZE_APP_ZELDA3_INVENTORY_H
#define YAZE_APP_ZELDA3_INVENTORY_H

#include "absl/status/status.h"
#include "app/gfx/core/bitmap.h"
#include "app/gfx/types/snes_palette.h"
#include "app/gfx/types/snes_tile.h"
#include "app/gui/canvas/canvas.h"
#include "app/rom.h"

namespace yaze {
namespace zelda3 {

constexpr int kInventoryStart = 0x6564A;
// ItemIcons base address in SNES format (0x0DF629)
constexpr int kItemIconsPtr = 0x0DF629;

/**
 * @brief Represents a single item icon (2x2 tiles = 4 tile words)
 */
struct ItemIcon {
  uint16_t tile_tl;  // Top-left tile word (vhopppcc cccccccc format)
  uint16_t tile_tr;  // Top-right tile word
  uint16_t tile_bl;  // Bottom-left tile word
  uint16_t tile_br;  // Bottom-right tile word
  std::string name;  // Human-readable name for debugging
};

/**
 * @brief Inventory manages the inventory screen graphics and layout.
 *
 * The inventory screen consists of a 256x256 bitmap displaying equipment,
 * items, and UI elements using 2BPP graphics and HUD palette.
 */
class Inventory {
 public:
  /**
   * @brief Initialize and load inventory screen data from ROM
   * @param rom ROM instance to read data from
   */
  absl::Status Create(Rom* rom);

  auto &bitmap() { return bitmap_; }
  auto &tilesheet() { return tilesheets_bmp_; }
  auto &palette() { return palette_; }
  auto &item_icons() { return item_icons_; }

 private:
  /**
   * @brief Build the tileset from 2BPP graphics
   * @param rom ROM instance to read graphics from
   */
  absl::Status BuildTileset(Rom* rom);

  /**
   * @brief Load individual item icons from ROM
   * @param rom ROM instance to read icon data from
   */
  absl::Status LoadItemIcons(Rom* rom);

  std::vector<uint8_t> data_;
  gfx::Bitmap bitmap_;

  std::vector<uint8_t> tilesheets_;
  std::vector<uint8_t> test_;
  gfx::Bitmap tilesheets_bmp_;
  gfx::SnesPalette palette_;

  gui::Canvas canvas_;
  std::vector<gfx::TileInfo> tiles_;
  std::vector<ItemIcon> item_icons_;
};

}  // namespace zelda3
}  // namespace yaze

#endif  // YAZE_APP_ZELDA3_INVENTORY_H
