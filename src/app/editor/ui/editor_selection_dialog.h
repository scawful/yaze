#ifndef YAZE_APP_EDITOR_UI_EDITOR_SELECTION_DIALOG_H_
#define YAZE_APP_EDITOR_UI_EDITOR_SELECTION_DIALOG_H_

#include <functional>
#include <string>
#include <vector>

#include "app/editor/editor.h"
#include "imgui/imgui.h"

namespace yaze {
namespace editor {

/**
 * @struct EditorInfo
 * @brief Metadata about an available editor
 */
struct EditorInfo {
  EditorType type;
  const char* name;
  const char* icon;
  const char* description;
  const char* shortcut;
  bool recently_used = false;
  bool requires_rom = true;
  ImVec4 color = ImVec4(0.5f, 0.5f, 0.5f, 1.0f);  // Theme color for this editor
};

/**
 * @class EditorSelectionDialog
 * @brief Beautiful grid-based editor selection dialog
 *
 * Displays when a ROM is loaded, showing all available editors
 * with icons, descriptions, and quick access.
 */
class EditorSelectionDialog {
 public:
  EditorSelectionDialog();

  /**
   * @brief Show the dialog
   * @return True if an editor was selected
   */
  bool Show(bool* p_open = nullptr);

  /**
   * @brief Get the selected editor type
   */
  EditorType GetSelectedEditor() const { return selected_editor_; }

  /**
   * @brief Check if dialog is open
   */
  bool IsOpen() const { return is_open_; }

  /**
   * @brief Open the dialog
   */
  void Open() { is_open_ = true; }

  /**
   * @brief Close the dialog
   */
  void Close() { is_open_ = false; }

  /**
   * @brief Set callback for when editor is selected
   */
  void SetSelectionCallback(std::function<void(EditorType)> callback) {
    selection_callback_ = callback;
  }

  /**
   * @brief Mark an editor as recently used
   */
  void MarkRecentlyUsed(EditorType type);

  /**
   * @brief Load recently used editors from settings
   */
  void LoadRecentEditors();

  /**
   * @brief Save recently used editors to settings
   */
  void SaveRecentEditors();

  /**
   * @brief Clear recent editors (for new ROM sessions)
   */
  void ClearRecentEditors() {
    recent_editors_.clear();
    SaveRecentEditors();
  }

 private:
  void DrawEditorCard(const EditorInfo& info, int index);
  void DrawWelcomeHeader();
  void DrawQuickAccessButtons();

  std::vector<EditorInfo> editors_;
  EditorType selected_editor_ = static_cast<EditorType>(0);
  bool is_open_ = false;
  std::function<void(EditorType)> selection_callback_;

  // Recently used tracking
  std::vector<EditorType> recent_editors_;
  static constexpr int kMaxRecentEditors = 5;
};

}  // namespace editor
}  // namespace yaze

#endif  // YAZE_APP_EDITOR_UI_EDITOR_SELECTION_DIALOG_H_
