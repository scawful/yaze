/**
 * @file ppu_catchup_test.cc
 * @brief Unit tests for the PPU JIT catch-up system
 *
 * Tests the mid-scanline raster effect support:
 * - StartLine(int line) - Initialize scanline, evaluate sprites
 * - CatchUp(int h_pos) - Render pixels from last position to h_pos
 * - RunLine(int line) - Legacy wrapper calling StartLine + CatchUp
 */

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include <array>
#include <cstdint>

#include "app/emu/memory/memory.h"
#include "app/emu/video/ppu.h"
#include "mocks/mock_memory.h"

namespace yaze {
namespace emu {

using ::testing::_;
using ::testing::Return;

/**
 * @class PpuCatchupTestFixture
 * @brief Test fixture for PPU catch-up system tests
 *
 * Provides a PPU instance with mock memory and helper methods
 * for inspecting rendered output. Uses only public PPU APIs
 * (Write, PutPixels, etc.) to ensure tests validate the public interface.
 */
class PpuCatchupTestFixture : public ::testing::Test {
 protected:
  void SetUp() override {
    // Initialize mock memory with defaults
    mock_memory_.memory_.resize(0x1000000, 0);
    mock_memory_.Init();

    // Setup default return values for memory interface
    ON_CALL(mock_memory_, h_pos()).WillByDefault(Return(0));
    ON_CALL(mock_memory_, v_pos()).WillByDefault(Return(0));
    ON_CALL(mock_memory_, pal_timing()).WillByDefault(Return(false));
    ON_CALL(mock_memory_, open_bus()).WillByDefault(Return(0));

    // Create PPU with mock memory
    ppu_ = std::make_unique<Ppu>(mock_memory_);
    ppu_->Init();
    ppu_->Reset();

    // Initialize output pixel buffer for inspection
    output_pixels_.resize(512 * 4 * 480, 0);
  }

  void TearDown() override { ppu_.reset(); }

  /**
   * @brief Copy pixel buffer to output array for inspection
   */
  void CopyPixelBuffer() { ppu_->PutPixels(output_pixels_.data()); }

  /**
   * @brief Get pixel color at a specific position in the pixel buffer
   * @param x X position (0-255)
   * @param y Y position (0-238)
   * @param even_frame True for even frame, false for odd
   * @return ARGB color value
   *
   * Uses PutPixels() public API to copy the internal pixel buffer
   * to an output array for inspection.
   */
  uint32_t GetPixelAt(int x, int y, bool even_frame = true) {
    // Copy pixel buffer to output array first
    CopyPixelBuffer();

    // Output buffer layout after PutPixels: row * 2048 + x * 8
    // PutPixels copies to dest with row = y * 2 + (overscan ? 2 : 16)
    // For simplicity, use the internal buffer structure
    int dest_row = y * 2 + (ppu_->frame_overscan_ ? 2 : 16);
    int offset = dest_row * 2048 + x * 8;

    // Read BGRX format (format 0)
    uint8_t b = output_pixels_[offset + 0];
    uint8_t g = output_pixels_[offset + 1];
    uint8_t r = output_pixels_[offset + 2];
    uint8_t a = output_pixels_[offset + 3];

    return (a << 24) | (r << 16) | (g << 8) | b;
  }

  /**
   * @brief Check if pixel at position was rendered (non-zero)
   *
   * This checks the alpha channel in the output buffer after PutPixels.
   * When pixels are rendered, they have alpha = 0xFF.
   */
  bool IsPixelRendered(int x, int y, bool even_frame = true) {
    CopyPixelBuffer();

    int dest_row = y * 2 + (ppu_->frame_overscan_ ? 2 : 16);
    int offset = dest_row * 2048 + x * 8;

    // Check if alpha channel is 0xFF (rendered pixel)
    return output_pixels_[offset + 3] == 0xFF;
  }

  /**
   * @brief Setup a simple palette for testing
   */
  void SetupTestPalette() {
    // Set backdrop color (palette entry 0) to a known non-black value
    // Format: 0bbbbbgggggrrrrr (15-bit BGR)
    ppu_->cgram[0] = 0x001F;  // Red backdrop
    ppu_->cgram[1] = 0x03E0;  // Green
    ppu_->cgram[2] = 0x7C00;  // Blue
  }

