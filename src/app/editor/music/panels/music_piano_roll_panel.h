#ifndef YAZE_APP_EDITOR_MUSIC_PANELS_MUSIC_PIANO_ROLL_PANEL_H_
#define YAZE_APP_EDITOR_MUSIC_PANELS_MUSIC_PIANO_ROLL_PANEL_H_

#include <functional>
#include <string>

#include "app/editor/music/music_player.h"
#include "app/editor/music/piano_roll_view.h"
#include "app/editor/system/editor_panel.h"
#include "app/gui/core/icons.h"
#include "zelda3/music/music_bank.h"

namespace yaze {
namespace editor {

/**
 * @class MusicPianoRollPanel
 * @brief EditorPanel wrapper for the piano roll view
 *
 * Delegates to PianoRollView for the actual UI drawing.
 */
class MusicPianoRollPanel : public EditorPanel {
 public:
  MusicPianoRollPanel(zelda3::music::MusicBank* music_bank,
                      int* current_song_index, int* current_segment_index,
                      int* current_channel_index,
                      music::PianoRollView* piano_roll_view,
                      music::MusicPlayer* music_player)
      : music_bank_(music_bank),
        current_song_index_(current_song_index),
        current_segment_index_(current_segment_index),
        current_channel_index_(current_channel_index),
        piano_roll_view_(piano_roll_view),
        music_player_(music_player) {}

  // ==========================================================================
  // EditorPanel Identity
  // ==========================================================================

  std::string GetId() const override { return "music.piano_roll"; }
  std::string GetDisplayName() const override { return "Piano Roll"; }
  std::string GetIcon() const override { return ICON_MD_PIANO; }
  std::string GetEditorCategory() const override { return "Music"; }
  int GetPriority() const override { return 15; }

  // ==========================================================================
  // Callback Setters
  // ==========================================================================

  void SetOnEditCallback(std::function<void()> callback) {
    on_edit_ = callback;
    if (piano_roll_view_) piano_roll_view_->SetOnEditCallback(callback);
  }

  // ==========================================================================
  // EditorPanel Drawing
  // ==========================================================================

  void Draw(bool* p_open) override {
    if (!music_bank_ || !piano_roll_view_ || !current_song_index_) {
      ImGui::TextDisabled("Music bank not loaded");
      return;
    }

    auto* song = music_bank_->GetSong(*current_song_index_);
    if (song && *current_segment_index_ >=
                    static_cast<int>(song->segments.size())) {
      *current_segment_index_ = 0;
    }

    piano_roll_view_->SetActiveChannel(*current_channel_index_);
    piano_roll_view_->SetActiveSegment(*current_segment_index_);

    // Set up note preview callback
    piano_roll_view_->SetOnNotePreview(
        [this, song_index = *current_song_index_](
            const zelda3::music::TrackEvent& evt, int segment_idx,
            int channel_idx) {
          auto* target = music_bank_->GetSong(song_index);
          if (!target || !music_player_) return;
          music_player_->PreviewNote(*target, evt, segment_idx, channel_idx);
        });

    piano_roll_view_->SetOnSegmentPreview(
        [this, song_index = *current_song_index_](
            const zelda3::music::MusicSong& /*unused*/, int segment_idx) {
          auto* target = music_bank_->GetSong(song_index);
          if (!target || !music_player_) return;
          music_player_->PreviewSegment(*target, segment_idx);
        });

    // Update playback state for cursor visualization
    auto state =
        music_player_ ? music_player_->GetState() : music::PlaybackState{};
    piano_roll_view_->SetPlaybackState(state.is_playing, state.is_paused,
                                       state.current_tick);

    piano_roll_view_->Draw(song, music_bank_);

    // Update indices from view state
    *current_segment_index_ = piano_roll_view_->GetActiveSegment();
    *current_channel_index_ = piano_roll_view_->GetActiveChannel();
  }

 private:
  zelda3::music::MusicBank* music_bank_ = nullptr;
  int* current_song_index_ = nullptr;
  int* current_segment_index_ = nullptr;
  int* current_channel_index_ = nullptr;
  music::PianoRollView* piano_roll_view_ = nullptr;
  music::MusicPlayer* music_player_ = nullptr;
  std::function<void()> on_edit_;
};

}  // namespace editor
}  // namespace yaze

#endif  // YAZE_APP_EDITOR_MUSIC_PANELS_MUSIC_PIANO_ROLL_PANEL_H_
