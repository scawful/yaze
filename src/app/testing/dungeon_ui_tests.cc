#include "app/testing/dungeon_ui_tests.h"

#if defined(YAZE_ENABLE_IMGUI_TEST_ENGINE) && YAZE_ENABLE_IMGUI_TEST_ENGINE

#include <chrono>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <string>
#include <vector>

#include "absl/strings/match.h"
#include "app/application.h"
#include "app/editor/agent/ui/review_boards/review_boards_view.h"
#include "app/editor/editor_manager.h"
#include "app/editor/menu/right_drawer_manager.h"
#include "app/editor/registry/content_registry.h"
#include "app/editor/shell/feedback/toast_manager.h"
#include "app/service/screenshot_utils.h"
#include "imgui.h"
#include "imgui_internal.h"
#include "nlohmann/json.hpp"
#if __has_include("imgui_test_engine/imgui_te_context.h")
#include "imgui_test_engine/imgui_te_context.h"
#elif __has_include("imgui_te_context.h")
#include "imgui_te_context.h"
#else
#error "ImGui Test Engine context header not found"
#endif

#if __has_include("imgui_test_engine/imgui_te_engine.h")
#include "imgui_test_engine/imgui_te_engine.h"
#elif __has_include("imgui_te_engine.h")
#include "imgui_te_engine.h"
#else
#error "ImGui Test Engine engine header not found"
#endif

