#include "e2e/editor_smoke_tests.h"

#include <string>

#include "app/controller.h"
#include "app/editor/system/panel_manager.h"
#include "imgui_test_engine/imgui_te_engine.h"
#include "test_utils.h"

namespace {

using yaze::Controller;
using yaze::editor::PanelDescriptor;
using yaze::editor::PanelManager;

Controller* GetController(ImGuiTestContext* ctx) {
  if (!ctx || !ctx->Test) {
    return nullptr;
  }
  return static_cast<Controller*>(ctx->Test->UserData);
}

PanelManager* GetPanelManager(Controller* controller) {
  if (!controller) {
    return nullptr;
  }
  return controller->editor_manager()->GetPanelManager();
}

size_t GetSessionId(Controller* controller) {
  return controller ? controller->editor_manager()->GetCurrentSessionId() : 0U;
}

bool EnsureRomLoaded(ImGuiTestContext* ctx, Controller* controller) {
  if (!controller) {
    ctx->LogError("Editor smoke test: Controller is null.");
    return false;
  }

  yaze::test::gui::LoadRomInTest(ctx, yaze::test::TestRomManager::GetTestRomPath());
  auto* rom = controller->GetCurrentRom();
  if (!rom || !rom->is_loaded()) {
    ctx->LogWarning("Editor smoke test: ROM not loaded; skipping test.");
    return false;
  }

  return true;
}

const PanelDescriptor* GetPanelDescriptor(Controller* controller,
                                          const std::string& panel_id) {
  PanelManager* panel_manager = GetPanelManager(controller);
  if (!panel_manager) {
    return nullptr;
  }
  const size_t session_id = GetSessionId(controller);
  return panel_manager->GetPanelDescriptor(session_id, panel_id);
}

std::string GetPanelWindowTitle(const PanelDescriptor* descriptor) {
  if (!descriptor) {
    return "";
  }
  if (descriptor->icon.empty()) {
    return descriptor->display_name;
  }
  return descriptor->icon + " " + descriptor->display_name;
}

bool ShowPanel(ImGuiTestContext* ctx, Controller* controller,
               const std::string& panel_id) {
  PanelManager* panel_manager = GetPanelManager(controller);
  if (!panel_manager) {
    ctx->LogError("Editor smoke test: PanelManager unavailable.");
    return false;
  }
  const size_t session_id = GetSessionId(controller);
  const bool shown = panel_manager->ShowPanel(session_id, panel_id);
  if (!shown) {
    ctx->LogWarning("Editor smoke test: ShowPanel failed for %s",
                    panel_id.c_str());
    return false;
  }
  return true;
}

bool CheckPanelWindow(ImGuiTestContext* ctx, Controller* controller,
                      const std::string& panel_id) {
  const PanelDescriptor* descriptor = GetPanelDescriptor(controller, panel_id);
  if (!descriptor) {
    ctx->LogError("Editor smoke test: Missing panel descriptor for %s",
                  panel_id.c_str());
    return false;
  }

  const std::string window_title = GetPanelWindowTitle(descriptor);
  if (window_title.empty()) {
    ctx->LogError("Editor smoke test: Empty window title for %s",
                  panel_id.c_str());
    return false;
  }

  ImGuiTestItemInfo info =
      ctx->WindowInfo(window_title.c_str(), ImGuiTestOpFlags_NoError);
  if (!info.Window) {
    ctx->LogWarning("Editor smoke test: Panel window not found: %s",
                    window_title.c_str());
    return false;
  }

  return true;
}

}  // namespace

void E2ETest_GraphicsEditorSmokeTest(ImGuiTestContext* ctx) {
  auto* controller = GetController(ctx);
  if (!EnsureRomLoaded(ctx, controller)) {
    return;
  }

  ctx->LogInfo("=== Graphics Editor Smoke Test ===");
  yaze::test::gui::OpenEditorInTest(ctx, "Graphics");
  ctx->Yield(5);

  IM_CHECK(ShowPanel(ctx, controller, "graphics.sheet_browser_v2"));
  IM_CHECK(ShowPanel(ctx, controller, "graphics.pixel_editor"));
  ctx->Yield(2);

  IM_CHECK(CheckPanelWindow(ctx, controller, "graphics.sheet_browser_v2"));
  IM_CHECK(CheckPanelWindow(ctx, controller, "graphics.pixel_editor"));

  const auto* sheet_descriptor =
      GetPanelDescriptor(controller, "graphics.sheet_browser_v2");
  const std::string sheet_window_title =
      GetPanelWindowTitle(sheet_descriptor);
  if (!sheet_window_title.empty()) {
    ctx->SetRef(sheet_window_title.c_str());
    if (!ctx->ItemExists("**/GraphicsSheet_0")) {
      ctx->LogWarning("Graphics Editor: sheet list item not found");
    }
  }
}

