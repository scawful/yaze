#include "e2e/editor_smoke_tests.h"

#include <chrono>
#include <filesystem>
#include <fstream>
#include <string>

#include "app/controller.h"
#include "app/editor/code/assembly_editor.h"
#include "app/editor/menu/right_drawer_manager.h"
#include "app/editor/system/workspace_window_manager.h"
#include "app/editor/ui/ui_coordinator.h"
#include "app/gui/core/icons.h"
#include "imgui_test_engine/imgui_te_engine.h"
#include "test_utils.h"

namespace {

using yaze::Controller;
using yaze::editor::RightDrawerManager;
using yaze::editor::WindowDescriptor;
using yaze::editor::WorkspaceWindowManager;
namespace fs = std::filesystem;

fs::path MakeUniqueTempDir(const std::string& prefix) {
  const auto now = std::chrono::steady_clock::now().time_since_epoch().count();
  return fs::temp_directory_path() / (prefix + "_" + std::to_string(now));
}

void WriteTextFile(const fs::path& path, const std::string& content) {
  fs::create_directories(path.parent_path());
  std::ofstream file(path);
  file << content;
}

Controller* GetController(ImGuiTestContext* ctx) {
  if (!ctx || !ctx->Test) {
    return nullptr;
  }
  return static_cast<Controller*>(ctx->Test->UserData);
}

WorkspaceWindowManager* GetWindowManager(Controller* controller) {
  if (!controller) {
    return nullptr;
  }
  return controller->editor_manager()->GetWindowManager();
}

size_t GetSessionId(Controller* controller) {
  return controller ? controller->editor_manager()->GetCurrentSessionId() : 0U;
}

bool EnsureRomLoaded(ImGuiTestContext* ctx, Controller* controller) {
  if (!controller) {
    ctx->LogError("Editor smoke test: Controller is null.");
    return false;
  }

  yaze::test::gui::LoadRomInTest(ctx,
                                 yaze::test::TestRomManager::GetTestRomPath());
  auto* rom = controller->GetCurrentRom();
  if (!rom || !rom->is_loaded()) {
    ctx->LogWarning("Editor smoke test: ROM not loaded; skipping test.");
    return false;
  }

  return true;
}

const WindowDescriptor* GetWindowDescriptor(Controller* controller,
                                            const std::string& window_id) {
  WorkspaceWindowManager* window_manager = GetWindowManager(controller);
  if (!window_manager) {
    return nullptr;
  }
  const size_t session_id = GetSessionId(controller);
  return window_manager->GetWindowDescriptor(session_id, window_id);
}

std::string GetWindowTitle(const WindowDescriptor* descriptor) {
  if (!descriptor) {
    return "";
  }
  if (descriptor->icon.empty()) {
    return descriptor->display_name;
  }
  return descriptor->icon + " " + descriptor->display_name;
}

bool OpenWindow(ImGuiTestContext* ctx, Controller* controller,
                const std::string& window_id) {
  WorkspaceWindowManager* window_manager = GetWindowManager(controller);
  if (!window_manager) {
    ctx->LogError("Editor smoke test: WorkspaceWindowManager unavailable.");
    return false;
  }
  const size_t session_id = GetSessionId(controller);
  const bool shown = window_manager->OpenWindow(session_id, window_id);
  if (!shown) {
    ctx->LogWarning("Editor smoke test: OpenWindow failed for %s",
                    window_id.c_str());
    return false;
  }
  return true;
}

bool CheckWindow(ImGuiTestContext* ctx, Controller* controller,
                 const std::string& window_id) {
  const WindowDescriptor* descriptor =
      GetWindowDescriptor(controller, window_id);
  if (!descriptor) {
    ctx->LogError("Editor smoke test: Missing window descriptor for %s",
                  window_id.c_str());
    return false;
  }

  const std::string window_title = GetWindowTitle(descriptor);
  if (window_title.empty()) {
    ctx->LogError("Editor smoke test: Empty window title for %s",
                  window_id.c_str());
    return false;
  }

  ImGuiTestItemInfo info =
      ctx->WindowInfo(window_title.c_str(), ImGuiTestOpFlags_NoError);
  if (!info.Window) {
    ctx->LogWarning("Editor smoke test: Window not found: %s",
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

  IM_CHECK(OpenWindow(ctx, controller, "graphics.sheet_browser_v2"));
  IM_CHECK(OpenWindow(ctx, controller, "graphics.pixel_editor"));
  ctx->Yield(2);

  IM_CHECK(CheckWindow(ctx, controller, "graphics.sheet_browser_v2"));
  IM_CHECK(CheckWindow(ctx, controller, "graphics.pixel_editor"));

  const auto* sheet_descriptor =
      GetWindowDescriptor(controller, "graphics.sheet_browser_v2");
  const std::string sheet_window_title = GetWindowTitle(sheet_descriptor);
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

  IM_CHECK(OpenWindow(ctx, controller, "sprite.vanilla_editor"));
  ctx->Yield(2);

  IM_CHECK(CheckWindow(ctx, controller, "sprite.vanilla_editor"));
}

void E2ETest_MessageEditorSmokeTest(ImGuiTestContext* ctx) {
  auto* controller = GetController(ctx);
  if (!EnsureRomLoaded(ctx, controller)) {
    return;
  }

  ctx->LogInfo("=== Message Editor Smoke Test ===");
  yaze::test::gui::OpenEditorInTest(ctx, "Message");
  ctx->Yield(5);

  IM_CHECK(OpenWindow(ctx, controller, "message.message_list"));
  IM_CHECK(OpenWindow(ctx, controller, "message.message_editor"));
  ctx->Yield(2);

  IM_CHECK(CheckWindow(ctx, controller, "message.message_list"));
  IM_CHECK(CheckWindow(ctx, controller, "message.message_editor"));

  const auto* editor_descriptor =
      GetWindowDescriptor(controller, "message.message_editor");
  const std::string editor_window_title = GetWindowTitle(editor_descriptor);
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

  IM_CHECK(OpenWindow(ctx, controller, "music.song_browser"));
  IM_CHECK(OpenWindow(ctx, controller, "music.tracker"));
  IM_CHECK(OpenWindow(ctx, controller, "music.piano_roll"));
  ctx->Yield(2);

  IM_CHECK(CheckWindow(ctx, controller, "music.song_browser"));
  IM_CHECK(CheckWindow(ctx, controller, "music.tracker"));
  IM_CHECK(CheckWindow(ctx, controller, "music.piano_roll"));
}

void E2ETest_EmulatorSaveStatesSmokeTest(ImGuiTestContext* ctx) {
  auto* controller = GetController(ctx);
  if (!EnsureRomLoaded(ctx, controller)) {
    return;
  }

  ctx->LogInfo("=== Emulator Save States Smoke Test ===");
  yaze::test::gui::OpenEditorInTest(ctx, "Emulator");
  ctx->Yield(5);

  IM_CHECK(OpenWindow(ctx, controller, "emulator.save_states"));
  ctx->Yield(2);

  IM_CHECK(CheckWindow(ctx, controller, "emulator.save_states"));

  const auto* save_descriptor =
      GetWindowDescriptor(controller, "emulator.save_states");
  const std::string save_window_title = GetWindowTitle(save_descriptor);
  if (!save_window_title.empty()) {
    ctx->SetRef(save_window_title.c_str());
    if (!ctx->ItemExists(
            "Save state functionality will be implemented here.")) {
      ctx->LogWarning("Save States panel text not found");
    }
  }
}

void E2ETest_WindowBrowserSmokeTest(ImGuiTestContext* ctx) {
  auto* controller = GetController(ctx);
  if (!EnsureRomLoaded(ctx, controller)) {
    return;
  }

  ctx->LogInfo("=== Window Browser Smoke Test ===");
  yaze::test::gui::OpenEditorInTest(ctx, "Overworld");
  ctx->Yield(5);

  auto* ui = controller->editor_manager()->ui_coordinator();
  IM_CHECK(ui != nullptr);
  ui->ShowWindowBrowser();
  ctx->Yield(5);

  const std::string browser_title =
      std::string(ICON_MD_DASHBOARD) + " Window Browser";
  IM_CHECK(
      ctx->WindowInfo(browser_title.c_str(), ImGuiTestOpFlags_NoError).Window !=
      nullptr);
}

void E2ETest_WindowSidebarToggleSmokeTest(ImGuiTestContext* ctx) {
  auto* controller = GetController(ctx);
  if (!EnsureRomLoaded(ctx, controller)) {
    return;
  }

  ctx->LogInfo("=== Window Sidebar Toggle Smoke Test ===");
  yaze::test::gui::OpenEditorInTest(ctx, "Overworld");
  ctx->Yield(5);

  auto* window_manager = GetWindowManager(controller);
  IM_CHECK(window_manager != nullptr);
  const size_t session_id = GetSessionId(controller);
  const std::string window_id = "overworld.item_list";

  window_manager->SetActiveCategory("Overworld");
  window_manager->SetSidebarExpanded(true);
  window_manager->CloseWindow(session_id, window_id);
  ctx->Yield(5);

  IM_CHECK(ctx->WindowInfo("##SidePanel", ImGuiTestOpFlags_NoError).Window !=
           nullptr);

  const auto* descriptor = GetWindowDescriptor(controller, window_id);
  IM_CHECK(descriptor != nullptr);
  const std::string sidebar_label =
      descriptor->icon + std::string("  ") + descriptor->display_name;

  ctx->SetRef("##SidePanel");
  IM_CHECK(ctx->ItemExists(sidebar_label.c_str()));
  ctx->ItemClick(sidebar_label.c_str());
  ctx->Yield(5);

  IM_CHECK(window_manager->IsWindowOpen(session_id, window_id));
  IM_CHECK(CheckWindow(ctx, controller, window_id));
}

void E2ETest_RightDrawerSmokeTest(ImGuiTestContext* ctx) {
  auto* controller = GetController(ctx);
  if (!EnsureRomLoaded(ctx, controller)) {
    return;
  }

  ctx->LogInfo("=== Right Drawer Smoke Test ===");
  yaze::test::gui::OpenEditorInTest(ctx, "Overworld");
  ctx->Yield(5);

  auto* drawer_manager = controller->editor_manager()->right_drawer_manager();
  IM_CHECK(drawer_manager != nullptr);
  drawer_manager->OpenDrawer(RightDrawerManager::DrawerType::kSettings);
  ctx->Yield(5);

  IM_CHECK(drawer_manager->IsDrawerExpanded());
  IM_CHECK(drawer_manager->GetActiveDrawer() ==
           RightDrawerManager::DrawerType::kSettings);
  IM_CHECK(ctx->WindowInfo("##RightPanel", ImGuiTestOpFlags_NoError).Window !=
           nullptr);
}

void E2ETest_AssemblyDisassemblyGraphJumpSmokeTest(ImGuiTestContext* ctx) {
  auto* controller = GetController(ctx);
  if (!EnsureRomLoaded(ctx, controller)) {
    return;
  }

  ctx->LogInfo("=== Assembly Disassembly Graph Jump Smoke Test ===");

  auto project_root = MakeUniqueTempDir("yaze_disasm_graph_smoke");
  const auto code_dir = project_root / "code";
  const auto out_dir = project_root / "out";
  const auto disasm_dir = out_dir / "z3disasm";

  WriteTextFile(code_dir / "asm" / "main.asm",
                "; line 1\n; line 2\n; line 3\n; line 4\n; line 5\n"
                "; line 6\n; line 7\n; line 8\n; line 9\n; line 10\n"
                "; line 11\nStart:\n  lda #$42\n  rts\n");
  WriteTextFile(disasm_dir / "bank_00.asm", "; bank 00\norg $808000\nStart:\n");
  WriteTextFile(
      out_dir / "sourcemap.json",
      R"({"version":1,"files":[{"id":1,"path":"asm/main.asm"}],"entries":[{"address":"0x808000","file_id":1,"line":12}]})");
  WriteTextFile(
      out_dir / "hooks.json",
      R"({"version":1,"hooks":[{"address":"0x808000","size":4,"kind":"hook","name":"Start","source":"asm/main.asm:12"}]})");

  auto* project = controller->editor_manager()->GetCurrentProject();
  IM_CHECK(project != nullptr);
  project->code_folder = code_dir.string();
  project->output_folder = out_dir.string();

  controller->editor_manager()->SwitchToEditor(
      yaze::editor::EditorType::kAssembly, true);
  ctx->Yield(5);

  auto* window_manager = GetWindowManager(controller);
  IM_CHECK(window_manager != nullptr);
  IM_CHECK(OpenWindow(ctx, controller, "assembly.disassembly"));
  window_manager->ShowOnlyWindow(GetSessionId(controller),
                                 "assembly.disassembly");
  ctx->Yield(10);

  auto* editor_set = controller->editor_manager()->GetCurrentEditorSet();
  IM_CHECK(editor_set != nullptr);
  auto* assembly_editor = editor_set->GetEditorAs<yaze::editor::AssemblyEditor>(
      yaze::editor::EditorType::kAssembly);
  IM_CHECK(assembly_editor != nullptr);

  IM_CHECK(ctx->ItemExists("**/Open Bank Graph"));
  ctx->ItemClick("**/Open Bank Graph");
  ctx->Yield(5);

  auto* drawer_manager = controller->editor_manager()->right_drawer_manager();
  IM_CHECK(drawer_manager != nullptr);
  IM_CHECK(drawer_manager->GetActiveDrawer() ==
           RightDrawerManager::DrawerType::kToolOutput);
  IM_CHECK(drawer_manager->tool_output_content().find("asm/main.asm") !=
           std::string::npos);

  IM_CHECK(ctx->ItemExists("**/Open##tool_output_open_808000"));
  ctx->ItemClick("**/Open##tool_output_open_808000");
  ctx->Yield(5);

  IM_CHECK(assembly_editor->active_file_path().find("main.asm") !=
           std::string::npos);
  IM_CHECK(assembly_editor->active_cursor_position().mLine == 11);
}

namespace yaze {
namespace test {
namespace e2e {

void RegisterEditorSmokeTests(ImGuiTestEngine* engine, Controller* controller) {
  {
    ImGuiTest* test = IM_REGISTER_TEST(engine, "EditorSmoke", "Graphics");
    test->TestFunc = E2ETest_GraphicsEditorSmokeTest;
    test->UserData = controller;
  }

  {
    ImGuiTest* test = IM_REGISTER_TEST(engine, "EditorSmoke", "Sprite");
    test->TestFunc = E2ETest_SpriteEditorSmokeTest;
    test->UserData = controller;
  }

  {
    ImGuiTest* test = IM_REGISTER_TEST(engine, "EditorSmoke", "Message");
    test->TestFunc = E2ETest_MessageEditorSmokeTest;
    test->UserData = controller;
  }

  {
    ImGuiTest* test = IM_REGISTER_TEST(engine, "EditorSmoke", "Music");
    test->TestFunc = E2ETest_MusicEditorSmokeTest;
    test->UserData = controller;
  }

  {
    ImGuiTest* test =
        IM_REGISTER_TEST(engine, "EditorSmoke", "EmulatorSaveStates");
    test->TestFunc = E2ETest_EmulatorSaveStatesSmokeTest;
    test->UserData = controller;
  }

  {
    ImGuiTest* test = IM_REGISTER_TEST(engine, "EditorSmoke", "WindowBrowser");
    test->TestFunc = E2ETest_WindowBrowserSmokeTest;
    test->UserData = controller;
  }

  {
    ImGuiTest* test =
        IM_REGISTER_TEST(engine, "EditorSmoke", "WindowSidebarToggle");
    test->TestFunc = E2ETest_WindowSidebarToggleSmokeTest;
    test->UserData = controller;
  }

  {
    ImGuiTest* test = IM_REGISTER_TEST(engine, "EditorSmoke", "RightDrawer");
    test->TestFunc = E2ETest_RightDrawerSmokeTest;
    test->UserData = controller;
  }

  {
    ImGuiTest* test =
        IM_REGISTER_TEST(engine, "EditorSmoke", "AssemblyDisassemblyGraphJump");
    test->TestFunc = E2ETest_AssemblyDisassemblyGraphJumpSmokeTest;
    test->UserData = controller;
  }
}

}  // namespace e2e
}  // namespace test
}  // namespace yaze
