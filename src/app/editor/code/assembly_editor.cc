#include "assembly_editor.h"

#include <filesystem>
#include <fstream>
#include <optional>
#include <string>
#include <vector>

#if defined(__APPLE__)
#include <TargetConditionals.h>
#endif

#include <cctype>

#include "absl/strings/ascii.h"
#include "absl/strings/match.h"
#include "absl/strings/str_cat.h"
#include "app/editor/code/panels/assembly_editor_panels.h"
#include "app/editor/system/panel_manager.h"
#include "app/editor/ui/toast_manager.h"
#include "app/gui/core/icons.h"
#include "app/gui/core/style_guard.h"
#include "app/gui/core/ui_helpers.h"
#include "app/gui/widgets/text_editor.h"
#include "core/project.h"
#include "core/version_manager.h"
#include "util/file_util.h"

namespace yaze::editor {

using util::FileDialogWrapper;

namespace {

static const char* const kKeywords[] = {
    "ADC", "AND", "ASL", "BCC", "BCS", "BEQ", "BIT",   "BMI",  "BNE", "BPL",
    "BRA", "BRL", "BVC", "BVS", "CLC", "CLD", "CLI",   "CLV",  "CMP", "CPX",
    "CPY", "DEC", "DEX", "DEY", "EOR", "INC", "INX",   "INY",  "JMP", "JSR",
    "JSL", "LDA", "LDX", "LDY", "LSR", "MVN", "NOP",   "ORA",  "PEA", "PER",
    "PHA", "PHB", "PHD", "PHP", "PHX", "PHY", "PLA",   "PLB",  "PLD", "PLP",
    "PLX", "PLY", "REP", "ROL", "ROR", "RTI", "RTL",   "RTS",  "SBC", "SEC",
    "SEI", "SEP", "STA", "STP", "STX", "STY", "STZ",   "TAX",  "TAY", "TCD",
    "TCS", "TDC", "TRB", "TSB", "TSC", "TSX", "TXA",   "TXS",  "TXY", "TYA",
    "TYX", "WAI", "WDM", "XBA", "XCE", "ORG", "LOROM", "HIROM"};

static const char* const kIdentifiers[] = {
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
  for (auto& k : kKeywords)
    language_65816.mKeywords.emplace(k);

  for (auto& k : kIdentifiers) {
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
  std::vector<std::string> ignored_files;
#if !(defined(__APPLE__) && TARGET_OS_IOS == 1)
  std::ifstream gitignore(folder + "/.gitignore");
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
#endif

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

std::optional<AsmSymbolLocation> FindLabelInFile(
    const std::filesystem::path& path, const std::string& label) {
  std::ifstream file(path);
  if (!file.is_open()) {
    return std::nullopt;
  }

  std::string line;
  int line_index = 0;
  while (std::getline(file, line)) {
    const size_t start = line.find_first_not_of(" \t");
    if (start == std::string::npos) {
      ++line_index;
      continue;
    }

    if (line.compare(start, label.size(), label) != 0) {
      ++line_index;
      continue;
    }

    size_t pos = start + label.size();
    while (pos < line.size() && (line[pos] == ' ' || line[pos] == '\t')) {
      ++pos;
    }

    if (pos < line.size() && line[pos] == ':') {
      AsmSymbolLocation loc;
      loc.file = path.string();
      loc.line = line_index;
      loc.column = static_cast<int>(start);
      return loc;
    }

    ++line_index;
  }

  return std::nullopt;
}

bool IsAssemblyLikeFile(const std::filesystem::path& path) {
  const auto ext = path.extension().string();
  return ext == ".asm" || ext == ".inc" || ext == ".s";
}

struct AsmFileLineRef {
  std::string file_ref;
  int line_one_based = 0;
  int column_one_based = 1;
};

struct AsmFileSymbolRef {
  std::string file_ref;
  std::string symbol;
};

std::optional<int> ParsePositiveInt(const std::string& s) {
  if (s.empty()) {
    return std::nullopt;
  }
  for (char c : s) {
    if (!std::isdigit(static_cast<unsigned char>(c))) {
      return std::nullopt;
    }
  }
  try {
    const int v = std::stoi(s);
    return v > 0 ? std::optional<int>(v) : std::nullopt;
  } catch (...) {
    return std::nullopt;
  }
}

bool LooksLikeAssemblyPathRef(const std::string& file_ref) {
  if (file_ref.empty()) {
    return false;
  }
  const std::filesystem::path p(file_ref);
  return IsAssemblyLikeFile(p);
}

std::optional<AsmFileLineRef> ParseAsmFileLineRef(
    const std::string& reference) {
  const std::string trimmed =
      std::string(absl::StripAsciiWhitespace(reference));
  if (trimmed.empty()) {
    return std::nullopt;
  }

  // Format: "file.asm#L123"
  if (const size_t pos = trimmed.find("#L"); pos != std::string::npos) {
    const std::string file =
        std::string(absl::StripAsciiWhitespace(trimmed.substr(0, pos)));
    const std::string line_str = std::string(
        absl::StripAsciiWhitespace(trimmed.substr(pos + 2)));
    if (!LooksLikeAssemblyPathRef(file)) {
      return std::nullopt;
    }
    if (auto line = ParsePositiveInt(line_str); line.has_value()) {
      return AsmFileLineRef{file, *line, /*column_one_based=*/1};
    }
    return std::nullopt;
  }

  // Formats:
  // - "file.asm:123"
  // - "file.asm:123:10"
  const size_t last_colon = trimmed.rfind(':');
  if (last_colon == std::string::npos) {
    return std::nullopt;
  }

  const std::string tail = std::string(
      absl::StripAsciiWhitespace(trimmed.substr(last_colon + 1)));
  if (tail.empty()) {
    return std::nullopt;
  }

  const size_t second_last_colon =
      (last_colon == 0) ? std::string::npos : trimmed.rfind(':', last_colon - 1);

  if (second_last_colon != std::string::npos) {
    const std::string file = std::string(
        absl::StripAsciiWhitespace(trimmed.substr(0, second_last_colon)));
    const std::string line_str = std::string(absl::StripAsciiWhitespace(
        trimmed.substr(second_last_colon + 1,
                       last_colon - second_last_colon - 1)));
    const std::string col_str = tail;
    if (!LooksLikeAssemblyPathRef(file)) {
      return std::nullopt;
    }
    auto line = ParsePositiveInt(line_str);
    auto col = ParsePositiveInt(col_str);
    if (!line.has_value() || !col.has_value()) {
      return std::nullopt;
    }
    return AsmFileLineRef{file, *line, *col};
  }

  const std::string file = std::string(
      absl::StripAsciiWhitespace(trimmed.substr(0, last_colon)));
  if (!LooksLikeAssemblyPathRef(file)) {
    return std::nullopt;
  }
  if (auto line = ParsePositiveInt(tail); line.has_value()) {
    return AsmFileLineRef{file, *line, /*column_one_based=*/1};
  }
  return std::nullopt;
}

std::optional<AsmFileSymbolRef> ParseAsmFileSymbolRef(
    const std::string& reference) {
  const std::string trimmed =
      std::string(absl::StripAsciiWhitespace(reference));
  if (trimmed.empty()) {
    return std::nullopt;
  }

  // Format: "file.asm#Label"
  if (const size_t pos = trimmed.rfind('#'); pos != std::string::npos) {
    const std::string file =
        std::string(absl::StripAsciiWhitespace(trimmed.substr(0, pos)));
    const std::string sym = std::string(
        absl::StripAsciiWhitespace(trimmed.substr(pos + 1)));
    if (!LooksLikeAssemblyPathRef(file) || sym.empty()) {
      return std::nullopt;
    }
    return AsmFileSymbolRef{file, sym};
  }

  // Format: "file.asm:Label"
  const size_t last_colon = trimmed.rfind(':');
  if (last_colon == std::string::npos) {
    return std::nullopt;
  }

  const std::string file = std::string(
      absl::StripAsciiWhitespace(trimmed.substr(0, last_colon)));
  const std::string sym = std::string(
      absl::StripAsciiWhitespace(trimmed.substr(last_colon + 1)));
  if (!LooksLikeAssemblyPathRef(file) || sym.empty()) {
    return std::nullopt;
  }

  // Avoid interpreting file:line refs as file:symbol (line parser should run
  // first, but keep this extra guard anyway).
  if (ParsePositiveInt(sym).has_value()) {
    return std::nullopt;
  }

  return AsmFileSymbolRef{file, sym};
}

std::optional<std::filesystem::path> FindAsmFileInFolder(
    const std::filesystem::path& root, const std::string& file_ref) {
  std::filesystem::path p(file_ref);
  if (p.is_absolute()) {
    std::error_code ec;
    if (std::filesystem::exists(p, ec) && std::filesystem::is_regular_file(p, ec)) {
      return p;
    }
    return std::nullopt;
  }

  // Try relative to root first (supports "dir/file.asm" paths).
  {
    const std::filesystem::path candidate = root / p;
    std::error_code ec;
    if (std::filesystem::exists(candidate, ec) &&
        std::filesystem::is_regular_file(candidate, ec)) {
      return candidate;
    }
  }

  // Fallback: recursive search by suffix match.
  const std::string want_suffix = p.generic_string();
  const std::string want_name = p.filename().string();

  std::error_code ec;
  if (!std::filesystem::exists(root, ec)) {
    return std::nullopt;
  }

  std::filesystem::recursive_directory_iterator it(
      root, std::filesystem::directory_options::skip_permission_denied, ec);
  const std::filesystem::recursive_directory_iterator end;
  for (; it != end && !ec; it.increment(ec)) {
    const auto& entry = *it;
    if (entry.is_directory()) {
      const auto name = entry.path().filename().string();
      if (!name.empty() && name.front() == '.') {
        it.disable_recursion_pending();
      } else if (name == "build" || name == "build_ai" || name == "build-ios" ||
                 name == "build-ios-sim" || name == "build-wasm" ||
                 name == "node_modules") {
        it.disable_recursion_pending();
      }
      continue;
    }

    if (!entry.is_regular_file()) {
      continue;
    }
    if (!IsAssemblyLikeFile(entry.path())) {
      continue;
    }

    const std::string cand = entry.path().generic_string();
    if (!want_suffix.empty() && absl::EndsWith(cand, want_suffix)) {
      return entry.path();
    }
    if (!want_name.empty() && entry.path().filename() == want_name) {
      return entry.path();
    }
  }

  return std::nullopt;
}

std::optional<AsmSymbolLocation> FindLabelInFolder(
    const std::filesystem::path& root, const std::string& label) {
  std::error_code ec;
  if (!std::filesystem::exists(root, ec)) {
    return std::nullopt;
  }

  std::filesystem::recursive_directory_iterator it(
      root, std::filesystem::directory_options::skip_permission_denied, ec);
  const std::filesystem::recursive_directory_iterator end;
  for (; it != end && !ec; it.increment(ec)) {
    const auto& entry = *it;
    if (entry.is_directory()) {
      const auto name = entry.path().filename().string();
      if (!name.empty() && name.front() == '.') {
        it.disable_recursion_pending();
      } else if (name == "build" || name == "build_ai" || name == "build-ios" ||
                 name == "build-ios-sim" || name == "build-wasm" ||
                 name == "node_modules") {
        it.disable_recursion_pending();
      }
      continue;
    }

    if (!entry.is_regular_file()) {
      continue;
    }

    if (!IsAssemblyLikeFile(entry.path())) {
      continue;
    }

    if (auto loc = FindLabelInFile(entry.path(), label); loc.has_value()) {
      return loc;
    }
  }

  return std::nullopt;
}

}  // namespace

void AssemblyEditor::Initialize() {
  text_editor_.SetLanguageDefinition(GetAssemblyLanguageDef());

  // Register panels with PanelManager using EditorPanel instances
  if (!dependencies_.panel_manager)
    return;
  auto* panel_manager = dependencies_.panel_manager;

  // Register Code Editor panel - main text editing
  panel_manager->RegisterEditorPanel(
      std::make_unique<AssemblyCodeEditorPanel>([this]() { DrawCodeEditor(); }));

  // Register File Browser panel - project file navigation
  panel_manager->RegisterEditorPanel(
      std::make_unique<AssemblyFileBrowserPanel>([this]() { DrawFileBrowser(); }));

  // Register Symbols panel - symbol table viewer
  panel_manager->RegisterEditorPanel(
      std::make_unique<AssemblySymbolsPanel>([this]() { DrawSymbolsContent(); }));

  // Register Build Output panel - errors/warnings
  panel_manager->RegisterEditorPanel(
      std::make_unique<AssemblyBuildOutputPanel>([this]() { DrawBuildOutput(); }));

  // Register Toolbar panel - quick actions
  panel_manager->RegisterEditorPanel(
      std::make_unique<AssemblyToolbarPanel>([this]() { DrawToolbarContent(); }));
}

absl::Status AssemblyEditor::Load() {
  // Assembly editor doesn't require ROM data - files are loaded independently
  return absl::OkStatus();
}

absl::Status AssemblyEditor::JumpToSymbolDefinition(const std::string& symbol) {
  if (symbol.empty()) {
    return absl::InvalidArgumentError("Symbol is empty");
  }

  std::filesystem::path root;
  if (dependencies_.project && !dependencies_.project->code_folder.empty()) {
    root =
        dependencies_.project->GetAbsolutePath(dependencies_.project->code_folder);
  } else if (!current_folder_.name.empty()) {
    root = current_folder_.name;
  } else {
    return absl::FailedPreconditionError(
        "No code folder loaded (open a folder or set project code_folder)");
  }

  const std::string root_string = root.string();
  if (symbol_jump_root_ != root_string) {
    symbol_jump_root_ = root_string;
    symbol_jump_cache_.clear();
    symbol_jump_negative_cache_.clear();
  }

  if (current_folder_.name.empty()) {
    OpenFolder(root_string);
  }

  if (auto it = symbol_jump_cache_.find(symbol); it != symbol_jump_cache_.end()) {
    const auto& cached = it->second;
    ChangeActiveFile(cached.file);
    if (!HasActiveFile()) {
      return absl::InternalError("Failed to open file for symbol: " + symbol);
    }

    auto* editor = GetActiveEditor();
    if (!editor) {
      return absl::InternalError("No active text editor");
    }

    editor->SetCursorPosition(
        TextEditor::Coordinates(cached.line, cached.column));
    editor->SelectWordUnderCursor();
    return absl::OkStatus();
  }

  if (symbol_jump_negative_cache_.contains(symbol)) {
    return absl::NotFoundError("Symbol not found: " + symbol);
  }

  const auto loc = FindLabelInFolder(root, symbol);
  if (!loc.has_value()) {
    symbol_jump_negative_cache_.insert(symbol);
    return absl::NotFoundError("Symbol not found: " + symbol);
  }

  symbol_jump_cache_[symbol] = *loc;
  symbol_jump_negative_cache_.erase(symbol);

  ChangeActiveFile(loc->file);
  if (!HasActiveFile()) {
    return absl::InternalError("Failed to open file for symbol: " + symbol);
  }

  auto* editor = GetActiveEditor();
  if (!editor) {
    return absl::InternalError("No active text editor");
  }

  editor->SetCursorPosition(TextEditor::Coordinates(loc->line, loc->column));
  editor->SelectWordUnderCursor();
  return absl::OkStatus();
}

absl::Status AssemblyEditor::JumpToReference(const std::string& reference) {
  if (reference.empty()) {
    return absl::InvalidArgumentError("Reference is empty");
  }

  if (auto file_ref = ParseAsmFileLineRef(reference); file_ref.has_value()) {
    std::filesystem::path root;
    if (dependencies_.project && !dependencies_.project->code_folder.empty()) {
      root = dependencies_.project->GetAbsolutePath(
          dependencies_.project->code_folder);
    } else if (!current_folder_.name.empty()) {
      root = current_folder_.name;
    } else {
      return absl::FailedPreconditionError(
          "No code folder loaded (open a folder or set project code_folder)");
    }

    if (current_folder_.name.empty()) {
      OpenFolder(root.string());
    }

    auto path_or = FindAsmFileInFolder(root, file_ref->file_ref);
    if (!path_or.has_value()) {
      return absl::NotFoundError("File not found: " + file_ref->file_ref);
    }

    ChangeActiveFile(path_or->string());
    if (!HasActiveFile()) {
      return absl::InternalError("Failed to open file: " + path_or->string());
    }

    const int line0 = std::max(0, file_ref->line_one_based - 1);
    const int col0 = std::max(0, file_ref->column_one_based - 1);
    auto* editor = GetActiveEditor();
    if (!editor) {
      return absl::InternalError("No active text editor");
    }
    editor->SetCursorPosition(TextEditor::Coordinates(line0, col0));
    editor->SelectWordUnderCursor();
    return absl::OkStatus();
  }

  if (auto file_ref = ParseAsmFileSymbolRef(reference); file_ref.has_value()) {
    std::filesystem::path root;
    if (dependencies_.project && !dependencies_.project->code_folder.empty()) {
      root = dependencies_.project->GetAbsolutePath(
          dependencies_.project->code_folder);
    } else if (!current_folder_.name.empty()) {
      root = current_folder_.name;
    } else {
      return absl::FailedPreconditionError(
          "No code folder loaded (open a folder or set project code_folder)");
    }

    if (current_folder_.name.empty()) {
      OpenFolder(root.string());
    }

    auto path_or = FindAsmFileInFolder(root, file_ref->file_ref);
    if (!path_or.has_value()) {
      return absl::NotFoundError("File not found: " + file_ref->file_ref);
    }

    auto loc = FindLabelInFile(*path_or, file_ref->symbol);
    if (!loc.has_value()) {
      return absl::NotFoundError(
          absl::StrCat("Symbol not found in ", file_ref->file_ref, ": ",
                       file_ref->symbol));
    }

    ChangeActiveFile(loc->file);
    if (!HasActiveFile()) {
      return absl::InternalError("Failed to open file: " + loc->file);
    }

    auto* editor = GetActiveEditor();
    if (!editor) {
      return absl::InternalError("No active text editor");
    }
    editor->SetCursorPosition(TextEditor::Coordinates(loc->line, loc->column));
    editor->SelectWordUnderCursor();
    return absl::OkStatus();
  }

  return JumpToSymbolDefinition(reference);
}

std::string AssemblyEditor::active_file_path() const {
  if (!HasActiveFile()) {
    return "";
  }
  return files_[active_file_id_];
}

TextEditor::Coordinates AssemblyEditor::active_cursor_position() const {
  if (!HasActiveFile() ||
      active_file_id_ >= static_cast<int>(open_files_.size())) {
    return TextEditor::Coordinates::Invalid();
  }
  return open_files_[active_file_id_].GetCursorPosition();
}

TextEditor* AssemblyEditor::GetActiveEditor() {
  if (HasActiveFile()) {
    return &open_files_[active_file_id_];
  }
  return &text_editor_;
}

const TextEditor* AssemblyEditor::GetActiveEditor() const {
  if (HasActiveFile()) {
    return &open_files_[active_file_id_];
  }
  return &text_editor_;
}

void AssemblyEditor::OpenFolder(const std::string& folder_path) {
  current_folder_ = LoadFolder(folder_path);
  if (symbol_jump_root_ != folder_path) {
    symbol_jump_root_ = folder_path;
    ClearSymbolJumpCache();
  }
}

void AssemblyEditor::ClearSymbolJumpCache() {
  symbol_jump_cache_.clear();
  symbol_jump_negative_cache_.clear();
}

// =============================================================================
// Panel Content Drawing (EditorPanel System)
// =============================================================================

void AssemblyEditor::DrawCodeEditor() {
  TextEditor* editor = GetActiveEditor();
  // Menu bar for file operations
  if (ImGui::BeginMenuBar()) {
    DrawFileMenu();
    DrawEditMenu();
    DrawAssembleMenu();
    ImGui::EndMenuBar();
  }

  // Status line
  auto cpos = editor->GetCursorPosition();
  const char* file_label =
      current_file_.empty() ? "No file" : current_file_.c_str();
  ImGui::Text("%6d/%-6d %6d lines  | %s | %s | %s | %s", cpos.mLine + 1,
              cpos.mColumn + 1, editor->GetTotalLines(),
              editor->IsOverwrite() ? "Ovr" : "Ins",
              editor->CanUndo() ? "*" : " ",
              editor->GetLanguageDefinition().mName.c_str(), file_label);

  // Main text editor
  editor->Render("##asm_editor",
                 ImVec2(0, -ImGui::GetFrameHeightWithSpacing()));

  // Draw open file tabs at bottom
  DrawFileTabView();
}

void AssemblyEditor::DrawFileBrowser() {
  // Lazy load project folder if not already loaded
  if (current_folder_.name.empty() && dependencies_.project &&
      !dependencies_.project->code_folder.empty()) {
    OpenFolder(
        dependencies_.project->GetAbsolutePath(dependencies_.project->code_folder));
  }

  // Open folder button if no folder loaded
  if (current_folder_.name.empty()) {
    if (ImGui::Button(ICON_MD_FOLDER_OPEN " Open Folder",
                      ImVec2(ImGui::GetContentRegionAvail().x, 0))) {
      current_folder_ = LoadFolder(FileDialogWrapper::ShowOpenFolderDialog());
    }
    ImGui::Spacing();
    ImGui::TextDisabled("No folder opened");
    return;
  }

  // Folder path display
  ImGui::TextColored(ImVec4(0.6f, 0.6f, 0.6f, 1.0f), "%s",
                     current_folder_.name.c_str());
  ImGui::Separator();

  // File tree
      DrawCurrentFolder();
}

void AssemblyEditor::DrawSymbolsContent() {
  if (symbols_.empty()) {
    ImGui::TextDisabled("No symbols loaded.");
    ImGui::Spacing();
    ImGui::TextWrapped("Apply a patch or load external symbols to populate this list.");
    return;
  }

  // Search filter
  static char filter[256] = "";
  ImGui::SetNextItemWidth(-1);
  ImGui::InputTextWithHint("##symbol_filter", ICON_MD_SEARCH " Filter symbols...",
                           filter, sizeof(filter));
  ImGui::Separator();

  // Symbol list
  if (ImGui::BeginChild("##symbol_list", ImVec2(0, 0), false)) {
    for (const auto& [name, symbol] : symbols_) {
      // Apply filter
      if (filter[0] != '\0' && name.find(filter) == std::string::npos) {
        continue;
      }

      ImGui::PushID(name.c_str());
      if (ImGui::Selectable(name.c_str())) {
        // Could jump to symbol definition if line info is available
      }
      ImGui::SameLine(ImGui::GetContentRegionAvail().x - 60);
      ImGui::TextDisabled("$%06X", symbol.address);
      ImGui::PopID();
    }
  }
  ImGui::EndChild();
}

void AssemblyEditor::DrawBuildOutput() {
  // Error/warning counts
  ImGui::Text("Errors: %zu  Warnings: %zu", last_errors_.size(),
              last_warnings_.size());
  ImGui::Separator();

  // Build buttons
  bool has_active_file = HasActiveFile();
  bool has_rom = (rom_ && rom_->is_loaded());

  if (ImGui::Button(ICON_MD_CHECK_CIRCLE " Validate", ImVec2(120, 0))) {
    if (has_active_file) {
      auto status = ValidateCurrentFile();
      if (status.ok() && dependencies_.toast_manager) {
        dependencies_.toast_manager->Show("Validation passed!", ToastType::kSuccess);
      }
    }
  }
  ImGui::SameLine();
  bool apply_disabled = !has_rom || !has_active_file;
  ImGui::BeginDisabled(apply_disabled);
  if (ImGui::Button(ICON_MD_BUILD " Apply to ROM", ImVec2(140, 0))) {
    auto status = ApplyPatchToRom();
    if (status.ok() && dependencies_.toast_manager) {
      dependencies_.toast_manager->Show("Patch applied!", ToastType::kSuccess);
    }
  }
  ImGui::EndDisabled();
  if (apply_disabled &&
      ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled)) {
    if (!has_rom)
      ImGui::SetTooltip("Load a ROM first");
    else
      ImGui::SetTooltip("Open an assembly file first");
  }

  ImGui::Separator();

  // Output log
  if (ImGui::BeginChild("##build_log", ImVec2(0, 0), true)) {
    // Show errors in red
    for (const auto& error : last_errors_) {
      gui::StyleColorGuard err_guard(ImGuiCol_Text,
                                     ImVec4(1.0f, 0.4f, 0.4f, 1.0f));
      ImGui::TextWrapped("%s %s", ICON_MD_ERROR, error.c_str());
    }
    // Show warnings in yellow
    for (const auto& warning : last_warnings_) {
      gui::StyleColorGuard warn_guard(ImGuiCol_Text,
                                      ImVec4(1.0f, 0.8f, 0.2f, 1.0f));
      ImGui::TextWrapped("%s %s", ICON_MD_WARNING, warning.c_str());
    }
    if (last_errors_.empty() && last_warnings_.empty()) {
      ImGui::TextDisabled("No build output");
    }
  }
  ImGui::EndChild();
}

void AssemblyEditor::DrawToolbarContent() {
  float button_size = 32.0f;

  if (ImGui::Button(ICON_MD_FOLDER_OPEN, ImVec2(button_size, button_size))) {
    auto folder = FileDialogWrapper::ShowOpenFolderDialog();
    if (!folder.empty()) {
      current_folder_ = LoadFolder(folder);
    }
  }
  if (ImGui::IsItemHovered()) ImGui::SetTooltip("Open Folder");

  ImGui::SameLine();
  if (ImGui::Button(ICON_MD_FILE_OPEN, ImVec2(button_size, button_size))) {
    auto filename = FileDialogWrapper::ShowOpenFileDialog();
    if (!filename.empty()) {
      ChangeActiveFile(filename);
    }
  }
  if (ImGui::IsItemHovered()) ImGui::SetTooltip("Open File");

  ImGui::SameLine();
  bool can_save = HasActiveFile();
  ImGui::BeginDisabled(!can_save);
  if (ImGui::Button(ICON_MD_SAVE, ImVec2(button_size, button_size))) {
    Save();
  }
  ImGui::EndDisabled();
  if (ImGui::IsItemHovered()) ImGui::SetTooltip("Save File");

  ImGui::SameLine();
  ImGui::Text("|");  // Visual separator
  ImGui::SameLine();

  // Build actions
  ImGui::BeginDisabled(!can_save);
  if (ImGui::Button(ICON_MD_CHECK_CIRCLE, ImVec2(button_size, button_size))) {
    ValidateCurrentFile();
  }
  ImGui::EndDisabled();
  if (ImGui::IsItemHovered()) ImGui::SetTooltip("Validate (Ctrl+B)");

  ImGui::SameLine();
  bool can_apply = can_save && rom_ && rom_->is_loaded();
  ImGui::BeginDisabled(!can_apply);
  if (ImGui::Button(ICON_MD_BUILD, ImVec2(button_size, button_size))) {
    ApplyPatchToRom();
  }
  ImGui::EndDisabled();
  if (ImGui::IsItemHovered()) ImGui::SetTooltip("Apply to ROM (Ctrl+Shift+B)");
}

void AssemblyEditor::DrawFileTabView() {
  if (active_files_.empty()) {
    return;
  }

  if (ImGui::BeginTabBar(
          "##OpenFileTabs",
          ImGuiTabBarFlags_Reorderable | ImGuiTabBarFlags_AutoSelectNewTabs |
              ImGuiTabBarFlags_FittingPolicyScroll)) {
    for (int i = 0; i < active_files_.Size; i++) {
      int file_id = active_files_[i];
      if (file_id >= files_.size()) {
        continue;
      }

      // Extract just the filename from the path
      std::string filename = files_[file_id];
      size_t pos = filename.find_last_of("/\\");
      if (pos != std::string::npos) {
        filename = filename.substr(pos + 1);
      }

      bool is_active = (active_file_id_ == file_id);
      ImGuiTabItemFlags flags = is_active ? ImGuiTabItemFlags_SetSelected : 0;
      bool tab_open = true;

      if (ImGui::BeginTabItem(filename.c_str(), &tab_open, flags)) {
        // When tab is selected, update active file
        if (!is_active) {
          active_file_id_ = file_id;
          current_file_ = util::GetFileName(files_[file_id]);
        }
        ImGui::EndTabItem();
      }

      // Handle tab close
      if (!tab_open) {
        active_files_.erase(active_files_.Data + i);
        if (active_file_id_ == file_id) {
          active_file_id_ = active_files_.empty() ? -1 : active_files_[0];
          if (active_file_id_ >= 0 &&
              active_file_id_ < static_cast<int>(open_files_.size())) {
            current_file_ = util::GetFileName(files_[active_file_id_]);
          } else {
            current_file_.clear();
          }
        }
        i--;
      }
    }
    ImGui::EndTabBar();
  }
}

// =============================================================================
// Legacy Update Methods (kept for backward compatibility)
// =============================================================================

absl::Status AssemblyEditor::Update() {
  if (!active_) {
    return absl::OkStatus();
  }

  // Legacy window-based update - kept for backward compatibility
  // New code should use the panel system via DrawCodeEditor()
  ImGui::Begin("Assembly Editor", &active_, ImGuiWindowFlags_MenuBar);
  DrawCodeEditor();
  ImGui::End();

  // Draw symbol panel as separate window if visible (legacy)
  DrawSymbolPanel();

  return absl::OkStatus();
}

void AssemblyEditor::InlineUpdate() {
  TextEditor* editor = GetActiveEditor();
  auto cpos = editor->GetCursorPosition();
  const char* file_label =
      current_file_.empty() ? "No file" : current_file_.c_str();
  ImGui::Text("%6d/%-6d %6d lines  | %s | %s | %s | %s", cpos.mLine + 1,
              cpos.mColumn + 1, editor->GetTotalLines(),
              editor->IsOverwrite() ? "Ovr" : "Ins",
              editor->CanUndo() ? "*" : " ",
              editor->GetLanguageDefinition().mName.c_str(), file_label);

  editor->Render("##asm_editor", ImVec2(0, 0));
}

void AssemblyEditor::UpdateCodeView() {
  // Deprecated: Use the EditorPanel system instead
  // This method is kept for backward compatibility during transition
  DrawToolbarContent();
  ImGui::Separator();
  DrawFileBrowser();
}

absl::Status AssemblyEditor::Save() {
  if (!HasActiveFile()) {
    return absl::FailedPreconditionError("No active file to save.");
  }

  const std::string& path = files_[active_file_id_];
  std::ofstream file(path);
  if (!file.is_open()) {
    return absl::InvalidArgumentError(
        absl::StrCat("Cannot write file: ", path));
  }

  file << GetActiveEditor()->GetText();
  file.close();
  return absl::OkStatus();
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
  // Lazy load project folder if not already loaded
  if (current_folder_.name.empty() && dependencies_.project && !dependencies_.project->code_folder.empty()) {
    OpenFolder(dependencies_.project->GetAbsolutePath(dependencies_.project->code_folder));
  }

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
  }
  ImGui::EndChild();
}

void AssemblyEditor::DrawFileMenu() {
  if (ImGui::BeginMenu("File")) {
    if (ImGui::MenuItem(ICON_MD_FILE_OPEN " Open", "Ctrl+O")) {
      auto filename = util::FileDialogWrapper::ShowOpenFileDialog();
      if (!filename.empty()) {
        ChangeActiveFile(filename);
      }
    }
    if (ImGui::MenuItem(ICON_MD_SAVE " Save", "Ctrl+S")) {
      Save();
    }
    ImGui::EndMenu();
  }
}

void AssemblyEditor::DrawEditMenu() {
  if (ImGui::BeginMenu("Edit")) {
    if (ImGui::MenuItem(ICON_MD_UNDO " Undo", "Ctrl+Z")) {
      GetActiveEditor()->Undo();
    }
    if (ImGui::MenuItem(ICON_MD_REDO " Redo", "Ctrl+Y")) {
      GetActiveEditor()->Redo();
    }
    ImGui::Separator();
    if (ImGui::MenuItem(ICON_MD_CONTENT_CUT " Cut", "Ctrl+X")) {
      GetActiveEditor()->Cut();
    }
    if (ImGui::MenuItem(ICON_MD_CONTENT_COPY " Copy", "Ctrl+C")) {
      GetActiveEditor()->Copy();
    }
    if (ImGui::MenuItem(ICON_MD_CONTENT_PASTE " Paste", "Ctrl+V")) {
      GetActiveEditor()->Paste();
    }
    ImGui::Separator();
    if (ImGui::MenuItem(ICON_MD_SEARCH " Find", "Ctrl+F")) {
      // TODO: Implement this.
    }
    ImGui::EndMenu();
  }
}

void AssemblyEditor::ChangeActiveFile(const std::string_view& filename) {
  if (filename.empty()) {
    return;
  }

  // Check if file is already open
  for (int i = 0; i < active_files_.Size; ++i) {
    int file_id = active_files_[i];
    if (files_[file_id] == filename) {
      // Optional: Focus window
      active_file_id_ = file_id;
      current_file_ = util::GetFileName(files_[file_id]);
      return;
    }
  }

  // Load file content using utility
  try {
    std::string content = util::LoadFile(std::string(filename));
    int new_file_id = files_.size();
    files_.push_back(std::string(filename));
    active_files_.push_back(new_file_id);

    // Resize open_files_ if needed
    if (new_file_id >= open_files_.size()) {
      open_files_.resize(new_file_id + 1);
    }

    open_files_[new_file_id].SetText(content);
    open_files_[new_file_id].SetLanguageDefinition(GetAssemblyLanguageDef());
    open_files_[new_file_id].SetPalette(TextEditor::GetDarkPalette());
    open_files_[new_file_id].SetShowWhitespaces(false);
    active_file_id_ = new_file_id;
    current_file_ = util::GetFileName(std::string(filename));
  } catch (const std::exception& ex) {
    SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "Error opening file: %s\n",
                 ex.what());
  }
}

