#ifndef YAZE_ZELDA3_MUSIC_SPC_PARSER_H
#define YAZE_ZELDA3_MUSIC_SPC_PARSER_H

#include <cstdint>
#include <vector>

#include "absl/status/statusor.h"
#include "rom/rom.h"
#include "zelda3/music/song_data.h"

namespace yaze {
namespace zelda3 {
namespace music {

// ROM offsets for sound bank block headers (from usdasm disassembly)
// Each bank has a block header: [size:2][aram_addr:2][data:size]
//
// Banks 0, 1, 3 are contiguous starting at $19:8000 (PC 0xC8000)
// Bank 2 (dungeon) is separate at $1B:8000 (PC 0xD8000)
//
// Song table blocks (loading to ARAM $D000):
// - Overworld: $1A:9EF5 (PC 0xD1EF5)
// - Dungeon:   $1B:8000 (PC 0xD8000)
// - Credits:   $1A:D380 (PC 0xD5380)
constexpr uint32_t kSoundBankOffsets[] = {
    0xC8000,   // Bank 0 (common) - start of contiguous region
    0xD1EF5,   // Bank 1 (overworld) - song table block
    0xD8000,   // Bank 2 (dungeon) - separate region
    0xD5380    // Bank 3 (credits) - song table block
};

/**
 * @brief Parser for N-SPC music data from ROM.
 *
 * This class provides a clean interface for parsing the game's music format,
 * which consists of:
 * - Song tables with pointers to segment data
 * - Segments with 8 track pointers
 * - Tracks with note/command sequences
 *
 * The implementation is a modernized version of Hyrule Magic's tracker logic.
 */
class SpcParser {
 public:
  /**
   * @brief Context for parsing operations.
   */
  struct ParseContext {
    Rom* rom = nullptr;
    uint8_t current_bank = 0;
    uint32_t bank_offset = 0;

    // Parsing state
    std::vector<uint16_t> visited_addresses;  // Prevent infinite loops
    int max_parse_depth = 100;
    int current_depth = 0;
  };

  // =========================================================================
  // Song Parsing
  // =========================================================================

  /**
   * @brief Parse a complete song from ROM.
   * @param rom The ROM to parse from.
   * @param address The song table address (SPC address space).
   * @param bank The sound bank (0-3).
   * @return The parsed song, or error status.
   */
  static absl::StatusOr<MusicSong> ParseSong(Rom& rom, uint16_t address,
                                              uint8_t bank);

  /**
   * @brief Read the song pointer table for a given SPC bank.
   * @param rom The ROM to read from.
   * @param table_address Address of the pointer table (typically $D000).
   * @param bank The SPC bank identifier.
   * @param max_entries Maximum number of entries to read (default 40).
   * @return Vector of song pointer addresses (including null entries).
   */
  static absl::StatusOr<std::vector<uint16_t>> ReadSongPointerTable(
      Rom& rom, uint16_t table_address, uint8_t bank, int max_entries = 40);

  /**
   * @brief Parse a single track from ROM.
   * @param rom The ROM to parse from.
   * @param address The track start address (SPC address space).
   * @param bank The sound bank (0-3).
   * @param max_ticks Maximum ticks to parse (prevents infinite loops).
   * @return The parsed track, or error status.
   */
  static absl::StatusOr<MusicTrack> ParseTrack(Rom& rom, uint16_t address,
                                                uint8_t bank,
                                                int max_ticks = 50000);

  // =========================================================================
  // Address Resolution
  // =========================================================================

  /**
   * @brief Convert an SPC address to a ROM offset.
   * @param rom The ROM (for reading bank pointers).
   * @param spc_address The address in SPC RAM space.
   * @param bank The sound bank.
   * @return The ROM file offset, or 0 if not found.
   */
  static uint32_t SpcAddressToRomOffset(Rom& rom, uint16_t spc_address,
                                         uint8_t bank);

  /**
   * @brief Get a pointer to ROM data at an SPC address.
   * @param rom The ROM.
   * @param spc_address The address in SPC RAM space.
   * @param bank The sound bank.
   * @param out_length Receives the length of the data block.
   * @return Pointer to ROM data, or nullptr if not found.
   */
  static const uint8_t* GetSpcData(Rom& rom, uint16_t spc_address,
                                    uint8_t bank, int* out_length = nullptr);

  // =========================================================================
  // Command Utilities
  // =========================================================================

