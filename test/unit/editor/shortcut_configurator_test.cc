#include <gtest/gtest.h>

#include <memory>

#include "app/editor/editor_manager.h"
#include "app/editor/system/workspace_window_manager.h"
#include "app/editor/system/shortcut_configurator.h"
#include "app/editor/system/shortcut_manager.h"
#include "app/gfx/backend/null_renderer.h"
#include "imgui/imgui.h"

namespace yaze::editor {
namespace {

class ShortcutConfiguratorTest : public ::testing::Test {
 protected:
  void SetUp() override {
    imgui_context_ = ImGui::CreateContext();
    ImGui::SetCurrentContext(imgui_context_);
    ImGuiIO& io = ImGui::GetIO();
    unsigned char* pixels = nullptr;
    int width = 0;
    int height = 0;
    io.Fonts->GetTexDataAsRGBA32(&pixels, &width, &height);

    renderer_ = std::make_unique<gfx::NullRenderer>();
    editor_manager_ = std::make_unique<EditorManager>();
    editor_manager_->Initialize(renderer_.get(), "");

    auto* window_manager = editor_manager_->GetWindowManager();
    window_manager->RegisterSession(0);
    window_manager->SetActiveSession(0);

    test_window_visible_ = false;
    WindowDescriptor descriptor;
    descriptor.card_id = "test.demo";
    descriptor.display_name = "Demo";
    descriptor.icon = "ICON_DEMO";
    descriptor.category = "Test";
    descriptor.visibility_flag = &test_window_visible_;
    descriptor.priority = 1;
    window_manager->RegisterWindow(0, descriptor);
  }

  void TearDown() override {
    editor_manager_.reset();
    renderer_.reset();
    if (imgui_context_) {
      ImGui::DestroyContext(imgui_context_);
      imgui_context_ = nullptr;
    }
  }

  ShortcutManager ConfigureShortcuts() {
    ShortcutManager shortcuts;
    ShortcutDependencies deps;
    deps.editor_manager = editor_manager_.get();
    deps.window_manager = editor_manager_->GetWindowManager();
    ConfigureEditorShortcuts(deps, &shortcuts);
    return shortcuts;
  }

  std::unique_ptr<gfx::NullRenderer> renderer_;
  std::unique_ptr<EditorManager> editor_manager_;
  ImGuiContext* imgui_context_ = nullptr;
  bool test_window_visible_ = false;
};

TEST_F(ShortcutConfiguratorTest, RegistersWindowBrowserAndDrawerAliases) {
  ShortcutManager shortcuts = ConfigureShortcuts();

  const Shortcut* panel_browser = shortcuts.FindShortcut("Panel Browser");
  const Shortcut* window_browser = shortcuts.FindShortcut("Window Browser");
  ASSERT_NE(panel_browser, nullptr);
  ASSERT_NE(window_browser, nullptr);
  EXPECT_EQ(window_browser->keys, panel_browser->keys);

  const Shortcut* panel_browser_alt =
      shortcuts.FindShortcut("Panel Browser (Alt)");
  const Shortcut* window_browser_alt =
      shortcuts.FindShortcut("Window Browser (Alt)");
  ASSERT_NE(panel_browser_alt, nullptr);
  ASSERT_NE(window_browser_alt, nullptr);
  EXPECT_EQ(window_browser_alt->keys, panel_browser_alt->keys);

  EXPECT_NE(shortcuts.FindShortcut("View: Previous Right Drawer"), nullptr);
  EXPECT_NE(shortcuts.FindShortcut("View: Next Right Drawer"), nullptr);
  EXPECT_NE(shortcuts.FindShortcut("View: Toggle Project Drawer"), nullptr);
  EXPECT_NE(shortcuts.FindShortcut("View: Show Window Browser"), nullptr);
}

TEST_F(ShortcutConfiguratorTest, RegistersWindowCommandsAndExecutesDrawerAlias) {
  ShortcutManager shortcuts = ConfigureShortcuts();

  const Shortcut* open_demo_window =
      shortcuts.FindShortcut("View: Open Demo Window");
  ASSERT_NE(open_demo_window, nullptr);
  EXPECT_TRUE(open_demo_window->keys.empty());

  const Shortcut* toggle_demo_window =
      shortcuts.FindShortcut("View: Toggle Demo Window");
  ASSERT_NE(toggle_demo_window, nullptr);
  EXPECT_TRUE(toggle_demo_window->keys.empty());

  auto* drawers = editor_manager_->right_drawer_manager();
  ASSERT_NE(drawers, nullptr);
  EXPECT_EQ(drawers->GetActiveDrawer(), RightDrawerManager::DrawerType::kNone);

  shortcuts.ExecuteShortcut("View: Toggle Project Drawer");
  EXPECT_EQ(drawers->GetActiveDrawer(),
            RightDrawerManager::DrawerType::kProject);

  shortcuts.ExecuteShortcut("View: Open Demo Window");
  EXPECT_TRUE(test_window_visible_);

  shortcuts.ExecuteShortcut("View: Toggle Demo Window");
  EXPECT_FALSE(test_window_visible_);
}

}  // namespace
}  // namespace yaze::editor
