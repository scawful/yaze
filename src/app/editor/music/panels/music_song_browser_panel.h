#ifndef YAZE_APP_EDITOR_MUSIC_PANELS_MUSIC_SONG_BROWSER_PANEL_H_
#define YAZE_APP_EDITOR_MUSIC_PANELS_MUSIC_SONG_BROWSER_PANEL_H_

#include <functional>
#include <string>

#include "app/editor/music/song_browser_view.h"
#include "app/editor/system/editor_panel.h"
#include "app/gui/core/icons.h"
#include "zelda3/music/music_bank.h"

namespace yaze {
namespace editor {

/**
 * @class MusicSongBrowserPanel
 * @brief EditorPanel wrapper for the music song browser
 *
 * Delegates to SongBrowserView for the actual UI drawing.
 */
class MusicSongBrowserPanel : public EditorPanel {
 public:
  MusicSongBrowserPanel(zelda3::music::MusicBank* music_bank,
                        int* current_song_index,
                        music::SongBrowserView* song_browser_view)
      : music_bank_(music_bank),
        current_song_index_(current_song_index),
        song_browser_view_(song_browser_view) {}

  // ==========================================================================
  // EditorPanel Identity
  // ==========================================================================

  std::string GetId() const override { return "music.song_browser"; }
  std::string GetDisplayName() const override { return "Song Browser"; }
  std::string GetIcon() const override { return ICON_MD_LIBRARY_MUSIC; }
  std::string GetEditorCategory() const override { return "Music"; }
  int GetPriority() const override { return 5; }

  // ==========================================================================
  // EditorPanel Drawing
  // ==========================================================================

  void Draw(bool* p_open) override {
    if (!music_bank_ || !song_browser_view_) {
      ImGui::TextDisabled("Music bank not loaded");
      return;
    }

    song_browser_view_->SetSelectedSongIndex(*current_song_index_);
    song_browser_view_->Draw(*music_bank_);

    // Update current song if selection changed
    if (song_browser_view_->GetSelectedSongIndex() != *current_song_index_) {
      *current_song_index_ = song_browser_view_->GetSelectedSongIndex();
    }
  }

  // ==========================================================================
  // Callback Setters (for integration with MusicEditor)
  // ==========================================================================

  void SetOnSongSelected(std::function<void(int)> callback) {
    if (song_browser_view_) song_browser_view_->SetOnSongSelected(callback);
  }

  void SetOnOpenTracker(std::function<void(int)> callback) {
    if (song_browser_view_) song_browser_view_->SetOnOpenTracker(callback);
  }

  void SetOnOpenPianoRoll(std::function<void(int)> callback) {
    if (song_browser_view_) song_browser_view_->SetOnOpenPianoRoll(callback);
  }

  void SetOnExportAsm(std::function<void(int)> callback) {
    if (song_browser_view_) song_browser_view_->SetOnExportAsm(callback);
  }

  void SetOnImportAsm(std::function<void(int)> callback) {
    if (song_browser_view_) song_browser_view_->SetOnImportAsm(callback);
  }

  void SetOnEdit(std::function<void()> callback) {
    if (song_browser_view_) song_browser_view_->SetOnEdit(callback);
  }

 private:
  zelda3::music::MusicBank* music_bank_ = nullptr;
  int* current_song_index_ = nullptr;
  music::SongBrowserView* song_browser_view_ = nullptr;
};

}  // namespace editor
}  // namespace yaze

#endif  // YAZE_APP_EDITOR_MUSIC_PANELS_MUSIC_SONG_BROWSER_PANEL_H_
