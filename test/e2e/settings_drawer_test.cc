#define IMGUI_DEFINE_MATH_OPERATORS

#include "e2e/settings_drawer_test.h"

#include <chrono>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <memory>
#include <string>
#include <utility>

#include "absl/cleanup/cleanup.h"

#include "app/controller.h"
#include "app/editor/editor_manager.h"
#include "app/editor/menu/right_drawer_manager.h"
#include "app/editor/shell/coordinator/ui_coordinator.h"
#include "app/editor/shell/windows/project_management_panel.h"
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

  // The scoped Settings IA search is drawn only by the real SettingsPanel
  // (the unavailable placeholder has no searchable category model).
  ImGuiTestItemInfo header = ctx->ItemInfo("//##RightPanel/**/##SettingsSearch",
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

void E2ETest_SettingsDrawerFromSidebarToggle(ImGuiTestContext* ctx) {
  Controller* controller = GetController(ctx);
  IM_CHECK(controller != nullptr);
  RightDrawerManager* drawers = GetDrawers(controller);
  IM_CHECK(drawers != nullptr);
  CloseDrawers(ctx, drawers);

  // Seed a non-Settings last tab so this covers both the single toggle and
  // persistent tab navigation.
  drawers->OpenDrawer(DrawerType::kProject);
  drawers->CloseDrawer();
  ctx->Yield(30);

  ctx->SetRef("DockSpaceWindow");
  ctx->ItemClick("**/" ICON_MD_VIEW_SIDEBAR "##RightSidebarToggle");
  ctx->Yield(3);
  IM_CHECK_EQ_NO_RET(drawers->GetActiveDrawer(), DrawerType::kProject);
  ctx->SetRef("//##RightPanel");
  ctx->ItemClick("**/##drawer_nav_6_Settings");
  ctx->SetRef("DockSpaceWindow");
  CheckSettingsDrawerShown(ctx, drawers, "Right sidebar toggle");
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

void E2ETest_SettingsDrawerMainMenus(ImGuiTestContext* ctx) {
  Controller* controller = GetController(ctx);
  IM_CHECK(controller != nullptr);
  yaze::test::gui::LoadRomInTest(ctx,
                                 yaze::test::TestRomManager::GetTestRomPath());
  IM_CHECK(controller->GetCurrentRom() != nullptr);
  IM_CHECK(controller->GetCurrentRom()->is_loaded());
  auto* manager = controller->editor_manager();
  manager->SwitchToEditor(yaze::editor::EditorType::kOverworld, true);
  ctx->Yield(20);
  auto* drawers = GetDrawers(controller);
  IM_CHECK(drawers != nullptr);
  drawers->OpenDrawer(DrawerType::kSettings);
  CheckSettingsDrawerShown(ctx, drawers, "Menu input regression");
  for (const char* name :
       {"File", "Edit", "View", "Windows", "Tools", "Help"}) {
    ctx->PopupCloseAll();
    const auto search = ctx->ItemInfo("//##RightPanel/**/##SettingsSearch");
    IM_CHECK(search.ID != 0);
    ctx->MouseMoveToPos(search.RectFull.GetCenter());
    ctx->MouseClick(ImGuiMouseButton_Left);
    ctx->Yield(2);
    IM_CHECK_EQ(ctx->UiContext->ActiveId, search.ID);
    ctx->SetRef("DockSpaceWindow");
    const auto item = ctx->ItemInfo((std::string("##MenuBar/") + name).c_str());
    IM_CHECK(item.ID != 0);
    // ItemClick focuses its host; raw coordinates must work without that help.
    ctx->MouseMoveToPos(item.RectFull.GetCenter());
    ctx->MouseClick(ImGuiMouseButton_Left);
    ctx->Yield(4);
    const std::string popup_name = std::string(name) + "###Menu_00";
    ImGuiWindow* popup = ImGui::FindWindowByName(popup_name.c_str());
    ctx->LogInfo("Menu %s open=%d drawer=%d", name,
                 popup && popup->Active && !popup->Hidden,
                 static_cast<int>(drawers->GetActiveDrawer()));
    IM_CHECK(popup != nullptr && popup->Active && !popup->Hidden);
    IM_CHECK_EQ(drawers->GetActiveDrawer(), DrawerType::kSettings);
  }
  CloseDrawers(ctx, drawers);
}

namespace {

struct ProjectBuildFieldsState {
  std::unique_ptr<yaze::project::YazeProject> project;
  yaze::editor::ProjectManagementPanel panel;
  std::filesystem::path root;
  bool active = false;
  bool fail_save = false;
  int save_count = 0;
};

ProjectBuildFieldsState& BuildFieldsState() {
  static ProjectBuildFieldsState state;
  return state;
}

void DrawProjectBuildFields(ImGuiTestContext*) {
  auto& state = BuildFieldsState();
  if (!state.active) {
    return;
  }
  ImGui::SetNextWindowPos(ImVec2(20, 20), ImGuiCond_Always);
  ImGui::SetNextWindowSize(ImVec2(720, 640), ImGuiCond_Always);
  if (ImGui::Begin("Project Build Fields", nullptr,
                   ImGuiWindowFlags_NoSavedSettings)) {
    state.panel.Draw();
  }
  ImGui::End();
}

void LongProjectBuildFieldsRoundtrip(ImGuiTestContext* ctx) {
  auto& state = BuildFieldsState();
  state.root =
      std::filesystem::temp_directory_path() /
      ("yaze-long-build-fields-" +
       std::to_string(
           std::chrono::steady_clock::now().time_since_epoch().count()));
  std::error_code error;
  std::filesystem::create_directories(state.root, error);
  IM_CHECK(!error);
  absl::Cleanup cleanup = [&] {
    state.active = false;
    state.panel.SetProject(nullptr);
    state.panel.SetSaveProjectCallback({});
    state.project.reset();
    if (ctx->IsError()) {
      ctx->LogInfo("Retained failed project fixture: %s",
                   state.root.string().c_str());
    } else {
      std::filesystem::remove_all(state.root, error);
    }
  };
  const auto project_path = state.root / "project.yaze";
  const std::string initial_script =
      "echo '" + std::string(4096, 'a') + " initial-script-tail'";
  std::string initial_target = "output/";
  for (int part = 0; part < 36; ++part) {
    initial_target += "expanded-data/";
  }
  initial_target += "candidatex.sfc";
  IM_CHECK_GT(initial_script.size(), 511u);
  IM_CHECK_GT(initial_target.size(), 255u);

  state.project = std::make_unique<yaze::project::YazeProject>();
  state.project->name = "Long build fields";
  state.project->filepath = project_path.string();
  state.project->build_script = initial_script;
  state.project->build_target = initial_target;
  IM_CHECK(state.project->Save().ok());
  const auto read_project = [&] {
    std::ifstream input(project_path, std::ios::binary);
    return std::string(std::istreambuf_iterator<char>(input), {});
  };
  const std::string before = read_project();
  state.fail_save = true;
  state.save_count = 0;
  state.panel.SetProject(state.project.get());
  state.panel.SetSaveProjectCallback([&state] {
    ++state.save_count;
    return state.fail_save
               ? absl::InternalError("Expected build-fields save failure")
               : state.project->Save();
  });
  state.active = true;
  ctx->Yield(2);
  ctx->SetRef("Project Build Fields");

  // Append to a loaded long value: replacing the whole field would miss
  // truncation during SetProject/ReloadProjectBuffers.
  const std::string script_edit = " && echo edited-script-tail";
  const std::string target_edit = ".edited";
  ctx->ItemInput("##build_script");
  ctx->KeyCharsAppendEnter(script_edit.c_str());
  ctx->ItemInput("##build_target");
  ctx->KeyCharsAppendEnter(target_edit.c_str());
  IM_CHECK(state.project->build_script == initial_script + script_edit);
  IM_CHECK(state.project->build_target == initial_target + target_edit);
  IM_CHECK(state.panel.IsProjectDirty());

  ctx->ItemClick(ICON_MD_SAVE " Save Project");
  IM_CHECK_EQ(state.save_count, 1);
  IM_CHECK(state.panel.IsProjectDirty());
  IM_CHECK(read_project() == before);
  state.fail_save = false;
  ctx->ItemClick(ICON_MD_SAVE " Save Project");
  IM_CHECK_EQ(state.save_count, 2);
  IM_CHECK(!state.panel.IsProjectDirty());

  auto reopened = std::make_unique<yaze::project::YazeProject>();
  IM_CHECK(reopened->Open(project_path.string()).ok());
  IM_CHECK(reopened->build_script == initial_script + script_edit);
  IM_CHECK(reopened->build_target == initial_target + target_edit);
  state.project = std::move(reopened);
  state.panel.SetProject(state.project.get());
  ctx->Yield(2);

  // Exercise the actual inputs again after persistence and buffer reload.
  const std::string second_script_edit = " && echo reopened-script-tail";
  const std::string second_target_edit = ".reopened";
  ctx->ItemInput("##build_script");
  ctx->KeyCharsAppendEnter(second_script_edit.c_str());
  ctx->ItemInput("##build_target");
  ctx->KeyCharsAppendEnter(second_target_edit.c_str());
  ctx->ItemClick(ICON_MD_SAVE " Save Project");
  IM_CHECK_EQ(state.save_count, 3);
  IM_CHECK(!state.panel.IsProjectDirty());
  yaze::project::YazeProject final_project;
  IM_CHECK(final_project.Open(project_path.string()).ok());
  IM_CHECK(final_project.build_script ==
           initial_script + script_edit + second_script_edit);
  IM_CHECK(final_project.build_target ==
           initial_target + target_edit + second_target_edit);
}

}  // namespace

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
      {"SidebarToggle", E2ETest_SettingsDrawerFromSidebarToggle},
      {"KeyChord", E2ETest_SettingsDrawerFromKeyChord},
      {"PaletteSwitch", E2ETest_SettingsDrawerFromPaletteSwitch},
      {"PaletteToggle", E2ETest_SettingsDrawerFromPaletteToggle},
      // Loads a ROM, so it runs last.
      {"WithRom", E2ETest_SettingsDrawerWithRom},
      {"MainMenusWithSettings", E2ETest_SettingsDrawerMainMenus},
  };
  for (const Entry& e : kEntries) {
    ImGuiTest* t = IM_REGISTER_TEST(engine, "SettingsDrawerSmoke", e.name);
    t->TestFunc = e.func;
    t->UserData = controller;
  }
  ImGuiTest* build_fields =
      IM_REGISTER_TEST(engine, "ProjectManagement", "LongBuildFieldsRoundtrip");
  build_fields->GuiFunc = DrawProjectBuildFields;
  build_fields->TestFunc = LongProjectBuildFieldsRoundtrip;
}

}  // namespace e2e
}  // namespace test
}  // namespace yaze
