#include "assembly_editor.h"

#include <ImGuiColorTextEdit/TextEditor.h>

#include "app/core/platform/file_dialog.h"
#include "app/gui/widgets.h"
#include "core/constants.h"

namespace yaze {
namespace app {
namespace editor {

namespace {

std::vector<std::string> RemoveIgnoredFiles(
    const std::vector<std::string>& files,
    const std::vector<std::string>& ignored_files) {
  std::vector<std::string> filtered_files;
  for (const auto& file : files) {
    // Remove subdirectory files
    if (file.find('/') != std::string::npos) {
      continue;
    }
    // Make sure the file has an extension
    if (file.find('.') == std::string::npos) {
      continue;
    }
    if (std::find(ignored_files.begin(), ignored_files.end(), file) ==
        ignored_files.end()) {
      filtered_files.push_back(file);
    }
  }
  return filtered_files;
}

core::FolderItem LoadFolder(const std::string& folder) {
  // Check if .gitignore exists in the folder
  std::ifstream gitignore(folder + "/.gitignore");
  std::vector<std::string> ignored_files;
  if (gitignore.good()) {
    std::string line;
    while (std::getline(gitignore, line)) {
      if (line[0] == '#') {
        continue;
      }
      if (line[0] == '!') {
        // Ignore the file
        continue;
      }
      ignored_files.push_back(line);
    }
  }

  core::FolderItem current_folder;
  current_folder.name = folder;
  auto root_files = FileDialogWrapper::GetFilesInFolder(current_folder.name);
  current_folder.files = RemoveIgnoredFiles(root_files, ignored_files);

  for (const auto& folder :
       FileDialogWrapper::GetSubdirectoriesInFolder(current_folder.name)) {
    core::FolderItem folder_item;
    folder_item.name = folder;
    std::string full_folder = current_folder.name + "/" + folder;
    auto folder_files = FileDialogWrapper::GetFilesInFolder(full_folder);
    for (const auto& files : folder_files) {
      // Remove subdirectory files
      if (files.find('/') != std::string::npos) {
        continue;
      }
      // Make sure the file has an extension
      if (files.find('.') == std::string::npos) {
        continue;
      }
      if (std::find(ignored_files.begin(), ignored_files.end(), files) !=
          ignored_files.end()) {
        continue;
      }
      folder_item.files.push_back(files);
    }

    for (const auto& subdir :
         FileDialogWrapper::GetSubdirectoriesInFolder(full_folder)) {
      core::FolderItem subfolder_item;
      subfolder_item.name = subdir;
      subfolder_item.files = FileDialogWrapper::GetFilesInFolder(subdir);
      folder_item.subfolders.push_back(subfolder_item);
    }
    current_folder.subfolders.push_back(folder_item);
  }

  return current_folder;
}

}  // namespace

void AssemblyEditor::OpenFolder(const std::string& folder_path) {
  current_folder_ = LoadFolder(folder_path);
}

void AssemblyEditor::Update(bool& is_loaded) {
  ImGui::Begin("Assembly Editor", &is_loaded);
  MENU_BAR()
  DrawFileMenu();
  DrawEditMenu();
  END_MENU_BAR()

  auto cpos = text_editor_.GetCursorPosition();
  SetEditorText();
  ImGui::Text("%6d/%-6d %6d lines  | %s | %s | %s | %s", cpos.mLine + 1,
              cpos.mColumn + 1, text_editor_.GetTotalLines(),
              text_editor_.IsOverwrite() ? "Ovr" : "Ins",
              text_editor_.CanUndo() ? "*" : " ",
              text_editor_.GetLanguageDefinition().mName.c_str(),
              current_file_.c_str());

  text_editor_.Render("##asm_editor");
  ImGui::End();
}

void AssemblyEditor::InlineUpdate() {
  ChangeActiveFile("assets/asm/template_song.asm");
  auto cpos = text_editor_.GetCursorPosition();
  SetEditorText();
  ImGui::Text("%6d/%-6d %6d lines  | %s | %s | %s | %s", cpos.mLine + 1,
              cpos.mColumn + 1, text_editor_.GetTotalLines(),
              text_editor_.IsOverwrite() ? "Ovr" : "Ins",
              text_editor_.CanUndo() ? "*" : " ",
              text_editor_.GetLanguageDefinition().mName.c_str(),
              current_file_.c_str());

  text_editor_.Render("##asm_editor", ImVec2(0, 0));
}

void AssemblyEditor::UpdateCodeView() {
  ImGui::BeginTable("##table_view", 2,
                    ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg |
                        ImGuiTableFlags_Resizable);

  // Table headers
  ImGui::TableSetupColumn("Files", ImGuiTableColumnFlags_WidthFixed, 256.0f);
  ImGui::TableSetupColumn("Editor", ImGuiTableColumnFlags_WidthStretch);

  ImGui::TableHeadersRow();

  // Table data
  ImGui::TableNextRow();
  ImGui::TableNextColumn();
  if (current_folder_.name != "") {
    DrawCurrentFolder();
  } else {
    if (ImGui::Button("Open Folder")) {
      current_folder_ = LoadFolder(FileDialogWrapper::ShowOpenFolderDialog());
    }
  }

  ImGui::TableNextColumn();

  auto cpos = text_editor_.GetCursorPosition();
  SetEditorText();
  ImGui::Text("%6d/%-6d %6d lines  | %s | %s | %s | %s", cpos.mLine + 1,
              cpos.mColumn + 1, text_editor_.GetTotalLines(),
              text_editor_.IsOverwrite() ? "Ovr" : "Ins",
              text_editor_.CanUndo() ? "*" : " ",
              text_editor_.GetLanguageDefinition().mName.c_str(),
              current_file_.c_str());

  text_editor_.Render("##asm_editor");

  ImGui::EndTable();
}

void AssemblyEditor::DrawCurrentFolder() {
  if (ImGui::BeginChild("##current_folder", ImVec2(0, 0), true,
                        ImGuiWindowFlags_AlwaysVerticalScrollbar)) {
    if (ImGui::BeginTable("##file_table", 2,
                          ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg |
                              ImGuiTableFlags_Resizable |
                              ImGuiTableFlags_Sortable)) {
      ImGui::TableSetupColumn("Name", ImGuiTableColumnFlags_WidthFixed, 256.0f);
      ImGui::TableSetupColumn("Type", ImGuiTableColumnFlags_WidthStretch);

      ImGui::TableHeadersRow();

      for (const auto& file : current_folder_.files) {
        ImGui::TableNextRow();
        ImGui::TableNextColumn();
        if (ImGui::Selectable(file.c_str())) {
          ChangeActiveFile(absl::StrCat(current_folder_.name, "/", file));
        }
        ImGui::TableNextColumn();
        ImGui::Text("File");
      }

      for (const auto& subfolder : current_folder_.subfolders) {
        ImGui::TableNextRow();
        ImGui::TableNextColumn();
        if (ImGui::TreeNode(subfolder.name.c_str())) {
          for (const auto& file : subfolder.files) {
            ImGui::TableNextRow();
            ImGui::TableNextColumn();
            if (ImGui::Selectable(file.c_str())) {
              ChangeActiveFile(absl::StrCat(current_folder_.name, "/",
                                            subfolder.name, "/", file));
            }
            ImGui::TableNextColumn();
            ImGui::Text("File");
          }
          ImGui::TreePop();
        } else {
          ImGui::TableNextColumn();
          ImGui::Text("Folder");
        }
      }

      ImGui::EndTable();
    }

    ImGui::EndChild();
  }
}

void AssemblyEditor::DrawFileTabView() {
  static int next_tab_id = 0;

  if (ImGui::BeginTabBar("AssemblyFileTabBar", ImGuiTabBarFlags_None)) {
    if (ImGui::TabItemButton("+", ImGuiTabItemFlags_None)) {
      if (std::find(active_files_.begin(), active_files_.end(),
                    current_file_id_) != active_files_.end()) {
        // Room is already open
        next_tab_id++;
      }
      active_files_.push_back(next_tab_id++);  // Add new tab
    }

    // Submit our regular tabs
    for (int n = 0; n < active_files_.Size;) {
      bool open = true;

      if (ImGui::BeginTabItem(files_[active_files_[n]].data(), &open,
                              ImGuiTabItemFlags_None)) {
        auto cpos = text_editor_.GetCursorPosition();
        {
          std::ifstream t(current_file_);
          if (t.good()) {
            std::string str((std::istreambuf_iterator<char>(t)),
                            std::istreambuf_iterator<char>());
            text_editor_.SetText(str);
          } else {
            SDL_LogError(SDL_LOG_CATEGORY_APPLICATION,
                         "Error opening file: %s\n", current_file_.c_str());
          }
        }
        ImGui::Text("%6d/%-6d %6d lines  | %s | %s | %s | %s", cpos.mLine + 1,
                    cpos.mColumn + 1, text_editor_.GetTotalLines(),
                    text_editor_.IsOverwrite() ? "Ovr" : "Ins",
                    text_editor_.CanUndo() ? "*" : " ",
                    text_editor_.GetLanguageDefinition().mName.c_str(),
                    current_file_.c_str());

        open_files_[active_files_[n]].Render("##asm_editor");
        ImGui::EndTabItem();
      }

      if (!open)
        active_files_.erase(active_files_.Data + n);
      else
        n++;
    }

    ImGui::EndTabBar();
  }
  ImGui::Separator();
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
    } else {
      SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "Error opening file: %s\n",
                   current_file_.c_str());
    }
    file_is_loaded_ = true;
  }
}

absl::Status AssemblyEditor::Cut() {
  text_editor_.Cut();
  return absl::OkStatus();
}

absl::Status AssemblyEditor::Copy() {
  text_editor_.Copy();
  return absl::OkStatus();
}

absl::Status AssemblyEditor::Paste() {
  text_editor_.Paste();
  return absl::OkStatus();
}

absl::Status AssemblyEditor::Undo() {
  text_editor_.Undo();
  return absl::OkStatus();
}

absl::Status AssemblyEditor::Redo() {
  text_editor_.Redo();
  return absl::OkStatus();
}

absl::Status AssemblyEditor::Update() { return absl::OkStatus(); }

}  // namespace editor
}  // namespace app
}  // namespace yaze