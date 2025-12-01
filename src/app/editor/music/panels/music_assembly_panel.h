#ifndef YAZE_APP_EDITOR_MUSIC_PANELS_MUSIC_ASSEMBLY_PANEL_H_
#define YAZE_APP_EDITOR_MUSIC_PANELS_MUSIC_ASSEMBLY_PANEL_H_

#include <string>

#include "app/editor/code/assembly_editor.h"
#include "app/editor/system/editor_panel.h"
#include "app/gui/core/icons.h"

namespace yaze {
namespace editor {

/**
 * @class MusicAssemblyPanel
 * @brief EditorPanel wrapper for the assembly editor view in Music context
 */
class MusicAssemblyPanel : public EditorPanel {
 public:
  explicit MusicAssemblyPanel(AssemblyEditor* assembly_editor)
      : assembly_editor_(assembly_editor) {}

  // ==========================================================================
  // EditorPanel Identity
  // ==========================================================================

  std::string GetId() const override { return "music.assembly"; }
  std::string GetDisplayName() const override { return "Assembly View"; }
  std::string GetIcon() const override { return ICON_MD_CODE; }
  std::string GetEditorCategory() const override { return "Music"; }
  int GetPriority() const override { return 30; }

  // ==========================================================================
  // EditorPanel Drawing
  // ==========================================================================

  void Draw(bool* p_open) override {
    if (!assembly_editor_) {
      ImGui::TextDisabled("Assembly editor not available");
      return;
    }

    assembly_editor_->InlineUpdate();
  }

 private:
  AssemblyEditor* assembly_editor_ = nullptr;
};

}  // namespace editor
}  // namespace yaze

#endif  // YAZE_APP_EDITOR_MUSIC_PANELS_MUSIC_ASSEMBLY_PANEL_H_
