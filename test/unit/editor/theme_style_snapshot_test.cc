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
#include <chrono>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <string>

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

// temp_directory_path() is shared by every process for this user, so a fixed
// filename collides when two checkouts (or two CI shards) test at once.
std::filesystem::path TempThemePath(const char* stem) {
  // getpid() is not portable to the Windows job; a clock token plus a counter
  // is, and is unique across concurrent processes either way.
  static const std::string token = std::to_string(
      std::chrono::high_resolution_clock::now().time_since_epoch().count());
  static int serial = 0;
  return std::filesystem::temp_directory_path() /
         (std::string("yaze_") + stem + "_" + token + "_" +
          std::to_string(++serial) + ".theme");
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

// ColorsYaze() is the only writer for Classic YAZE's ImGui palette, so any
// ImGuiCol_ slot it skips keeps whatever the previously applied preset wrote.
// Switching preset -> Classic left docking previews, the text caret and tab
// overlines painted in the preset's colors.
TEST_F(ThemeStyleSnapshotTest, ClassicYazeWritesEveryImGuiColorSlot) {
  auto& mgr = ThemeManager::Get();
  // Poison every slot. ColorsYaze() is the ONLY writer for Classic YAZE, so
  // any slot it skips keeps whatever was there before — in practice the
  // previously applied preset, which is how Cyberpunk's purple docking
  // preview survived a switch to Classic. Asserting the whole array guards
  // the bug class, including slots a future ImGui upgrade adds.
  constexpr ImVec4 kSentinel(1.0f, 0.0f, 1.0f, 0.123f);
  ImVec4* colors = ImGui::GetStyle().Colors;
  for (int i = 0; i < ImGuiCol_COUNT; ++i) {
    colors[i] = kSentinel;
  }

  mgr.ApplyClassicYazeTheme();

  for (int i = 0; i < ImGuiCol_COUNT; ++i) {
    const bool untouched =
        colors[i].x == kSentinel.x && colors[i].y == kSentinel.y &&
        colors[i].z == kSentinel.z && colors[i].w == kSentinel.w;
    EXPECT_FALSE(untouched)
        << "ColorsYaze() left ImGuiCol_" << ImGui::GetStyleColorName(i)
        << " (index " << i << ") unwritten; it will inherit the previous theme";
  }
  // Style scalars ColorsYaze must also own.
  EXPECT_FLOAT_EQ(ImGui::GetStyle().TabRounding, 0.0f);
  EXPECT_FLOAT_EQ(ImGui::GetStyle().GrabRounding, 5.0f);
}

// Classic YAZE's spacing is part of its identity: ColorsYaze() sets
// FramePadding 10x2 and ItemSpacing 10x5, not the 8px base every other theme
// derives from. Carrying density across a theme switch must scale those, not
// replace them.
TEST_F(ThemeStyleSnapshotTest, ClassicYazeKeepsItsOwnSpacingAtNormalDensity) {
  auto& mgr = ThemeManager::Get();
  mgr.ApplyTheme("Cyberpunk");
  mgr.ApplyClassicYazeTheme();

  const ImGuiStyle& style = ImGui::GetStyle();
  ASSERT_EQ(mgr.GetCurrentTheme().density_preset, DensityPreset::kNormal);
  EXPECT_FLOAT_EQ(style.FramePadding.x, 10.0f);
  EXPECT_FLOAT_EQ(style.FramePadding.y, 2.0f);
  EXPECT_FLOAT_EQ(style.ItemSpacing.x, 10.0f);
  EXPECT_FLOAT_EQ(style.ItemSpacing.y, 5.0f);
  EXPECT_FLOAT_EQ(style.WindowPadding.x, 10.0f);
  EXPECT_FLOAT_EQ(style.WindowPadding.y, 10.0f);
  EXPECT_FLOAT_EQ(style.GrabMinSize, 15.0f);
}

// EndPreview restores through ApplyTheme(const Theme&), which repaints from the
// Theme struct. For Classic YAZE that struct cannot reproduce ColorsYaze(), so
// the restore has to route back to ApplyClassicYazeTheme().
TEST_F(ThemeStyleSnapshotTest, EndPreviewRestoresClassicYazeAppearance) {
  auto& mgr = ThemeManager::Get();
  mgr.ApplyClassicYazeTheme();
  ASSERT_EQ(mgr.GetCurrentThemeName(), "Classic YAZE");
  const ImVec4* colors = ImGui::GetStyle().Colors;
  const ImVec4 classic_check = colors[ImGuiCol_CheckMark];
  const ImVec4 classic_title = colors[ImGuiCol_TitleBgActive];
  const float classic_grab_rounding = ImGui::GetStyle().GrabRounding;

  mgr.StartPreview("Nord");
  ASSERT_EQ(mgr.GetCurrentThemeName(), "Nord");
  mgr.EndPreview();

  EXPECT_EQ(mgr.GetCurrentThemeName(), "Classic YAZE");
  EXPECT_FLOAT_EQ(colors[ImGuiCol_CheckMark].x, classic_check.x);
  EXPECT_FLOAT_EQ(colors[ImGuiCol_CheckMark].y, classic_check.y);
  EXPECT_FLOAT_EQ(colors[ImGuiCol_CheckMark].z, classic_check.z);
  EXPECT_FLOAT_EQ(colors[ImGuiCol_TitleBgActive].x, classic_title.x);
  EXPECT_FLOAT_EQ(colors[ImGuiCol_TitleBgActive].y, classic_title.y);
  EXPECT_FLOAT_EQ(colors[ImGuiCol_TitleBgActive].z, classic_title.z);
  EXPECT_FLOAT_EQ(ImGui::GetStyle().GrabRounding, classic_grab_rounding);
}

// The Display Density combo builds a copy of the current theme, flips its
// preset and re-applies it. Classic YAZE routes through ApplyClassicYazeTheme,
// which rebuilds from BuildClassicYazeTheme — so the incoming preset has to be
// threaded through, and the sizing has to be re-applied over ColorsYaze()'s
// hardcoded padding.
TEST_F(ThemeStyleSnapshotTest, DensityChangeTakesEffectUnderClassicYaze) {
  auto& mgr = ThemeManager::Get();
  mgr.ApplyClassicYazeTheme();
  Theme normal = mgr.GetCurrentTheme();
  normal.ApplyDensityPreset(DensityPreset::kNormal);
  mgr.ReapplyTheme(normal);
  const ImVec2 normal_padding = ImGui::GetStyle().FramePadding;
  const float normal_scrollbar = ImGui::GetStyle().ScrollbarSize;

  Theme compact = mgr.GetCurrentTheme();
  compact.ApplyDensityPreset(DensityPreset::kCompact);
  mgr.ReapplyTheme(compact);

  EXPECT_EQ(mgr.GetCurrentTheme().density_preset, DensityPreset::kCompact);
  EXPECT_LT(ImGui::GetStyle().FramePadding.x, normal_padding.x);
  EXPECT_LT(ImGui::GetStyle().FramePadding.y, normal_padding.y);
  EXPECT_LT(ImGui::GetStyle().ScrollbarSize, normal_scrollbar);
}

// The Classic YAZE restore path must not swallow deliberate edits: the theme
// editor seeds `edit_theme` from the current theme, so while Classic is active
// every live edit still carries the name "Classic YAZE". ApplyTheme paints
// what it is handed; only ReapplyTheme re-routes to ColorsYaze().
TEST_F(ThemeStyleSnapshotTest, ApplyThemeHonorsEditsToClassicYazeStruct) {
  auto& mgr = ThemeManager::Get();
  mgr.ApplyClassicYazeTheme();
  Theme edited = mgr.GetCurrentTheme();
  ASSERT_EQ(edited.name, "Classic YAZE");
  edited.button = {0.9f, 0.1f, 0.6f, 1.0f};
  // ApplyTheme lerps from the old colors when the OUTGOING theme has
  // animations on, so pin Classic with animations off first — otherwise the
  // frame after the switch still shows the start color, not the target.
  Theme unanimated_classic = edited;
  unanimated_classic.button = mgr.GetCurrentTheme().button;
  unanimated_classic.enable_animations = false;
  mgr.ApplyTheme(unanimated_classic);
  edited.enable_animations = false;

  mgr.ApplyTheme(edited);

  const ImVec4* colors = ImGui::GetStyle().Colors;
  EXPECT_NEAR(colors[ImGuiCol_Button].x, 0.9f, 0.02f);
  EXPECT_NEAR(colors[ImGuiCol_Button].y, 0.1f, 0.02f);
  EXPECT_NEAR(colors[ImGuiCol_Button].z, 0.6f, 0.02f);

  // ReapplyTheme on the same struct discards the edit and restores Classic's
  // ColorsYaze() green.
  mgr.ReapplyTheme(edited);
  EXPECT_LT(colors[ImGuiCol_Button].x, 0.6f);
  EXPECT_GT(colors[ImGuiCol_Button].y, colors[ImGuiCol_Button].x);
}

// No shipped .theme file declares plot colors, and active_selection/
// hover_highlight derive from selection colors that the file parser also does
// not set. Without ApplySmartDefaults they reach ImGui as opaque black.
TEST_F(ThemeStyleSnapshotTest, FileThemesHydratePlotAndSelectionColors) {
  auto& mgr = ThemeManager::Get();
  auto is_black = [](const Color& c) {
    return c.red == 0.0f && c.green == 0.0f && c.blue == 0.0f;
  };
  for (const auto& name : ShippedFileThemeNames()) {
    const Theme* theme = mgr.GetTheme(name);
    ASSERT_NE(theme, nullptr) << name;
    EXPECT_FALSE(is_black(theme->plot_lines)) << name << " plot_lines";
    EXPECT_FALSE(is_black(theme->plot_histogram)) << name << " plot_histogram";
    EXPECT_FALSE(is_black(theme->active_selection))
        << name << " active_selection";
    EXPECT_FALSE(is_black(theme->hover_highlight))
        << name << " hover_highlight";
    EXPECT_FALSE(is_black(theme->selection_primary))
        << name << " selection_primary";
  }
}

// GenerateThemeFromAccent built colors field by field and never ran the
// semantic-default pass, so accent themes shipped the same black holes.
TEST_F(ThemeStyleSnapshotTest, GeneratedAccentThemeHydratesSemanticColors) {
  auto& mgr = ThemeManager::Get();
  const Color accent{120.0f / 255.0f, 90.0f / 255.0f, 200.0f / 255.0f, 1.0f};
  const Theme theme = mgr.GenerateThemeFromAccent(accent, true);
  auto not_black = [](const Color& c, const char* field) {
    EXPECT_FALSE(c.red == 0.0f && c.green == 0.0f && c.blue == 0.0f)
        << field << " is black";
  };
  not_black(theme.plot_lines, "plot_lines");
  not_black(theme.plot_histogram, "plot_histogram");
  not_black(theme.active_selection, "active_selection");
  not_black(theme.hover_highlight, "hover_highlight");
  not_black(theme.selection_primary, "selection_primary");
}

// The [style] parser called std::stof unguarded: one bad value in a
// hand-edited or third-party .theme file threw std::invalid_argument out of
// the loader, and themes load from the ThemeManager singleton's constructor
// where that ends the process. Malform EVERY numeric key rather than one:
// guarding six of seven keys looks identical to guarding all of them if the
// test only ever feeds a bad value to the first.
TEST_F(ThemeStyleSnapshotTest,
       MalformedStyleValueKeepsDefaultInsteadOfThrowing) {
  const char* kNumericStyleKeys[] = {"window_rounding",    "frame_rounding",
                                     "scrollbar_rounding", "grab_rounding",
                                     "tab_rounding",       "window_border_size",
                                     "frame_border_size",  "animation_speed"};

  for (const char* bad_key : kNumericStyleKeys) {
    const std::string theme_name =
        std::string("Yaze Malformed ") + bad_key + " Test";
    const auto path = TempThemePath("malformed_style");
    {
      std::ofstream out(path);
      out << "name=" << theme_name << "\n"
          << "[colors]\n"
          << "primary=10,20,30,255\n"
          << "[style]\n";
      // Every numeric key present and valid except the one under test, so a
      // failure names exactly which key is unguarded.
      for (const char* key : kNumericStyleKeys) {
        out << key << "=" << (key == bad_key ? "not-a-number" : "4.0") << "\n";
      }
    }
    auto& mgr = ThemeManager::Get();
    const auto status = mgr.LoadThemeFromFile(path.string());
    std::error_code ec;
    std::filesystem::remove(path, ec);
    ASSERT_TRUE(status.ok()) << bad_key << ": " << status.message();

    const Theme* parsed = mgr.GetTheme(theme_name);
    ASSERT_NE(parsed, nullptr) << bad_key;
    // The other keys still parsed, and the bad one kept its default rather
    // than aborting the whole load.
    EXPECT_FLOAT_EQ(parsed->frame_rounding,
                    std::string(bad_key) == "frame_rounding"
                        ? Theme{}.frame_rounding
                        : 4.0f)
        << bad_key;
    if (std::string(bad_key) == "animation_speed") {
      EXPECT_FLOAT_EQ(parsed->animation_speed, Theme{}.animation_speed);
    }
    if (std::string(bad_key) == "window_rounding") {
      EXPECT_FLOAT_EQ(parsed->window_rounding, Theme{}.window_rounding);
    }
  }
}

TEST_F(ThemeStyleSnapshotTest, SerializedThemeKeepsEveryParsedStyleKey) {
  auto& mgr = ThemeManager::Get();
  Theme theme = *mgr.GetTheme("YAZE Tre");
  theme.name = "Yaze Style Roundtrip Test";
  // Every numeric key ParseThemeFile's [style] branch recognises, each with a
  // distinct value. The test's name promises the invariant "the serializer
  // writes everything the parser reads" — pinning only the keys one commit
  // happened to add would let the next omission through exactly as
  // animation_speed's did.
  theme.window_rounding = 1.5f;
  theme.frame_rounding = 2.5f;
  theme.scrollbar_rounding = 3.5f;
  theme.grab_rounding = 7.0f;
  theme.tab_rounding = 4.5f;
  theme.window_border_size = 2.0f;
  theme.frame_border_size = 3.0f;
  theme.animation_speed = 1.75f;
  theme.enable_animations = false;
  theme.enable_glow_effects = true;

  const auto path = TempThemePath("style_roundtrip");
  ASSERT_TRUE(mgr.SaveThemeToFile(theme, path.string()).ok());
  const auto status = mgr.LoadThemeFromFile(path.string());
  std::error_code ec;
  std::filesystem::remove(path, ec);
  ASSERT_TRUE(status.ok()) << status.message();

  const Theme* parsed = mgr.GetTheme(theme.name);
  ASSERT_NE(parsed, nullptr);
  EXPECT_FLOAT_EQ(parsed->window_rounding, 1.5f);
  EXPECT_FLOAT_EQ(parsed->frame_rounding, 2.5f);
  EXPECT_FLOAT_EQ(parsed->scrollbar_rounding, 3.5f);
  EXPECT_FLOAT_EQ(parsed->grab_rounding, 7.0f);
  EXPECT_FLOAT_EQ(parsed->tab_rounding, 4.5f);
  EXPECT_FLOAT_EQ(parsed->window_border_size, 2.0f);
  EXPECT_FLOAT_EQ(parsed->frame_border_size, 3.0f);
  EXPECT_FLOAT_EQ(parsed->animation_speed, 1.75f);
  EXPECT_FALSE(parsed->enable_animations);
  EXPECT_TRUE(parsed->enable_glow_effects);
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
    EXPECT_GT(theme.agent.panel_border.alpha, 0.0f)
        << name << " panel border is invisible";
    constexpr float kMaxQuietBorderAlpha = 115.0f / 255.0f + 1e-5f;
    EXPECT_LE(theme.agent.panel_border.alpha, kMaxQuietBorderAlpha)
        << name << " panel border competes with active controls";
  };

  for (const auto& theme_name : ShippedFileThemeNames()) {
    mgr.ApplyTheme(theme_name);
    SCOPED_TRACE(theme_name);
    expect_hydrated(mgr.GetCurrentTheme(), theme_name.c_str());
  }
}

TEST_F(ThemeStyleSnapshotTest, ForestPairUsesQuietChrome) {
  auto& mgr = ThemeManager::Get();

  const Theme* forest = mgr.GetTheme("Forest");
  ASSERT_NE(forest, nullptr);
  ExpectRgbNear(forest->background, 13, 18, 15);
  ExpectRgbNear(forest->surface, 21, 27, 23);
  ExpectRgbNear(forest->button, 37, 49, 41);
  ExpectRgbNear(forest->border, 74, 91, 79, 115);
  EXPECT_LT(forest->button.green, forest->primary.green);

  const Theme* forest_light = mgr.GetTheme("Forest Light");
  ASSERT_NE(forest_light, nullptr);
  ExpectRgbNear(forest_light->button, 232, 226, 211);
  ExpectRgbNear(forest_light->border, 72, 102, 80, 110);
  EXPECT_LT(forest_light->border.alpha, 0.5f);
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
