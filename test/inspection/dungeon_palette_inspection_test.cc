#include <gtest/gtest.h>
#include "rom/rom.h"
#include "app/gfx/types/snes_palette.h"
#include "util/log.h"

namespace yaze {
namespace test {

// Test to verify dungeon palette colors are loaded correctly
TEST(DungeonPaletteInspection, VerifyColors) {
  // Load ROM
  Rom rom;
  auto load_result = rom.LoadFromFile("zelda3.sfc");
  if (!load_result.ok()) {
    GTEST_SKIP() << "ROM file not found, skipping palette inspection";
  }

  // Get dungeon main palette group
  const auto& dungeon_pal_group = rom.palette_group().dungeon_main;
  
  ASSERT_FALSE(dungeon_pal_group.empty()) << "Dungeon palette group is empty!";
  
  // Get first palette (palette 0)
  const auto& palette0 = dungeon_pal_group[0];
  
  LOG_INFO("Dungeon Palette 0 - First 16 colors:");
  for (size_t i = 0; i < std::min(size_t(16), palette0.size()); ++i) {
    const auto& color = palette0[i];
    auto rgb = color.rgb();
    LOG_INFO("  Color %02zu: R=%03d G=%03d B=%03d (0x%02X%02X%02X)", 
             i, 
             static_cast<int>(rgb.x), 
             static_cast<int>(rgb.y), 
             static_cast<int>(rgb.z),
             static_cast<int>(rgb.x),
             static_cast<int>(rgb.y),
             static_cast<int>(rgb.z));
  }
  
  // Check palette 7 (the one used in the logs: pal=7)
  if (dungeon_pal_group.size() > 7) {
    const auto& palette7 = dungeon_pal_group[7];
    LOG_INFO("\nDungeon Palette 7 (used by objects) - First 16 colors:");
    for (size_t i = 0; i < std::min(size_t(16), palette7.size()); ++i) {
      const auto& color = palette7[i];
      auto rgb = color.rgb();
      LOG_INFO("  Color %02zu: R=%03d G=%03d B=%03d (0x%02X%02X%02X)", 
               i, 
               static_cast<int>(rgb.x), 
               static_cast<int>(rgb.y), 
               static_cast<int>(rgb.z),
               static_cast<int>(rgb.x),
               static_cast<int>(rgb.y),
               static_cast<int>(rgb.z));
    }
    
    // Color index 56-63 is where pal=7 with offset 7*8=56 would be
    LOG_INFO("\nPalette 7 - Colors 56-63 (pal=7 offset region):");
    for (size_t i = 56; i < std::min(size_t(64), palette7.size()); ++i) {
      const auto& color = palette7[i];
      auto rgb = color.rgb();
      LOG_INFO("  Color %02zu: R=%03d G=%03d B=%03d (0x%02X%02X%02X)", 
               i, 
               static_cast<int>(rgb.x), 
               static_cast<int>(rgb.y), 
               static_cast<int>(rgb.z),
               static_cast<int>(rgb.x),
               static_cast<int>(rgb.y),
               static_cast<int>(rgb.z));
    }
  }
  
  // Verify we have 90 colors
  LOG_INFO("\nTotal palette size: %zu colors", palette0.size());
  EXPECT_EQ(palette0.size(), 90) << "Expected 90 colors for dungeon palette";
}

}  // namespace test
}  // namespace yaze
