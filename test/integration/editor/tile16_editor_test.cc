#include "app/editor/overworld/tile16_editor.h"

#include <gtest/gtest.h>

#include <iostream>
#include <memory>
#include <string>
#include <vector>

#include "app/gfx/backend/renderer_factory.h"
#include "app/gfx/core/bitmap.h"
#include "app/gfx/render/tilemap.h"
#include "app/gfx/resource/arena.h"
#include "app/platform/window.h"
#include "rom/rom.h"
#include "test_utils.h"
#include "zelda3/overworld/overworld.h"

namespace yaze {
namespace editor {
namespace test {

class Tile16EditorIntegrationTest : public ::testing::Test {
 protected:
  static void SetUpTestSuite() {
    // Initialize SDL and rendering system once for all tests
    InitializeTestEnvironment();
  }

  static void TearDownTestSuite() {
    // Clean up SDL
    if (window_initialized_) {
      auto shutdown_result = core::ShutdownWindow(test_window_);
      (void)shutdown_result;  // Suppress unused variable warning
      window_initialized_ = false;
    }
  }

  void SetUp() override {
#ifdef YAZE_ENABLE_ROM_TESTS
    if (!window_initialized_) {
      GTEST_SKIP() << "Failed to initialize graphics system";
    }

    // Load the test ROM
    rom_ = std::make_unique<Rom>();
    YAZE_SKIP_IF_ROM_MISSING(
        yaze::test::RomRole::kVanilla,
        "Tile16EditorIntegrationTest");
    const std::string rom_path =
        yaze::test::TestRomManager::GetRomPath(yaze::test::RomRole::kVanilla);
    auto load_result = rom_->LoadFromFile(rom_path);
    ASSERT_TRUE(load_result.ok())
        << "Failed to load test ROM: " << load_result.message();

    // Load overworld data
    overworld_ = std::make_unique<zelda3::Overworld>(rom_.get());
    auto overworld_load_result = overworld_->Load(rom_.get());
    ASSERT_TRUE(overworld_load_result.ok())
        << "Failed to load overworld: " << overworld_load_result.message();

    // Create tile16 blockset
    auto tile16_data = overworld_->tile16_blockset_data();
    auto palette = overworld_->current_area_palette();

    tile16_blockset_ = std::make_unique<gfx::Tilemap>(
        gfx::CreateTilemap(nullptr, tile16_data, 0x80, 0x2000, 16,
                           zelda3::kNumTile16Individual, palette));

    // Create graphics bitmap
    current_gfx_bmp_ = std::make_unique<gfx::Bitmap>();
    current_gfx_bmp_->Create(0x80, 512, 0x40, overworld_->current_graphics());
    current_gfx_bmp_->SetPalette(palette);
    gfx::Arena::Get().QueueTextureCommand(
        gfx::Arena::TextureCommandType::CREATE, current_gfx_bmp_.get());

    // Create tile16 blockset bitmap
    tile16_blockset_bmp_ = std::make_unique<gfx::Bitmap>();
    tile16_blockset_bmp_->Create(0x80, 0x2000, 0x08, tile16_data);
    tile16_blockset_bmp_->SetPalette(palette);
    gfx::Arena::Get().QueueTextureCommand(
        gfx::Arena::TextureCommandType::CREATE, tile16_blockset_bmp_.get());

    // Initialize the tile16 editor
    editor_ =
        std::make_unique<Tile16Editor>(rom_.get(), tile16_blockset_.get());
    auto init_result =
        editor_->Initialize(*tile16_blockset_bmp_, *current_gfx_bmp_,
                            *overworld_->mutable_all_tiles_types());
    ASSERT_TRUE(init_result.ok())
        << "Failed to initialize editor: " << init_result.message();

    rom_loaded_ = true;
#else
    // Fallback for non-ROM tests
    rom_ = std::make_unique<Rom>();
    tilemap_ = std::make_unique<gfx::Tilemap>();
    editor_ = std::make_unique<Tile16Editor>(rom_.get(), tilemap_.get());
    rom_loaded_ = false;
#endif
  }

