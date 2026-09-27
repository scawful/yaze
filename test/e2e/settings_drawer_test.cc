#define IMGUI_DEFINE_MATH_OPERATORS

#include "e2e/settings_drawer_test.h"

#include <string>

#include "app/controller.h"
#include "app/editor/editor_manager.h"
#include "app/editor/menu/right_drawer_manager.h"
#include "app/editor/shell/coordinator/ui_coordinator.h"
#include "app/gui/core/icons.h"
#include "imgui/imgui_internal.h"
#include "imgui_test_engine/imgui_te_context.h"
#include "imgui_test_engine/imgui_te_engine.h"
#include "test_utils.h"

namespace {

using yaze::Controller;
using yaze::editor::RightDrawerManager;
using DrawerType = RightDrawerManager::DrawerType;

Controller* GetController(ImGuiTestContext* ctx) {
  return ctx && ctx->Test ? static_cast<Controller*>(ctx->Test->UserData)
                          : nullptr;
}

RightDrawerManager* GetDrawers(Controller* controller) {
  return controller ? controller->editor_manager()->right_drawer_manager()
                    : nullptr;
}

// Close any open drawer and let the close animation finish.
void CloseDrawers(ImGuiTestContext* ctx, RightDrawerManager* drawers) {
  ctx->PopupCloseAll();
  if (drawers->GetActiveDrawer() != DrawerType::kNone) {
    drawers->CloseDrawer();
  }
  ctx->Yield(30);
}

// The Settings drawer is open, its window is on screen, and it draws the
// real SettingsPanel (not the "Settings Not Available" placeholder).
void CheckSettingsDrawerShown(ImGuiTestContext* ctx,
                              RightDrawerManager* drawers,
                              const char* entry_point) {
  ctx->Yield(30);  // Let the open animation settle.
  ctx->LogInfo("%s: active drawer=%d", entry_point,
               static_cast<int>(drawers->GetActiveDrawer()));
  IM_CHECK_EQ_NO_RET(drawers->GetActiveDrawer(), DrawerType::kSettings);

  ImGuiWindow* panel = ctx->GetWindowByRef("//##RightPanel");
  IM_CHECK_NO_RET(panel != nullptr && panel->Active && !panel->Hidden);
  if (panel != nullptr) {
    ctx->LogInfo("%s: ##RightPanel size=(%.0f, %.0f)", entry_point,
                 panel->Size.x, panel->Size.y);
    IM_CHECK_GT_NO_RET(panel->Size.x, 100.0f);
  }

  IM_CHECK_NO_RET(drawers->settings_panel() != nullptr);

  // The first SettingsPanel section header is drawn inside the drawer (it is
  // skipped when the panel has no UserSettings).
  ImGuiTestItemInfo header =
      ctx->ItemInfo("//##RightPanel/**/" ICON_MD_SETTINGS " General Settings",
                    ImGuiTestOpFlags_NoError);
  IM_CHECK_NO_RET(header.ID != 0);
}

// ctx->MenuClick() cannot find ImGui 1.92 menu windows ("<label>###Menu_00").
// Open the menu with a real click, locate its popup, and click the item.
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

ImGuiWindow* FindWindowWithPrefix(ImGuiTestContext* ctx, const char* prefix) {
  for (ImGuiWindow* w : ctx->UiContext->Windows) {
    if (w->Active && std::string(w->Name).rfind(prefix, 0) == 0) {
      return w;
    }
  }
  return nullptr;
}

}  // namespace

void E2ETest_SettingsDrawerFromFileMenu(ImGuiTestContext* ctx) {
  Controller* controller = GetController(ctx);
  IM_CHECK(controller != nullptr);
  RightDrawerManager* drawers = GetDrawers(controller);
  IM_CHECK(drawers != nullptr);
  CloseDrawers(ctx, drawers);

  IM_CHECK(ClickMainMenuItem(ctx, "File", ICON_MD_SETTINGS " Settings"));
  CheckSettingsDrawerShown(ctx, drawers, "File > Settings");
  CloseDrawers(ctx, drawers);
}

void E2ETest_SettingsDrawerFromDrawersButton(ImGuiTestContext* ctx) {
  Controller* controller = GetController(ctx);
  IM_CHECK(controller != nullptr);
  RightDrawerManager* drawers = GetDrawers(controller);
  IM_CHECK(drawers != nullptr);
  CloseDrawers(ctx, drawers);

  ctx->SetRef("DockSpaceWindow");
  ctx->ItemClick("**/" ICON_MD_VERTICAL_SPLIT "##DrawersOverflow");
  ctx->Yield(3);
  ImGuiWindow* popup = FindWindowWithPrefix(ctx, "##Popup_");
  if (popup == nullptr && ctx->UiContext->OpenPopupStack.Size > 0) {
    popup = ctx->UiContext->OpenPopupStack.back().Window;
  }
  IM_CHECK(popup != nullptr);
  ctx->SetRef(popup->ID);
  ctx->ItemClick(ICON_MD_SETTINGS " Settings");
  ctx->SetRef("DockSpaceWindow");
  CheckSettingsDrawerShown(ctx, drawers, "Drawers button");
  CloseDrawers(ctx, drawers);
}

