#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include <filesystem>
#include <fstream>
#include <string>

#include "app/editor/editor_manager.h"
#include "app/editor/layout/layout_coordinator.h"
#include "app/editor/registry/content_registry.h"
#include "app/gfx/backend/irenderer.h"
#include "app/gfx/backend/null_renderer.h"
#include "app/platform/null_window_backend.h"
#include "editor_test_support.h"
#include "imgui/imgui.h"
#include "zelda3/resource_labels.h"

namespace yaze {
namespace editor {

class EditorManagerLayoutTestPeer {
 public:
  static void RestoreEditorLayoutAfterAssets(EditorManager* manager,
                                             RomSession* session,
                                             EditorType type) {
    manager->RestoreEditorLayoutAfterAssets(session, type);
  }
};

namespace {

class EditorManagerTest : public ::testing::Test {
 protected:
  void SetUp() override {
    imgui_context_ = ImGui::CreateContext();
    ImGui::SetCurrentContext(imgui_context_);
    ImGuiIO& io = ImGui::GetIO();
    unsigned char* pixels;
    int width = 0;
    int height = 0;
    io.Fonts->GetTexDataAsRGBA32(&pixels, &width, &height);
    // Setup minimal dependencies
    renderer_ = std::make_unique<gfx::NullRenderer>();
    editor_manager_ = std::make_unique<EditorManager>();
  }

  void TearDown() override {
    editor_manager_.reset();
    renderer_.reset();
    if (imgui_context_ != nullptr) {
      ImGui::DestroyContext(imgui_context_);
      imgui_context_ = nullptr;
    }
  }

  std::unique_ptr<gfx::NullRenderer> renderer_;
  std::unique_ptr<EditorManager> editor_manager_;
  ImGuiContext* imgui_context_ = nullptr;
};

TEST_F(EditorManagerTest, UpdateWithoutCrash) {
  ::yaze::test::InitializeWithIsolatedSettings(*editor_manager_,
                                               renderer_.get());

  ImGuiIO& io = ImGui::GetIO();
  io.DisplaySize = ImVec2(1280, 720);
  io.DeltaTime = 1.0f / 60.0f;
  ImGui::NewFrame();

  // Update should return OkStatus
  auto status = editor_manager_->Update();
  EXPECT_TRUE(status.ok()) << status.message();

  ImGui::EndFrame();
  ImGui::Render();
}

TEST(LayoutCoordinatorChromeBudgetTest,
     SidePanelsOverlayBeforeTheyCrushWorkspaceCanvas) {
  const auto wide = LayoutCoordinator::ResolveWorkspaceChromeBudget(
      1600.0f, 48.0f, 300.0f, 480.0f);
  EXPECT_FLOAT_EQ(wide.left_offset, 348.0f);
  EXPECT_FLOAT_EQ(wide.right_offset, 480.0f);
  EXPECT_FALSE(wide.side_panels_overlay);

  const auto constrained = LayoutCoordinator::ResolveWorkspaceChromeBudget(
      1200.0f, 48.0f, 300.0f, 480.0f);
  EXPECT_FLOAT_EQ(constrained.left_offset, 48.0f);
  EXPECT_FLOAT_EQ(constrained.right_offset, 0.0f);
  EXPECT_TRUE(constrained.side_panels_overlay);

  const auto opening = LayoutCoordinator::ResolveWorkspaceChromeBudget(
      1200.0f, 48.0f, 300.0f, 100.0f, 480.0f);
  EXPECT_FLOAT_EQ(opening.left_offset, 48.0f);
  EXPECT_FLOAT_EQ(opening.right_offset, 0.0f);
  EXPECT_TRUE(opening.side_panels_overlay);

  const auto left_only = LayoutCoordinator::ResolveWorkspaceChromeBudget(
      1000.0f, 48.0f, 300.0f, 0.0f);
  EXPECT_FLOAT_EQ(left_only.left_offset, 348.0f);
  EXPECT_FLOAT_EQ(left_only.right_offset, 0.0f);
  EXPECT_FALSE(left_only.side_panels_overlay);
}

TEST_F(EditorManagerTest,
       TargetedDungeonDefaultsMigrationPreservesWorkspaceAndWidths) {
  const std::filesystem::path settings_path =
      std::filesystem::temp_directory_path() /
      ("yaze_editor_manager_targeted_layout_" + std::to_string(::getpid()) +
       ".json");
  {
    std::ofstream settings_file(settings_path);
    ASSERT_TRUE(settings_file.is_open());
    settings_file << R"({
      "layouts": {
        "defaults_revision": 21,
        "panel_visibility": {
          "Dungeon": {
            "dungeon.workbench": false,
            "dungeon.object_selector": true,
            "dungeon.door_editor": true
          }
        },
        "pinned_panels": {"dungeon.object_selector": true},
        "right_panel_widths": {
          "agent_chat": 777.0,
          "dungeon.workbench": 444.0
        },
        "saved_layouts": {
          "custom": {"dungeon.object_selector": true}
        },
        "named_layouts": {
          "favorite": {
            "schema_version": 2,
            "name": "favorite",
            "root": {
              "id": 1,
              "type": "leaf",
              "active_tab_index": 0,
              "panels": [
                {"panel_id": "dungeon.object_selector"}
              ]
            }
          }
        }
      },
      "sidebar": {
        "active_category": "Dungeon",
        "visible": true,
        "panel_expanded": true
      }
    })";
  }

