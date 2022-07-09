#include "assembly_editor.h"

#include "core/constants.h"

namespace yaze {
namespace app {
namespace editor {

void AssemblyEditor::Update() {
  auto cpos = text_editor_.GetCursorPosition();
  ImGui::Begin("ASM Editor", &file_is_loaded_, ImGuiWindowFlags_MenuBar);
  MENU_BAR()
  DrawFileMenu();
  DrawEditMenu();
  END_MENU_BAR()

  SetEditorText();
  ImGui::Text("%6d/%-6d %6d lines  | %s | %s | %s | %s", cpos.mLine + 1,
              cpos.mColumn + 1, text_editor_.GetTotalLines(),
              text_editor_.IsOverwrite() ? "Ovr" : "Ins",
              text_editor_.CanUndo() ? "*" : " ",
              text_editor_.GetLanguageDefinition().mName.c_str(),
              current_file_.c_str());

  text_editor_.Render(current_file_.c_str());
  ImGui::End();
}

void AssemblyEditor::ChangeActiveFile(const std::string& filename) {
  current_file_ = filename;
}

void AssemblyEditor::DrawFileMenu() {
  if (ImGui::BeginMenu("File")) {
    if (ImGui::MenuItem("Open", "Ctrl+O")) {
      ImGuiFileDialog::Instance()->OpenDialog(
          "ChooseASMFileDlg", "Open ASM file", ".asm,.txt", ".");
    }
    if (ImGui::MenuItem("Save", "Ctrl+S")) {
      // TODO: Implement this
    }
    ImGui::EndMenu();
  }

  if (ImGuiFileDialog::Instance()->Display("ChooseASMFileDlg")) {
    if (ImGuiFileDialog::Instance()->IsOk()) {
      ChangeActiveFile(ImGuiFileDialog::Instance()->GetFilePathName());
    }
    ImGuiFileDialog::Instance()->Close();
  }
}

void AssemblyEditor::DrawEditMenu() {
  if (ImGui::BeginMenu("Edit")) {
    if (ImGui::MenuItem("Undo", "Ctrl+Z")) {
      text_editor_.Undo();
    }
    if (ImGui::MenuItem("Redo", "Ctrl+Y")) {
      text_editor_.Redo();
    }
    ImGui::Separator();
    if (ImGui::MenuItem("Cut", "Ctrl+X")) {
      text_editor_.Cut();
    }
    if (ImGui::MenuItem("Copy", "Ctrl+C")) {
      text_editor_.Copy();
    }
    if (ImGui::MenuItem("Paste", "Ctrl+V")) {
      text_editor_.Paste();
    }
    ImGui::Separator();
    if (ImGui::MenuItem("Find", "Ctrl+F")) {
      // TODO: Implement this.
    }
    ImGui::EndMenu();
  }
}

void AssemblyEditor::SetEditorText() {
  if (!file_is_loaded_) {
    std::ifstream t(current_file_);
    if (t.good()) {
      std::string str((std::istreambuf_iterator<char>(t)),
                      std::istreambuf_iterator<char>());
      text_editor_.SetText(str);
      file_is_loaded_ = true;
    }
  }
}

}  // namespace editor
}  // namespace app
}  // namespace yaze