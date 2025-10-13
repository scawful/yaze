#ifndef YAZE_APP_EDITOR_CODE_PROJECT_FILE_EDITOR_H_
#define YAZE_APP_EDITOR_CODE_PROJECT_FILE_EDITOR_H_

#include <string>

#include "absl/status/status.h"
#include "app/core/project.h"
#include "app/gui/widgets/text_editor.h"

namespace yaze {
namespace editor {

class ToastManager;

/**
 * @class ProjectFileEditor
 * @brief Editor for .yaze project files with syntax highlighting and validation
 * 
 * Provides a rich text editing experience for yaze project files with:
 * - Syntax highlighting for INI-style format
 * - Real-time validation
 * - Auto-save capability
 * - Integration with core::YazeProject
 */
class ProjectFileEditor {
 public:
  ProjectFileEditor();
  
  void Draw();
  
  /**
   * @brief Load a project file into the editor
   */
  absl::Status LoadFile(const std::string& filepath);
  
  /**
   * @brief Save the current editor contents to disk
   */
  absl::Status SaveFile();
  
  /**
   * @brief Save to a new file path
   */
  absl::Status SaveFileAs(const std::string& filepath);
  
  /**
   * @brief Get whether the file has unsaved changes
   */
  bool IsModified() const { return text_editor_.IsTextChanged() || modified_; }
  
  /**
   * @brief Get the current filepath
   */
  const std::string& filepath() const { return filepath_; }
  
  /**
   * @brief Set whether the editor window is active
   */
  void set_active(bool active) { active_ = active; }
  
  /**
   * @brief Get pointer to active state for ImGui
   */
  bool* active() { return &active_; }
  
  /**
   * @brief Set toast manager for notifications
   */
  void SetToastManager(ToastManager* toast_manager) {
    toast_manager_ = toast_manager;
  }
  
  /**
   * @brief Create a new empty project file
   */
  void NewFile();
  
 private:
  void ApplySyntaxHighlighting();
  void ValidateContent();
  void ShowValidationErrors();
  
  TextEditor text_editor_;
  std::string filepath_;
  bool active_ = false;
  bool modified_ = false;
  bool show_validation_ = true;
  std::vector<std::string> validation_errors_;
  ToastManager* toast_manager_ = nullptr;
};

}  // namespace editor
}  // namespace yaze

#endif  // YAZE_APP_EDITOR_CODE_PROJECT_FILE_EDITOR_H_
