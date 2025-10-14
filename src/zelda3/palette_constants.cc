#include "zelda3/palette_constants.h"
#include <string>
#include <vector>

namespace yaze {
namespace zelda3 {

const PaletteGroupMetadata* GetPaletteGroupMetadata(const char* group_id) {
  std::string id_str(group_id);
  
  if (id_str == PaletteGroupName::kOverworldMain) {
    return &PaletteMetadata::kOverworldMain;
  } else if (id_str == PaletteGroupName::kOverworldAux) {
    return &PaletteMetadata::kOverworldAux;
  } else if (id_str == PaletteGroupName::kOverworldAnimated) {
    return &PaletteMetadata::kOverworldAnimated;
  } else if (id_str == PaletteGroupName::kDungeonMain) {
    return &PaletteMetadata::kDungeonMain;
  } else if (id_str == PaletteGroupName::kGlobalSprites) {
    return &PaletteMetadata::kGlobalSprites;
  } else if (id_str == PaletteGroupName::kSpritesAux1) {
    return &PaletteMetadata::kSpritesAux1;
  } else if (id_str == PaletteGroupName::kSpritesAux2) {
    return &PaletteMetadata::kSpritesAux2;
  } else if (id_str == PaletteGroupName::kSpritesAux3) {
    return &PaletteMetadata::kSpritesAux3;
  } else if (id_str == PaletteGroupName::kArmor) {
    return &PaletteMetadata::kArmor;
  } else if (id_str == PaletteGroupName::kSwords) {
    return &PaletteMetadata::kSwords;
  } else if (id_str == PaletteGroupName::kShields) {
    return &PaletteMetadata::kShields;
  } else if (id_str == PaletteGroupName::kHud) {
    return &PaletteMetadata::kHud;
  } else if (id_str == PaletteGroupName::kGrass) {
    return &PaletteMetadata::kGrass;
  } else if (id_str == PaletteGroupName::k3DObject) {
    return &PaletteMetadata::k3DObject;
  } else if (id_str == PaletteGroupName::kOverworldMiniMap) {
    return &PaletteMetadata::kOverworldMiniMap;
  }
  return nullptr;
}

std::vector<const PaletteGroupMetadata*> GetAllPaletteGroups() {
  return {
    // Overworld
    &PaletteMetadata::kOverworldMain,
    &PaletteMetadata::kOverworldAux,
    &PaletteMetadata::kOverworldAnimated,
    
    // Dungeon
    &PaletteMetadata::kDungeonMain,
    
    // Sprites
    &PaletteMetadata::kGlobalSprites,
    &PaletteMetadata::kSpritesAux1,
    &PaletteMetadata::kSpritesAux2,
    &PaletteMetadata::kSpritesAux3,
    
    // Equipment
    &PaletteMetadata::kArmor,
    &PaletteMetadata::kSwords,
    &PaletteMetadata::kShields,
    
    // Interface
    &PaletteMetadata::kHud,
    &PaletteMetadata::kOverworldMiniMap,
    
    // Special
    &PaletteMetadata::kGrass,
    &PaletteMetadata::k3DObject
  };
}

}  // namespace zelda3
}  // namespace yaze

