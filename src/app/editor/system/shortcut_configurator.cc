#include "app/editor/system/shortcut_configurator.h"

#include "absl/functional/bind_front.h"
#include "absl/strings/str_format.h"
#include "app/editor/editor_manager.h"
#include "app/editor/system/editor_card_registry.h"
#include "app/editor/system/menu_orchestrator.h"
#include "app/editor/system/popup_manager.h"
#include "app/editor/system/proposal_drawer.h"
#include "app/editor/system/rom_file_manager.h"
#include "app/editor/system/session_coordinator.h"
#include "app/editor/system/toast_manager.h"
#include "app/editor/ui/ui_coordinator.h"
#include "app/editor/ui/workspace_manager.h"
#include "core/project.h"

namespace yaze::editor {

namespace {

void RegisterIfValid(ShortcutManager* shortcut_manager, const std::string& name,
                     const std::vector<ImGuiKey>& keys,
                     std::function<void()> callback) {
  if (!shortcut_manager || !callback) {
    return;
  }
  shortcut_manager->RegisterShortcut(name, keys, std::move(callback));
}

void RegisterIfValid(ShortcutManager* shortcut_manager, const std::string& name,
                     ImGuiKey key, std::function<void()> callback) {
  if (!shortcut_manager || !callback) {
    return;
  }
  shortcut_manager->RegisterShortcut(name, key, std::move(callback));
}

}  // namespace

void ConfigureEditorShortcuts(const ShortcutDependencies& deps,
                              ShortcutManager* shortcut_manager) {
  if (!shortcut_manager) {
    return;
  }

  auto* editor_manager = deps.editor_manager;
  auto* ui_coordinator = deps.ui_coordinator;
  auto* popup_manager = deps.popup_manager;
  auto* card_registry = deps.card_registry;

  RegisterIfValid(shortcut_manager, "global.toggle_sidebar",
                  {ImGuiKey_LeftCtrl, ImGuiKey_B}, [ui_coordinator]() {
                    if (ui_coordinator) {
                      ui_coordinator->ToggleCardSidebar();
                    }
                  });

  RegisterIfValid(shortcut_manager, "Open", {ImGuiMod_Ctrl, ImGuiKey_O},
                  [editor_manager]() {
                    if (editor_manager) {
                      editor_manager->LoadRom();
                    }
                  });

  RegisterIfValid(shortcut_manager, "Save", {ImGuiMod_Ctrl, ImGuiKey_S},
                  [editor_manager]() {
                    if (editor_manager) {
                      editor_manager->SaveRom();
                    }
                  });

  RegisterIfValid(shortcut_manager, "Save As",
                  {ImGuiMod_Ctrl, ImGuiMod_Shift, ImGuiKey_S},
                  [editor_manager]() {
                    if (editor_manager) {
                      // Use project-aware default filename when possible
                      std::string filename =
                          editor_manager->GetCurrentRom()
                              ? editor_manager->GetCurrentRom()->filename()
                              : "";
                      editor_manager->SaveRomAs(filename);
                    }
                  });

  RegisterIfValid(shortcut_manager, "Close ROM", {ImGuiMod_Ctrl, ImGuiKey_W},
                  [editor_manager]() {
                    if (editor_manager && editor_manager->GetCurrentRom()) {
                      editor_manager->GetCurrentRom()->Close();
                    }
                  });

  RegisterIfValid(shortcut_manager, "Quit", {ImGuiMod_Ctrl, ImGuiKey_Q},
                  [editor_manager]() {
                    if (editor_manager) {
                      editor_manager->Quit();
                    }
                  });

  RegisterIfValid(shortcut_manager, "Undo", {ImGuiMod_Ctrl, ImGuiKey_Z},
                  [editor_manager]() {
                    if (editor_manager && editor_manager->GetCurrentEditor()) {
                      editor_manager->GetCurrentEditor()->Undo();
                    }
                  });

  RegisterIfValid(shortcut_manager, "Redo",
                  {ImGuiMod_Ctrl, ImGuiMod_Shift, ImGuiKey_Z},
                  [editor_manager]() {
                    if (editor_manager && editor_manager->GetCurrentEditor()) {
                      editor_manager->GetCurrentEditor()->Redo();
                    }
                  });

  RegisterIfValid(shortcut_manager, "Cut", {ImGuiMod_Ctrl, ImGuiKey_X},
                  [editor_manager]() {
                    if (editor_manager && editor_manager->GetCurrentEditor()) {
                      editor_manager->GetCurrentEditor()->Cut();
                    }
                  });

  RegisterIfValid(shortcut_manager, "Copy", {ImGuiMod_Ctrl, ImGuiKey_C},
                  [editor_manager]() {
                    if (editor_manager && editor_manager->GetCurrentEditor()) {
                      editor_manager->GetCurrentEditor()->Copy();
                    }
                  });

  RegisterIfValid(shortcut_manager, "Paste", {ImGuiMod_Ctrl, ImGuiKey_V},
                  [editor_manager]() {
                    if (editor_manager && editor_manager->GetCurrentEditor()) {
                      editor_manager->GetCurrentEditor()->Paste();
                    }
                  });

  RegisterIfValid(shortcut_manager, "Find", {ImGuiMod_Ctrl, ImGuiKey_F},
                  [editor_manager]() {
                    if (editor_manager && editor_manager->GetCurrentEditor()) {
                      editor_manager->GetCurrentEditor()->Find();
                    }
                  });

  RegisterIfValid(shortcut_manager, "Command Palette",
                  {ImGuiMod_Ctrl, ImGuiMod_Shift, ImGuiKey_P},
                  [ui_coordinator]() {
                    if (ui_coordinator) {
                      ui_coordinator->ShowCommandPalette();
                    }
                  });

  RegisterIfValid(shortcut_manager, "Global Search",
                  {ImGuiMod_Ctrl, ImGuiMod_Shift, ImGuiKey_K},
                  [ui_coordinator]() {
                    if (ui_coordinator) {
                      ui_coordinator->ShowGlobalSearch();
                    }
                  });

  RegisterIfValid(
      shortcut_manager, "Load Last ROM", {ImGuiMod_Ctrl, ImGuiKey_R},
      [editor_manager]() {
        auto& recent = project::RecentFilesManager::GetInstance();
        if (!recent.GetRecentFiles().empty() && editor_manager) {
          editor_manager->OpenRomOrProject(recent.GetRecentFiles().front());
        }
      });

  RegisterIfValid(shortcut_manager, "Show About", ImGuiKey_F1,
                  [popup_manager]() {
                    if (popup_manager) {
                      popup_manager->Show("About");
                    }
                  });

  auto register_editor_shortcut = [&](EditorType type, ImGuiKey key) {
    RegisterIfValid(shortcut_manager,
                    absl::StrFormat("switch.%d", static_cast<int>(type)),
                    {ImGuiMod_Ctrl, key}, [editor_manager, type]() {
                      if (editor_manager) {
                        editor_manager->SwitchToEditor(type);
                      }
                    });
  };

  register_editor_shortcut(EditorType::kOverworld, ImGuiKey_1);
  register_editor_shortcut(EditorType::kDungeon, ImGuiKey_2);
  register_editor_shortcut(EditorType::kGraphics, ImGuiKey_3);
  register_editor_shortcut(EditorType::kSprite, ImGuiKey_4);
  register_editor_shortcut(EditorType::kMessage, ImGuiKey_5);
  register_editor_shortcut(EditorType::kMusic, ImGuiKey_6);
  register_editor_shortcut(EditorType::kPalette, ImGuiKey_7);
  register_editor_shortcut(EditorType::kScreen, ImGuiKey_8);
  register_editor_shortcut(EditorType::kAssembly, ImGuiKey_9);
  register_editor_shortcut(EditorType::kSettings, ImGuiKey_0);

  RegisterIfValid(shortcut_manager, "Editor Selection",
                  {ImGuiMod_Ctrl, ImGuiKey_E}, [ui_coordinator]() {
                    if (ui_coordinator) {
                      ui_coordinator->ShowEditorSelection();
                    }
                  });

  RegisterIfValid(shortcut_manager, "Card Browser",
                  {ImGuiMod_Ctrl, ImGuiMod_Shift, ImGuiKey_B},
                  [ui_coordinator]() {
                    if (ui_coordinator) {
                      ui_coordinator->ShowCardBrowser();
                    }
                  });

  if (card_registry) {
    // Note: Using Ctrl+Alt for card shortcuts to avoid conflicts with Save As (Ctrl+Shift+S)
    RegisterIfValid(shortcut_manager, "Show Dungeon Cards",
                    {ImGuiMod_Ctrl, ImGuiMod_Alt, ImGuiKey_D},
                    [card_registry]() {
                      card_registry->ShowAllCardsInCategory("Dungeon");
                    });
    RegisterIfValid(shortcut_manager, "Show Graphics Cards",
                    {ImGuiMod_Ctrl, ImGuiMod_Alt, ImGuiKey_G},
                    [card_registry]() {
                      card_registry->ShowAllCardsInCategory("Graphics");
                    });
    RegisterIfValid(
        shortcut_manager, "Show Screen Cards",
        {ImGuiMod_Ctrl, ImGuiMod_Alt, ImGuiKey_S},
        [card_registry]() { card_registry->ShowAllCardsInCategory("Screen"); });
  }

#ifdef YAZE_WITH_GRPC
  RegisterIfValid(shortcut_manager, "Agent Editor",
                  {ImGuiMod_Ctrl, ImGuiMod_Shift, ImGuiKey_A},
                  [editor_manager]() {
                    if (editor_manager) {
                      editor_manager->ShowAIAgent();
                    }
                  });

  RegisterIfValid(shortcut_manager, "Agent Chat History",
                  {ImGuiMod_Ctrl, ImGuiKey_H}, [editor_manager]() {
                    if (editor_manager) {
                      editor_manager->ShowChatHistory();
                    }
                  });

  RegisterIfValid(shortcut_manager, "Proposal Drawer",
                  {ImGuiMod_Ctrl | ImGuiMod_Shift,
                   ImGuiKey_R},  // Changed from Ctrl+P to Ctrl+Shift+R
                  [editor_manager]() {
                    if (editor_manager) {
                      editor_manager->ShowProposalDrawer();
                    }
                  });
#endif
}

void ConfigureMenuShortcuts(const ShortcutDependencies& deps,
                            ShortcutManager* shortcut_manager) {
  if (!shortcut_manager) {
    return;
  }

  auto* menu_orchestrator = deps.menu_orchestrator;
  auto* session_coordinator = deps.session_coordinator;
  auto* workspace_manager = deps.workspace_manager;

  RegisterIfValid(shortcut_manager, "New Session",
                  {ImGuiMod_Ctrl, ImGuiMod_Shift, ImGuiKey_N},
                  [session_coordinator]() {
                    if (session_coordinator) {
                      session_coordinator->CreateNewSession();
                    }
                  });

  RegisterIfValid(shortcut_manager, "Duplicate Session",
                  {ImGuiMod_Ctrl, ImGuiMod_Shift, ImGuiKey_D},
                  [session_coordinator]() {
                    if (session_coordinator) {
                      session_coordinator->DuplicateCurrentSession();
                    }
                  });

  RegisterIfValid(shortcut_manager, "Close Session",
                  {ImGuiMod_Ctrl, ImGuiMod_Shift, ImGuiKey_W},
                  [session_coordinator]() {
                    if (session_coordinator) {
                      session_coordinator->CloseCurrentSession();
                    }
                  });

  RegisterIfValid(shortcut_manager, "Session Switcher",
                  {ImGuiMod_Ctrl, ImGuiKey_Tab}, [session_coordinator]() {
                    if (session_coordinator) {
                      session_coordinator->ShowSessionSwitcher();
                    }
                  });

  RegisterIfValid(shortcut_manager, "Save Layout",
                  {ImGuiMod_Ctrl, ImGuiMod_Shift, ImGuiKey_L},
                  [workspace_manager]() {
                    if (workspace_manager) {
                      workspace_manager->SaveWorkspaceLayout();
                    }
                  });

  RegisterIfValid(shortcut_manager, "Load Layout",
                  {ImGuiMod_Ctrl, ImGuiMod_Shift, ImGuiKey_O},
                  [workspace_manager]() {
                    if (workspace_manager) {
                      workspace_manager->LoadWorkspaceLayout();
                    }
                  });

  // Note: Changed from Ctrl+Shift+R to Ctrl+Alt+R to avoid conflict with Proposal Drawer
  RegisterIfValid(shortcut_manager, "Reset Layout",
                  {ImGuiMod_Ctrl, ImGuiMod_Alt, ImGuiKey_R},
                  [workspace_manager]() {
                    if (workspace_manager) {
                      workspace_manager->ResetWorkspaceLayout();
                    }
                  });

  RegisterIfValid(shortcut_manager, "Maximize Window", ImGuiKey_F11,
                  [workspace_manager]() {
                    if (workspace_manager) {
                      workspace_manager->MaximizeCurrentWindow();
                    }
                  });

#ifdef YAZE_ENABLE_TESTING
  RegisterIfValid(shortcut_manager, "Test Dashboard",
                  {ImGuiMod_Ctrl, ImGuiKey_T}, [menu_orchestrator]() {
                    if (menu_orchestrator) {
                      menu_orchestrator->OnShowTestDashboard();
                    }
                  });
#endif
}

}  // namespace yaze::editor
