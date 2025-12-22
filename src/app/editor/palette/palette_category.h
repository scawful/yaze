#ifndef YAZE_APP_EDITOR_PALETTE_PALETTE_CATEGORY_H
#define YAZE_APP_EDITOR_PALETTE_PALETTE_CATEGORY_H

#include <string>
#include <unordered_map>
#include <vector>

#include "app/gui/core/icons.h"

namespace yaze {
namespace editor {

/**
 * @brief Categories for organizing palette groups in the UI
 */
enum class PaletteCategory {
  kOverworld,
  kDungeon,
  kSprites,
  kEquipment,
  kMiscellaneous
};

/**
 * @brief Information about a palette category for UI rendering
 */
struct PaletteCategoryInfo {
  PaletteCategory category;
  std::string display_name;
  std::string icon;
  std::vector<std::string> group_names;
};

/**
 * @brief Get all palette categories with their associated groups
 * @return Vector of category info structures
 */
inline const std::vector<PaletteCategoryInfo>& GetPaletteCategories() {
  static const std::vector<PaletteCategoryInfo> categories = {
      {PaletteCategory::kOverworld,
       "Overworld",
       ICON_MD_LANDSCAPE,
       {"ow_main", "ow_aux", "ow_animated", "ow_mini_map", "grass"}},
      {PaletteCategory::kDungeon,
       "Dungeon",
       ICON_MD_CASTLE,
       {"dungeon_main"}},
      {PaletteCategory::kSprites,
       "Sprites",
       ICON_MD_PETS,
       {"global_sprites", "sprites_aux1", "sprites_aux2", "sprites_aux3"}},
      {PaletteCategory::kEquipment,
       "Equipment",
       ICON_MD_SHIELD,
       {"armors", "swords", "shields"}},
      {PaletteCategory::kMiscellaneous,
       "Miscellaneous",
       ICON_MD_MORE_HORIZ,
       {"hud", "3d_object"}}};
  return categories;
}

/**
 * @brief Get display name for a palette group
 * @param group_name Internal group name (e.g., "ow_main")
 * @return Human-readable display name
 */
inline std::string GetGroupDisplayName(const std::string& group_name) {
  static const std::unordered_map<std::string, std::string> names = {
      {"ow_main", "Overworld Main"},
      {"ow_aux", "Overworld Auxiliary"},
      {"ow_animated", "Overworld Animated"},
      {"ow_mini_map", "Mini Map"},
      {"grass", "Grass"},
      {"dungeon_main", "Dungeon Main"},
      {"global_sprites", "Global Sprites"},
      {"sprites_aux1", "Sprites Aux 1"},
      {"sprites_aux2", "Sprites Aux 2"},
      {"sprites_aux3", "Sprites Aux 3"},
      {"armors", "Armor/Tunic"},
      {"swords", "Swords"},
      {"shields", "Shields"},
      {"hud", "HUD"},
      {"3d_object", "3D Objects"}};
  auto it = names.find(group_name);
  return it != names.end() ? it->second : group_name;
}

/**
 * @brief Get the category that a palette group belongs to
 * @param group_name Internal group name
 * @return Category enum value
 */
inline PaletteCategory GetGroupCategory(const std::string& group_name) {
  for (const auto& cat : GetPaletteCategories()) {
    for (const auto& name : cat.group_names) {
      if (name == group_name) {
        return cat.category;
      }
    }
  }
  return PaletteCategory::kMiscellaneous;
}

}  // namespace editor
}  // namespace yaze

#endif  // YAZE_APP_EDITOR_PALETTE_PALETTE_CATEGORY_H
