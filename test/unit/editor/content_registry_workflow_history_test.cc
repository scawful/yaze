#include "app/editor/core/content_registry.h"
#include "app/editor/dungeon/dungeon_editor_v2.h"
#include "app/editor/dungeon/ui/window/dungeon_panel_access.h"
#include "app/editor/overworld/overworld_editor.h"
#include "app/editor/overworld/panels/overworld_panel_access.h"

#include <gtest/gtest.h>

#include <chrono>

namespace yaze::editor {
namespace {

TEST(ContentRegistryWorkflowHistoryTest, StoresNewestHistoryEntriesFirst) {
  ContentRegistry::Context::ClearWorkflowHistory();

  ContentRegistry::Context::AppendWorkflowHistory(
      {.kind = "Build",
       .status = {.visible = true,
                  .label = "Build",
                  .summary = "Build succeeded",
                  .detail = "first",
                  .state = ProjectWorkflowState::kSuccess},
       .output_log = "first log",
       .timestamp =
           std::chrono::system_clock::now() - std::chrono::minutes(1)});
  ContentRegistry::Context::AppendWorkflowHistory(
      {.kind = "Run",
       .status = {.visible = true,
                  .label = "Run",
                  .summary = "Reloaded in emulator",
                  .detail = "second",
                  .state = ProjectWorkflowState::kSuccess},
       .output_log = "",
       .timestamp = std::chrono::system_clock::now()});

  const auto history = ContentRegistry::Context::workflow_history();
  ASSERT_EQ(history.size(), 2u);
  EXPECT_EQ(history[0].kind, "Run");
  EXPECT_EQ(history[0].status.summary, "Reloaded in emulator");
  EXPECT_EQ(history[1].kind, "Build");
  EXPECT_EQ(history[1].output_log, "first log");

  ContentRegistry::Context::ClearWorkflowHistory();
  EXPECT_TRUE(ContentRegistry::Context::workflow_history().empty());
}

TEST(ContentRegistryWorkflowHistoryTest, StoresWorkflowCallbacks) {
  bool build_invoked = false;
  bool run_invoked = false;
  bool output_invoked = false;
  bool cancel_invoked = false;

  ContentRegistry::Context::SetStartBuildWorkflowCallback(
      [&build_invoked]() { build_invoked = true; });
  ContentRegistry::Context::SetRunProjectWorkflowCallback(
      [&run_invoked]() { run_invoked = true; });
  ContentRegistry::Context::SetShowWorkflowOutputCallback(
      [&output_invoked]() { output_invoked = true; });
  ContentRegistry::Context::SetCancelBuildWorkflowCallback(
      [&cancel_invoked]() { cancel_invoked = true; });

  ASSERT_TRUE(ContentRegistry::Context::start_build_workflow_callback());
  ASSERT_TRUE(ContentRegistry::Context::run_project_workflow_callback());
  ASSERT_TRUE(ContentRegistry::Context::show_workflow_output_callback());
  ASSERT_TRUE(ContentRegistry::Context::cancel_build_workflow_callback());

  ContentRegistry::Context::start_build_workflow_callback()();
  ContentRegistry::Context::run_project_workflow_callback()();
  ContentRegistry::Context::show_workflow_output_callback()();
  ContentRegistry::Context::cancel_build_workflow_callback()();

  EXPECT_TRUE(build_invoked);
  EXPECT_TRUE(run_invoked);
  EXPECT_TRUE(output_invoked);
  EXPECT_TRUE(cancel_invoked);

  ContentRegistry::Context::Clear();
}

TEST(ContentRegistryWorkflowHistoryTest, StoresTypedWindowEditorContexts) {
  OverworldEditor overworld_editor(nullptr);
  DungeonEditorV2 dungeon_editor(nullptr);

  ContentRegistry::Context::SetEditorWindowContext("Overworld",
                                                   &overworld_editor);
  ContentRegistry::Context::SetEditorWindowContext("Dungeon", &dungeon_editor);

  EXPECT_EQ(ContentRegistry::Context::editor_window_context("Overworld"),
            static_cast<Editor*>(&overworld_editor));
  EXPECT_EQ(ContentRegistry::Context::editor_window_context("Dungeon"),
            static_cast<Editor*>(&dungeon_editor));
  EXPECT_EQ(CurrentOverworldWindowContext().editor, &overworld_editor);
  EXPECT_EQ(CurrentDungeonWindowContext().editor, &dungeon_editor);

  ContentRegistry::Context::Clear();
  EXPECT_EQ(ContentRegistry::Context::editor_window_context("Overworld"),
            nullptr);
  EXPECT_EQ(ContentRegistry::Context::editor_window_context("Dungeon"),
            nullptr);
}

}  // namespace
}  // namespace yaze::editor
