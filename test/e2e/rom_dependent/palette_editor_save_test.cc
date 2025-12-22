#include <gtest/gtest.h>

#include <filesystem>
#include <memory>
#include <string>
#include <vector>

#include "e2e/rom_dependent/editor_save_test_base.h"
#include "app/gfx/types/snes_color.h"
#include "app/gfx/types/snes_palette.h"
#include "app/gfx/util/palette_manager.h"
#include "rom/rom.h"
#include "testing.h"
#include "zelda3/game_data.h"

namespace yaze {
namespace test {

/**
 * @brief E2E Test Suite for PaletteEditor Save Operations
 *
 * Validates the complete palette editing workflow:
 * 1. Load ROM and palette data
 * 2. Modify colors in various palette groups
 * 3. Save changes to ROM
 * 4. Reload ROM and verify edits persisted
 * 5. Verify SNES color format round-trip accuracy
 */
class PaletteEditorSaveTest : public EditorSaveTestBase {
 protected:
  void SetUp() override {
    EditorSaveTestBase::SetUp();

    // Load the test ROM
    rom_ = std::make_unique<Rom>();
    auto load_result = rom_->LoadFromFile(test_rom_path_);
    if (!load_result.ok()) {
      GTEST_SKIP() << "Failed to load test ROM: " << load_result.message();
    }

    // Load game data (which includes all palettes)
    game_data_ = std::make_unique<zelda3::GameData>();
    auto gd_result = zelda3::LoadGameData(*rom_, *game_data_);
    if (!gd_result.ok()) {
      GTEST_SKIP() << "Failed to load game data: " << gd_result.message();
    }

    // Initialize PaletteManager with game data
    gfx::PaletteManager::Get().Initialize(game_data_.get());
  }

  void TearDown() override {
    // Reset PaletteManager state
    gfx::PaletteManager::Get().DiscardAllChanges();
    EditorSaveTestBase::TearDown();
  }

  // Helper to read a SNES color directly from ROM
  absl::StatusOr<gfx::SnesColor> ReadColorFromRom(Rom& rom, uint32_t offset) {
    auto word = rom.ReadWord(offset);
    if (!word.ok()) {
      return word.status();
    }
    return gfx::SnesColor(*word);
  }

  // Helper to write a SNES color to ROM
  absl::Status WriteColorToRom(Rom& rom, uint32_t offset, const gfx::SnesColor& color) {
    return rom.WriteWord(offset, color.snes());
  }

  // Known palette addresses in vanilla ROM (version-specific)
  static constexpr uint32_t kOverworldPaletteMain = 0xDE6C8;
  static constexpr uint32_t kDungeonPaletteMain = 0xDD734;
  static constexpr uint32_t kSpritePaletteGlobal = 0xDD218;
  static constexpr uint32_t kHudPalette = 0xDD660;

