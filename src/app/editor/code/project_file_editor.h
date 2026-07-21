#ifndef YAZE_APP_EDITOR_CODE_PROJECT_FILE_EDITOR_H_
#define YAZE_APP_EDITOR_CODE_PROJECT_FILE_EDITOR_H_

#include <functional>
#include <string>
#include <vector>

#include "absl/status/status.h"
#include "app/gui/widgets/text_editor.h"
#include "core/project.h"

namespace yaze {
namespace editor {

class ToastManager;

struct ProjectFileEditorState {
  std::string filepath;
  std::string text;
  bool initialized = false;
  bool active = false;
  bool modified = false;
  bool show_validation = true;
  std::vector<std::string> validation_errors;
};

/**
 * @class ProjectFileEditor
 * @brief Editor for .yaze project files with syntax highlighting and validation
 *
 * Provides a rich text editing experience for yaze project files with:
 * - Syntax highlighting for INI-style format
 * - Real-time validation
 * - Auto-save capability
 * - Integration with project::YazeProject
 */
class ProjectFileEditor {
 public:
  using SaveGuardCallback = std::function<absl::Status(
      const std::string& filepath, const std::string& contents)>;
  using SaveCompleteCallback = std::function<absl::Status(
      const std::string& filepath, const std::string& contents)>;

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
  bool IsModified() const { return modified_; }

  /**
   * @brief Get the current filepath
   */
  const std::string& filepath() const { return filepath_; }
  bool IsInitialized() const { return initialized_; }

  ProjectFileEditorState CaptureState() const;
  void RestoreState(const ProjectFileEditorState& state,
                    project::YazeProject* project);
  void ResetForProject(project::YazeProject* project);

  /**
   * @brief Set whether the editor window is active
   */
  void set_active(bool active) { active_ = active; }

  /**
   * @brief Get pointer to active state for ImGui
   */
  bool* active() { return &active_; }
  bool is_active() const { return active_; }

  /**
   * @brief Set toast manager for notifications
   */
  void SetToastManager(ToastManager* toast_manager) {
    toast_manager_ = toast_manager;
  }
  void SetSaveGuardCallback(SaveGuardCallback callback) {
    save_guard_callback_ = std::move(callback);
  }
  void SetSaveCompleteCallback(SaveCompleteCallback callback) {
    save_complete_callback_ = std::move(callback);
  }

  // New/Open must never discard a modified draft implicitly.
  absl::Status CanReplaceDocument() const;

  /**
   * @brief Create a new empty project file
   */
  absl::Status NewFile();

  /**
   * @brief Set the project pointer for label import operations
   */
  void SetProject(project::YazeProject* project) { project_ = project; }

 private:
  /**
   * @brief Import labels from a ZScream DefaultNames.txt file
   */
  absl::Status ImportLabelsFromZScream();

  void ApplySyntaxHighlighting();
  void ValidateContent();
  void ShowValidationErrors();
  std::string GetDocumentText() const;

  TextEditor text_editor_;
  std::string filepath_;
  bool initialized_ = false;
  bool active_ = false;
  bool modified_ = false;
  bool show_validation_ = true;
  std::vector<std::string> validation_errors_;
  ToastManager* toast_manager_ = nullptr;
  project::YazeProject* project_ = nullptr;
  SaveGuardCallback save_guard_callback_;
  SaveCompleteCallback save_complete_callback_;
};

}  // namespace editor
}  // namespace yaze

#endif  // YAZE_APP_EDITOR_CODE_PROJECT_FILE_EDITOR_H_
