#include "app/gfx/types/snes_color.h"

#include <gtest/gtest.h>

#include "imgui/imgui.h"

namespace yaze {
namespace gfx {
namespace {

// Test fixture for SnesColor tests
class SnesColorTest : public ::testing::Test {
 protected:
  void SetUp() override {
    // Common setup if needed
  }
};

// ============================================================================
// RGB Format Conversion Tests
// ============================================================================

TEST_F(SnesColorTest, SetRgbFromImGuiNormalizedValues) {
  SnesColor color;

  // ImGui ColorPicker returns values in 0-1 range
  ImVec4 imgui_color(0.5f, 0.75f, 1.0f, 1.0f);
  color.set_rgb(imgui_color);

  // Internal storage should be in 0-255 range
  auto rgb = color.rgb();
  EXPECT_FLOAT_EQ(rgb.x, 127.5f);  // 0.5 * 255
  EXPECT_FLOAT_EQ(rgb.y, 191.25f);  // 0.75 * 255
  EXPECT_FLOAT_EQ(rgb.z, 255.0f);   // 1.0 * 255
  EXPECT_FLOAT_EQ(rgb.w, 255.0f);   // Alpha always 255
}

TEST_F(SnesColorTest, SetRgbBlackColor) {
  SnesColor color;

  ImVec4 black(0.0f, 0.0f, 0.0f, 1.0f);
  color.set_rgb(black);

  auto rgb = color.rgb();
  EXPECT_FLOAT_EQ(rgb.x, 0.0f);
  EXPECT_FLOAT_EQ(rgb.y, 0.0f);
  EXPECT_FLOAT_EQ(rgb.z, 0.0f);
  EXPECT_FLOAT_EQ(rgb.w, 255.0f);
}

TEST_F(SnesColorTest, SetRgbWhiteColor) {
  SnesColor color;

  ImVec4 white(1.0f, 1.0f, 1.0f, 1.0f);
  color.set_rgb(white);

  auto rgb = color.rgb();
  EXPECT_FLOAT_EQ(rgb.x, 255.0f);
  EXPECT_FLOAT_EQ(rgb.y, 255.0f);
  EXPECT_FLOAT_EQ(rgb.z, 255.0f);
  EXPECT_FLOAT_EQ(rgb.w, 255.0f);
}

TEST_F(SnesColorTest, SetRgbMidRangeColor) {
  SnesColor color;

  // Test a mid-range color (medium gray)
  ImVec4 gray(0.5f, 0.5f, 0.5f, 1.0f);
  color.set_rgb(gray);

  auto rgb = color.rgb();
  EXPECT_NEAR(rgb.x, 127.5f, 0.01f);
  EXPECT_NEAR(rgb.y, 127.5f, 0.01f);
  EXPECT_NEAR(rgb.z, 127.5f, 0.01f);
}

// ============================================================================
// Constructor Tests
// ============================================================================

TEST_F(SnesColorTest, ConstructFromImVec4) {
  // ImGui color in 0-1 range
  ImVec4 imgui_color(0.25f, 0.5f, 0.75f, 1.0f);

  SnesColor color(imgui_color);

  // Should be converted to 0-255 range
  auto rgb = color.rgb();
  EXPECT_NEAR(rgb.x, 63.75f, 0.01f);   // 0.25 * 255
  EXPECT_NEAR(rgb.y, 127.5f, 0.01f);   // 0.5 * 255
  EXPECT_NEAR(rgb.z, 191.25f, 0.01f);  // 0.75 * 255
  EXPECT_FLOAT_EQ(rgb.w, 255.0f);
}

TEST_F(SnesColorTest, ConstructFromSnesValue) {
  // SNES BGR555 format: 0x7FFF = white (all bits set in 15-bit color)
  SnesColor white(0x7FFF);

  auto rgb = white.rgb();
  // All channels should be max (after BGR555 conversion)
  EXPECT_GT(rgb.x, 240.0f);  // Close to 255
  EXPECT_GT(rgb.y, 240.0f);
  EXPECT_GT(rgb.z, 240.0f);
}

TEST_F(SnesColorTest, ConstructFromSnesBlack) {
  // SNES BGR555 format: 0x0000 = black
  SnesColor black(0x0000);

  auto rgb = black.rgb();
  EXPECT_FLOAT_EQ(rgb.x, 0.0f);
  EXPECT_FLOAT_EQ(rgb.y, 0.0f);
  EXPECT_FLOAT_EQ(rgb.z, 0.0f);
}

// ============================================================================
// SNES Format Conversion Tests
// ============================================================================

TEST_F(SnesColorTest, SetSnesUpdatesRgb) {
  SnesColor color;

  // Set a SNES color value
  color.set_snes(0x7FFF);  // White in BGR555

  // RGB should be updated
  auto rgb = color.rgb();
  EXPECT_GT(rgb.x, 240.0f);
  EXPECT_GT(rgb.y, 240.0f);
  EXPECT_GT(rgb.z, 240.0f);
}

TEST_F(SnesColorTest, RgbToSnesConversion) {
  SnesColor color;

  // Set pure red in RGB (0-1 range for ImGui)
  ImVec4 red(1.0f, 0.0f, 0.0f, 1.0f);
  color.set_rgb(red);

  // SNES value should be set (BGR555 format)
  uint16_t snes = color.snes();
  EXPECT_NE(snes, 0x0000);  // Should not be black

  // Extract red component from BGR555 (bits 0-4)
  uint16_t snes_red = snes & 0x1F;
  EXPECT_EQ(snes_red, 0x1F);  // Max red in 5-bit
}

// ============================================================================
// Round-Trip Conversion Tests
// ============================================================================

TEST_F(SnesColorTest, RoundTripImGuiToSnesColor) {
  // Start with ImGui color
  ImVec4 original(0.6f, 0.4f, 0.8f, 1.0f);

  // Convert to SnesColor
  SnesColor color(original);

  // Convert back to ImVec4 (normalized)
  auto rgb = color.rgb();
  ImVec4 converted(rgb.x / 255.0f, rgb.y / 255.0f, rgb.z / 255.0f, 1.0f);

  // Should be approximately equal (within floating point precision)
  EXPECT_NEAR(converted.x, original.x, 0.01f);
  EXPECT_NEAR(converted.y, original.y, 0.01f);
  EXPECT_NEAR(converted.z, original.z, 0.01f);
}

TEST_F(SnesColorTest, MultipleSetRgbCalls) {
  SnesColor color;

  // First color
  ImVec4 color1(0.2f, 0.4f, 0.6f, 1.0f);
  color.set_rgb(color1);

  auto rgb1 = color.rgb();
  EXPECT_NEAR(rgb1.x, 51.0f, 1.0f);
  EXPECT_NEAR(rgb1.y, 102.0f, 1.0f);
  EXPECT_NEAR(rgb1.z, 153.0f, 1.0f);

  // Second color (should completely replace)
  ImVec4 color2(0.8f, 0.6f, 0.4f, 1.0f);
  color.set_rgb(color2);

  auto rgb2 = color.rgb();
  EXPECT_NEAR(rgb2.x, 204.0f, 1.0f);
  EXPECT_NEAR(rgb2.y, 153.0f, 1.0f);
  EXPECT_NEAR(rgb2.z, 102.0f, 1.0f);
}

// ============================================================================
// Edge Case Tests
// ============================================================================

TEST_F(SnesColorTest, HandlesMaxValues) {
  SnesColor color;

  ImVec4 max(1.0f, 1.0f, 1.0f, 1.0f);
  color.set_rgb(max);

  auto rgb = color.rgb();
  EXPECT_FLOAT_EQ(rgb.x, 255.0f);
  EXPECT_FLOAT_EQ(rgb.y, 255.0f);
  EXPECT_FLOAT_EQ(rgb.z, 255.0f);
}

TEST_F(SnesColorTest, HandlesMinValues) {
  SnesColor color;

  ImVec4 min(0.0f, 0.0f, 0.0f, 1.0f);
  color.set_rgb(min);

  auto rgb = color.rgb();
  EXPECT_FLOAT_EQ(rgb.x, 0.0f);
  EXPECT_FLOAT_EQ(rgb.y, 0.0f);
  EXPECT_FLOAT_EQ(rgb.z, 0.0f);
}

TEST_F(SnesColorTest, AlphaAlwaysMaximum) {
  SnesColor color;

  // Try setting alpha to different values (should always be ignored)
  ImVec4 color_with_alpha(0.5f, 0.5f, 0.5f, 0.5f);
  color.set_rgb(color_with_alpha);

  auto rgb = color.rgb();
  EXPECT_FLOAT_EQ(rgb.w, 255.0f);  // Alpha should always be 255
}

// ============================================================================
// Modified Flag Tests
// ============================================================================

TEST_F(SnesColorTest, ModifiedFlagSetOnRgbChange) {
  SnesColor color;

  EXPECT_FALSE(color.is_modified());

  ImVec4 new_color(0.5f, 0.5f, 0.5f, 1.0f);
  color.set_rgb(new_color);

  EXPECT_TRUE(color.is_modified());
}

TEST_F(SnesColorTest, ModifiedFlagSetOnSnesChange) {
  SnesColor color;

  EXPECT_FALSE(color.is_modified());

  color.set_snes(0x7FFF);

  EXPECT_TRUE(color.is_modified());
}

}  // namespace
}  // namespace gfx
}  // namespace yaze