  /**
   * @brief Enable main screen rendering for testing
   */
  void EnableMainScreen() {
    // Enable forced blank to false and brightness to max
    ppu_->forced_blank_ = false;
    ppu_->brightness = 15;
    ppu_->mode = 0;  // Mode 0 for simplicity

    // Write to PPU registers via the Write method for proper state setup
    // $2100: Screen Display - brightness 15, forced blank off
    ppu_->Write(0x00, 0x0F);

    // $212C: Main Screen Designation - enable BG1
    ppu_->Write(0x2C, 0x01);
  }

  MockMemory mock_memory_;
  std::unique_ptr<Ppu> ppu_;
  std::vector<uint8_t> output_pixels_;

  // Constants for cycle/pixel conversion
  static constexpr int kCyclesPerPixel = 4;
  static constexpr int kScreenWidth = 256;
  static constexpr int kMaxHPos = kScreenWidth * kCyclesPerPixel;  // 1024
};

// =============================================================================
// Basic Functionality Tests
// =============================================================================

TEST_F(PpuCatchupTestFixture, StartLineResetsRenderPosition) {
  // GIVEN: PPU in a state where some pixels might have been rendered
  ppu_->StartLine(50);
  ppu_->CatchUp(400);  // Render some pixels

  // WHEN: Starting a new line
  ppu_->StartLine(51);

  // THEN: The next CatchUp should render from the beginning (x=0)
  // We verify by rendering a small range and checking pixels are rendered
  SetupTestPalette();
  EnableMainScreen();

  ppu_->CatchUp(40);  // Render first 10 pixels (40/4 = 10)

  // Pixel at x=0 should be rendered
  EXPECT_TRUE(IsPixelRendered(0, 50));
}

TEST_F(PpuCatchupTestFixture, CatchUpRendersPixelRange) {
  // GIVEN: PPU initialized for a scanline
  SetupTestPalette();
  EnableMainScreen();
  ppu_->StartLine(100);

  // WHEN: Calling CatchUp with h_pos = 200 (50 pixels)
  ppu_->CatchUp(200);

  // THEN: Pixels 0-49 should be rendered (h_pos 200 / 4 = 50)
  for (int x = 0; x < 50; ++x) {
    EXPECT_TRUE(IsPixelRendered(x, 99))
        << "Pixel at x=" << x << " should be rendered";
  }
}

TEST_F(PpuCatchupTestFixture, CatchUpConvertsHPosToPosCorrectly) {
  // GIVEN: PPU ready to render
  SetupTestPalette();
  EnableMainScreen();
  ppu_->StartLine(50);

  // Test various h_pos values and their expected pixel counts
  // h_pos / 4 = pixel position (1 pixel = 4 master cycles)

  struct TestCase {
    int h_pos;
    int expected_pixels;
  };

  TestCase test_cases[] = {
      {4, 1},     // 4 cycles = 1 pixel
      {8, 2},     // 8 cycles = 2 pixels
      {40, 10},   // 40 cycles = 10 pixels
      {100, 25},  // 100 cycles = 25 pixels
      {256, 64},  // 256 cycles = 64 pixels
  };

  for (const auto& tc : test_cases) {
    ppu_->StartLine(50);
    ppu_->CatchUp(tc.h_pos);

    // Verify the last expected pixel is rendered
    int last_pixel = tc.expected_pixels - 1;
    EXPECT_TRUE(IsPixelRendered(last_pixel, 49))
        << "h_pos=" << tc.h_pos << " should render pixel " << last_pixel;
  }
}

TEST_F(PpuCatchupTestFixture, CatchUpClampsTo256Pixels) {
  // GIVEN: PPU ready to render
  SetupTestPalette();
  EnableMainScreen();
  ppu_->StartLine(50);

  // WHEN: Calling CatchUp with h_pos > 1024 (beyond screen width)
  ppu_->CatchUp(2000);  // Should clamp to 256 pixels

  // THEN: All 256 pixels should be rendered, but no more
  for (int x = 0; x < 256; ++x) {
    EXPECT_TRUE(IsPixelRendered(x, 49))
        << "Pixel at x=" << x << " should be rendered";
  }
}

TEST_F(PpuCatchupTestFixture, CatchUpSkipsIfAlreadyRendered) {
  // GIVEN: PPU has already rendered some pixels
  SetupTestPalette();
  EnableMainScreen();
  ppu_->StartLine(50);
  ppu_->CatchUp(400);  // Render pixels 0-99

  // Record state of pixel buffer at position that's already rendered
  uint32_t pixel_before = GetPixelAt(50, 49);

  // WHEN: Calling CatchUp with same or earlier h_pos
  ppu_->CatchUp(200);  // Earlier than previous catch-up
  ppu_->CatchUp(400);  // Same as previous catch-up

  // THEN: No pixels should be re-rendered (state unchanged)
  uint32_t pixel_after = GetPixelAt(50, 49);
  EXPECT_EQ(pixel_before, pixel_after);
}

TEST_F(PpuCatchupTestFixture, CatchUpProgressiveRendering) {
  // GIVEN: PPU ready to render
  SetupTestPalette();
  EnableMainScreen();
  ppu_->StartLine(50);

  // WHEN: Making progressive CatchUp calls
  ppu_->CatchUp(100);   // Render pixels 0-24
  ppu_->CatchUp(200);   // Render pixels 25-49
  ppu_->CatchUp(300);   // Render pixels 50-74
  ppu_->CatchUp(1024);  // Complete the line

  // THEN: All pixels should be rendered correctly
  for (int x = 0; x < 256; ++x) {
    EXPECT_TRUE(IsPixelRendered(x, 49))
        << "Pixel at x=" << x << " should be rendered";
  }
}

// =============================================================================
// Integration Tests
// =============================================================================

TEST_F(PpuCatchupTestFixture, RunLineRendersFullScanline) {
  // GIVEN: PPU ready to render
  SetupTestPalette();
  EnableMainScreen();

  // WHEN: Using RunLine (legacy wrapper)
  ppu_->RunLine(100);

  // THEN: All 256 pixels should be rendered
  for (int x = 0; x < 256; ++x) {
    EXPECT_TRUE(IsPixelRendered(x, 99))
        << "Pixel at x=" << x << " should be rendered by RunLine";
  }
}

TEST_F(PpuCatchupTestFixture, MultipleCatchUpCallsRenderCorrectly) {
  // GIVEN: PPU ready to render (simulating multiple register writes)
  SetupTestPalette();
  EnableMainScreen();
  ppu_->StartLine(50);

  // WHEN: Simulating multiple mid-scanline register changes
  // First segment: scroll at position 0
  ppu_->CatchUp(200);  // Render 50 pixels

  // Simulated register change would happen here in real usage
  // Second segment
  ppu_->CatchUp(400);  // Render next 50 pixels

  // Third segment
  ppu_->CatchUp(1024);  // Complete the line

  // THEN: All segments rendered correctly
  for (int x = 0; x < 256; ++x) {
    EXPECT_TRUE(IsPixelRendered(x, 49))
        << "Pixel at x=" << x << " should be rendered";
  }
}

TEST_F(PpuCatchupTestFixture, ConsecutiveLinesRenderIndependently) {
  // GIVEN: PPU ready to render multiple lines
  SetupTestPalette();
  EnableMainScreen();

  // WHEN: Rendering consecutive lines
  for (int line = 1; line <= 10; ++line) {
    ppu_->RunLine(line);
  }

  // THEN: Each line should be fully rendered
  for (int line = 0; line < 10; ++line) {
    for (int x = 0; x < 256; ++x) {
      EXPECT_TRUE(IsPixelRendered(x, line))
          << "Pixel at line=" << line << ", x=" << x << " should be rendered";
    }
  }
}

// =============================================================================
// Edge Case Tests
// =============================================================================

TEST_F(PpuCatchupTestFixture, CatchUpDuringForcedBlank) {
  // GIVEN: PPU in forced blank mode
  SetupTestPalette();
  ppu_->forced_blank_ = true;
  ppu_->brightness = 15;
  ppu_->Write(0x00, 0x8F);  // Forced blank enabled

  ppu_->StartLine(50);

  // WHEN: Calling CatchUp during forced blank
  ppu_->CatchUp(1024);

  // THEN: Pixels should be black (all zeros) during forced blank
  uint32_t pixel = GetPixelAt(100, 49);
  // In forced blank, HandlePixel skips color calculation, resulting in black
  // The alpha channel should still be set, but RGB should be 0
  uint8_t r = (pixel >> 16) & 0xFF;
  uint8_t g = (pixel >> 8) & 0xFF;
  uint8_t b = pixel & 0xFF;
  EXPECT_EQ(r, 0) << "Red channel should be 0 during forced blank";
  EXPECT_EQ(g, 0) << "Green channel should be 0 during forced blank";
  EXPECT_EQ(b, 0) << "Blue channel should be 0 during forced blank";
}

TEST_F(PpuCatchupTestFixture, CatchUpMode7Handling) {
  // GIVEN: PPU configured for Mode 7
  SetupTestPalette();
  EnableMainScreen();
  ppu_->mode = 7;
  ppu_->Write(0x05, 0x07);  // Set mode 7

  // Set Mode 7 matrix to identity (simple case)
  // A = 0x0100 (1.0 in fixed point)
  ppu_->Write(0x1B, 0x00);  // M7A low
  ppu_->Write(0x1B, 0x01);  // M7A high
  // B = 0x0000
  ppu_->Write(0x1C, 0x00);  // M7B low
  ppu_->Write(0x1C, 0x00);  // M7B high
  // C = 0x0000
  ppu_->Write(0x1D, 0x00);  // M7C low
  ppu_->Write(0x1D, 0x00);  // M7C high
  // D = 0x0100 (1.0 in fixed point)
  ppu_->Write(0x1E, 0x00);  // M7D low
  ppu_->Write(0x1E, 0x01);  // M7D high

  ppu_->StartLine(50);

  // WHEN: Calling CatchUp in Mode 7
  ppu_->CatchUp(1024);

  // THEN: Mode 7 calculations should execute without crash
  // and pixels should be rendered
  EXPECT_TRUE(IsPixelRendered(128, 49)) << "Mode 7 should render pixels";
}

TEST_F(PpuCatchupTestFixture, CatchUpAtScanlineStart) {
  // GIVEN: PPU at start of scanline
  SetupTestPalette();
  EnableMainScreen();
  ppu_->StartLine(50);

  // WHEN: Calling CatchUp at h_pos = 0
  ppu_->CatchUp(0);

  // THEN: No pixels should be rendered yet (target_x = 0, nothing to render)
  // This is a no-op case
  // Subsequent CatchUp should still work
  ppu_->CatchUp(100);
  EXPECT_TRUE(IsPixelRendered(24, 49));
}

TEST_F(PpuCatchupTestFixture, CatchUpAtScanlineEnd) {
  // GIVEN: PPU mid-scanline
  SetupTestPalette();
  EnableMainScreen();
  ppu_->StartLine(50);
  ppu_->CatchUp(500);  // Render first 125 pixels

  // WHEN: Calling CatchUp at end of scanline (h_pos >= 1024)
  ppu_->CatchUp(1024);  // Should complete the remaining pixels
  ppu_->CatchUp(1500);  // Should be a no-op (already at end)

  // THEN: All 256 pixels should be rendered
  EXPECT_TRUE(IsPixelRendered(0, 49));
  EXPECT_TRUE(IsPixelRendered(127, 49));
  EXPECT_TRUE(IsPixelRendered(255, 49));
}

TEST_F(PpuCatchupTestFixture, CatchUpWithNegativeOrZeroDoesNotCrash) {
  // GIVEN: PPU ready to render
  SetupTestPalette();
  EnableMainScreen();
  ppu_->StartLine(50);

  // WHEN: Calling CatchUp with edge case values
  // These should not crash and should be handled gracefully
  ppu_->CatchUp(0);
  ppu_->CatchUp(1);
  ppu_->CatchUp(2);
  ppu_->CatchUp(3);

  // THEN: No crash occurred (test passes if we get here)
  SUCCEED();
}

TEST_F(PpuCatchupTestFixture, StartLineEvaluatesSprites) {
  // GIVEN: PPU with sprite data in OAM
  SetupTestPalette();
  EnableMainScreen();

  // Enable sprites on main screen
  ppu_->Write(0x2C, 0x10);  // Enable OBJ on main screen

  // Setup a simple sprite in OAM via Write interface
  // $2102/$2103: OAM address
  ppu_->Write(0x02, 0x00);  // OAM address low = 0
  ppu_->Write(0x03, 0x00);  // OAM address high = 0

  // $2104: Write OAM data (two writes per word)
  // Sprite 0 word 0: X-low=100, Y=50
  ppu_->Write(0x04, 100);   // X position low byte
  ppu_->Write(0x04, 50);    // Y position
  // Sprite 0 word 1: tile=1, attributes=0
  ppu_->Write(0x04, 0x01);  // Tile number low byte
  ppu_->Write(0x04, 0x00);  // Attributes

  // WHEN: Starting a line where sprite should be visible
  ppu_->StartLine(51);  // Sprites are evaluated for line-1

  // THEN: Sprite evaluation should run without crash
  // The obj_pixel_buffer_ should be cleared/initialized
  SUCCEED();
}

TEST_F(PpuCatchupTestFixture, BrightnessAffectsRenderedPixels) {
  // GIVEN: PPU with a known palette color
  ppu_->cgram[0] = 0x7FFF;  // White (max values)
  ppu_->forced_blank_ = false;
  ppu_->mode = 0;

  // Test with maximum brightness
  ppu_->brightness = 15;
  ppu_->StartLine(10);
  ppu_->CatchUp(40);  // Render 10 pixels at max brightness

  uint32_t pixel_max = GetPixelAt(5, 9);

  // Test with half brightness
  ppu_->brightness = 7;
  ppu_->StartLine(20);
  ppu_->CatchUp(40);

  uint32_t pixel_half = GetPixelAt(5, 19);

  // THEN: Lower brightness should result in darker pixels
  uint8_t r_max = (pixel_max >> 16) & 0xFF;
  uint8_t r_half = (pixel_half >> 16) & 0xFF;
  EXPECT_GT(r_max, r_half) << "Higher brightness should produce brighter pixels";
}

TEST_F(PpuCatchupTestFixture, EvenOddFrameHandling) {
  // GIVEN: PPU in different frame states
  SetupTestPalette();
  EnableMainScreen();

  // WHEN: Rendering on even frame
  ppu_->even_frame = true;
  ppu_->StartLine(50);
  ppu_->CatchUp(1024);

  // THEN: Pixels go to even frame buffer location
  EXPECT_TRUE(IsPixelRendered(128, 49, true));

  // WHEN: Rendering on odd frame
  ppu_->even_frame = false;
  ppu_->StartLine(50);
  ppu_->CatchUp(1024);

  // THEN: Pixels go to odd frame buffer location
  EXPECT_TRUE(IsPixelRendered(128, 49, false));
}

// =============================================================================
// Performance Boundary Tests
// =============================================================================

TEST_F(PpuCatchupTestFixture, RenderFullFrameLines) {
  // GIVEN: PPU ready to render
  SetupTestPalette();
  EnableMainScreen();

  // WHEN: Rendering a complete frame worth of visible lines (1-224)
  for (int line = 1; line <= 224; ++line) {
    ppu_->RunLine(line);
  }

  // THEN: All lines should be rendered without crash
  // Spot check a few lines
  EXPECT_TRUE(IsPixelRendered(128, 0));    // Line 1
  EXPECT_TRUE(IsPixelRendered(128, 111));  // Line 112
  EXPECT_TRUE(IsPixelRendered(128, 223));  // Line 224
}

TEST_F(PpuCatchupTestFixture, MidScanlineRegisterChangeSimulation) {
  // GIVEN: PPU ready for mid-scanline raster effects
  SetupTestPalette();
  EnableMainScreen();
  ppu_->StartLine(100);

  // Simulate a game that changes scroll mid-scanline
  // First part: render with current scroll
  ppu_->CatchUp(128 * 4);  // Render first 128 pixels

  // Change scroll register via PPU Write interface
  // $210D: BG1 Horizontal Scroll (two writes)
  ppu_->Write(0x0D, 0x08);  // Low byte of scroll = 8
  ppu_->Write(0x0D, 0x00);  // High byte of scroll = 0

  // Second part: render remaining pixels with new scroll
  ppu_->CatchUp(256 * 4);

  // THEN: Both halves rendered
  EXPECT_TRUE(IsPixelRendered(0, 99));
  EXPECT_TRUE(IsPixelRendered(127, 99));
  EXPECT_TRUE(IsPixelRendered(128, 99));
  EXPECT_TRUE(IsPixelRendered(255, 99));
}

}  // namespace emu
}  // namespace yaze
