#ifndef YAZE_APP_EDITOR_SYSTEM_EDITOR_REGISTRY_H_
#define YAZE_APP_EDITOR_SYSTEM_EDITOR_REGISTRY_H_

#include <string>
#include <unordered_map>
#include <vector>

#include "app/editor/editor.h"

namespace yaze {
namespace editor {

/**
 * @class EditorRegistry
 * @brief Manages editor types, categories, and lifecycle
 *
 * Extracted from EditorManager to provide focused editor management:
 * - Editor type classification and categorization
 * - Editor activation and switching
 * - Editor-specific navigation (jump to room/map)
 * - Editor card management
 */
class EditorRegistry {
 public:
  EditorRegistry() = default;
  ~EditorRegistry() = default;

  // Editor type management (static methods for global access)
  static bool IsPanelBasedEditor(EditorType type);
  static std::string GetEditorCategory(EditorType type);
  static EditorType GetEditorTypeFromCategory(const std::string& category);

  /**
   * @brief Get all editor categories in display order for sidebar
   * @return Vector of category names in preferred display order
   */
  static std::vector<std::string> GetAllEditorCategories();

  // Editor navigation
  // DEPRECATED: Use EventBus with JumpToRoomRequestEvent/JumpToMapRequestEvent instead
  [[deprecated("Use EventBus::Publish(JumpToRoomRequestEvent::Create(room_id))")]]
  void JumpToDungeonRoom(int room_id);
  [[deprecated("Use EventBus::Publish(JumpToMapRequestEvent::Create(map_id))")]]
  void JumpToOverworldMap(int map_id);
  void SwitchToEditor(EditorType editor_type);

  // DEPRECATED: Navigation callbacks replaced by EventBus subscriptions
  [[deprecated("EditorActivator now subscribes to JumpToRoomRequestEvent")]]
  void SetJumpToDungeonRoomCallback(std::function<void(int)> callback) {
    jump_to_room_callback_ = std::move(callback);
  }
  [[deprecated("EditorActivator now subscribes to JumpToMapRequestEvent")]]
  void SetJumpToOverworldMapCallback(std::function<void(int)> callback) {
    jump_to_map_callback_ = std::move(callback);
  }

  // Editor card management
  void HideCurrentEditorPanels();
  void ShowEditorPanels(EditorType editor_type);
  void ToggleEditorPanels(EditorType editor_type);

  // Editor information
  std::vector<EditorType> GetEditorsInCategory(
      const std::string& category) const;
  std::vector<std::string> GetAvailableCategories() const;
  std::string GetEditorDisplayName(EditorType type) const;

  // Editor lifecycle
  void RegisterEditor(EditorType type, Editor* editor);
  void UnregisterEditor(EditorType type);
  Editor* GetEditor(EditorType type) const;

  // Editor state queries
  bool IsEditorActive(EditorType type) const;
  bool IsEditorVisible(EditorType type) const;
  void SetEditorActive(EditorType type, bool active);

 private:
  // Editor type mappings
  static const std::unordered_map<EditorType, std::string> kEditorCategories;
  static const std::unordered_map<EditorType, std::string> kEditorNames;
  static const std::unordered_map<EditorType, bool> kPanelBasedEditors;

  // Registered editors
  std::unordered_map<EditorType, Editor*> registered_editors_;

  // Navigation callbacks
  std::function<void(int)> jump_to_room_callback_;
  std::function<void(int)> jump_to_map_callback_;

  // Helper methods
  bool IsValidEditorType(EditorType type) const;
  void ValidateEditorType(EditorType type) const;
};

}  // namespace editor
}  // namespace yaze

#endif  // YAZE_APP_EDITOR_SYSTEM_EDITOR_REGISTRY_H_
