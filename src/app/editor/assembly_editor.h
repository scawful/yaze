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
  AssemblyEditor();

  void Update();
  void ChangeActiveFile(const std::string &);

 private:
  void DrawFileMenu();
  void DrawEditMenu();
  void SetEditorText();

  bool file_is_loaded_ = false;

  std::string current_file_;
  TextEditor text_editor_;
};

}  // namespace editor
}  // namespace app
}  // namespace yaze

#endif