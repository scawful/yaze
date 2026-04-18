// Pins semantic theme tokens on Classic YAZE so subsequent migration phases
// (5.3 overlays, 5.4 brand, 5.5 presets) can't silently shift values that
// editor code now reads through `gui::GetErrorColor()` etc.
//
// Scope is deliberately narrow: only the four semantic tokens directly
// consumed by Phase 5.2 migrations (error/warning/success/info) plus a few
// Classic-YAZE identity colors (primary/secondary/accent). A full ImGui
// GetStyle().Colors[] snapshot would be louder but needs an ImGui context
// and regenerates on every minor theme tweak. The semantic-token pin is
// what callers actually depend on.
#include "app/gui/core/color.h"
#include "app/gui/core/theme_manager.h"
#include "app/gui/core/ui_helpers.h"
#include "gtest/gtest.h"
#include "imgui/imgui.h"

namespace yaze::gui {
namespace {

// Helper: Classic YAZE stores palette entries as 0-255 ints via RGBA(r,g,b,a).
// Color::red/green/blue are floats in [0,1]. Compare with a tiny epsilon so
// roundtrip (int → float → back) doesn't flake.
void ExpectRgbNear(const Color& c, int r, int g, int b, int a = 255) {
  constexpr float kEps = 1.0f / 255.0f + 1e-5f;
  EXPECT_NEAR(c.red, r / 255.0f, kEps) << "red mismatch";
  EXPECT_NEAR(c.green, g / 255.0f, kEps) << "green mismatch";
  EXPECT_NEAR(c.blue, b / 255.0f, kEps) << "blue mismatch";
  EXPECT_NEAR(c.alpha, a / 255.0f, kEps) << "alpha mismatch";
}

// ApplyClassicYazeTheme() calls into ImGui::GetStyle() via ColorsYaze(), which
// aborts without an active context. Fixture spins up a headless ImGui context
// per test so the theme application path is well-defined.
class ThemeStyleSnapshotTest : public ::testing::Test {
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
};

TEST_F(ThemeStyleSnapshotTest, ClassicYazeSemanticTokens) {
  ThemeManager::Get().ApplyClassicYazeTheme();

  const auto& theme = ThemeManager::Get().GetCurrentTheme();
  EXPECT_EQ(theme.name, "Classic YAZE");

  // Values below are pinned from ApplyClassicYazeTheme() in
  // src/app/gui/core/theme_manager.cc (around line 1981). If Classic YAZE
  // is intentionally retuned, update these expectations with the intended
  // values — do NOT loosen the test.
  ExpectRgbNear(theme.error, 220, 50, 50);
  ExpectRgbNear(theme.warning, 255, 200, 50);
  ExpectRgbNear(theme.success, 92, 115, 92);  // = primary (alttpLightGreen)
  ExpectRgbNear(theme.info, 70, 170, 255);
}

TEST_F(ThemeStyleSnapshotTest, ClassicYazeIdentityColors) {
  ThemeManager::Get().ApplyClassicYazeTheme();
  const auto& theme = ThemeManager::Get().GetCurrentTheme();

  // Canonical Classic YAZE ALTTP greens — if these shift, the theme is no
  // longer "Classic YAZE" in any meaningful sense. These pin the RGBA values
  // used in CreateFallbackYazeClassic.
  ExpectRgbNear(theme.primary, 92, 115, 92);   // alttpLightGreen
  ExpectRgbNear(theme.secondary, 71, 92, 71);  // alttpMidGreen
  ExpectRgbNear(theme.accent, 89, 119, 89);    // TabActive
}

TEST_F(ThemeStyleSnapshotTest, SemanticHelpersReturnThemeTokens) {
  // The ui_helpers::GetSuccessColor/GetErrorColor/etc. functions are what
  // Phase 5.2 migrated callers use. Assert they return exactly what the
  // theme struct holds — not some parallel hardcoded palette.
  ThemeManager::Get().ApplyClassicYazeTheme();
  const auto& theme = ThemeManager::Get().GetCurrentTheme();

  auto ExpectEqual = [](const ImVec4& lhs, const Color& rhs) {
    constexpr float kEps = 1e-6f;
    EXPECT_NEAR(lhs.x, rhs.red, kEps);
    EXPECT_NEAR(lhs.y, rhs.green, kEps);
    EXPECT_NEAR(lhs.z, rhs.blue, kEps);
    EXPECT_NEAR(lhs.w, rhs.alpha, kEps);
  };

  ExpectEqual(GetSuccessColor(), theme.success);
  ExpectEqual(GetWarningColor(), theme.warning);
  ExpectEqual(GetErrorColor(), theme.error);
  ExpectEqual(GetInfoColor(), theme.info);
}

}  // namespace
}  // namespace yaze::gui
