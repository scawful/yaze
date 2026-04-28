#ifndef YAZE_APP_EDITOR_GRAPHICS_PALETTE_CONTROLS_PANEL_INTERNAL_H_
#define YAZE_APP_EDITOR_GRAPHICS_PALETTE_CONTROLS_PANEL_INTERNAL_H_

#include <cstddef>
#include <string_view>

namespace yaze {
namespace editor {
namespace internal {

// Row-layout for a palette group's display in the panel.
//
//   colors_per_row              - cells across (e.g. 16 for HUD, 7 for OW main).
//   has_explicit_transparent    - true if column 0 of the row is the SNES
//                                 transparent slot (rendered, but skipped when
//                                 building a sub-palette slice for sheets).
struct PaletteRowLayout {
  int colors_per_row;
  bool has_explicit_transparent;
};

// Determine the panel's row-layout for a named palette group.
//
// NOTE on a deferred unification:
//
// A previous plan (review-all-usages-of-vast-thunder slice 8 / B1) proposed
// migrating this function onto `gfx::DefaultBindingFor(SheetRole)` from
// `app/gfx/types/sheet_role_palette_table.h`, on the theory that the binding
// table is the canonical place for palette-group metadata. Investigation
// surfaced two structural blockers that make a literal migration lossy:
//
//   1. `SheetRolePaletteBinding` carries `{palette_group_name,
//      default_sub_index, cgram_base_row}`. It does NOT carry
//      `colors_per_row` or `has_explicit_transparent`. The row-layout
//      data has no representation in the binding struct today.
//
//   2. `SheetRole` covers 8 roles mapping to 6 distinct group names
//      (kOverworldMain/Aux1 share "ow_aux", kOverworldGfx maps to "ow_main"
//      not "ow_main" as the slot-0 rendering route, etc). The 13 group names
//      below cover seven groups not modeled by SheetRole at all:
//      "swords", "shields", "grass", "3d_object", "global_sprites",
//      "armors", "ow_mini_map" (plus "sprites_aux2"/"sprites_aux3" which
//      collapse onto kSpriteAux1 in the role taxonomy).
//
// A real unification would need to (a) extend `SheetRolePaletteBinding` with
// row-layout fields, (b) add a reverse lookup `BindingForGroupName(string_view)`
// keyed on the full set of palette group names, and (c) decide whether
// non-SheetRole groups warrant new role variants or a separate
// `KnownPaletteGroup` enum. That is a multi-file refactor with risk of
// silently flipping cgram routing for callers of the existing binding, so it
// stays out of slice 8.
//
// This header keeps the function inline so a regression test can pin the
// truth table directly without depending on the panel UI; any future
// migration is expected to use that test as a safety net.
inline PaletteRowLayout GetPaletteRowLayout(std::string_view group_name,
                                            std::size_t palette_size) {
  if (group_name == "ow_main" || group_name == "ow_aux" ||
      group_name == "ow_animated" || group_name == "sprites_aux1" ||
      group_name == "sprites_aux2" || group_name == "sprites_aux3") {
    return {7, false};
  }
  if (group_name == "global_sprites" || group_name == "armors" ||
      group_name == "dungeon_main") {
    return {15, false};
  }
  if (group_name == "hud" || group_name == "ow_mini_map") {
    return {16, true};
  }
  if (group_name == "swords") {
    return {3, false};
  }
  if (group_name == "shields") {
    return {4, false};
  }
  if (group_name == "grass") {
    return {3, false};
  }
  if (group_name == "3d_object") {
    return {8, false};
  }

  if (palette_size % 16 == 0) {
    return {16, true};
  }
  if (palette_size % 15 == 0) {
    return {15, false};
  }
  if (palette_size % 7 == 0) {
    return {7, false};
  }

  int fallback = palette_size > 0 ? static_cast<int>(palette_size) : 1;
  return {fallback, false};
}

inline int GetPaletteRowCount(std::size_t palette_size, int colors_per_row) {
  if (colors_per_row <= 0) {
    return 1;
  }
  return static_cast<int>((palette_size + colors_per_row - 1) / colors_per_row);
}

}  // namespace internal
}  // namespace editor
}  // namespace yaze

#endif  // YAZE_APP_EDITOR_GRAPHICS_PALETTE_CONTROLS_PANEL_INTERNAL_H_
