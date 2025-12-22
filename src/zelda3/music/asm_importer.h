#ifndef YAZE_ZELDA3_MUSIC_ASM_IMPORTER_H
#define YAZE_ZELDA3_MUSIC_ASM_IMPORTER_H

#include <string>
#include <unordered_map>
#include <vector>

#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "zelda3/music/asm_exporter.h"
#include "zelda3/music/song_data.h"

namespace yaze {
namespace zelda3 {
namespace music {

/**
 * @brief Options for ASM import from music_macros.asm format.
 */
struct AsmImportOptions {
  bool strict_mode = false;       // Fail on unknown macros/syntax
  bool resolve_subroutines = true;  // Inline subroutine calls
  bool verbose_errors = true;     // Include line numbers in errors
};

/**
 * @brief Parse result with diagnostics.
 */
struct AsmParseResult {
  MusicSong song;
  std::vector<std::string> warnings;
  std::vector<std::string> errors;
  int lines_parsed = 0;
  int bytes_generated = 0;
};

/**
 * @brief Imports music_macros.asm format files into MusicSong.
 *
 * Supports the Oracle of Secrets music_macros.asm conventions:
 * - Duration constants (!4th, !8th, etc.)
 * - Instrument helper macros (%Piano(), %Strings(), etc.)
 * - N-SPC command macros (%SetTempo, %SetPan, etc.)
 * - ARAM address calculations (!ARAMAddr, !ARAMC)
 * - Channel labels (.Channel0, .Channel1, etc.)
 */
class AsmImporter {
 public:
  AsmImporter() = default;
  ~AsmImporter() = default;

  /**
   * @brief Import a song from ASM string.
   * @param asm_source The ASM source code.
   * @param options Import options.
   * @return Parse result with song and diagnostics.
   */
  absl::StatusOr<AsmParseResult> ImportSong(const std::string& asm_source,
                                             const AsmImportOptions& options);

  /**
   * @brief Import a song from a file.
   * @param path Input file path.
   * @param options Import options.
   * @return Parse result with song and diagnostics.
   */
  absl::StatusOr<AsmParseResult> ImportFromFile(
      const std::string& path, const AsmImportOptions& options);

 private:
  // Parsing state
  struct ParseState {
    int line_number = 0;
    int current_channel = -1;
    uint8_t current_duration = 0;
    std::string song_label;
    uint16_t aram_address = 0xD800;
    std::unordered_map<std::string, int> label_to_channel;
    std::unordered_map<std::string, std::vector<TrackEvent>> subroutines;
    std::vector<std::string> warnings;
    std::vector<std::string> errors;
    int bytes_generated = 0;
  };

  // Line parsing
  absl::Status ParseLine(const std::string& line, MusicSong& song,
                         ParseState& state, const AsmImportOptions& options);

  // Label parsing
  bool ParseLabel(const std::string& line, ParseState& state);

  // Directive parsing (!ARAMAddr, etc.)
  bool ParseDirective(const std::string& line, ParseState& state);

  // Data byte parsing (db statements)
  absl::Status ParseDataBytes(const std::string& line, MusicSong& song,
                              ParseState& state,
                              const AsmImportOptions& options);

  // Macro parsing (%SetTempo, %Piano, etc.)
  absl::StatusOr<std::vector<TrackEvent>> ParseMacro(
      const std::string& macro_call, ParseState& state,
      const AsmImportOptions& options);

  // Note name parsing (C4, G4s, Rest, Tie -> byte values)
  absl::StatusOr<uint8_t> ParseNoteName(const std::string& note_name);

  // Duration constant parsing (!4th -> 0x48)
  absl::StatusOr<uint8_t> ParseDurationConstant(const std::string& duration);

  // Instrument macro resolution (%Piano() -> SetInstrument($18))
  absl::StatusOr<MusicCommand> ResolveInstrumentMacro(
      const std::string& macro_name);

  // Command macro resolution (%SetTempo($80) -> command bytes)
  absl::StatusOr<MusicCommand> ResolveCommandMacro(
      const std::string& macro_name, const std::vector<uint8_t>& params);

  // Helper to extract macro name and parameters
  bool ParseMacroCall(const std::string& call, std::string& macro_name,
                      std::vector<std::string>& params);

  // Helper to parse hex value ($XX or 0xXX)
  absl::StatusOr<uint8_t> ParseHexValue(const std::string& value);

