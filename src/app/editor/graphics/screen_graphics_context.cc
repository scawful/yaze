#include "app/editor/graphics/screen_graphics_context.h"

#include <algorithm>
#include <cstdint>
#include <vector>

#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "absl/strings/str_format.h"
#include "app/editor/graphics/graphics_edit_context.h"
#include "zelda3/game_data.h"
#include "zelda3/gfx_sheet_storage.h"

namespace yaze::editor {

// The Screen editor's ROM-backed Dungeon Map / Tiles Gfx picker is four
// 128x32 sheets stacked into a 128x128, 8x8-tile atlas. Its displayed raw
// indices use the first eight colors of dungeon_main[3]. This is deliberately
// separate from the map's Tile16 composition and its per-tile palette words.
absl::StatusOr<usage_preview::IndexedImage> RenderDungeonMapTilesGfxPreview(
    const zelda3::GameData* owner) {
  if (owner == nullptr || owner->palette_groups.dungeon_main.size() <= 3) {
    return absl::FailedPreconditionError(
        "Dungeon Map / Tiles Gfx palette is unavailable");
  }
  const auto& palette = owner->palette_groups.dungeon_main[3];
  if (palette.size() < 8) {
    return absl::FailedPreconditionError(
        "Dungeon Map / Tiles Gfx needs eight preview colors");
  }

  usage_preview::IndexedImage image;
  image.width = 128;
  image.height = 128;
  image.pixels.resize(128 * 128);
  for (int slot = 0; slot < 4; ++slot) {
    const auto sheet = owner->sheet_store.Sheet(212 + slot);
    if (sheet.size() != 128 * 32) {
      return absl::FailedPreconditionError(
          absl::StrFormat("Dungeon Map sheet %d is not loaded", 212 + slot));
    }
    std::copy(sheet.begin(), sheet.end(),
              image.pixels.begin() + slot * 128 * 32);
  }
  for (int color = 0; color < 8; ++color) {
    const auto rgb = palette[color].rgb();
    image.colors[color] =
        SDL_Color{static_cast<uint8_t>(rgb.x), static_cast<uint8_t>(rgb.y),
                  static_cast<uint8_t>(rgb.z), 255};
  }
  return image;
}

absl::StatusOr<GraphicsEditContext> MakeDungeonMapTilesGfxEditContext(
    const zelda3::GameData* owner, int selected_tile8, bool binary_gfx_loaded) {
  if (binary_gfx_loaded) {
    return absl::FailedPreconditionError(
        "BIN graphics are not ROM sheets; reopen Screen assets to edit ROM "
        "Dungeon Map tiles");
  }
  if (selected_tile8 < 0 || selected_tile8 >= 256) {
    return absl::InvalidArgumentError(
        "Select a Dungeon Map Tiles Gfx tile from 0 to 255");
  }
  const auto preview = RenderDungeonMapTilesGfxPreview(owner);
  if (!preview.ok()) {
    return preview.status();
  }
  const uint16_t sheet_id = static_cast<uint16_t>(212 + selected_tile8 / 64);
  if (zelda3::GetGfxSheetStorageKind(sheet_id) ==
      zelda3::GfxSheetStorageKind::kCompressed2bpp) {
    return absl::FailedPreconditionError(
        "Selected Dungeon Map sheet is not 3bpp");
  }
  const auto& palette = owner->palette_groups.dungeon_main[3];
  std::vector<gfx::SnesColor> colors;
  colors.reserve(8);
  for (int index = 0; index < 8; ++index) {
    colors.push_back(palette[index]);
  }

  GraphicsEditContext context;
  context.owner = owner;
  context.label = "Dungeon Map / Tiles Gfx / raw palette row 0";
  context.limitation =
      "This previews raw 3bpp tile indices with dungeon_main[3] colors 0-7; "
      "Tile16 composition and map palette words are separate.";
  context.targets.push_back({
      .sheet_id = sheet_id,
      .tile_index = static_cast<uint16_t>(selected_tile8 % 64),
      .label = absl::StrFormat("Dungeon Map Tiles Gfx tile %d / sheet %d / "
                               "local tile %d / raw row 0",
                               selected_tile8, sheet_id, selected_tile8 % 64),
      .palette = gfx::SnesPalette(colors),
  });
  context.resolved.surface = "dungeon_map_tiles_gfx";
  context.resolved.id = selected_tile8;
  context.resolved.graphics_loaded = true;
  context.resolved.loader_agrees = true;
  context.resolved.palette_colors_resolved = true;
  for (int slot = 0; slot < 4; ++slot) {
    context.resolved.slots.push_back({
        .slot = slot,
        .role = "dungeon_map_tiles_gfx",
        .palette_group = "dungeon_main",
        .sheet_id = 212 + slot,
        .source = "fixed_rom_sheet",
        .loader_agrees = true,
        .store_revision = owner->sheet_store.Revision(212 + slot),
        .width = 128,
        .height = 32,
    });
  }
  context.resolved.palettes.push_back(
      {"raw_preview", "dungeon_main", 3, false});
  context.render_preview = [owner] {
    return RenderDungeonMapTilesGfxPreview(owner);
  };
  return context;
}

}  // namespace yaze::editor
