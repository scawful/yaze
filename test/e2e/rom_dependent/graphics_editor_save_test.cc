#include <gtest/gtest.h>

#include <algorithm>
#include <filesystem>
#include <fstream>
#include <memory>
#include <set>
#include <string>
#include <vector>

#include "app/gfx/resource/arena.h"
#include "app/gfx/util/compression.h"
#include "rom/rom.h"
#include "test/test_utils.h"
#include "zelda3/game_data.h"
#include "zelda3/game_data.h"
#include "testing.h"
#include "zelda.h"

namespace yaze {
namespace test {

/**
 * @brief E2E Test Suite for Graphics Editor Save Operations
 *
 * Validates the complete graphics editing workflow:
 * 1. Load ROM and graphics sheets
 * 2. Modify pixel data in sheets
 * 3. Save changes (8BPP→SNES indexed + LC-LZ2 compression)
 * 4. Reload ROM and verify edits persisted
 * 5. Verify no data corruption occurred
 */
class GraphicsEditorSaveTest : public ::testing::Test {
 protected:
  void SetUp() override {
    yaze::test::TestRomManager::SkipIfRomMissing(
        yaze::test::RomRole::kVanilla, "GraphicsEditorSaveTest");
    vanilla_rom_path_ =
        yaze::test::TestRomManager::GetRomPath(yaze::test::RomRole::kVanilla);

    // Create test ROM copies
    test_rom_path_ = "test_graphics_edit.sfc";
    backup_rom_path_ = "test_graphics_backup.sfc";

    // Copy vanilla ROM for testing
    std::filesystem::copy_file(
        vanilla_rom_path_, test_rom_path_,
        std::filesystem::copy_options::overwrite_existing);
    std::filesystem::copy_file(
        vanilla_rom_path_, backup_rom_path_,
        std::filesystem::copy_options::overwrite_existing);
  }

  void TearDown() override {
    // Clean up test files
    if (std::filesystem::exists(test_rom_path_)) {
      std::filesystem::remove(test_rom_path_);
    }
    if (std::filesystem::exists(backup_rom_path_)) {
      std::filesystem::remove(backup_rom_path_);
    }
  }

  static int GetSheetBpp(uint16_t sheet_id) {
    if (sheet_id == 113 || sheet_id == 114 || sheet_id >= 218) {
      return 2;
    }
    return 3;
  }

  static uint8_t NextPixelValue(uint16_t sheet_id, uint8_t current,
                                uint8_t delta = 1) {
    const uint8_t max_colors = static_cast<uint8_t>(1u << GetSheetBpp(sheet_id));
    return static_cast<uint8_t>((current + delta) % max_colors);
  }

  // Helper to load ROM and verify basic integrity
  static absl::Status LoadAndVerifyROM(const std::string& path,
                                       std::unique_ptr<Rom>& rom) {
    rom = std::make_unique<Rom>();
    RETURN_IF_ERROR(rom->LoadFromFile(path));

    // Basic ROM integrity checks
    EXPECT_EQ(rom->size(), 0x200000) << "ROM size should be 2MB";
    EXPECT_NE(rom->data(), nullptr) << "ROM data should not be null";

    return absl::OkStatus();
  }

  // Helper to load graphics sheets from ROM into Arena
  static absl::Status LoadGraphicsFromRom(Rom& rom) {
    zelda3::GameData game_data;
    RETURN_IF_ERROR(zelda3::LoadGameData(rom, game_data));

    // Copy loaded sheets to Arena
    auto& arena_sheets = gfx::Arena::Get().gfx_sheets();
    for (size_t i = 0; i < zelda3::kNumGfxSheets; i++) {
      arena_sheets[i] = std::move(game_data.gfx_bitmaps[i]);
    }

    return absl::OkStatus();
  }

