#include "inventory.h"

#include "app/gfx/backend/irenderer.h"
#include "app/gfx/core/bitmap.h"
#include "app/gfx/resource/arena.h"
#include "app/gfx/types/snes_tile.h"
#include "app/platform/window.h"
#include "rom/rom.h"
#include "rom/snes.h"

namespace yaze {
namespace zelda3 {

absl::Status Inventory::Create(Rom* rom, GameData* game_data) {
  if (!rom || !rom->is_loaded()) {
    return absl::InvalidArgumentError("ROM is not loaded");
  }
  game_data_ = game_data;

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
  gfx::Arena::Get().QueueTextureCommand(gfx::Arena::TextureCommandType::CREATE,
                                        &bitmap_);

  return absl::OkStatus();
}

absl::Status Inventory::BuildTileset(Rom* rom) {
  tilesheets_.reserve(6 * 0x2000);
  for (int i = 0; i < 6 * 0x2000; i++)
    tilesheets_.push_back(0xFF);
  ASSIGN_OR_RETURN(tilesheets_, Load2BppGraphics(*rom));
  std::vector<uint8_t> test;
  for (int i = 0; i < 0x4000; i++) {
    test_.push_back(tilesheets_[i]);
  }
  for (int i = 0x8000; i < +0x8000 + 0x2000; i++) {
    test_.push_back(tilesheets_[i]);
  }
  tilesheets_bmp_.Create(128, 0x130, 64, test_);
  if (!game_data_) {
    return absl::FailedPreconditionError("GameData not set for Inventory");
  }
  auto& hud_pal_group = game_data_->palette_groups.hud;
  palette_ = hud_pal_group[0];
  tilesheets_bmp_.SetPalette(palette_);

  // Queue texture creation via Arena's deferred system
  gfx::Arena::Get().QueueTextureCommand(gfx::Arena::TextureCommandType::CREATE,
                                        &tilesheets_bmp_);

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
  std::vector<IconDef> bow_icons = {{0x00, "No bow"},
                                    {0x08, "Empty bow"},
                                    {0x10, "Bow and arrows"},
                                    {0x18, "Empty silvers bow"},
                                    {0x20, "Silver bow and arrows"}};

  // Boomerang icons (.booms section)
  std::vector<IconDef> boom_icons = {{0x28, "No boomerang"},
                                     {0x30, "Blue boomerang"},
                                     {0x38, "Red boomerang"}};

  // Hookshot icons (.hook section)
  std::vector<IconDef> hook_icons = {{0x40, "No hookshot"}, {0x48, "Hookshot"}};

  // Bomb icons (.bombs section)
  std::vector<IconDef> bomb_icons = {{0x50, "No bombs"}, {0x58, "Bombs"}};

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

  // Mushroom/Powder (.shroom)
  std::vector<IconDef> shroom_icons = {{0x60, "No mushroom"},
                                       {0x68, "Mushroom"}};
  RETURN_IF_ERROR(load_icons(shroom_icons));

  // Magic powder (.powder)
  std::vector<IconDef> powder_icons = {{0x70, "No powder"}, {0x78, "Powder"}};
  RETURN_IF_ERROR(load_icons(powder_icons));

  // Fire rod (.fires)
  std::vector<IconDef> fire_rod_icons = {{0x80, "No fire rod"},
                                         {0x88, "Fire rod"}};
  RETURN_IF_ERROR(load_icons(fire_rod_icons));

  // Ice rod (.ices)
  std::vector<IconDef> ice_rod_icons = {{0x90, "No ice rod"}, {0x98, "Ice rod"}};
  RETURN_IF_ERROR(load_icons(ice_rod_icons));

  // Medallions
  std::vector<IconDef> medal_icons = {
      {0xA0, "No Bombos"}, {0xA8, "Bombos"}, {0xB0, "No Ether"},
      {0xB8, "Ether"},     {0xC0, "No Quake"},  {0xC8, "Quake"}};
  RETURN_IF_ERROR(load_icons(medal_icons));

  // Utilities
  std::vector<IconDef> util_icons = {
      {0xD0, "No lantern"}, {0xD8, "Lantern"},    {0xE0, "No hammer"},
      {0xE8, "Hammer"},     {0xF0, "No flute"},     {0xF8, "Flute"},
      {0x100, "No net"},    {0x108, "Bug net"},   {0x110, "No book"},
      {0x118, "Book"}};
  RETURN_IF_ERROR(load_icons(util_icons));

  // Bottles
  std::vector<IconDef> bottle_icons = {
      {0x120, "No bottle"},        {0x128, "Empty bottle"},
      {0x130, "Red potion"},       {0x138, "Green potion"},
      {0x140, "Blue potion"},      {0x148, "Fairy"},
      {0x150, "Bee"},              {0x158, "Good bee"}};
  RETURN_IF_ERROR(load_icons(bottle_icons));

  // Canes and Mirror
  std::vector<IconDef> magic_icons = {
      {0x160, "No Somaria"}, {0x168, "Cane of Somaria"}, {0x170, "No Byrna"},
      {0x178, "Cane of Byrna"}, {0x180, "No Cape"},     {0x188, "Magic cape"},
      {0x190, "No mirror"},  {0x198, "Magic mirror"}};
  RETURN_IF_ERROR(load_icons(magic_icons));

  // Passive equipment
  std::vector<IconDef> equip_icons = {
      {0x1A0, "No gloves"},  {0x1A8, "Power glove"}, {0x1B0, "Titan mitts"},
      {0x1B8, "No boots"},   {0x1C0, "Pegasus boots"}, {0x1C8, "No flippers"},
      {0x1D0, "Flippers"},   {0x1D8, "No pearl"},    {0x1E0, "Moon pearl"}};
  RETURN_IF_ERROR(load_icons(equip_icons));

  // Combat
  std::vector<IconDef> combat_icons = {
      {0x1E8, "No sword"},   {0x1F0, "Fighter sword"}, {0x1F8, "Master sword"},
      {0x200, "Tempered sword"}, {0x208, "Golden sword"}, {0x210, "No shield"},
      {0x218, "Blue shield"}, {0x220, "Red shield"}, {0x228, "Mirror shield"},
      {0x230, "Green mail"}, {0x238, "Blue mail"}, {0x240, "Red mail"}};
  RETURN_IF_ERROR(load_icons(combat_icons));

  return absl::OkStatus();
}

}  // namespace zelda3
}  // namespace yaze
