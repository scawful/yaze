#ifndef YAZE_APP_EDITOR_SYSTEM_FILE_BROWSER_H_
#define YAZE_APP_EDITOR_SYSTEM_FILE_BROWSER_H_

#include <filesystem>
#include <functional>
#include <set>
#include <string>
#include <vector>

namespace yaze {
namespace editor {

/**
 * @struct FileEntry
 * @brief Represents a file or folder in the file browser
 */
struct FileEntry {
  std::string name;
  std::string full_path;
  bool is_directory;
  bool is_expanded = false;  // For directories: whether to show children
  std::vector<FileEntry> children;

  // File type detection for icons
  enum class FileType {
    kDirectory,
    kAssembly,
    kSource,
    kHeader,
    kText,
    kConfig,
    kJson,
    kImage,
    kBinary,
    kUnknown
  };
  FileType file_type = FileType::kUnknown;
};

/**
 * @class GitignoreParser
 * @brief Simple .gitignore pattern matcher
 *
 * Supports basic gitignore patterns:
 * - Simple file/folder names: "node_modules"
 * - Wildcards: "*.log"
 * - Directory-only: "build/"
 * - Comments: "# comment"
 * - Negation: "!important.txt"
 */
class GitignoreParser {
 public:
  void LoadFromFile(const std::string& gitignore_path);
  void AddPattern(const std::string& pattern);
  bool IsIgnored(const std::string& path, bool is_directory) const;
  void Clear();

 private:
  struct Pattern {
    std::string pattern;
    bool is_negation = false;
    bool directory_only = false;
  };

  std::vector<Pattern> patterns_;

  bool MatchPattern(const std::string& path, const Pattern& pattern) const;
  bool MatchGlob(const std::string& text, const std::string& pattern) const;
};

/**
 * @class FileBrowser
 * @brief File system browser for the sidebar
 *
 * Features:
 * - Respects .gitignore patterns
 * - Hides dotfiles by default (configurable)
 * - Tree view rendering
 * - Works with native filesystem and WASM virtual FS
 * - File type detection for icons
 *
 * Usage:
 * ```cpp
 * FileBrowser browser;
 * browser.SetRootPath("/path/to/asm");
 * browser.SetFileClickedCallback([](const std::string& path) {
 *   // Open file in editor
 * });
 * browser.Draw();
 * ```
 */
class FileBrowser {
 public:
  FileBrowser() = default;

  /**
   * @brief Set the root path for the file browser
   * @param path Path to display (absolute or relative)
   */
  void SetRootPath(const std::string& path);

  /**
   * @brief Get the current root path
   */
  const std::string& GetRootPath() const { return root_path_; }

  /**
   * @brief Check if a root path is set
   */
  bool HasRootPath() const { return !root_path_.empty(); }

  /**
   * @brief Refresh the file tree from disk
   */
  void Refresh();

  /**
   * @brief Draw the file tree in ImGui
   */
  void Draw();

  /**
   * @brief Draw a compact version for narrow sidebars
   */
  void DrawCompact();

  // Configuration
  void SetShowHiddenFiles(bool show) { show_hidden_files_ = show; }
  bool GetShowHiddenFiles() const { return show_hidden_files_; }

  void SetRespectGitignore(bool respect) { respect_gitignore_ = respect; }
  bool GetRespectGitignore() const { return respect_gitignore_; }

  /**
   * @brief Add file extensions to filter (empty = show all)
   */
  void SetFileFilter(const std::vector<std::string>& extensions);
  void ClearFileFilter();

  // Callbacks
  using FileClickedCallback = std::function<void(const std::string& path)>;
  using DirectoryClickedCallback = std::function<void(const std::string& path)>;

  void SetFileClickedCallback(FileClickedCallback callback) {
    on_file_clicked_ = std::move(callback);
  }

  void SetDirectoryClickedCallback(DirectoryClickedCallback callback) {
    on_directory_clicked_ = std::move(callback);
  }

  // Statistics
  size_t GetFileCount() const { return file_count_; }
  size_t GetDirectoryCount() const { return directory_count_; }

 private:
  void ScanDirectory(const std::filesystem::path& path, FileEntry& parent,
                     int depth = 0);
  bool ShouldShow(const std::filesystem::path& path, bool is_directory) const;
  bool MatchesFilter(const std::string& filename) const;
  FileEntry::FileType DetectFileType(const std::string& filename) const;
  const char* GetFileIcon(FileEntry::FileType type) const;
  void DrawEntry(FileEntry& entry, int depth = 0);

  // State
  std::string root_path_;
  FileEntry root_entry_;
  bool needs_refresh_ = true;

  // Configuration
  bool show_hidden_files_ = false;
  bool respect_gitignore_ = true;
  std::set<std::string> file_filter_;  // Empty = show all

  // Gitignore handling
  GitignoreParser gitignore_parser_;

  // Statistics
  size_t file_count_ = 0;
  size_t directory_count_ = 0;

  // Callbacks
  FileClickedCallback on_file_clicked_;
  DirectoryClickedCallback on_directory_clicked_;

  // UI state
  std::string selected_path_;

  // Constants
  static constexpr int kMaxDepth = 10;
  static constexpr size_t kMaxEntries = 1000;
};

}  // namespace editor
}  // namespace yaze

#endif  // YAZE_APP_EDITOR_SYSTEM_FILE_BROWSER_H_
