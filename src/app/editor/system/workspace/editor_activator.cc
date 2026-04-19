#include "editor_activator.h"

#include "app/editor/code/assembly_editor.h"
#include "app/editor/dungeon/dungeon_editor_v2.h"
#include "app/editor/events/core_events.h"
#include "app/editor/layout/layout_manager.h"
#include "app/editor/menu/right_drawer_manager.h"
#include "app/editor/message/message_editor.h"
#include "app/editor/overworld/overworld_editor.h"
#include "app/editor/session_types.h"
#include "app/editor/shell/coordinator/ui_coordinator.h"
#include "app/editor/shell/feedback/toast_manager.h"
#include "app/editor/system/workspace/editor_registry.h"
#include "app/editor/system/workspace/workspace_window_manager.h"
#include "app/gui/animation/animator.h"
#include "app/gui/core/icons.h"
#include "imgui/imgui.h"
#include "imgui/imgui_internal.h"
#include "util/log.h"

namespace yaze {
namespace editor {

void EditorActivator::Initialize(const Dependencies& deps) {
  deps_ = deps;
  initialized_ = true;

  // Subscribe to navigation events via EventBus
  if (deps_.event_bus) {
    deps_.event_bus->Subscribe<JumpToRoomRequestEvent>(
        [this](const JumpToRoomRequestEvent& event) {
          JumpToDungeonRoom(event.room_id);
        });

    deps_.event_bus->Subscribe<JumpToMapRequestEvent>(
        [this](const JumpToMapRequestEvent& event) {
          JumpToOverworldMap(event.map_id);
        });

    deps_.event_bus->Subscribe<JumpToMessageRequestEvent>(
        [this](const JumpToMessageRequestEvent& event) {
          JumpToMessage(event.message_id);
        });

    deps_.event_bus->Subscribe<JumpToAssemblySymbolRequestEvent>(
        [this](const JumpToAssemblySymbolRequestEvent& event) {
          JumpToAssemblySymbol(event.symbol);
        });

    LOG_INFO("EditorActivator", "Subscribed to navigation events");
  }
}

void EditorActivator::SwitchToEditor(EditorType editor_type, bool force_visible,
                                     bool from_dialog) {
  if (!initialized_) {
    LOG_WARN("EditorActivator", "Not initialized, cannot switch editor");
    return;
  }

  // Avoid touching ImGui docking state when outside a frame
  ImGuiContext* imgui_ctx = ImGui::GetCurrentContext();
  const bool frame_active = imgui_ctx != nullptr && imgui_ctx->WithinFrameScope;
  if (!frame_active && deps_.queue_deferred_action) {
    deps_.queue_deferred_action(
        [this, editor_type, force_visible, from_dialog]() {
          SwitchToEditor(editor_type, force_visible, from_dialog);
        });
    return;
  }

  // If NOT coming from dialog, close editor selection UI
  if (!from_dialog && deps_.ui_coordinator) {
    deps_.ui_coordinator->SetEditorSelectionVisible(false);
  }

  auto* editor_set =
      deps_.get_current_editor_set ? deps_.get_current_editor_set() : nullptr;
  if (!editor_set) {
    return;
  }

  if (deps_.ensure_editor_assets_loaded) {
    switch (editor_type) {
      case EditorType::kAssembly:
      case EditorType::kDungeon:
      case EditorType::kGraphics:
      case EditorType::kMessage:
      case EditorType::kMusic:
      case EditorType::kOverworld:
      case EditorType::kPalette:
      case EditorType::kScreen:
      case EditorType::kSprite: {
        const auto status = deps_.ensure_editor_assets_loaded(editor_type);
        if (!status.ok()) {
          if (deps_.toast_manager) {
            deps_.toast_manager->Show(
                "Failed to prepare editor: " + std::string(status.message()),
                ToastType::kError);
          }
          return;
        }
        break;
      }
      default:
        break;
    }
  }

  // Toggle the editor in the active editors list
  for (auto* editor : editor_set->active_editors_) {
    if (editor->type() == editor_type) {
      if (force_visible) {
        editor->set_active(true);
      } else {
        editor->toggle_active();
      }

      if (EditorRegistry::IsPanelBasedEditor(editor_type)) {
        if (*editor->active()) {
          // Smooth transition: fade out old, then fade in new
          // Panel refresh handled by OnEditorSwitch below
          ActivatePanelBasedEditor(editor_type, editor);
        } else {
          DeactivatePanelBasedEditor(editor_type, editor, editor_set);
        }
      }
      return;
    }
  }

  // Handle non-editor-class cases (Assembly, Emulator, Hex, Settings, Agent)
  HandleNonEditorClassSwitch(editor_type, force_visible);
}

void EditorActivator::ActivatePanelBasedEditor(EditorType type,
                                               Editor* editor) {
  if (!deps_.window_manager)
    return;

  std::string old_category = deps_.window_manager->GetActiveCategory();
  std::string new_category = EditorRegistry::GetEditorCategory(type);

  // Only trigger OnEditorSwitch if category actually changes
  if (old_category != new_category) {
    // Kill any in-flight panel transitions from the previous editor so the
    // new editor's windows start their fade from a clean state. Without this,
    // stale alpha values leave old bitmap surfaces ghosted during the switch.
    gui::GetAnimator().ClearWorkspaceTransitionState();
    deps_.window_manager->OnEditorSwitch(old_category, new_category);
  }

  // Initialize default layout on first activation
  if (deps_.layout_manager &&
      !deps_.layout_manager->IsLayoutInitialized(type)) {
    if (deps_.queue_deferred_action) {
      deps_.queue_deferred_action([this, type]() {
        if (deps_.layout_manager &&
            !deps_.layout_manager->IsLayoutInitialized(type)) {
          ImGuiID dockspace_id = ImGui::GetID("MainDockSpace");
          deps_.layout_manager->InitializeEditorLayout(type, dockspace_id);
        }
      });
    }
  }
}

void EditorActivator::DeactivatePanelBasedEditor(EditorType type,
                                                 Editor* editor,
                                                 EditorSet* editor_set) {
  if (!deps_.window_manager || !editor_set)
    return;

  // Switch to another active panel-based editor
  for (auto* other : editor_set->active_editors_) {
    if (*other->active() && EditorRegistry::IsPanelBasedEditor(other->type()) &&
        other != editor) {
      std::string old_category = deps_.window_manager->GetActiveCategory();
      std::string new_category =
          EditorRegistry::GetEditorCategory(other->type());
      if (old_category != new_category) {
        gui::GetAnimator().ClearWorkspaceTransitionState();
        deps_.window_manager->OnEditorSwitch(old_category, new_category);
      }
      break;
    }
  }
}

void EditorActivator::HandleNonEditorClassSwitch(EditorType type,
                                                 bool force_visible) {
  switch (type) {
    case EditorType::kAssembly:
      if (deps_.ui_coordinator) {
        bool is_visible = !deps_.ui_coordinator->IsAsmEditorVisible();
        if (force_visible) {
          is_visible = true;
        }

        deps_.ui_coordinator->SetAsmEditorVisible(is_visible);

        if (is_visible && deps_.window_manager) {
          deps_.window_manager->SetActiveCategory("Assembly");
          InitializeEditorLayout(EditorType::kAssembly);
        }
      }
      break;

    case EditorType::kEmulator:
      if (deps_.ui_coordinator) {
        bool is_visible = !deps_.ui_coordinator->IsEmulatorVisible();
        if (force_visible)
          is_visible = true;

        deps_.ui_coordinator->SetEmulatorVisible(is_visible);

        if (is_visible && deps_.window_manager) {
          deps_.window_manager->SetActiveCategory("Emulator");

          if (deps_.queue_deferred_action) {
            deps_.queue_deferred_action([this]() {
              ImGuiContext* ctx = ImGui::GetCurrentContext();
              if (deps_.layout_manager && ctx && ctx->WithinFrameScope) {
                ImGuiID dockspace_id = ImGui::GetID("MainDockSpace");
                if (!deps_.layout_manager->IsLayoutInitialized(
                        EditorType::kEmulator)) {
                  deps_.layout_manager->InitializeEditorLayout(
                      EditorType::kEmulator, dockspace_id);
                  LOG_INFO("EditorActivator", "Initialized emulator layout");
                }
              }
            });
          }
        }
      }
      break;

    case EditorType::kHex:
      if (deps_.window_manager && deps_.get_current_session_id) {
        deps_.window_manager->OpenWindow(deps_.get_current_session_id(),
                                         "Hex Editor");
      }
      break;

    case EditorType::kSettings:
      if (deps_.right_drawer_manager) {
        if (deps_.right_drawer_manager->GetActiveDrawer() ==
                RightDrawerManager::DrawerType::kSettings &&
            !force_visible) {
          deps_.right_drawer_manager->CloseDrawer();
        } else {
          deps_.right_drawer_manager->OpenDrawer(
              RightDrawerManager::DrawerType::kSettings);
        }
      }
      break;

    default:
      // Other editor types not handled here
      break;
  }
}

void EditorActivator::InitializeEditorLayout(EditorType type) {
  if (!initialized_ || !deps_.layout_manager)
    return;

  ImGuiContext* ctx = ImGui::GetCurrentContext();
  if (!ctx || !ctx->WithinFrameScope) {
    if (deps_.queue_deferred_action) {
      deps_.queue_deferred_action(
          [this, type]() { InitializeEditorLayout(type); });
    }
    return;
  }

  if (!deps_.layout_manager->IsLayoutInitialized(type)) {
    ImGuiID dockspace_id = ImGui::GetID("MainDockSpace");
    deps_.layout_manager->InitializeEditorLayout(type, dockspace_id);
    LOG_INFO("EditorActivator", "Initialized layout for editor type %d",
             static_cast<int>(type));
  }
}

void EditorActivator::JumpToDungeonRoom(int room_id) {
  auto* editor_set =
      deps_.get_current_editor_set ? deps_.get_current_editor_set() : nullptr;
  if (!editor_set)
    return;

  if (deps_.ensure_editor_assets_loaded) {
    const auto status = deps_.ensure_editor_assets_loaded(EditorType::kDungeon);
    if (!status.ok()) {
      if (deps_.toast_manager) {
        deps_.toast_manager->Show("Failed to prepare Dungeon editor: " +
                                      std::string(status.message()),
                                  ToastType::kError);
      }
      return;
    }
  }

  // Switch to dungeon editor
  SwitchToEditor(EditorType::kDungeon);

  // Open the room in the dungeon editor
  editor_set->GetDungeonEditor()->add_room(room_id);
}

void EditorActivator::JumpToOverworldMap(int map_id) {
  auto* editor_set =
      deps_.get_current_editor_set ? deps_.get_current_editor_set() : nullptr;
  if (!editor_set)
    return;

  if (deps_.ensure_editor_assets_loaded) {
    const auto status =
        deps_.ensure_editor_assets_loaded(EditorType::kOverworld);
    if (!status.ok()) {
      if (deps_.toast_manager) {
        deps_.toast_manager->Show("Failed to prepare Overworld editor: " +
                                      std::string(status.message()),
                                  ToastType::kError);
      }
      return;
    }
  }

  // Switch to overworld editor
  SwitchToEditor(EditorType::kOverworld);

  // Set the current map in the overworld editor
  editor_set->GetOverworldEditor()->set_current_map(map_id);
}

void EditorActivator::JumpToMessage(int message_id) {
  if (message_id < 0)
    return;

  auto* editor_set =
      deps_.get_current_editor_set ? deps_.get_current_editor_set() : nullptr;
  if (!editor_set)
    return;

  if (deps_.ensure_editor_assets_loaded) {
    const auto status = deps_.ensure_editor_assets_loaded(EditorType::kMessage);
    if (!status.ok()) {
      if (deps_.toast_manager) {
        deps_.toast_manager->Show("Failed to prepare Message editor: " +
                                      std::string(status.message()),
                                  ToastType::kError);
      }
      return;
    }
  }

  // Switch to message editor (force visible so this behaves like navigation).
  SwitchToEditor(EditorType::kMessage, /*force_visible=*/true);

  if (auto* message_editor = editor_set->GetMessageEditor()) {
    if (!message_editor->OpenMessageById(message_id)) {
      if (deps_.toast_manager) {
        deps_.toast_manager->Show(
            "Message ID not found: " + std::to_string(message_id),
            ToastType::kWarning);
      }
    }
  }
}

void EditorActivator::JumpToAssemblySymbol(const std::string& symbol) {
  if (symbol.empty())
    return;

  auto* editor_set =
      deps_.get_current_editor_set ? deps_.get_current_editor_set() : nullptr;
  if (!editor_set)
    return;

  if (deps_.ensure_editor_assets_loaded) {
    const auto status =
        deps_.ensure_editor_assets_loaded(EditorType::kAssembly);
    if (!status.ok()) {
      if (deps_.toast_manager) {
        deps_.toast_manager->Show("Failed to prepare Assembly editor: " +
                                      std::string(status.message()),
                                  ToastType::kError);
      }
      return;
    }
  }

  // Switch to assembly editor (force visible so this behaves like navigation).
  SwitchToEditor(EditorType::kAssembly, /*force_visible=*/true);

  if (auto* asm_editor = editor_set->GetAssemblyEditor()) {
    const auto status = asm_editor->JumpToReference(symbol);
    if (!status.ok()) {
      if (deps_.toast_manager) {
        deps_.toast_manager->Show(
            "Assembly jump failed: " + std::string(status.message()),
            ToastType::kWarning);
      }
      return;
    }
  }
}

}  // namespace editor
}  // namespace yaze
