#include "e2e/workflow_activity_widgets_test.h"

#include <string>

#include "app/editor/hack/workflow/workflow_activity_widgets.h"
#include "imgui.h"
#include "imgui_test_engine/imgui_te_context.h"
#include "imgui_test_engine/imgui_te_engine.h"

#include "absl/strings/str_replace.h"

namespace yaze::test::e2e {
namespace {

using ::yaze::editor::ProjectWorkflowHistoryEntry;
using ::yaze::editor::ProjectWorkflowState;
using ::yaze::editor::workflow::DrawCopyCurrentLogButton;
using ::yaze::editor::workflow::DrawHistoryActionRow;
using ::yaze::editor::workflow::WorkflowActionCallbacks;
using ::yaze::editor::workflow::WorkflowActionRowResult;
using ::yaze::editor::workflow::WorkflowButtonRect;

struct WorkflowWidgetGuiState {
  int build_output_clicks = 0;
  int build_rebuild_clicks = 0;
  int run_again_clicks = 0;
  std::string clipboard;
  WorkflowButtonRect build_output_button;
  WorkflowButtonRect build_rebuild_button;
  WorkflowButtonRect build_copy_button;
  WorkflowButtonRect run_again_button;
  WorkflowButtonRect run_copy_button;
  WorkflowButtonRect current_copy_button;

