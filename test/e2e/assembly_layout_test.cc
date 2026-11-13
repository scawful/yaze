#define IMGUI_DEFINE_MATH_OPERATORS

#include "e2e/assembly_layout_test.h"

#include <SDL.h>

#include <chrono>
#include <filesystem>
#include <fstream>
#include <string>

#include "app/controller.h"
#include "app/editor/editor_manager.h"
#include "app/editor/system/workspace/workspace_window_manager.h"
#include "core/project.h"
#include "imgui/imgui_internal.h"
#include "imgui_test_engine/imgui_te_context.h"
#include "imgui_test_engine/imgui_te_engine.h"
#include "test_utils.h"

namespace {

using yaze::Controller;
using yaze::editor::EditorType;
using yaze::editor::WindowDescriptor;
using yaze::editor::WorkspaceWindowManager;
namespace fs = std::filesystem;

Controller* GetController(ImGuiTestContext* ctx) {
  return ctx && ctx->Test ? static_cast<Controller*>(ctx->Test->UserData)
                          : nullptr;
}

void WriteTextFile(const fs::path& path, const std::string& content) {
  fs::create_directories(path.parent_path());
  std::ofstream file(path);
  file << content;
}

// One generated bank plus a source map and hook list, so the preview shows
// its file actions and one row each of source and hook jumps.
fs::path WriteDisassemblyFixture() {
  const auto now = std::chrono::steady_clock::now().time_since_epoch().count();
  const fs::path root =
      fs::temp_directory_path() / ("yaze_disasm_layout_" + std::to_string(now));
  const fs::path out_dir = root / "out";
  WriteTextFile(root / "code" / "asm" / "main.asm",
                "Start:\n  lda #$42\n  rts\n");
  WriteTextFile(out_dir / "z3disasm" / "bank_00.asm",
                "; bank 00\norg $808000\nStart:\n");
  WriteTextFile(
      out_dir / "sourcemap.json",
      R"({"version":1,"files":[{"id":1,"path":"asm/main.asm"}],"entries":[{"address":"0x808000","file_id":1,"line":1}]})");
  WriteTextFile(
      out_dir / "hooks.json",
      R"({"version":1,"hooks":[{"address":"0x808000","size":4,"kind":"hook","name":"Start","source":"asm/main.asm:1"}]})");
  return root;
}

ImGuiWindow* FindChildWindow(ImGuiTestContext* ctx, ImGuiWindow* root,
                             const char* child_id) {
  for (ImGuiWindow* w : ctx->UiContext->Windows) {
    if (w->RootWindow == root && w != root &&
        std::string(w->Name).find(child_id) != std::string::npos) {
      return w;
    }
  }
  return nullptr;
}

// The item must exist, end inside its window horizontally, and start above
// the window's bottom edge.
void CheckItemVisible(ImGuiTestContext* ctx, const char* ref) {
  const ImGuiTestItemInfo info = ctx->ItemInfo(ref, ImGuiTestOpFlags_NoError);
  IM_CHECK_NO_RET(info.ID != 0 && info.Window != nullptr);
  if (info.ID == 0 || info.Window == nullptr) {
    ctx->LogError("Missing item %s", ref);
    return;
  }
  const ImRect clip = info.Window->InnerClipRect;
  ctx->LogInfo("%s: x %.0f..%.0f y %.0f, window clip x <= %.0f y <= %.0f", ref,
               info.RectFull.Min.x, info.RectFull.Max.x, info.RectFull.Min.y,
               clip.Max.x, clip.Max.y);
  IM_CHECK_LE_NO_RET(info.RectFull.Max.x, clip.Max.x + 1.0f);
  IM_CHECK_LT_NO_RET(info.RectFull.Min.y, clip.Max.y);
}

}  // namespace

