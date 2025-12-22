#ifndef YAZE_APP_EDITOR_MUSIC_PANELS_MUSIC_INSTRUMENT_EDITOR_PANEL_H_
#define YAZE_APP_EDITOR_MUSIC_PANELS_MUSIC_INSTRUMENT_EDITOR_PANEL_H_

#include <functional>
#include <string>

#include "app/editor/music/instrument_editor_view.h"
#include "app/editor/system/editor_panel.h"
#include "app/gui/core/icons.h"
#include "zelda3/music/music_bank.h"

namespace yaze {
namespace editor {

/**
 * @class MusicInstrumentEditorPanel
 * @brief EditorPanel wrapper for the instrument editor
 *
 * Delegates to InstrumentEditorView for the actual UI drawing.
 */
class MusicInstrumentEditorPanel : public EditorPanel {
 public:
  MusicInstrumentEditorPanel(zelda3::music::MusicBank* music_bank,
                             music::InstrumentEditorView* instrument_view)
      : music_bank_(music_bank), instrument_view_(instrument_view) {}

  // ==========================================================================
  // EditorPanel Identity
  // ==========================================================================

  std::string GetId() const override { return "music.instrument_editor"; }
  std::string GetDisplayName() const override { return "Instrument Editor"; }
  std::string GetIcon() const override { return ICON_MD_SPEAKER; }
  std::string GetEditorCategory() const override { return "Music"; }
  int GetPriority() const override { return 20; }

  // ==========================================================================
  // EditorPanel Drawing
  // ==========================================================================

  void Draw(bool* p_open) override {
    if (!music_bank_ || !instrument_view_) {
      ImGui::TextDisabled("Music bank not loaded");
      return;
    }

    instrument_view_->Draw(*music_bank_);
  }

  // ==========================================================================
  // Callback Setters
  // ==========================================================================

  void SetOnEditCallback(std::function<void()> callback) {
    if (instrument_view_) instrument_view_->SetOnEditCallback(callback);
  }

  void SetOnPreviewCallback(std::function<void(int)> callback) {
    if (instrument_view_) instrument_view_->SetOnPreviewCallback(callback);
  }

 private:
  zelda3::music::MusicBank* music_bank_ = nullptr;
  music::InstrumentEditorView* instrument_view_ = nullptr;
};

}  // namespace editor
}  // namespace yaze

#endif  // YAZE_APP_EDITOR_MUSIC_PANELS_MUSIC_INSTRUMENT_EDITOR_PANEL_H_
