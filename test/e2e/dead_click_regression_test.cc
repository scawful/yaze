#define IMGUI_DEFINE_MATH_OPERATORS

#include "e2e/dead_click_regression_test.h"

#include <string>

#include "app/controller.h"
#include "app/editor/editor_manager.h"
#include "app/editor/overworld/overworld_editor.h"
#include "app/editor/system/workspace/workspace_window_manager.h"
#include "app/gui/core/icons.h"
#include "imgui/imgui_internal.h"
#include "imgui_test_engine/imgui_te_context.h"
#include "imgui_test_engine/imgui_te_engine.h"
#include "test_utils.h"

namespace {

using yaze::Controller;
using yaze::editor::EditingMode;
using yaze::editor::EditorType;
using yaze::editor::OverworldEditor;
using yaze::editor::WindowDescriptor;
using yaze::editor::WorkspaceWindowManager;

Controller* GetController(ImGuiTestContext* ctx) {
  return ctx && ctx->Test ? static_cast<Controller*>(ctx->Test->UserData)
                          : nullptr;
}

WorkspaceWindowManager* GetWindowManager(Controller* controller) {
  return controller ? controller->editor_manager()->GetWindowManager()
                    : nullptr;
}

std::string WindowTitle(Controller* controller, const std::string& id) {
  auto* wm = GetWindowManager(controller);
  if (!wm) {
    return "";
  }
  const WindowDescriptor* d = wm->GetWindowDescriptor(
      controller->editor_manager()->GetCurrentSessionId(), id);
  if (!d) {
    return "";
  }
  return d->GetImGuiWindowName();
}

bool EnsureOverworldReady(ImGuiTestContext* ctx, Controller* controller) {
  yaze::test::gui::LoadRomInTest(ctx,
                                 yaze::test::TestRomManager::GetTestRomPath());
  auto* rom = controller->GetCurrentRom();
  if (!rom || !rom->is_loaded()) {
    ctx->LogWarning("DeadClick: no test ROM loaded; skipping.");
    return false;
  }
  controller->editor_manager()->SwitchToEditor(EditorType::kOverworld, true);
  ctx->Yield(10);
  return true;
}

OverworldEditor* GetOverworldEditor(Controller* controller) {
  auto* set = controller->editor_manager()->GetCurrentEditorSet();
  return set ? static_cast<OverworldEditor*>(
                   set->GetEditor(EditorType::kOverworld))
             : nullptr;
}

// ctx->MenuClick() cannot find ImGui 1.92 menu windows ("<label>###Menu_00"):
// the engine's "//###Menu_00" ref hashes differently. Open the menu with a
// real click, locate its popup window by name, and click the item in it.
bool ClickMainMenuItem(ImGuiTestContext* ctx, const char* menu,
                       const char* item) {
  ctx->SetRef("DockSpaceWindow");
  ctx->ItemClick((std::string("##MenuBar/") + menu).c_str());
  ctx->Yield(2);
  ImGuiWindow* popup = nullptr;
  const std::string suffix = std::string(menu) + "###Menu_00";
  for (ImGuiWindow* w : ctx->UiContext->Windows) {
    if (w->Active && suffix == w->Name) {
      popup = w;
    }
  }
  if (popup == nullptr) {
    ctx->LogError("Menu '%s' did not open", menu);
    return false;
  }
  ctx->SetRef(popup->ID);
  // Enter the popup straight down from the menu title, then move to the
  // item, so the path never crosses a neighbouring menu-bar title (hovering
  // one switches menus).
  const ImGuiTestItemInfo target = ctx->ItemInfo(item);
  if (target.ID == 0) {
    return false;
  }
  ctx->MouseMoveToPos(
      ImVec2(ctx->UiContext->IO.MousePos.x, target.RectFull.GetCenter().y));
  ctx->ItemClick(item);
  ctx->Yield(3);
  ctx->SetRef("DockSpaceWindow");
  return !ctx->IsError();
}

}  // namespace

