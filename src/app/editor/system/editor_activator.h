#ifndef YAZE_APP_EDITOR_SYSTEM_EDITOR_ACTIVATOR_H_
#define YAZE_APP_EDITOR_SYSTEM_EDITOR_ACTIVATOR_H_

#include <functional>
#include <string>

#include "absl/status/status.h"
#include "app/editor/core/event_bus.h"
#include "app/editor/editor.h"

namespace yaze {
namespace editor {

// Forward declarations
class EditorSet;
class PanelManager;
class LayoutManager;
class UICoordinator;
class RightPanelManager;
class ToastManager;

/**
 * @class EditorActivator
 * @brief Handles editor switching, layout initialization, and jump-to navigation
 *
 * Extracted from EditorManager to reduce cognitive complexity.
 * Centralizes editor activation logic:
 * - SwitchToEditor: Toggle/activate editors with panel management
 * - InitializeEditorLayout: Set up DockBuilder layouts for editors
 * - JumpToDungeonRoom/JumpToOverworldMap: Cross-editor navigation
 */
class EditorActivator {
 public:
  struct Dependencies {
    PanelManager* panel_manager = nullptr;
    LayoutManager* layout_manager = nullptr;
    UICoordinator* ui_coordinator = nullptr;
    RightPanelManager* right_panel_manager = nullptr;
    ToastManager* toast_manager = nullptr;
    EventBus* event_bus = nullptr;  // For navigation event subscriptions
    std::function<absl::Status(EditorType)> ensure_editor_assets_loaded;
    std::function<EditorSet*()> get_current_editor_set;
    std::function<size_t()> get_current_session_id;
    std::function<void(std::function<void()>)> queue_deferred_action;
  };

  EditorActivator() = default;
  ~EditorActivator() = default;

  void Initialize(const Dependencies& deps);

  /**
   * @brief Switch to an editor, optionally forcing visibility
   * @param type The editor type to switch to
   * @param force_visible If true, always make editor visible
   * @param from_dialog If true, keep editor selection dialog open
   */
  void SwitchToEditor(EditorType type, bool force_visible = false,
                      bool from_dialog = false);

  /**
   * @brief Initialize the DockBuilder layout for an editor
   * @param type The editor type to initialize layout for
   */
  void InitializeEditorLayout(EditorType type);

  /**
   * @brief Jump to a specific dungeon room
   * @param room_id The ID of the dungeon room to jump to
   */
  void JumpToDungeonRoom(int room_id);

  /**
   * @brief Jump to a specific overworld map
   * @param map_id The ID of the overworld map to jump to
   */
  void JumpToOverworldMap(int map_id);

  /**
   * @brief Jump to a specific message ID in the Message editor.
   * @param message_id Vanilla or expanded message ID.
   */
  void JumpToMessage(int message_id);

 private:
  void ActivatePanelBasedEditor(EditorType type, Editor* editor);
  void DeactivatePanelBasedEditor(EditorType type, Editor* editor,
                                  EditorSet* editor_set);
  void HandleNonEditorClassSwitch(EditorType type, bool force_visible);

  Dependencies deps_;
  bool initialized_ = false;
};

}  // namespace editor
}  // namespace yaze

#endif  // YAZE_APP_EDITOR_SYSTEM_EDITOR_ACTIVATOR_H_
