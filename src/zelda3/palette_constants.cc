#include "zelda3/palette_constants.h"

#include <string>
#include <vector>

namespace yaze::zelda3 {

const PaletteGroupMetadata* GetPaletteGroupMetadata(const char* group_id) {
  std::string id_str(group_id);

  if (id_str == PaletteGroupName::kOverworldMain) {
    return &PaletteMetadata::kOverworldMain;
  }
  if (id_str == PaletteGroupName::kOverworldAux) {
    return &PaletteMetadata::kOverworldAux;
  }
  if (id_str == PaletteGroupName::kOverworldAnimated) {
    return &PaletteMetadata::kOverworldAnimated;
  }
  if (id_str == PaletteGroupName::kDungeonMain) {
    return &PaletteMetadata::kDungeonMain;
  }
  if (id_str == PaletteGroupName::kGlobalSprites) {
    return &PaletteMetadata::kGlobalSprites;
  }
  if (id_str == PaletteGroupName::kSpritesAux1) {
    return &PaletteMetadata::kSpritesAux1;
  }
  if (id_str == PaletteGroupName::kSpritesAux2) {
    return &PaletteMetadata::kSpritesAux2;
  }
  if (id_str == PaletteGroupName::kSpritesAux3) {
    return &PaletteMetadata::kSpritesAux3;
  }
  if (id_str == PaletteGroupName::kArmor) {
    return &PaletteMetadata::kArmor;
  }
  if (id_str == PaletteGroupName::kSwords) {
    return &PaletteMetadata::kSwords;
  }
  if (id_str == PaletteGroupName::kShields) {
    return &PaletteMetadata::kShields;
  }
  if (id_str == PaletteGroupName::kHud) {
    return &PaletteMetadata::kHud;
  }
  if (id_str == PaletteGroupName::kGrass) {
    return &PaletteMetadata::kGrass;
  }
  if (id_str == PaletteGroupName::k3DObject) {
    return &PaletteMetadata::k3DObject;
  }
  if (id_str == PaletteGroupName::kOverworldMiniMap) {
    return &PaletteMetadata::kOverworldMiniMap;
  }
  return nullptr;
}

std::vector<const PaletteGroupMetadata*> GetAllPaletteGroups() {
  return {// Overworld
          &PaletteMetadata::kOverworldMain, &PaletteMetadata::kOverworldAux,
          &PaletteMetadata::kOverworldAnimated,

          // Dungeon
          &PaletteMetadata::kDungeonMain,

          // Sprites
          &PaletteMetadata::kGlobalSprites, &PaletteMetadata::kSpritesAux1,
          &PaletteMetadata::kSpritesAux2, &PaletteMetadata::kSpritesAux3,

          // Equipment
          &PaletteMetadata::kArmor, &PaletteMetadata::kSwords,
          &PaletteMetadata::kShields,

          // Interface
          &PaletteMetadata::kHud, &PaletteMetadata::kOverworldMiniMap,

          // Special
          &PaletteMetadata::kGrass, &PaletteMetadata::k3DObject};
}

}  // namespace yaze::zelda3
