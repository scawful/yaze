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
#include <cstdio>
#include <filesystem>
#include <fstream>

#include "app/gui/core/color.h"
#include "app/gui/core/theme_manager.h"
#include "app/gui/core/ui_helpers.h"
#include "gtest/gtest.h"
#include "imgui/imgui.h"

namespace yaze::gui {
namespace {

std::vector<std::string> ShippedFileThemeNames() {
  return {"Breath of the Wild",
          "Cyberpunk",
          "Forest",
          "Forest Light",
          "Gruvbox",
          "Majora's Moon",
          "Midnight",
          "Midnight Light",
          "Nord",
          "Ocean",
          "Ocean Light",
          "Solarized Dark",
          "Solarized Light",
          "Sunset",
          "Tokyo Night",
          "Twilight",
          "Wind Waker",
          "YAZE Tre"};
}

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

// Regression for the Phase 5.1 restore path. Classic YAZE is intentionally
// kept out of themes_ (see ApplyClassicYazeTheme's off-by-one note), so a
// naive ApplyTheme("Classic YAZE") used to fall through LoadTheme's "not
// found" branch and land on YAZE Tre. The fix routes by name inside ApplyTheme
// so startup restore, command-palette switch, and any programmatic caller all
// end up on Classic YAZE. Without this test, the bug could resurface any time
// someone refactors the classic-theme off-by-one guard.
TEST_F(ThemeStyleSnapshotTest, ApplyThemeByNameRestoresClassicYaze) {
  // Prime to a different theme so the test actually observes a transition.
  // Avoids the false-positive where current_theme_name_ happened to already
  // be "Classic YAZE" from a prior TEST_F on the fixture-owned singleton.
  auto& mgr = ThemeManager::Get();
  mgr.ApplyTheme("YAZE Tre");
  ASSERT_EQ(mgr.GetCurrentThemeName(), "YAZE Tre");

  mgr.ApplyTheme("Classic YAZE");
  EXPECT_EQ(mgr.GetCurrentThemeName(), "Classic YAZE");
  EXPECT_EQ(mgr.GetCurrentTheme().name, "Classic YAZE");
}

TEST_F(ThemeStyleSnapshotTest, MissingThemeFallsBackToClassicYaze) {
  auto& mgr = ThemeManager::Get();
  mgr.ApplyTheme("YAZE Tre");
  ASSERT_EQ(mgr.GetCurrentThemeName(), "YAZE Tre");

  mgr.ApplyTheme("Definitely Missing Theme");
  EXPECT_EQ(mgr.GetCurrentThemeName(), "Classic YAZE");
  EXPECT_EQ(mgr.GetCurrentTheme().name, "Classic YAZE");
}

TEST_F(ThemeStyleSnapshotTest, AvailableThemesExposeClassicYaze) {
  auto& mgr = ThemeManager::Get();
  const auto themes = mgr.GetAvailableThemes();

  ASSERT_FALSE(themes.empty());
  EXPECT_EQ(themes.front(), "Classic YAZE");

  const Theme* classic = mgr.GetTheme("Classic YAZE");
  ASSERT_NE(classic, nullptr);
  EXPECT_EQ(classic->name, "Classic YAZE");
}

TEST_F(ThemeStyleSnapshotTest, PreviewClassicYazeThroughThemeListApi) {
  auto& mgr = ThemeManager::Get();
  mgr.ApplyTheme("YAZE Tre");
  ASSERT_EQ(mgr.GetCurrentThemeName(), "YAZE Tre");

  mgr.StartPreview("Classic YAZE");
  EXPECT_TRUE(mgr.IsPreviewActive());
  EXPECT_EQ(mgr.GetCurrentThemeName(), "Classic YAZE");

  mgr.EndPreview();
  EXPECT_EQ(mgr.GetCurrentThemeName(), "YAZE Tre");
}

// Regression for the review follow-up on 07619173: the new
// ApplyTheme("Classic YAZE") short-circuit routed directly to
// ApplyClassicYazeTheme, but that helper skipped the transition reset and
// AgentUI refresh that LoadTheme and ApplyTheme(const Theme&) perform. Left
// unchecked, an in-progress transition would keep lerping over the Classic
// style on subsequent UpdateTransition frames and agent panels would draw
// against stale cached theme colors. Assert that Classic apply leaves the
// transition state clean regardless of how it was entered.
TEST_F(ThemeStyleSnapshotTest, ApplyClassicYazeClearsTransitionState) {
  auto& mgr = ThemeManager::Get();

  // Prime a non-classic theme, then trigger a transition by re-applying it
  // through the Theme& overload (which sets transitioning_ when animations
  // are enabled). YAZE Tre ships with enable_animations=true by default.
  mgr.ApplyTheme("YAZE Tre");
  const auto* tre = mgr.GetTheme("YAZE Tre");
  ASSERT_NE(tre, nullptr);
  Theme tre_with_animation = *tre;
  tre_with_animation.enable_animations = true;
  mgr.ApplyTheme(tre_with_animation);
  ASSERT_TRUE(mgr.IsTransitioning())
      << "pre-condition: YAZE Tre re-apply should start a transition";

  mgr.ApplyTheme("Classic YAZE");
  EXPECT_FALSE(mgr.IsTransitioning())
      << "Classic YAZE apply must cancel the in-flight transition so "
         "UpdateTransition doesn't overwrite the Classic style.";
}

// Regression for the second review follow-up: Phase 5 initially only recorded
// theme file paths on load, so "Save Over Current" broke for themes that were
// freshly saved or renamed through the save dialog. SaveThemeToFile now
// populates theme_file_paths_ too — exercise that round trip.
TEST_F(ThemeStyleSnapshotTest, SaveThemeToFileRecordsPathForRenamedTheme) {
  namespace fs = std::filesystem;
  auto& mgr = ThemeManager::Get();

  // Build a freshly-named theme that has no pre-existing file association.
  const auto* base = mgr.GetTheme("YAZE Tre");
  ASSERT_NE(base, nullptr);
  Theme renamed = *base;
  renamed.name = "YazeThemeSnapshotRenamed";

  // Write it to a deterministic temp path.
  fs::path tmp_dir = fs::temp_directory_path();
  fs::path tmp_path = tmp_dir / "yaze_theme_snapshot_renamed.theme";
  std::error_code ec;
  fs::remove(tmp_path, ec);  // ignore absent-file error

  auto save_status = mgr.SaveThemeToFile(renamed, tmp_path.string());
  ASSERT_TRUE(save_status.ok()) << save_status.message();

  // Apply by value so current_theme_name_ matches the renamed theme, then
  // ask the manager where the current theme lives. Without the save-side
  // path recording, this would fall through to filename synthesis and miss
  // the temp path entirely.
  mgr.ApplyTheme(renamed);
  EXPECT_EQ(mgr.GetCurrentThemeName(), "YazeThemeSnapshotRenamed");
  EXPECT_EQ(mgr.GetCurrentThemeFilePath(), tmp_path.string());

  fs::remove(tmp_path, ec);
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

TEST_F(ThemeStyleSnapshotTest, FileThemesHydrateEnhancedSemanticDefaults) {
  auto& mgr = ThemeManager::Get();

  auto expect_hydrated = [](const Theme& theme, const char* name) {
    auto not_missing = [name](const Color& c, const char* field) {
      const bool all_zero =
          c.red == 0.0f && c.green == 0.0f && c.blue == 0.0f && c.alpha == 0.0f;
      const bool opaque_black =
          c.red == 0.0f && c.green == 0.0f && c.blue == 0.0f && c.alpha == 1.0f;
      EXPECT_FALSE(all_zero) << name << " missing " << field;
      EXPECT_FALSE(opaque_black)
          << name << " defaulted " << field << " to opaque black";
    };

    not_missing(theme.text_highlight, "text_highlight");
    not_missing(theme.link_hover, "link_hover");
    not_missing(theme.code_background, "code_background");
    not_missing(theme.success_light, "success_light");
    not_missing(theme.warning_light, "warning_light");
    not_missing(theme.error_light, "error_light");
    not_missing(theme.info_light, "info_light");
    not_missing(theme.active_selection, "active_selection");
    not_missing(theme.hover_highlight, "hover_highlight");
    not_missing(theme.focus_border, "focus_border");
    not_missing(theme.disabled_overlay, "disabled_overlay");
    not_missing(theme.editor_background, "editor_background");
    not_missing(theme.editor_grid, "editor_grid");
    not_missing(theme.selection_primary, "selection_primary");
    not_missing(theme.selection_secondary, "selection_secondary");
    not_missing(theme.dungeon.object_door, "dungeon.object_door");
    not_missing(theme.agent.panel_bg, "agent.panel_bg");
    not_missing(theme.agent.code_background, "agent.code_background");
  };

  for (const auto& theme_name : ShippedFileThemeNames()) {
    mgr.ApplyTheme(theme_name);
    SCOPED_TRACE(theme_name);
    expect_hydrated(mgr.GetCurrentTheme(), theme_name.c_str());
  }
}

TEST_F(ThemeStyleSnapshotTest,
       ParseThemeFileLoadsEnhancedSemanticAndEditorFields) {
  auto& mgr = ThemeManager::Get();
  Theme theme = *mgr.GetTheme("YAZE Tre");
  theme.name = "YAZE Tre Roundtrip Test";

  theme.text_highlight = {0.11f, 0.22f, 0.33f, 0.44f};
  theme.link_hover = {0.21f, 0.32f, 0.43f, 1.0f};
  theme.code_background = {0.07f, 0.08f, 0.09f, 1.0f};
  theme.success_light = {0.31f, 0.72f, 0.43f, 1.0f};
  theme.active_selection = {0.91f, 0.61f, 0.21f, 0.51f};
  theme.focus_border = {0.17f, 0.47f, 0.77f, 1.0f};
  theme.editor_background = {0.09f, 0.14f, 0.19f, 1.0f};
  theme.editor_grid = {0.23f, 0.28f, 0.33f, 0.39f};
  theme.editor_cursor = {0.95f, 0.9f, 0.85f, 1.0f};
  theme.editor_selection = {0.24f, 0.44f, 0.64f, 0.34f};

  const auto temp_path =
      std::filesystem::temp_directory_path() / "yaze_theme_roundtrip.theme";
  ASSERT_TRUE(mgr.SaveThemeToFile(theme, temp_path.string()).ok());
  ASSERT_TRUE(mgr.LoadThemeFromFile(temp_path.string()).ok());
  const Theme* parsed = mgr.GetTheme(theme.name);
  ASSERT_NE(parsed, nullptr);

  auto expect_same = [](const Color& lhs, const Color& rhs) {
    constexpr float kEps = 1.0f / 255.0f + 1e-5f;
    EXPECT_NEAR(lhs.red, rhs.red, kEps);
    EXPECT_NEAR(lhs.green, rhs.green, kEps);
    EXPECT_NEAR(lhs.blue, rhs.blue, kEps);
    EXPECT_NEAR(lhs.alpha, rhs.alpha, kEps);
  };

  expect_same(parsed->text_highlight, theme.text_highlight);
  expect_same(parsed->link_hover, theme.link_hover);
  expect_same(parsed->code_background, theme.code_background);
  expect_same(parsed->success_light, theme.success_light);
  expect_same(parsed->active_selection, theme.active_selection);
  expect_same(parsed->focus_border, theme.focus_border);
  expect_same(parsed->editor_background, theme.editor_background);
  expect_same(parsed->editor_grid, theme.editor_grid);
  expect_same(parsed->editor_cursor, theme.editor_cursor);
  expect_same(parsed->editor_selection, theme.editor_selection);

  std::error_code ec;
  std::filesystem::remove(temp_path, ec);
}

}  // namespace
}  // namespace yaze::gui
