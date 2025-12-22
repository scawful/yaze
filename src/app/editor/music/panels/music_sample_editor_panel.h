#ifndef YAZE_APP_EDITOR_MUSIC_PANELS_MUSIC_SAMPLE_EDITOR_PANEL_H_
#define YAZE_APP_EDITOR_MUSIC_PANELS_MUSIC_SAMPLE_EDITOR_PANEL_H_

#include <functional>
#include <string>

#include "app/editor/music/sample_editor_view.h"
#include "app/editor/system/editor_panel.h"
#include "app/gui/core/icons.h"
#include "zelda3/music/music_bank.h"

namespace yaze {
namespace editor {

/**
 * @class MusicSampleEditorPanel
 * @brief EditorPanel wrapper for the sample editor
 *
 * Delegates to SampleEditorView for the actual UI drawing.
 */
class MusicSampleEditorPanel : public EditorPanel {
 public:
  MusicSampleEditorPanel(zelda3::music::MusicBank* music_bank,
                         music::SampleEditorView* sample_view)
      : music_bank_(music_bank), sample_view_(sample_view) {}

  // ==========================================================================
  // EditorPanel Identity
  // ==========================================================================

  std::string GetId() const override { return "music.sample_editor"; }
  std::string GetDisplayName() const override { return "Sample Editor"; }
  std::string GetIcon() const override { return ICON_MD_WAVES; }
  std::string GetEditorCategory() const override { return "Music"; }
  int GetPriority() const override { return 25; }

  // ==========================================================================
  // EditorPanel Drawing
  // ==========================================================================

  void Draw(bool* p_open) override {
    if (!music_bank_ || !sample_view_) {
      ImGui::TextDisabled("Music bank not loaded");
      return;
    }

    sample_view_->Draw(*music_bank_);
  }

  // ==========================================================================
  // Callback Setters
  // ==========================================================================

  void SetOnEditCallback(std::function<void()> callback) {
    if (sample_view_) sample_view_->SetOnEditCallback(callback);
  }

  void SetOnPreviewCallback(std::function<void(int)> callback) {
    if (sample_view_) sample_view_->SetOnPreviewCallback(callback);
  }

 private:
  zelda3::music::MusicBank* music_bank_ = nullptr;
  music::SampleEditorView* sample_view_ = nullptr;
};

}  // namespace editor
}  // namespace yaze

#endif  // YAZE_APP_EDITOR_MUSIC_PANELS_MUSIC_SAMPLE_EDITOR_PANEL_H_