  // Trim whitespace
  static std::string Trim(const std::string& s);
};

// =============================================================================
// Note Name Mappings (for import)
// =============================================================================

// Maps note names to pitch values
struct NoteNameMapping {
  const char* name;
  uint8_t pitch;
};

// Standard notes (C1-B6)
constexpr NoteNameMapping kAsmNoteNames[] = {
    // Octave 1
    {"C1", 0x80},
    {"Cs1", 0x81},
    {"D1", 0x82},
    {"Ds1", 0x83},
    {"E1", 0x84},
    {"F1", 0x85},
    {"Fs1", 0x86},
    {"G1", 0x87},
    {"Gs1", 0x88},
    {"A1", 0x89},
    {"As1", 0x8A},
    {"B1", 0x8B},
    // Octave 2
    {"C2", 0x8C},
    {"Cs2", 0x8D},
    {"D2", 0x8E},
    {"Ds2", 0x8F},
    {"E2", 0x90},
    {"F2", 0x91},
    {"Fs2", 0x92},
    {"G2", 0x93},
    {"Gs2", 0x94},
    {"A2", 0x95},
    {"As2", 0x96},
    {"B2", 0x97},
    // Octave 3
    {"C3", 0x98},
    {"Cs3", 0x99},
    {"D3", 0x9A},
    {"Ds3", 0x9B},
    {"E3", 0x9C},
    {"F3", 0x9D},
    {"Fs3", 0x9E},
    {"G3", 0x9F},
    {"Gs3", 0xA0},
    {"A3", 0xA1},
    {"As3", 0xA2},
    {"B3", 0xA3},
    // Octave 4
    {"C4", 0xA4},
    {"Cs4", 0xA5},
    {"D4", 0xA6},
    {"Ds4", 0xA7},
    {"E4", 0xA8},
    {"F4", 0xA9},
    {"Fs4", 0xAA},
    {"G4", 0xAB},
    {"Gs4", 0xAC},
    {"A4", 0xAD},
    {"As4", 0xAE},
    {"B4", 0xAF},
    // Octave 5
    {"C5", 0xB0},
    {"Cs5", 0xB1},
    {"D5", 0xB2},
    {"Ds5", 0xB3},
    {"E5", 0xB4},
    {"F5", 0xB5},
    {"Fs5", 0xB6},
    {"G5", 0xB7},
    {"Gs5", 0xB8},
    {"A5", 0xB9},
    {"As5", 0xBA},
    {"B5", 0xBB},
    // Octave 6
    {"C6", 0xBC},
    {"Cs6", 0xBD},
    {"D6", 0xBE},
    {"Ds6", 0xBF},
    {"E6", 0xC0},
    {"F6", 0xC1},
    {"Fs6", 0xC2},
    {"G6", 0xC3},
    {"Gs6", 0xC4},
    {"A6", 0xC5},
    {"As6", 0xC6},
    {"B6", 0xC7},
    // Special
    {"Tie", 0xC8},
    {"Rest", 0xC9},
    {"End", 0x00},
};

// Maps command macro names to opcodes
struct CommandMacroMapping {
  const char* name;
  uint8_t opcode;
  int param_count;
};

constexpr CommandMacroMapping kCommandMacros[] = {
    {"SetInstrument", 0xE0, 1},
    {"SetPan", 0xE1, 1},
    {"PanFade", 0xE2, 2},
    {"VibratoOn", 0xE3, 3},
    {"VibratoOff", 0xE4, 0},
    {"SetMasterVolume", 0xE5, 1},
    {"MasterVolumeFade", 0xE6, 2},
    {"SetTempo", 0xE7, 1},
    {"TempoFade", 0xE8, 2},
    {"GlobalTranspose", 0xE9, 1},
    {"ChannelTranspose", 0xEA, 1},
    {"TremoloOn", 0xEB, 3},
    {"TremoloOff", 0xEC, 0},
    {"SetChannelVolume", 0xED, 1},
    {"ChannelVolumeFade", 0xEE, 2},
    {"CallSubroutine", 0xEF, 3},
    {"VibratoFade", 0xF0, 1},
    {"PitchEnvelopeTo", 0xF1, 3},
    {"PitchEnvelopeFrom", 0xF2, 3},
    {"PitchEnvelopeOff", 0xF3, 0},
    {"Tuning", 0xF4, 1},
    {"EchoVBits", 0xF5, 1},
    {"EchoOff", 0xF6, 0},
    {"EchoParams", 0xF7, 3},
    {"EchoVolumeFade", 0xF8, 3},
    {"PitchSlide", 0xF9, 3},
    {"PercussionPatch", 0xFA, 1},
};

// Maps instrument macro names to instrument IDs
struct InstrumentMacroMapping {
  const char* macro;
  uint8_t id;
};

constexpr InstrumentMacroMapping kInstrumentMacroImport[] = {
    {"Tympani", 0x02},  {"Sawtooth", 0x04}, {"Sine", 0x05},
    {"Strings", 0x09},  {"Trombone", 0x0B}, {"Cymbal", 0x0C},
    {"Ocarina", 0x0D},  {"Harp", 0x0F},     {"Splash", 0x10},
    {"Trumpet", 0x11},  {"Horn", 0x12},     {"Snare", 0x13},
    {"Choir", 0x15},    {"Flute", 0x16},    {"Piano", 0x18},
};

}  // namespace music
}  // namespace zelda3
}  // namespace yaze

#endif  // YAZE_ZELDA3_MUSIC_ASM_IMPORTER_H
