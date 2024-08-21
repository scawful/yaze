#ifndef YAZE_APP_EDITOR_ASSEMBLY_EDITOR_H
#define YAZE_APP_EDITOR_ASSEMBLY_EDITOR_H

#include <fstream>
#include <istream>
#include <string>

#include "ImGuiColorTextEdit/TextEditor.h"
#include "ImGuiFileDialog/ImGuiFileDialog.h"
#include "app/core/common.h"
#include "app/editor/utils/editor.h"
#include "app/gui/style.h"

namespace yaze {
namespace app {
namespace editor {

/**
 * @class AssemblyEditor
 * @brief Text editor for modifying assembly code.
 */
class AssemblyEditor : public Editor {
 public:
  AssemblyEditor() {
    text_editor_.SetLanguageDefinition(gui::GetAssemblyLanguageDef());
    text_editor_.SetPalette(TextEditor::GetDarkPalette());
    text_editor_.SetShowWhitespaces(false);
    type_ = EditorType::kAssembly;
  }
  void ChangeActiveFile(const std::string_view &filename) {
    current_file_ = filename;
    file_is_loaded_ = false;
  }

  void Update(bool &is_loaded);
  void InlineUpdate();

  void UpdateCodeView();

  absl::Status Cut() override;
  absl::Status Copy() override;
  absl::Status Paste() override;

  absl::Status Undo() override;
  absl::Status Redo() override;
  absl::Status Find() override { return absl::UnimplementedError("Find"); }

  absl::Status Update() override;

  void OpenFolder(const std::string &folder_path);

 private:
  void DrawFileMenu();
  void DrawEditMenu();

  void SetEditorText();

  void DrawCurrentFolder();

  void DrawFileTabView();

  bool file_is_loaded_ = false;

  std::vector<std::string> files_;
  std::vector<TextEditor> open_files_;
  ImVector<int> active_files_;
  int current_file_id_ = 0;

  std::string current_file_;
  core::FolderItem current_folder_;
  TextEditor text_editor_;
};

}  // namespace editor
}  // namespace app
}  // namespace yaze

#endif
