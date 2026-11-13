#include <gtest/gtest.h>

#include <filesystem>
#include <fstream>
#include <string>

#include "app/editor/events/core_events.h"
#include "app/editor/layout/layout_coordinator.h"
#include "app/editor/layout/layout_manager.h"
#include "app/editor/layout/layout_presets.h"
#include "app/editor/registry/content_registry.h"
#include "app/editor/system/workspace/workspace_window_manager.h"
#include "core/features.h"
#include "imgui/imgui.h"
#include "imgui/imgui_internal.h"
#include "unique_temp_path.h"
#include "util/json.h"
#include "util/platform_paths.h"

namespace yaze::editor {
namespace {

constexpr ImGuiID kDockspaceId = 0x41594C54u;

class LayoutManagerPersistenceTest : public ::testing::Test {
 protected:
  void SetUp() override {
    imgui_context_ = ImGui::CreateContext();
    ImGui::SetCurrentContext(imgui_context_);
    ImGuiIO& io = ImGui::GetIO();
    io.DisplaySize = ImVec2(1280.0f, 720.0f);
    io.DeltaTime = 1.0f / 60.0f;
    io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;
    unsigned char* pixels = nullptr;
    int width = 0;
    int height = 0;
    io.Fonts->GetTexDataAsRGBA32(&pixels, &width, &height);
    ImGui::NewFrame();

    window_manager_.RegisterSession(0);
    window_manager_.SetActiveSession(0);

    visible_ = false;
    WindowDescriptor descriptor;
    descriptor.card_id = "test.demo";
    descriptor.display_name = "Demo";
    descriptor.icon = "ICON_DEMO";
    descriptor.category = "Test";
    descriptor.visibility_flag = &visible_;
    descriptor.priority = 1;
    window_manager_.RegisterWindow(0, descriptor);

    WindowDescriptor workbench = descriptor;
    workbench.card_id = LayoutPresets::Panels::kDungeonWorkbench;
    workbench.display_name = "Dungeon Workbench";
    workbench.category = "Dungeon";
    workbench.visibility_flag = &dungeon_workbench_visible_;
    window_manager_.RegisterWindow(0, workbench);

    WindowDescriptor object_selector = workbench;
    object_selector.card_id = LayoutPresets::Panels::kDungeonObjectSelector;
    object_selector.display_name = "Object Selector";
    object_selector.visibility_flag = &dungeon_object_selector_visible_;
    window_manager_.RegisterWindow(0, object_selector);

    layout_manager_.SetWindowManager(&window_manager_);

    // One key per case: ctest -j runs every case in its own process, and a
    // shared key let one case's SetUp/TearDown remove the file another case
    // was reading (all processes share the app-data directory).
    project_key_ =
        ::yaze::test::UniqueTempPath("layout-manager-window-schema-test")
            .filename()
            .string();
    layout_path_ = ResolveProjectLayoutPath(project_key_);
    std::error_code ec;
    std::filesystem::remove(layout_path_, ec);
  }

  void TearDown() override {
    std::error_code ec;
    std::filesystem::remove(layout_path_, ec);
    if (imgui_context_) {
      ImGui::DockBuilderRemoveNode(kDockspaceId);
      ImGui::EndFrame();
      ImGui::DestroyContext(imgui_context_);
      imgui_context_ = nullptr;
    }
  }

  static std::filesystem::path ResolveProjectLayoutPath(
      const std::string& project_key) {
    auto layouts_dir = util::PlatformPaths::GetAppDataSubdirectory("layouts");
    EXPECT_TRUE(layouts_dir.ok());
    std::filesystem::path projects_dir = *layouts_dir / "projects";
    auto ensure_status =
        util::PlatformPaths::EnsureDirectoryExists(projects_dir);
    EXPECT_TRUE(ensure_status.ok());
    return projects_dir / (project_key + ".json");
  }

