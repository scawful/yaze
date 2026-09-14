#include "app/editor/core/content_registry.h"
#include "app/editor/menu/right_drawer_manager.h"
#include "app/editor/system/command_palette.h"
#include "app/editor/system/editor_panel.h"
#include "app/editor/system/workspace/workspace_window_manager.h"

#include <gtest/gtest.h>

#include <algorithm>
#include <string>
#include <vector>

namespace yaze::editor {
namespace {

class MockWorkflowPanel final : public WindowContent {
 public:
  MockWorkflowPanel(std::string id, bool enabled)
      : id_(std::move(id)), enabled_(enabled) {}

  std::string GetId() const override { return id_; }
  std::string GetDisplayName() const override { return "Story Graph"; }
  std::string GetIcon() const override { return "ICON_MOCK"; }
  std::string GetEditorCategory() const override { return "Agent"; }
  std::string GetWorkflowGroup() const override { return "Planning"; }
  std::string GetWorkflowLabel() const override { return "Open Story Graph"; }
  std::string GetWorkflowDescription() const override {
    return "Open story graph panel";
  }
  bool IsEnabled() const override { return enabled_; }
  void Draw(bool* /*p_open*/) override {}

 private:
  std::string id_;
  bool enabled_ = true;
};

TEST(CommandPaletteWorkflowTest, RegistersEnabledWorkflowPanelsAndActions) {
  WorkspaceWindowManager window_manager;
  window_manager.RegisterSession(0);
  window_manager.SetActiveSession(0);
  window_manager.RegisterWindowContent(
      std::make_unique<MockWorkflowPanel>("test.workflow_panel", true));
  window_manager.RegisterWindowContent(std::make_unique<MockWorkflowPanel>(
      "test.workflow_panel_disabled", false));

  bool action_invoked = false;
  ContentRegistry::WorkflowActions::Clear();
  ContentRegistry::WorkflowActions::Register(
      {.id = "workflow.enabled",
       .group = "Build & Run",
       .label = "Build Project",
       .description = "Run build",
       .shortcut = "",
       .priority = 5,
       .callback = [&action_invoked]() { action_invoked = true; },
       .enabled = []() { return true; }});
  ContentRegistry::WorkflowActions::Register(
      {.id = "workflow.disabled",
       .group = "Build & Run",
       .label = "Disabled Workflow",
       .description = "Disabled",
       .shortcut = "",
       .priority = 10,
       .callback = []() {},
       .enabled = []() { return false; }});

  CommandPalette palette;
  palette.RegisterWorkflowCommands(&window_manager, 0);
  const auto commands = palette.GetAllCommands();

  const auto has_command = [&](const std::string& name) {
    return std::any_of(
        commands.begin(), commands.end(),
        [&](const CommandEntry& entry) { return entry.name == name; });
  };

  EXPECT_TRUE(has_command("Planning: Open Story Graph"));
  EXPECT_TRUE(has_command("Build & Run: Build Project"));
  EXPECT_FALSE(has_command("Build & Run: Disabled Workflow"));

  auto panel_it = std::find_if(
      commands.begin(), commands.end(), [](const CommandEntry& entry) {
        return entry.name == "Planning: Open Story Graph";
      });
  ASSERT_NE(panel_it, commands.end());
  EXPECT_EQ(panel_it->category, CommandCategory::kWorkflow);
  panel_it->callback();
  EXPECT_TRUE(window_manager.IsWindowOpen(0, "test.workflow_panel"));

  auto action_it = std::find_if(
      commands.begin(), commands.end(), [](const CommandEntry& entry) {
        return entry.name == "Build & Run: Build Project";
      });
  ASSERT_NE(action_it, commands.end());
  action_it->callback();
  EXPECT_TRUE(action_invoked);

  ContentRegistry::WorkflowActions::Clear();
}

TEST(CommandPaletteWorkflowTest, RegistersDrawerPrefixedCommands) {
  std::vector<int> toggled;
  bool next = false;
  bool prev = false;

  CommandPalette palette;
  palette.RegisterDrawerCommands([&](int type) { toggled.push_back(type); },
                                 [&]() { next = true; },
                                 [&]() { prev = true; });

  const auto commands = palette.GetAllCommands();
  const auto has_command = [&](const std::string& name) {
    return std::any_of(
        commands.begin(), commands.end(),
        [&](const CommandEntry& entry) { return entry.name == name; });
  };

  EXPECT_TRUE(has_command("drawer: Project"));
  EXPECT_TRUE(has_command("drawer: Settings"));
  EXPECT_TRUE(has_command("drawer: Next"));
  EXPECT_TRUE(has_command("drawer: Previous"));
  EXPECT_EQ(static_cast<size_t>(GetDrawerCatalog().size()) + 2,
            palette.GetCommandCount());

  auto project_it = std::find_if(commands.begin(), commands.end(),
                                 [](const CommandEntry& entry) {
                                   return entry.name == "drawer: Project";
                                 });
  ASSERT_NE(project_it, commands.end());
  EXPECT_EQ(project_it->category, CommandCategory::kDrawer);
  project_it->callback();
  ASSERT_EQ(toggled.size(), 1u);
  EXPECT_EQ(toggled[0],
            static_cast<int>(RightDrawerManager::DrawerType::kProject));

  auto next_it = std::find_if(
      commands.begin(), commands.end(),
      [](const CommandEntry& entry) { return entry.name == "drawer: Next"; });
  ASSERT_NE(next_it, commands.end());
  next_it->callback();
  EXPECT_TRUE(next);

  auto prev_it = std::find_if(commands.begin(), commands.end(),
                              [](const CommandEntry& entry) {
                                return entry.name == "drawer: Previous";
                              });
  ASSERT_NE(prev_it, commands.end());
  prev_it->callback();
  EXPECT_TRUE(prev);

  const auto drawer_hits = palette.SearchCommands("drawer:");
  ASSERT_FALSE(drawer_hits.empty());
  EXPECT_TRUE(drawer_hits.front().name.rfind("drawer:", 0) == 0);
}

TEST(CommandPaletteWorkflowTest, RegistersWindowPrefixedToggleAlias) {
  WorkspaceWindowManager window_manager;
  window_manager.RegisterSession(0);
  window_manager.SetActiveSession(0);

  bool visible = false;
  WindowDescriptor descriptor;
  descriptor.card_id = "test.palette_window";
  descriptor.display_name = "Palette Window";
  descriptor.category = "Test";
  descriptor.visibility_flag = &visible;
  descriptor.priority = 1;
  window_manager.RegisterWindow(0, descriptor);

  CommandPalette palette;
  palette.RegisterPanelCommands(&window_manager, 0);
  const auto commands = palette.GetAllCommands();

  const auto has_command = [&](const std::string& name) {
    return std::any_of(
        commands.begin(), commands.end(),
        [&](const CommandEntry& entry) { return entry.name == name; });
  };

  EXPECT_TRUE(has_command("Toggle: Palette Window"));
  EXPECT_TRUE(has_command("window: Palette Window"));
  EXPECT_TRUE(has_command("Show: Palette Window"));

  auto window_it = std::find_if(commands.begin(), commands.end(),
                                [](const CommandEntry& entry) {
                                  return entry.name == "window: Palette Window";
                                });
  ASSERT_NE(window_it, commands.end());
  EXPECT_FALSE(window_manager.IsWindowOpen(0, "test.palette_window"));
  window_it->callback();
  EXPECT_TRUE(window_manager.IsWindowOpen(0, "test.palette_window"));

  const auto window_hits = palette.SearchCommands("window:");
  ASSERT_FALSE(window_hits.empty());
  EXPECT_TRUE(window_hits.front().name.rfind("window:", 0) == 0);
}

}  // namespace
}  // namespace yaze::editor