absl::Status AssemblyEditor::Cut() {
  GetActiveEditor()->Cut();
  return absl::OkStatus();
}

absl::Status AssemblyEditor::Copy() {
  GetActiveEditor()->Copy();
  return absl::OkStatus();
}

absl::Status AssemblyEditor::Paste() {
  GetActiveEditor()->Paste();
  return absl::OkStatus();
}

absl::Status AssemblyEditor::Undo() {
  GetActiveEditor()->Undo();
  return absl::OkStatus();
}

absl::Status AssemblyEditor::Redo() {
  GetActiveEditor()->Redo();
  return absl::OkStatus();
}

// ============================================================================
// Asar Integration Implementation
// ============================================================================

absl::Status AssemblyEditor::ValidateCurrentFile() {
  if (!HasActiveFile()) {
    return absl::FailedPreconditionError("No file is currently active");
  }

  // Initialize Asar if not already done
  if (!asar_initialized_) {
    auto status = asar_.Initialize();
    if (!status.ok()) {
      return status;
    }
    asar_initialized_ = true;
  }

  // Get the file path
  const std::string& file_path = files_[active_file_id_];

  // Validate the assembly
  auto status = asar_.ValidateAssembly(file_path);

  // Update error markers based on result
  if (!status.ok()) {
    // Get the error messages and show them
    last_errors_.clear();
    last_errors_.push_back(std::string(status.message()));
    // Parse and update error markers
    TextEditor::ErrorMarkers markers;
    // Asar errors typically contain line numbers we can parse
    for (const auto& error : last_errors_) {
      // Simple heuristic: look for "line X" or ":X:" pattern
      size_t line_pos = error.find(':');
      if (line_pos != std::string::npos) {
        size_t num_start = line_pos + 1;
        size_t num_end = error.find(':', num_start);
        if (num_end != std::string::npos) {
          std::string line_str = error.substr(num_start, num_end - num_start);
          try {
            int line = std::stoi(line_str);
            markers[line] = error;
          } catch (...) {
            // Not a line number, skip
          }
        }
      }
    }
    GetActiveEditor()->SetErrorMarkers(markers);
    return status;
  }

  // Clear any previous error markers
  ClearErrorMarkers();
  return absl::OkStatus();
}