  void Reset() {
    build_output_clicks = 0;
    build_rebuild_clicks = 0;
    run_again_clicks = 0;
    clipboard.clear();
    build_output_button = WorkflowButtonRect{};
    build_rebuild_button = WorkflowButtonRect{};
    build_copy_button = WorkflowButtonRect{};
    run_again_button = WorkflowButtonRect{};
    run_copy_button = WorkflowButtonRect{};
    current_copy_button = WorkflowButtonRect{};
  }
};

WorkflowWidgetGuiState& GetWorkflowWidgetGuiState() {
  static WorkflowWidgetGuiState state;
  return state;
}

const char* GetClipboardText(void* user_data) {
  return static_cast<WorkflowWidgetGuiState*>(user_data)->clipboard.c_str();
}

void SetClipboardText(void* user_data, const char* text) {
  static_cast<WorkflowWidgetGuiState*>(user_data)->clipboard =
      text != nullptr ? text : "";
}

const char* GetPlatformClipboardText(ImGuiContext* ctx) {
  return GetClipboardText(ctx->IO.ClipboardUserData);
}

void SetPlatformClipboardText(ImGuiContext* ctx, const char* text) {
  SetClipboardText(ctx->IO.ClipboardUserData, text);
}

void ConfigureClipboard() {
  auto& state = GetWorkflowWidgetGuiState();
  ImGuiIO& io = ImGui::GetIO();
  ImGuiPlatformIO& platform_io = ImGui::GetPlatformIO();
  io.SetClipboardTextFn = &SetClipboardText;
  io.GetClipboardTextFn = &GetClipboardText;
  io.ClipboardUserData = &state;
  platform_io.Platform_SetClipboardTextFn = &SetPlatformClipboardText;
  platform_io.Platform_GetClipboardTextFn = &GetPlatformClipboardText;
}

std::string ClipboardDisplayText(const std::string& text) {
  if (text.empty()) {
    return "<empty>";
  }
  return absl::StrReplaceAll(text, {{"\n", " | "}});
}

ProjectWorkflowHistoryEntry MakeBuildEntry() {
  return {.kind = "Build",
          .status = {.label = "Build",
                     .summary = "Build succeeded",
                     .detail = "done",
                     .state = ProjectWorkflowState::kSuccess},
          .output_log = "build-log",
          .timestamp = std::chrono::system_clock::now()};
}

ProjectWorkflowHistoryEntry MakeRunEntry() {
  return {.kind = "Run",
          .status = {.label = "Run",
                     .summary = "Reloaded in emulator",
                     .detail = "patched.sfc",
                     .state = ProjectWorkflowState::kSuccess},
          .output_log = "run-log",
          .timestamp = std::chrono::system_clock::now()};
}

void DrawWorkflowActivityWidgetWindows(ImGuiTestContext* ctx) {
  (void)ctx;
  auto& state = GetWorkflowWidgetGuiState();
  ConfigureClipboard();

  ImGui::SetNextWindowPos(ImVec2(20.0f, 20.0f), ImGuiCond_Always);
  if (ImGui::Begin("Workflow Build History")) {
    WorkflowActionCallbacks callbacks;
    callbacks.show_output = [&state]() { ++state.build_output_clicks; };
    callbacks.start_build = [&state]() { ++state.build_rebuild_clicks; };
    ImGui::PushID("build_entry");
    const WorkflowActionRowResult build_row =
        DrawHistoryActionRow(MakeBuildEntry(), callbacks,
                             {.show_open_output = true, .show_copy_log = true});
    ImGui::PopID();
    state.build_output_button = build_row.open_output;
    state.build_rebuild_button = build_row.primary_action;
    state.build_copy_button = build_row.copy_log;
    ImGui::Text("Build output clicks: %d", state.build_output_clicks);
    ImGui::Text("Build rebuild clicks: %d", state.build_rebuild_clicks);
    const std::string clipboard_text = ClipboardDisplayText(state.clipboard);
    ImGui::Text("Clipboard: %s", clipboard_text.c_str());
  }
  ImGui::End();

  ImGui::SetNextWindowPos(ImVec2(20.0f, 180.0f), ImGuiCond_Always);
  if (ImGui::Begin("Workflow Run History")) {
    WorkflowActionCallbacks callbacks;
    callbacks.run_project = [&state]() { ++state.run_again_clicks; };
    ImGui::PushID("run_entry");
    const WorkflowActionRowResult run_row =
        DrawHistoryActionRow(MakeRunEntry(), callbacks,
                             {.show_open_output = false, .show_copy_log = true});
    ImGui::PopID();
    state.run_again_button = run_row.primary_action;
    state.run_copy_button = run_row.copy_log;
    ImGui::Text("Run again clicks: %d", state.run_again_clicks);
    const std::string clipboard_text = ClipboardDisplayText(state.clipboard);
    ImGui::Text("Clipboard: %s", clipboard_text.c_str());
  }
  ImGui::End();

  ImGui::SetNextWindowPos(ImVec2(20.0f, 340.0f), ImGuiCond_Always);
  if (ImGui::Begin("Workflow Current Log")) {
    state.current_copy_button = DrawCopyCurrentLogButton("line one\nline two");
    const std::string clipboard_text = ClipboardDisplayText(state.clipboard);
    ImGui::Text("Clipboard: %s", clipboard_text.c_str());
  }
  ImGui::End();
}

}  // namespace

void RegisterWorkflowActivityWidgetTests(ImGuiTestEngine* engine) {
  ImGuiTest* test =
      IM_REGISTER_TEST(engine, "E2ETest", "WorkflowActivityWidgets");
  test->GuiFunc = DrawWorkflowActivityWidgetWindows;
  test->TestFunc = [](ImGuiTestContext* ctx) {
    auto& state = GetWorkflowWidgetGuiState();
    state.Reset();
    ctx->Yield(2);

    IM_CHECK(state.build_output_button.visible);
    IM_CHECK(state.build_rebuild_button.visible);
    IM_CHECK(state.build_copy_button.visible);
    IM_CHECK(state.run_again_button.visible);
    IM_CHECK(state.run_copy_button.visible);
    IM_CHECK(state.current_copy_button.visible);

    ctx->MouseMoveToPos(state.build_output_button.Center());
    ctx->MouseClick();
    ctx->Yield(1);
    IM_CHECK_EQ(state.build_output_clicks, 1);

    ctx->MouseMoveToPos(state.build_rebuild_button.Center());
    ctx->MouseClick();
    ctx->Yield(1);
    IM_CHECK_EQ(state.build_rebuild_clicks, 1);

    ctx->MouseMoveToPos(state.build_copy_button.Center());
    ctx->MouseClick();
    ctx->Yield(1);
    IM_CHECK_STR_EQ(state.clipboard.c_str(), "build-log");

    ctx->MouseMoveToPos(state.run_again_button.Center());
    ctx->MouseClick();
    ctx->Yield(1);
    IM_CHECK_EQ(state.run_again_clicks, 1);

    ctx->MouseMoveToPos(state.run_copy_button.Center());
    ctx->MouseClick();
    ctx->Yield(1);
    IM_CHECK_STR_EQ(state.clipboard.c_str(), "run-log");

    ctx->MouseMoveToPos(state.current_copy_button.Center());
    ctx->MouseClick();
    ctx->Yield(1);
    IM_CHECK_STR_EQ(state.clipboard.c_str(), "line one\nline two");
  };
}

}  // namespace yaze::test::e2e
