#include "inventory.h"

#include "app/gfx/backend/irenderer.h"
#include "app/core/window.h"
#include "app/gfx/core/bitmap.h"
#include "app/gfx/resource/arena.h"
#include "app/gfx/types/snes_tile.h"
#include "app/rom.h"
#include "app/snes.h"

namespace yaze {
namespace zelda3 {

absl::Status Inventory::Create(Rom* rom) {
  if (!rom || !rom->is_loaded()) {
    return absl::InvalidArgumentError("ROM is not loaded");
  }

  // Build the tileset first (loads 2BPP graphics)
  RETURN_IF_ERROR(BuildTileset(rom));

  // Load item icons from ROM
  RETURN_IF_ERROR(LoadItemIcons(rom));

  // TODO(scawful): For now, create a simple display bitmap
  // Future: Oracle of Secrets menu editor will handle full menu layout
  data_.reserve(256 * 256);
  for (int i = 0; i < 256 * 256; i++) {
    data_.push_back(0xFF);
  }

  bitmap_.Create(256, 256, 8, data_);
  bitmap_.SetPalette(palette_);

  // Queue texture creation via Arena's deferred system
  gfx::Arena::Get().QueueTextureCommand(
      gfx::Arena::TextureCommandType::CREATE, &bitmap_);

  return absl::OkStatus();
}

absl::Status Inventory::BuildTileset(Rom* rom) {
  tilesheets_.reserve(6 * 0x2000);
  for (int i = 0; i < 6 * 0x2000; i++) tilesheets_.push_back(0xFF);
  ASSIGN_OR_RETURN(tilesheets_, Load2BppGraphics(*rom));
  std::vector<uint8_t> test;
  for (int i = 0; i < 0x4000; i++) {
    test_.push_back(tilesheets_[i]);
  }
  for (int i = 0x8000; i < +0x8000 + 0x2000; i++) {
    test_.push_back(tilesheets_[i]);
  }
  tilesheets_bmp_.Create(128, 0x130, 64, test_);
  auto hud_pal_group = rom->palette_group().hud;
  palette_ = hud_pal_group[0];
  tilesheets_bmp_.SetPalette(palette_);

  // Queue texture creation via Arena's deferred system
  gfx::Arena::Get().QueueTextureCommand(
      gfx::Arena::TextureCommandType::CREATE, &tilesheets_bmp_);

  return absl::OkStatus();
}

absl::Status Inventory::LoadItemIcons(Rom* rom) {
  // Convert SNES address to PC address
  int pc_addr = SnesToPc(kItemIconsPtr);

  // Define icon categories and their ROM offsets (relative to kItemIconsPtr)
  // Based on bank_0D.asm ItemIcons structure
  struct IconDef {
    int offset;
    std::string name;
  };

  // Bow icons (.bows section)
  std::vector<IconDef> bow_icons = {
      {0x00, "No bow"},
      {0x08, "Empty bow"},
      {0x10, "Bow and arrows"},
      {0x18, "Empty silvers bow"},
      {0x20, "Silver bow and arrows"}
  };

  // Boomerang icons (.booms section)
  std::vector<IconDef> boom_icons = {
      {0x28, "No boomerang"},
      {0x30, "Blue boomerang"},
      {0x38, "Red boomerang"}
  };

  // Hookshot icons (.hook section)
  std::vector<IconDef> hook_icons = {
      {0x40, "No hookshot"},
      {0x48, "Hookshot"}
  };

  // Bomb icons (.bombs section)
  std::vector<IconDef> bomb_icons = {
      {0x50, "No bombs"},
      {0x58, "Bombs"}
  };

  // Load all icon categories
  auto load_icons = [&](const std::vector<IconDef>& icons) -> absl::Status {
    for (const auto& icon_def : icons) {
      ItemIcon icon;
      int icon_addr = pc_addr + icon_def.offset;

      ASSIGN_OR_RETURN(icon.tile_tl, rom->ReadWord(icon_addr));
      ASSIGN_OR_RETURN(icon.tile_tr, rom->ReadWord(icon_addr + 2));
      ASSIGN_OR_RETURN(icon.tile_bl, rom->ReadWord(icon_addr + 4));
      ASSIGN_OR_RETURN(icon.tile_br, rom->ReadWord(icon_addr + 6));
      icon.name = icon_def.name;

      item_icons_.push_back(icon);
    }
    return absl::OkStatus();
  };

  RETURN_IF_ERROR(load_icons(bow_icons));
  RETURN_IF_ERROR(load_icons(boom_icons));
  RETURN_IF_ERROR(load_icons(hook_icons));
  RETURN_IF_ERROR(load_icons(bomb_icons));

  // TODO(scawful): Load remaining icon categories:
  // - Mushroom/Powder (.shroom)
  // - Magic powder (.powder)
  // - Fire rod (.fires)
  // - Ice rod (.ices)
  // - Bombos medallion (.bombos)
  // - Ether medallion (.ether)
  // - Quake medallion (.quake)
  // - Lantern (.lamp)
  // - Hammer (.hammer)
  // - Flute (.flute)
  // - Bug net (.net)
  // - Book of Mudora (.book)
  // - Bottles (.bottles) - Multiple variants (empty, red potion, green potion, etc.)
  // - Cane of Somaria (.canes)
  // - Cane of Byrna (.byrn)
  // - Magic cape (.cape)
  // - Magic mirror (.mirror)
  // - Gloves (.glove)
  // - Boots (.boots)
  // - Flippers (.flippers)
  // - Moon pearl (.pearl)
  // - Swords (.swords)
  // - Shields (.shields)
  // - Armor (.armor)

  return absl::OkStatus();
}

}  // namespace zelda3
}  // namespace yaze