absl::Status AssemblyEditor::ApplyPatchToRom() {
  if (!rom_ || !rom_->is_loaded()) {
    return absl::FailedPreconditionError("No ROM is loaded");
  }

  if (!HasActiveFile()) {
    return absl::FailedPreconditionError("No file is currently active");
  }

  // Initialize Asar if not already done
  if (!asar_initialized_) {
    auto status = asar_.Initialize();
    if (!status.ok()) {
      return status;
    }
    asar_initialized_ = true;
  }

  // Get the file path
  const std::string& file_path = files_[active_file_id_];

  // Get ROM data as vector for patching
  std::vector<uint8_t> rom_data = rom_->vector();

  // Apply the patch
  auto result = asar_.ApplyPatch(file_path, rom_data);

  if (!result.ok()) {
    UpdateErrorMarkers(*result);
    return result.status();
  }

  if (result->success) {
    // Update the ROM with the patched data
    rom_->LoadFromData(rom_data);

    // Store symbols for lookup
    symbols_ = asar_.GetSymbolTable();
    last_errors_.clear();
    last_warnings_ = result->warnings;

    // Clear error markers
    ClearErrorMarkers();

    return absl::OkStatus();
  } else {
    UpdateErrorMarkers(*result);
    return absl::InternalError("Patch application failed");
  }
}

