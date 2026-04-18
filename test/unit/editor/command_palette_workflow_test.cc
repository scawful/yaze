#include "app/editor/core/content_registry.h"
#include "app/editor/system/command_palette.h"
#include "app/editor/system/editor_panel.h"
#include "app/editor/system/workspace_window_manager.h"

#include <gtest/gtest.h>

#include <algorithm>
#include <string>

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
  window_manager.RegisterWindowContent(
      std::make_unique<MockWorkflowPanel>("test.workflow_panel_disabled", false));

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
    return std::any_of(commands.begin(), commands.end(),
                       [&](const CommandEntry& entry) { return entry.name == name; });
  };

  EXPECT_TRUE(has_command("Planning: Open Story Graph"));
  EXPECT_TRUE(has_command("Build & Run: Build Project"));
  EXPECT_FALSE(has_command("Build & Run: Disabled Workflow"));

  auto panel_it = std::find_if(commands.begin(), commands.end(),
                               [](const CommandEntry& entry) {
                                 return entry.name == "Planning: Open Story Graph";
                               });
  ASSERT_NE(panel_it, commands.end());
  EXPECT_EQ(panel_it->category, CommandCategory::kWorkflow);
  panel_it->callback();
  EXPECT_TRUE(window_manager.IsWindowOpen(0, "test.workflow_panel"));

  auto action_it = std::find_if(commands.begin(), commands.end(),
                                [](const CommandEntry& entry) {
                                  return entry.name == "Build & Run: Build Project";
                                });
  ASSERT_NE(action_it, commands.end());
  action_it->callback();
  EXPECT_TRUE(action_invoked);

  ContentRegistry::WorkflowActions::Clear();
}

}  // namespace
}  // namespace yaze::editor
