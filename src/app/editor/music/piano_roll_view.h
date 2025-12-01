#ifndef YAZE_EDITOR_MUSIC_PIANO_ROLL_VIEW_H
#define YAZE_EDITOR_MUSIC_PIANO_ROLL_VIEW_H

#include <functional>

#include "imgui/imgui.h"
#include "app/editor/music/music_constants.h"
#include "zelda3/music/music_bank.h"
#include "zelda3/music/song_data.h"

namespace yaze {
namespace editor {
namespace music {

struct RollPalette {
  ImU32 white_key;
  ImU32 black_key;
  ImU32 grid_major;
  ImU32 grid_minor;
  ImU32 note;
  ImU32 note_hover;
  ImU32 note_shadow;
  ImU32 background;
  ImU32 key_label;
  ImU32 beat_marker;
  ImU32 octave_line;
};

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

  // Playback cursor support
  void SetPlaybackState(bool is_playing, bool is_paused, uint32_t current_tick) {
    is_playing_ = is_playing;
    is_paused_ = is_paused;
    playback_tick_ = current_tick;
  }

  void SetFollowPlayback(bool follow) { follow_playback_ = follow; }
  bool IsFollowingPlayback() const { return follow_playback_; }
  bool IsPlaying() const { return is_playing_; }
  bool IsPaused() const { return is_paused_; }

 private:
  // UI Helper methods
  void DrawToolbar(const zelda3::music::MusicSong* song, const zelda3::music::MusicBank* bank);
  void DrawChannelList(const zelda3::music::MusicSong* song);
  void DrawStatusBar(const zelda3::music::MusicSong* song);
  void DrawRollCanvas(zelda3::music::MusicSong* song, const RollPalette& palette,
                      const ImVec2& canvas_size);
  
  // Drawing Helpers
  void DrawPianoKeys(ImDrawList* draw_list, const ImVec2& key_origin, float total_height, 
                     int start_key_idx, int visible_keys, const RollPalette& palette);
  void DrawGrid(ImDrawList* draw_list, const ImVec2& grid_origin, const ImVec2& canvas_pos,
                const ImVec2& canvas_size, float total_height, float clip_bottom,
                int start_tick, int visible_ticks, int start_key_idx, int visible_keys,
                float content_width, const RollPalette& palette);
  void DrawNotes(ImDrawList* draw_list, const zelda3::music::MusicSong* song,
                 const ImVec2& grid_origin, float total_height,
                 int start_tick, int end_tick, int start_key_idx, int visible_keys,
                 const RollPalette& palette);
  void DrawPlaybackCursor(ImDrawList* draw_list, const ImVec2& grid_origin,
                          float grid_height, uint32_t segment_start_tick);

  // Input Handling
  void HandleMouseInput(zelda3::music::MusicSong* song, int active_channel, int active_segment,
                        const ImVec2& grid_origin, const ImVec2& grid_size, bool is_hovered);

  // Layout constants
  static constexpr float kToolbarHeight = 32.0f;
  static constexpr float kStatusBarHeight = 24.0f;
  static constexpr float kChannelListWidth = 140.0f;

  // State
  int active_channel_index_ = 0;
  int active_segment_index_ = 0;
  int preview_instrument_index_ = 0; // Selected instrument for new notes
  float pixels_per_tick_ = 2.0f;
  float key_height_ = 12.0f;
  float key_width_ = 40.0f;
  float scroll_x_px_ = 0.0f;
  float scroll_y_px_ = 0.0f;  // Scroll offsets in pixels
  bool snap_enabled_ = true;
  int snap_ticks_ = zelda3::music::kDurationSixteenth;
  bool follow_playback_ = false;

  // Channel State
  std::vector<bool> channel_visible_ = std::vector<bool>(8, true);
  std::vector<bool> channel_muted_ = std::vector<bool>(8, false);
  std::vector<bool> channel_solo_ = std::vector<bool>(8, false);
  std::vector<ImU32> channel_colors_;

  // Editing State
  int drag_mode_ = 0; // 0=None, 1=Move, 2=ResizeLeft, 3=ResizeRight
  int drag_start_tick_ = 0;
  int drag_start_duration_ = 0;
  int drag_event_index_ = -1;
  int hovered_event_index_ = -1;
  int hovered_channel_index_ = -1;
  int hovered_segment_index_ = -1;

  // Status bar state (mouse position in grid coordinates)
  int status_tick_ = -1;
  int status_pitch_ = -1;
  std::string status_note_name_;

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

  // Playback state
  bool is_playing_ = false;
  bool is_paused_ = false;
  uint32_t playback_tick_ = 0;
};

}  // namespace music
}  // namespace editor
}  // namespace yaze

#endif  // YAZE_EDITOR_MUSIC_PIANO_ROLL_VIEW_H