void AssemblyEditor::UpdateErrorMarkers(const core::AsarPatchResult& result) {
  last_errors_ = result.errors;
  last_warnings_ = result.warnings;

  if (!HasActiveFile()) {
    return;
  }

  TextEditor::ErrorMarkers markers;

  // Parse error messages to extract line numbers
  // Example Asar output: "asm/main.asm:42: error: Unknown command."
  for (const auto& error : result.errors) {
    try {
      // Simple parsing: look for first two colons numbers
      size_t first_colon = error.find(':');
      if (first_colon != std::string::npos) {
        size_t second_colon = error.find(':', first_colon + 1);
        if (second_colon != std::string::npos) {
          std::string line_str = error.substr(first_colon + 1, second_colon - (first_colon + 1));
          int line = std::stoi(line_str);
          
          // Adjust for 1-based line numbers if necessary (ImGuiColorTextEdit usually uses 1-based in UI but 0-based internally? Or vice versa?)
          // Assuming standard compiler output 1-based, editor usually takes 1-based for markers key.
          markers[line] = error;
        }
      }
    } catch (...) {
      // Ignore parsing errors
    }
  }

  GetActiveEditor()->SetErrorMarkers(markers);
}

void AssemblyEditor::ClearErrorMarkers() {
  last_errors_.clear();

  if (!HasActiveFile()) {
    return;
  }

  TextEditor::ErrorMarkers empty_markers;
  GetActiveEditor()->SetErrorMarkers(empty_markers);
}