  editor_manager_->user_settings().SetSettingsFilePathForTesting(
      settings_path.string());
  static constexpr char kSentinelIni[] =
      "[Window][MigrationSentinel]\n"
      "Pos=11,22\n"
      "Size=333,222\n"
      "Collapsed=0\n\n";
  ImGui::LoadIniSettingsFromMemory(kSentinelIni, sizeof(kSentinelIni) - 1);

  editor_manager_->Initialize(renderer_.get(), "");

  const auto& prefs = editor_manager_->user_settings().prefs();
  EXPECT_EQ(prefs.panel_layout_defaults_revision,
            UserSettings::kLatestPanelLayoutDefaultsRevision);
  EXPECT_FALSE(prefs.sidebar_panel_expanded);
  EXPECT_TRUE(
      prefs.panel_visibility_state.at("Dungeon").at("dungeon.workbench"));
  EXPECT_FALSE(
      prefs.panel_visibility_state.at("Dungeon").at("dungeon.object_selector"));
  EXPECT_FALSE(
      prefs.panel_visibility_state.at("Dungeon").at("dungeon.door_editor"));
  EXPECT_TRUE(prefs.pinned_panels.at("dungeon.object_selector"));
  EXPECT_TRUE(prefs.saved_layouts.at("custom").at("dungeon.object_selector"));
  EXPECT_NE(prefs.named_layouts.at("favorite").find("dungeon.object_selector"),
            std::string::npos);
  EXPECT_EQ(prefs.right_panel_widths.count("agent_chat"), 0U);
  EXPECT_FLOAT_EQ(prefs.right_panel_widths.at("right_sidebar.shared"), 777.0f);
  EXPECT_FLOAT_EQ(prefs.right_panel_widths.at("dungeon.workbench"), 444.0f);

  EXPECT_FALSE(editor_manager_->window_manager().IsSidebarExpanded());

  size_t ini_size = 0;
  const char* ini_data = ImGui::SaveIniSettingsToMemory(&ini_size);
  ASSERT_NE(ini_data, nullptr);
  EXPECT_NE(std::string(ini_data, ini_size).find("MigrationSentinel"),
            std::string::npos);

