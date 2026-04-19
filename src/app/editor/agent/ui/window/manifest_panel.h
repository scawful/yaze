#ifndef YAZE_APP_EDITOR_AGENT_PANELS_MANIFEST_PANEL_H_
#define YAZE_APP_EDITOR_AGENT_PANELS_MANIFEST_PANEL_H_

#include <string>
#include <vector>

#include "core/hack_manifest.h"

namespace yaze {
namespace project {
class YazeProject;
}

namespace editor {

/**
 * @brief Panel for manifest freshness UX and protected regions inspection.
 *
 * Card B3: Shows manifest path, mtime, loaded status, and a "Reload" button.
 * Card B4: Searchable table of protected regions from the HackManifest.
 *
 * Both are read-only — this panel never writes to the ROM or manifest file.
 */
class ManifestPanel {
 public:
  ManifestPanel() = default;
  ~ManifestPanel() = default;

  void SetProject(project::YazeProject* project) { project_ = project; }

  /// Draw the combined panel content (no ImGui::Begin/End wrapper).
  void Draw();

 private:
  /// Draw Card B3: manifest status, path, mtime, reload button.
  void DrawManifestStatus();

  /// Draw Card B4: searchable protected regions table.
  void DrawProtectedRegions();

  /// Resolve the absolute path to hack_manifest.json.
  std::string ResolveManifestPath() const;

  project::YazeProject* project_ = nullptr;

  // B4 filter state
  char filter_text_[128] = {};

  // Track reload feedback
  std::string status_message_;
  bool status_is_error_ = false;
};

}  // namespace editor
}  // namespace yaze

#endif  // YAZE_APP_EDITOR_AGENT_PANELS_MANIFEST_PANEL_H_
