// Pins the GenerateThemeFromAccent contract used by Phase 5.5a/b/c preset
// authoring. If someone rewrites GenerateThemeFromAccent, this test is the
// canary — without it, new `.theme` files might silently change semantics
// (e.g., "dark mode" background going pale, or error red drifting per accent).
#include "app/gui/core/color.h"
#include "app/gui/core/theme_manager.h"
#include "gtest/gtest.h"
#include "imgui/imgui.h"

namespace yaze::gui {
namespace {

// GenerateThemeFromAccent uses ImGui math helpers indirectly through
// ApplySmartDefaults; construct a headless context so the function path is
// well-defined.
class ThemeGeneratorTest : public ::testing::Test {
 protected:
  void SetUp() override {
    imgui_context_ = ImGui::CreateContext();
    ImGui::SetCurrentContext(imgui_context_);
    ImGui::GetIO().DeltaTime = 1.0f / 60.0f;
    ImGui::GetIO().DisplaySize = ImVec2(1280.0f, 720.0f);
  }

  void TearDown() override {
    if (imgui_context_ != nullptr) {
      ImGui::DestroyContext(imgui_context_);
      imgui_context_ = nullptr;
    }
  }

  ImGuiContext* imgui_context_ = nullptr;

  static float Luminance(const Color& c) {
    // Rough relative luminance. Used only for dark/light classification.
    return 0.2126f * c.red + 0.7152f * c.green + 0.0722f * c.blue;
  }
};

TEST_F(ThemeGeneratorTest, DarkModeProducesDarkBackground) {
  // Any accent in dark mode → background luminance should be well below 0.5.
  // This is the contract Phase 5.5 presets lean on: "dark_mode=true" means
  // dark bg regardless of accent hue.
  Color purple{0.5f, 0.25f, 0.75f, 1.0f};
  Theme t = ThemeManager::Get().GenerateThemeFromAccent(purple, true);
  EXPECT_LT(Luminance(t.background), 0.3f)
      << "dark_mode=true produced a light background "
      << "(luminance=" << Luminance(t.background) << ")";
}

TEST_F(ThemeGeneratorTest, LightModeProducesLightBackground) {
  Color cyan{0.2f, 0.7f, 0.85f, 1.0f};
  Theme t = ThemeManager::Get().GenerateThemeFromAccent(cyan, false);
  EXPECT_GT(Luminance(t.background), 0.7f)
      << "dark_mode=false produced a dark background "
      << "(luminance=" << Luminance(t.background) << ")";
}

TEST_F(ThemeGeneratorTest, StatusColorsAreStableAcrossAccents) {
  // error/warning/success/info must stay recognizably red/yellow/green/blue
  // regardless of accent, otherwise a cyberpunk-accented theme turns "error"
  // into something that doesn't read as error.
  Color purple{0.5f, 0.25f, 0.75f, 1.0f};
  Color cyan{0.1f, 0.7f, 0.85f, 1.0f};

  Theme tp = ThemeManager::Get().GenerateThemeFromAccent(purple, true);
  Theme tc = ThemeManager::Get().GenerateThemeFromAccent(cyan, true);

  // Error dominates red channel.
  EXPECT_GT(tp.error.red, tp.error.green) << "purple-accent error isn't red";
  EXPECT_GT(tc.error.red, tc.error.green) << "cyan-accent error isn't red";

  // Success dominates green channel.
  EXPECT_GT(tp.success.green, tp.success.red)
      << "purple-accent success isn't green";
  EXPECT_GT(tc.success.green, tc.success.red)
      << "cyan-accent success isn't green";

  // Warning is yellow-ish: red and green both high, blue low.
  EXPECT_GT(tp.warning.red, tp.warning.blue);
  EXPECT_GT(tp.warning.green, tp.warning.blue);

  // Info dominates blue channel (or is at least bluer than red).
  EXPECT_GT(tp.info.blue, tp.info.red) << "purple-accent info isn't blue";
  EXPECT_GT(tc.info.blue, tc.info.red) << "cyan-accent info isn't blue";
}

TEST_F(ThemeGeneratorTest, GeneratedThemeIsNonEmpty) {
  // Sanity: all color fields should be set (not zero-initialized).
  Color accent{0.4f, 0.6f, 0.9f, 1.0f};
  Theme t = ThemeManager::Get().GenerateThemeFromAccent(accent, true);

  auto IsNonZero = [](const Color& c) {
    return c.red + c.green + c.blue > 0.0f;
  };

  EXPECT_TRUE(IsNonZero(t.primary));
  EXPECT_TRUE(IsNonZero(t.secondary));
  EXPECT_TRUE(IsNonZero(t.accent));
  EXPECT_TRUE(IsNonZero(t.text_primary));
  EXPECT_TRUE(IsNonZero(t.window_bg));
  EXPECT_TRUE(IsNonZero(t.button));
}

TEST_F(ThemeGeneratorTest, DarkModeTextContrastsWithBackground) {
  // Dark-mode text should be light (high luminance) so it's legible against
  // the dark background. Without this, a cyberpunk/nord/tokyo-night preset
  // can ship unreadable output.
  Color accent{0.2f, 0.5f, 0.8f, 1.0f};
  Theme t = ThemeManager::Get().GenerateThemeFromAccent(accent, true);

  const float text_lum = Luminance(t.text_primary);
  const float bg_lum = Luminance(t.background);
  EXPECT_GT(text_lum - bg_lum, 0.4f)
      << "dark-mode text is not sufficiently lighter than bg "
      << "(text=" << text_lum << ", bg=" << bg_lum << ")";
}

}  // namespace
}  // namespace yaze::gui
