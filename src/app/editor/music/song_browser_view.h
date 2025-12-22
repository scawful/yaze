#ifndef YAZE_EDITOR_MUSIC_SONG_BROWSER_VIEW_H
#define YAZE_EDITOR_MUSIC_SONG_BROWSER_VIEW_H

#include <functional>
#include <string>
#include <vector>

#include "zelda3/music/music_bank.h"

namespace yaze {
namespace editor {
namespace music {

using namespace yaze::zelda3::music;

/**
 * @brief UI component for browsing and managing songs.
 */
class SongBrowserView {
 public:
  SongBrowserView() = default;
  ~SongBrowserView() = default;

  /**
   * @brief Draw the song browser.
   * @param bank The music bank containing songs.
   */
  void Draw(MusicBank& bank);

  /**
   * @brief Set callback for when a song is selected.
   */
  void SetOnSongSelected(std::function<void(int)> callback) {
    on_song_selected_ = callback;
  }

  /**
   * @brief Set callback for when edits occur (e.g. renaming).
   */
  void SetOnEditCallback(std::function<void()> callback) { on_edit_ = callback; }
  void SetOnEdit(std::function<void()> callback) { on_edit_ = callback; }

  /**
   * @brief Set callback for opening tracker on a song.
   */
  void SetOnOpenTracker(std::function<void(int)> callback) {
    on_open_tracker_ = callback;
  }

  /**
   * @brief Set callback for opening piano roll on a song.
   */
  void SetOnOpenPianoRoll(std::function<void(int)> callback) {
    on_open_piano_roll_ = callback;
  }

  /**
   * @brief Set callback for exporting a song to ASM.
   */
  void SetOnExportAsm(std::function<void(int)> callback) {
    on_export_asm_ = callback;
  }

  /**
   * @brief Set callback for importing ASM to a song.
   */
  void SetOnImportAsm(std::function<void(int)> callback) {
    on_import_asm_ = callback;
  }

  int GetSelectedSongIndex() const { return selected_song_index_; }
  void SetSelectedSongIndex(int index) { selected_song_index_ = index; }

 private:
  void DrawCustomSection(MusicBank& bank, int current_index);
  void DrawSongItem(MusicBank& bank, int index, bool is_selected, bool is_custom);
  void HandleContextMenu(MusicBank& bank, int index, bool is_custom);

  // Search
  char search_buffer_[64] = "";
  bool MatchesSearch(const std::string& name) const;

  // Callbacks
  std::function<void(int)> on_song_selected_;
  std::function<void(int)> on_open_tracker_;
  std::function<void(int)> on_open_piano_roll_;
  std::function<void(int)> on_export_asm_;
  std::function<void(int)> on_import_asm_;
  std::function<void()> on_edit_;
  
  // State
  int selected_song_index_ = 0;
  int rename_target_index_ = -1;
};

}  // namespace music
}  // namespace editor
}  // namespace yaze

#endif  // YAZE_EDITOR_MUSIC_SONG_BROWSER_VIEW_H
