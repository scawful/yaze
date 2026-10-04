#include "app/editor/graphics/graphics_palette_preview.h"

#include <algorithm>
#include <vector>

#include "app/editor/graphics/palette_controls_panel_internal.h"
#include "app/gfx/resource/arena.h"
#include "app/platform/sdl_compat.h"
#include "zelda3/game_data.h"

namespace yaze::editor {

absl::StatusOr<GraphicsPalettePreview> ResolveGraphicsPreviewPalette(
    zelda3::GameData& data, const GraphicsPaletteSelection& selection) {
  auto* group = data.palette_groups.get_group(selection.group);
  if (!group || selection.palette < 0 || selection.palette >= group->size()) {
    return absl::InvalidArgumentError("Preview palette is unavailable");
  }
  const auto& palette = group->palette_ref(selection.palette);
  const auto layout =
      internal::GetPaletteRowLayout(selection.group, palette.size());
  const int rows =
      internal::GetPaletteRowCount(palette.size(), layout.colors_per_row);
  if (selection.row < 0 || selection.row >= rows) {
    return absl::InvalidArgumentError("Preview palette row is unavailable");
  }
  const size_t offset = selection.row * layout.colors_per_row +
                        (layout.has_explicit_transparent ? 1 : 0);
  const size_t length = std::min<size_t>(
      7, std::min<size_t>(
             layout.colors_per_row - (layout.has_explicit_transparent ? 1 : 0),
             palette.size() - offset));
  std::vector<gfx::SnesColor> colors(8, gfx::SnesColor(uint16_t{0}));
  colors[0].set_transparent(true);
  for (size_t i = 0; i < length; ++i) {
    colors[i + 1] = palette[offset + i];
    colors[i + 1].set_transparent(false);
  }
  return GraphicsPalettePreview{gfx::SnesPalette(colors),
                                static_cast<int>(group->size()), rows};
}

absl::Status SetGraphicsPreviewPalette(
    GraphicsEditorState& state, uint16_t sheet,
    std::optional<GraphicsPaletteSelection> selection) {
  auto& arena = gfx::Arena::Get();
  if (!state.game_data || arena.gfx_sheets_owner() != state.game_data ||
      sheet >= arena.gfx_sheets().size()) {
    return absl::FailedPreconditionError("This ROM's graphics are not active");
  }
  auto& bitmap = arena.mutable_gfx_sheets()->at(sheet);
  const auto* current = platform::GetSurfacePalette(bitmap.surface());
  if (!bitmap.is_active() || !current) {
    return absl::FailedPreconditionError("This sheet has no display palette");
  }

  std::vector<SDL_Color> colors;
  if (selection) {
    const auto resolved =
        ResolveGraphicsPreviewPalette(*state.game_data, *selection);
    if (!resolved.ok()) {
      return resolved.status();
    }
    for (size_t i = 0; i < resolved->colors.size(); ++i) {
      const auto rgb = resolved->colors[i].rgb();
      colors.push_back({static_cast<Uint8>(rgb.x), static_cast<Uint8>(rgb.y),
                        static_cast<Uint8>(rgb.z),
                        static_cast<Uint8>(i == 0 ? 0 : 255)});
    }
  } else {
    const auto* loaded = platform::GetSurfacePalette(
        state.game_data->gfx_bitmaps[sheet].surface());
    if (!loaded) {
      return absl::FailedPreconditionError("Loaded colors are unavailable");
    }
    colors.assign(loaded->colors,
                  loaded->colors + std::min(8, loaded->ncolors));
  }
  bool changed = current->ncolors < static_cast<int>(colors.size());
  for (size_t i = 0; !changed && i < colors.size(); ++i) {
    const auto& a = colors[i];
    const auto& b = current->colors[i];
    changed = a.r != b.r || a.g != b.g || a.b != b.b || a.a != b.a;
  }
  if (changed) {
    bitmap.SetPalette(colors);
    arena.NotifySheetModified(sheet);
  }
  if (selection) {
    state.preview_palettes[sheet] = std::move(*selection);
  } else {
    state.preview_palettes.erase(sheet);
  }
  return absl::OkStatus();
}

void RefreshGraphicsPreviewPalettes(GraphicsEditorState& state) {
  for (const auto& [sheet, selection] : state.preview_palettes) {
    SetGraphicsPreviewPalette(state, sheet, selection).IgnoreError();
  }
}

}  // namespace yaze::editor
