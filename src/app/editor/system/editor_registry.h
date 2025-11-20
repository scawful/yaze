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
  static bool IsCardBasedEditor(EditorType type);
  static std::string GetEditorCategory(EditorType type);
  static EditorType GetEditorTypeFromCategory(const std::string& category);

  // Editor navigation
  void JumpToDungeonRoom(int room_id);
  void JumpToOverworldMap(int map_id);
  void SwitchToEditor(EditorType editor_type);

  // Editor card management
  void HideCurrentEditorCards();
  void ShowEditorCards(EditorType editor_type);
  void ToggleEditorCards(EditorType editor_type);

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
  static const std::unordered_map<EditorType, bool> kCardBasedEditors;

  // Registered editors
  std::unordered_map<EditorType, Editor*> registered_editors_;

  // Helper methods
  bool IsValidEditorType(EditorType type) const;
  void ValidateEditorType(EditorType type) const;
};

}  // namespace editor
}  // namespace yaze

#endif  // YAZE_APP_EDITOR_SYSTEM_EDITOR_REGISTRY_H_
