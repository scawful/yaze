#ifndef YAZE_ZELDA3_MUSIC_MUSIC_BANK_H
#define YAZE_ZELDA3_MUSIC_MUSIC_BANK_H

#include <cstdint>
#include <string>
#include <vector>

#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "rom/rom.h"
#include "nlohmann/json.hpp"
#include "zelda3/music/song_data.h"

namespace yaze {
namespace zelda3 {
namespace music {

/**
 * @brief Manages the collection of songs, instruments, and samples from a ROM.
 *
 * The MusicBank handles:
 * - Loading all music data from ROM
 * - Saving modified music data back to ROM
 * - Managing instrument and sample definitions
 * - Tracking space usage within ROM banks
 */
class MusicBank {
 public:
  // Bank identifiers
  enum class Bank : uint8_t {
    Overworld = 0,
    Dungeon = 1,
    Credits = 2,
    OverworldExpanded = 3,  // Oracle of Secrets expanded dark world bank
    Auxiliary = 4           // SONG_POINTERS_AUX bank at $2B00
  };

  // Space usage information
  struct SpaceInfo {
    int used_bytes;
    int free_bytes;
    int total_bytes;
    float usage_percent;
    bool is_warning = false;   // >75% usage
    bool is_critical = false;  // >90% usage
    std::string recommendation;
  };

  // Expanded bank detection info (Oracle of Secrets format)
  struct ExpandedBankInfo {
    uint32_t main_rom_offset = 0;      // SongBank_OverworldExpanded_Main
    uint32_t aux_rom_offset = 0;       // SongBank_Overworld_Auxiliary
    uint16_t aux_aram_address = 0;     // SONG_POINTERS_AUX ($2B00)
    uint32_t hook_address = 0;         // LoadOverworldSongsExpanded location
    bool detected = false;
  };

  MusicBank() = default;
  ~MusicBank() = default;

  // Non-copyable, moveable
  MusicBank(const MusicBank&) = delete;
  MusicBank& operator=(const MusicBank&) = delete;
  MusicBank(MusicBank&&) = default;
  MusicBank& operator=(MusicBank&&) = default;

  // =========================================================================
  // ROM Loading/Saving
  // =========================================================================

  /**
   * @brief Load all music data from a ROM.
   * @param rom The ROM to load from.
   * @return Status indicating success or failure.
   */
  absl::Status LoadFromRom(Rom& rom);

  /**
   * @brief Save all modified music data back to ROM.
   * @param rom The ROM to save to.
   * @return Status indicating success or failure.
   */
  absl::Status SaveToRom(Rom& rom);

  /**
   * @brief Check if music data has been loaded.
   */
  bool IsLoaded() const { return loaded_; }

  // =========================================================================
  // Song Access
  // =========================================================================

  /**
   * @brief Get the number of songs loaded.
   */
  size_t GetSongCount() const { return songs_.size(); }

  /**
   * @brief Get a song by index.
   * @param index The song index (0-based).
   * @return Pointer to the song, or nullptr if index is invalid.
   */
  MusicSong* GetSong(int index);
  const MusicSong* GetSong(int index) const;

  /**
   * @brief Get a song by vanilla ID (1-based).
   * @param song_id The vanilla song ID (1-34).
   * @return Pointer to the song, or nullptr if not found.
   */
  MusicSong* GetSongById(int song_id);

  /**
   * @brief Create a new empty song.
   * @param name The name of the new song.
   * @param bank The bank to place the song in.
   * @return Index of the new song, or -1 on failure.
   */
  int CreateNewSong(const std::string& name, Bank bank);

  /**
   * @brief Delete a song by index.
   * @param index The song index to delete.
   * @return Status indicating success or failure.
   */
  absl::Status DeleteSong(int index);

  /**
   * @brief Duplicate a song.
   * @param index The song index to duplicate.
   * @return Index of the new song, or -1 on failure.
   */
  int DuplicateSong(int index);

  /**
   * @brief Check if a song is a vanilla (original) song.
   * @param index The song index to check.
   * @return True if vanilla, false if custom.
   */
  bool IsVanilla(int index) const;

  /**
   * @brief Check if the ROM has the Oracle of Secrets expanded music patch.
   * @return True if expanded music is detected.
   */
  bool HasExpandedMusicPatch() const { return expanded_bank_info_.detected; }

  /**
   * @brief Check if a song is from an expanded bank.
   * @param index The song index to check.
   * @return True if the song is in OverworldExpanded or Auxiliary bank.
   */
  bool IsExpandedSong(int index) const;

  /**
   * @brief Get information about the expanded bank configuration.
   * @return ExpandedBankInfo with detection details.
   */
  const ExpandedBankInfo& GetExpandedBankInfo() const {
    return expanded_bank_info_;
  }

  /**
   * @brief Get all songs in a specific bank.
   */
  std::vector<MusicSong*> GetSongsInBank(Bank bank);

