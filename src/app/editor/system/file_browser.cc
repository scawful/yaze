#include "app/editor/system/file_browser.h"

#include <algorithm>
#include <fstream>
#include <regex>

#if defined(__APPLE__)
#include <TargetConditionals.h>
#endif

#include "absl/strings/str_cat.h"
#include "absl/strings/str_split.h"
#include "app/gui/core/icons.h"
#include "app/gui/core/style.h"
#include "app/gui/core/style_guard.h"
#include "app/gui/core/theme_manager.h"
#include "app/gui/core/ui_helpers.h"
#include "imgui/imgui.h"

namespace yaze {
namespace editor {

namespace fs = std::filesystem;

// ============================================================================
// GitignoreParser Implementation
// ============================================================================

void GitignoreParser::LoadFromFile(const std::string& gitignore_path) {
  std::ifstream file(gitignore_path);
  if (!file.is_open()) {
    return;
  }

  std::string line;
  while (std::getline(file, line)) {
    // Skip empty lines and comments
    if (line.empty() || line[0] == '#') {
      continue;
    }

    // Trim whitespace
    size_t start = line.find_first_not_of(" \t");
    size_t end = line.find_last_not_of(" \t\r\n");
    if (start == std::string::npos || end == std::string::npos) {
      continue;
    }
    line = line.substr(start, end - start + 1);

    if (!line.empty()) {
      AddPattern(line);
    }
  }
}

void GitignoreParser::AddPattern(const std::string& pattern) {
  Pattern p;
  p.pattern = pattern;

  // Check for negation
  if (!pattern.empty() && pattern[0] == '!') {
    p.is_negation = true;
    p.pattern = pattern.substr(1);
  }

  // Check for directory-only
  if (!p.pattern.empty() && p.pattern.back() == '/') {
    p.directory_only = true;
    p.pattern.pop_back();
  }

  // Remove leading slash (anchors to root, but we match anywhere for simplicity)
  if (!p.pattern.empty() && p.pattern[0] == '/') {
    p.pattern = p.pattern.substr(1);
  }

  patterns_.push_back(p);
}

bool GitignoreParser::IsIgnored(const std::string& path,
                                bool is_directory) const {
  // Extract just the filename for simple patterns
  fs::path filepath(path);
  std::string filename = filepath.filename().string();

  bool ignored = false;

  for (const auto& pattern : patterns_) {
    // Directory-only patterns only match directories
    if (pattern.directory_only && !is_directory) {
      continue;
    }

    if (MatchPattern(filename, pattern) || MatchPattern(path, pattern)) {
      ignored = !pattern.is_negation;
    }
  }

  return ignored;
}

void GitignoreParser::Clear() { patterns_.clear(); }

bool GitignoreParser::MatchPattern(const std::string& path,
                                   const Pattern& pattern) const {
  return MatchGlob(path, pattern.pattern);
}

bool GitignoreParser::MatchGlob(const std::string& text,
                                const std::string& pattern) const {
  // Simple glob matching with * wildcard
  size_t text_pos = 0;
  size_t pattern_pos = 0;
  size_t star_pos = std::string::npos;
  size_t text_backup = 0;

  while (text_pos < text.length()) {
    if (pattern_pos < pattern.length() &&
        (pattern[pattern_pos] == text[text_pos] || pattern[pattern_pos] == '?')) {
      // Characters match or single-char wildcard
      text_pos++;
      pattern_pos++;
    } else if (pattern_pos < pattern.length() && pattern[pattern_pos] == '*') {
      // Multi-char wildcard - remember position
      star_pos = pattern_pos;
      text_backup = text_pos;
      pattern_pos++;
    } else if (star_pos != std::string::npos) {
      // Mismatch after wildcard - backtrack
      pattern_pos = star_pos + 1;
      text_backup++;
      text_pos = text_backup;
    } else {
      // No match
      return false;
    }
  }

  // Check remaining pattern characters (should only be wildcards)
  while (pattern_pos < pattern.length() && pattern[pattern_pos] == '*') {
    pattern_pos++;
  }

  return pattern_pos == pattern.length();
}

// ============================================================================
// FileBrowser Implementation
// ============================================================================

void FileBrowser::SetRootPath(const std::string& path) {
  if (path.empty()) {
    root_path_.clear();
    root_entry_ = FileEntry{};
    needs_refresh_ = true;
    return;
  }

  std::error_code ec;
  fs::path resolved_path;

  if (fs::path(path).is_relative()) {
    resolved_path = fs::absolute(path, ec);
  } else {
    resolved_path = path;
  }

  if (ec || !fs::exists(resolved_path, ec) ||
      !fs::is_directory(resolved_path, ec)) {
    // Invalid path - keep current state
    return;
  }

  root_path_ = resolved_path.string();
  needs_refresh_ = true;

  // Load .gitignore if present
  if (respect_gitignore_) {
    gitignore_parser_.Clear();

    // Load .gitignore from root
    fs::path gitignore = resolved_path / ".gitignore";
#if !(defined(__APPLE__) && TARGET_OS_IOS == 1)
    if (fs::exists(gitignore, ec)) {
      gitignore_parser_.LoadFromFile(gitignore.string());
    }
#else
    // iOS: avoid synchronous reads from iCloud Drive during project open.
    // File provider backed reads can block and trigger watchdog termination.
    (void)gitignore;
#endif

    // Also add common default ignores
    gitignore_parser_.AddPattern("node_modules/");
    gitignore_parser_.AddPattern("build/");
    gitignore_parser_.AddPattern(".git/");
    gitignore_parser_.AddPattern("__pycache__/");
    gitignore_parser_.AddPattern("*.pyc");
    gitignore_parser_.AddPattern(".DS_Store");
    gitignore_parser_.AddPattern("Thumbs.db");
  }
}

void FileBrowser::Refresh() {
  if (root_path_.empty()) {
    return;
  }

  root_entry_ = FileEntry{};
  root_entry_.name = fs::path(root_path_).filename().string();
  root_entry_.full_path = root_path_;
  root_entry_.is_directory = true;
  root_entry_.is_expanded = true;
  root_entry_.file_type = FileEntry::FileType::kDirectory;

  file_count_ = 0;
  directory_count_ = 0;

  ScanDirectory(fs::path(root_path_), root_entry_);
  needs_refresh_ = false;
}

void FileBrowser::ScanDirectory(const fs::path& path, FileEntry& parent,
                                int depth) {
  if (depth > kMaxDepth) {
    return;
  }

  std::error_code ec;
  std::vector<FileEntry> entries;

  for (const auto& entry : fs::directory_iterator(
           path, fs::directory_options::skip_permission_denied, ec)) {
    if (ec) {
      continue;
    }

    // Check entry count limit
    if (file_count_ + directory_count_ >= kMaxEntries) {
      break;
    }

    fs::path entry_path = entry.path();
    std::string filename = entry_path.filename().string();
    bool is_dir = entry.is_directory(ec);

    // Apply filters
    if (!ShouldShow(entry_path, is_dir)) {
      continue;
    }

    // Check gitignore
    if (respect_gitignore_) {
      std::string relative_path =
          fs::relative(entry_path, fs::path(root_path_), ec).string();
      if (!ec && gitignore_parser_.IsIgnored(relative_path, is_dir)) {
        continue;
      }
    }

    FileEntry fe;
    fe.name = filename;
    fe.full_path = entry_path.string();
    fe.is_directory = is_dir;
    fe.file_type = is_dir ? FileEntry::FileType::kDirectory
                          : DetectFileType(filename);

    if (is_dir) {
      directory_count_++;
      // Recursively scan subdirectories
      ScanDirectory(entry_path, fe, depth + 1);
    } else {
      file_count_++;
    }

    entries.push_back(std::move(fe));
  }

  // Sort: directories first, then alphabetically
  std::sort(entries.begin(), entries.end(), [](const FileEntry& a, const FileEntry& b) {
    if (a.is_directory != b.is_directory) {
      return a.is_directory;  // Directories first
    }
    return a.name < b.name;
  });

  parent.children = std::move(entries);
}

bool FileBrowser::ShouldShow(const fs::path& path, bool is_directory) const {
  std::string filename = path.filename().string();

  // Hide dotfiles unless explicitly enabled
  if (!show_hidden_files_ && !filename.empty() && filename[0] == '.') {
    return false;
  }

  // Apply file filter (only for files, not directories)
  if (!is_directory && !file_filter_.empty()) {
    return MatchesFilter(filename);
  }

  return true;
}

bool FileBrowser::MatchesFilter(const std::string& filename) const {
  if (file_filter_.empty()) {
    return true;
  }

  // Extract extension
  size_t dot_pos = filename.rfind('.');
  if (dot_pos == std::string::npos) {
    return false;
  }

  std::string ext = filename.substr(dot_pos);
  // Convert to lowercase for comparison
  std::transform(ext.begin(), ext.end(), ext.begin(), ::tolower);

  return file_filter_.count(ext) > 0;
}

void FileBrowser::SetFileFilter(const std::vector<std::string>& extensions) {
  file_filter_.clear();
  for (const auto& ext : extensions) {
    std::string lower_ext = ext;
    std::transform(lower_ext.begin(), lower_ext.end(), lower_ext.begin(),
                   ::tolower);
    // Ensure extension starts with dot
    if (!lower_ext.empty() && lower_ext[0] != '.') {
      lower_ext = "." + lower_ext;
    }
    file_filter_.insert(lower_ext);
  }
  needs_refresh_ = true;
}

void FileBrowser::ClearFileFilter() {
  file_filter_.clear();
  needs_refresh_ = true;
}

FileEntry::FileType FileBrowser::DetectFileType(
    const std::string& filename) const {
  // Extract extension
  size_t dot_pos = filename.rfind('.');
  if (dot_pos == std::string::npos) {
    return FileEntry::FileType::kUnknown;
  }

  std::string ext = filename.substr(dot_pos);
  std::transform(ext.begin(), ext.end(), ext.begin(), ::tolower);

  // Assembly files
  if (ext == ".asm" || ext == ".s" || ext == ".65c816") {
    return FileEntry::FileType::kAssembly;
  }

  // Source files
  if (ext == ".cc" || ext == ".cpp" || ext == ".c" || ext == ".py" ||
      ext == ".js" || ext == ".ts" || ext == ".rs" || ext == ".go") {
    return FileEntry::FileType::kSource;
  }

  // Header files
  if (ext == ".h" || ext == ".hpp" || ext == ".hxx") {
    return FileEntry::FileType::kHeader;
  }

  // Text files
  if (ext == ".txt" || ext == ".md" || ext == ".rst") {
    return FileEntry::FileType::kText;
  }

  // Config files
  if (ext == ".cfg" || ext == ".ini" || ext == ".conf" || ext == ".yaml" ||
      ext == ".yml" || ext == ".toml") {
    return FileEntry::FileType::kConfig;
  }

  // JSON
  if (ext == ".json") {
    return FileEntry::FileType::kJson;
  }

  // Images
  if (ext == ".png" || ext == ".jpg" || ext == ".jpeg" || ext == ".gif" ||
      ext == ".bmp") {
    return FileEntry::FileType::kImage;
  }

  // Binary
  if (ext == ".bin" || ext == ".sfc" || ext == ".smc" || ext == ".rom") {
    return FileEntry::FileType::kBinary;
  }

  return FileEntry::FileType::kUnknown;
}

const char* FileBrowser::GetFileIcon(FileEntry::FileType type) const {
  switch (type) {
    case FileEntry::FileType::kDirectory:
      return ICON_MD_FOLDER;
    case FileEntry::FileType::kAssembly:
      return ICON_MD_MEMORY;
    case FileEntry::FileType::kSource:
      return ICON_MD_CODE;
    case FileEntry::FileType::kHeader:
      return ICON_MD_DEVELOPER_BOARD;
    case FileEntry::FileType::kText:
      return ICON_MD_DESCRIPTION;
    case FileEntry::FileType::kConfig:
      return ICON_MD_SETTINGS;
    case FileEntry::FileType::kJson:
      return ICON_MD_DATA_OBJECT;
    case FileEntry::FileType::kImage:
      return ICON_MD_IMAGE;
    case FileEntry::FileType::kBinary:
      return ICON_MD_HEXAGON;
    default:
      return ICON_MD_INSERT_DRIVE_FILE;
  }
}

void FileBrowser::Draw() {
  if (root_path_.empty()) {
    ImGui::TextDisabled("No folder selected");
    if (ImGui::Button(ICON_MD_FOLDER_OPEN " Open Folder...")) {
      // Note: Actual folder dialog should be handled externally
      // via the callback or by the host component
    }
    return;
  }

  if (needs_refresh_) {
    Refresh();
  }

  // Header with folder name and refresh button
  ImGui::PushStyleColor(ImGuiCol_Text, gui::GetTextSecondaryVec4());
  ImGui::Text("%s", ICON_MD_FOLDER_OPEN);
  ImGui::PopStyleColor();
  ImGui::SameLine();
  ImGui::Text("%s", root_entry_.name.c_str());
  ImGui::SameLine(ImGui::GetContentRegionAvail().x - 24.0f);
  if (ImGui::SmallButton(ICON_MD_REFRESH)) {
    needs_refresh_ = true;
  }
  if (ImGui::IsItemHovered()) {
    ImGui::SetTooltip("Refresh file list");
  }

  ImGui::Separator();

  // File count
  ImGui::PushStyleColor(ImGuiCol_Text, gui::GetTextDisabledVec4());
  ImGui::Text("%zu files, %zu folders", file_count_, directory_count_);
  ImGui::PopStyleColor();

  ImGui::Spacing();

  // Tree view
  ImGui::BeginChild("##FileTree", ImVec2(0, 0), false);
  for (auto& child : root_entry_.children) {
    DrawEntry(child);
  }
  ImGui::EndChild();
}

void FileBrowser::DrawCompact() {
  if (root_path_.empty()) {
    ImGui::TextDisabled("No folder");
    return;
  }

  if (needs_refresh_) {
    Refresh();
  }

  // Just the tree without header
  for (auto& child : root_entry_.children) {
    DrawEntry(child);
  }
}

void FileBrowser::DrawEntry(FileEntry& entry, int depth) {
  ImGuiTreeNodeFlags flags = ImGuiTreeNodeFlags_SpanAvailWidth |
                             ImGuiTreeNodeFlags_OpenOnArrow |
                             ImGuiTreeNodeFlags_OpenOnDoubleClick;

  if (!entry.is_directory || entry.children.empty()) {
    flags |= ImGuiTreeNodeFlags_Leaf | ImGuiTreeNodeFlags_NoTreePushOnOpen;
  }

  if (entry.full_path == selected_path_) {
    flags |= ImGuiTreeNodeFlags_Selected;
  }

  // Build label with icon
  std::string label =
      absl::StrCat(GetFileIcon(entry.file_type), " ", entry.name);

  bool node_open = ImGui::TreeNodeEx(label.c_str(), flags);

  // Handle selection
  if (ImGui::IsItemClicked() && !ImGui::IsItemToggledOpen()) {
    selected_path_ = entry.full_path;

    if (entry.is_directory) {
      if (on_directory_clicked_) {
        on_directory_clicked_(entry.full_path);
      }
    } else {
      if (on_file_clicked_) {
        on_file_clicked_(entry.full_path);
      }
    }
  }

  // Tooltip with full path
  if (ImGui::IsItemHovered()) {
    ImGui::SetTooltip("%s", entry.full_path.c_str());
  }

  // Draw children if expanded
  if (node_open && entry.is_directory && !entry.children.empty()) {
    for (auto& child : entry.children) {
      DrawEntry(child, depth + 1);
    }
    ImGui::TreePop();
  }
}

}  // namespace editor
}  // namespace yaze
