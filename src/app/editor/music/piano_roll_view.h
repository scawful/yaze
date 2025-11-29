#ifndef YAZE_EDITOR_MUSIC_PIANO_ROLL_VIEW_H
#define YAZE_EDITOR_MUSIC_PIANO_ROLL_VIEW_H

#include <functional>

#include "imgui/imgui.h"
#include "zelda3/music/song_data.h"
#include "zelda3/music/music_bank.h"

namespace yaze {
namespace editor {
namespace music {

/**
 * @brief UI component for displaying and editing music tracks as a piano roll.
 */
class PianoRollView {
 public:
  PianoRollView() = default;
  ~PianoRollView() = default;

  /**
   * @brief Draw the piano roll view for the given song.
   * @param song The song to display and edit.
   * @param bank The music bank for instrument names (optional).
   */
  void Draw(zelda3::music::MusicSong* song, const zelda3::music::MusicBank* bank = nullptr);

  /**
   * @brief Set callback for when edits occur.
   */
  void SetOnEditCallback(std::function<void()> callback) { on_edit_ = callback; }

  /**
   * @brief Set callback for note preview.
   */
  void SetOnNotePreview(
      std::function<void(const zelda3::music::TrackEvent&, int, int)> callback) {
    on_note_preview_ = callback;
  }

  /**
   * @brief Set callback for segment preview.
   */
  void SetOnSegmentPreview(
      std::function<void(const zelda3::music::MusicSong&, int)> callback) {
    on_segment_preview_ = callback;
  }

  int GetActiveChannel() const { return active_channel_index_; }
  void SetActiveChannel(int channel) { active_channel_index_ = channel; }

  int GetActiveSegment() const { return active_segment_index_; }
  void SetActiveSegment(int segment) { active_segment_index_ = segment; }

  // Get the selected instrument for preview/insertion
  int GetPreviewInstrument() const { return preview_instrument_index_; }

 private:
  // UI Helper methods
  void DrawToolbar(const zelda3::music::MusicSong* song, const zelda3::music::MusicBank* bank);

  // Input Handling
  void HandleMouseInput(zelda3::music::MusicSong* song, int active_channel, int active_segment,
                        const ImVec2& grid_origin, const ImVec2& grid_size);

  // State
  int active_channel_index_ = 0;
  int active_segment_index_ = 0;
  int preview_instrument_index_ = 0; // Selected instrument for new notes
  float pixels_per_tick_ = 2.0f;
  float key_height_ = 12.0f;
  float key_width_ = 60.0f;
  int scroll_x_ = 0;
  int scroll_y_ = 0; // Scroll offset in keys (from top C-1)
  bool snap_enabled_ = true;
  int snap_ticks_ = zelda3::music::kDurationSixteenth;
  bool follow_playback_ = false;

  // Channel State
  std::vector<bool> channel_visible_ = std::vector<bool>(8, true);
  std::vector<ImU32> channel_colors_;

  // Editing State
  int drag_mode_ = 0; // 0=None, 1=Move, 2=ResizeLeft, 3=ResizeRight
  int drag_start_tick_ = 0;
  int drag_start_duration_ = 0;
  int drag_event_index_ = -1;
  int hovered_event_index_ = -1;
  int hovered_channel_index_ = -1;
  int hovered_segment_index_ = -1;

  // Drag state for HandleMouseInput
  int dragging_event_index_ = -1;
  int drag_segment_index_ = -1;
  int drag_channel_index_ = -1;
  bool drag_moved_ = false;
  zelda3::music::TrackEvent drag_original_event_;
  ImVec2 drag_start_mouse_;

  // Context Menu State
  struct ContextTarget {
    int segment = -1;
    int channel = -1;
    int event_index = -1;
  } context_target_;

  struct EmptyContextTarget {
    int segment = -1;
    int channel = -1;
    int tick = -1;
    uint8_t pitch = 0;
  } empty_context_;

  // Callbacks
  std::function<void()> on_edit_;
  std::function<void(const zelda3::music::TrackEvent&, int segment_index, int channel_index)>
      on_note_preview_;
  std::function<void(const zelda3::music::MusicSong&, int segment_index)> on_segment_preview_;
};

}  // namespace music
}  // namespace editor
}  // namespace yaze

#endif  // YAZE_EDITOR_MUSIC_PIANO_ROLL_VIEW_H
