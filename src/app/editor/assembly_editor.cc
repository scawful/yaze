#include "assembly_editor.h"

namespace yaze {
namespace app {
namespace editor {

void AssemblyEditor::Update() {
  SetEditorText();
  auto cpos = text_editor_.GetCursorPosition();
  ImGui::Begin("ASM Editor", &file_is_loaded_);
  ImGui::Text("%6d/%-6d %6d lines  | %s | %s | %s | %s", cpos.mLine + 1,
              cpos.mColumn + 1, text_editor_.GetTotalLines(),
              text_editor_.IsOverwrite() ? "Ovr" : "Ins",
              text_editor_.CanUndo() ? "*" : " ",
              text_editor_.GetLanguageDefinition().mName.c_str(),
              current_file_.c_str());

  text_editor_.Render(current_file_.c_str());
  ImGui::End();
}

void AssemblyEditor::ChangeActiveFile(const std::string & filename) {
  current_file_ = filename;
}

void AssemblyEditor::SetEditorText() {
  if (!file_is_loaded_) {
    std::ifstream t(current_file_);
    if (t.good()) {
      std::string str((std::istreambuf_iterator<char>(t)),
                      std::istreambuf_iterator<char>());
      text_editor_.SetText(str);
    }
    file_is_loaded_ = true;
  }
}

}  // namespace editor
}  // namespace app
}  // namespace yaze