  std::error_code ec;
  std::filesystem::remove(settings_path, ec);
}

TEST_F(EditorManagerTest,
       PostRegistrationRestoreAppliesLazySecondSessionPanelVisibility) {
  ::yaze::test::InitializeWithIsolatedSettings(*editor_manager_,
                                               renderer_.get());

  constexpr size_t kSecondSessionId = 7;
  RomSession second_session(&editor_manager_->user_settings(),
                            kSecondSessionId);
  second_session.editor_initialized[EditorTypeIndex(EditorType::kDungeon)] =
      true;

  bool object_selector_visible = false;
  WindowDescriptor descriptor{};
  descriptor.card_id = LayoutPresets::Panels::kDungeonObjectSelector;
  descriptor.display_name = "Object Selector";
  descriptor.icon = "ICON_MD_ACCOUNT_TREE";
  descriptor.category = "Dungeon";
  descriptor.priority = 1;
  descriptor.visibility_flag = &object_selector_visible;

  auto& window_manager = editor_manager_->window_manager();
  window_manager.RegisterSession(kSecondSessionId);
  window_manager.SetActiveSession(kSecondSessionId);
  window_manager.SetActiveCategory("Dungeon");
  window_manager.RegisterWindow(kSecondSessionId, descriptor);
  editor_manager_->user_settings()
      .prefs()
      .panel_visibility_state["Dungeon"]
                             [LayoutPresets::Panels::kDungeonObjectSelector] =
      true;

  EditorManagerLayoutTestPeer::RestoreEditorLayoutAfterAssets(
      editor_manager_.get(), &second_session, EditorType::kDungeon);

  EXPECT_TRUE(object_selector_visible);
}

TEST_F(EditorManagerTest,
       RepeatedEnsureDoesNotReplayStaleVisibilityAfterPanelClose) {
  ::yaze::test::InitializeWithIsolatedSettings(*editor_manager_,
                                               renderer_.get());
  editor_manager_->CreateNewSession();

  auto& window_manager = editor_manager_->window_manager();
  RomSession* active_session =
      editor_manager_->session_coordinator()->GetActiveRomSession();
  ASSERT_NE(active_session, nullptr);
  const size_t session_id = window_manager.GetActiveSessionId();
  window_manager.SetActiveCategory("Assembly");
  editor_manager_->user_settings()
      .prefs()
      .panel_visibility_state["Assembly"]
                             [LayoutPresets::Panels::kAssemblyEditor] = true;

  ASSERT_TRUE(
      editor_manager_->EnsureEditorAssetsLoaded(EditorType::kAssembly).ok());
  const WindowDescriptor* descriptor = window_manager.GetWindowDescriptor(
      session_id, LayoutPresets::Panels::kAssemblyEditor);
  ASSERT_NE(descriptor, nullptr);
  ASSERT_NE(descriptor->visibility_flag, nullptr);
  ASSERT_TRUE(*descriptor->visibility_flag);

  // Model an ImGui title-bar X close. That path changes the live visibility
  // flag before the debounced settings snapshot catches up.
  *descriptor->visibility_flag = false;
  ASSERT_TRUE(editor_manager_->user_settings()
                  .prefs()
                  .panel_visibility_state.at("Assembly")
                  .at(LayoutPresets::Panels::kAssemblyEditor));

  ASSERT_TRUE(
      editor_manager_->EnsureEditorAssetsLoaded(EditorType::kAssembly).ok());
  EXPECT_FALSE(*descriptor->visibility_flag);
}

// Regression: ~EditorManager must clear the static singletons that hold
// non-owning pointers into its members. Without this, a subsequent test that
// reads e.g. ContentRegistry::Context::event_bus() (via Canvas::set_global_scale)
// or zelda3::GetResourceLabels() (via Sprite::Sprite -> ResolveSpriteName)
// dereferences freed memory.
TEST(EditorManagerLifecycleTest, DestructorClearsSingletonPointers) {
  ImGuiContext* ctx = ImGui::CreateContext();
  ImGui::SetCurrentContext(ctx);
  unsigned char* pixels;
  int width = 0;
  int height = 0;
  ImGui::GetIO().Fonts->GetTexDataAsRGBA32(&pixels, &width, &height);

  {
    EditorManager manager;
    EXPECT_NE(ContentRegistry::Context::event_bus(), nullptr);
  }
  EXPECT_EQ(ContentRegistry::Context::event_bus(), nullptr);
  EXPECT_EQ(zelda3::GetResourceLabels().GetAllProjectLabels(), nullptr);

  ImGui::DestroyContext(ctx);
}

TEST(EditorManagerStartupFlagsTest, ParsesCategoryAliasAndFullEditorName) {
  auto alias = ParseEditorTypeFromString("Overworld");
  ASSERT_TRUE(alias.has_value());
  EXPECT_EQ(*alias, EditorType::kOverworld);

  auto full_name = ParseEditorTypeFromString("Overworld Editor");
  ASSERT_TRUE(full_name.has_value());
  EXPECT_EQ(*full_name, EditorType::kOverworld);

  auto trimmed_lower = ParseEditorTypeFromString("  overworld editor  ");
  ASSERT_TRUE(trimmed_lower.has_value());
  EXPECT_EQ(*trimmed_lower, EditorType::kOverworld);

  EXPECT_FALSE(ParseEditorTypeFromString("No Such Editor").has_value());
}

}  // namespace
}  // namespace editor
}  // namespace yaze
