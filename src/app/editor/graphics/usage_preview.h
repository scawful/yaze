#ifndef YAZE_APP_EDITOR_GRAPHICS_USAGE_PREVIEW_H
#define YAZE_APP_EDITOR_GRAPHICS_USAGE_PREVIEW_H

#include <array>
#include <cstdint>
#include <map>
#include <memory>
#include <optional>
#include <set>
#include <string>
#include <vector>

#include "absl/status/status.h"
#include "app/editor/sprite/zsprite.h"
#include "app/gfx/core/bitmap.h"
#include "app/gfx/types/snes_tile.h"
#include "rom/rom.h"
#include "zelda3/dungeon/room.h"
#include "zelda3/game_data.h"
#include "zelda3/gfx_sheet_inventory.h"
#include "zelda3/overworld/overworld_map.h"

namespace yaze::editor::usage_preview {

/**
 * @file usage_preview.h
 * @brief Renders a graphics sheet the way the game uses it, from unsaved
 * sheet pixels, without writing the ROM or GameData.
 *
 * Every context reuses an existing renderer: rooms go through Room's
 * RenderRoomGraphics/RenderComposite, vanilla sprites through
 * Sprite::RenderPreviewGraphics, .zsm frames through SpriteDrawer, and tile16s
 * through OverworldMap::BuildTileset/BuildTiles16Gfx. Edited sheets reach them
 * through the renderers' SetGraphicsSheetOverrides() hooks.
 *
 * No ImGui here, so tests and offscreen captures drive the same code the
 * Usage Preview panel draws.
 */

constexpr size_t kSheetBytes = 4096;  // 128x32 8bpp sheet

/// Sheet id -> 4096 8bpp pixels (the GameData::graphics_buffer layout).
using SheetPixels = std::map<uint16_t, std::vector<uint8_t>>;

/// Copies the Arena sheets in `ids` (where the pixel editor writes). Sheets
/// that are not loaded or not 128x32 8bpp are left out. Returns nothing when
/// the Arena holds another ROM's sheets (its owner is not `game_data`).
SheetPixels SnapshotArenaSheets(const std::set<uint16_t>& ids,
                                const zelda3::GameData* game_data);

/// An 8bpp image with its own 256-color table. Index 0 is transparent when
/// `index0_transparent` is set (sprites, tile16s).
struct IndexedImage {
  int width = 0;
  int height = 0;
  std::vector<uint8_t> pixels;
  std::array<SDL_Color, 256> colors{};
  bool index0_transparent = false;

  bool empty() const { return width <= 0 || height <= 0; }
  /// RGBA8 rows; transparent pixels get alpha 0.
  std::vector<uint8_t> ToRgba() const;
  /// Number of pixels whose index differs from `other` (sizes must match;
  /// returns -1 when they do not).
  int CountDifferentPixels(const IndexedImage& other) const;
};

/// Copies a palettized Bitmap (for example a room composite).
IndexedImage SnapshotBitmap(const gfx::Bitmap& bitmap);

// ---------------------------------------------------------------------------
// Room context
// ---------------------------------------------------------------------------

/// Graphics blocks (0-7 background, 8-15 sprites) per dungeon room, from
/// Room::LoadRoomGraphics() with each room's header blockset and spriteset.
/// Only the group tables are read; no room objects are loaded.
std::map<int, std::array<uint8_t, 16>> CollectRoomSheetBlocks(
    Rom* rom, zelda3::GameData* game_data,
    const std::map<int, zelda3::RoomGfxInfo>& rooms);

/// Rooms whose background blocks (or, with `include_sprites`, sprite blocks)
/// contain `sheet`. Sorted by room id.
std::vector<int> RoomsUsingSheet(
    const std::map<int, std::array<uint8_t, 16>>& room_blocks, uint16_t sheet,
    bool include_sprites);

class RoomUsagePreview {
 public:
  /// Loads `room_id` on first use (or when it changes) and renders it with
  /// `sheets` swapped in. `objects_only` hides the floor and layout layers.
  absl::Status Render(Rom* rom, zelda3::GameData* game_data, int room_id,
                      const SheetPixels& sheets, bool objects_only);

