#ifndef YAZE_APP_EDITOR_AGENT_PANELS_FEATURE_FLAG_EDITOR_PANEL_H_
#define YAZE_APP_EDITOR_AGENT_PANELS_FEATURE_FLAG_EDITOR_PANEL_H_

#include <string>
#include <vector>

#include "core/hack_manifest.h"

namespace yaze {
namespace project {
class YazeProject;
}

namespace editor {

/**
 * @brief Panel for viewing and toggling ASM feature flags from the hack
 * manifest.
 *
 * Reads feature flags from the loaded HackManifest and allows toggling them
 * by writing updated values back to Config/feature_flags.asm.
 *
 * The panel displays a table with columns:
 *   Flag Name | Value | Status | Source | Toggle
 *
 * When the user clicks "Save", the panel regenerates the feature_flags.asm
 * file, preserving the header comment block and updating flag values.
 */
class FeatureFlagEditorPanel {
 public:
  FeatureFlagEditorPanel();
  ~FeatureFlagEditorPanel();

  /**
   * @brief Set the project pointer for manifest and path access.
   */
  void SetProject(project::YazeProject* project) { project_ = project; }

  /**
   * @brief Draw the panel content (no ImGui::Begin/End).
   */
  void Draw();

 private:
  /**
   * @brief Refresh the local flags list from the hack manifest.
   */
  void RefreshFromManifest();

  /**
   * @brief Write the current flag state to Config/feature_flags.asm.
   * @return true on success.
   */
  bool SaveToFile();

  /**
   * @brief Resolve the absolute path to Config/feature_flags.asm.
   */
  std::string ResolveConfigPath() const;

  // The project we read the manifest from
  project::YazeProject* project_ = nullptr;

  // Local mutable copy of flags (allows toggling before saving)
  struct EditableFlag {
    std::string name;
    int value;
    bool enabled;
    std::string source;
    bool dirty = false;  // Changed since last save
  };
  std::vector<EditableFlag> flags_;

  // UI state
  bool needs_refresh_ = true;
  std::string status_message_;
  bool status_is_error_ = false;
  char filter_text_[128] = {};
};

}  // namespace editor
}  // namespace yaze

#endif  // YAZE_APP_EDITOR_AGENT_PANELS_FEATURE_FLAG_EDITOR_PANEL_H_