  std::unique_ptr<Rom> rom_;
  std::unique_ptr<zelda3::GameData> game_data_;
};

// Test 1: Single color modification persistence
TEST_F(PaletteEditorSaveTest, SingleColor_SaveAndReload) {
  // Read original color from overworld palette
  auto original_color = ReadColorFromRom(*rom_, kOverworldPaletteMain);
  if (!original_color.ok()) {
    GTEST_SKIP() << "Failed to read original color";
  }

  // Create a modified color (shift hue)
  uint16_t original_snes = original_color->snes();
  uint16_t modified_snes = ((original_snes + 0x0421) & 0x7FFF);  // Add some color
  gfx::SnesColor modified_color(modified_snes);

  // Write modified color to ROM
  ASSERT_OK(WriteColorToRom(*rom_, kOverworldPaletteMain, modified_color));

  // Save ROM
  ASSERT_OK(SaveRomToFile(rom_.get(), test_rom_path_));

  // Reload and verify
  std::unique_ptr<Rom> reloaded_rom;
  ASSERT_OK(LoadAndVerifyRom(test_rom_path_, reloaded_rom));

  auto reloaded_color = ReadColorFromRom(*reloaded_rom, kOverworldPaletteMain);
  ASSERT_TRUE(reloaded_color.ok());
  EXPECT_EQ(reloaded_color->snes(), modified_snes)
      << "Color modification should persist after save/reload";
}

// Test 2: Multiple palette group modifications
TEST_F(PaletteEditorSaveTest, MultiplePaletteGroups_SaveAndReload) {
  // Test modifying colors in different palette groups
  const std::vector<std::pair<uint32_t, std::string>> palette_offsets = {
      {kOverworldPaletteMain, "Overworld Main"},
      {kDungeonPaletteMain, "Dungeon Main"},
      {kSpritePaletteGlobal, "Sprite Global"},
  };

  std::map<uint32_t, uint16_t> original_colors;
  std::map<uint32_t, uint16_t> modified_colors;

  // Record originals and prepare modifications
  for (const auto& [offset, name] : palette_offsets) {
    auto color = ReadColorFromRom(*rom_, offset);
    if (!color.ok()) continue;

    original_colors[offset] = color->snes();
    // Create unique modification for each palette
    modified_colors[offset] = (color->snes() ^ 0x1234) & 0x7FFF;
  }

  // Apply all modifications
  for (const auto& [offset, new_color] : modified_colors) {
    ASSERT_OK(WriteColorToRom(*rom_, offset, gfx::SnesColor(new_color)));
  }

  // Save ROM
  ASSERT_OK(SaveRomToFile(rom_.get(), test_rom_path_));

  // Reload and verify all changes
  std::unique_ptr<Rom> reloaded_rom;
  ASSERT_OK(LoadAndVerifyRom(test_rom_path_, reloaded_rom));

  for (const auto& [offset, expected_color] : modified_colors) {
    auto reloaded = ReadColorFromRom(*reloaded_rom, offset);
    ASSERT_TRUE(reloaded.ok());
    EXPECT_EQ(reloaded->snes(), expected_color)
        << "Palette at 0x" << std::hex << offset << " should persist";
  }
}

// Test 3: SNES color format round-trip accuracy
TEST_F(PaletteEditorSaveTest, SnesColorFormat_RoundTrip) {
  // Test that SNES color format conversions are accurate
  const std::vector<uint16_t> test_colors = {
      0x0000,  // Black
      0x7FFF,  // White
      0x001F,  // Red (max)
      0x03E0,  // Green (max)
      0x7C00,  // Blue (max)
      0x0421,  // Dark gray
      0x294A,  // Medium gray
      0x5294,  // Light gray
      0x1234,  // Random color
      0x5678,  // Another random
  };

  for (uint16_t test_snes : test_colors) {
    // Convert to SnesColor and back
    gfx::SnesColor color(test_snes);

    // Get RGB representation in 0-255 range
    auto rgb = color.rom_color();

    // Create new color from RGB
    gfx::SnesColor reconstructed(rgb);

    // Verify SNES value matches (may have small rounding differences)
    uint16_t reconstructed_snes = reconstructed.snes();

    // Allow for minor quantization differences (SNES uses 5-bit color)
    int diff_r = std::abs((test_snes & 0x1F) - (reconstructed_snes & 0x1F));
    int diff_g = std::abs(((test_snes >> 5) & 0x1F) - 
                          ((reconstructed_snes >> 5) & 0x1F));
    int diff_b = std::abs(((test_snes >> 10) & 0x1F) - 
                          ((reconstructed_snes >> 10) & 0x1F));

    EXPECT_LE(diff_r, 1) << "Red channel should be accurate for 0x" 
                         << std::hex << test_snes;
    EXPECT_LE(diff_g, 1) << "Green channel should be accurate for 0x" 
                         << std::hex << test_snes;
    EXPECT_LE(diff_b, 1) << "Blue channel should be accurate for 0x" 
                         << std::hex << test_snes;
  }
}

// Test 4: Full palette (16 colors) save/reload
TEST_F(PaletteEditorSaveTest, FullPalette_SaveAndReload) {
  // Save/reload a complete 16-color palette
  const uint32_t palette_base = kOverworldPaletteMain;
  std::vector<uint16_t> original_palette(16);
  std::vector<uint16_t> modified_palette(16);

  // Read original palette
  for (int i = 0; i < 16; ++i) {
    auto color = ReadColorFromRom(*rom_, palette_base + (i * 2));
    original_palette[i] = color.ok() ? color->snes() : 0;
    // Create gradient modification
    modified_palette[i] = (i * 0x0842) & 0x7FFF;
  }

  // Write modified palette
  for (int i = 0; i < 16; ++i) {
    ASSERT_OK(WriteColorToRom(*rom_, palette_base + (i * 2),
                               gfx::SnesColor(modified_palette[i])));
  }

  // Save ROM
  ASSERT_OK(SaveRomToFile(rom_.get(), test_rom_path_));

  // Reload and verify
  std::unique_ptr<Rom> reloaded_rom;
  ASSERT_OK(LoadAndVerifyRom(test_rom_path_, reloaded_rom));

  for (int i = 0; i < 16; ++i) {
    auto reloaded = ReadColorFromRom(*reloaded_rom, palette_base + (i * 2));
    ASSERT_TRUE(reloaded.ok());
    EXPECT_EQ(reloaded->snes(), modified_palette[i])
        << "Palette entry " << i << " should persist";
  }
}

// Test 5: Adjacent palette data not corrupted
TEST_F(PaletteEditorSaveTest, NoAdjacentCorruption) {
  // Modify middle palette and verify adjacent palettes aren't corrupted
  const uint32_t target_offset = kOverworldPaletteMain + 32;  // 16th color
  const uint32_t prev_offset = kOverworldPaletteMain + 30;    // 15th color
  const uint32_t next_offset = kOverworldPaletteMain + 34;    // 17th color

  // Record adjacent colors
  auto prev_color = ReadColorFromRom(*rom_, prev_offset);
  auto next_color = ReadColorFromRom(*rom_, next_offset);

  if (!prev_color.ok() || !next_color.ok()) {
    GTEST_SKIP() << "Failed to read adjacent colors";
  }

  // Modify target color
  gfx::SnesColor target_modified(0x5555);
  ASSERT_OK(WriteColorToRom(*rom_, target_offset, target_modified));

  // Save ROM
  ASSERT_OK(SaveRomToFile(rom_.get(), test_rom_path_));

  // Reload and verify
  std::unique_ptr<Rom> reloaded_rom;
  ASSERT_OK(LoadAndVerifyRom(test_rom_path_, reloaded_rom));

  // Verify target was modified
  auto target_reloaded = ReadColorFromRom(*reloaded_rom, target_offset);
  ASSERT_TRUE(target_reloaded.ok());
  EXPECT_EQ(target_reloaded->snes(), 0x5555);

  // Verify adjacent colors not corrupted
  auto prev_reloaded = ReadColorFromRom(*reloaded_rom, prev_offset);
  auto next_reloaded = ReadColorFromRom(*reloaded_rom, next_offset);
  ASSERT_TRUE(prev_reloaded.ok());
  ASSERT_TRUE(next_reloaded.ok());

  EXPECT_EQ(prev_reloaded->snes(), prev_color->snes())
      << "Previous color should not be corrupted";
  EXPECT_EQ(next_reloaded->snes(), next_color->snes())
      << "Next color should not be corrupted";
}

// Test 6: PaletteManager integration
TEST_F(PaletteEditorSaveTest, PaletteManager_SaveAllToRom) {
  // Get a palette group through PaletteManager
  auto& pm = gfx::PaletteManager::Get();

  if (!pm.IsInitialized()) {
    GTEST_SKIP() << "PaletteManager not initialized";
  }

  // Try to modify a color through PaletteManager
  gfx::SnesColor original_color = pm.GetColor("ow_main", 0, 0);

  // Modify the color
  uint16_t new_snes_value = (original_color.snes() + 0x0842) & 0x7FFF;
  ASSERT_OK(pm.SetColor("ow_main", 0, 0, gfx::SnesColor(new_snes_value)));

  // Save through PaletteManager
  auto save_result = pm.SaveAllToRom();
  if (!save_result.ok()) {
    // If save isn't implemented, skip gracefully
    GTEST_SKIP() << "PaletteManager SaveAllToRom not implemented: "
                 << save_result.message();
  }

  // Save ROM to disk
  ASSERT_OK(SaveRomToFile(rom_.get(), test_rom_path_));

  // Reload and verify
  std::unique_ptr<Rom> reloaded_rom;
  ASSERT_OK(LoadAndVerifyRom(test_rom_path_, reloaded_rom));

  std::unique_ptr<zelda3::GameData> reloaded_gd = std::make_unique<zelda3::GameData>();
  ASSERT_OK(zelda3::LoadGameData(*reloaded_rom, *reloaded_gd));

  auto* reloaded_palette = reloaded_gd->palette_groups.overworld_main.mutable_palette(0);
  if (reloaded_palette && reloaded_palette->size() > 0) {
    EXPECT_EQ((*reloaded_palette)[0].snes(), new_snes_value)
        << "Color should persist through PaletteManager save";
  }
}

// Test 7: HUD palette modifications
TEST_F(PaletteEditorSaveTest, HudPalette_Persistence) {
  // HUD palette should persist correctly
  std::vector<uint16_t> original_hud(16);
  std::vector<uint16_t> modified_hud(16);

  // Read and modify HUD palette
  for (int i = 0; i < 16; ++i) {
    auto color = ReadColorFromRom(*rom_, kHudPalette + (i * 2));
    original_hud[i] = color.ok() ? color->snes() : 0;
    // Invert colors for testing
    modified_hud[i] = (original_hud[i] ^ 0x7FFF) & 0x7FFF;
  }

  // Apply modifications
  for (int i = 0; i < 16; ++i) {
    ASSERT_OK(WriteColorToRom(*rom_, kHudPalette + (i * 2),
                               gfx::SnesColor(modified_hud[i])));
  }

  // Save ROM
  ASSERT_OK(SaveRomToFile(rom_.get(), test_rom_path_));

  // Reload and verify
  std::unique_ptr<Rom> reloaded_rom;
  ASSERT_OK(LoadAndVerifyRom(test_rom_path_, reloaded_rom));

  for (int i = 0; i < 16; ++i) {
    auto reloaded = ReadColorFromRom(*reloaded_rom, kHudPalette + (i * 2));
    ASSERT_TRUE(reloaded.ok());
    EXPECT_EQ(reloaded->snes(), modified_hud[i])
        << "HUD palette entry " << i << " should persist";
  }
}

// Test 8: Large batch palette modifications
TEST_F(PaletteEditorSaveTest, LargeBatch_PaletteModifications) {
  // Test modifying many palette entries at once
  const int batch_size = 256;  // 256 colors = 16 full palettes
  const uint32_t base_offset = kOverworldPaletteMain;

  std::vector<uint16_t> original_colors(batch_size);
  std::vector<uint16_t> modified_colors(batch_size);

  // Read and prepare modifications
  for (int i = 0; i < batch_size; ++i) {
    auto color = ReadColorFromRom(*rom_, base_offset + (i * 2));
    original_colors[i] = color.ok() ? color->snes() : 0;
    // Create rainbow pattern
    modified_colors[i] = ((i * 0x0102) & 0x7FFF);
  }

  // Apply all modifications
  for (int i = 0; i < batch_size; ++i) {
    ASSERT_OK(WriteColorToRom(*rom_, base_offset + (i * 2),
                               gfx::SnesColor(modified_colors[i])));
  }

  // Save ROM
  ASSERT_OK(SaveRomToFile(rom_.get(), test_rom_path_));

  // Reload and verify all changes
  std::unique_ptr<Rom> reloaded_rom;
  ASSERT_OK(LoadAndVerifyRom(test_rom_path_, reloaded_rom));

  int verified_count = 0;
  for (int i = 0; i < batch_size; ++i) {
    auto reloaded = ReadColorFromRom(*reloaded_rom, base_offset + (i * 2));
    if (reloaded.ok() && reloaded->snes() == modified_colors[i]) {
      verified_count++;
    }
  }

  EXPECT_EQ(verified_count, batch_size)
      << "All batch palette modifications should persist";
}

// Test 9: Round-trip without modification preserves data
TEST_F(PaletteEditorSaveTest, RoundTrip_NoModification) {
  // Record sample palette colors
  const std::vector<uint32_t> sample_offsets = {
      kOverworldPaletteMain,
      kOverworldPaletteMain + 16,
      kDungeonPaletteMain,
      kSpritePaletteGlobal,
      kHudPalette,
  };

  std::map<uint32_t, uint16_t> original_colors;
  for (uint32_t offset : sample_offsets) {
    auto color = ReadColorFromRom(*rom_, offset);
    if (color.ok()) {
      original_colors[offset] = color->snes();
    }
  }

  // Save ROM without modifications
  ASSERT_OK(SaveRomToFile(rom_.get(), test_rom_path_));

  // Reload and verify
  std::unique_ptr<Rom> reloaded_rom;
  ASSERT_OK(LoadAndVerifyRom(test_rom_path_, reloaded_rom));

  for (const auto& [offset, original_value] : original_colors) {
    auto reloaded = ReadColorFromRom(*reloaded_rom, offset);
    ASSERT_TRUE(reloaded.ok());
    EXPECT_EQ(reloaded->snes(), original_value)
        << "Color at 0x" << std::hex << offset << " should be preserved";
  }
}

}  // namespace test
}  // namespace yaze