  // Helper to save modified sheets to ROM (mirrors GraphicsEditor::Save())
  static absl::Status SaveSheetToRom(Rom& rom, uint16_t sheet_id) {
    if (sheet_id >= zelda3::kNumGfxSheets) {
      return absl::InvalidArgumentError("Sheet ID out of range");
    }

    auto& sheets = gfx::Arena::Get().gfx_sheets();
    auto& sheet = sheets[sheet_id];

    if (!sheet.is_active()) {
      return absl::FailedPreconditionError("Sheet not active");
    }

    // Determine BPP and compression based on sheet range
    const int bpp = GetSheetBpp(sheet_id);
    bool compressed = true;

    // Sheets 115-126 are uncompressed
    if (sheet_id >= 115 && sheet_id <= 126) {
      compressed = false;
    }

    // Calculate ROM offset for this sheet
    auto version = zelda3_detect_version(rom.data(), rom.size());
    auto vc_it = zelda3::kVersionConstantsMap.find(version);
    if (vc_it == zelda3::kVersionConstantsMap.end() ||
        vc_it->second.kOverworldGfxPtr1 == 0) {
      vc_it = zelda3::kVersionConstantsMap.find(zelda3_version::US);
    }
    const auto& vc = vc_it->second;
    uint32_t offset = zelda3::GetGraphicsAddress(
        rom.data(), static_cast<uint8_t>(sheet_id),
        vc.kOverworldGfxPtr1, vc.kOverworldGfxPtr2,
        vc.kOverworldGfxPtr3, rom.size());

    // Convert 8BPP bitmap data to SNES planar format
    auto snes_tile_data = gfx::IndexedToSnesSheet(sheet.vector(), bpp);

    constexpr size_t kDecompressedSheetSize = 0x800;
    std::vector<uint8_t> base_data;
    if (compressed) {
      auto decomp_result = gfx::lc_lz2::DecompressV2(
          rom.data(), offset, static_cast<int>(kDecompressedSheetSize), 1,
          rom.size());
      RETURN_IF_ERROR(decomp_result.status());
      base_data = std::move(*decomp_result);
    } else {
      auto read_result = rom.ReadByteVector(offset, kDecompressedSheetSize);
      RETURN_IF_ERROR(read_result.status());
      base_data = std::move(*read_result);
    }

    if (base_data.size() < snes_tile_data.size()) {
      base_data.resize(snes_tile_data.size(), 0);
    }
    std::copy(snes_tile_data.begin(), snes_tile_data.end(),
              base_data.begin());

    std::vector<uint8_t> final_data;
    if (compressed) {
      // Compress using Hyrule Magic LC-LZ2
      int compressed_size = 0;
      auto compressed_data = gfx::HyruleMagicCompress(
          base_data.data(), static_cast<int>(base_data.size()),
          &compressed_size, 1);
      final_data.assign(compressed_data.begin(),
                        compressed_data.begin() + compressed_size);
    } else {
      final_data = std::move(base_data);
    }

    // Write data to ROM buffer
    for (size_t i = 0; i < final_data.size(); i++) {
      RETURN_IF_ERROR(rom.WriteByte(offset + i, final_data[i]));
    }

    return absl::OkStatus();
  }

