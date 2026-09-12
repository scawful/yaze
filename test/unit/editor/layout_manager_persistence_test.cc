#include <gtest/gtest.h>

#include <filesystem>
#include <fstream>
#include <string>

#include "app/editor/layout/layout_manager.h"
#include "app/editor/layout/layout_presets.h"
#include "app/editor/system/workspace/workspace_window_manager.h"
#include "imgui/imgui.h"
#include "imgui/imgui_internal.h"
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

    project_key_ = "layout-manager-window-schema-test";
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
