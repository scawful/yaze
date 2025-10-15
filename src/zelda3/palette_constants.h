#ifndef YAZE_ZELDA3_PALETTE_CONSTANTS_H
#define YAZE_ZELDA3_PALETTE_CONSTANTS_H

#include <cstdint>
#include <vector>

namespace yaze::zelda3 {

// ============================================================================
// Palette Group Names
// ============================================================================
// These constants ensure consistent naming across the entire program

namespace PaletteGroupName {
constexpr const char* kOverworldMain = "ow_main";
constexpr const char* kOverworldAux = "ow_aux";
constexpr const char* kOverworldAnimated = "ow_animated";
constexpr const char* kHud = "hud";
constexpr const char* kGlobalSprites = "global_sprites";
constexpr const char* kArmor = "armor";
constexpr const char* kSwords = "swords";
constexpr const char* kShields = "shields";
constexpr const char* kSpritesAux1 = "sprites_aux1";
constexpr const char* kSpritesAux2 = "sprites_aux2";
constexpr const char* kSpritesAux3 = "sprites_aux3";
constexpr const char* kDungeonMain = "dungeon_main";
constexpr const char* kGrass = "grass";
constexpr const char* k3DObject = "3d_object";
constexpr const char* kOverworldMiniMap = "ow_mini_map";
}  // namespace PaletteGroupName

// ============================================================================
// ROM Addresses
// ============================================================================

namespace PaletteAddress {
constexpr uint32_t kOverworldMain = 0xDE6C8;
constexpr uint32_t kOverworldAux = 0xDE86C;
constexpr uint32_t kOverworldAnimated = 0xDE604;
constexpr uint32_t kGlobalSpritesLW = 0xDD218;
constexpr uint32_t kGlobalSpritesDW = 0xDD290;
constexpr uint32_t kArmor = 0xDD308;
constexpr uint32_t kSpritesAux1 = 0xDD39E;
constexpr uint32_t kSpritesAux2 = 0xDD446;
constexpr uint32_t kSpritesAux3 = 0xDD4E0;
constexpr uint32_t kSwords = 0xDD630;
constexpr uint32_t kShields = 0xDD648;
constexpr uint32_t kHud = 0xDD660;
constexpr uint32_t kDungeonMap = 0xDD70A;
constexpr uint32_t kDungeonMain = 0xDD734;
constexpr uint32_t kDungeonMapBg = 0xDE544;
constexpr uint32_t kGrassLW = 0x5FEA9;
constexpr uint32_t kGrassDW = 0x05FEB3;
constexpr uint32_t kGrassSpecial = 0x75640;
constexpr uint32_t kOverworldMiniMap = 0x55B27;
constexpr uint32_t kTriforce = 0x64425;
constexpr uint32_t kCrystal = 0xF4CD3;
}  // namespace PaletteAddress

// ============================================================================
// Palette Counts
// ============================================================================

namespace PaletteCount {
constexpr int kHud = 2;
constexpr int kOverworldMain = 60;  // 20 LW, 20 DW, 20 Special
constexpr int kOverworldAux = 20;
constexpr int kOverworldAnimated = 14;
constexpr int kGlobalSprites = 6;
constexpr int kArmor = 5;
constexpr int kSwords = 4;
constexpr int kSpritesAux1 = 12;
constexpr int kSpritesAux2 = 11;
constexpr int kSpritesAux3 = 24;
constexpr int kShields = 3;
constexpr int kDungeonMain = 20;
constexpr int kGrass = 3;
constexpr int k3DObject = 2;
constexpr int kOverworldMiniMap = 2;
}  // namespace PaletteCount

// ============================================================================
// Palette Metadata
// ============================================================================

struct PaletteGroupMetadata {
  const char* group_id;          // Unique identifier (e.g., "ow_main")
  const char* display_name;      // Human-readable name
  const char* category;          // Category (e.g., "Overworld", "Dungeon")
  uint32_t base_address;         // ROM address
  int palette_count;             // Number of palettes
  int colors_per_palette;        // Colors in each palette
  int colors_per_row;            // How many colors per row in UI
  int bits_per_pixel;            // Color depth (typically 4 for SNES)
  const char* description;       // Usage description
  bool has_animations;           // Whether palettes animate
};

// Predefined metadata for all palette groups
namespace PaletteMetadata {

constexpr PaletteGroupMetadata kOverworldMain = {
  .group_id = PaletteGroupName::kOverworldMain,
  .display_name = "Overworld Main",
  .category = "Overworld",
  .base_address = PaletteAddress::kOverworldMain,
  .palette_count = PaletteCount::kOverworldMain,
  .colors_per_palette = 35,  // 35 colors: 2 full rows (0-15, 16-31) + 3 colors (32-34)
  .colors_per_row = 7,       // Display in 16-color rows for proper SNES alignment
  .bits_per_pixel = 4,
  .description = "Main overworld palettes: 35 colors per set (2 full rows + 3 colors)",
  .has_animations = false
};

constexpr PaletteGroupMetadata kOverworldAnimated = {
  .group_id = PaletteGroupName::kOverworldAnimated,
  .display_name = "Overworld Animated",
  .category = "Overworld",
  .base_address = PaletteAddress::kOverworldAnimated,
  .palette_count = PaletteCount::kOverworldAnimated,
  .colors_per_palette = 7,   // 7 colors (overlay palette, no transparent)
  .colors_per_row = 8,        // Display in 8-color groups
  .bits_per_pixel = 4,
  .description = "Animated overlay palettes: 7 colors per set (water, lava, etc.)",
  .has_animations = true
};

constexpr PaletteGroupMetadata kDungeonMain = {
  .group_id = PaletteGroupName::kDungeonMain,
  .display_name = "Dungeon Main",
  .category = "Dungeon",
  .base_address = PaletteAddress::kDungeonMain,
  .palette_count = PaletteCount::kDungeonMain,
  .colors_per_palette = 90,  // 90 colors: 5 full rows (0-15, 16-31, 32-47, 48-63, 64-79) + 10 colors (80-89)
  .colors_per_row = 16,       // Display in 16-color rows for proper SNES alignment
  .bits_per_pixel = 4,
  .description = "Dungeon-specific palettes: 90 colors per set (5 full rows + 10 colors)",
  .has_animations = false
};

constexpr PaletteGroupMetadata kGlobalSprites = {
  .group_id = PaletteGroupName::kGlobalSprites,
  .display_name = "Global Sprites",
  .category = "Sprites",
  .base_address = PaletteAddress::kGlobalSpritesLW,
  .palette_count = 2,         // 2 sets (LW and DW), each with 60 colors
  .colors_per_palette = 60,   // 60 colors: 4 rows (0-15, 16-31, 32-47, 48-59) with transparent at 0, 16, 32, 48
  .colors_per_row = 16,       // Display in 16-color rows for proper SNES alignment
  .bits_per_pixel = 4,
  .description = "Global sprite palettes: 60 colors per set (4 sprite sub-palettes of 15+transparent each)",
  .has_animations = false
};

constexpr PaletteGroupMetadata kSpritesAux1 = {
  .group_id = PaletteGroupName::kSpritesAux1,
  .display_name = "Sprites Aux 1",
  .category = "Sprites",
  .base_address = PaletteAddress::kSpritesAux1,
  .palette_count = PaletteCount::kSpritesAux1,
  .colors_per_palette = 7,   // 7 colors (ROM stores 7, transparent added in memory)
  .colors_per_row = 8,        // Display as 8-color sub-palettes (with transparent)
  .bits_per_pixel = 4,
  .description = "Auxiliary sprite palettes 1: 7 colors per palette (transparent added at runtime)",
  .has_animations = false
};

constexpr PaletteGroupMetadata kSpritesAux2 = {
  .group_id = PaletteGroupName::kSpritesAux2,
  .display_name = "Sprites Aux 2",
  .category = "Sprites",
  .base_address = PaletteAddress::kSpritesAux2,
  .palette_count = PaletteCount::kSpritesAux2,
  .colors_per_palette = 7,   // 7 colors (ROM stores 7, transparent added in memory)
  .colors_per_row = 8,        // Display as 8-color sub-palettes (with transparent)
  .bits_per_pixel = 4,
  .description = "Auxiliary sprite palettes 2: 7 colors per palette (transparent added at runtime)",
  .has_animations = false
};

constexpr PaletteGroupMetadata kSpritesAux3 = {
  .group_id = PaletteGroupName::kSpritesAux3,
  .display_name = "Sprites Aux 3",
  .category = "Sprites",
  .base_address = PaletteAddress::kSpritesAux3,
  .palette_count = PaletteCount::kSpritesAux3,
  .colors_per_palette = 7,   // 7 colors (ROM stores 7, transparent added in memory)
  .colors_per_row = 8,        // Display as 8-color sub-palettes (with transparent)
  .bits_per_pixel = 4,
  .description = "Auxiliary sprite palettes 3: 7 colors per palette (transparent added at runtime)",
  .has_animations = false
};

constexpr PaletteGroupMetadata kArmor = {
  .group_id = PaletteGroupName::kArmor,
  .display_name = "Armor / Link",
  .category = "Equipment",
  .base_address = PaletteAddress::kArmor,
  .palette_count = PaletteCount::kArmor,
  .colors_per_palette = 15,  // 15 colors (ROM stores 15, transparent added in memory for full row)
  .colors_per_row = 16,       // Display as full 16-color rows (with transparent at index 0)
  .bits_per_pixel = 4,
  .description = "Link's tunic colors: 15 colors per palette (Green, Blue, Red, Bunny, Electrocuted)",
  .has_animations = false
};

constexpr PaletteGroupMetadata kSwords = {
  .group_id = PaletteGroupName::kSwords,
  .display_name = "Swords",
  .category = "Equipment",
  .base_address = PaletteAddress::kSwords,
  .palette_count = PaletteCount::kSwords,
  .colors_per_palette = 3,   // 3 colors (overlay palette, no transparent)
  .colors_per_row = 4,        // Display in compact groups
  .bits_per_pixel = 4,
  .description = "Sword blade colors: 3 colors per palette (Fighter, Master, Tempered, Golden)",
  .has_animations = false
};

constexpr PaletteGroupMetadata kShields = {
  .group_id = PaletteGroupName::kShields,
  .display_name = "Shields",
  .category = "Equipment",
  .base_address = PaletteAddress::kShields,
  .palette_count = PaletteCount::kShields,
  .colors_per_palette = 4,   // 4 colors (overlay palette, no transparent)
  .colors_per_row = 4,        // Display in compact groups
  .bits_per_pixel = 4,
  .description = "Shield colors: 4 colors per palette (Fighter, Fire, Mirror)",
  .has_animations = false
};

constexpr PaletteGroupMetadata kHud = {
  .group_id = PaletteGroupName::kHud,
  .display_name = "HUD",
  .category = "Interface",
  .base_address = PaletteAddress::kHud,
  .palette_count = PaletteCount::kHud,
  .colors_per_palette = 32,  // 32 colors: 2 full rows (0-15, 16-31) with transparent at 0, 16
  .colors_per_row = 16,       // Display in 16-color rows
  .bits_per_pixel = 2, // HUD palettes are 2bpp
  .description = "HUD/Interface palettes: 32 colors per set (2 full rows)",
  .has_animations = false
};

constexpr PaletteGroupMetadata kOverworldAux = {
  .group_id = PaletteGroupName::kOverworldAux,
  .display_name = "Overworld Auxiliary",
  .category = "Overworld",
  .base_address = PaletteAddress::kOverworldAux,
  .palette_count = PaletteCount::kOverworldAux,
  .colors_per_palette = 21,  // 21 colors: 1 full row (0-15) + 5 colors (16-20)
  .colors_per_row = 16,       // Display in 16-color rows
  .bits_per_pixel = 4,
  .description = "Overworld auxiliary palettes: 21 colors per set (1 full row + 5 colors)",
  .has_animations = false
};

constexpr PaletteGroupMetadata kGrass = {
  .group_id = PaletteGroupName::kGrass,
  .display_name = "Grass",
  .category = "Overworld",
  .base_address = PaletteAddress::kGrassLW,
  .palette_count = PaletteCount::kGrass,
  .colors_per_palette = 1,   // Single color per entry
  .colors_per_row = 3,        // Display all 3 in one row
  .bits_per_pixel = 4,
  .description = "Hardcoded grass colors: 3 individual colors (LW, DW, Special)",
  .has_animations = false
};

constexpr PaletteGroupMetadata k3DObject = {
  .group_id = PaletteGroupName::k3DObject,
  .display_name = "3D Objects",
  .category = "Special",
  .base_address = PaletteAddress::kTriforce,
  .palette_count = PaletteCount::k3DObject,
  .colors_per_palette = 8,   // 8 colors per palette (7 + transparent)
  .colors_per_row = 8,        // Display in 8-color groups
  .bits_per_pixel = 4,
  .description = "3D object palettes: 8 colors per palette (Triforce, Crystal)",
  .has_animations = false
};

constexpr PaletteGroupMetadata kOverworldMiniMap = {
  .group_id = PaletteGroupName::kOverworldMiniMap,
  .display_name = "Overworld Mini Map",
  .category = "Interface",
  .base_address = PaletteAddress::kOverworldMiniMap,
  .palette_count = PaletteCount::kOverworldMiniMap,
  .colors_per_palette = 128, // 128 colors: 8 full rows (0-127) with transparent at 0, 16, 32, 48, 64, 80, 96, 112
  .colors_per_row = 16,       // Display in 16-color rows
  .bits_per_pixel = 4,
  .description = "Overworld mini-map palettes: 128 colors per set (8 full rows)",
  .has_animations = false
};

}  // namespace PaletteMetadata

// Helper to get metadata by group name
const PaletteGroupMetadata* GetPaletteGroupMetadata(const char* group_id);

// Get all available palette groups
std::vector<const PaletteGroupMetadata*> GetAllPaletteGroups();

}  // namespace yaze::zelda3

#endif  // YAZE_ZELDA3_PALETTE_CONSTANTS_H

