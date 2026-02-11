#ifndef YAZE_APP_EDITOR_MUSIC_UNDO_ACTIONS_H_
#define YAZE_APP_EDITOR_MUSIC_UNDO_ACTIONS_H_

#include <cstddef>
#include <string>
#include <utility>

#include "absl/status/status.h"
#include "absl/strings/str_format.h"
#include "app/editor/core/undo_action.h"
#include "zelda3/music/music_bank.h"
#include "zelda3/music/song_data.h"

namespace yaze {
namespace editor {

/**
 * @class MusicSongEditAction
 * @brief Undoable action for edits to a music song.
 *
 * Captures a full snapshot of the MusicSong before and after the edit
 * so that Undo restores the before-state and Redo restores the after-state.
 * Applies changes via a pointer to the owning MusicBank.
 */
class MusicSongEditAction : public UndoAction {
 public:
  MusicSongEditAction(int song_index,
                      zelda3::music::MusicSong before_snapshot,
                      zelda3::music::MusicSong after_snapshot,
                      zelda3::music::MusicBank* music_bank)
      : song_index_(song_index),
        before_snapshot_(std::move(before_snapshot)),
        after_snapshot_(std::move(after_snapshot)),
        music_bank_(music_bank) {}

  absl::Status Undo() override {
    auto* song = music_bank_->GetSong(song_index_);
    if (!song) {
      return absl::OutOfRangeError(
          absl::StrFormat("Song %d out of range", song_index_));
    }
    *song = before_snapshot_;
    return absl::OkStatus();
  }

  absl::Status Redo() override {
    auto* song = music_bank_->GetSong(song_index_);
    if (!song) {
      return absl::OutOfRangeError(
          absl::StrFormat("Song %d out of range", song_index_));
    }
    *song = after_snapshot_;
    return absl::OkStatus();
  }

  std::string Description() const override {
    return absl::StrFormat("Edit song %d", song_index_);
  }

  size_t MemoryUsage() const override {
    // Rough estimate based on segment count
    size_t usage = sizeof(*this);
    for (const auto& seg : before_snapshot_.segments) {
      for (const auto& track : seg.tracks) {
        usage += track.events.capacity() * sizeof(zelda3::music::TrackEvent);
      }
    }
    for (const auto& seg : after_snapshot_.segments) {
      for (const auto& track : seg.tracks) {
        usage += track.events.capacity() * sizeof(zelda3::music::TrackEvent);
      }
    }
    return usage;
  }

 private:
  int song_index_;
  zelda3::music::MusicSong before_snapshot_;
  zelda3::music::MusicSong after_snapshot_;
  zelda3::music::MusicBank* music_bank_;
};

}  // namespace editor
}  // namespace yaze

#endif  // YAZE_APP_EDITOR_MUSIC_UNDO_ACTIONS_H_
