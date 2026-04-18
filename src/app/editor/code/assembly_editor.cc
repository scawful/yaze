#include "assembly_editor.h"

#include <algorithm>
#include <array>
#include <cstdlib>
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
#include "absl/strings/str_format.h"
#include "absl/strings/str_join.h"
#include "app/editor/code/diagnostics_panel.h"
#include "app/editor/code/panels/assembly_editor_panels.h"
#include "app/editor/editor_manager.h"
#include "app/editor/system/workspace_window_manager.h"
#include "app/editor/ui/toast_manager.h"
#include "app/emu/debug/disassembler.h"
#include "app/emu/mesen/mesen_client_registry.h"
#include "app/gui/core/icons.h"
#include "app/gui/core/style_guard.h"
#include "app/gui/core/ui_helpers.h"
#include "app/gui/widgets/text_editor.h"
#include "app/gui/widgets/themed_widgets.h"
#include "cli/service/agent/tools/project_graph_tool.h"
#include "core/project.h"
#include "core/version_manager.h"
#include "imgui/misc/cpp/imgui_stdlib.h"
#include "rom/snes.h"
#include "util/file_util.h"
#include "util/json.h"

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

bool HasFileExtension(const std::string& name) {
  return absl::StrContains(name, '.');
}

bool IsHiddenName(const std::string& name) {
  return !name.empty() && name[0] == '.';
}

bool IsIgnoredFile(const std::string& name,
                   const std::vector<std::string>& ignored_files) {
  return std::ranges::find(ignored_files, name) != ignored_files.end();
}

bool ShouldSkipDirectory(const std::string& name) {
  static const std::array<const char*, 13> kSkippedDirectories = {
      ".git",       ".context",     ".idea",      ".vscode",   "build",
      "build_ai",   "build_agent",  "build_test", "build-ios", "build-ios-sim",
      "build-wasm", "node_modules", "dist"};
  for (const char* skipped : kSkippedDirectories) {
    if (name == skipped) {
      return true;
    }
  }
  if (name == "out") {
    return true;
  }
  return false;
}

std::string ShellQuote(const std::string& value) {
  std::string quoted = "'";
  for (char ch : value) {
    if (ch == '\'') {
      quoted += "'\\''";
    } else {
      quoted.push_back(ch);
    }
  }
  quoted += "'";
  return quoted;
}

std::optional<std::filesystem::path> FindUpwardPath(
    const std::filesystem::path& start, const std::filesystem::path& relative) {
  std::error_code ec;
  auto current = std::filesystem::absolute(start, ec);
  if (ec) {
    current = start;
  }
  while (!current.empty()) {
    const auto candidate = current / relative;
    if (std::filesystem::exists(candidate, ec) && !ec) {
      return candidate;
    }
    if (current == current.root_path() || current == current.parent_path()) {
      break;
    }
    current = current.parent_path();
  }
  return std::nullopt;
}

bool IsGeneratedBankFile(const std::filesystem::path& path) {
  if (path.extension() != ".asm") {
    return false;
  }
  const std::string name = path.filename().string();
  return absl::StartsWith(name, "bank_");
}

bool LoadJsonFile(const std::string& path, Json* out) {
  if (path.empty()) {
    return false;
  }
  std::ifstream file(path);
  if (!file.is_open()) {
    return false;
  }
  try {
    file >> *out;
    return true;
  } catch (...) {
    return false;
  }
}

int NormalizeLoRomBankIndex(uint32_t address) {
  if ((address & 0xFF0000u) >= 0x800000u) {
    address &= 0x7FFFFFu;
  }
  return static_cast<int>((address >> 16) & 0xFFu);
}

std::optional<int> ParseGeneratedBankIndex(const std::string& path) {
  const std::string name = std::filesystem::path(path).filename().string();
  if (!absl::StartsWith(name, "bank_") || !absl::EndsWith(name, ".asm")) {
    return std::nullopt;
  }
  const std::string hex = name.substr(5, name.size() - 9);
  try {
    return std::stoi(hex, nullptr, 16);
  } catch (...) {
    return std::nullopt;
  }
}

std::optional<uint32_t> ParseHexAddress(const std::string& text) {
  if (text.empty()) {
    return std::nullopt;
  }

  std::string token = text;
  const size_t first = token.find_first_not_of(" \t");
  if (first == std::string::npos) {
    return std::nullopt;
  }
  const size_t last = token.find_last_not_of(" \t");
  token = token.substr(first, last - first + 1);
  if (token.empty()) {
    return std::nullopt;
  }
  if (token[0] == '$') {
    token = token.substr(1);
  } else if (token.size() > 2 && token[0] == '0' &&
             (token[1] == 'x' || token[1] == 'X')) {
    token = token.substr(2);
  }
  try {
    return static_cast<uint32_t>(std::stoul(token, nullptr, 16));
  } catch (...) {
    return std::nullopt;
  }
}

void SortFolderItem(FolderItem* item) {
  if (!item) {
    return;
  }
  std::sort(item->files.begin(), item->files.end());
  std::sort(item->subfolders.begin(), item->subfolders.end(),
            [](const FolderItem& lhs, const FolderItem& rhs) {
              return lhs.name < rhs.name;
            });
  for (auto& subfolder : item->subfolders) {
    SortFolderItem(&subfolder);
  }
}

