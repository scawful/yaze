#include "app/editor/hack/workflow/workflow_activity_widgets.h"

#include <gtest/gtest.h>

#include <chrono>
#include <filesystem>
#include <fstream>
#include <functional>
#include <string>
#include <vector>

#include "app/editor/hack/workflow/build_log_view.h"
#include "imgui/imgui.h"
#include "imgui/imgui_internal.h"

namespace yaze::editor::workflow {
namespace {

class WorkflowActivityWidgetsTest : public ::testing::Test {
 protected:
  struct ClipboardState {
    std::string text;
  };

  static const char* GetClipboardText(void* user_data) {
    return static_cast<ClipboardState*>(user_data)->text.c_str();
  }

  static void SetClipboardText(void* user_data, const char* text) {
    static_cast<ClipboardState*>(user_data)->text = text != nullptr ? text : "";
  }

  void SetUp() override {
    context_ = ImGui::CreateContext();
    ImGui::SetCurrentContext(context_);

    ImGuiIO& io = ImGui::GetIO();
    io.DisplaySize = ImVec2(1280.0f, 720.0f);
    io.DeltaTime = 1.0f / 60.0f;
    io.Fonts->AddFontDefault();
    io.SetClipboardTextFn = &SetClipboardText;
    io.GetClipboardTextFn = &GetClipboardText;
    io.ClipboardUserData = &clipboard_;

    unsigned char* pixels = nullptr;
    int width = 0;
    int height = 0;
    io.Fonts->GetTexDataAsRGBA32(&pixels, &width, &height);
  }

  void TearDown() override {
    if (context_ != nullptr) {
      ImGui::DestroyContext(context_);
      context_ = nullptr;
    }
  }

  template <typename DrawFn>
  auto RunFrame(DrawFn&& draw,
                std::function<void(ImGuiIO&)> inject = nullptr)
      -> decltype(draw()) {
    ImGuiIO& io = ImGui::GetIO();
    if (inject) {
      inject(io);
    }
    io.DisplaySize = ImVec2(1280.0f, 720.0f);
    io.DeltaTime = 1.0f / 60.0f;

    ImGui::NewFrame();
    ImGui::SetNextWindowPos(ImVec2(0.0f, 0.0f));
    ImGui::SetNextWindowSize(ImVec2(480.0f, 320.0f));
    ImGui::SetNextWindowFocus();
    ImGui::Begin("WorkflowWidgetsHost", nullptr,
                 ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoMove |
                     ImGuiWindowFlags_NoResize |
                     ImGuiWindowFlags_NoSavedSettings);
    auto result = draw();
    ImGui::End();
    ImGui::EndFrame();
    ImGui::Render();
    return result;
  }