void E2ETest_DisassemblyFitsDefaultDock(ImGuiTestContext* ctx) {
  Controller* controller = GetController(ctx);
  IM_CHECK(controller != nullptr);
  yaze::test::gui::LoadRomInTest(ctx,
                                 yaze::test::TestRomManager::GetTestRomPath());
  auto* rom = controller->GetCurrentRom();
  if (!rom || !rom->is_loaded()) {
    ctx->LogWarning("AssemblyLayout: no test ROM loaded; skipping.");
    return;
  }
  auto* editor_manager = controller->editor_manager();
  const fs::path root = WriteDisassemblyFixture();
  // Editors read the session's project context, so open a real project
  // rather than editing the raw-ROM placeholder project.
  yaze::project::YazeProject project;
  IM_CHECK(project.Create("disasm_layout", root.string()).ok());
  const fs::path project_rom = root / "project.sfc";
  std::error_code copy_error;
  fs::copy_file(yaze::test::TestRomManager::GetTestRomPath(), project_rom,
                fs::copy_options::overwrite_existing, copy_error);
  IM_CHECK(!copy_error);
  project.rom_filename = project_rom.string();
  project.code_folder = (root / "code").string();
  project.output_folder = (root / "out").string();
  IM_CHECK(project.Save().ok());
  const absl::Status open_status = editor_manager->OpenRomOrProject(
      project.filepath,
      yaze::editor::EditorManager::LoadOptionsPrompt::kNoPrompt);
  if (!open_status.ok()) {
    ctx->LogError("Open project failed: %s",
                  std::string(open_status.message()).c_str());
  }
  IM_CHECK(open_status.ok());
  ctx->Yield(10);

  SDL_Window* window = controller->window();
  IM_CHECK(window != nullptr);
  int original_width = 0;
  int original_height = 0;
  SDL_GetWindowSize(window, &original_width, &original_height);
  SDL_SetWindowSize(window, 1280, 800);
  ctx->Yield(5);

  editor_manager->SwitchToEditor(EditorType::kAssembly, true);
  editor_manager->ResetCurrentEditorLayout();
  ctx->Yield(10);
  WorkspaceWindowManager* wm = editor_manager->GetWindowManager();
  IM_CHECK(wm != nullptr);
  const size_t session = editor_manager->GetCurrentSessionId();
  IM_CHECK(wm->OpenWindow(session, "assembly.disassembly"));
  ctx->Yield(10);

  const WindowDescriptor* descriptor =
      wm->GetWindowDescriptor(session, "assembly.disassembly");
  IM_CHECK(descriptor != nullptr);
  ImGuiWindow* panel =
      ImGui::FindWindowByName(descriptor->GetImGuiWindowName().c_str());
  IM_CHECK(panel != nullptr && panel->WasActive);
  ctx->LogInfo("Disassembly panel: %.0f px wide, docked=%d", panel->Size.x,
               panel->DockIsActive ? 1 : 0);

  ctx->SetRef(panel->ID);
  CheckItemVisible(ctx, "End");
  CheckItemVisible(ctx, "Use Query Bank");
  CheckItemVisible(ctx, "Go");
  CheckItemVisible(ctx, "Count");

  ImGuiWindow* preview = FindChildWindow(ctx, panel, "##z3disasm_preview");
  IM_CHECK(preview != nullptr && preview->WasActive);
  ctx->LogInfo("Bank preview: %.0f x %.0f px", preview->Size.x,
               preview->Size.y);
  ctx->SetRef(preview->ID);
  CheckItemVisible(ctx, "**/Open in Code Editor");
  CheckItemVisible(ctx, "**/Open Bank Graph");
  CheckItemVisible(ctx, "**/Copy Bank Query");
  CheckItemVisible(ctx, "**/Addr##source_addr_808000");
  CheckItemVisible(ctx, "**/Addr##hook_addr_808000");

  SDL_SetWindowSize(window, original_width, original_height);
  ctx->Yield(5);
  std::error_code ec;
  fs::remove_all(root, ec);
}

namespace yaze {
namespace test {
namespace e2e {

void RegisterAssemblyLayoutTests(ImGuiTestEngine* engine,
                                 Controller* controller) {
  ImGuiTest* t = IM_REGISTER_TEST(engine, "AssemblyLayoutSmoke",
                                  "DisassemblyFitsDefaultDock");
  t->TestFunc = E2ETest_DisassemblyFitsDefaultDock;
  t->UserData = controller;
}

}  // namespace e2e
}  // namespace test
}  // namespace yaze