// Menu items fire on mouse release inside the menu popup. With a test ROM
// the overworld editor is active (where the reported clicks went dead);
// without one the menu bar is still exercised.
void E2ETest_DeadClickMainMenuItem(ImGuiTestContext* ctx) {
  Controller* controller = GetController(ctx);
  IM_CHECK(controller != nullptr);
  (void)EnsureOverworldReady(ctx, controller);
  WorkspaceWindowManager* wm = GetWindowManager(controller);
  IM_CHECK(wm != nullptr);

  ctx->SetRef("DockSpaceWindow");
  const bool before = wm->IsSidebarVisible();
  IM_CHECK(
      ClickMainMenuItem(ctx, "View", ICON_MD_VIEW_SIDEBAR " Show Sidebar"));
  IM_CHECK_NO_RET(wm->IsSidebarVisible() != before);

  // Restore.
  if (wm->IsSidebarVisible() != before) {
    ClickMainMenuItem(ctx, "View", ICON_MD_VIEW_SIDEBAR " Show Sidebar");
  }
  IM_CHECK_EQ(wm->IsSidebarVisible(), before);
}

// The overworld map context popup opens on right-button release over the
// canvas item while the Select (Mouse) tool is active.
void E2ETest_DeadClickOverworldContextMenu(ImGuiTestContext* ctx) {
  Controller* controller = GetController(ctx);
  IM_CHECK(controller != nullptr);
  if (!EnsureOverworldReady(ctx, controller)) {
    return;
  }
  OverworldEditor* ow = GetOverworldEditor(controller);
  IM_CHECK(ow != nullptr);
  ow->SetEditingMode(EditingMode::MOUSE);
  ctx->Yield(3);

  const std::string title = WindowTitle(controller, "overworld.canvas");
  IM_CHECK(!title.empty());
  ImGuiWindow* window = ctx->GetWindowByRef(title.c_str());
  IM_CHECK(window != nullptr);
  ctx->WindowFocus(title.c_str());
  ctx->Yield(2);

  // Try a few points: a hovered entity vetoes the map menu by design.
  const ImRect r = window->InnerRect;
  const ImVec2 points[] = {
      ImVec2(r.Min.x + r.GetWidth() * 0.30f, r.Min.y + r.GetHeight() * 0.70f),
      ImVec2(r.Min.x + r.GetWidth() * 0.55f, r.Min.y + r.GetHeight() * 0.55f),
      ImVec2(r.Min.x + r.GetWidth() * 0.20f, r.Min.y + r.GetHeight() * 0.35f),
      ImVec2(r.Min.x + r.GetWidth() * 0.75f, r.Min.y + r.GetHeight() * 0.80f),
  };
  bool opened = false;
  for (const ImVec2& p : points) {
    ctx->MouseMoveToPos(p);
    ctx->Yield(2);
    ctx->MouseClick(ImGuiMouseButton_Right);
    ctx->Yield(3);
    if (ctx->UiContext->OpenPopupStack.Size > 0) {
      opened = true;
      break;
    }
  }
  IM_CHECK_NO_RET(opened);
  ctx->PopupCloseAll();
  ctx->Yield(2);
}

// A docked panel's close X acts on release over the tab close button.
void E2ETest_DeadClickPanelCloseButton(ImGuiTestContext* ctx) {
  Controller* controller = GetController(ctx);
  IM_CHECK(controller != nullptr);
  if (!EnsureOverworldReady(ctx, controller)) {
    return;
  }
  WorkspaceWindowManager* wm = GetWindowManager(controller);
  IM_CHECK(wm != nullptr);
  const size_t session = controller->editor_manager()->GetCurrentSessionId();
  const std::string id = "overworld.properties";
  wm->OpenWindow(session, id);
  ctx->Yield(5);
  IM_CHECK(wm->IsWindowOpen(session, id));

  const std::string title = WindowTitle(controller, id);
  IM_CHECK(!title.empty());
  ctx->WindowClose(title.c_str());
  ctx->Yield(3);
  IM_CHECK_NO_RET(!wm->IsWindowOpen(session, id));

  wm->OpenWindow(session, id);
  ctx->Yield(2);
}

namespace yaze {
namespace test {
namespace e2e {

void RegisterDeadClickRegressionTests(ImGuiTestEngine* engine,
                                      Controller* controller) {
  ImGuiTest* t = IM_REGISTER_TEST(engine, "DeadClickSmoke", "MainMenuItem");
  t->TestFunc = E2ETest_DeadClickMainMenuItem;
  t->UserData = controller;

  t = IM_REGISTER_TEST(engine, "DeadClickSmoke", "OverworldContextMenu");
  t->TestFunc = E2ETest_DeadClickOverworldContextMenu;
  t->UserData = controller;

  t = IM_REGISTER_TEST(engine, "DeadClickSmoke", "PanelCloseButton");
  t->TestFunc = E2ETest_DeadClickPanelCloseButton;
  t->UserData = controller;
}

}  // namespace e2e
}  // namespace test
}  // namespace yaze
