#include "app/editor/system/shortcut_configurator.h"

#include "absl/functional/bind_front.h"
#include "absl/strings/str_format.h"
#include "app/editor/editor_manager.h"
#include "app/editor/menu/menu_orchestrator.h"
#include "app/editor/menu/right_panel_manager.h"
#include "app/editor/system/panel_manager.h"
#include "app/editor/system/proposal_drawer.h"
#include "app/editor/system/rom_file_manager.h"
#include "app/editor/system/session_coordinator.h"
#include "app/editor/system/user_settings.h"
#include "app/editor/ui/popup_manager.h"
#include "app/editor/ui/toast_manager.h"
#include "app/editor/ui/ui_coordinator.h"
#include "app/editor/ui/workspace_manager.h"
#include "core/project.h"

namespace yaze::editor {

namespace {

void RegisterIfValid(ShortcutManager* shortcut_manager, const std::string& name,
                     const std::vector<ImGuiKey>& keys,
                     std::function<void()> callback,
                     Shortcut::Scope scope = Shortcut::Scope::kGlobal) {
  if (!shortcut_manager || !callback) {
    return;
  }
  shortcut_manager->RegisterShortcut(name, keys, std::move(callback), scope);
}

void RegisterIfValid(ShortcutManager* shortcut_manager, const std::string& name,
                     ImGuiKey key, std::function<void()> callback,
                     Shortcut::Scope scope = Shortcut::Scope::kGlobal) {
  if (!shortcut_manager || !callback) {
    return;
  }
  shortcut_manager->RegisterShortcut(name, key, std::move(callback), scope);
}

struct EditorShortcutDef {
  std::string id;
  std::vector<ImGuiKey> keys;
  std::string description;
};

const std::vector<EditorShortcutDef> kMusicEditorShortcuts = {
    {"music.play_pause", {ImGuiKey_Space}, "Play/Pause current song"},
    {"music.stop", {ImGuiKey_Escape}, "Stop playback"},
    {"music.speed_up", {ImGuiKey_Equal}, "Increase playback speed"},
    {"music.speed_up_keypad",
     {ImGuiKey_KeypadAdd},
     "Increase playback speed (keypad)"},
    {"music.speed_down", {ImGuiKey_Minus}, "Decrease playback speed"},
    {"music.speed_down_keypad",
     {ImGuiKey_KeypadSubtract},
     "Decrease playback speed (keypad)"},
};

const std::vector<EditorShortcutDef> kDungeonEditorShortcuts = {
    {"dungeon.object.select_tool", {ImGuiKey_S}, "Select tool"},
    {"dungeon.object.place_tool", {ImGuiKey_P}, "Place tool"},
    {"dungeon.object.delete_tool", {ImGuiKey_D}, "Delete tool"},
    {"dungeon.object.next_object", {ImGuiKey_RightBracket}, "Next object"},
    {"dungeon.object.prev_object", {ImGuiKey_LeftBracket}, "Previous object"},
    {"dungeon.object.copy", {ImGuiMod_Ctrl, ImGuiKey_C}, "Copy selection"},
    {"dungeon.object.paste", {ImGuiMod_Ctrl, ImGuiKey_V}, "Paste selection"},
    {"dungeon.object.delete", {ImGuiKey_Delete}, "Delete selection"},
};

const std::vector<EditorShortcutDef> kOverworldShortcuts = {
    {"overworld.brush_toggle", {ImGuiKey_B}, "Toggle brush"},
    {"overworld.fill", {ImGuiKey_F}, "Fill tool"},
    {"overworld.next_tile", {ImGuiKey_RightBracket}, "Next tile"},
    {"overworld.prev_tile", {ImGuiKey_LeftBracket}, "Previous tile"},
};

const std::vector<EditorShortcutDef> kGraphicsShortcuts = {
    // Sheet navigation
    {"graphics.next_sheet", {ImGuiKey_PageDown}, "Next sheet"},
    {"graphics.prev_sheet", {ImGuiKey_PageUp}, "Previous sheet"},

    // Tool selection shortcuts
    {"graphics.tool.select", {ImGuiKey_V}, "Select tool"},
    {"graphics.tool.pencil", {ImGuiKey_B}, "Pencil tool"},
    {"graphics.tool.brush", {ImGuiKey_P}, "Brush tool"},
    {"graphics.tool.eraser", {ImGuiKey_E}, "Eraser tool"},
    {"graphics.tool.fill", {ImGuiKey_G}, "Fill tool"},
    {"graphics.tool.line", {ImGuiKey_L}, "Line tool"},
    {"graphics.tool.rectangle", {ImGuiKey_R}, "Rectangle tool"},
    {"graphics.tool.eyedropper", {ImGuiKey_I}, "Eyedropper tool"},

    // Zoom controls
    {"graphics.zoom_in", {ImGuiKey_Equal}, "Zoom in"},
    {"graphics.zoom_in_keypad", {ImGuiKey_KeypadAdd}, "Zoom in (keypad)"},
    {"graphics.zoom_out", {ImGuiKey_Minus}, "Zoom out"},
    {"graphics.zoom_out_keypad",
     {ImGuiKey_KeypadSubtract},
     "Zoom out (keypad)"},

    // View toggles
    {"graphics.toggle_grid", {ImGuiMod_Ctrl, ImGuiKey_G}, "Toggle grid"},
};

}  // namespace

void ConfigureEditorShortcuts(const ShortcutDependencies& deps,
                              ShortcutManager* shortcut_manager) {
  if (!shortcut_manager) {
    return;
  }

  auto* editor_manager = deps.editor_manager;
  auto* ui_coordinator = deps.ui_coordinator;
  auto* popup_manager = deps.popup_manager;
  auto* panel_manager = deps.panel_manager;

  // Toggle activity bar (48px icon strip) visibility
  RegisterIfValid(
      shortcut_manager, "view.toggle_activity_bar", {ImGuiMod_Ctrl, ImGuiKey_B},
      [panel_manager]() {
        if (panel_manager) {
          panel_manager->ToggleSidebarVisibility();
        }
      },
      Shortcut::Scope::kGlobal);

  // Toggle side panel (250px expanded panel) expansion
  RegisterIfValid(
      shortcut_manager, "view.toggle_side_panel",
      {ImGuiMod_Ctrl, ImGuiMod_Alt, ImGuiKey_E},
      [panel_manager]() {
        if (panel_manager) {
          panel_manager->TogglePanelExpanded();
        }
      },
      Shortcut::Scope::kGlobal);

  RegisterIfValid(
      shortcut_manager, "Open", {ImGuiMod_Ctrl, ImGuiKey_O},
      [editor_manager]() {
        if (editor_manager) {
          editor_manager->LoadRom();
        }
      },
      Shortcut::Scope::kGlobal);

  RegisterIfValid(shortcut_manager, "Save", {ImGuiMod_Ctrl, ImGuiKey_S},
                  [editor_manager]() {
                    if (editor_manager) {
                      editor_manager->SaveRom();
                    }
                  });

  RegisterIfValid(
      shortcut_manager, "Save As", {ImGuiMod_Ctrl, ImGuiMod_Shift, ImGuiKey_S},
      [editor_manager]() {
        if (editor_manager) {
          // Use project-aware default filename when possible
          std::string filename =
              editor_manager->GetCurrentRom()
                  ? editor_manager->GetCurrentRom()->filename()
                  : "";
          editor_manager->SaveRomAs(filename);
        }
      },
      Shortcut::Scope::kGlobal);

  RegisterIfValid(
      shortcut_manager, "Close ROM", {ImGuiMod_Ctrl, ImGuiKey_W},
      [editor_manager]() {
        if (editor_manager && editor_manager->GetCurrentRom()) {
          editor_manager->GetCurrentRom()->Close();
        }
      },
      Shortcut::Scope::kGlobal);

  RegisterIfValid(
      shortcut_manager, "Quit", {ImGuiMod_Ctrl, ImGuiKey_Q},
      [editor_manager]() {
        if (editor_manager) {
          editor_manager->Quit();
        }
      },
      Shortcut::Scope::kGlobal);

  RegisterIfValid(
      shortcut_manager, "Undo", {ImGuiMod_Ctrl, ImGuiKey_Z},
      [editor_manager]() {
        if (editor_manager && editor_manager->GetCurrentEditor()) {
          editor_manager->GetCurrentEditor()->Undo();
        }
      },
      Shortcut::Scope::kEditor);

  RegisterIfValid(
      shortcut_manager, "Redo", {ImGuiMod_Ctrl, ImGuiMod_Shift, ImGuiKey_Z},
      [editor_manager]() {
        if (editor_manager && editor_manager->GetCurrentEditor()) {
          editor_manager->GetCurrentEditor()->Redo();
        }
      },
      Shortcut::Scope::kEditor);

  RegisterIfValid(
      shortcut_manager, "Cut", {ImGuiMod_Ctrl, ImGuiKey_X},
      [editor_manager]() {
        if (editor_manager && editor_manager->GetCurrentEditor()) {
          editor_manager->GetCurrentEditor()->Cut();
        }
      },
      Shortcut::Scope::kEditor);

  RegisterIfValid(
      shortcut_manager, "Copy", {ImGuiMod_Ctrl, ImGuiKey_C},
      [editor_manager]() {
        if (editor_manager && editor_manager->GetCurrentEditor()) {
          editor_manager->GetCurrentEditor()->Copy();
        }
      },
      Shortcut::Scope::kEditor);

  RegisterIfValid(
      shortcut_manager, "Paste", {ImGuiMod_Ctrl, ImGuiKey_V},
      [editor_manager]() {
        if (editor_manager && editor_manager->GetCurrentEditor()) {
          editor_manager->GetCurrentEditor()->Paste();
        }
      },
      Shortcut::Scope::kEditor);

  RegisterIfValid(
      shortcut_manager, "Find", {ImGuiMod_Ctrl, ImGuiKey_F},
      [editor_manager]() {
        if (editor_manager && editor_manager->GetCurrentEditor()) {
          editor_manager->GetCurrentEditor()->Find();
        }
      },
      Shortcut::Scope::kEditor);

  RegisterIfValid(
      shortcut_manager, "Command Palette",
      {ImGuiMod_Ctrl, ImGuiMod_Shift, ImGuiKey_P},
      [ui_coordinator]() {
        if (ui_coordinator) {
          ui_coordinator->ShowCommandPalette();
        }
      },
      Shortcut::Scope::kGlobal);

  RegisterIfValid(
      shortcut_manager, "Global Search",
      {ImGuiMod_Ctrl, ImGuiMod_Shift, ImGuiKey_K},
      [ui_coordinator]() {
        if (ui_coordinator) {
          ui_coordinator->ShowGlobalSearch();
        }
      },
      Shortcut::Scope::kGlobal);

  RegisterIfValid(
      shortcut_manager, "Load Last ROM", {ImGuiMod_Ctrl, ImGuiKey_R},
      [editor_manager]() {
        auto& recent = project::RecentFilesManager::GetInstance();
        if (!recent.GetRecentFiles().empty() && editor_manager) {
          editor_manager->OpenRomOrProject(recent.GetRecentFiles().front());
        }
      },
      Shortcut::Scope::kGlobal);

  RegisterIfValid(
      shortcut_manager, "Show About", ImGuiKey_F1,
      [popup_manager]() {
        if (popup_manager) {
          popup_manager->Show("About");
        }
      },
      Shortcut::Scope::kGlobal);

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

  // ============================================================================
  // Editor Switch Commands (command palette with friendly names)
  // ============================================================================
  auto register_editor_command = [&](EditorType type, const std::string& name) {
    shortcut_manager->RegisterCommand(
        absl::StrFormat("Switch to %s Editor", name), [editor_manager, type]() {
          if (editor_manager) {
            editor_manager->SwitchToEditor(type);
          }
        });
  };

  register_editor_command(EditorType::kOverworld, "Overworld");
  register_editor_command(EditorType::kDungeon, "Dungeon");
  register_editor_command(EditorType::kGraphics, "Graphics");
  register_editor_command(EditorType::kSprite, "Sprite");
  register_editor_command(EditorType::kMessage, "Message");
  register_editor_command(EditorType::kMusic, "Music");
  register_editor_command(EditorType::kPalette, "Palette");
  register_editor_command(EditorType::kScreen, "Screen");
  register_editor_command(EditorType::kAssembly, "Assembly");
  register_editor_command(EditorType::kSettings, "Settings");

  // Editor-scoped Music shortcuts (toggle playback, speed controls)
  if (editor_manager) {
    for (const auto& def : kMusicEditorShortcuts) {
      RegisterIfValid(
          shortcut_manager, def.id, def.keys,
          [editor_manager, id = def.id]() {
            if (!editor_manager) return;
            auto* current_editor = editor_manager->GetCurrentEditor();
            if (!current_editor || current_editor->type() != EditorType::kMusic) {
              return;
            }
            auto* editor_set = editor_manager->GetCurrentEditorSet();
            auto* music_editor = editor_set ? editor_set->GetMusicEditor() : nullptr;
            if (!music_editor) return;

            if (id == "music.play_pause") {
              music_editor->TogglePlayPause();
            } else if (id == "music.stop") {
              music_editor->StopPlayback();
            } else if (id == "music.speed_up" || id == "music.speed_up_keypad") {
              music_editor->SpeedUp();
            } else if (id == "music.speed_down" ||
                       id == "music.speed_down_keypad") {
              music_editor->SlowDown();
            }
          },
          Shortcut::Scope::kEditor);
    }
  }

  // Editor-scoped Dungeon shortcuts (object tools)
  if (editor_manager) {
    for (const auto& def : kDungeonEditorShortcuts) {
      RegisterIfValid(
          shortcut_manager, def.id, def.keys,
          [editor_manager, id = def.id]() {
            if (!editor_manager) return;
            auto* current_editor = editor_manager->GetCurrentEditor();
            if (!current_editor ||
                current_editor->type() != EditorType::kDungeon) {
              return;
            }
            auto* editor_set = editor_manager->GetCurrentEditorSet();
            auto* dungeon_editor = editor_set ? editor_set->GetDungeonEditor() : nullptr;
            if (!dungeon_editor) return;
            auto* obj_panel = dungeon_editor->object_editor_panel();
            if (!obj_panel) return;

            if (id == "dungeon.object.select_tool") {
              // Unified mode: cancel placement to switch to selection
              obj_panel->CancelPlacement();
            } else if (id == "dungeon.object.place_tool") {
              // Unified mode: handled by object selector click
              // No-op (mode is controlled by selecting an object)
            } else if (id == "dungeon.object.delete_tool") {
              // Unified mode: delete selected objects
              obj_panel->DeleteSelectedObjects();
            } else if (id == "dungeon.object.next_object") {
              obj_panel->CycleObjectSelection(1);
            } else if (id == "dungeon.object.prev_object") {
              obj_panel->CycleObjectSelection(-1);
            } else if (id == "dungeon.object.copy") {
              obj_panel->CopySelectedObjects();
            } else if (id == "dungeon.object.paste") {
              obj_panel->PasteObjects();
            } else if (id == "dungeon.object.delete") {
              obj_panel->DeleteSelectedObjects();
            }
          },
          Shortcut::Scope::kEditor);
    }
  }

  // Editor-scoped Overworld shortcuts (basic tools)
  if (editor_manager) {
    for (const auto& def : kOverworldShortcuts) {
      RegisterIfValid(
          shortcut_manager, def.id, def.keys,
          [editor_manager, id = def.id]() {
            if (!editor_manager) return;
            auto* current_editor = editor_manager->GetCurrentEditor();
            if (!current_editor ||
                current_editor->type() != EditorType::kOverworld) {
              return;
            }
            auto* editor_set = editor_manager->GetCurrentEditorSet();
            auto* overworld_editor =
                editor_set ? editor_set->GetOverworldEditor() : nullptr;
            if (!overworld_editor) return;

            if (id == "overworld.brush_toggle") {
              overworld_editor->ToggleBrushTool();
            } else if (id == "overworld.fill") {
              overworld_editor->ActivateFillTool();
            } else if (id == "overworld.next_tile") {
              overworld_editor->CycleTileSelection(1);
            } else if (id == "overworld.prev_tile") {
              overworld_editor->CycleTileSelection(-1);
            }
          },
          Shortcut::Scope::kEditor);
    }
  }

  // Editor-scoped Graphics shortcuts (sheet navigation)
  if (editor_manager) {
    for (const auto& def : kGraphicsShortcuts) {
      RegisterIfValid(
          shortcut_manager, def.id, def.keys,
          [editor_manager, id = def.id]() {
            if (!editor_manager) return;
            auto* current_editor = editor_manager->GetCurrentEditor();
            if (!current_editor ||
                current_editor->type() != EditorType::kGraphics) {
              return;
            }
            auto* editor_set = editor_manager->GetCurrentEditorSet();
            auto* graphics_editor =
                editor_set ? editor_set->GetGraphicsEditor() : nullptr;
            if (!graphics_editor) return;

            if (id == "graphics.next_sheet") {
              graphics_editor->NextSheet();
            } else if (id == "graphics.prev_sheet") {
              graphics_editor->PrevSheet();
            }
          },
          Shortcut::Scope::kEditor);
    }
  }

  RegisterIfValid(
      shortcut_manager, "Editor Selection", {ImGuiMod_Ctrl, ImGuiKey_E},
      [ui_coordinator]() {
        if (ui_coordinator) {
          ui_coordinator->ShowEditorSelection();
        }
      },
      Shortcut::Scope::kGlobal);

  RegisterIfValid(
      shortcut_manager, "Panel Browser",
      {ImGuiMod_Ctrl, ImGuiMod_Shift, ImGuiKey_B},
      [ui_coordinator]() {
        if (ui_coordinator) {
          ui_coordinator->ShowPanelBrowser();
        }
      },
      Shortcut::Scope::kGlobal);
  RegisterIfValid(
      shortcut_manager, "Panel Browser (Alt)",
      {ImGuiMod_Ctrl, ImGuiMod_Alt, ImGuiKey_P},
      [ui_coordinator]() {
        if (ui_coordinator) {
          ui_coordinator->ShowPanelBrowser();
        }
      },
      Shortcut::Scope::kGlobal);

  if (panel_manager) {
    // Note: Using Ctrl+Alt for panel shortcuts to avoid conflicts with Save As
    // (Ctrl+Shift+S)
    RegisterIfValid(
        shortcut_manager, "Show Dungeon Panels",
        {ImGuiMod_Ctrl, ImGuiMod_Alt, ImGuiKey_D},
        [panel_manager]() {
          panel_manager->ShowAllPanelsInCategory(0, "Dungeon");
        },
        Shortcut::Scope::kEditor);
    RegisterIfValid(
        shortcut_manager, "Show Graphics Panels",
        {ImGuiMod_Ctrl, ImGuiMod_Alt, ImGuiKey_G},
        [panel_manager]() {
          panel_manager->ShowAllPanelsInCategory(0, "Graphics");
        },
        Shortcut::Scope::kEditor);
    RegisterIfValid(
        shortcut_manager, "Show Screen Panels",
        {ImGuiMod_Ctrl, ImGuiMod_Alt, ImGuiKey_S},
        [panel_manager]() {
          panel_manager->ShowAllPanelsInCategory(0, "Screen");
        },
        Shortcut::Scope::kEditor);
  }

#ifdef YAZE_BUILD_AGENT_UI
  RegisterIfValid(shortcut_manager, "Agent Editor",
                  {ImGuiMod_Ctrl, ImGuiMod_Shift, ImGuiKey_A},
                  [editor_manager]() {
                    if (editor_manager) {
                      editor_manager->ShowAIAgent();
                    }
                  });

  RegisterIfValid(shortcut_manager, "Agent Sidebar",
                  {ImGuiMod_Ctrl, ImGuiKey_H}, [editor_manager]() {
                    if (editor_manager) {
                      editor_manager->ShowChatHistory();
                    }
                  });

  RegisterIfValid(shortcut_manager, "Proposal Drawer",
                  {ImGuiMod_Ctrl, ImGuiMod_Shift,
                   ImGuiKey_R},  // Changed from Ctrl+P to Ctrl+Shift+R
                  [editor_manager]() {
                    if (editor_manager) {
                      editor_manager->ShowProposalDrawer();
                    }
                  });
#endif

  // ============================================================================
  // Layout Presets (command palette only - no keyboard shortcuts)
  // ============================================================================
  shortcut_manager->RegisterCommand(
      "Layout: Apply Minimal Preset", [editor_manager]() {
        if (editor_manager) {
          editor_manager->ApplyLayoutPreset("Minimal");
        }
      });
  shortcut_manager->RegisterCommand(
      "Layout: Apply Developer Preset", [editor_manager]() {
        if (editor_manager) {
          editor_manager->ApplyLayoutPreset("Developer");
        }
      });
  shortcut_manager->RegisterCommand(
      "Layout: Apply Designer Preset", [editor_manager]() {
        if (editor_manager) {
          editor_manager->ApplyLayoutPreset("Designer");
        }
      });
  shortcut_manager->RegisterCommand(
      "Layout: Apply Modder Preset", [editor_manager]() {
        if (editor_manager) {
          editor_manager->ApplyLayoutPreset("Modder");
        }
      });
  shortcut_manager->RegisterCommand(
      "Layout: Apply Overworld Expert Preset", [editor_manager]() {
        if (editor_manager) {
          editor_manager->ApplyLayoutPreset("Overworld Expert");
        }
      });
  shortcut_manager->RegisterCommand(
      "Layout: Apply Dungeon Expert Preset", [editor_manager]() {
        if (editor_manager) {
          editor_manager->ApplyLayoutPreset("Dungeon Expert");
        }
      });
  shortcut_manager->RegisterCommand(
      "Layout: Apply Testing Preset", [editor_manager]() {
        if (editor_manager) {
          editor_manager->ApplyLayoutPreset("Testing");
        }
      });
  shortcut_manager->RegisterCommand(
      "Layout: Apply Audio Preset", [editor_manager]() {
        if (editor_manager) {
          editor_manager->ApplyLayoutPreset("Audio");
        }
      });
  shortcut_manager->RegisterCommand(
      "Layout: Reset Current Editor", [editor_manager]() {
        if (editor_manager) {
          editor_manager->ResetCurrentEditorLayout();
        }
      });

  // ============================================================================
  // Right Panel Toggle Commands (command palette only)
  // ============================================================================
  if (editor_manager) {
    using PanelType = RightPanelManager::PanelType;
    auto* rpm = editor_manager->right_panel_manager();
    if (rpm) {
      struct PanelCmd {
        const char* label;
        PanelType type;
      };
      PanelCmd panel_cmds[] = {
          {"View: Toggle AI Agent Panel", PanelType::kAgentChat},
          {"View: Toggle Proposals Panel", PanelType::kProposals},
          {"View: Toggle Settings Panel", PanelType::kSettings},
          {"View: Toggle Help Panel", PanelType::kHelp},
          {"View: Toggle Notifications", PanelType::kNotifications},
          {"View: Toggle Properties Panel", PanelType::kProperties},
          {"View: Toggle Project Panel", PanelType::kProject},
      };
      for (const auto& cmd : panel_cmds) {
        shortcut_manager->RegisterCommand(
            cmd.label, [rpm, panel_type = cmd.type]() {
              rpm->TogglePanel(panel_type);
            });
      }
    }
  }

  // ============================================================================
  // Panel Visibility Toggle Commands (command palette only)
  // ============================================================================
  if (panel_manager) {
    auto categories = panel_manager->GetAllCategories();
    int session_id = 0;  // Default session for command registration

    for (const auto& category : categories) {
      auto panels = panel_manager->GetPanelsInCategory(session_id, category);

      for (const auto& panel : panels) {
        std::string panel_id = panel.card_id;
        std::string display_name = panel.display_name;

        // Register show command
        shortcut_manager->RegisterCommand(
            absl::StrFormat("View: Show %s", display_name),
            [panel_manager, panel_id]() {
              if (panel_manager) {
                panel_manager->ShowPanel(0, panel_id);
              }
            });

        // Register hide command
        shortcut_manager->RegisterCommand(
            absl::StrFormat("View: Hide %s", display_name),
            [panel_manager, panel_id]() {
              if (panel_manager) {
                panel_manager->HidePanel(0, panel_id);
              }
            });

        // Register toggle command
        shortcut_manager->RegisterCommand(
            absl::StrFormat("View: Toggle %s", display_name),
            [panel_manager, panel_id]() {
              if (panel_manager) {
                panel_manager->TogglePanel(0, panel_id);
              }
            });
      }
    }

    // Category-level commands
    for (const auto& category : categories) {
      shortcut_manager->RegisterCommand(
          absl::StrFormat("View: Show All %s Panels", category),
          [panel_manager, category]() {
            if (panel_manager) {
              panel_manager->ShowAllPanelsInCategory(0, category);
            }
          });

      shortcut_manager->RegisterCommand(
          absl::StrFormat("View: Hide All %s Panels", category),
          [panel_manager, category]() {
            if (panel_manager) {
              panel_manager->HideAllPanelsInCategory(0, category);
            }
          });
    }
  }
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

void ConfigurePanelShortcuts(const ShortcutDependencies& deps,
                             ShortcutManager* shortcut_manager) {
  if (!shortcut_manager || !deps.panel_manager) {
    return;
  }

  auto* panel_manager = deps.panel_manager;
  auto* user_settings = deps.user_settings;
  int session_id = deps.session_coordinator
                       ? deps.session_coordinator->GetActiveSessionIndex()
                       : 0;

  // Get all categories and panels
  auto categories = panel_manager->GetAllCategories();

  for (const auto& category : categories) {
    auto panels = panel_manager->GetPanelsInCategory(session_id, category);

    for (const auto& panel : panels) {
      std::string shortcut_string;

      // Check for user-defined shortcut first
      if (user_settings) {
        auto it = user_settings->prefs().panel_shortcuts.find(panel.card_id);
        if (it != user_settings->prefs().panel_shortcuts.end()) {
          shortcut_string = it->second;
        }
      }

      // Fall back to default shortcut_hint
      if (shortcut_string.empty() && !panel.shortcut_hint.empty()) {
        shortcut_string = panel.shortcut_hint;
      }

      // If we have a shortcut, parse and register it
      if (!shortcut_string.empty()) {
        auto keys = ParseShortcut(shortcut_string);
        if (!keys.empty()) {
          std::string panel_id_copy = panel.card_id;
          // Toggle panel visibility shortcut
          if (panel.shortcut_scope == PanelDescriptor::ShortcutScope::kPanel) {
            std::string toggle_id = "view.toggle." + panel.card_id;
            RegisterIfValid(shortcut_manager, toggle_id, keys,
                            [panel_manager, panel_id_copy, session_id]() {
                              panel_manager->TogglePanel(session_id,
                                                         panel_id_copy);
                            });
          }
        }
      }
    }
  }
}

}  // namespace yaze::editor
