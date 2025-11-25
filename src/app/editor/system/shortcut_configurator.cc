#include "app/editor/system/shortcut_configurator.h"

#include "absl/functional/bind_front.h"
#include "absl/strings/str_format.h"
#include "absl/strings/str_split.h"
#include "app/editor/editor_manager.h"
#include "app/editor/system/editor_card_registry.h"
#include "app/editor/system/menu_orchestrator.h"
#include "app/editor/system/popup_manager.h"
#include "app/editor/system/proposal_drawer.h"
#include "app/editor/system/rom_file_manager.h"
#include "app/editor/system/session_coordinator.h"
#include "app/editor/system/toast_manager.h"
#include "app/editor/system/user_settings.h"
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
    // Note: Using Ctrl+Alt for card shortcuts to avoid conflicts with Save As
    // (Ctrl+Shift+S)
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

  // Note: Changed from Ctrl+Shift+R to Ctrl+Alt+R to avoid conflict with
  // Proposal Drawer
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

namespace {

// Helper to parse shortcut strings like "Ctrl+Shift+R" into ImGuiKey combinations
std::vector<ImGuiKey> ParseShortcutString(const std::string& shortcut) {
  std::vector<ImGuiKey> keys;
  if (shortcut.empty()) {
    return keys;
  }

  std::vector<std::string> parts = absl::StrSplit(shortcut, '+');
  
  for (const auto& part : parts) {
    std::string trimmed = part;
    // Trim whitespace
    while (!trimmed.empty() && (trimmed.front() == ' ' || trimmed.front() == '\t')) {
      trimmed = trimmed.substr(1);
    }
    while (!trimmed.empty() && (trimmed.back() == ' ' || trimmed.back() == '\t')) {
      trimmed.pop_back();
    }
    
    if (trimmed.empty()) continue;
    
    // Modifiers
    if (trimmed == "Ctrl" || trimmed == "Control") {
      keys.push_back(ImGuiMod_Ctrl);
    } else if (trimmed == "Shift") {
      keys.push_back(ImGuiMod_Shift);
    } else if (trimmed == "Alt") {
      keys.push_back(ImGuiMod_Alt);
    } else if (trimmed == "Super" || trimmed == "Win" || trimmed == "Cmd") {
      keys.push_back(ImGuiMod_Super);
    }
    // Letter keys
    else if (trimmed.length() == 1 && trimmed[0] >= 'A' && trimmed[0] <= 'Z') {
      keys.push_back(static_cast<ImGuiKey>(ImGuiKey_A + (trimmed[0] - 'A')));
    }
    else if (trimmed.length() == 1 && trimmed[0] >= 'a' && trimmed[0] <= 'z') {
      keys.push_back(static_cast<ImGuiKey>(ImGuiKey_A + (trimmed[0] - 'a')));
    }
    // Number keys
    else if (trimmed.length() == 1 && trimmed[0] >= '0' && trimmed[0] <= '9') {
      keys.push_back(static_cast<ImGuiKey>(ImGuiKey_0 + (trimmed[0] - '0')));
    }
    // Function keys
    else if (trimmed[0] == 'F' && trimmed.length() >= 2) {
      try {
        int fnum = std::stoi(trimmed.substr(1));
        if (fnum >= 1 && fnum <= 12) {
          keys.push_back(static_cast<ImGuiKey>(ImGuiKey_F1 + (fnum - 1)));
        }
      } catch (const std::exception&) {
        // Invalid function key format (e.g., "Fabc") - skip this key
      }
    }
  }
  
  return keys;
}

}  // namespace

void ConfigureCardShortcuts(const ShortcutDependencies& deps,
                            ShortcutManager* shortcut_manager) {
  if (!shortcut_manager || !deps.card_registry) {
    return;
  }

  auto* card_registry = deps.card_registry;
  auto* user_settings = deps.user_settings;

  // Get all categories and cards
  auto categories = card_registry->GetAllCategories();

  for (const auto& category : categories) {
    auto cards = card_registry->GetCardsInCategory(0, category);

    for (const auto& card : cards) {
      std::string shortcut_string;

      // Check for user-defined shortcut first
      if (user_settings) {
        auto it = user_settings->prefs().card_shortcuts.find(card.card_id);
        if (it != user_settings->prefs().card_shortcuts.end()) {
          shortcut_string = it->second;
        }
      }

      // Fall back to default shortcut_hint
      if (shortcut_string.empty() && !card.shortcut_hint.empty()) {
        shortcut_string = card.shortcut_hint;
      }

      // If we have a shortcut, parse and register it
      if (!shortcut_string.empty()) {
        auto keys = ParseShortcutString(shortcut_string);
        if (!keys.empty()) {
          std::string card_id_copy = card.card_id;
          RegisterIfValid(
              shortcut_manager, 
              absl::StrFormat("card.%s", card.card_id),
              keys,
              [card_registry, card_id_copy]() {
                // Use session_id 0 for global card toggle
                card_registry->ToggleCard(0, card_id_copy);
              });
        }
      }
    }
  }
}

}  // namespace yaze::editor
