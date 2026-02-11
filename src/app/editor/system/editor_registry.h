#ifndef YAZE_APP_EDITOR_SYSTEM_EDITOR_REGISTRY_H_
#define YAZE_APP_EDITOR_SYSTEM_EDITOR_REGISTRY_H_

#include <functional>
#include <memory>
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
 * - Editor registration + factories (EditorSet/session creation)
 * - Lightweight editor state queries (active/visible)
 */
class EditorRegistry {
 public:
  using EditorFactory = std::function<std::unique_ptr<Editor>(Rom*)>;

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

  // Editor information
  std::vector<EditorType> GetEditorsInCategory(
      const std::string& category) const;
  std::vector<std::string> GetAvailableCategories() const;
  std::string GetEditorDisplayName(EditorType type) const;

  // Editor lifecycle
  void RegisterEditor(EditorType type, Editor* editor);
  void UnregisterEditor(EditorType type);
  Editor* GetEditor(EditorType type) const;

  // Factory lifecycle (Phase 3: registry-based instantiation)
  void RegisterFactory(EditorType type, EditorFactory factory);
  void UnregisterFactory(EditorType type);
  bool HasFactory(EditorType type) const;
  std::unique_ptr<Editor> CreateEditor(EditorType type, Rom* rom) const;

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

  // Editor factories (for EditorSet/session creation).
  std::unordered_map<EditorType, EditorFactory> editor_factories_;


  // Helper methods
  bool IsValidEditorType(EditorType type) const;
  void ValidateEditorType(EditorType type) const;
};

}  // namespace editor
}  // namespace yaze

#endif  // YAZE_APP_EDITOR_SYSTEM_EDITOR_REGISTRY_H_