 protected:
  static void InitializeTestEnvironment() {
    // Create renderer for test (uses factory for SDL2/SDL3 selection)
    test_renderer_ = gfx::RendererFactory::Create();
    auto window_result = core::CreateWindow(test_window_, test_renderer_.get(),
                                            SDL_WINDOW_HIDDEN);
    if (window_result.ok()) {
      window_initialized_ = true;
    } else {
      window_initialized_ = false;
      // Log the error but don't fail test setup
      std::cerr << "Failed to initialize test window: "
                << window_result.message() << std::endl;
    }
  }

  static bool window_initialized_;
  static core::Window test_window_;
  static std::unique_ptr<gfx::IRenderer> test_renderer_;

  bool rom_loaded_ = false;
  std::unique_ptr<Rom> rom_;
  std::unique_ptr<gfx::Tilemap> tilemap_;
  std::unique_ptr<gfx::Tilemap> tile16_blockset_;
  std::unique_ptr<gfx::Bitmap> current_gfx_bmp_;
  std::unique_ptr<gfx::Bitmap> tile16_blockset_bmp_;
  std::unique_ptr<zelda3::Overworld> overworld_;
  std::unique_ptr<Tile16Editor> editor_;
};

// Static member definitions
bool Tile16EditorIntegrationTest::window_initialized_ = false;
core::Window Tile16EditorIntegrationTest::test_window_;
std::unique_ptr<gfx::IRenderer> Tile16EditorIntegrationTest::test_renderer_;

// Basic validation tests (no ROM required)
TEST_F(Tile16EditorIntegrationTest, BasicValidation) {
  // Test with invalid tile ID
  EXPECT_FALSE(editor_->IsTile16Valid(-1));
  EXPECT_FALSE(editor_->IsTile16Valid(9999));

  // Test scratch space operations with invalid slots
  auto save_invalid = editor_->SaveTile16ToScratchSpace(-1);
  EXPECT_FALSE(save_invalid.ok());
  EXPECT_EQ(save_invalid.code(), absl::StatusCode::kInvalidArgument);

  auto load_invalid = editor_->LoadTile16FromScratchSpace(5);
  EXPECT_FALSE(load_invalid.ok());
  EXPECT_EQ(load_invalid.code(), absl::StatusCode::kInvalidArgument);

  // Test valid scratch space clearing
  auto clear_valid = editor_->ClearScratchSpace(0);
  EXPECT_TRUE(clear_valid.ok());
}

// ROM-dependent tests
TEST_F(Tile16EditorIntegrationTest, ValidateTile16DataWithROM) {
#ifdef YAZE_ENABLE_ROM_TESTS
  if (!rom_loaded_) {
    GTEST_SKIP() << "ROM not loaded, skipping integration test";
  }

  // Test validation with properly loaded ROM
  auto status = editor_->ValidateTile16Data();
  EXPECT_TRUE(status.ok()) << "Validation failed: " << status.message();
#else
  GTEST_SKIP() << "ROM tests disabled";
#endif
}

TEST_F(Tile16EditorIntegrationTest, SetCurrentTileWithROM) {
#ifdef YAZE_ENABLE_ROM_TESTS
  if (!rom_loaded_) {
    GTEST_SKIP() << "ROM not loaded, skipping integration test";
  }

  // Test setting a valid tile
  auto valid_tile_result = editor_->SetCurrentTile(0);
  EXPECT_TRUE(valid_tile_result.ok())
      << "Failed to set tile 0: " << valid_tile_result.message();

  auto valid_tile_result2 = editor_->SetCurrentTile(100);
  EXPECT_TRUE(valid_tile_result2.ok())
      << "Failed to set tile 100: " << valid_tile_result2.message();

  // Test invalid ranges still fail
  auto invalid_low = editor_->SetCurrentTile(-1);
  EXPECT_FALSE(invalid_low.ok());
  EXPECT_EQ(invalid_low.code(), absl::StatusCode::kOutOfRange);

  auto invalid_high = editor_->SetCurrentTile(10000);
  EXPECT_FALSE(invalid_high.ok());
  EXPECT_EQ(invalid_high.code(), absl::StatusCode::kOutOfRange);
#else
  GTEST_SKIP() << "ROM tests disabled";
#endif
}

TEST_F(Tile16EditorIntegrationTest, FlipOperationsWithROM) {
#ifdef YAZE_ENABLE_ROM_TESTS
  if (!rom_loaded_) {
    GTEST_SKIP() << "ROM not loaded, skipping integration test";
  }

  // Set a valid tile first
  auto set_result = editor_->SetCurrentTile(1);
  ASSERT_TRUE(set_result.ok())
      << "Failed to set initial tile: " << set_result.message();

  // Test flip operations
  auto flip_h_result = editor_->FlipTile16Horizontal();
  EXPECT_TRUE(flip_h_result.ok())
      << "Horizontal flip failed: " << flip_h_result.message();

  auto flip_v_result = editor_->FlipTile16Vertical();
  EXPECT_TRUE(flip_v_result.ok())
      << "Vertical flip failed: " << flip_v_result.message();

  auto rotate_result = editor_->RotateTile16();
  EXPECT_TRUE(rotate_result.ok())
      << "Rotation failed: " << rotate_result.message();
#else
  GTEST_SKIP() << "ROM tests disabled";
#endif
}

TEST_F(Tile16EditorIntegrationTest, UndoRedoWithROM) {
#ifdef YAZE_ENABLE_ROM_TESTS
  if (!rom_loaded_) {
    GTEST_SKIP() << "ROM not loaded, skipping integration test";
  }

  // Set a tile and perform an operation to create undo state
  auto set_result = editor_->SetCurrentTile(1);
  ASSERT_TRUE(set_result.ok());

  auto clear_result = editor_->ClearTile16();
  ASSERT_TRUE(clear_result.ok())
      << "Clear operation failed: " << clear_result.message();

  // Test undo
  auto undo_result = editor_->Undo();
  EXPECT_TRUE(undo_result.ok()) << "Undo failed: " << undo_result.message();

  // Test redo
  auto redo_result = editor_->Redo();
  EXPECT_TRUE(redo_result.ok()) << "Redo failed: " << redo_result.message();
#else
  GTEST_SKIP() << "ROM tests disabled";
#endif
}

TEST_F(Tile16EditorIntegrationTest, PaletteOperationsWithROM) {
#ifdef YAZE_ENABLE_ROM_TESTS
  if (!rom_loaded_) {
    GTEST_SKIP() << "ROM not loaded, skipping integration test";
  }

  // Test palette cycling
  auto cycle_forward = editor_->CyclePalette(true);
  EXPECT_TRUE(cycle_forward.ok())
      << "Palette cycle forward failed: " << cycle_forward.message();

  auto cycle_backward = editor_->CyclePalette(false);
  EXPECT_TRUE(cycle_backward.ok())
      << "Palette cycle backward failed: " << cycle_backward.message();

  // Test valid palette preview
  auto valid_palette = editor_->PreviewPaletteChange(3);
  EXPECT_TRUE(valid_palette.ok())
      << "Palette preview failed: " << valid_palette.message();

  // Test invalid palette
  auto invalid_palette = editor_->PreviewPaletteChange(10);
  EXPECT_FALSE(invalid_palette.ok());
  EXPECT_EQ(invalid_palette.code(), absl::StatusCode::kInvalidArgument);
#else
  GTEST_SKIP() << "ROM tests disabled";
#endif
}

TEST_F(Tile16EditorIntegrationTest, CopyPasteOperationsWithROM) {
#ifdef YAZE_ENABLE_ROM_TESTS
  if (!rom_loaded_) {
    GTEST_SKIP() << "ROM not loaded, skipping integration test";
  }

  // Set a tile first
  auto set_result = editor_->SetCurrentTile(10);
  ASSERT_TRUE(set_result.ok());

  // Test copy operation
  auto copy_result = editor_->CopyTile16ToClipboard(10);
  EXPECT_TRUE(copy_result.ok()) << "Copy failed: " << copy_result.message();

  // Test paste operation
  auto paste_result = editor_->PasteTile16FromClipboard();
  EXPECT_TRUE(paste_result.ok()) << "Paste failed: " << paste_result.message();
#else
  GTEST_SKIP() << "ROM tests disabled";
#endif
}

TEST_F(Tile16EditorIntegrationTest, ScratchSpaceWithROM) {
#ifdef YAZE_ENABLE_ROM_TESTS
  if (!rom_loaded_) {
    GTEST_SKIP() << "ROM not loaded, skipping integration test";
  }

  // Set a tile first
  auto set_result = editor_->SetCurrentTile(15);
  ASSERT_TRUE(set_result.ok());

  // Test scratch space save
  auto save_result = editor_->SaveTile16ToScratchSpace(0);
  EXPECT_TRUE(save_result.ok())
      << "Scratch save failed: " << save_result.message();

  // Test scratch space load
  auto load_result = editor_->LoadTile16FromScratchSpace(0);
  EXPECT_TRUE(load_result.ok())
      << "Scratch load failed: " << load_result.message();

  // Test scratch space clear
  auto clear_result = editor_->ClearScratchSpace(0);
  EXPECT_TRUE(clear_result.ok())
      << "Scratch clear failed: " << clear_result.message();
#else
  GTEST_SKIP() << "ROM tests disabled";
#endif
}

// Palette slot calculation tests - these don't require ROM data
// Row-based addressing: (base_row + button) * 16, where base_row depends on
// the sheet group (main/aux/animated) and skips HUD rows 0-1.
TEST_F(Tile16EditorIntegrationTest, GetActualPaletteSlot_Aux1Sheets) {
  // Row-based: button 0 -> row 2 (32), button 1 -> row 3 (48), etc.
  EXPECT_EQ(editor_->GetActualPaletteSlot(0, 0), 32);   // Row 2
  EXPECT_EQ(editor_->GetActualPaletteSlot(1, 0), 48);   // Row 3
  EXPECT_EQ(editor_->GetActualPaletteSlot(2, 0), 64);   // Row 4
  EXPECT_EQ(editor_->GetActualPaletteSlot(7, 0), 144);  // Row 9

  // Sheet 3 also uses row-based (same values)
  EXPECT_EQ(editor_->GetActualPaletteSlot(0, 3), 32);
  EXPECT_EQ(editor_->GetActualPaletteSlot(4, 3), 96);   // Row 6

  // Sheet 4 also uses row-based
  EXPECT_EQ(editor_->GetActualPaletteSlot(0, 4), 32);
}

TEST_F(Tile16EditorIntegrationTest, GetActualPaletteSlot_MainSheets) {
  // Row-based addressing is consistent across all sheets
  EXPECT_EQ(editor_->GetActualPaletteSlot(0, 1), 32);   // Row 2
  EXPECT_EQ(editor_->GetActualPaletteSlot(1, 1), 48);   // Row 3
  EXPECT_EQ(editor_->GetActualPaletteSlot(7, 1), 144);  // Row 9

  // Sheet 2 uses same row-based values
  EXPECT_EQ(editor_->GetActualPaletteSlot(0, 2), 32);
}

TEST_F(Tile16EditorIntegrationTest, GetActualPaletteSlot_Aux2Sheets) {
  // AUX2 sheets use base row 5
  EXPECT_EQ(editor_->GetActualPaletteSlot(0, 5), 80);   // Row 5
  EXPECT_EQ(editor_->GetActualPaletteSlot(1, 5), 96);   // Row 6
  EXPECT_EQ(editor_->GetActualPaletteSlot(7, 5), 192);  // Row 12

  // Sheet 6 uses same values (AUX2)
  EXPECT_EQ(editor_->GetActualPaletteSlot(0, 6), 80);
}

TEST_F(Tile16EditorIntegrationTest, GetActualPaletteSlot_AnimatedSheet) {
  // Animated sheet uses base row 7
  EXPECT_EQ(editor_->GetActualPaletteSlot(0, 7), 112);  // Row 7
  EXPECT_EQ(editor_->GetActualPaletteSlot(1, 7), 128);  // Row 8
  EXPECT_EQ(editor_->GetActualPaletteSlot(7, 7), 224);  // Row 14
}

TEST_F(Tile16EditorIntegrationTest, GetSheetIndexForTile8_BoundsCheck) {
  // 256 tiles per sheet
  EXPECT_EQ(editor_->GetSheetIndexForTile8(0), 0);
  EXPECT_EQ(editor_->GetSheetIndexForTile8(255), 0);
  EXPECT_EQ(editor_->GetSheetIndexForTile8(256), 1);
  EXPECT_EQ(editor_->GetSheetIndexForTile8(511), 1);
  EXPECT_EQ(editor_->GetSheetIndexForTile8(512), 2);
  EXPECT_EQ(editor_->GetSheetIndexForTile8(1792), 7);  // 7 * 256 = 1792
  EXPECT_EQ(editor_->GetSheetIndexForTile8(2047), 7);  // Max clamped to 7
  EXPECT_EQ(editor_->GetSheetIndexForTile8(3000), 7);  // Beyond max still 7
}

TEST_F(Tile16EditorIntegrationTest, PaletteAccessors) {
  // Test initial palette value
  int initial = editor_->current_palette();
  EXPECT_GE(initial, 0);
  EXPECT_LE(initial, 7);

  // Test setting palette
  editor_->set_current_palette(5);
  EXPECT_EQ(editor_->current_palette(), 5);

  // Test clamping
  editor_->set_current_palette(-1);
  EXPECT_EQ(editor_->current_palette(), 0);

  editor_->set_current_palette(10);
  EXPECT_EQ(editor_->current_palette(), 7);
}

// Navigation tests - use SetCurrentTile which returns absl::Status
TEST_F(Tile16EditorIntegrationTest, NavigationBoundsCheck_InvalidTile) {
  // Setting tile -1 should fail
  auto status = editor_->SetCurrentTile(-1);
  EXPECT_FALSE(status.ok());
  EXPECT_EQ(status.code(), absl::StatusCode::kOutOfRange);

  // Setting tile beyond max should fail
  status = editor_->SetCurrentTile(10000);
  EXPECT_FALSE(status.ok());
  EXPECT_EQ(status.code(), absl::StatusCode::kOutOfRange);
}

TEST_F(Tile16EditorIntegrationTest, NavigationBoundsCheck_ValidRange) {
#ifdef YAZE_ENABLE_ROM_TESTS
  if (!rom_loaded_) {
    GTEST_SKIP() << "ROM not loaded, skipping integration test";
  }

  // Setting valid tiles should succeed (requires ROM for bitmap operations)
  auto status = editor_->SetCurrentTile(0);
  EXPECT_TRUE(status.ok()) << status.message();
  EXPECT_EQ(editor_->current_tile16(), 0);

  status = editor_->SetCurrentTile(100);
  EXPECT_TRUE(status.ok()) << status.message();
  EXPECT_EQ(editor_->current_tile16(), 100);
#else
  GTEST_SKIP() << "ROM tests disabled";
#endif
}

}  // namespace test
}  // namespace editor
}  // namespace yaze