  std::string vanilla_rom_path_;
  std::string test_rom_path_;
  std::string backup_rom_path_;
};

// Test 1: Single sheet edit, save, and reload verification
TEST_F(GraphicsEditorSaveTest, SingleSheetEdit_SaveAndReload) {
  std::unique_ptr<Rom> rom;
  ASSERT_OK(LoadAndVerifyROM(vanilla_rom_path_, rom));

  // Load graphics into Arena
  ASSERT_OK(LoadGraphicsFromRom(*rom));

  // Get initial pixel value from sheet 0
  auto& sheets = gfx::Arena::Get().gfx_sheets();
  auto& sheet = sheets[0];
  ASSERT_TRUE(sheet.is_active()) << "Sheet 0 should be active after loading";

  // Record original pixel value at (0,0)
  uint8_t original_pixel = sheet.GetPixel(0, 0);

  // Modify pixel (cycle to next value, wrapping at 16 for 4-bit indexed)
  uint8_t new_pixel = NextPixelValue(0, original_pixel);
  sheet.WriteToPixel(0, 0, new_pixel);

  // Verify modification took effect in memory
  EXPECT_EQ(sheet.GetPixel(0, 0), new_pixel)
      << "Pixel modification should be reflected immediately";

  // Save modified sheet to ROM
  ASSERT_OK(SaveSheetToRom(*rom, 0));

  // Save ROM to file
  ASSERT_OK(rom->SaveToFile(Rom::SaveSettings{.filename = test_rom_path_}));

  // --- Reload and verify ---
  std::unique_ptr<Rom> reloaded_rom;
  ASSERT_OK(LoadAndVerifyROM(test_rom_path_, reloaded_rom));

  // Load graphics from reloaded ROM
  ASSERT_OK(LoadGraphicsFromRom(*reloaded_rom));

  // Verify pixel change persisted
  auto& reloaded_sheet = gfx::Arena::Get().gfx_sheets()[0];
  EXPECT_EQ(reloaded_sheet.GetPixel(0, 0), new_pixel)
      << "Pixel modification should persist after save/reload";
}

// Test 2: Multiple sheet edits save atomically
TEST_F(GraphicsEditorSaveTest, MultipleSheetEdit_Atomicity) {
  std::unique_ptr<Rom> rom;
  ASSERT_OK(LoadAndVerifyROM(vanilla_rom_path_, rom));
  ASSERT_OK(LoadGraphicsFromRom(*rom));

  auto& sheets = gfx::Arena::Get().gfx_sheets();

  // Modify sheets 0, 50, and 100 with distinct values
  const std::vector<uint16_t> test_sheets = {0, 50, 100};
  std::vector<uint8_t> test_values;
  test_values.reserve(test_sheets.size());

  for (size_t i = 0; i < test_sheets.size(); i++) {
    auto& sheet = sheets[test_sheets[i]];
    if (sheet.is_active()) {
      uint8_t original_pixel = sheet.GetPixel(0, 0);
      uint8_t new_pixel =
          NextPixelValue(test_sheets[i], original_pixel, static_cast<uint8_t>(i + 1));
      sheet.WriteToPixel(0, 0, new_pixel);
      test_values.push_back(new_pixel);
    } else {
      test_values.push_back(0);
    }
  }

  // Save all modified sheets
  for (uint16_t sheet_id : test_sheets) {
    if (sheets[sheet_id].is_active()) {
      ASSERT_OK(SaveSheetToRom(*rom, sheet_id));
    }
  }

  // Save ROM
  ASSERT_OK(rom->SaveToFile(Rom::SaveSettings{.filename = test_rom_path_}));

  // Reload and verify ALL changes persisted
  std::unique_ptr<Rom> reloaded_rom;
  ASSERT_OK(LoadAndVerifyROM(test_rom_path_, reloaded_rom));
  ASSERT_OK(LoadGraphicsFromRom(*reloaded_rom));

  auto& reloaded_sheets = gfx::Arena::Get().gfx_sheets();
  for (size_t i = 0; i < test_sheets.size(); i++) {
    if (reloaded_sheets[test_sheets[i]].is_active()) {
      EXPECT_EQ(reloaded_sheets[test_sheets[i]].GetPixel(0, 0), test_values[i])
          << "Sheet " << test_sheets[i] << " modification should persist";
    }
  }
}

// Test 3: Compression integrity for LC-LZ2 compressed sheets
TEST_F(GraphicsEditorSaveTest, CompressionIntegrity_LZ2Sheets) {
  std::unique_ptr<Rom> rom;
  ASSERT_OK(LoadAndVerifyROM(vanilla_rom_path_, rom));
  ASSERT_OK(LoadGraphicsFromRom(*rom));

  // Sheet 50 should be compressed (in range 0-112)
  const uint16_t test_sheet = 50;
  auto& sheets = gfx::Arena::Get().gfx_sheets();
  auto& sheet = sheets[test_sheet];

  if (!sheet.is_active()) {
    GTEST_SKIP() << "Sheet 50 not active in this ROM";
  }

  // Record original data for a small region
  std::vector<uint8_t> original_region;
  for (int y = 0; y < 8; y++) {
    for (int x = 0; x < 8; x++) {
      original_region.push_back(sheet.GetPixel(x, y));
    }
  }

  // Modify a single pixel
  uint8_t original_pixel = sheet.GetPixel(4, 4);
  uint8_t new_pixel = NextPixelValue(test_sheet, original_pixel, 1);
  sheet.WriteToPixel(4, 4, new_pixel);

  // Save and reload
  ASSERT_OK(SaveSheetToRom(*rom, test_sheet));
  ASSERT_OK(rom->SaveToFile(Rom::SaveSettings{.filename = test_rom_path_}));

  std::unique_ptr<Rom> reloaded_rom;
  ASSERT_OK(LoadAndVerifyROM(test_rom_path_, reloaded_rom));
  ASSERT_OK(LoadGraphicsFromRom(*reloaded_rom));

  // Verify the modified pixel
  auto& reloaded_sheet = gfx::Arena::Get().gfx_sheets()[test_sheet];
  EXPECT_EQ(reloaded_sheet.GetPixel(4, 4), new_pixel)
      << "Modified pixel should persist through compression round-trip";

  // Verify surrounding pixels weren't corrupted
  for (int y = 0; y < 8; y++) {
    for (int x = 0; x < 8; x++) {
      if (x == 4 && y == 4) continue;  // Skip modified pixel
      int idx = y * 8 + x;
      EXPECT_EQ(reloaded_sheet.GetPixel(x, y), original_region[idx])
          << "Pixel at (" << x << "," << y << ") should not be corrupted";
    }
  }
}

// Test 4: Uncompressed sheets (115-126) save correctly
TEST_F(GraphicsEditorSaveTest, UncompressedSheets_SaveCorrectly) {
  std::unique_ptr<Rom> rom;
  ASSERT_OK(LoadAndVerifyROM(vanilla_rom_path_, rom));
  ASSERT_OK(LoadGraphicsFromRom(*rom));

  // Sheet 115 is uncompressed
  const uint16_t test_sheet = 115;
  auto& sheets = gfx::Arena::Get().gfx_sheets();
  auto& sheet = sheets[test_sheet];

  if (!sheet.is_active()) {
    GTEST_SKIP() << "Sheet 115 not active in this ROM";
  }

  // Modify pixel
  uint8_t original_pixel = sheet.GetPixel(0, 0);
  uint8_t new_pixel = NextPixelValue(test_sheet, original_pixel, 1);
  sheet.WriteToPixel(0, 0, new_pixel);

  // Save and reload
  ASSERT_OK(SaveSheetToRom(*rom, test_sheet));
  ASSERT_OK(rom->SaveToFile(Rom::SaveSettings{.filename = test_rom_path_}));

  std::unique_ptr<Rom> reloaded_rom;
  ASSERT_OK(LoadAndVerifyROM(test_rom_path_, reloaded_rom));
  ASSERT_OK(LoadGraphicsFromRom(*reloaded_rom));

  auto& reloaded_sheet = gfx::Arena::Get().gfx_sheets()[test_sheet];
  EXPECT_EQ(reloaded_sheet.GetPixel(0, 0), new_pixel)
      << "Uncompressed sheet modification should persist";
}

// Test 5: Save without corrupting adjacent sheet data
TEST_F(GraphicsEditorSaveTest, SaveWithoutCorruption_AdjacentData) {
  std::unique_ptr<Rom> rom;
  ASSERT_OK(LoadAndVerifyROM(vanilla_rom_path_, rom));
  ASSERT_OK(LoadGraphicsFromRom(*rom));

  auto& sheets = gfx::Arena::Get().gfx_sheets();

  // Record pixel values from adjacent sheets (49 and 51)
  uint8_t sheet49_pixel = 0;
  uint8_t sheet51_pixel = 0;

  if (sheets[49].is_active()) {
    sheet49_pixel = sheets[49].GetPixel(0, 0);
  }
  if (sheets[51].is_active()) {
    sheet51_pixel = sheets[51].GetPixel(0, 0);
  }

  // Modify only sheet 50
  if (sheets[50].is_active()) {
    uint8_t original_pixel = sheets[50].GetPixel(0, 0);
    uint8_t new_pixel = NextPixelValue(50, original_pixel, 1);
    sheets[50].WriteToPixel(0, 0, new_pixel);
    ASSERT_OK(SaveSheetToRom(*rom, 50));
  }

  // Save and reload
  ASSERT_OK(rom->SaveToFile(Rom::SaveSettings{.filename = test_rom_path_}));

  std::unique_ptr<Rom> reloaded_rom;
  ASSERT_OK(LoadAndVerifyROM(test_rom_path_, reloaded_rom));
  ASSERT_OK(LoadGraphicsFromRom(*reloaded_rom));

  auto& reloaded_sheets = gfx::Arena::Get().gfx_sheets();

  // Verify adjacent sheets weren't corrupted
  if (reloaded_sheets[49].is_active()) {
    EXPECT_EQ(reloaded_sheets[49].GetPixel(0, 0), sheet49_pixel)
        << "Sheet 49 should not be corrupted by saving sheet 50";
  }
  if (reloaded_sheets[51].is_active()) {
    EXPECT_EQ(reloaded_sheets[51].GetPixel(0, 0), sheet51_pixel)
        << "Sheet 51 should not be corrupted by saving sheet 50";
  }
}

// Test 6: Round-trip with no modifications preserves data
TEST_F(GraphicsEditorSaveTest, RoundTrip_NoModifications) {
  std::unique_ptr<Rom> rom;
  ASSERT_OK(LoadAndVerifyROM(vanilla_rom_path_, rom));
  ASSERT_OK(LoadGraphicsFromRom(*rom));

  // Record sample pixel values from multiple sheets
  auto& sheets = gfx::Arena::Get().gfx_sheets();
  std::map<uint16_t, uint8_t> original_pixels;

  for (uint16_t id : {0, 25, 50, 75, 100, 150, 200}) {
    if (sheets[id].is_active()) {
      original_pixels[id] = sheets[id].GetPixel(0, 0);
    }
  }

  // Save ROM without modifications
  ASSERT_OK(rom->SaveToFile(Rom::SaveSettings{.filename = test_rom_path_}));

  // Reload
  std::unique_ptr<Rom> reloaded_rom;
  ASSERT_OK(LoadAndVerifyROM(test_rom_path_, reloaded_rom));
  ASSERT_OK(LoadGraphicsFromRom(*reloaded_rom));

  auto& reloaded_sheets = gfx::Arena::Get().gfx_sheets();

  // Verify all recorded pixels match
  for (const auto& [id, pixel] : original_pixels) {
    if (reloaded_sheets[id].is_active()) {
      EXPECT_EQ(reloaded_sheets[id].GetPixel(0, 0), pixel)
          << "Sheet " << id << " should preserve data through round-trip";
    }
  }
}

}  // namespace test
}  // namespace yaze