  ImGuiContext* context_ = nullptr;
  ClipboardState clipboard_;
};

TEST_F(WorkflowActivityWidgetsTest, SelectWorkflowPreviewEntriesKeepsNewestItems) {
  const auto now = std::chrono::system_clock::now();
  std::vector<ProjectWorkflowHistoryEntry> history = {
      {.kind = "Run", .status = {.summary = "run"}, .timestamp = now},
      {.kind = "Build",
       .status = {.summary = "build 1"},
       .timestamp = now - std::chrono::minutes(1)},
      {.kind = "Build",
       .status = {.summary = "build 2"},
       .timestamp = now - std::chrono::minutes(2)},
      {.kind = "Run",
       .status = {.summary = "run older"},
       .timestamp = now - std::chrono::minutes(3)},
  };

  const auto preview = SelectWorkflowPreviewEntries(history, 3);
  ASSERT_EQ(preview.size(), 3u);
  EXPECT_EQ(preview[0].status.summary, "run");
  EXPECT_EQ(preview[1].status.summary, "build 1");
  EXPECT_EQ(preview[2].status.summary, "build 2");
}

TEST_F(WorkflowActivityWidgetsTest,
       HistoryActionRowRendersBuildAndOpenOutputButtons) {
  const ProjectWorkflowHistoryEntry entry{
      .kind = "Build",
      .status = {.label = "Build",
                 .summary = "Build succeeded",
                 .detail = "done",
                 .state = ProjectWorkflowState::kSuccess},
      .output_log = "build-log",
      .timestamp = std::chrono::system_clock::now()};

  WorkflowActionCallbacks callbacks;
  callbacks.start_build = []() {};
  callbacks.show_output = []() {};

  auto draw = [&]() {
    ImGui::PushID("build_entry");
    const auto result = DrawHistoryActionRow(
        entry, callbacks, {.show_open_output = true, .show_copy_log = true});
    ImGui::PopID();
    return result;
  };

  const auto initial = RunFrame(draw);
  EXPECT_TRUE(initial.open_output.visible);
  EXPECT_NE(initial.open_output.id, 0u);
  EXPECT_TRUE(initial.primary_action.visible);
  EXPECT_NE(initial.primary_action.id, 0u);
  EXPECT_TRUE(initial.copy_log.visible);
  EXPECT_NE(initial.copy_log.id, 0u);
}

TEST_F(WorkflowActivityWidgetsTest, HistoryActionRowRendersRunAndCopyButtons) {
  const ProjectWorkflowHistoryEntry entry{
      .kind = "Run",
      .status = {.label = "Run",
                 .summary = "Reloaded in emulator",
                 .detail = "patched.sfc",
                 .state = ProjectWorkflowState::kSuccess},
      .output_log = "run-log",
      .timestamp = std::chrono::system_clock::now()};

  WorkflowActionCallbacks callbacks;
  callbacks.run_project = []() {};

  auto draw = [&]() {
    ImGui::PushID("run_entry");
    const auto result = DrawHistoryActionRow(
        entry, callbacks, {.show_open_output = false, .show_copy_log = true});
    ImGui::PopID();
    return result;
  };

  const auto initial = RunFrame(draw);
  EXPECT_FALSE(initial.open_output.visible);
  EXPECT_TRUE(initial.primary_action.visible);
  EXPECT_NE(initial.primary_action.id, 0u);
  EXPECT_TRUE(initial.copy_log.visible);
  EXPECT_NE(initial.copy_log.id, 0u);
}

TEST_F(WorkflowActivityWidgetsTest, CopyCurrentLogButtonRendersClickableItem) {
  auto draw = [&]() {
    ImGui::PushID("current_log");
    const auto rect = DrawCopyCurrentLogButton("line one\nline two");
    ImGui::PopID();
    return rect;
  };

  const auto rect = RunFrame(draw);
  EXPECT_TRUE(rect.visible);
  EXPECT_NE(rect.id, 0u);
}

#if defined(YAZE_WITH_JSON)
TEST_F(WorkflowActivityWidgetsTest, BuildLogViewRendersStepsAndReceipt) {
  const auto receipt_path =
      std::filesystem::temp_directory_path() /
      ("yaze-build-log-view-" +
       std::to_string(
           std::chrono::steady_clock::now().time_since_epoch().count()) +
       ".json");
  {
    std::ofstream receipt(receipt_path);
    receipt << R"({"state": "failed", "finish": {"exit_code": 1},
      "checks": [
        {"name": "assembly", "status": "passed", "required": true},
        {"name": "overlap", "status": "failed", "required": true,
         "detail": "reserved region"},
        {"name": "smoke", "status": "unavailable", "required": false}
      ]})";
  }
  std::string log =
      "[*] Assembling\n"
      "Core/credits_text.asm:59: warning: (Wfeature_deprecated): warnpc\n";
  for (int i = 0; i < 200; ++i) {
    log += "detail line " + std::to_string(i) + "\n";
  }
  log += "[!] overlap check failed\nBuild receipt: " + receipt_path.string() +
         "\n";

  BuildLogView view;
  view.SetLog(log);
  ASSERT_TRUE(view.receipt().has_value());
  EXPECT_EQ(view.receipt()->checks.size(), 3u);
  EXPECT_EQ(view.summary().warnings, 1);
  EXPECT_EQ(view.summary().errors, 1);

  for (int frame = 0; frame < 2; ++frame) {
    RunFrame([&]() {
      view.Draw("build_log_view", 220.0f, /*follow_tail=*/true);
      return 0;
    });
  }

  std::filesystem::remove(receipt_path);
  view.SetLog(log);
  EXPECT_TRUE(view.receipt().has_value());
  view.SetLog(log + "trailing\n");
  EXPECT_FALSE(view.receipt().has_value());
}
#endif

}  // namespace
}  // namespace yaze::editor::workflow
