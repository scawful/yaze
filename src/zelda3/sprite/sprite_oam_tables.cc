#include "zelda3/sprite/sprite_oam_tables.h"

#include <unordered_map>

namespace yaze {
namespace zelda3 {

namespace {

// Static registry of all sprite layouts
const std::unordered_map<uint8_t, const SpriteOamLayout*>& GetLayoutMap() {
  static const std::unordered_map<uint8_t, const SpriteOamLayout*> layouts = {
      {0x01, &kVultureLayout},
      {0x08, &kOctorokLayout},
      {0x0B, &kChickenLayout},
      {0x0E, &kGreenSoldierLayout},
      {0x0F, &kBlueSoldierLayout},
      {0x10, &kRedSoldierLayout},
      {0x29, &kBlueGuardLayout},
      {0x41, &kGreenPatrolLayout},
      {0x44, &kArmosKnightLayout},
      {0x4B, &kOctoballoonLayout},
      {0x53, &kRedEyegoreLayout},
      {0x54, &kGreenEyegoreLayout},
      {0x64, &kMoblinLayout},
      {0x81, &kHinoxLayout},
      {0x88, &kUncleLayout},
      {0xD8, &kHeartContainerLayout},
      {0xDA, &kGreenRupeeLayout},
      {0xDB, &kBlueRupeeLayout},
      {0xDC, &kRedRupeeLayout},
      {0xDE, &kSmallHeartLayout},
      {0xDF, &kKeyLayout},
      {0xE1, &kSmallMagicLayout},
      {0xE2, &kLargeMagicLayout},
  };
  return layouts;
}

}  // namespace

const SpriteOamLayout* SpriteOamRegistry::GetLayout(uint8_t sprite_id) {
  const auto& layouts = GetLayoutMap();
  auto it = layouts.find(sprite_id);
  if (it != layouts.end()) {
    return it->second;
  }
  return nullptr;
}

std::optional<std::array<uint8_t, 4>> SpriteOamRegistry::GetRequiredSheets(
    uint8_t sprite_id) {
  const auto* layout = GetLayout(sprite_id);
  if (layout) {
    return layout->required_sheets;
  }
  return std::nullopt;
}

bool SpriteOamRegistry::HasLayout(uint8_t sprite_id) {
  const auto& layouts = GetLayoutMap();
  return layouts.find(sprite_id) != layouts.end();
}

std::vector<const SpriteOamLayout*> SpriteOamRegistry::GetAllLayouts() {
  std::vector<const SpriteOamLayout*> result;
  const auto& layouts = GetLayoutMap();
  result.reserve(layouts.size());
  for (const auto& [id, layout] : layouts) {
    result.push_back(layout);
  }
  return result;
}

}  // namespace zelda3
}  // namespace yaze
