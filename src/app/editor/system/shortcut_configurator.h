#ifndef YAZE_APP_EDITOR_SYSTEM_SHORTCUT_CONFIGURATOR_H_
#define YAZE_APP_EDITOR_SYSTEM_SHORTCUT_CONFIGURATOR_H_

#include <functional>

#include "app/editor/editor.h"
#include "app/editor/system/shortcut_manager.h"

namespace yaze {
namespace editor {

class EditorManager;
class EditorRegistry;
class MenuOrchestrator;
class RomFileManager;
class ProjectManager;
class SessionCoordinator;
class UICoordinator;
class WorkspaceManager;
class PopupManager;
class ToastManager;
class EditorCardRegistry;

// Forward declaration
class UserSettings;

struct ShortcutDependencies {
  EditorManager* editor_manager = nullptr;
  EditorRegistry* editor_registry = nullptr;
  MenuOrchestrator* menu_orchestrator = nullptr;
  RomFileManager* rom_file_manager = nullptr;
  ProjectManager* project_manager = nullptr;
  SessionCoordinator* session_coordinator = nullptr;
  UICoordinator* ui_coordinator = nullptr;
  WorkspaceManager* workspace_manager = nullptr;
  PopupManager* popup_manager = nullptr;
  ToastManager* toast_manager = nullptr;
  EditorCardRegistry* card_registry = nullptr;
  UserSettings* user_settings = nullptr;
};

void ConfigureEditorShortcuts(const ShortcutDependencies& deps,
                              ShortcutManager* shortcut_manager);

void ConfigureMenuShortcuts(const ShortcutDependencies& deps,
                            ShortcutManager* shortcut_manager);

/**
 * @brief Register configurable card shortcuts from user settings
 * @param deps Shortcut dependencies
 * @param shortcut_manager The shortcut manager to register with
 *
 * This function reads card shortcuts from UserSettings and registers them
 * with the shortcut manager. It falls back to CardInfo.shortcut_hint if
 * no custom shortcut is defined for a card.
 */
void ConfigureCardShortcuts(const ShortcutDependencies& deps,
                            ShortcutManager* shortcut_manager);

}  // namespace editor
}  // namespace yaze

#endif  // YAZE_APP_EDITOR_SYSTEM_SHORTCUT_CONFIGURATOR_H_