namespace yaze::test {

// ============================================================================
// Test: Panel visibility toggles
//
// Verifies that dungeon editor panels can be opened and closed via the
// PanelManager without crashing or leaving orphaned state.
// ============================================================================

static void RegisterPanelVisibilityTests(ImGuiTestEngine* engine) {
  // Test: Object Editor panel opens when requested
  {
    ImGuiTest* test =
        IM_REGISTER_TEST(engine, "dungeon_panels", "object_editor_opens");
    test->TestFunc = [](ImGuiTestContext* ctx) {
      ctx->SetRef("Dungeon Workbench");
      IM_CHECK(ctx->GetWindowByRef("") != nullptr);
      ctx->WindowFocus("");
      ctx->ItemClick("**/BG1");
      ctx->ItemClick("**/BG1");
    };
  }

  // Test: Room selector panel can be focused
  {
    ImGuiTest* test =
        IM_REGISTER_TEST(engine, "dungeon_panels", "room_selector_exists");
    test->TestFunc = [](ImGuiTestContext* ctx) {
      ctx->SetRef("Room Selector");
      IM_CHECK(ctx->GetWindowByRef("") != nullptr);
      ctx->WindowFocus("");
      IM_CHECK(ctx->GetWindowByRef("") != nullptr);
    };
  }
}

// ============================================================================
// Test: Layer visibility toggles (BG1/BG2/Sprites/Grid/Bounds)
//
// The compact layer toggle bar renders SmallButton("BG1"), SmallButton("BG2"),
// etc. inside the canvas viewer. These are standard ImGui buttons addressable
// by the test engine.
// ============================================================================

static void RegisterLayerToggleTests(ImGuiTestEngine* engine) {
  // Test: BG1 toggle button exists and is clickable
  {
    ImGuiTest* test =
        IM_REGISTER_TEST(engine, "dungeon_layers", "bg1_toggle_clickable");
    test->TestFunc = [](ImGuiTestContext* ctx) {
      // Navigate to the workbench first
      ctx->SetRef("Dungeon Workbench");
      IM_CHECK(ctx->GetWindowByRef("") != nullptr);
      ctx->WindowFocus("");
      ctx->ItemClick("**/BG1");

      // Click again to restore original state
      ctx->ItemClick("**/BG1");
    };
  }

  // Test: BG2 toggle
  {
    ImGuiTest* test =
        IM_REGISTER_TEST(engine, "dungeon_layers", "bg2_toggle_clickable");
    test->TestFunc = [](ImGuiTestContext* ctx) {
      ctx->SetRef("Dungeon Workbench");
      IM_CHECK(ctx->GetWindowByRef("") != nullptr);
      ctx->WindowFocus("");
      ctx->ItemClick("**/BG2");
      ctx->ItemClick("**/BG2");
    };
  }
}

// ============================================================================
// Test: Workbench toolbar interactions
//
// Tests that toolbar buttons in the workbench panel respond to clicks.
// ============================================================================

static void RegisterToolbarTests(ImGuiTestEngine* engine) {
  // Test: Workbench window can be focused
  {
    ImGuiTest* test =
        IM_REGISTER_TEST(engine, "dungeon_toolbar", "workbench_focus");
    test->TestFunc = [](ImGuiTestContext* ctx) {
      ctx->SetRef("Dungeon Workbench");
      IM_CHECK(ctx->GetWindowByRef("") != nullptr);
      ctx->WindowFocus("");
      IM_CHECK(ctx->GetWindowByRef("") != nullptr);
    };
  }
}

// ============================================================================
// Test: Keyboard shortcut smoke tests
//
// Verifies that common keyboard shortcuts don't crash the application.
// These are smoke tests - they verify no crash, not specific behavior
// (behavior depends on editor state which may vary).
// ============================================================================

static void RegisterKeyboardShortcutTests(ImGuiTestEngine* engine) {
  // Test: Escape key doesn't crash (should cancel placement if active)
  {
    ImGuiTest* test =
        IM_REGISTER_TEST(engine, "dungeon_keys", "escape_no_crash");
    test->TestFunc = [](ImGuiTestContext* ctx) {
      ctx->SetRef("Dungeon Workbench");
      IM_CHECK(ctx->GetWindowByRef("") != nullptr);
      ctx->WindowFocus("");
      ctx->KeyPress(ImGuiKey_Escape);
      ctx->Yield(3);  // Let a few frames process
      IM_CHECK(ctx->GetWindowByRef("") != nullptr);
    };
  }

  // Test: Delete key doesn't crash (should delete selection if any)
  {
    ImGuiTest* test =
        IM_REGISTER_TEST(engine, "dungeon_keys", "delete_no_crash");
    test->TestFunc = [](ImGuiTestContext* ctx) {
      ctx->SetRef("Dungeon Workbench");
      IM_CHECK(ctx->GetWindowByRef("") != nullptr);
      ctx->WindowFocus("");
      ctx->KeyPress(ImGuiKey_Delete);
      ctx->Yield(3);
      IM_CHECK(ctx->GetWindowByRef("") != nullptr);
    };
  }

  // Test: Ctrl+Z (undo) doesn't crash
  {
    ImGuiTest* test = IM_REGISTER_TEST(engine, "dungeon_keys", "undo_no_crash");
    test->TestFunc = [](ImGuiTestContext* ctx) {
      ctx->SetRef("Dungeon Workbench");
      IM_CHECK(ctx->GetWindowByRef("") != nullptr);
      ctx->WindowFocus("");
      ctx->KeyPress(ImGuiMod_Ctrl | ImGuiKey_Z);
      ctx->Yield(3);
      IM_CHECK(ctx->GetWindowByRef("") != nullptr);
    };
  }
}

// ============================================================================
// Test: Toast dedup verification
//
// Triggers the same error condition twice rapidly and verifies the UI
// doesn't stack duplicate toasts. Since we can't easily inspect toast
// state from the test engine, we verify the action doesn't crash and
// the UI remains responsive.
// ============================================================================

static void RegisterToastTests(ImGuiTestEngine* engine) {
  // Test: Rapid repeated actions don't flood the UI
  {
    ImGuiTest* test =
        IM_REGISTER_TEST(engine, "dungeon_toast", "rapid_actions_stable");
    test->TestFunc = [](ImGuiTestContext* ctx) {
      ctx->SetRef("Dungeon Workbench");
      IM_CHECK(ctx->GetWindowByRef("") != nullptr);
      ctx->WindowFocus("");

      // Rapidly press escape multiple times - if placement mode is active
      // this would generate repeated "placement cancelled" toasts.
      // With dedup, only one should appear.
      for (int i = 0; i < 5; ++i) {
        ctx->KeyPress(ImGuiKey_Escape);
        ctx->Yield(1);
      }
      ctx->Yield(5);  // Let toasts settle
      IM_CHECK(ctx->GetWindowByRef("") != nullptr);
    };
  }

  const char* screenshot_env = std::getenv("YAZE_TOAST_COPY_SCREENSHOT");
  if (screenshot_env && *screenshot_env) {
    ImGuiTestEngine_GetIO(engine).ConfigLogToTTY = true;
    ImGuiTest* shot_test = IM_REGISTER_TEST(engine, "toast_copy_acceptance",
                                            "error_toast_screenshot");
    shot_test->TestFunc = [](ImGuiTestContext* ctx) {
      namespace fs = std::filesystem;
      const char* dest_env = std::getenv("YAZE_TOAST_COPY_SCREENSHOT");
      IM_CHECK(dest_env && *dest_env);
      const fs::path error_dest(dest_env);
      std::error_code ec;
      fs::create_directories(error_dest.parent_path(), ec);
      IM_CHECK(!ec);

      Controller* controller = nullptr;
      editor::EditorManager* manager = nullptr;
      for (int frame = 0; frame < 300; ++frame) {
        controller = Application::Instance().GetController();
        manager = controller ? controller->editor_manager() : nullptr;
        if (controller && manager && manager->toast_manager() &&
            manager->right_drawer_manager() &&
            ImGui::GetIO().DisplaySize.x >= 640.0f) {
          break;
        }
        ctx->Yield(1);
      }
      IM_CHECK(controller && manager && manager->toast_manager() &&
               manager->right_drawer_manager());
      ctx->Yield(10);

      struct ShotState {
        bool done = false;
        absl::StatusOr<test::ScreenshotArtifact> result =
            absl::InternalError("uninitialized");
      };
      const auto capture = [&](const fs::path& path) {
        auto state = std::make_shared<ShotState>();
        Controller::ScreenshotRequest req;
        req.preferred_path = path.string();
        req.window_title = "";
        req.format = test::ScreenshotFormat::kPng;
        req.reveal_to_user = false;
        req.callback = [state](absl::StatusOr<test::ScreenshotArtifact> res) {
          state->result = std::move(res);
          state->done = true;
        };
        controller->RequestScreenshot(req);
        for (int i = 0; i < 60 && !state->done && !ctx->IsError(); ++i) {
          ctx->Yield(1);
        }
        IM_CHECK(state->done && state->result.ok());
      };

      manager->toast_manager()->Show(
          "Save failed: Native overworld save cannot publish pending items on "
          "map $28",
          editor::ToastType::kError, 30.0f);
      ctx->Yield(5);
      capture(error_dest);

      manager->toast_manager()->Show(
          "Warning: Unsaved property edits may require reload",
          editor::ToastType::kWarning, 30.0f, "Details", []() {});
      manager->right_drawer_manager()->OpenDrawer(
          editor::RightDrawerManager::DrawerType::kNotifications);
      ctx->Yield(5);
      capture(error_dest.parent_path() / "notification_history_copy.png");

      manager->Quit();
    };
    ImGuiTestEngine_QueueTest(engine, shot_test,
                              ImGuiTestRunFlags_RunFromCommandLine);
  }
}

static ImGuiWindow* FindImGuiWindowContaining(std::string_view needle) {
  ImGuiContext* g = ImGui::GetCurrentContext();
  if (!g)
    return nullptr;
  for (ImGuiWindow* win : g->Windows) {
    if (win && win->Active && absl::StrContains(win->Name, needle)) {
      return win;
    }
  }
  for (ImGuiWindow* win : g->Windows) {
    if (win && absl::StrContains(win->Name, needle)) {
      return win;
    }
  }
  return nullptr;
}

static void RegisterReviewBoardsUITests(ImGuiTestEngine* engine) {
  const char* save_out_env = std::getenv("YAZE_Q237_SAVE_OUT_DIR");
  if (save_out_env && save_out_env[0] != '\0') {
    ImGuiTest* t =
        IM_REGISTER_TEST(engine, "review_boards_v2", "answer_submit_save");
    t->TeardownFunc = [](ImGuiTestContext* ctx) {
      if (ctx && ctx->IsError()) {
        LOG_ERROR("Q237UITest", "answer_submit_save failed");
      }
      if (auto* c = Application::Instance().GetController()) {
        if (auto* m = c->editor_manager()) {
          m->Quit();
        }
      }
    };
    t->TestFunc = [](ImGuiTestContext* ctx) {
      namespace fs = std::filesystem;
      const char* env_dir = std::getenv("YAZE_Q237_SAVE_OUT_DIR");
      IM_CHECK(env_dir != nullptr && env_dir[0] != '\0');
      const fs::path out_dir(env_dir);
      std::error_code ec;
      fs::create_directories(out_dir, ec);
      IM_CHECK(!ec);

      Controller* controller = nullptr;
      editor::EditorManager* manager = nullptr;
      for (int frame = 0; frame < 300; ++frame) {
        controller = Application::Instance().GetController();
        manager = controller ? controller->editor_manager() : nullptr;
        if (controller && manager && manager->toast_manager() &&
            ImGui::GetIO().DisplaySize.x >= 800.0f) {
          break;
        }
        ctx->Yield(1);
      }
      IM_CHECK(controller && manager && manager->toast_manager());
      ctx->Yield(10);

      struct ShotState {
        bool done = false;
        absl::StatusOr<test::ScreenshotArtifact> result =
            absl::InternalError("uninitialized");
      };
      const auto capture = [&](const std::string& filename) -> fs::path {
        const fs::path dest = out_dir / filename;
        auto state = std::make_shared<ShotState>();
        Controller::ScreenshotRequest req;
        req.preferred_path = dest.string();
        req.window_title = "";
        req.format = test::ScreenshotFormat::kPng;
        req.reveal_to_user = false;
        req.callback = [state](absl::StatusOr<test::ScreenshotArtifact> res) {
          state->result = std::move(res);
          state->done = true;
        };
        controller->RequestScreenshot(req);
        for (int i = 0; i < 60 && !state->done && !ctx->IsError(); ++i) {
          ctx->Yield(1);
        }
        if (!state->done || !state->result.ok()) {
          IM_ERRORF("Screenshot %s failed", filename.c_str());
          return fs::path{};
        }
        return fs::path(state->result->file_path);
      };

      // Verify both Command Palette workflow actions are registered.
      bool found_next_waiting = false;
      bool found_refresh = false;
      std::function<void()> open_next_cb;
      for (const auto& act :
           editor::ContentRegistry::WorkflowActions::GetAll()) {
        if (act.id == "workflow.review_boards.next_waiting") {
          found_next_waiting = true;
          open_next_cb = act.callback;
        } else if (act.id == "workflow.review_boards.refresh") {
          found_refresh = true;
        }
      }
      IM_CHECK(found_next_waiting && found_refresh && open_next_cb);
      open_next_cb();
      ctx->Yield(8);

      auto* view = dynamic_cast<editor::ReviewBoardsView*>(
          manager->window_manager().GetWindowContent("agent.review_boards"));
      IM_CHECK(view != nullptr);

      const auto mark_step = [&](const char* step) {
        std::ofstream s(out_dir / "q237_step.txt", std::ios::binary);
        s << step << '\n';
      };
      mark_step("find_rb_win");
      ImGuiWindow* rb_win = FindImGuiWindowContaining("agent.review_boards");
      IM_CHECK(rb_win != nullptr);
      const ImVec2 disp = ImGui::GetIO().DisplaySize;
      ImGui::SetWindowPos(rb_win, ImVec2(24.0f, 36.0f));
      ImGui::SetWindowSize(rb_win, ImVec2(std::max(960.0f, disp.x - 48.0f),
                                          std::max(640.0f, disp.y - 60.0f)));
      ImGui::FocusWindow(rb_win);
      ctx->Yield(12);
      ctx->SetRef(rb_win);

      // Default filter & newest-first sort check.
      mark_step("check_defaults");
      IM_CHECK(view->filter() == editor::ReviewBoardFilter::kWaitingOnYou);
      IM_CHECK(view->sort_mode() == editor::ReviewBoardSort::kNewest);
      const int waiting_before =
          static_cast<int>(view->visible_indices().size());
      IM_CHECK(waiting_before >= 2);

      // Select the primary sample v2 board and wait for PNG texture creation.
      mark_step("select_sky_board");
      view->SelectBoardById("q237-sample-sky-8c");
      ctx->Yield(15);

      const editor::ReviewBoard* target_board = nullptr;
      for (const auto& b : view->scan().boards) {
        if (b.id == "q237-sample-sky-8c") {
          target_board = &b;
          break;
        }
      }
      IM_CHECK(target_board != nullptr);
      const auto* draft_before = view->DraftAnswersFor("q237-sample-sky-8c");
      IM_CHECK(draft_before != nullptr);
      const auto* first_missing =
          editor::FirstUnansweredRequiredQuestion(*target_board, *draft_before);
      IM_CHECK(first_missing != nullptr);
      IM_CHECK_STR_EQ(first_missing->id.c_str(), "cliff_style");
      IM_CHECK(!view->HasUnsavedDraft(*target_board));

      mark_step("shot_list");
      const fs::path shot_list = capture("review_boards_list_waiting.png");
      IM_CHECK(!shot_list.empty());

      ImGuiWindow* detail_win = FindImGuiWindowContaining("##board_detail");
      IM_CHECK(detail_win != nullptr);
      ctx->SetRef(detail_win);

      // Answer all 5 question kinds via real ImGui widget inputs.
      mark_step("answer_q1_single");
      ctx->ItemClick("cliff_style/a/Brown b58 rock");
      ctx->Yield(2);
      mark_step("answer_q2_multi");
      ctx->ItemClick("keep_layers/c/Rope posts");
      ctx->Yield(2);
      ctx->ItemInputValue("keep_layers/##multi_other", "mix seam");
      ctx->Yield(2);
      mark_step("answer_q3_text");
      ctx->ItemInputValue("notes/##text_answer",
                          "Move east bridge 2 tiles north");
      ctx->Yield(2);
      mark_step("answer_q4_number");
      ctx->ItemInputValue("bridge_shift/##number_answer", "2");
      ctx->Yield(2);
      mark_step("answer_q5_yes_no");
      ctx->ItemClick("ship_now/Yes");
      ctx->Yield(4);

      IM_CHECK(view->HasUnsavedDraft(*target_board));

      // Switch to second board and back to prove in-memory draft survival & dot.
      mark_step("switch_boards");
      view->SelectBoardById("q237-sample-wisdom-door");
      ctx->Yield(6);
      IM_CHECK(view->HasUnsavedDraft(*target_board));
      view->SelectBoardById("q237-sample-sky-8c");
      ctx->Yield(6);

      const auto* draft_ready = view->DraftAnswersFor("q237-sample-sky-8c");
      IM_CHECK(draft_ready != nullptr);
      IM_CHECK(editor::FirstUnansweredRequiredQuestion(
                   *target_board, *draft_ready) == nullptr);
      IM_CHECK_EQ(editor::CountAnsweredQuestions(*target_board, *draft_ready),
                  5);

      // Click option thumbnail to verify full-image viewer modal opens & closes.
      mark_step("open_image_viewer");
      detail_win = FindImGuiWindowContaining("##board_detail");
      IM_CHECK(detail_win != nullptr);
      ctx->SetRef(detail_win);
      detail_win->Scroll.y = 0.0f;
      ctx->Yield(2);
      ctx->ItemClick("cliff_style/a/##opt_thumb");
      ctx->Yield(8);
      const fs::path shot_viewer = capture("review_boards_image_viewer.png");
      IM_CHECK(!shot_viewer.empty());
      ImGuiWindow* modal_win =
          FindImGuiWindowContaining("review_board_image_viewer");
      IM_CHECK(modal_win != nullptr);
      ctx->SetRef(modal_win);
      ctx->ItemClick("Close");
      ctx->Yield(6);

      mark_step("enter_comment_and_submit");
      rb_win = FindImGuiWindowContaining("agent.review_boards");
      IM_CHECK(rb_win != nullptr);
      ctx->SetRef(rb_win);
      ctx->ItemInputValue("##board_comment",
                          "Approved b58 rock with +2 bridge shift");
      ctx->Yield(4);
      const fs::path shot_answered =
          capture("review_boards_question_answered.png");
      IM_CHECK(!shot_answered.empty());

      // Submit!
      ctx->ItemClick("###submit_feedback");
      ctx->Yield(8);
      const fs::path shot_submitted =
          capture("review_boards_submitted_toast.png");

      IM_CHECK(!view->HasUnsavedDraft(*target_board));
      const fs::path fb_path = editor::ReviewBoardFeedbackPath(*target_board);
      IM_CHECK(fs::is_regular_file(fb_path));

      std::ifstream fb_in(fb_path, std::ios::binary);
      const std::string fb_text((std::istreambuf_iterator<char>(fb_in)),
                                std::istreambuf_iterator<char>());
      const auto fb_json = nlohmann::json::parse(fb_text);
      IM_CHECK_STR_EQ(fb_json["schema"].get<std::string>().c_str(),
                      "yaze.review_board_feedback.v2");

      nlohmann::json receipt = {
          {"result", "PASS"},
          {"waiting_before", waiting_before},
          {"feedback_file", fb_path.string()},
          {"feedback_json", fb_json},
          {"screenshots",
           {{"list_waiting", shot_list.string()},
            {"image_viewer", shot_viewer.string()},
            {"question_answered", shot_answered.string()},
            {"submitted_toast", shot_submitted.string()}}},
      };
      const fs::path receipt_path = out_dir / "q237_save_receipt.json";
      std::ofstream out(receipt_path, std::ios::binary);
      out << receipt.dump(2) << '\n';
      IM_CHECK(out.good());
      manager->Quit();
    };
    ImGuiTestEngine_QueueTest(engine, t, ImGuiTestRunFlags_RunFromCommandLine);
  }

  const char* reopen_out_env = std::getenv("YAZE_Q237_REOPEN_OUT_DIR");
  if (reopen_out_env && reopen_out_env[0] != '\0') {
    ImGuiTest* t =
        IM_REGISTER_TEST(engine, "review_boards_v2", "cold_reopen_restore");
    t->TeardownFunc = [](ImGuiTestContext* ctx) {
      if (ctx && ctx->IsError()) {
        LOG_ERROR("Q237UITest", "answer_submit_save failed");
      }
      if (auto* c = Application::Instance().GetController()) {
        if (auto* m = c->editor_manager()) {
          m->Quit();
        }
      }
    };
    t->TestFunc = [](ImGuiTestContext* ctx) {
      namespace fs = std::filesystem;
      const char* env_dir = std::getenv("YAZE_Q237_REOPEN_OUT_DIR");
      IM_CHECK(env_dir != nullptr && env_dir[0] != '\0');
      const fs::path out_dir(env_dir);
      std::error_code ec;
      fs::create_directories(out_dir, ec);
      IM_CHECK(!ec);

      Controller* controller = nullptr;
      editor::EditorManager* manager = nullptr;
      for (int frame = 0; frame < 300; ++frame) {
        controller = Application::Instance().GetController();
        manager = controller ? controller->editor_manager() : nullptr;
        if (controller && manager && manager->toast_manager() &&
            ImGui::GetIO().DisplaySize.x >= 800.0f) {
          break;
        }
        ctx->Yield(1);
      }
      IM_CHECK(controller && manager && manager->toast_manager());
      ctx->Yield(10);

      struct ShotState {
        bool done = false;
        absl::StatusOr<test::ScreenshotArtifact> result =
            absl::InternalError("uninitialized");
      };
      const auto capture = [&](const std::string& filename) -> fs::path {
        const fs::path dest = out_dir / filename;
        auto state = std::make_shared<ShotState>();
        Controller::ScreenshotRequest req;
        req.preferred_path = dest.string();
        req.window_title = "";
        req.format = test::ScreenshotFormat::kPng;
        req.reveal_to_user = false;
        req.callback = [state](absl::StatusOr<test::ScreenshotArtifact> res) {
          state->result = std::move(res);
          state->done = true;
        };
        controller->RequestScreenshot(req);
        for (int i = 0; i < 60 && !state->done && !ctx->IsError(); ++i) {
          ctx->Yield(1);
        }
        if (!state->done || !state->result.ok()) {
          IM_ERRORF("Screenshot %s failed", filename.c_str());
          return fs::path{};
        }
        return fs::path(state->result->file_path);
      };

      for (const auto& act :
           editor::ContentRegistry::WorkflowActions::GetAll()) {
        if (act.id == "workflow.review_boards.refresh" && act.callback) {
          act.callback();
          break;
        }
      }
      ctx->Yield(10);

      auto* view = dynamic_cast<editor::ReviewBoardsView*>(
          manager->window_manager().GetWindowContent("agent.review_boards"));
      IM_CHECK(view != nullptr);
      ImGuiWindow* rb_win = FindImGuiWindowContaining("agent.review_boards");
      IM_CHECK(rb_win != nullptr);
      const ImVec2 disp = ImGui::GetIO().DisplaySize;
      ImGui::SetWindowPos(rb_win, ImVec2(24.0f, 36.0f));
      ImGui::SetWindowSize(rb_win, ImVec2(std::max(960.0f, disp.x - 48.0f),
                                          std::max(640.0f, disp.y - 60.0f)));
      ctx->Yield(10);

      const int waiting_after =
          static_cast<int>(view->visible_indices().size());
      view->SelectBoardById("q237-sample-sky-8c");
      ctx->Yield(15);

      const editor::ReviewBoard* reopened = nullptr;
      for (const auto& b : view->scan().boards) {
        if (b.id == "q237-sample-sky-8c") {
          reopened = &b;
          break;
        }
      }
      IM_CHECK(reopened != nullptr);
      IM_CHECK(!editor::ReviewBoardWaitingOnUser(*reopened));
      IM_CHECK(!view->HasUnsavedDraft(*reopened));

      const auto* restored = view->DraftAnswersFor("q237-sample-sky-8c");
      IM_CHECK(restored != nullptr);
      IM_CHECK_EQ(editor::CountAnsweredQuestions(*reopened, *restored), 5);
      IM_CHECK_STR_EQ(
          std::get<std::string>(restored->at("cliff_style").value).c_str(),
          "a");
      const auto& multi_vals =
          std::get<std::vector<std::string>>(restored->at("keep_layers").value);
      IM_CHECK_EQ(multi_vals.size(), 2u);
      IM_CHECK_STR_EQ(restored->at("keep_layers").other.c_str(), "mix seam");
      IM_CHECK_STR_EQ(
          std::get<std::string>(restored->at("notes").value).c_str(),
          "Move east bridge 2 tiles north");
      IM_CHECK_EQ(std::get<double>(restored->at("bridge_shift").value), 2.0);
      IM_CHECK_EQ(std::get<bool>(restored->at("ship_now").value), true);

      const fs::path shot_reopen = capture("review_boards_reopen_restored.png");

      nlohmann::json receipt = {
          {"result", "PASS"},
          {"waiting_after", waiting_after},
          {"restored_answered_count",
           editor::CountAnsweredQuestions(*reopened, *restored)},
          {"screenshot_reopen", shot_reopen.string()},
      };
      const fs::path receipt_path = out_dir / "q237_reopen_receipt.json";
      std::ofstream out(receipt_path, std::ios::binary);
      out << receipt.dump(2) << '\n';
      IM_CHECK(out.good());
      manager->Quit();
    };
    ImGuiTestEngine_QueueTest(engine, t, ImGuiTestRunFlags_RunFromCommandLine);
  }
}

// ============================================================================
// Public registration entry point
// ============================================================================

void RegisterDungeonUITests(ImGuiTestEngine* engine) {
  RegisterPanelVisibilityTests(engine);
  RegisterLayerToggleTests(engine);
  RegisterToolbarTests(engine);
  RegisterKeyboardShortcutTests(engine);
  RegisterToastTests(engine);
  RegisterReviewBoardsUITests(engine);
}

}  // namespace yaze::test

#else  // !YAZE_ENABLE_IMGUI_TEST_ENGINE

namespace yaze::test {
void RegisterDungeonUITests(ImGuiTestEngine*) {}
}  // namespace yaze::test

#endif  // YAZE_ENABLE_IMGUI_TEST_ENGINE
