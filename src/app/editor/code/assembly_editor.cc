#include "assembly_editor.h"

#include <fstream>
#include <string>
#include <vector>

#include "absl/strings/str_cat.h"
#include "absl/strings/match.h"
#include "util/file_util.h"
#include "app/gui/core/icons.h"
#include "app/gui/core/ui_helpers.h"
#include "app/gui/widgets/text_editor.h"

namespace yaze::editor {

using util::FileDialogWrapper;

namespace {

static const char *const kKeywords[] = {
    "ADC", "AND", "ASL", "BCC", "BCS", "BEQ", "BIT",   "BMI",  "BNE", "BPL",
    "BRA", "BRL", "BVC", "BVS", "CLC", "CLD", "CLI",   "CLV",  "CMP", "CPX",
    "CPY", "DEC", "DEX", "DEY", "EOR", "INC", "INX",   "INY",  "JMP", "JSR",
    "JSL", "LDA", "LDX", "LDY", "LSR", "MVN", "NOP",   "ORA",  "PEA", "PER",
    "PHA", "PHB", "PHD", "PHP", "PHX", "PHY", "PLA",   "PLB",  "PLD", "PLP",
    "PLX", "PLY", "REP", "ROL", "ROR", "RTI", "RTL",   "RTS",  "SBC", "SEC",
    "SEI", "SEP", "STA", "STP", "STX", "STY", "STZ",   "TAX",  "TAY", "TCD",
    "TCS", "TDC", "TRB", "TSB", "TSC", "TSX", "TXA",   "TXS",  "TXY", "TYA",
    "TYX", "WAI", "WDM", "XBA", "XCE", "ORG", "LOROM", "HIROM"};

static const char *const kIdentifiers[] = {
    "abort",   "abs",     "acos",    "asin",     "atan",    "atexit",
    "atof",    "atoi",    "atol",    "ceil",     "clock",   "cosh",
    "ctime",   "div",     "exit",    "fabs",     "floor",   "fmod",
    "getchar", "getenv",  "isalnum", "isalpha",  "isdigit", "isgraph",
    "ispunct", "isspace", "isupper", "kbhit",    "log10",   "log2",
    "log",     "memcmp",  "modf",    "pow",      "putchar", "putenv",
    "puts",    "rand",    "remove",  "rename",   "sinh",    "sqrt",
    "srand",   "strcat",  "strcmp",  "strerror", "time",    "tolower",
    "toupper"};

TextEditor::LanguageDefinition GetAssemblyLanguageDef() {
  TextEditor::LanguageDefinition language_65816;
  for (auto &k : kKeywords) language_65816.mKeywords.emplace(k);

  for (auto &k : kIdentifiers) {
    TextEditor::Identifier id;
    id.mDeclaration = "Built-in function";
    language_65816.mIdentifiers.insert(std::make_pair(std::string(k), id));
  }

  language_65816.mTokenRegexStrings.push_back(
      std::make_pair<std::string, TextEditor::PaletteIndex>(
          "[ \\t]*#[ \\t]*[a-zA-Z_]+", TextEditor::PaletteIndex::Preprocessor));
  language_65816.mTokenRegexStrings.push_back(
      std::make_pair<std::string, TextEditor::PaletteIndex>(
          "L?\\\"(\\\\.|[^\\\"])*\\\"", TextEditor::PaletteIndex::String));
  language_65816.mTokenRegexStrings.push_back(
      std::make_pair<std::string, TextEditor::PaletteIndex>(
          "\\'\\\\?[^\\']\\'", TextEditor::PaletteIndex::CharLiteral));
  language_65816.mTokenRegexStrings.push_back(
      std::make_pair<std::string, TextEditor::PaletteIndex>(
          "[+-]?([0-9]+([.][0-9]*)?|[.][0-9]+)([eE][+-]?[0-9]+)?[fF]?",
          TextEditor::PaletteIndex::Number));
  language_65816.mTokenRegexStrings.push_back(
      std::make_pair<std::string, TextEditor::PaletteIndex>(
          "[+-]?[0-9]+[Uu]?[lL]?[lL]?", TextEditor::PaletteIndex::Number));
  language_65816.mTokenRegexStrings.push_back(
      std::make_pair<std::string, TextEditor::PaletteIndex>(
          "0[0-7]+[Uu]?[lL]?[lL]?", TextEditor::PaletteIndex::Number));
  language_65816.mTokenRegexStrings.push_back(
      std::make_pair<std::string, TextEditor::PaletteIndex>(
          "0[xX][0-9a-fA-F]+[uU]?[lL]?[lL]?",
          TextEditor::PaletteIndex::Number));
  language_65816.mTokenRegexStrings.push_back(
      std::make_pair<std::string, TextEditor::PaletteIndex>(
          "[a-zA-Z_][a-zA-Z0-9_]*", TextEditor::PaletteIndex::Identifier));
  language_65816.mTokenRegexStrings.push_back(
      std::make_pair<std::string, TextEditor::PaletteIndex>(
          "[\\[\\]\\{\\}\\!\\%\\^\\&\\*\\(\\)\\-\\+\\=\\~\\|\\<\\>\\?\\/"
          "\\;\\,\\.]",
          TextEditor::PaletteIndex::Punctuation));

  language_65816.mCommentStart = "/*";
  language_65816.mCommentEnd = "*/";
  language_65816.mSingleLineComment = ";";

  language_65816.mCaseSensitive = false;
  language_65816.mAutoIndentation = true;

  language_65816.mName = "65816";

  return language_65816;
}

std::vector<std::string> RemoveIgnoredFiles(
    const std::vector<std::string>& files,
    const std::vector<std::string>& ignored_files) {
  std::vector<std::string> filtered_files;
  for (const auto& file : files) {
    // Remove subdirectory files
    if (absl::StrContains(file, '/')) {
      continue;
    }
    // Make sure the file has an extension
    if (!absl::StrContains(file, '.')) {
      continue;
    }
    if (std::ranges::find(ignored_files, file) == ignored_files.end()) {
      filtered_files.push_back(file);
    }
  }
  return filtered_files;
}

FolderItem LoadFolder(const std::string& folder) {
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

  FolderItem current_folder;
  current_folder.name = folder;
  auto root_files = FileDialogWrapper::GetFilesInFolder(current_folder.name);
  current_folder.files = RemoveIgnoredFiles(root_files, ignored_files);

  for (const auto& subfolder :
       FileDialogWrapper::GetSubdirectoriesInFolder(current_folder.name)) {
    FolderItem folder_item;
    folder_item.name = subfolder;
    std::string full_folder = current_folder.name + "/" + subfolder;
    auto folder_files = FileDialogWrapper::GetFilesInFolder(full_folder);
    for (const auto& files : folder_files) {
      // Remove subdirectory files
      if (absl::StrContains(files, '/')) {
        continue;
      }
      // Make sure the file has an extension
      if (!absl::StrContains(files, '.')) {
        continue;
      }
      if (std::ranges::find(ignored_files, files) != ignored_files.end()) {
        continue;
      }
      folder_item.files.push_back(files);
    }

    for (const auto& subdir :
         FileDialogWrapper::GetSubdirectoriesInFolder(full_folder)) {
      FolderItem subfolder_item;
      subfolder_item.name = subdir;
      subfolder_item.files = FileDialogWrapper::GetFilesInFolder(subdir);
      folder_item.subfolders.push_back(subfolder_item);
    }
    current_folder.subfolders.push_back(folder_item);
  }

  return current_folder;
}

}  // namespace

void AssemblyEditor::Initialize() {
  text_editor_.SetLanguageDefinition(GetAssemblyLanguageDef());
  
  // Register cards with EditorCardManager
  auto& card_manager = gui::EditorCardManager::Get();
  card_manager.RegisterCard({.card_id = "assembly.editor", .display_name = "Assembly Editor",
                            .icon = ICON_MD_CODE, .category = "Assembly",
                            .shortcut_hint = "", .priority = 10});
  card_manager.RegisterCard({.card_id = "assembly.file_browser", .display_name = "File Browser",
                            .icon = ICON_MD_FOLDER_OPEN, .category = "Assembly",
                            .shortcut_hint = "", .priority = 20});
  
  // Don't show by default - only show when user explicitly opens Assembly Editor
}

absl::Status AssemblyEditor::Load() {
  // Register cards with EditorCardManager
  // Note: Assembly editor uses dynamic file tabs, so we register the main editor window
  auto& card_manager = gui::EditorCardManager::Get();
  
  return absl::OkStatus(); 
}

void AssemblyEditor::OpenFolder(const std::string& folder_path) {
  current_folder_ = LoadFolder(folder_path);
}

void AssemblyEditor::Update(bool& is_loaded) {
  ImGui::Begin("Assembly Editor", &is_loaded);
  if (ImGui::BeginMenuBar()) {
    DrawFileMenu();
    DrawEditMenu();
    ImGui::EndMenuBar();
  }

  auto cpos = text_editor_.GetCursorPosition();
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
  auto cpos = text_editor_.GetCursorPosition();
  ImGui::Text("%6d/%-6d %6d lines  | %s | %s | %s | %s", cpos.mLine + 1,
              cpos.mColumn + 1, text_editor_.GetTotalLines(),
              text_editor_.IsOverwrite() ? "Ovr" : "Ins",
              text_editor_.CanUndo() ? "*" : " ",
              text_editor_.GetLanguageDefinition().mName.c_str(),
              current_file_.c_str());

  text_editor_.Render("##asm_editor", ImVec2(0, 0));
}

void AssemblyEditor::UpdateCodeView() {
  DrawToolset();
  gui::VerticalSpacing(2.0f);

  // Create session-aware card (non-static for multi-session support)
  gui::EditorCard file_browser_card(MakeCardTitle("File Browser").c_str(), ICON_MD_FOLDER);
  bool file_browser_open = true;
  if (file_browser_card.Begin(&file_browser_open)) {
    if (current_folder_.name != "") {
      DrawCurrentFolder();
    } else {
      if (ImGui::Button("Open Folder")) {
        current_folder_ = LoadFolder(FileDialogWrapper::ShowOpenFolderDialog());
      }
    }
  }
  file_browser_card.End();  // ALWAYS call End after Begin

  // Draw open files as individual, dockable EditorCards
  for (int i = 0; i < active_files_.Size; i++) {
    int file_id = active_files_[i];
    bool open = true;

    // Ensure we have a TextEditor instance for this file
    if (file_id >= open_files_.size()) {
        open_files_.resize(file_id + 1);
    }
    if (file_id >= files_.size()) {
        // This can happen if a file was closed and its ID is being reused.
        // For now, we just skip it.
        continue;
    }

    // Create session-aware card title for each file
    std::string card_name = MakeCardTitle(files_[file_id]);
    gui::EditorCard file_card(card_name.c_str(), ICON_MD_DESCRIPTION, &open);
    if (file_card.Begin()) {
        if (ImGui::IsWindowFocused(ImGuiFocusedFlags_RootAndChildWindows)) {
            active_file_id_ = file_id;
        }
        open_files_[file_id].Render(absl::StrCat("##", card_name).c_str());
    }
    file_card.End();  // ALWAYS call End after Begin

    if (!open) {
      active_files_.erase(active_files_.Data + i);
      i--;
    }
  }
}

absl::Status AssemblyEditor::Save() {
    if (active_file_id_ != -1 && active_file_id_ < open_files_.size()) {
        std::string content = open_files_[active_file_id_].GetText();
        util::SaveFile(files_[active_file_id_], content);
        return absl::OkStatus();
    }
    return absl::FailedPreconditionError("No active file to save.");
}

void AssemblyEditor::DrawToolset() {
    static gui::Toolset toolbar;
    toolbar.Begin();

    if (toolbar.AddAction(ICON_MD_FOLDER_OPEN, "Open Folder")) {
        current_folder_ = LoadFolder(FileDialogWrapper::ShowOpenFolderDialog());
    }
    if (toolbar.AddAction(ICON_MD_SAVE, "Save File")) {
        Save();
    }

    toolbar.End();
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


void AssemblyEditor::DrawFileMenu() {
  if (ImGui::BeginMenu("File")) {
    if (ImGui::MenuItem("Open", "Ctrl+O")) {
      auto filename = util::FileDialogWrapper::ShowOpenFileDialog();
      ChangeActiveFile(filename);
    }
    if (ImGui::MenuItem("Save", "Ctrl+S")) {
      // TODO: Implement this
    }
    ImGui::EndMenu();
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

void AssemblyEditor::ChangeActiveFile(const std::string_view &filename) {
    // Check if file is already open
    for (int i = 0; i < active_files_.Size; ++i) {
        int file_id = active_files_[i];
        if (files_[file_id] == filename) {
            // Optional: Focus window
            return;
        }
    }

    // Add new file
    int new_file_id = files_.size();
    files_.push_back(std::string(filename));
    active_files_.push_back(new_file_id);

    // Resize open_files_ if needed
    if (new_file_id >= open_files_.size()) {
        open_files_.resize(new_file_id + 1);
    }

    // Load file content using utility
    std::string content = util::LoadFile(std::string(filename));
    if (!content.empty()) {
        open_files_[new_file_id].SetText(content);
        open_files_[new_file_id].SetLanguageDefinition(GetAssemblyLanguageDef());
        open_files_[new_file_id].SetPalette(TextEditor::GetDarkPalette());
    } else {
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "Error opening file: %s\n",
                     std::string(filename).c_str());
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

}  // namespace yaze::editor
