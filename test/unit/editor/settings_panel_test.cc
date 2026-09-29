#include "app/editor/shell/windows/settings_panel.h"

#include <algorithm>
#include <array>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "absl/status/status.h"
#include "app/editor/dungeon/dungeon_editor_v2.h"
#include "app/editor/editor_manager.h"
#include "app/gfx/backend/null_renderer.h"
#include "core/features.h"
#include "core/project.h"
#include "editor_test_support.h"
#include "gtest/gtest.h"
#include "imgui/imgui.h"

namespace yaze::editor {

class SettingsPanelTestPeer {
 public:
  static std::array<std::pair<std::string, bool>, 5> BuildOverlaySummary(
      const project::DungeonOverlaySettings& overlay) {
    return SettingsPanel::BuildDungeonOverlaySummary(overlay);
  }

  static absl::Status RequestOpenMinecartTracks(SettingsPanel& panel) {
    return panel.RequestOpenMinecartTracks();
  }

  static const std::string& ProjectStatusMessage(const SettingsPanel& panel) {
    return panel.project_status_message_;
  }

  static void ApplyDisplayDensity(SettingsPanel& panel,
                                  gui::DensityPreset preset) {
    panel.ApplyDisplayDensity(preset);
  }
};

namespace {

struct ScopedImGuiContext {
  ScopedImGuiContext() {
    context = ImGui::CreateContext();
    ImGui::SetCurrentContext(context);
    unsigned char* pixels = nullptr;
    int width = 0;
    int height = 0;
    ImGui::GetIO().Fonts->GetTexDataAsRGBA32(&pixels, &width, &height);
  }

  ~ScopedImGuiContext() {
    if (context != nullptr) {
      ImGui::DestroyContext(context);
    }
  }

  ImGuiContext* context = nullptr;
};

struct FeatureFlagsGuard {
  core::FeatureFlags::Flags previous = core::FeatureFlags::get();
  ~FeatureFlagsGuard() { core::FeatureFlags::get() = previous; }
};

class ScopedManagerProject {
 public:
  ScopedManagerProject() {
    const auto nonce =
        std::chrono::steady_clock::now().time_since_epoch().count();
    root_ = std::filesystem::temp_directory_path() /
            ("yaze_settings_minecart_" + std::to_string(nonce));
    std::filesystem::create_directories(root_);
  }

  ~ScopedManagerProject() {
    std::error_code error;
    std::filesystem::remove_all(root_, error);
  }

  const std::filesystem::path& root() const { return root_; }
  std::filesystem::path rom_path() const { return root_ / "test.sfc"; }
  std::filesystem::path project_path() const { return root_ / "test.yaze"; }

