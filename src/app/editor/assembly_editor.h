#ifndef YAZE_APP_EDITOR_ASSEMBLY_EDITOR_H
#define YAZE_APP_EDITOR_ASSEMBLY_EDITOR_H

#include <ImGuiColorTextEdit/TextEditor.h>
#include <ImGuiFileDialog/ImGuiFileDialog.h>

#include <fstream>
#include <istream>
#include <string>

namespace yaze {
namespace app {
namespace editor {

class AssemblyEditor {
 public:
  AssemblyEditor() = default;

  void Update();
  void ChangeActiveFile(const std::string &);

 private:
  void SetEditorText();

  std::string current_file_;
  bool file_is_loaded_ = false;
  TextEditor text_editor_;
};

}  // namespace editor
}  // namespace app
}  // namespace yaze

#endif