#ifndef YAZE_ZELDA3_MUSIC_ASM_EXPORTER_H
#define YAZE_ZELDA3_MUSIC_ASM_EXPORTER_H

#include <string>
#include <unordered_map>
#include <vector>

#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "zelda3/music/song_data.h"

namespace yaze {
namespace zelda3 {
namespace music {

/**
 * @brief Options for ASM export in music_macros.asm format.
 */
struct AsmExportOptions {
  uint16_t base_aram_address = 0xD800;  // Default !ARAMAddr
  bool use_instrument_macros = true;    // %Strings() vs %SetInstrument($09)
  bool detect_subroutines = true;       // Extract repeated patterns
  int min_pattern_length = 8;           // Min bytes for subroutine extraction
  int min_pattern_repeats = 2;          // Min occurrences for extraction
  bool include_comments = true;         // Generate documentation comments
  std::string label_prefix = "MySong";  // Label naming prefix
};

/**
 * @brief Exports MusicSong to Oracle of Secrets music_macros.asm format.
 *
 * The exported ASM file can be assembled with asar and integrated into
 * Oracle of Secrets or similar ROM hack music systems.
 *
 * Output format follows the music_macros.asm conventions:
 * - Duration constants (!4th, !8th, etc.)
 * - Instrument helper macros (%Piano(), %Strings(), etc.)
 * - N-SPC command macros (%SetTempo, %SetPan, etc.)
 * - ARAM address calculations (!ARAMAddr, !ARAMC)
 */
class AsmExporter {
 public:
  AsmExporter() = default;
  ~AsmExporter() = default;

  /**
   * @brief Export a song to ASM string.
   * @param song The song to export.
   * @param options Export options.
   * @return ASM source code string or error.
   */
  absl::StatusOr<std::string> ExportSong(const MusicSong& song,
                                          const AsmExportOptions& options);

  /**
   * @brief Export a song to a file.
   * @param song The song to export.
   * @param path Output file path.
   * @param options Export options.
   * @return Status indicating success or failure.
   */
  absl::Status ExportToFile(const MusicSong& song, const std::string& path,
                            const AsmExportOptions& options);

 private:
  // Generate song header (label, !ARAMAddr, pointers)
  std::string GenerateHeader(const MusicSong& song,
                             const AsmExportOptions& options);

  // Generate channel pointer table
  std::string GenerateChannelPointers(const MusicSong& song,
                                      const AsmExportOptions& options);

  // Generate channel data
  std::string GenerateChannelData(const MusicTrack& track, int channel_index,
                                  const AsmExportOptions& options);

  // Convert a single event to ASM
  std::string ConvertEventToAsm(const TrackEvent& event,
                                const AsmExportOptions& options,
                                uint8_t& current_duration);

  // Convert note to ASM format
  std::string ConvertNoteToAsm(const Note& note,
                               const AsmExportOptions& options,
                               uint8_t& current_duration);

  // Convert command to ASM macro
  std::string ConvertCommandToAsm(const MusicCommand& cmd,
                                  const AsmExportOptions& options);

  // Duration value to constant name
  static const char* GetDurationConstant(uint8_t duration);

  // Note pitch to name (C4, G4s, etc.)
  static std::string GetNoteName(uint8_t pitch);

  // Instrument ID to macro name
  static const char* GetInstrumentMacro(uint8_t instrument_id);

  // Command opcode to macro name and format
  static const char* GetCommandMacro(uint8_t opcode);
  static int GetCommandParamCount(uint8_t opcode);
};

// =============================================================================
// Duration Constants (from music_macros.asm)
// =============================================================================

// Maps duration byte values to constant names
struct DurationConstant {
  uint8_t value;
  const char* name;
};

constexpr DurationConstant kDurationConstants[] = {
    {0x48, "!4th"},    // Quarter note (72 ticks)
    {0x6C, "!4thD"},   // Dotted quarter (108 ticks)
    {0x30, "!4thT"},   // Quarter triplet (48 ticks)
    {0x24, "!8th"},    // Eighth note (36 ticks)
    {0x36, "!8thD"},   // Dotted eighth (54 ticks)
    {0x18, "!8thT"},   // Eighth triplet (24 ticks)
    {0x12, "!16th"},   // Sixteenth (18 ticks)
    {0x1B, "!16thD"},  // Dotted sixteenth (27 ticks)
    {0x09, "!32nd"},   // Thirty-second (9 ticks)
};

// =============================================================================
// Instrument Macros (from music_macros.asm)
// =============================================================================

struct InstrumentMacro {
  uint8_t id;
  const char* macro;
};

constexpr InstrumentMacro kInstrumentMacros[] = {
    {0x02, "%Tympani()"},  {0x04, "%Sawtooth()"}, {0x05, "%Sine()"},
    {0x09, "%Strings()"},  {0x0B, "%Trombone()"}, {0x0C, "%Cymbal()"},
    {0x0D, "%Ocarina()"},  {0x0F, "%Harp()"},     {0x10, "%Splash()"},
    {0x11, "%Trumpet()"},  {0x12, "%Horn()"},     {0x13, "%Snare()"},
    {0x15, "%Choir()"},    {0x16, "%Flute()"},    {0x18, "%Piano()"},
};

}  // namespace music
}  // namespace zelda3
}  // namespace yaze

#endif  // YAZE_ZELDA3_MUSIC_ASM_EXPORTER_H