 private:
  std::filesystem::path root_;
};

TEST(SettingsPanelTest, OverlaySummaryClassifiesStandardAndCustomValues) {
  project::DungeonOverlaySettings overlay;

  auto summary = SettingsPanelTestPeer::BuildOverlaySummary(overlay);
  EXPECT_EQ(summary[0].first,
            "0xB0, 0xB1, 0xB2, 0xB3, 0xB4, 0xB5, 0xB6, 0xB7, 0xB8, "
            "0xB9, 0xBA, 0xBB, 0xBC, 0xBD, 0xBE");
  EXPECT_TRUE(summary[0].second);
  EXPECT_EQ(summary[1].first, "0xB7, 0xB8, 0xB9, 0xBA");
  EXPECT_TRUE(summary[1].second);
  EXPECT_EQ(summary[2].first, "0xD0, 0xD1, 0xD2, 0xD3");
  EXPECT_TRUE(summary[2].second);
  EXPECT_EQ(summary[3].first, "0x31");
  EXPECT_TRUE(summary[3].second);
  EXPECT_EQ(summary[4].first, "0xA3");
  EXPECT_TRUE(summary[4].second);
  EXPECT_TRUE(overlay.track_tiles.empty());
  EXPECT_TRUE(overlay.track_stop_tiles.empty());
  EXPECT_TRUE(overlay.track_switch_tiles.empty());
  EXPECT_TRUE(overlay.track_object_ids.empty());
  EXPECT_TRUE(overlay.minecart_sprite_ids.empty());

  overlay.track_tiles = {0xC0, 0xC1};
  overlay.track_stop_tiles = {0xB7, 0xB8, 0xB9, 0xBA};
  summary = SettingsPanelTestPeer::BuildOverlaySummary(overlay);
  EXPECT_EQ(summary[0].first, "0xC0, 0xC1");
  EXPECT_FALSE(summary[0].second);
  EXPECT_TRUE(summary[1].second);
}

TEST(SettingsPanelTest, CategoriesAreSeparatedByScope) {
  const auto application =
      SettingsPanel::FilterCategories(SettingsPanel::Scope::kApplication, "");
  const auto workspace =
      SettingsPanel::FilterCategories(SettingsPanel::Scope::kWorkspace, "");
  const auto project =
      SettingsPanel::FilterCategories(SettingsPanel::Scope::kProject, "");

  ASSERT_EQ(application.size(), 6U);
  ASSERT_EQ(workspace.size(), 1U);
  ASSERT_EQ(project.size(), 3U);
  EXPECT_EQ(workspace.front().id, std::string("layout"));
  EXPECT_TRUE(
      std::any_of(project.begin(), project.end(),
                  [](const SettingsPanel::CategoryDescriptor& category) {
                    return std::string(category.id) == "features";
                  }));
  EXPECT_TRUE(
      std::any_of(project.begin(), project.end(),
                  [](const SettingsPanel::CategoryDescriptor& category) {
                    return std::string(category.id) == "patches";
                  }));
}

TEST(SettingsPanelTest, CategorySearchMatchesKeywordsWithinScope) {
  const auto experimental = SettingsPanel::FilterCategories(
      SettingsPanel::Scope::kProject, "experimental");
  ASSERT_EQ(experimental.size(), 1U);
  EXPECT_EQ(experimental.front().id, std::string("features"));

  const auto model = SettingsPanel::FilterCategories(
      SettingsPanel::Scope::kApplication, "ollama");
  ASSERT_EQ(model.size(), 1U);
  EXPECT_EQ(model.front().id, std::string("ai"));

  EXPECT_TRUE(SettingsPanel::FilterCategories(SettingsPanel::Scope::kWorkspace,
                                              "experimental")
                  .empty());
}

TEST(SettingsPanelTest, ScopedLayoutRendersAtNarrowAndWideWidths) {
  ScopedImGuiContext imgui;
  UserSettings settings;
  SettingsPanel panel;
  panel.SetUserSettings(&settings);

  for (float scale : {1.0f, 1.5f}) {
    for (float width : {360.0f, 800.0f}) {
      SCOPED_TRACE(::testing::Message()
                   << "width=" << width << " scale=" << scale);
      ImGui::GetIO().FontGlobalScale = scale;
      ImGui::GetIO().DisplaySize = ImVec2(width, 720.0f);
      ImGui::NewFrame();
      ImGui::SetNextWindowSize(ImVec2(width, 700.0f));
      ImGui::Begin("SettingsLayoutTest");
      EXPECT_NO_FATAL_FAILURE(panel.Draw());
      ImGui::End();
      ImGui::EndFrame();
      ImGui::Render();
    }
  }
}

TEST(SettingsPanelTest, MinecartNavigationUsesCallbackWithoutEditingProject) {
  SettingsPanel panel;
  project::YazeProject project;
  project.dungeon_overlay.track_tiles = {0xC0};
  panel.SetProject(&project);
  const project::DungeonOverlaySettings before = project.dungeon_overlay;
  int callback_count = 0;
  panel.SetOpenMinecartTracksCallback([&callback_count]() {
    ++callback_count;
    return absl::OkStatus();
  });

  EXPECT_TRUE(SettingsPanelTestPeer::RequestOpenMinecartTracks(panel).ok());
  EXPECT_EQ(callback_count, 1);
  EXPECT_EQ(project.dungeon_overlay.track_tiles, before.track_tiles);
  EXPECT_TRUE(SettingsPanelTestPeer::ProjectStatusMessage(panel).empty());
}

TEST(SettingsPanelTest, MinecartNavigationFailsClosedAndReportsReason) {
  SettingsPanel panel;

  const absl::Status missing_callback =
      SettingsPanelTestPeer::RequestOpenMinecartTracks(panel);
  EXPECT_TRUE(absl::IsFailedPrecondition(missing_callback));
  EXPECT_EQ(SettingsPanelTestPeer::ProjectStatusMessage(panel),
            "Minecart Tracks navigation is unavailable.");

  panel.SetOpenMinecartTracksCallback([]() {
    return absl::NotFoundError("Minecart panel is not registered.");
  });
  const absl::Status failed_open =
      SettingsPanelTestPeer::RequestOpenMinecartTracks(panel);
  EXPECT_TRUE(absl::IsNotFound(failed_open));
  EXPECT_EQ(SettingsPanelTestPeer::ProjectStatusMessage(panel),
            "Minecart panel is not registered.");
}

// The Display Density combo is one line calling ApplyDisplayDensity. What
// matters is that it routes through ReapplyTheme: Classic YAZE's struct
// cannot reproduce ColorsYaze(), so a plain ApplyTheme would repaint Classic
// as a different-looking theme the moment the user touched density.
TEST(SettingsPanelTest, DisplayDensityKeepsClassicYazePaintedByColorsYaze) {
  ScopedImGuiContext imgui;
  auto& themes = gui::ThemeManager::Get();
  // ThemeManager is a process-wide singleton. Earlier cases in the same
  // process (welcome-screen and theme density tests) can leave it on Compact
  // or Comfortable, and ApplyClassicYazeTheme() carries the current density
  // forward. Pin Normal so the 10px baseline holds in any order, and restore
  // the whole theme (density included) afterwards, not only its name.
  const gui::Theme saved = themes.GetCurrentTheme();
  themes.ApplyClassicYazeTheme(gui::DensityPreset::kNormal);

  // Assert on spacing, not colour: ApplyTheme starts a lerp that leaves the
  // colour array at its START values until frames run, so a colour check here
  // passes under both routes and proves nothing.
  ASSERT_FLOAT_EQ(ImGui::GetStyle().FramePadding.x, 10.0f);

  SettingsPanel panel;
  SettingsPanelTestPeer::ApplyDisplayDensity(panel,
                                             gui::DensityPreset::kCompact);

  EXPECT_EQ(themes.GetCurrentThemeName(), "Classic YAZE");
  EXPECT_EQ(themes.GetCurrentTheme().density_preset,
            gui::DensityPreset::kCompact);
  // ColorsYaze's own FramePadding.x of 10 scaled by Compact's 0.75. Routing
  // through ApplyTheme instead would rebuild from the shared 8px base and
  // land on 3.0, discarding Classic's identity metrics.
  EXPECT_FLOAT_EQ(ImGui::GetStyle().FramePadding.x, 7.5f);

  themes.ReapplyTheme(saved);
}

TEST(SettingsPanelTest,
     MinecartManifestProjectOpensPanelWhenExperimentFlagIsOff) {
  FeatureFlagsGuard flags_guard;
  ScopedImGuiContext imgui;
  ScopedManagerProject fixture;
  core::FeatureFlags::get().kEnableCustomObjects = false;

  std::vector<uint8_t> rom_data(512 * 1024, 0);
  constexpr size_t kTitleOffset = 0x7FC0;
  constexpr char kTitle[] = "SETTINGS MINECART";
  std::copy(std::begin(kTitle), std::end(kTitle) - 1,
            rom_data.begin() + kTitleOffset);
  std::ofstream rom_file(fixture.rom_path(),
                         std::ios::binary | std::ios::trunc);
  ASSERT_TRUE(rom_file.is_open());
  rom_file.write(reinterpret_cast<const char*>(rom_data.data()),
                 static_cast<std::streamsize>(rom_data.size()));
  rom_file.close();

  project::YazeProject project;
  project.name = "Settings Minecart";
  project.filepath = fixture.project_path().string();
  project.rom_filename = fixture.rom_path().string();
  const std::filesystem::path manifest_path =
      fixture.root() / "hack_manifest.json";
  std::ofstream manifest_file(manifest_path,
                              std::ios::binary | std::ios::trunc);
  ASSERT_TRUE(manifest_file.is_open());
  manifest_file
      << R"({"manifest_version":3,"minecart_tracks":{"source":{"format":"yaze-minecart-track-table","version":1,"path":"Data/minecart_tracks.asm"}}})";
  manifest_file.close();
  project.hack_manifest_file = manifest_path.string();
  project.feature_flags.kEnableCustomObjects = false;
  ASSERT_TRUE(project.Save().ok());

  auto renderer = std::make_unique<gfx::NullRenderer>();
  auto manager = std::make_unique<EditorManager>();
  ::yaze::test::InitializeWithIsolatedSettings(*manager, renderer.get());
  manager->SetAssetLoadMode(AssetLoadMode::kLazy);
  ASSERT_TRUE(manager->OpenRomOrProject(fixture.project_path().string()).ok());

  auto* session =
      static_cast<RomSession*>(manager->session_coordinator()->GetSession(0));
  ASSERT_NE(session, nullptr);
  auto* dungeon =
      session->editors.GetEditorAs<DungeonEditorV2>(EditorType::kDungeon);
  ASSERT_NE(dungeon, nullptr);
  dungeon->Initialize();

  const size_t dungeon_index = EditorTypeIndex(EditorType::kDungeon);
  session->game_data_loaded = true;
  session->editor_initialized[dungeon_index] = true;
  session->editor_assets_loaded[dungeon_index] = true;
  const size_t session_id = session->session_id();
  ASSERT_EQ(manager->window_manager().GetWindowContent(
                session_id, DungeonEditorV2::kMinecartTrackEditorId),
            nullptr);

  // Project capability admits the tool. The flag controls runtime/write
  // behavior and warning posture, not whether the editor can be opened.
  core::FeatureFlags::get().kEnableCustomObjects = false;
  SettingsPanel* settings = session->editors.GetSettingsPanel();
  ASSERT_NE(settings, nullptr);
  const absl::Status open_status =
      SettingsPanelTestPeer::RequestOpenMinecartTracks(*settings);
  ASSERT_TRUE(open_status.ok()) << open_status;

  EXPECT_NE(manager->window_manager().GetWindowContent(
                session_id, DungeonEditorV2::kMinecartTrackEditorId),
            nullptr);
  EXPECT_TRUE(manager->window_manager().IsWindowOpen(
      session_id, DungeonEditorV2::kMinecartTrackEditorId));
}

}  // namespace
}  // namespace yaze::editor
