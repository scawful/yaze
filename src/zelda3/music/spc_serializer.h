#ifndef YAZE_ZELDA3_MUSIC_SPC_SERIALIZER_H
#define YAZE_ZELDA3_MUSIC_SPC_SERIALIZER_H

#include <cstdint>
#include <string>
#include <vector>

#include "absl/status/statusor.h"
#include "zelda3/music/song_data.h"

namespace yaze {
namespace zelda3 {
namespace music {

/**
 * @brief Serializes MusicSong to N-SPC byte format for direct ARAM upload.
 *
 * Produces a complete sound bank block that can be uploaded to ARAM:
 * - Song header (section pointers)
 * - Channel pointer table
 * - Channel data (notes, commands, durations)
 *
 * This enables preview of custom songs without saving to ROM first.
 */
class SpcSerializer {
 public:
  /**
   * @brief Serialization options.
   */
  struct SerializeOptions {
    uint16_t base_aram_address = 0xD000;  // Target ARAM address for song table
    bool include_header = true;           // Include song header structure
  };

  /**
   * @brief Serialization result.
   */
  struct SerializeResult {
    std::vector<uint8_t> data;
    uint16_t base_address;
    std::vector<uint16_t> relocations;
  };

  SpcSerializer() = default;
  ~SpcSerializer() = default;

  static absl::StatusOr<SerializeResult> SerializeSong(const MusicSong& song,
                                                       uint16_t base_address);

  /**
   * @brief Serialize a song starting from a specific segment (for seeking).
   *
   * Creates song data starting from segment_index, allowing playback to
   * resume from a specific point in the song.
   */
  static absl::StatusOr<SerializeResult> SerializeSongFromSegment(
      const MusicSong& song, int start_segment, uint16_t base_address);

  static void ApplyBaseAddress(SerializeResult* result,
                               uint16_t new_base_address);

  static int CalculateRequiredSpace(const MusicSong& song);

 private:
  static std::vector<uint8_t> SerializeTrack(const MusicTrack& track);
  static std::vector<uint8_t> SerializeNote(const Note& note,
                                            uint8_t* current_duration);
  static std::vector<uint8_t> SerializeCommand(const MusicCommand& command);

 private:

};

}  // namespace music
}  // namespace zelda3
}  // namespace yaze

#endif  // YAZE_ZELDA3_MUSIC_SPC_SERIALIZER_H