FolderItem LoadFolder(const std::string& folder) {
  std::vector<std::string> ignored_files;
#if !(defined(__APPLE__) && TARGET_OS_IOS == 1)
  std::ifstream gitignore(folder + "/.gitignore");
  if (gitignore.good()) {
    std::string line;
    while (std::getline(gitignore, line)) {
      if (line.empty() || line[0] == '#' || line[0] == '!') {
        continue;
      }
      ignored_files.push_back(line);
    }
  }
#endif

  FolderItem current_folder;

  std::error_code path_ec;
  std::filesystem::path root_path =
      std::filesystem::weakly_canonical(folder, path_ec);
  if (path_ec) {
    path_ec.clear();
    root_path = std::filesystem::absolute(folder, path_ec);
    if (path_ec) {
      root_path = folder;
    }
  }
  current_folder.name = root_path.string();

  std::error_code root_ec;
  for (const auto& entry :
       std::filesystem::directory_iterator(root_path, root_ec)) {
    if (root_ec) {
      break;
    }

    const std::string entry_name = entry.path().filename().string();
    if (entry_name.empty() || IsHiddenName(entry_name)) {
      continue;
    }

    std::error_code type_ec;
    if (entry.is_regular_file(type_ec)) {
      if (!HasFileExtension(entry_name) ||
          IsIgnoredFile(entry_name, ignored_files)) {
        continue;
      }
      current_folder.files.push_back(entry_name);
      continue;
    }

    if (!entry.is_directory(type_ec) || ShouldSkipDirectory(entry_name)) {
      continue;
    }

    FolderItem folder_item;
    folder_item.name = entry_name;

    std::error_code sub_ec;
    for (const auto& sub_entry :
         std::filesystem::directory_iterator(entry.path(), sub_ec)) {
      if (sub_ec) {
        break;
      }

      const std::string sub_name = sub_entry.path().filename().string();
      if (sub_name.empty() || IsHiddenName(sub_name)) {
        continue;
      }

      std::error_code sub_type_ec;
      if (sub_entry.is_regular_file(sub_type_ec)) {
        if (!HasFileExtension(sub_name) ||
            IsIgnoredFile(sub_name, ignored_files)) {
          continue;
        }
        folder_item.files.push_back(sub_name);
        continue;
      }

      if (!sub_entry.is_directory(sub_type_ec) ||
          ShouldSkipDirectory(sub_name)) {
        continue;
      }

      FolderItem subfolder_item;
      subfolder_item.name = sub_name;
      std::error_code leaf_ec;
      for (const auto& leaf_entry :
           std::filesystem::directory_iterator(sub_entry.path(), leaf_ec)) {
        if (leaf_ec) {
          break;
        }
        const std::string leaf_name = leaf_entry.path().filename().string();
        if (leaf_name.empty() || IsHiddenName(leaf_name)) {
          continue;
        }
        std::error_code leaf_type_ec;
        if (!leaf_entry.is_regular_file(leaf_type_ec)) {
          continue;
        }
        if (!HasFileExtension(leaf_name) ||
            IsIgnoredFile(leaf_name, ignored_files)) {
          continue;
        }
        subfolder_item.files.push_back(leaf_name);
      }
      folder_item.subfolders.push_back(std::move(subfolder_item));
    }

    current_folder.subfolders.push_back(std::move(folder_item));
  }

  SortFolderItem(&current_folder);
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
    const std::string line_str =
        std::string(absl::StripAsciiWhitespace(trimmed.substr(pos + 2)));
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

  const std::string tail =
      std::string(absl::StripAsciiWhitespace(trimmed.substr(last_colon + 1)));
  if (tail.empty()) {
    return std::nullopt;
  }

  const size_t second_last_colon = (last_colon == 0)
                                       ? std::string::npos
                                       : trimmed.rfind(':', last_colon - 1);

  if (second_last_colon != std::string::npos) {
    const std::string file = std::string(
        absl::StripAsciiWhitespace(trimmed.substr(0, second_last_colon)));
    const std::string line_str =
        std::string(absl::StripAsciiWhitespace(trimmed.substr(
            second_last_colon + 1, last_colon - second_last_colon - 1)));
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

  const std::string file =
      std::string(absl::StripAsciiWhitespace(trimmed.substr(0, last_colon)));
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
    const std::string sym =
        std::string(absl::StripAsciiWhitespace(trimmed.substr(pos + 1)));
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

  const std::string file =
      std::string(absl::StripAsciiWhitespace(trimmed.substr(0, last_colon)));
  const std::string sym =
      std::string(absl::StripAsciiWhitespace(trimmed.substr(last_colon + 1)));
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
    if (std::filesystem::exists(p, ec) &&
        std::filesystem::is_regular_file(p, ec)) {
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
      } else if (ShouldSkipDirectory(name)) {
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
      } else if (ShouldSkipDirectory(name)) {
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

  // Register panels with WorkspaceWindowManager using WindowContent instances
  if (!dependencies_.window_manager)
    return;
  auto* window_manager = dependencies_.window_manager;

  // Register Code Editor panel - main text editing
  window_manager->RegisterWindowContent(
      std::make_unique<AssemblyCodeEditorPanel>(
          [this]() { DrawCodeEditor(); }));

  // Register File Browser panel - project file navigation
  window_manager->RegisterWindowContent(
      std::make_unique<AssemblyFileBrowserPanel>(
          [this]() { DrawFileBrowser(); }));

  // Register Symbols panel - symbol table viewer
  window_manager->RegisterWindowContent(std::make_unique<AssemblySymbolsPanel>(
      [this]() { DrawSymbolsContent(); }));

  // Register Build Output panel - errors/warnings
  window_manager->RegisterWindowContent(
      std::make_unique<AssemblyBuildOutputPanel>(
          [this]() { DrawBuildOutput(); }));

  window_manager->RegisterWindowContent(
      std::make_unique<AssemblyDisassemblyPanel>(
          [this]() { DrawDisassemblyContent(); }));

  // Register Toolbar panel - quick actions
  window_manager->RegisterWindowContent(std::make_unique<AssemblyToolbarPanel>(
      [this]() { DrawToolbarContent(); }));
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
    root = dependencies_.project->GetAbsolutePath(
        dependencies_.project->code_folder);
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

  if (auto it = symbol_jump_cache_.find(symbol);
      it != symbol_jump_cache_.end()) {
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
      return absl::NotFoundError(absl::StrCat(
          "Symbol not found in ", file_ref->file_ref, ": ", file_ref->symbol));
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
// Panel Content Drawing (WindowContent System)
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
    OpenFolder(dependencies_.project->GetAbsolutePath(
        dependencies_.project->code_folder));
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
    ImGui::TextWrapped(
        "Apply a patch or load external symbols to populate this list.");
    return;
  }

  // Search filter
  static char filter[256] = "";
  ImGui::SetNextItemWidth(-1);
  ImGui::InputTextWithHint("##symbol_filter",
                           ICON_MD_SEARCH " Filter symbols...", filter,
                           sizeof(filter));
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
        dependencies_.toast_manager->Show("Validation passed!",
                                          ToastType::kSuccess);
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
    if (!last_diagnostics_.empty()) {
      // Structured path — when the backend (z3dk, or future enriched Asar)
      // gave us file:line diagnostics, render via the dedicated panel.
      DiagnosticsPanelCallbacks callbacks;
      callbacks.on_diagnostic_activated = [this](const std::string& file,
                                                 int line, int column) {
        if (!file.empty()) {
          std::string reference = file;
          if (line > 0) {
            reference = absl::StrCat(reference, ":", line);
            if (column > 0) {
              reference = absl::StrCat(reference, ":", column);
            }
          }
          auto status = JumpToReference(reference);
          if (!status.ok() && dependencies_.toast_manager) {
            dependencies_.toast_manager->Show(
                "Failed to open diagnostic location: " +
                    std::string(status.message()),
                ToastType::kError);
          }
          return;
        }
        // TextEditor coords are 0-based; diagnostics are 1-based.
        if (auto* editor = GetActiveEditor()) {
          TextEditor::Coordinates coords(line > 0 ? line - 1 : 0,
                                         column > 0 ? column - 1 : 0);
          editor->SetCursorPosition(coords);
        }
      };
      DrawDiagnosticsPanel(last_diagnostics_, callbacks);
    } else {
      // Legacy flat-string fallback (vanilla Asar without structured output).
      for (const auto& error : last_errors_) {
        gui::StyleColorGuard err_guard(ImGuiCol_Text,
                                       ImVec4(1.0f, 0.4f, 0.4f, 1.0f));
        ImGui::TextWrapped("%s %s", ICON_MD_ERROR, error.c_str());
      }
      for (const auto& warning : last_warnings_) {
        gui::StyleColorGuard warn_guard(ImGuiCol_Text,
                                        ImVec4(1.0f, 0.8f, 0.2f, 1.0f));
        ImGui::TextWrapped("%s %s", ICON_MD_WARNING, warning.c_str());
      }
      if (last_errors_.empty() && last_warnings_.empty()) {
        ImGui::TextDisabled("No build output");
      }
    }
  }
  ImGui::EndChild();
}

std::optional<uint32_t> AssemblyEditor::CurrentDisassemblyBank() const {
  if (auto symbol_it = symbols_.find(disasm_query_);
      symbol_it != symbols_.end()) {
    return (symbol_it->second.address >> 16) & 0xFFu;
  }
  auto parsed = ParseHexAddress(disasm_query_);
  if (!parsed.has_value()) {
    return std::nullopt;
  }
  return (*parsed >> 16) & 0xFFu;
}

std::string AssemblyEditor::ResolveZ3DisasmCommand() const {
  if (const char* env = std::getenv("Z3DISASM_BIN"); env && env[0] != '\0') {
    return std::string(env);
  }

  std::error_code ec;
  const auto cwd = std::filesystem::current_path(ec);
  if (!ec) {
    if (auto script =
            FindUpwardPath(cwd, std::filesystem::path("scripts") / "z3disasm");
        script.has_value()) {
      return script->string();
    }
  }

  return "z3disasm";
}

std::string AssemblyEditor::ResolveZ3DisasmOutputDir() const {
  if (!z3disasm_output_dir_.empty()) {
    return z3disasm_output_dir_;
  }
  if (dependencies_.project) {
    return dependencies_.project->GetZ3dkArtifactPath("z3disasm");
  }
  if (rom_ && rom_->is_loaded() && !rom_->filename().empty()) {
    return (std::filesystem::path(rom_->filename()).parent_path() / "z3disasm")
        .string();
  }
  std::error_code ec;
  return (std::filesystem::current_path(ec) / "z3disasm").string();
}

std::string AssemblyEditor::ResolveZ3DisasmRomPath() const {
  if (rom_ && rom_->is_loaded() && !rom_->filename().empty()) {
    return rom_->filename();
  }
  if (dependencies_.project) {
    if (!dependencies_.project->z3dk_settings.rom_path.empty()) {
      return dependencies_.project->z3dk_settings.rom_path;
    }
    return dependencies_.project->rom_filename;
  }
  return {};
}

int AssemblyEditor::SelectedZ3DisasmBankIndex() const {
  if (auto bank = ParseGeneratedBankIndex(z3disasm_selected_path_);
      bank.has_value()) {
    return *bank;
  }
  return -1;
}

std::string AssemblyEditor::BuildProjectGraphBankQuery() const {
  const int bank = SelectedZ3DisasmBankIndex();
  if (bank < 0) {
    return {};
  }
  return absl::StrFormat("project-graph --query=bank --bank=%02X --format=json",
                         bank);
}

std::string AssemblyEditor::BuildProjectGraphLookupQuery(
    uint32_t address) const {
  return absl::StrFormat(
      "project-graph --query=lookup --address=%06X --format=json", address);
}

absl::Status AssemblyEditor::RunProjectGraphQueryInDrawer(
    const std::vector<std::string>& args, const std::string& title) {
  auto* editor_manager = static_cast<EditorManager*>(dependencies_.custom_data);
  if (!editor_manager || !editor_manager->right_drawer_manager()) {
    return absl::FailedPreconditionError(
        "Right drawer manager is unavailable for project-graph results");
  }

  cli::agent::tools::ProjectGraphTool tool;
  if (dependencies_.project) {
    tool.SetProjectContext(dependencies_.project);
  }
  tool.SetAssemblySymbolTable(&symbols_);
  tool.SetAsarWrapper(&asar_);

  std::string output;
  auto status = tool.Run(args, nullptr, &output);
  if (!status.ok()) {
    return status;
  }

  RightDrawerManager::ToolOutputActions actions;
  actions.on_open_reference = [this](const std::string& reference) {
    JumpToReference(reference).IgnoreError();
  };
  actions.on_open_address = [this](uint32_t address) {
    disasm_query_ = absl::StrFormat("0x%06X", address);
    NavigateDisassemblyQuery().IgnoreError();
  };
  actions.on_open_lookup = [this](uint32_t address) {
    RunProjectGraphQueryInDrawer(
        {"--query=lookup", absl::StrFormat("--address=%06X", address),
         "--format=json"},
        absl::StrFormat("Project Graph Lookup $%06X", address))
        .IgnoreError();
  };

  editor_manager->right_drawer_manager()->SetToolOutput(
      title, absl::StrJoin(args, " "), output, std::move(actions));
  editor_manager->right_drawer_manager()->OpenDrawer(
      RightDrawerManager::DrawerType::kToolOutput);
  return absl::OkStatus();
}

void AssemblyEditor::RefreshSelectedZ3DisassemblyMetadata() {
  z3disasm_source_jumps_.clear();
  z3disasm_hook_jumps_.clear();

  if (!dependencies_.project) {
    return;
  }

  const int selected_bank = SelectedZ3DisasmBankIndex();
  if (selected_bank < 0) {
    return;
  }

  const auto& project = *dependencies_.project;
  Json sourcemap;
  const std::string sourcemap_path =
      !project.z3dk_settings.artifact_paths.sourcemap_json.empty()
          ? project.z3dk_settings.artifact_paths.sourcemap_json
          : project.GetZ3dkArtifactPath("sourcemap.json");
  if (LoadJsonFile(sourcemap_path, &sourcemap) &&
      sourcemap.contains("entries") && sourcemap["entries"].is_array() &&
      sourcemap.contains("files") && sourcemap["files"].is_array()) {
    std::map<int, std::string> files_by_id;
    for (const auto& file : sourcemap["files"]) {
      files_by_id[file.value("id", -1)] = file.value("path", "");
    }

    for (const auto& entry : sourcemap["entries"]) {
      const std::string address_str = entry.value("address", "0x0");
      auto address = ParseHexAddress(address_str);
      if (!address.has_value() ||
          NormalizeLoRomBankIndex(*address) != selected_bank) {
        continue;
      }

      Z3DisasmSourceJump jump;
      jump.address = *address;
      jump.line = entry.value("line", 0);
      jump.file = files_by_id[entry.value("file_id", -1)];
      if (!jump.file.empty()) {
        z3disasm_source_jumps_.push_back(std::move(jump));
      }
    }
    std::sort(z3disasm_source_jumps_.begin(), z3disasm_source_jumps_.end(),
              [](const Z3DisasmSourceJump& lhs, const Z3DisasmSourceJump& rhs) {
                if (lhs.address != rhs.address) {
                  return lhs.address < rhs.address;
                }
                if (lhs.file != rhs.file) {
                  return lhs.file < rhs.file;
                }
                return lhs.line < rhs.line;
              });
  }

  Json hooks;
  const std::string hooks_path =
      !project.z3dk_settings.artifact_paths.hooks_json.empty()
          ? project.z3dk_settings.artifact_paths.hooks_json
          : project.GetZ3dkArtifactPath("hooks.json");
  if (LoadJsonFile(hooks_path, &hooks) && hooks.contains("hooks") &&
      hooks["hooks"].is_array()) {
    for (const auto& hook : hooks["hooks"]) {
      const std::string address_str = hook.value("address", "0x0");
      auto address = ParseHexAddress(address_str);
      if (!address.has_value() ||
          NormalizeLoRomBankIndex(*address) != selected_bank) {
        continue;
      }

      Z3DisasmHookJump jump;
      jump.address = *address;
      jump.size = hook.value("size", 0);
      jump.kind = hook.value("kind", "patch");
      jump.name = hook.value("name", "");
      jump.source = hook.value("source", "");
      z3disasm_hook_jumps_.push_back(std::move(jump));
    }
    std::sort(z3disasm_hook_jumps_.begin(), z3disasm_hook_jumps_.end(),
              [](const Z3DisasmHookJump& lhs, const Z3DisasmHookJump& rhs) {
                if (lhs.address != rhs.address) {
                  return lhs.address < rhs.address;
                }
                return lhs.name < rhs.name;
              });
  }
}

void AssemblyEditor::LoadSelectedZ3DisassemblyFile() {
  z3disasm_selected_contents_.clear();
  z3disasm_selected_path_.clear();
  z3disasm_source_jumps_.clear();
  z3disasm_hook_jumps_.clear();
  if (z3disasm_selected_index_ < 0 ||
      z3disasm_selected_index_ >= static_cast<int>(z3disasm_files_.size())) {
    return;
  }

  z3disasm_selected_path_ = z3disasm_files_[z3disasm_selected_index_];
  std::ifstream file(z3disasm_selected_path_);
  if (!file.is_open()) {
    z3disasm_status_ = absl::StrCat("Failed to open ", z3disasm_selected_path_);
    return;
  }
  z3disasm_selected_contents_.assign(std::istreambuf_iterator<char>(file),
                                     std::istreambuf_iterator<char>());
  RefreshSelectedZ3DisassemblyMetadata();
}

void AssemblyEditor::RefreshZ3DisassemblyFiles() {
  z3disasm_files_.clear();
  const std::filesystem::path output_dir(ResolveZ3DisasmOutputDir());
  std::error_code ec;
  if (!std::filesystem::exists(output_dir, ec) || ec) {
    z3disasm_selected_index_ = -1;
    z3disasm_selected_path_.clear();
    z3disasm_selected_contents_.clear();
    z3disasm_source_jumps_.clear();
    z3disasm_hook_jumps_.clear();
    return;
  }

  std::string previous_selection = z3disasm_selected_path_;
  for (const auto& entry :
       std::filesystem::directory_iterator(output_dir, ec)) {
    if (ec) {
      break;
    }
    if (!entry.is_regular_file()) {
      continue;
    }
    if (IsGeneratedBankFile(entry.path())) {
      z3disasm_files_.push_back(entry.path().string());
    }
  }
  std::sort(z3disasm_files_.begin(), z3disasm_files_.end());

  if (z3disasm_files_.empty()) {
    z3disasm_selected_index_ = -1;
    z3disasm_selected_path_.clear();
    z3disasm_selected_contents_.clear();
    z3disasm_source_jumps_.clear();
    z3disasm_hook_jumps_.clear();
    return;
  }

  z3disasm_selected_index_ = 0;
  if (!previous_selection.empty()) {
    const auto it = std::find(z3disasm_files_.begin(), z3disasm_files_.end(),
                              previous_selection);
    if (it != z3disasm_files_.end()) {
      z3disasm_selected_index_ =
          static_cast<int>(std::distance(z3disasm_files_.begin(), it));
    }
  }
  LoadSelectedZ3DisassemblyFile();
}

void AssemblyEditor::PollZ3DisassemblyTask() {
  const auto snapshot = z3disasm_task_.GetSnapshot();
  if (!snapshot.started) {
    return;
  }
  if (snapshot.running) {
    z3disasm_status_ =
        absl::StrCat("z3disasm running...\n", snapshot.output_tail);
    return;
  }
  if (z3disasm_task_acknowledged_ || !snapshot.finished) {
    return;
  }

  z3disasm_task_acknowledged_ = true;
  if (snapshot.status.ok()) {
    RefreshZ3DisassemblyFiles();
    z3disasm_status_ = absl::StrCat(
        "z3disasm finished.",
        z3disasm_files_.empty()
            ? std::string(" No bank files were generated.")
            : absl::StrFormat(" Loaded %d bank file(s).",
                              static_cast<int>(z3disasm_files_.size())));
  } else {
    z3disasm_status_ =
        absl::StrCat("z3disasm failed: ", snapshot.status.message(), "\n",
                     snapshot.output_tail);
  }
  z3disasm_task_.Wait().IgnoreError();
}

absl::Status AssemblyEditor::GenerateZ3Disassembly() {
  if (const auto snapshot = z3disasm_task_.GetSnapshot(); snapshot.running) {
    return absl::FailedPreconditionError("z3disasm is already running");
  }

  const std::string rom_path = ResolveZ3DisasmRomPath();
  if (rom_path.empty()) {
    return absl::FailedPreconditionError(
        "No ROM path is available for z3disasm");
  }

  const std::string command_path = ResolveZ3DisasmCommand();
  const std::string output_dir = ResolveZ3DisasmOutputDir();
  z3disasm_output_dir_ = output_dir;

  std::error_code ec;
  std::filesystem::create_directories(output_dir, ec);
  for (const auto& entry :
       std::filesystem::directory_iterator(output_dir, ec)) {
    if (!ec && entry.is_regular_file() && IsGeneratedBankFile(entry.path())) {
      std::filesystem::remove(entry.path(), ec);
    }
  }

  std::string command = ShellQuote(command_path);
  command += " --rom ";
  command += ShellQuote(rom_path);
  command += " --out ";
  command += ShellQuote(output_dir);

  if (!z3disasm_all_banks_) {
    command += absl::StrFormat(" --bank-start %02X --bank-end %02X",
                               std::clamp(z3disasm_bank_start_, 0, 0xFF),
                               std::clamp(z3disasm_bank_end_, 0, 0xFF));
  }

  if (dependencies_.project) {
    const std::string symbols_path =
        !dependencies_.project->z3dk_settings.artifact_paths.symbols_mlb.empty()
            ? dependencies_.project->z3dk_settings.artifact_paths.symbols_mlb
            : dependencies_.project->GetZ3dkArtifactPath("symbols.mlb");
    if (!symbols_path.empty() && std::filesystem::exists(symbols_path)) {
      command += " --symbols ";
      command += ShellQuote(symbols_path);
    }

    const std::string hooks_path =
        !dependencies_.project->z3dk_settings.artifact_paths.hooks_json.empty()
            ? dependencies_.project->z3dk_settings.artifact_paths.hooks_json
            : dependencies_.project->GetZ3dkArtifactPath("hooks.json");
    if (!hooks_path.empty() && std::filesystem::exists(hooks_path)) {
      command += " --hooks ";
      command += ShellQuote(hooks_path);
    }
  }

  z3disasm_task_acknowledged_ = false;
  z3disasm_selected_index_ = -1;
  z3disasm_selected_path_.clear();
  z3disasm_selected_contents_.clear();
  z3disasm_source_jumps_.clear();
  z3disasm_hook_jumps_.clear();
  z3disasm_files_.clear();
  z3disasm_status_ = "Launching z3disasm...";
  return z3disasm_task_.Start(command,
                              std::filesystem::current_path().string());
}

absl::Status AssemblyEditor::NavigateDisassemblyQuery() {
  auto symbol_it = symbols_.find(disasm_query_);
  if (symbol_it != symbols_.end()) {
    disasm_query_ = absl::StrCat("0x", absl::Hex(symbol_it->second.address));
    disasm_status_ = absl::StrCat("Resolved symbol ", symbol_it->first);
    return absl::OkStatus();
  }

  auto parsed = ParseHexAddress(disasm_query_);
  if (!parsed.has_value()) {
    return absl::InvalidArgumentError("Enter a SNES address or known symbol");
  }

  disasm_query_ = absl::StrCat("0x", absl::Hex(*parsed));
  disasm_status_ = absl::StrFormat("Showing disassembly at $%06X", *parsed);
  return absl::OkStatus();
}

void AssemblyEditor::DrawDisassemblyContent() {
  PollZ3DisassemblyTask();

  if (z3disasm_output_dir_.empty()) {
    z3disasm_output_dir_ = ResolveZ3DisasmOutputDir();
  }
  if (z3disasm_files_.empty()) {
    RefreshZ3DisassemblyFiles();
  }

  ImGui::TextDisabled(
      "z3disasm-backed bank browser for generated `bank_XX.asm` files.");
  const std::string rom_path = ResolveZ3DisasmRomPath();
  ImGui::TextWrapped("ROM: %s",
                     rom_path.empty() ? "<unavailable>" : rom_path.c_str());
  ImGui::TextWrapped("Output: %s", z3disasm_output_dir_.c_str());

  ImGui::Checkbox("All banks", &z3disasm_all_banks_);
  if (!z3disasm_all_banks_) {
    ImGui::SameLine();
    ImGui::SetNextItemWidth(70.0f);
    ImGui::InputInt("Start", &z3disasm_bank_start_);
    ImGui::SameLine();
    ImGui::SetNextItemWidth(70.0f);
    ImGui::InputInt("End", &z3disasm_bank_end_);
    z3disasm_bank_start_ = std::clamp(z3disasm_bank_start_, 0, 0xFF);
    z3disasm_bank_end_ = std::clamp(z3disasm_bank_end_, 0, 0xFF);
    if (z3disasm_bank_end_ < z3disasm_bank_start_) {
      z3disasm_bank_end_ = z3disasm_bank_start_;
    }
    ImGui::SameLine();
    if (ImGui::SmallButton("Use Query Bank")) {
      if (auto bank = CurrentDisassemblyBank(); bank.has_value()) {
        z3disasm_bank_start_ = static_cast<int>(*bank);
        z3disasm_bank_end_ = static_cast<int>(*bank);
      }
    }
  }

  const auto task_snapshot = z3disasm_task_.GetSnapshot();
  const bool can_generate = !rom_path.empty();
  ImGui::BeginDisabled(!can_generate || task_snapshot.running);
  if (ImGui::Button(ICON_MD_REFRESH " Generate / Refresh Banks")) {
    auto status = GenerateZ3Disassembly();
    if (!status.ok()) {
      z3disasm_status_ = std::string(status.message());
      if (dependencies_.toast_manager) {
        dependencies_.toast_manager->Show(z3disasm_status_, ToastType::kError);
      }
    }
  }
  ImGui::EndDisabled();
  if (task_snapshot.running) {
    ImGui::SameLine();
    if (ImGui::Button(ICON_MD_CANCEL " Cancel")) {
      z3disasm_task_.Cancel();
    }
  }
  if (!can_generate) {
    ImGui::SameLine();
    ImGui::TextDisabled("Load or configure a ROM path first.");
  }

  if (!z3disasm_status_.empty()) {
    ImGui::Spacing();
    ImGui::TextWrapped("%s", z3disasm_status_.c_str());
  }

  if (!task_snapshot.output_tail.empty()) {
    if (ImGui::CollapsingHeader("z3disasm Output")) {
      std::string output_tail = task_snapshot.output_tail;
      ImGui::InputTextMultiline("##z3disasm_output", &output_tail,
                                ImVec2(-1.0f, 80.0f),
                                ImGuiInputTextFlags_ReadOnly);
    }
  }

  ImGui::SeparatorText("Bank Browser");
  if (z3disasm_files_.empty()) {
    ImGui::TextDisabled(
        "No generated bank files yet. Run z3disasm to populate this browser.");
  } else {
    if (ImGui::BeginTable(
            "##z3disasm_browser", 2,
            ImGuiTableFlags_Resizable | ImGuiTableFlags_BordersInnerV)) {
      ImGui::TableSetupColumn("Banks", ImGuiTableColumnFlags_WidthFixed,
                              180.0f);
      ImGui::TableSetupColumn("Preview", ImGuiTableColumnFlags_WidthStretch);
      ImGui::TableNextRow();

      ImGui::TableSetColumnIndex(0);
      if (ImGui::BeginChild("##z3disasm_list", ImVec2(0, 0), true)) {
        for (int i = 0; i < static_cast<int>(z3disasm_files_.size()); ++i) {
          const std::string name =
              std::filesystem::path(z3disasm_files_[i]).filename().string();
          if (ImGui::Selectable(name.c_str(), z3disasm_selected_index_ == i)) {
            z3disasm_selected_index_ = i;
            LoadSelectedZ3DisassemblyFile();
          }
        }
      }
      ImGui::EndChild();

      ImGui::TableSetColumnIndex(1);
      if (ImGui::BeginChild("##z3disasm_preview", ImVec2(0, 0), true)) {
        if (!z3disasm_selected_path_.empty()) {
          const std::string selected_name =
              std::filesystem::path(z3disasm_selected_path_)
                  .filename()
                  .string();
          ImGui::TextUnformatted(selected_name.c_str());
          ImGui::SameLine();
          if (ImGui::SmallButton("Open in Code Editor")) {
            ChangeActiveFile(z3disasm_selected_path_);
          }
          const std::string bank_query = BuildProjectGraphBankQuery();
          if (!bank_query.empty()) {
            ImGui::SameLine();
            if (ImGui::SmallButton("Open Bank Graph")) {
              auto status = RunProjectGraphQueryInDrawer(
                  {"--query=bank",
                   absl::StrFormat("--bank=%02X", SelectedZ3DisasmBankIndex()),
                   "--format=json"},
                  absl::StrFormat("Project Graph Bank $%02X",
                                  SelectedZ3DisasmBankIndex()));
              if (!status.ok()) {
                z3disasm_status_ = std::string(status.message());
              }
            }
            ImGui::SameLine();
            if (ImGui::SmallButton("Copy Bank Query")) {
              ImGui::SetClipboardText(bank_query.c_str());
            }
            ImGui::TextDisabled("%s", bank_query.c_str());
          }
          ImGui::Separator();
          if (!z3disasm_source_jumps_.empty() &&
              ImGui::CollapsingHeader("Source Map Jumps",
                                      ImGuiTreeNodeFlags_DefaultOpen)) {
            for (const auto& jump : z3disasm_source_jumps_) {
              ImGui::PushID(static_cast<int>(jump.address) ^ jump.line);
              const std::string source_label =
                  absl::StrFormat("Source##source_jump_%06X", jump.address);
              const std::string graph_label =
                  absl::StrFormat("Graph##source_graph_%06X", jump.address);
              const std::string copy_label =
                  absl::StrFormat("Copy##source_copy_%06X", jump.address);
              const std::string addr_label =
                  absl::StrFormat("Addr##source_addr_%06X", jump.address);
              if (ImGui::SmallButton(source_label.c_str())) {
                JumpToReference(absl::StrCat(jump.file, ":", jump.line))
                    .IgnoreError();
              }
              ImGui::SameLine();
              if (ImGui::SmallButton(graph_label.c_str())) {
                auto status = RunProjectGraphQueryInDrawer(
                    {"--query=lookup",
                     absl::StrFormat("--address=%06X", jump.address),
                     "--format=json"},
                    absl::StrFormat("Project Graph Lookup $%06X",
                                    jump.address));
                if (!status.ok()) {
                  z3disasm_status_ = std::string(status.message());
                }
              }
              ImGui::SameLine();
              if (ImGui::SmallButton(copy_label.c_str())) {
                const std::string query =
                    BuildProjectGraphLookupQuery(jump.address);
                ImGui::SetClipboardText(query.c_str());
              }
              ImGui::SameLine();
              if (ImGui::SmallButton(addr_label.c_str())) {
                disasm_query_ = absl::StrFormat("0x%06X", jump.address);
                NavigateDisassemblyQuery().IgnoreError();
              }
              ImGui::SameLine();
              ImGui::TextWrapped("$%06X  %s:%d", jump.address,
                                 jump.file.c_str(), jump.line);
              ImGui::PopID();
            }
          }
          if (!z3disasm_hook_jumps_.empty() &&
              ImGui::CollapsingHeader("Hook Jumps",
                                      ImGuiTreeNodeFlags_DefaultOpen)) {
            for (const auto& hook : z3disasm_hook_jumps_) {
              ImGui::PushID(static_cast<int>(hook.address) ^ hook.size ^
                            0x4000);
              const std::string source_label =
                  absl::StrFormat("Source##hook_jump_%06X", hook.address);
              const std::string graph_label =
                  absl::StrFormat("Graph##hook_graph_%06X", hook.address);
              const std::string copy_label =
                  absl::StrFormat("Copy##hook_copy_%06X", hook.address);
              const std::string addr_label =
                  absl::StrFormat("Addr##hook_addr_%06X", hook.address);
              if (!hook.source.empty() &&
                  ImGui::SmallButton(source_label.c_str())) {
                JumpToReference(hook.source).IgnoreError();
              }
              if (!hook.source.empty()) {
                ImGui::SameLine();
              }
              if (ImGui::SmallButton(graph_label.c_str())) {
                auto status = RunProjectGraphQueryInDrawer(
                    {"--query=lookup",
                     absl::StrFormat("--address=%06X", hook.address),
                     "--format=json"},
                    absl::StrFormat("Project Graph Lookup $%06X",
                                    hook.address));
                if (!status.ok()) {
                  z3disasm_status_ = std::string(status.message());
                }
              }
              ImGui::SameLine();
              if (ImGui::SmallButton(copy_label.c_str())) {
                const std::string query =
                    BuildProjectGraphLookupQuery(hook.address);
                ImGui::SetClipboardText(query.c_str());
              }
              ImGui::SameLine();
              if (ImGui::SmallButton(addr_label.c_str())) {
                disasm_query_ = absl::StrFormat("0x%06X", hook.address);
                NavigateDisassemblyQuery().IgnoreError();
              }
              ImGui::SameLine();
              ImGui::TextWrapped("$%06X  %s %s (+%d) %s", hook.address,
                                 hook.kind.c_str(), hook.name.c_str(),
                                 hook.size, hook.source.c_str());
              ImGui::PopID();
            }
          }
          ImGui::Separator();
          ImGui::InputTextMultiline(
              "##z3disasm_text", &z3disasm_selected_contents_,
              ImVec2(-1.0f, -1.0f), ImGuiInputTextFlags_ReadOnly);
        } else {
          ImGui::TextDisabled("Select a generated bank file to preview it.");
        }
      }
      ImGui::EndChild();
      ImGui::EndTable();
    }
  }

  ImGui::SeparatorText("Quick ROM Slice");
  ImGui::TextDisabled("Inline viewer for the currently loaded ROM buffer.");
  ImGui::SetNextItemWidth(220.0f);
  ImGui::InputText("Address or Symbol", &disasm_query_);
  ImGui::SameLine();
  if (ImGui::Button("Go")) {
    auto status = NavigateDisassemblyQuery();
    if (!status.ok()) {
      disasm_status_ = std::string(status.message());
    }
  }
  ImGui::SameLine();
  ImGui::SetNextItemWidth(80.0f);
  ImGui::InputInt("Count", &disasm_instruction_count_);
  if (disasm_instruction_count_ < 1) {
    disasm_instruction_count_ = 1;
  }
  if (disasm_instruction_count_ > 128) {
    disasm_instruction_count_ = 128;
  }

  if (!disasm_status_.empty()) {
    ImGui::TextDisabled("%s", disasm_status_.c_str());
  }
  ImGui::Separator();

  if (!rom_ || !rom_->is_loaded()) {
    ImGui::TextDisabled("Load a ROM to browse disassembly.");
    return;
  }

  uint32_t start_address = 0;
  if (auto symbol_it = symbols_.find(disasm_query_);
      symbol_it != symbols_.end()) {
    start_address = symbol_it->second.address;
  } else {
    auto parsed = ParseHexAddress(disasm_query_);
    if (!parsed.has_value()) {
      ImGui::TextDisabled(
          "Enter a SNES address like 0x008000 or a known label.");
      return;
    }
    start_address = *parsed;
  }

  std::map<uint32_t, std::string> symbols_by_address;
  for (const auto& [name, symbol] : symbols_) {
    symbols_by_address.emplace(symbol.address, name);
  }

  emu::debug::Disassembler65816 disassembler;
  disassembler.SetSymbolResolver([&symbols_by_address](uint32_t address) {
    auto it = symbols_by_address.find(address);
    return it == symbols_by_address.end() ? std::string() : it->second;
  });

  auto read_byte = [this](uint32_t snes_addr) -> uint8_t {
    if (!rom_ || !rom_->is_loaded()) {
      return 0;
    }
    const uint32_t pc_addr = SnesToPc(snes_addr);
    if (pc_addr >= rom_->size()) {
      return 0;
    }
    return rom_->vector()[pc_addr];
  };

  const auto instructions = disassembler.DisassembleRange(
      start_address, static_cast<size_t>(disasm_instruction_count_), read_byte);
  if (instructions.empty()) {
    ImGui::TextDisabled("No disassembly available for this address.");
    return;
  }

  if (ImGui::BeginTable("##assembly_disasm", 3,
                        ImGuiTableFlags_RowBg | ImGuiTableFlags_ScrollY |
                            ImGuiTableFlags_Resizable)) {
    ImGui::TableSetupColumn("Address", ImGuiTableColumnFlags_WidthFixed,
                            110.0f);
    ImGui::TableSetupColumn("Instruction", ImGuiTableColumnFlags_WidthStretch);
    ImGui::TableSetupColumn("Context", ImGuiTableColumnFlags_WidthFixed,
                            140.0f);
    ImGui::TableHeadersRow();

    for (const auto& instruction : instructions) {
      ImGui::PushID(static_cast<int>(instruction.address));
      ImGui::TableNextRow();

      ImGui::TableSetColumnIndex(0);
      const std::string address_label =
          absl::StrFormat("$%06X", instruction.address);
      ImGui::TextUnformatted(address_label.c_str());

      ImGui::TableSetColumnIndex(1);
      if (auto label_it = symbols_by_address.find(instruction.address);
          label_it != symbols_by_address.end()) {
        ImGui::TextColored(ImVec4(0.75f, 0.85f, 1.0f, 1.0f),
                           "%s:", label_it->second.c_str());
      }
      ImGui::TextWrapped("%s", instruction.full_text.c_str());

      ImGui::TableSetColumnIndex(2);
      if (instruction.branch_target != 0) {
        auto target_it = symbols_by_address.find(instruction.branch_target);
        if (target_it != symbols_by_address.end()) {
          if (ImGui::SmallButton(target_it->second.c_str())) {
            JumpToSymbolDefinition(target_it->second).IgnoreError();
          }
        } else {
          ImGui::Text("$%06X", instruction.branch_target);
        }
      } else {
        ImGui::TextDisabled("-");
      }
      ImGui::PopID();
    }
    ImGui::EndTable();
  }
}

void AssemblyEditor::DrawToolbarContent() {
  float button_size = 32.0f;

  if (ImGui::Button(ICON_MD_FOLDER_OPEN, ImVec2(button_size, button_size))) {
    auto folder = FileDialogWrapper::ShowOpenFolderDialog();
    if (!folder.empty()) {
      current_folder_ = LoadFolder(folder);
    }
  }
  if (ImGui::IsItemHovered())
    ImGui::SetTooltip("Open Folder");

  ImGui::SameLine();
  if (ImGui::Button(ICON_MD_FILE_OPEN, ImVec2(button_size, button_size))) {
    auto filename = FileDialogWrapper::ShowOpenFileDialog();
    if (!filename.empty()) {
      ChangeActiveFile(filename);
    }
  }
  if (ImGui::IsItemHovered())
    ImGui::SetTooltip("Open File");

  ImGui::SameLine();
  bool can_save = HasActiveFile();
  ImGui::BeginDisabled(!can_save);
  if (ImGui::Button(ICON_MD_SAVE, ImVec2(button_size, button_size))) {
    Save();
  }
  ImGui::EndDisabled();
  if (ImGui::IsItemHovered())
    ImGui::SetTooltip("Save File");

  ImGui::SameLine();
  ImGui::Text("|");  // Visual separator
  ImGui::SameLine();

  // Build actions
  ImGui::BeginDisabled(!can_save);
  if (ImGui::Button(ICON_MD_CHECK_CIRCLE, ImVec2(button_size, button_size))) {
    ValidateCurrentFile();
  }
  ImGui::EndDisabled();
  if (ImGui::IsItemHovered())
    ImGui::SetTooltip("Validate (Ctrl+B)");

  ImGui::SameLine();
  bool can_apply = can_save && rom_ && rom_->is_loaded();
  ImGui::BeginDisabled(!can_apply);
  if (ImGui::Button(ICON_MD_BUILD, ImVec2(button_size, button_size))) {
    ApplyPatchToRom();
  }
  ImGui::EndDisabled();
  if (ImGui::IsItemHovered())
    ImGui::SetTooltip("Apply to ROM (Ctrl+Shift+B)");
}

void AssemblyEditor::DrawFileTabView() {
  if (active_files_.empty()) {
    return;
  }

  if (gui::BeginThemedTabBar("##OpenFileTabs",
                             ImGuiTabBarFlags_Reorderable |
                                 ImGuiTabBarFlags_AutoSelectNewTabs |
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
    gui::EndThemedTabBar();
  }
}

// =============================================================================
// Legacy Update Methods (kept for backward compatibility)
// =============================================================================

absl::Status AssemblyEditor::Update() {
  if (!active_) {
    return absl::OkStatus();
  }

  if (dependencies_.window_manager != nullptr) {
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
  // Deprecated: Use the WindowContent system instead
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
  if (current_folder_.name.empty() && dependencies_.project &&
      !dependencies_.project->code_folder.empty()) {
    OpenFolder(dependencies_.project->GetAbsolutePath(
        dependencies_.project->code_folder));
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

#ifdef YAZE_WITH_Z3DK
core::Z3dkAssembleOptions AssemblyEditor::BuildZ3dkAssembleOptions() const {
  core::Z3dkAssembleOptions options;
  if (!dependencies_.project) {
    return options;
  }

  const auto& z3dk = dependencies_.project->z3dk_settings;
  options.include_paths = z3dk.include_paths;
  options.defines = z3dk.defines;
  options.warn_unused_symbols = z3dk.warn_unused_symbols;
  options.warn_branch_outside_bank = z3dk.warn_branch_outside_bank;
  options.warn_unknown_width = z3dk.warn_unknown_width;
  options.warn_org_collision = z3dk.warn_org_collision;
  options.warn_unauthorized_hook = z3dk.warn_unauthorized_hook;
  options.warn_stack_balance = z3dk.warn_stack_balance;
  options.warn_hook_return = z3dk.warn_hook_return;
  for (const auto& range : z3dk.prohibited_memory_ranges) {
    options.prohibited_memory_ranges.push_back(
        {.start = range.start, .end = range.end, .reason = range.reason});
  }

  auto append_unique = [&options](const std::string& path) {
    if (path.empty()) {
      return;
    }
    if (std::find(options.include_paths.begin(), options.include_paths.end(),
                  path) == options.include_paths.end()) {
      options.include_paths.push_back(path);
    }
  };

  append_unique(dependencies_.project->code_folder);
  if (HasActiveFile()) {
    append_unique(
        std::filesystem::path(files_[active_file_id_]).parent_path().string());
  }

  if (rom_ && rom_->is_loaded() && !rom_->filename().empty()) {
    options.hooks_rom_path = rom_->filename();
  } else if (!z3dk.rom_path.empty()) {
    options.hooks_rom_path = z3dk.rom_path;
  }

  return options;
}

void AssemblyEditor::ExportZ3dkArtifacts(const core::AsarPatchResult& result,
                                         bool sync_mesen_symbols) {
  if (!dependencies_.project) {
    return;
  }

  const auto& project = *dependencies_.project;
  const auto& configured = project.z3dk_settings.artifact_paths;
  const std::string symbols_path =
      configured.symbols_mlb.empty()
          ? project.GetZ3dkArtifactPath("symbols.mlb")
          : configured.symbols_mlb;
  const std::string sourcemap_path =
      configured.sourcemap_json.empty()
          ? project.GetZ3dkArtifactPath("sourcemap.json")
          : configured.sourcemap_json;
  const std::string annotations_path =
      configured.annotations_json.empty()
          ? project.GetZ3dkArtifactPath("annotations.json")
          : configured.annotations_json;
  const std::string hooks_path = configured.hooks_json.empty()
                                     ? project.GetZ3dkArtifactPath("hooks.json")
                                     : configured.hooks_json;
  const std::string lint_path = configured.lint_json.empty()
                                    ? project.GetZ3dkArtifactPath("lint.json")
                                    : configured.lint_json;
  auto write_artifact = [](const std::string& path,
                           const std::string& content) {
    if (path.empty() || content.empty()) {
      return;
    }
    std::error_code ec;
    std::filesystem::create_directories(
        std::filesystem::path(path).parent_path(), ec);
    std::ofstream file(path, std::ios::binary | std::ios::trunc);
    if (!file.is_open()) {
      return;
    }
    file << content;
  };

  write_artifact(symbols_path, result.symbols_mlb);
  write_artifact(sourcemap_path, result.sourcemap_json);
  write_artifact(annotations_path, result.annotations_json);
  write_artifact(hooks_path, result.hooks_json);
  write_artifact(lint_path, result.lint_json);

  if (!sync_mesen_symbols || symbols_path.empty() ||
      result.symbols_mlb.empty()) {
    return;
  }

  auto client = emu::mesen::MesenClientRegistry::GetOrCreate();
  if (!client->IsConnected()) {
    client->Connect().IgnoreError();
  }
  if (!client->IsConnected()) {
    return;
  }

  auto status = client->LoadSymbolsFile(symbols_path);
  if (!status.ok()) {
    if (dependencies_.toast_manager) {
      dependencies_.toast_manager->Show(
          "Failed to sync Mesen2 symbols: " + std::string(status.message()),
          ToastType::kWarning);
    }
  } else if (dependencies_.toast_manager) {
    dependencies_.toast_manager->Show(
        "Synced symbols to Mesen2 from " + symbols_path, ToastType::kSuccess);
  }
}
#endif

// ============================================================================
// Asar Integration Implementation
// ============================================================================

absl::Status AssemblyEditor::ValidateCurrentFile() {
  if (!HasActiveFile()) {
    return absl::FailedPreconditionError("No file is currently active");
  }

  const std::string& file_path = files_[active_file_id_];

#ifdef YAZE_WITH_Z3DK
  const auto options = BuildZ3dkAssembleOptions();
  // z3dk path: validate via scratch assemble; structured diagnostics are
  // populated natively. We reuse ApplyPatch against a throwaway buffer so
  // we get the full AsarPatchResult (including symbols) back.
  std::vector<uint8_t> scratch;
  auto result_or = z3dk_.ApplyPatch(file_path, scratch, options);
  if (!result_or.ok()) {
    last_errors_.clear();
    last_errors_.push_back(std::string(result_or.status().message()));
    last_warnings_.clear();
    last_diagnostics_.clear();
    TextEditor::ErrorMarkers empty_markers;
    GetActiveEditor()->SetErrorMarkers(empty_markers);
    return result_or.status();
  }
  UpdateErrorMarkers(*result_or);
  if (!result_or->success) {
    return absl::InternalError("Assembly validation failed");
  }
  ExportZ3dkArtifacts(*result_or, false);
  ClearErrorMarkers();
  return absl::OkStatus();
#else
  // Initialize Asar if not already done
  if (!asar_initialized_) {
    auto status = asar_.Initialize();
    if (!status.ok()) {
      return status;
    }
    asar_initialized_ = true;
  }

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
#endif
}

absl::Status AssemblyEditor::ApplyPatchToRom() {
  if (!rom_ || !rom_->is_loaded()) {
    return absl::FailedPreconditionError("No ROM is loaded");
  }

  if (!HasActiveFile()) {
    return absl::FailedPreconditionError("No file is currently active");
  }

  const std::string& file_path = files_[active_file_id_];
  std::vector<uint8_t> rom_data = rom_->vector();

#ifdef YAZE_WITH_Z3DK
  const auto options = BuildZ3dkAssembleOptions();
  auto result = z3dk_.ApplyPatch(file_path, rom_data, options);
  if (!result.ok()) {
    last_errors_.clear();
    last_errors_.push_back(std::string(result.status().message()));
    last_warnings_.clear();
    last_diagnostics_.clear();
    TextEditor::ErrorMarkers empty_markers;
    GetActiveEditor()->SetErrorMarkers(empty_markers);
    return result.status();
  }
  UpdateErrorMarkers(*result);
  if (!result->success) {
    return absl::InternalError("Patch application failed");
  }
  rom_->LoadFromData(rom_data);
  symbols_ = z3dk_.GetSymbolTable();
  ExportZ3dkArtifacts(*result, true);
  ClearErrorMarkers();
  return absl::OkStatus();
#else
  // Initialize Asar if not already done
  if (!asar_initialized_) {
    auto status = asar_.Initialize();
    if (!status.ok()) {
      return status;
    }
    asar_initialized_ = true;
  }

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
#endif
}

void AssemblyEditor::UpdateErrorMarkers(const core::AsarPatchResult& result) {
  last_errors_ = result.errors;
  last_warnings_ = result.warnings;
  last_diagnostics_ = result.structured_diagnostics;

  if (!HasActiveFile()) {
    return;
  }

  TextEditor::ErrorMarkers markers;

  // Prefer the native structured diagnostics when available — they carry
  // line numbers directly, no string parsing required. Fall back to the
  // flat error strings only when the backend did not populate structured
  // diagnostics (e.g., pre-M3 Asar path with a malformed error line).
  if (!result.structured_diagnostics.empty()) {
    for (const auto& d : result.structured_diagnostics) {
      if (d.line > 0 &&
          d.severity == core::AssemblyDiagnosticSeverity::kError) {
        markers[d.line] = d.message;
      }
    }
  } else {
    // Legacy fallback: parse "file:line:..." out of the flat strings.
    for (const auto& error : result.errors) {
      try {
        size_t first_colon = error.find(':');
        if (first_colon != std::string::npos) {
          size_t second_colon = error.find(':', first_colon + 1);
          if (second_colon != std::string::npos) {
            std::string line_str =
                error.substr(first_colon + 1, second_colon - (first_colon + 1));
            int line = std::stoi(line_str);
            markers[line] = error;
          }
        }
      } catch (...) {
        // Ignore parsing errors
      }
    }
  }

  GetActiveEditor()->SetErrorMarkers(markers);
}

void AssemblyEditor::ClearErrorMarkers() {
  last_errors_.clear();
  last_diagnostics_.clear();

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

    if (ImGui::MenuItem(ICON_MD_CHECK_CIRCLE " Validate", "Ctrl+B", false,
                        has_active_file)) {
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

    if (ImGui::MenuItem(ICON_MD_FILE_UPLOAD " Load External Symbols", nullptr,
                        false)) {
      if (dependencies_.project) {
        std::string sym_file = dependencies_.project->symbols_filename;
        if (dependencies_.project->HasZ3dkConfig() &&
            !dependencies_.project->z3dk_settings.artifact_paths.symbols_mlb
                 .empty()) {
          sym_file =
              dependencies_.project->z3dk_settings.artifact_paths.symbols_mlb;
        } else if (sym_file.empty() || !std::filesystem::exists(sym_file)) {
          sym_file = dependencies_.project->GetZ3dkArtifactPath("symbols.mlb");
        }
        if (!sym_file.empty()) {
          std::string abs_path = sym_file;
          if (!std::filesystem::path(abs_path).is_absolute()) {
            abs_path = dependencies_.project->GetAbsolutePath(sym_file);
          }
          auto status = asar_.LoadSymbolsFromFile(abs_path);
          if (status.ok()) {
            // Copy symbols to local map for display
            symbols_ = asar_.GetSymbolTable();
            if (dependencies_.toast_manager) {
              dependencies_.toast_manager->Show(
                  "Successfully loaded external symbols from " + sym_file,
                  ToastType::kSuccess);
            }
          } else {
            if (dependencies_.toast_manager) {
              dependencies_.toast_manager->Show(
                  "Failed to load symbols: " + std::string(status.message()),
                  ToastType::kError);
            }
          }
        } else {
          if (dependencies_.toast_manager) {
            dependencies_.toast_manager->Show(
                "Project does not specify a symbols file.",
                ToastType::kWarning);
          }
        }
      }
    }

    ImGui::Separator();

    if (ImGui::MenuItem(ICON_MD_LIST " Show Symbols", nullptr,
                        show_symbol_panel_)) {
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
    if (ImGui::MenuItem(ICON_MD_CAMERA_ALT " Create Snapshot", nullptr, false,
                        has_version_manager)) {
      if (has_version_manager) {
        ImGui::OpenPopup("Create Snapshot");
      }
    }

    // Snapshot Dialog
    if (ImGui::BeginPopupModal("Create Snapshot", nullptr,
                               ImGuiWindowFlags_AlwaysAutoResize)) {
      static char message[256] = "";
      ImGui::InputText("Message", message, sizeof(message));

      if (ImGui::Button("Create", ImVec2(120, 0))) {
        auto result = dependencies_.version_manager->CreateSnapshot(message);
        if (result.ok() && result->success) {
          if (dependencies_.toast_manager) {
            dependencies_.toast_manager->Show(
                "Snapshot Created: " + result->commit_hash,
                ToastType::kSuccess);
          }
        } else {
          if (dependencies_.toast_manager) {
            std::string err = result.ok()
                                  ? result->message
                                  : std::string(result.status().message());
            dependencies_.toast_manager->Show("Snapshot Failed: " + err,
                                              ToastType::kError);
          }
        }
        ImGui::CloseCurrentPopup();
        message[0] = '\0';  // Reset
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
          if (filter[0] != '\0' && name.find(filter) == std::string::npos) {
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