  ImGuiContext* imgui_context_ = nullptr;
  WorkspaceWindowManager window_manager_;
  LayoutManager layout_manager_;
  bool visible_ = false;
  bool dungeon_workbench_visible_ = false;
  bool dungeon_object_selector_visible_ = false;
  std::string project_key_;
  std::filesystem::path layout_path_;
};

TEST_F(LayoutManagerPersistenceTest, LoadsWindowSchemaAliasFromDisk) {
  yaze::Json root;
  root["version"] = 1;
  root["layouts"] = yaze::Json::object();
  root["layouts"]["Window Alias"]["windows"] = yaze::Json{{"test.demo", true}};

  std::ofstream file(layout_path_);
  ASSERT_TRUE(file.is_open());
  file << root.dump(2);
  file.close();

  visible_ = false;
  layout_manager_.SetProjectLayoutKey(project_key_);
  ASSERT_TRUE(layout_manager_.HasLayout("Window Alias"));

  layout_manager_.LoadLayout("Window Alias");
  EXPECT_TRUE(visible_);
}

TEST_F(LayoutManagerPersistenceTest, SavesWindowSchemaOnly) {
  visible_ = true;
  layout_manager_.SetProjectLayoutKey(project_key_);
  layout_manager_.SaveCurrentLayout("Dual Write", true);

  std::ifstream file(layout_path_);
  ASSERT_TRUE(file.is_open());

  yaze::Json root;
  file >> root;

  ASSERT_TRUE(root.contains("layouts"));
  ASSERT_TRUE(root["layouts"].contains("Dual Write"));
  const auto& entry = root["layouts"]["Dual Write"];
  ASSERT_EQ(root["version"].get<int>(), 2);
  ASSERT_TRUE(entry.contains("windows"));
  EXPECT_FALSE(entry.contains("panels"));
  EXPECT_TRUE(entry["windows"].contains("test.demo"));
  EXPECT_TRUE(entry["windows"]["test.demo"].get<bool>());
}

TEST_F(LayoutManagerPersistenceTest, PrefersWindowsKeyWhenBothSchemasExist) {
  yaze::Json root;
  root["version"] = 1;
  root["layouts"] = yaze::Json::object();
  root["layouts"]["Precedence"]["panels"] = yaze::Json{{"test.demo", false}};
  root["layouts"]["Precedence"]["windows"] = yaze::Json{{"test.demo", true}};

  std::ofstream file(layout_path_);
  ASSERT_TRUE(file.is_open());
  file << root.dump(2);
  file.close();

  visible_ = false;
  layout_manager_.SetProjectLayoutKey(project_key_);
  ASSERT_TRUE(layout_manager_.HasLayout("Precedence"));

  layout_manager_.LoadLayout("Precedence");
  EXPECT_TRUE(visible_);
}

TEST_F(LayoutManagerPersistenceTest,
       VisibilityOnlyLayoutPreservesLazyDefaultDocking) {
  yaze::Json root;
  root["version"] = 2;
  root["layouts"]["Visibility Only"]["windows"] =
      yaze::Json{{"test.demo", true}};

  std::ofstream file(layout_path_);
  ASSERT_TRUE(file.is_open());
  file << root.dump(2);
  file.close();

  layout_manager_.SetProjectLayoutKey(project_key_);
  layout_manager_.InitializeEditorLayout(EditorType::kDungeon, kDockspaceId);
  layout_manager_.LoadLayout("Visibility Only");

  EXPECT_TRUE(layout_manager_.DockDefaultPositionOnFirstOpen(
      0, LayoutPresets::Panels::kDungeonObjectSelector));
}

TEST_F(LayoutManagerPersistenceTest,
       DockingLayoutBeforeFirstEditorArmsDefaultRebuildProtection) {
  yaze::Json root;
  root["version"] = 2;
  root["layouts"]["Docking"]["windows"] = yaze::Json{{"test.demo", true}};
  root["layouts"]["Docking"]["imgui_ini"] =
      "[Window][Demo]\nPos=10,10\nSize=320,240\nCollapsed=0\n\n";

  std::ofstream file(layout_path_);
  ASSERT_TRUE(file.is_open());
  file << root.dump(2);
  file.close();

  layout_manager_.SetProjectLayoutKey(project_key_);
  ASSERT_FALSE(layout_manager_.startup_reapply_pending_protection_for_test());
  layout_manager_.LoadLayout("Docking");

  EXPECT_TRUE(layout_manager_.startup_reapply_pending_protection_for_test());
}

}  // namespace
}  // namespace yaze::editor