  int room_id() const { return room_id_; }
  const gfx::Bitmap& bitmap() const { return composite_; }
  gfx::Bitmap& mutable_bitmap() { return composite_; }
  IndexedImage Snapshot() const { return SnapshotBitmap(composite_); }

  /// Room graphics blocks (0-7 background, 8-15 sprites) of the loaded room.
  std::array<uint8_t, 16> blocks() const;
  /// Pixel rectangles of objects that draw at least one 8x8 tile from
  /// `sheet` (background blocks only).
  std::vector<SDL_Rect> ObjectRectsUsingSheet(uint16_t sheet) const;
  /// Clears the loaded room so the next Render() reloads it from the ROM.
  void Reset();

 private:
  std::unique_ptr<zelda3::Room> room_;
  int room_id_ = -1;
  gfx::Bitmap composite_;
};

// ---------------------------------------------------------------------------
// Sprite context
// ---------------------------------------------------------------------------

class SpriteUsagePreview {
 public:
  /// Sets the spriteset (sheets for OAM slots 4-7) and the palette source:
  /// the sprite palette of dungeon room `palette_room`, or the default sprite
  /// preview palettes when it is -1. `hack_name` selects project sprite
  /// layouts (SpriteOamRegistry::GetPreviewOverride).
  absl::Status Configure(Rom* rom, zelda3::GameData* game_data, int spriteset,
                         int palette_room, std::string hack_name = {});

  /// Plays a vanilla sprite (single pose from the sprite catalog).
  void SetVanillaSprite(uint8_t sprite_id);
  /// Plays animation `animation` of a .zsm (the preview keeps a copy).
  void SetZsm(const zsprite::ZSprite& zsm, int animation);
  bool has_zsm() const { return zsm_.has_value(); }

  /// Frames in the current animation (1 for vanilla sprites).
  int frame_count() const;
  int frame() const { return frame_; }
  void SetFrame(int frame);
  /// Advances by `seconds` at the animation's speed (60 ticks per second).
  /// Returns true when the frame changed.
  bool Advance(float seconds);

  /// Draws the current frame with `sheets` (edited pixels) over the ROM's.
  IndexedImage RenderFrame(const SheetPixels& sheets) const;

  const std::array<uint8_t, 8>& slots() const { return slots_; }
  bool UsesSheet(uint16_t sheet) const;

 private:
  std::vector<uint8_t> SheetOrRom(const SheetPixels& sheets,
                                  uint16_t sheet) const;

  Rom* rom_ = nullptr;
  zelda3::GameData* game_data_ = nullptr;
  std::array<uint8_t, 8> slots_{};
  std::array<SDL_Color, 256> colors_{};
  std::string hack_name_;
  uint8_t sprite_id_ = 0;
  std::optional<zsprite::ZSprite> zsm_;
  int animation_ = 0;
  int frame_ = 0;
  float remainder_ = 0.0f;
};

// ---------------------------------------------------------------------------
// Tile16 context
// ---------------------------------------------------------------------------

class Tile16UsagePreview {
 public:
  /// Loads overworld area `area` (graphics and palette) and the tile16 table.
  absl::Status Configure(Rom* rom, zelda3::GameData* game_data, int area);

  /// Tile16s in the loaded area's blockset that use 8x8 tile `tile_index`
  /// (0-63) of `sheet`, in any of the slots the area loads it into.
  std::vector<int> Tile16sUsingTile(uint16_t sheet, int tile_index) const;

  /// Draws `tile16s` in rows of 8 (16x16 each) with `sheets` swapped in.
  /// Quadrants that use `sheet`/`tile_index` are listed in `highlights` as
  /// pixel rectangles.
  IndexedImage Render(const SheetPixels& sheets,
                      const std::vector<int>& tile16s, uint16_t sheet,
                      int tile_index,
                      std::vector<SDL_Rect>* highlights = nullptr);

  int area() const { return area_; }

 private:
  std::vector<int> TileIdsFor(uint16_t sheet, int tile_index) const;

  Rom* rom_ = nullptr;
  zelda3::GameData* game_data_ = nullptr;
  int area_ = -1;
  std::unique_ptr<zelda3::OverworldMap> map_;
  std::vector<gfx::Tile16> tiles16_;
};

}  // namespace yaze::editor::usage_preview

#endif  // YAZE_APP_EDITOR_GRAPHICS_USAGE_PREVIEW_H
