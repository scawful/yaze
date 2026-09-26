#ifndef YAZE_APP_EDITOR_MENU_RECENT_FILES_MENU_MODEL_H_
#define YAZE_APP_EDITOR_MENU_RECENT_FILES_MENU_MODEL_H_

#include <cstddef>
#include <functional>
#include <string>
#include <vector>

namespace yaze {
namespace editor {

// One row of File > Open Recent.
struct RecentFileMenuEntry {
  std::string path;    // Full path passed to EditorManager::OpenRomOrProject.
  std::string label;   // Unique, human-readable menu label.
  bool exists = true;  // False when the file is gone; the row is disabled.
};

using RecentFileExistsFn = std::function<bool(const std::string&)>;

// Builds the Open Recent rows from the MRU list (most recent first).
// - Keeps at most `max_entries` rows and skips empty or duplicate paths.
// - Labels are the file name; when two rows share a file name, the parent
//   directory name is appended ("oos168.sfc (Roms)"), and if that still
//   collides the full path is used, so every label is unique (ImGui IDs).
// - `exists` defaults to std::filesystem::exists when not provided.
std::vector<RecentFileMenuEntry> BuildRecentFileMenuEntries(
    const std::vector<std::string>& recent_paths, size_t max_entries = 10,
    const RecentFileExistsFn& exists = nullptr);

}  // namespace editor
}  // namespace yaze

#endif  // YAZE_APP_EDITOR_MENU_RECENT_FILES_MENU_MODEL_H_