void AssemblyEditor::DrawAssembleMenu() {
  if (ImGui::BeginMenu("Assemble")) {
    bool has_active_file = HasActiveFile();
    bool has_rom = (rom_ && rom_->is_loaded());

    if (ImGui::MenuItem(ICON_MD_CHECK_CIRCLE " Validate", "Ctrl+B", false, has_active_file)) {
      auto status = ValidateCurrentFile();
      if (status.ok()) {
        // Show success notification (could add toast notification here)
      }
    }

    if (ImGui::MenuItem(ICON_MD_BUILD " Apply to ROM", "Ctrl+Shift+B", false,
                        has_active_file && has_rom)) {
      auto status = ApplyPatchToRom();
      if (status.ok()) {
        // Show success notification
      }
    }

    if (ImGui::MenuItem(ICON_MD_FILE_UPLOAD " Load External Symbols", nullptr, false)) {
      if (dependencies_.project) {
        std::string sym_file = dependencies_.project->symbols_filename;
        if (!sym_file.empty()) {
          std::string abs_path = dependencies_.project->GetAbsolutePath(sym_file);
          auto status = asar_.LoadSymbolsFromFile(abs_path);
          if (status.ok()) {
            // Copy symbols to local map for display
            symbols_ = asar_.GetSymbolTable();
            if (dependencies_.toast_manager) {
              dependencies_.toast_manager->Show("Successfully loaded external symbols from " + sym_file, ToastType::kSuccess);
            }
          } else {
            if (dependencies_.toast_manager) {
              dependencies_.toast_manager->Show("Failed to load symbols: " + std::string(status.message()), ToastType::kError);
            }
          }
        } else {
           if (dependencies_.toast_manager) {
              dependencies_.toast_manager->Show("Project does not specify a symbols file.", ToastType::kWarning);
            }
        }
      }
    }

    ImGui::Separator();

    if (ImGui::MenuItem(ICON_MD_LIST " Show Symbols", nullptr, show_symbol_panel_)) {
      show_symbol_panel_ = !show_symbol_panel_;
    }

    ImGui::Separator();

    // Show last error/warning count
    ImGui::TextDisabled("Errors: %zu, Warnings: %zu", last_errors_.size(),
                        last_warnings_.size());

    ImGui::EndMenu();
  }

  if (ImGui::BeginMenu("Version")) {
    bool has_version_manager = (dependencies_.version_manager != nullptr);
    if (ImGui::MenuItem(ICON_MD_CAMERA_ALT " Create Snapshot", nullptr, false, has_version_manager)) {
        if (has_version_manager) {
            ImGui::OpenPopup("Create Snapshot");
        }
    }
    
    // Snapshot Dialog
    if (ImGui::BeginPopupModal("Create Snapshot", nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
        static char message[256] = "";
        ImGui::InputText("Message", message, sizeof(message));
        
        if (ImGui::Button("Create", ImVec2(120, 0))) {
            auto result = dependencies_.version_manager->CreateSnapshot(message);
            if (result.ok() && result->success) {
                if (dependencies_.toast_manager) {
                    dependencies_.toast_manager->Show("Snapshot Created: " + result->commit_hash, ToastType::kSuccess);
                }
            } else {
                if (dependencies_.toast_manager) {
                    std::string err = result.ok() ? result->message : std::string(result.status().message());
                    dependencies_.toast_manager->Show("Snapshot Failed: " + err, ToastType::kError);
                }
            }
            ImGui::CloseCurrentPopup();
            message[0] = '\0'; // Reset
        }
        ImGui::SameLine();
        if (ImGui::Button("Cancel", ImVec2(120, 0))) {
            ImGui::CloseCurrentPopup();
        }
        ImGui::EndPopup();
    }

    ImGui::EndMenu();
  }
}

void AssemblyEditor::DrawSymbolPanel() {
  if (!show_symbol_panel_) {
    return;
  }

  ImGui::SetNextWindowSize(ImVec2(350, 400), ImGuiCond_FirstUseEver);
  if (ImGui::Begin("Symbols", &show_symbol_panel_)) {
    if (symbols_.empty()) {
      ImGui::TextDisabled("No symbols loaded.");
      ImGui::TextDisabled("Apply a patch to load symbols.");
    } else {
      // Search filter
      static char filter[256] = "";
      ImGui::InputTextWithHint("##symbol_filter", "Filter symbols...", filter,
                               sizeof(filter));

      ImGui::Separator();

      if (ImGui::BeginChild("##symbol_list", ImVec2(0, 0), true)) {
        for (const auto& [name, symbol] : symbols_) {
          // Apply filter
          if (filter[0] != '\0' &&
              name.find(filter) == std::string::npos) {
            continue;
          }

          ImGui::PushID(name.c_str());
          if (ImGui::Selectable(name.c_str())) {
            // Could jump to symbol definition if line info is available
            // For now, just select it
          }
          ImGui::SameLine(200);
          ImGui::TextDisabled("$%06X", symbol.address);
          ImGui::PopID();
        }
      }
      ImGui::EndChild();
    }
  }
  ImGui::End();
}

}  // namespace yaze::editor
