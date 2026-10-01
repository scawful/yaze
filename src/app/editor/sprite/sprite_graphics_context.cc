#include "app/editor/sprite/sprite_graphics_context.h"

#include <algorithm>
#include <array>
#include <set>
#include <string>
#include <vector>

#include "absl/strings/str_format.h"
#include "app/editor/graphics/graphics_edit_context.h"
#include "app/editor/graphics/usage_preview.h"
#include "app/editor/sprite/sprite_drawer.h"
#include "zelda3/game_data.h"
#include "zelda3/gfx_sheet_storage.h"
#include "zelda3/sprite/sprite_sheet_slots.h"

namespace yaze::editor::sprite_graphics_context {

// Capture one OAM entry in one selected ZSM frame. A 16x16 entry may cross a
// sheet slot, so resolve and validate each 8x8 tile independently.
absl::StatusOr<GraphicsEditContext> Make(const zsprite::Frame& frame,
                                         int frame_index, int oam_index,
                                         const std::array<uint8_t, 8>& slots,
                                         const gfx::PaletteGroup& palettes,
                                         const zelda3::GameData* owner,
                                         const std::string& source_label,
                                         const std::string& sheet_source) {
  if (!owner || oam_index < 0 ||
      oam_index >= static_cast<int>(frame.Tiles.size())) {
    return absl::InvalidArgumentError("Select a sprite frame and OAM tile");
  }
  const auto& oam = frame.Tiles[oam_index];
  if (oam.palette >= palettes.size() ||
      palettes.palette_ref(oam.palette).size() < 8) {
    return absl::FailedPreconditionError("Selected OAM palette is unresolved");
  }
  const auto& row = palettes.palette_ref(oam.palette);
  std::vector<gfx::SnesColor> colors;
  for (int i = 0; i < 8; ++i)
    colors.push_back(row[i]);
  GraphicsEditContext context;
  context.owner = owner;
  context.label =
      absl::StrFormat("%s / frame %d / OAM %d / palette %d", source_label,
                      frame_index, oam_index, oam.palette);
  context.limitation =
      "Captured pose and sheet assignments; re-enter from Sprite Editor after "
      "changing either.";
  context.resolved.surface = "sprite_oam";
  context.resolved.graphics_loaded = true;
  context.resolved.id = frame_index;
  context.resolved.palette_colors_resolved = true;
  context.resolved.loader_agrees = true;
  context.resolved.palettes.push_back(
      {absl::StrFormat("OAM palette row %d", oam.palette), sheet_source,
       oam.palette, false});
  for (int slot = 0; slot < 8; ++slot) {
    const uint16_t sheet = slots[slot];
    const bool loaded = owner->sheet_store.HasSheet(sheet);
    context.resolved.slots.push_back({
        .slot = slot,
        .role = slot < 4 ? "static_sprite" : "spriteset_sprite",
        .palette_group = absl::StrFormat("sprite_row_%d", oam.palette),
        .sheet_id = sheet,
        .source = sheet_source,
        .loader_agrees = loaded,
        .store_revision = owner->sheet_store.Revision(sheet),
        .width = 128,
        .height = 32,
    });
    context.resolved.loader_agrees &= loaded;
  }
  const int offsets[] = {0, 1, 16, 17};
  const int count = oam.size ? 4 : 1;
  for (int i = 0; i < count; ++i) {
    const int tile = static_cast<int>(oam.id) + offsets[i];
    if (tile < 0 || tile >= 512) {
      return absl::OutOfRangeError("Selected OAM tile extends past tile 0x1FF");
    }
    const int slot = zelda3::SpriteSlotForTile(tile);
    const uint16_t sheet = slots[slot];
    if (zelda3::GetGfxSheetStorageKind(sheet) ==
            zelda3::GfxSheetStorageKind::kCompressed2bpp ||
        !owner->sheet_store.HasSheet(sheet)) {
      return absl::FailedPreconditionError(absl::StrFormat(
          "OAM tile %03X uses unavailable or 2bpp sheet %02X", tile, sheet));
    }
    context.targets.push_back({
        .sheet_id = sheet,
        .tile_index = static_cast<uint16_t>(tile % 64),
        .label = absl::StrFormat(
            "Frame %d / OAM %d / tile %03X / slot %d / sheet %02X / palette %d",
            frame_index, oam_index, tile, slot, sheet, oam.palette),
        .palette = gfx::SnesPalette(colors),
    });
  }
  return context;
}

// The frame, assignments and colors are captured at navigation time; pixels
// always come from the owning session's sheet store, including unsaved strokes.
absl::StatusOr<usage_preview::IndexedImage> RenderCapturedFrame(
    const zsprite::Frame& frame, const std::array<uint8_t, 8>& slots,
    const gfx::PaletteGroup& palettes, const zelda3::GameData* owner) {
  if (!owner)
    return absl::FailedPreconditionError("Sprite ROM session ended");
  std::set<uint16_t> ids(slots.begin(), slots.end());
  const auto sheets = usage_preview::SnapshotSessionSheets(ids, owner);
  std::vector<uint8_t> buffer(0x10000, 0);
  for (size_t slot = 0; slot < slots.size(); ++slot) {
    const auto it = sheets.find(slots[slot]);
    if (it == sheets.end() || it->second.size() != 4096) {
      return absl::FailedPreconditionError(
          "Sprite sheet unavailable in source session");
    }
    std::copy(it->second.begin(), it->second.end(),
              buffer.begin() + slot * 4096);
  }
  constexpr int kSize = 256;
  gfx::Bitmap canvas(kSize, kSize, 8, std::vector<uint8_t>(kSize * kSize, 0));
  SpriteDrawer drawer(buffer.data());
  drawer.DrawFrame(canvas, frame, 128, 128);
  usage_preview::IndexedImage image;
  image.width = kSize;
  image.height = kSize;
  image.pixels = canvas.vector();
  image.index0_transparent = true;
  for (size_t row = 0; row < palettes.size() && row < 8; ++row) {
    const auto& palette = palettes.palette_ref(row);
    for (size_t index = 0; index < palette.size() && index < 16; ++index) {
      const auto rgb = palette[index].rgb();
      image.colors[row * 16 + index] = {static_cast<uint8_t>(rgb.x),
                                        static_cast<uint8_t>(rgb.y),
                                        static_cast<uint8_t>(rgb.z), 255};
    }
  }
  return image;
}

}  // namespace yaze::editor::sprite_graphics_context