  /**
   * @brief Get the parameter count for an N-SPC command.
   * @param opcode The command opcode (0xE0-0xFF).
   * @return Number of parameter bytes, or 0 for notes.
   */
  static int GetCommandParamCount(uint8_t opcode) {
    if (opcode < 0xE0) return 0;
    return kCommandParamCount[opcode - 0xE0];
  }

  /**
   * @brief Check if a byte is a note pitch.
   */
  static bool IsNotePitch(uint8_t byte) {
    return byte >= kNoteMinPitch && byte <= kNoteRest;
  }

  /**
   * @brief Check if a byte is a duration value (< 128).
   */
  static bool IsDuration(uint8_t byte) {
    return byte < 0x80;
  }

  /**
   * @brief Check if a byte is a command opcode.
   */
  static bool IsCommand(uint8_t byte) {
    return byte >= 0xE0;
  }

 private:
  // Internal parsing with context
  static absl::StatusOr<MusicTrack> ParseTrackInternal(ParseContext& ctx,
                                                        uint16_t address,
                                                        int max_ticks);

  // Parse subroutine call
  static absl::StatusOr<int> ParseSubroutine(ParseContext& ctx,
                                              uint16_t address,
                                              int repeat_count,
                                              int remaining_ticks);
};

/**
 * @brief Serializer for N-SPC music data to ROM format.
 *
 * This class handles converting the internal song representation back to
 * the binary format expected by the SNES APU.
 */
class SpcSerializer {
 public:
  /**
   * @brief Result of serialization with relocation info.
   */
  struct SerializeResult {
    std::vector<uint8_t> data;
    std::vector<uint16_t> relocations;  // Offsets that need address fixup
    uint16_t base_address = 0;
  };

  // =========================================================================
  // Song Serialization
  // =========================================================================

  /**
   * @brief Serialize a complete song to binary format.
   * @param song The song to serialize.
   * @param base_address The target SPC address.
   * @return Serialization result with binary data and relocations.
   */
  static absl::StatusOr<SerializeResult> SerializeSong(const MusicSong& song,
                                                        uint16_t base_address);

  /**
   * @brief Serialize a single track to binary format.
   * @param track The track to serialize.
   * @return Binary data for the track.
   */
  static std::vector<uint8_t> SerializeTrack(const MusicTrack& track);

  /**
   * @brief Calculate the space required for a song.
   * @param song The song to measure.
   * @return Size in bytes.
   */
  static int CalculateRequiredSpace(const MusicSong& song);

  // =========================================================================
  // Event Serialization
  // =========================================================================

  /**
   * @brief Serialize a note event.
   * @param note The note to serialize.
   * @param current_duration The current duration state.
   * @return Binary bytes for the note.
   */
  static std::vector<uint8_t> SerializeNote(const Note& note,
                                             uint8_t* current_duration);

  /**
   * @brief Serialize a command event.
   * @param command The command to serialize.
   * @return Binary bytes for the command.
   */
  static std::vector<uint8_t> SerializeCommand(const MusicCommand& command);

  /**
   * @brief Adjust all serialized pointers to a new base address.
   * @param result Serialized blob to patch (in-place).
   * @param new_base_address Target SPC base address.
   */
  static void ApplyBaseAddress(SerializeResult* result,
                               uint16_t new_base_address);
};

/**
 * @brief BRR sample encoder/decoder.
 *
 * Handles conversion between PCM audio and the SNES BRR format.
 */
class BrrCodec {
 public:
  /**
   * @brief Decode BRR data to PCM samples.
   * @param brr_data The BRR-encoded data.
   * @param loop_start Receives the loop start point.
   * @return Decoded 16-bit PCM samples.
   */
  static std::vector<int16_t> Decode(const std::vector<uint8_t>& brr_data,
                                      int* loop_start = nullptr);

  /**
   * @brief Encode PCM samples to BRR format.
   * @param pcm_data The source PCM samples.
   * @param loop_start Loop start point (-1 for no loop).
   * @return BRR-encoded data.
   */
  static std::vector<uint8_t> Encode(const std::vector<int16_t>& pcm_data,
                                      int loop_start = -1);

 private:
  // Filter coefficients for BRR decoding
  static constexpr int kFilter1[4] = {0, 15, 61, 115};
  static constexpr int kFilter2[4] = {0, 4, 5, 6};
  static constexpr int kFilter3[4] = {0, 0, 15, 13};
};

}  // namespace music
}  // namespace zelda3
}  // namespace yaze

#endif  // YAZE_ZELDA3_MUSIC_SPC_PARSER_H
