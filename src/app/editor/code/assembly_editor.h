#ifndef YAZE_APP_EDITOR_ASSEMBLY_EDITOR_H
#define YAZE_APP_EDITOR_ASSEMBLY_EDITOR_H

#include <string>

#include "app/editor/editor.h"
#include "app/gui/modules/text_editor.h"
#include "app/gui/style.h"

namespace yaze {
namespace editor {

struct FolderItem {
  std::string name;
  std::vector<FolderItem> subfolders;
  std::vector<std::string> files;
};

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

  void Initialize() override;
  absl::Status Load() override;
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

  absl::Status Save() override { return absl::UnimplementedError("Save"); }

  void OpenFolder(const std::string &folder_path);

 private:
  void DrawFileMenu();
  void DrawEditMenu();
  void SetEditorText();
  void DrawCurrentFolder();
  void DrawFileTabView();

  bool file_is_loaded_ = false;
  int current_file_id_ = 0;

  std::vector<std::string> files_;
  std::vector<TextEditor> open_files_;
  ImVector<int> active_files_;

  std::string current_file_;
  FolderItem current_folder_;
  TextEditor text_editor_;
};

}  // namespace editor
}  // namespace yaze

#endif