void E2ETest_SettingsDrawerFromKeyChord(ImGuiTestContext* ctx) {
  Controller* controller = GetController(ctx);
  IM_CHECK(controller != nullptr);
  RightDrawerManager* drawers = GetDrawers(controller);
  IM_CHECK(drawers != nullptr);
  CloseDrawers(ctx, drawers);

  // ShortcutManager's Ctrl is Cmd on macOS (ImGui swaps them).
  ctx->KeyPress(ImGuiMod_Ctrl | ImGuiKey_Comma);
  CheckSettingsDrawerShown(ctx, drawers, "Ctrl/Cmd+,");
  CloseDrawers(ctx, drawers);
}

void RunPaletteEntry(ImGuiTestContext* ctx, Controller* controller,
                     const char* entry) {
  auto* ui = controller->editor_manager()->ui_coordinator();
  IM_CHECK(ui != nullptr);
  ui->ShowCommandPalette(entry);
  ctx->Yield(3);
  ImGuiWindow* palette =
      FindWindowWithPrefix(ctx, ICON_MD_SEARCH " Command Palette");
  IM_CHECK(palette != nullptr);
  ctx->SetRef(palette->ID);
  ctx->ItemClick((std::string("**/") + entry).c_str());
  ctx->SetRef("DockSpaceWindow");
  ctx->Yield(3);
}

void E2ETest_SettingsDrawerFromPaletteSwitch(ImGuiTestContext* ctx) {
  Controller* controller = GetController(ctx);
  IM_CHECK(controller != nullptr);
  RightDrawerManager* drawers = GetDrawers(controller);
  IM_CHECK(drawers != nullptr);
  CloseDrawers(ctx, drawers);

  RunPaletteEntry(ctx, controller, "Switch to Settings Editor");
  CheckSettingsDrawerShown(ctx, drawers, "Palette: Switch to Settings Editor");
  CloseDrawers(ctx, drawers);
}

void E2ETest_SettingsDrawerFromPaletteToggle(ImGuiTestContext* ctx) {
  Controller* controller = GetController(ctx);
  IM_CHECK(controller != nullptr);
  RightDrawerManager* drawers = GetDrawers(controller);
  IM_CHECK(drawers != nullptr);
  CloseDrawers(ctx, drawers);

  RunPaletteEntry(ctx, controller, "View: Toggle Settings Panel");
  CheckSettingsDrawerShown(ctx, drawers, "Palette: Toggle Settings Panel");
  CloseDrawers(ctx, drawers);
}

// Same entry points once a ROM is loaded (the Welcome screen is gone and the
// editor surface draws the drawer). Skipped without a test ROM.
void E2ETest_SettingsDrawerWithRom(ImGuiTestContext* ctx) {
  Controller* controller = GetController(ctx);
  IM_CHECK(controller != nullptr);
  yaze::test::gui::LoadRomInTest(ctx,
                                 yaze::test::TestRomManager::GetTestRomPath());
  auto* rom = controller->GetCurrentRom();
  if (!rom || !rom->is_loaded()) {
    ctx->LogWarning("SettingsDrawer: no test ROM loaded; skipping.");
    return;
  }
  RightDrawerManager* drawers = GetDrawers(controller);
  IM_CHECK(drawers != nullptr);
  CloseDrawers(ctx, drawers);

  IM_CHECK(ClickMainMenuItem(ctx, "File", ICON_MD_SETTINGS " Settings"));
  CheckSettingsDrawerShown(ctx, drawers, "ROM: File > Settings");
  CloseDrawers(ctx, drawers);

  ctx->KeyPress(ImGuiMod_Ctrl | ImGuiKey_Comma);
  CheckSettingsDrawerShown(ctx, drawers, "ROM: Ctrl/Cmd+,");

  // Returning to the Welcome screen still closes a drawer the editor left
  // open.
  auto* ui = controller->editor_manager()->ui_coordinator();
  IM_CHECK(ui != nullptr);
  controller->editor_manager()->CloseRom();
  ctx->Yield(10);
  if (ui->ShouldShowWelcome()) {
    IM_CHECK_EQ_NO_RET(drawers->GetActiveDrawer(), DrawerType::kNone);
  } else {
    ctx->LogWarning("SettingsDrawer: Close ROM did not show Welcome.");
  }
  CloseDrawers(ctx, drawers);
}

namespace yaze {
namespace test {
namespace e2e {

void RegisterSettingsDrawerTests(ImGuiTestEngine* engine,
                                 Controller* controller) {
  struct Entry {
    const char* name;
    ImGuiTestTestFunc* func;
  };
  static const Entry kEntries[] = {
      {"FileMenu", E2ETest_SettingsDrawerFromFileMenu},
      {"DrawersButton", E2ETest_SettingsDrawerFromDrawersButton},
      {"KeyChord", E2ETest_SettingsDrawerFromKeyChord},
      {"PaletteSwitch", E2ETest_SettingsDrawerFromPaletteSwitch},
      {"PaletteToggle", E2ETest_SettingsDrawerFromPaletteToggle},
      // Loads a ROM, so it runs last.
      {"WithRom", E2ETest_SettingsDrawerWithRom},
  };
  for (const Entry& e : kEntries) {
    ImGuiTest* t = IM_REGISTER_TEST(engine, "SettingsDrawerSmoke", e.name);
    t->TestFunc = e.func;
    t->UserData = controller;
  }
}

}  // namespace e2e
}  // namespace test
}  // namespace yaze