  // =========================================================================
  // Instrument Access
  // =========================================================================

  /**
   * @brief Get the number of instruments.
   */
  size_t GetInstrumentCount() const { return instruments_.size(); }

  /**
   * @brief Get an instrument by index.
   */
  MusicInstrument* GetInstrument(int index);
  const MusicInstrument* GetInstrument(int index) const;

  /**
   * @brief Create a new instrument.
   * @param name The name of the new instrument.
   * @return Index of the new instrument, or -1 on failure.
   */
  int CreateNewInstrument(const std::string& name);

  // =========================================================================
  // Sample Access
  // =========================================================================

  /**
   * @brief Get the number of samples.
   */
  size_t GetSampleCount() const { return samples_.size(); }

  /**
   * @brief Get a sample by index.
   */
  MusicSample* GetSample(int index);
  const MusicSample* GetSample(int index) const;

  /**
   * @brief Import a WAV file as a new sample.
   * @param filepath Path to the WAV file.
   * @param name Name for the sample.
   * @return Index of the new sample, or -1 on failure.
   */
  absl::StatusOr<int> ImportSampleFromWav(const std::string& filepath,
                                           const std::string& name);

  // =========================================================================
  // Space Management
  // =========================================================================

  /**
   * @brief Calculate space usage for a bank.
   * @param bank The bank to check.
   * @return SpaceInfo with usage statistics.
   */
  SpaceInfo CalculateSpaceUsage(Bank bank) const;

  /**
   * @brief Check if all songs fit in their banks.
   * @return True if all songs fit, false if any bank overflows.
   */
  bool AllSongsFit() const;

  /**
   * @brief Get the maximum size for a bank.
   */
  static int GetBankMaxSize(Bank bank);

  /**
   * @brief Get the ROM address for a bank.
   */
  static uint32_t GetBankRomAddress(Bank bank);

  // =========================================================================
  // Modification Tracking
  // =========================================================================

  /**
   * @brief Check if any music data has been modified.
   */
  bool HasModifications() const;

  /**
   * @brief Mark all data as unmodified (after save).
   */
  void ClearModifications();

  // Serialization helpers for persistence (e.g., WASM storage)
  nlohmann::json ToJson() const;
  absl::Status LoadFromJson(const nlohmann::json& j);

 private:
  // Internal loading methods
  absl::Status LoadSongTable(Rom& rom, Bank bank,
                             std::vector<MusicSong>* custom_songs);
  absl::Status LoadInstruments(Rom& rom);
  absl::Status LoadSamples(Rom& rom);

  // Expanded bank detection and loading
  absl::Status DetectExpandedMusicPatch(Rom& rom);
  absl::Status LoadExpandedSongTable(Rom& rom,
                                     std::vector<MusicSong>* custom_songs);

  // Internal saving methods
  absl::Status SaveSongTable(Rom& rom, Bank bank);
  absl::Status SaveInstruments(Rom& rom);
  absl::Status SaveSamples(Rom& rom);

  // Calculate size of serialized song
  int CalculateSongSize(const MusicSong& song) const;

  // Data storage
  std::vector<MusicSong> songs_;
  std::vector<MusicInstrument> instruments_;
  std::vector<MusicSample> samples_;

  // State
  bool loaded_ = false;
  bool instruments_modified_ = false;
  bool samples_modified_ = false;

  // Song counts per bank (from original ROM)
  int overworld_song_count_ = 0;
  int dungeon_song_count_ = 0;
  int credits_song_count_ = 0;
  int expanded_song_count_ = 0;
  int auxiliary_song_count_ = 0;

  // Expanded bank info (Oracle of Secrets format)
  ExpandedBankInfo expanded_bank_info_;

  // Helper metadata for ROM banks
  struct BankSongRange {
    int start_id = 0;
    int end_id = -1;

    int Count() const {
      return (end_id >= start_id) ? (end_id - start_id + 1) : 0;
    }
  };

  static BankSongRange GetBankSongRange(Bank bank);
  static uint8_t GetSpcBankId(Bank bank);
  static constexpr uint16_t GetSongTableAddress() { return kSongTableAram; }
};

// =============================================================================
// Vanilla Song Names
// =============================================================================

/**
 * @brief Get the vanilla name for a song ID.
 * @param song_id The song ID (1-34).
 * @return The song name, or "Unknown" if invalid.
 */
const char* GetVanillaSongName(int song_id);

/**
 * @brief Get the bank for a vanilla song ID.
 * @param song_id The song ID (1-34).
 * @return The bank the song belongs to.
 */
MusicBank::Bank GetVanillaSongBank(int song_id);

}  // namespace music
}  // namespace zelda3
}  // namespace yaze

#endif  // YAZE_ZELDA3_MUSIC_MUSIC_BANK_H
