#define IMGUI_DEFINE_MATH_OPERATORS

#include "e2e/menu_tilemap_editor_test.h"

#include <cstdint>
#include <filesystem>
#include <fstream>
#include <string>
#include <vector>

#include "app/controller.h"
#include "app/editor/editor_manager.h"
#include "app/editor/graphics/screen_editor.h"
#include "app/editor/system/workspace/editor_registry.h"
#include "app/editor/system/workspace/workspace_window_manager.h"
#include "imgui/imgui_internal.h"
#include "imgui_test_engine/imgui_te_context.h"
#include "imgui_test_engine/imgui_te_engine.h"
#include "test_utils.h"
#include "unique_temp_path.h"

namespace {

using yaze::Controller;
using yaze::editor::EditorType;
using yaze::editor::ScreenEditor;
using yaze::editor::WorkspaceWindowManager;

Controller* GetController(ImGuiTestContext* ctx) {
  return ctx && ctx->Test ? static_cast<Controller*>(ctx->Test->UserData)
                          : nullptr;
}

ScreenEditor* GetScreenEditor(Controller* controller) {
  auto* set = controller->editor_manager()->GetCurrentEditorSet();
  return set ? static_cast<ScreenEditor*>(set->GetEditor(EditorType::kScreen))
             : nullptr;
}

WorkspaceWindowManager* GetWindowManager(Controller* controller) {
  return controller ? controller->editor_manager()->GetWindowManager()
                    : nullptr;
}

// A minimal 32x2-word synthetic tilemap (matches quest_icons.tilemap's real
// size), every cell filled with word 0x2000 (tile 0, palette 0, no flags,
// priority set) so painting cell (0,0) with the tool's default state (tile
// 0, palette 0, no flags, no priority -> word 0x0000) is an observable,
// byte-verifiable change.
constexpr uint16_t kFillWord = 0x2000;
constexpr int kRows = 2;
constexpr int kCols = 32;

std::vector<uint8_t> MakeSyntheticTilemapBytes() {
  std::vector<uint8_t> bytes(static_cast<size_t>(kRows) * kCols * 2);
  for (size_t i = 0; i < bytes.size() / 2; ++i) {
    bytes[i * 2] = static_cast<uint8_t>(kFillWord & 0xFF);
    bytes[i * 2 + 1] = static_cast<uint8_t>((kFillWord >> 8) & 0xFF);
  }
  return bytes;
}

std::vector<uint8_t> ReadFileBytes(const std::filesystem::path& path) {
  std::ifstream in(path, std::ios::binary | std::ios::ate);
  if (!in)
    return {};
  std::vector<uint8_t> out(static_cast<size_t>(in.tellg()));
  in.seekg(0);
  in.read(reinterpret_cast<char*>(out.data()),
          static_cast<std::streamsize>(out.size()));
  return out;
}

std::string WindowTitle(Controller* controller, const std::string& id) {
  auto* wm = GetWindowManager(controller);
  if (!wm)
    return "";
  const auto* d = wm->GetWindowDescriptor(
      controller->editor_manager()->GetCurrentSessionId(), id);
  return d ? d->GetImGuiWindowName() : "";
}

}  // namespace

void E2ETest_MenuTilemapEditor_PaintUndoSave(ImGuiTestContext* ctx) {
  Controller* controller = GetController(ctx);
  IM_CHECK(controller != nullptr);

  yaze::test::gui::LoadRomInTest(ctx,
                                 yaze::test::TestRomManager::GetTestRomPath());
  auto* rom = controller->GetCurrentRom();
  if (!rom || !rom->is_loaded()) {
    ctx->LogWarning("MenuTilemapEditor: no test ROM loaded; skipping.");
    return;
  }

  controller->editor_manager()->SwitchToEditor(EditorType::kScreen, true);
  ctx->Yield(5);

  ScreenEditor* screen_editor = GetScreenEditor(controller);
  IM_CHECK(screen_editor != nullptr);

  // Synthetic tilemap in a temp dir -- never a real Oracle of Secrets file,
  // per the "no ROM bytes / no game graphics in the repo" constraint. The
  // native "Open..." dialog can't be driven headlessly, so this goes
  // through the same testing-only hook Controller::LoadRomForTesting uses
  // for ROMs.
  auto path = yaze::test::UniqueTempPath("menu_tilemap_e2e", ".tilemap");
  auto original_bytes = MakeSyntheticTilemapBytes();
  {
    std::ofstream out(path, std::ios::binary);
    out.write(reinterpret_cast<const char*>(original_bytes.data()),
              static_cast<std::streamsize>(original_bytes.size()));
  }
  auto load_status = screen_editor->LoadMenuTilemapForTesting(path.string());
  IM_CHECK(load_status.ok());

  auto* wm = GetWindowManager(controller);
  IM_CHECK(wm != nullptr);
  IM_CHECK(wm->OpenWindow(controller->editor_manager()->GetCurrentSessionId(),
                          "screen.menu_tilemap"));
  ctx->Yield(5);

  const std::string window_title =
      WindowTitle(controller, "screen.menu_tilemap");
  IM_CHECK(!window_title.empty());
  ctx->WindowFocus(window_title.c_str());
  ctx->SetRef(window_title.c_str());

  // Real click on the canvas at tile (0,0): with the default zoom (2.0x)
  // that's a small offset from the canvas item's top-left corner.
  ImGuiTestItemInfo canvas_item = ctx->ItemInfo("##MenuTilemapCanvas");
  IM_CHECK(canvas_item.ID != 0);
  const ImVec2 click_pos = canvas_item.RectFull.Min + ImVec2(4.0f, 4.0f);
  ctx->MouseMoveToPos(click_pos);
  ctx->Yield(2);
  ctx->MouseClick(ImGuiMouseButton_Left);
  ctx->Yield(3);

  IM_CHECK(screen_editor->IsMenuTilemapDirtyForTesting());

  // Undo through the Screen editor's shared undo system (the same
  // ScreenEditType::kMenuTilemap action the paint stroke pushed).
  auto undo_status = screen_editor->Undo();
  IM_CHECK(undo_status.ok());
  ctx->Yield(2);

  // Save via the real Ctrl/Cmd+S keyboard path (panel-local: only fires
  // while this window is focused, per screen_menu_tilemap_editor.cc).
  ctx->SetRef(window_title.c_str());
  ctx->KeyPress(ImGuiMod_Ctrl | ImGuiKey_S);
  ctx->Yield(3);

  std::vector<uint8_t> saved_bytes = ReadFileBytes(path);
  IM_CHECK(saved_bytes == original_bytes);

  std::error_code ec;
  std::filesystem::remove(path, ec);
  std::filesystem::remove(path.string() + ".bak", ec);
  std::filesystem::remove(path.string() + ".tmp", ec);

  ctx->SetRef("DockSpaceWindow");
}

namespace yaze {
namespace test {
namespace e2e {

void RegisterMenuTilemapEditorTests(ImGuiTestEngine* engine,
                                    Controller* controller) {
  ImGuiTest* t =
      IM_REGISTER_TEST(engine, "MenuTilemapEditorSmoke", "PaintUndoSave");
  t->TestFunc = E2ETest_MenuTilemapEditor_PaintUndoSave;
  t->UserData = controller;
}

}  // namespace e2e
}  // namespace test
}  // namespace yaze
