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

// Screen position of an overworld world-pixel position on the canvas.
ImVec2 CanvasScreenPos(OverworldEditor* ow, ImVec2 world_px) {
  auto& canvas = ow->ow_map_canvas();
  const float scale = canvas.global_scale() > 0 ? canvas.global_scale() : 1.0f;
  return ImVec2(
      canvas.zero_point().x + canvas.scrolling().x + world_px.x * scale,
      canvas.zero_point().y + canvas.scrolling().y + world_px.y * scale);
}

// Center of the tile16 at (tile_x, tile_y) of `map_id`, in world pixels.
ImVec2 MapTileWorldPos(int map_id, int tile_x, int tile_y) {
  const int local = map_id % 0x40;
  return ImVec2((local % 8) * 512.0f + tile_x * 16.0f + 8.0f,
                (local / 8) * 512.0f + tile_y * 16.0f + 8.0f);
}

int WorldTile16At(OverworldEditor* ow, int world, ImVec2 world_px) {
  const auto* tiles = ow->overworld().mutable_map_tiles();
  const auto& grid = world == 0   ? tiles->light_world
                     : world == 1 ? tiles->dark_world
                                  : tiles->special_world;
  return grid[static_cast<int>(world_px.x) / 16]
             [static_cast<int>(world_px.y) / 16];
}

// A light-world map outside the area of `map_id` (its own parent area).
int MapInOtherArea(OverworldEditor* ow, int map_id) {
  const int parent = ow->overworld().overworld_map(map_id)->parent();
  for (int candidate : {0x1B, 0x2C, 0x33, 0x12, 0x25}) {
    const auto* map = ow->overworld().overworld_map(candidate);
    if (map && map->parent() != parent) {
      return candidate;
    }
  }
  return -1;
}

// Centers `map_id` in the canvas viewport and waits for the scroll.
bool ShowMapInViewport(ImGuiTestContext* ctx, OverworldEditor* ow, int map_id) {
  ow->SelectMapForEditing(map_id, /*respect_pin=*/false);
  ow->CenterOverworldView();
  ctx->Yield(6);
  return ow->current_map_id() == map_id;
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

// Brush tool: a right-click on another map both samples its Tile16 and makes
// that map (its whole area) the current map, like a Select-tool left-click.
// Run pinned: unpinned, hovering alone already selects the map, which hid
// that an explicit right-click did not.
void E2ETest_BrushRightClickSelectsMapAndSamples(ImGuiTestContext* ctx) {
  Controller* controller = GetController(ctx);
  IM_CHECK(controller != nullptr);
  if (!EnsureOverworldReady(ctx, controller)) {
    return;
  }
  OverworldEditor* ow = GetOverworldEditor(controller);
  IM_CHECK(ow != nullptr);
  if (!ow->map_pinned()) {
    ow->ToggleMapLock();
  }
  ow->SetEditingMode(EditingMode::DRAW_TILE);
  const std::string title = WindowTitle(controller, "overworld.canvas");
  IM_CHECK(!title.empty());
  ctx->WindowFocus(title.c_str());

  const int start_map = 0x00;
  const int target_map = MapInOtherArea(ow, start_map);
  IM_CHECK(target_map >= 0);
  IM_CHECK(ShowMapInViewport(ctx, ow, target_map));
  // Park the cursor over the start map's area first, outside the canvas.
  ctx->MouseMoveToPos(ImVec2(5, 5));
  ow->SelectMapForEditing(start_map, false);
  ctx->Yield(2);
  IM_CHECK_EQ(ow->current_map_id(), start_map);

  // Pick a tile whose ID differs from the current brush tile.
  ImVec2 world_px;
  int expected_tile = -1;
  for (int ty = 4; ty < 28 && expected_tile < 0; ty += 3) {
    for (int tx = 4; tx < 28; tx += 3) {
      const ImVec2 p = MapTileWorldPos(target_map, tx, ty);
      const int id = WorldTile16At(ow, 0, p);
      if (id != ow->current_tile16_id()) {
        world_px = p;
        expected_tile = id;
        break;
      }
    }
  }
  IM_CHECK(expected_tile >= 0);

  ImGuiWindow* window = ctx->GetWindowByRef(title.c_str());
  IM_CHECK(window != nullptr);
  const ImVec2 screen = CanvasScreenPos(ow, world_px);
  IM_CHECK(window->InnerRect.Contains(screen));
  ctx->MouseMoveToPos(screen);
  ctx->Yield(2);
  ctx->MouseClick(ImGuiMouseButton_Right);
  ctx->Yield(3);

  const int target_parent = ow->overworld().overworld_map(target_map)->parent();
  const int current = ow->current_map_id();
  IM_CHECK_NO_RET(current == target_map ||
                  ow->overworld().overworld_map(current)->parent() ==
                      target_parent);
  IM_CHECK_EQ_NO_RET(ow->current_tile16_id(), expected_tile);
  // The pin stays on and now holds the clicked map.
  IM_CHECK_NO_RET(ow->map_pinned());
  // Plain right-click in a paint tool samples; it never opens the menu.
  IM_CHECK_EQ_NO_RET(ctx->UiContext->OpenPopupStack.Size, 0);
  ctx->PopupCloseAll();
  ow->ToggleMapLock();
  ow->SetEditingMode(EditingMode::MOUSE);
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

  t = IM_REGISTER_TEST(engine, "OverworldRightClick",
                       "BrushRightClickSelectsMapAndSamples");
  t->TestFunc = E2ETest_BrushRightClickSelectsMapAndSamples;
  t->UserData = controller;
}

}  // namespace e2e
}  // namespace test
}  // namespace yaze
