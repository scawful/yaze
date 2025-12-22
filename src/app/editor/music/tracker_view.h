#ifndef YAZE_EDITOR_MUSIC_TRACKER_VIEW_H
#define YAZE_EDITOR_MUSIC_TRACKER_VIEW_H

#include <functional>

#include "zelda3/music/song_data.h"
#include "zelda3/music/music_bank.h"

namespace yaze {
namespace editor {
namespace music {

using namespace yaze::zelda3::music;

/**
 * @brief UI component for displaying and editing music tracks.
 *
 * Renders an 8-channel tracker view where rows represent time (ticks)
 * and columns represent audio channels.
 */
class TrackerView {
 public:
  TrackerView() = default;
  ~TrackerView() = default;

  /**
   * @brief Draw the tracker view for the given song.
   * @param song The song to display and edit (can be nullptr).
   * @param bank The music bank for resolving instrument names (optional).
   */
  void Draw(MusicSong* song, const MusicBank* bank = nullptr);

  /**
   * @brief Set callback for when edits occur (to trigger undo save)
   */
  void SetOnEditCallback(std::function<void()> callback) { on_edit_ = callback; }

 private:
  // UI Helper methods
  void DrawToolbar(MusicSong* song);
  void DrawGrid(MusicSong* song, const MusicBank* bank);
  void DrawChannelHeader(int channel_idx);
  void DrawEventCell(MusicTrack& track, int event_index, int channel_idx, uint16_t tick, const MusicBank* bank);

  // State
  int current_tick_ = 0;
  float row_height_ = 20.0f;
  bool follow_playback_ = false;
  int ticks_per_row_ = 18;

  // Selection
  int selected_row_ = 0;
  int selected_col_ = 0;  // 0 = Tick column (not selectable), 1-8 = Channels
  int selection_anchor_row_ = -1;
  int selection_anchor_col_ = -1;

  // Input handling
  void HandleKeyboardInput(MusicSong* song);
  void HandleNavigation();
  void HandleEditShortcuts(MusicSong* song);
  
  // Editing callbacks
  std::function<void()> on_edit_;
};

}  // namespace music
}  // namespace editor
}  // namespace yaze

#endif  // YAZE_EDITOR_MUSIC_TRACKER_VIEW_H