namespace yaze::editor {
TEST_F(LayoutManagerPersistenceTest,
       DefaultTrialSurvivesSwitchAndCanRestoreAfterRestart) {
  const auto old_flags = core::FeatureFlags::get().dungeon;
  core::FeatureFlags::get().dungeon.kUseWorkbench = true;
  layout_manager_.SetProjectLayoutKey(project_key_);
  window_manager_.OpenWindow(0, LayoutPresets::Panels::kDungeonObjectSelector);
  window_manager_.CloseWindow(0, LayoutPresets::Panels::kDungeonWorkbench);
  layout_manager_.SaveCurrentLayout("My workspace");
  const auto original = window_manager_.SerializeVisibilityState(0);
  auto persisted = original;
  EventBus bus;
  bus.Subscribe<PanelVisibilityChangedEvent>(
      [&](const auto& e) { persisted[e.base_panel_id] = e.visible; });
  auto* old_bus = ContentRegistry::Context::event_bus();
  ContentRegistry::Context::SetEventBus(&bus);
  LayoutCoordinator coordinator;
  coordinator.Initialize(
      {.layout_manager = &layout_manager_, .window_manager = &window_manager_});
  coordinator.ResetCurrentEditorLayout(EditorType::kDungeon, 0);
  EXPECT_TRUE(dungeon_workbench_visible_);
  EXPECT_FALSE(dungeon_object_selector_visible_);
  window_manager_.RestoreVisibilityState(0, persisted);
  EXPECT_FALSE(dungeon_object_selector_visible_);
  EXPECT_TRUE(dungeon_workbench_visible_);
  // A second trial must not replace the first recovery copy with defaults.
  coordinator.ResetCurrentEditorLayout(EditorType::kDungeon, 0);
  EXPECT_TRUE(coordinator.HasPreviousEditorLayout(EditorType::kDungeon));
  LayoutManager reopened;
  reopened.SetWindowManager(&window_manager_);
  reopened.SetProjectLayoutKey(project_key_);
  LayoutCoordinator resumed;
  resumed.Initialize(
      {.layout_manager = &reopened, .window_manager = &window_manager_});
  EXPECT_TRUE(resumed.RestorePreviousEditorLayout(EditorType::kDungeon));
  EXPECT_EQ(window_manager_.SerializeVisibilityState(0), original);
  EXPECT_FALSE(resumed.HasPreviousEditorLayout(EditorType::kDungeon));
  EXPECT_TRUE(reopened.HasLayout("My workspace"));
  ContentRegistry::Context::SetEventBus(old_bus);
  core::FeatureFlags::get().dungeon = old_flags;
}

TEST_F(LayoutManagerPersistenceTest, NamedPresetKeepsOpenDungeonWorkbench) {
  LayoutCoordinator coordinator;
  coordinator.Initialize(
      {.layout_manager = &layout_manager_, .window_manager = &window_manager_});
  window_manager_.OpenWindow(0, LayoutPresets::Panels::kDungeonWorkbench);
  window_manager_.OpenWindow(0, "test.demo");

  coordinator.ApplyLayoutPreset("Minimal", 0);

  EXPECT_TRUE(dungeon_workbench_visible_);
  EXPECT_FALSE(visible_);
}

TEST_F(LayoutManagerPersistenceTest, NamedPresetDoesNotOpenClosedWorkbench) {
  LayoutCoordinator coordinator;
  coordinator.Initialize(
      {.layout_manager = &layout_manager_, .window_manager = &window_manager_});
  window_manager_.CloseWindow(0, LayoutPresets::Panels::kDungeonWorkbench);

  coordinator.ApplyLayoutPreset("Dungeon Expert", 0);

  EXPECT_FALSE(dungeon_workbench_visible_);
}
}  // namespace yaze::editor