void E2ETest_SpriteEditorSmokeTest(ImGuiTestContext* ctx) {
  auto* controller = GetController(ctx);
  if (!EnsureRomLoaded(ctx, controller)) {
    return;
  }

  ctx->LogInfo("=== Sprite Editor Smoke Test ===");
  yaze::test::gui::OpenEditorInTest(ctx, "Sprite");
  ctx->Yield(5);

  IM_CHECK(ShowPanel(ctx, controller, "sprite.vanilla_editor"));
  ctx->Yield(2);

  IM_CHECK(CheckPanelWindow(ctx, controller, "sprite.vanilla_editor"));
}

void E2ETest_MessageEditorSmokeTest(ImGuiTestContext* ctx) {
  auto* controller = GetController(ctx);
  if (!EnsureRomLoaded(ctx, controller)) {
    return;
  }

  ctx->LogInfo("=== Message Editor Smoke Test ===");
  yaze::test::gui::OpenEditorInTest(ctx, "Message");
  ctx->Yield(5);

  IM_CHECK(ShowPanel(ctx, controller, "message.message_list"));
  IM_CHECK(ShowPanel(ctx, controller, "message.message_editor"));
  ctx->Yield(2);

  IM_CHECK(CheckPanelWindow(ctx, controller, "message.message_list"));
  IM_CHECK(CheckPanelWindow(ctx, controller, "message.message_editor"));

  const auto* editor_descriptor =
      GetPanelDescriptor(controller, "message.message_editor");
  const std::string editor_window_title =
      GetPanelWindowTitle(editor_descriptor);
  if (!editor_window_title.empty()) {
    ctx->SetRef(editor_window_title.c_str());
    if (!ctx->ItemExists("##MessageEditor")) {
      ctx->LogWarning("Message Editor: text input field not found");
    }
  }
}

void E2ETest_MusicEditorSmokeTest(ImGuiTestContext* ctx) {
  auto* controller = GetController(ctx);
  if (!EnsureRomLoaded(ctx, controller)) {
    return;
  }

  ctx->LogInfo("=== Music Editor Smoke Test ===");
  yaze::test::gui::OpenEditorInTest(ctx, "Music");
  ctx->Yield(5);

  IM_CHECK(ShowPanel(ctx, controller, "music.song_browser"));
  IM_CHECK(ShowPanel(ctx, controller, "music.tracker"));
  IM_CHECK(ShowPanel(ctx, controller, "music.piano_roll"));
  ctx->Yield(2);

  IM_CHECK(CheckPanelWindow(ctx, controller, "music.song_browser"));
  IM_CHECK(CheckPanelWindow(ctx, controller, "music.tracker"));
  IM_CHECK(CheckPanelWindow(ctx, controller, "music.piano_roll"));
}

void E2ETest_EmulatorSaveStatesSmokeTest(ImGuiTestContext* ctx) {
  auto* controller = GetController(ctx);
  if (!EnsureRomLoaded(ctx, controller)) {
    return;
  }

  ctx->LogInfo("=== Emulator Save States Smoke Test ===");
  yaze::test::gui::OpenEditorInTest(ctx, "Emulator");
  ctx->Yield(5);

  IM_CHECK(ShowPanel(ctx, controller, "emulator.save_states"));
  ctx->Yield(2);

  IM_CHECK(CheckPanelWindow(ctx, controller, "emulator.save_states"));

  const auto* save_descriptor =
      GetPanelDescriptor(controller, "emulator.save_states");
  const std::string save_window_title =
      GetPanelWindowTitle(save_descriptor);
  if (!save_window_title.empty()) {
    ctx->SetRef(save_window_title.c_str());
    if (!ctx->ItemExists("Save state functionality will be implemented here.")) {
      ctx->LogWarning("Save States panel text not found");
    }
  }
}

namespace yaze {
namespace test {
namespace e2e {

void RegisterEditorSmokeTests(ImGuiTestEngine* engine,
                              Controller* controller) {
  {
    ImGuiTest* test =
        IM_REGISTER_TEST(engine, "EditorSmoke", "Graphics");
    test->TestFunc = E2ETest_GraphicsEditorSmokeTest;
    test->UserData = controller;
  }

  {
    ImGuiTest* test =
        IM_REGISTER_TEST(engine, "EditorSmoke", "Sprite");
    test->TestFunc = E2ETest_SpriteEditorSmokeTest;
    test->UserData = controller;
  }

  {
    ImGuiTest* test =
        IM_REGISTER_TEST(engine, "EditorSmoke", "Message");
    test->TestFunc = E2ETest_MessageEditorSmokeTest;
    test->UserData = controller;
  }

  {
    ImGuiTest* test =
        IM_REGISTER_TEST(engine, "EditorSmoke", "Music");
    test->TestFunc = E2ETest_MusicEditorSmokeTest;
    test->UserData = controller;
  }

  {
    ImGuiTest* test =
        IM_REGISTER_TEST(engine, "EditorSmoke", "EmulatorSaveStates");
    test->TestFunc = E2ETest_EmulatorSaveStatesSmokeTest;
    test->UserData = controller;
  }
}

}  // namespace e2e
}  // namespace test
}  // namespace yaze
