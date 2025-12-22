#ifndef YAZE_ZELDA3_MUSIC_SONG_DATA_H
#define YAZE_ZELDA3_MUSIC_SONG_DATA_H

#include <array>
#include <cstdint>
#include <string>
#include <vector>

namespace yaze {
namespace zelda3 {
namespace music {

// =============================================================================
// Constants
// =============================================================================

// N-SPC command parameter counts (opcodes 0xE0-0xFF)
// Index = opcode - 0xE0
constexpr int kCommandParamCount[32] = {
    1,  // $E0 SetInstrument
    1,  // $E1 SetPan
    2,  // $E2 PanFade
    3,  // $E3 VibratoOn
    0,  // $E4 VibratoOff
    1,  // $E5 SetMasterVolume
    2,  // $E6 MasterVolumeFade
    1,  // $E7 SetTempo
    2,  // $E8 TempoFade
    1,  // $E9 GlobalTranspose
    1,  // $EA ChannelTranspose
    3,  // $EB TremoloOn
    0,  // $EC TremoloOff
    1,  // $ED SetChannelVolume
    2,  // $EE ChannelVolumeFade
    3,  // $EF CallSubroutine
    1,  // $F0 VibratoFade
    3,  // $F1 PitchEnvelopeTo
    3,  // $F2 PitchEnvelopeFrom
    0,  // $F3 PitchEnvelopeOff
    1,  // $F4 Tuning
    3,  // $F5 EchoVBits
    0,  // $F6 EchoOff
    3,  // $F7 EchoParams
    3,  // $F8 EchoVolumeFade
    3,  // $F9 PitchSlide
    1,  // $FA PercussionPatch
    2,  // $FB (unused)
    0,  // $FC (unused)
    0,  // $FD (unused)
    0,  // $FE (unused)
    0   // $FF (unused)
};

// Note pitch range: C1 = 0x80, B6 = 0xC7
constexpr uint8_t kNoteMinPitch = 0x80;  // C1
constexpr uint8_t kNoteMaxPitch = 0xC7;  // B6
constexpr uint8_t kNoteTie = 0xC8;
constexpr uint8_t kNoteRest = 0xC9;
constexpr uint8_t kTrackEnd = 0x00;

// Duration constants (ticks, quarter note = 72)
constexpr uint8_t kDurationQuarter = 0x48;       // 72 ticks
constexpr uint8_t kDurationQuarterDot = 0x6C;    // 108 ticks
constexpr uint8_t kDurationQuarterTrip = 0x30;   // 48 ticks
constexpr uint8_t kDurationEighth = 0x24;        // 36 ticks
constexpr uint8_t kDurationEighthDot = 0x36;     // 54 ticks
constexpr uint8_t kDurationEighthTrip = 0x18;    // 24 ticks
constexpr uint8_t kDurationSixteenth = 0x12;     // 18 ticks
constexpr uint8_t kDurationSixteenthDot = 0x1B;  // 27 ticks
constexpr uint8_t kDurationThirtySecond = 0x09;  // 9 ticks

// ROM addresses for song table blocks (PC file offsets for headerless ROM)
// From usdasm disassembly:
// - Banks 0, 1, 3 are contiguous starting at $19:8000 (PC 0xC8000)
// - Bank 2 (dungeon) is separate at $1B:8000 (PC 0xD8000)
// Song table blocks (loading to ARAM $D000):
constexpr uint32_t kOverworldBankRom = 0xD1EF5;  // $1A:9EF5
constexpr uint32_t kDungeonBankRom = 0xD8000;    // $1B:8000
constexpr uint32_t kCreditsBankRom = 0xD5380;    // $1A:D380

// ARAM addresses
constexpr uint16_t kSongTableAram = 0xD000;
constexpr uint16_t kInstrumentTableAram = 0x3D00;
constexpr uint16_t kSampleTableAram = 0x3C00;
constexpr uint16_t kSampleDataAram = 0x4000;

// Bank size limits
constexpr int kOverworldBankMaxSize = 12032;  // 0x2F00
constexpr int kDungeonBankMaxSize = 11200;
constexpr int kCreditsBankMaxSize = 4200;

// =============================================================================
// N-SPC Pitch Table
// =============================================================================

// N-SPC pitch table - maps note values 0x80-0xC7 to DSP pitch register values.
// The DSP pitch is a 14-bit value where 0x1000 = base sample rate (32kHz).
// Values derived from ALTTP N-SPC driver analysis.
// Notes: C1 ($80) through B6 ($C7) = 72 entries, each octave doubles.
constexpr uint16_t kNSpcPitchTable[72] = {
    // Octave 1: C1-B1 (note $80-$8B)
    0x0086, 0x008E, 0x0096, 0x009F, 0x00A9, 0x00B3,
    0x00BE, 0x00C9, 0x00D6, 0x00E3, 0x00F1, 0x00FF,
    // Octave 2: C2-B2 (note $8C-$97)
    0x010C, 0x011C, 0x012C, 0x013E, 0x0152, 0x0166,
    0x017C, 0x0192, 0x01AC, 0x01C6, 0x01E2, 0x01FE,
    // Octave 3: C3-B3 (note $98-$A3)
    0x0218, 0x0238, 0x0258, 0x027C, 0x02A4, 0x02CC,
    0x02F8, 0x0324, 0x0358, 0x038C, 0x03C4, 0x03FC,
    // Octave 4: C4-B4 (note $A4-$AF)
    0x0430, 0x0470, 0x04B0, 0x04F8, 0x0548, 0x0598,
    0x05F0, 0x0648, 0x06B0, 0x0718, 0x0788, 0x07F8,
    // Octave 5: C5-B5 (note $B0-$BB)
    0x0860, 0x08E0, 0x0960, 0x09F0, 0x0A90, 0x0B30,
    0x0BE0, 0x0C90, 0x0D60, 0x0E30, 0x0F10, 0x0FF0,
    // Octave 6: C6-B6 (note $BC-$C7)
    0x10C0, 0x11C0, 0x12C0, 0x13E0, 0x1520, 0x1660,
    0x17C0, 0x1920, 0x1AC0, 0x1C60, 0x1E20, 0x1FE0,
};

/**
 * @brief Look up the DSP pitch value for an N-SPC note byte.
 * @param note_byte The note value (0x80-0xC7 for C1-B6).
 * @return The DSP pitch register value, or 0x1000 (base) for invalid notes.
 */
inline uint16_t LookupNSpcPitch(uint8_t note_byte) {
  if (note_byte < kNoteMinPitch || note_byte > kNoteMaxPitch) {
    return 0x1000;  // Base pitch for invalid notes
  }
  return kNSpcPitchTable[note_byte - kNoteMinPitch];
}

// =============================================================================
// Expanded Bank Constants (Oracle of Secrets format)
// =============================================================================

// Oracle of Secrets expanded music system addresses
constexpr uint32_t kExpandedOverworldBankRom = 0x1A9EF5;  // SongBank_OverworldExpanded_Main
constexpr uint32_t kExpandedAuxBankRom = 0x1ACCA7;        // SongBank_Overworld_Auxiliary
constexpr uint16_t kAuxSongTableAram = 0x2B00;            // SONG_POINTERS_AUX
constexpr int kExpandedOverworldBankMaxSize = 0x2DAE;     // ~11KB
constexpr int kAuxBankMaxSize = 0x0688;                   // ~1.6KB

// Hook point for expanded music detection
constexpr uint32_t kExpandedMusicHookAddress = 0x008919;  // LoadOverworldSongs
constexpr uint8_t kJslOpcode = 0x22;                      // JSL instruction

// =============================================================================
// Command Types
// =============================================================================

enum class CommandType : uint8_t {
  SetInstrument = 0xE0,
  SetPan = 0xE1,
  PanFade = 0xE2,
  VibratoOn = 0xE3,
  VibratoOff = 0xE4,
  SetMasterVolume = 0xE5,
  MasterVolumeFade = 0xE6,
  SetTempo = 0xE7,
  TempoFade = 0xE8,
  GlobalTranspose = 0xE9,
  ChannelTranspose = 0xEA,
  TremoloOn = 0xEB,
  TremoloOff = 0xEC,
  SetChannelVolume = 0xED,
  ChannelVolumeFade = 0xEE,
  CallSubroutine = 0xEF,
  VibratoFade = 0xF0,
  PitchEnvelopeTo = 0xF1,
  PitchEnvelopeFrom = 0xF2,
  PitchEnvelopeOff = 0xF3,
  Tuning = 0xF4,
  EchoVBits = 0xF5,
  EchoOff = 0xF6,
  EchoParams = 0xF7,
  EchoVolumeFade = 0xF8,
  PitchSlide = 0xF9,
  PercussionPatch = 0xFA
};

// =============================================================================
// Data Structures
// =============================================================================

/**
 * @brief Represents a single musical note.
 *
 * Notes have a pitch (C1-B6), duration in ticks, and optional velocity.
 * Special pitch values: 0xC8 = tie (extend previous), 0xC9 = rest.
 */
struct Note {
  uint8_t pitch = kNoteRest;      // 0x80-0xC7 for notes, 0xC8=tie, 0xC9=rest
  uint8_t duration = 0;           // Duration in ticks (quarter = 72)
  uint8_t velocity = 0;           // Optional articulation byte
  bool has_duration_prefix = false;  // True if duration byte precedes note

  // Helper methods
  bool IsNote() const { return pitch >= kNoteMinPitch && pitch <= kNoteMaxPitch; }
  bool IsTie() const { return pitch == kNoteTie; }
  bool IsRest() const { return pitch == kNoteRest; }

  // Convert pitch to note name (e.g., "C4", "F#3")
  std::string GetNoteName() const;

  // Convert pitch to octave (1-6)
  int GetOctave() const {
    if (!IsNote()) return 0;
    return ((pitch - kNoteMinPitch) / 12) + 1;
  }

  // Convert pitch to semitone within octave (0-11)
  int GetSemitone() const {
    if (!IsNote()) return 0;
    return (pitch - kNoteMinPitch) % 12;
  }
};

/**
 * @brief Represents an N-SPC command (opcodes 0xE0-0xFF).
 */
struct MusicCommand {
  uint8_t opcode = 0;
  std::array<uint8_t, 3> params = {0, 0, 0};

  // Get parameter count for this command
  int GetParamCount() const {
    if (opcode < 0xE0) return 0;
    return kCommandParamCount[opcode - 0xE0];
  }

  // Get command type
  CommandType GetType() const { return static_cast<CommandType>(opcode); }

  // Helper for subroutine commands
  bool IsSubroutine() const { return opcode == 0xEF; }
  uint16_t GetSubroutineAddress() const {
    return static_cast<uint16_t>(params[0]) |
           (static_cast<uint16_t>(params[1]) << 8);
  }
  uint8_t GetSubroutineRepeatCount() const { return params[2]; }
};

/**
 * @brief A single event in a music track (note, command, or control).
 */
struct TrackEvent {
  enum class Type { Note, Command, SubroutineCall, End };

  Type type = Type::End;
  uint16_t tick = 0;        // Absolute position in ticks
  uint16_t rom_offset = 0;  // Original ROM address (for round-trip)

  // Data (only one is valid based on type)
  Note note;
  MusicCommand command;

  // Factory methods
  static TrackEvent MakeNote(uint16_t tick, uint8_t pitch, uint8_t duration,
                             uint8_t velocity = 0) {
    TrackEvent event;
    event.type = Type::Note;
    event.tick = tick;
    event.note.pitch = pitch;
    event.note.duration = duration;
    event.note.velocity = velocity;
    return event;
  }

  static TrackEvent MakeCommand(uint16_t tick, uint8_t opcode,
                                uint8_t p1 = 0, uint8_t p2 = 0, uint8_t p3 = 0) {
    TrackEvent event;
    event.type = Type::Command;
    event.tick = tick;
    event.command.opcode = opcode;
    event.command.params = {p1, p2, p3};
    return event;
  }

  static TrackEvent MakeEnd(uint16_t tick) {
    TrackEvent event;
    event.type = Type::End;
    event.tick = tick;
    return event;
  }
};

/**
 * @brief One of 8 channels in a music segment.
 */
struct MusicTrack {
  std::vector<TrackEvent> events;
  uint16_t rom_address = 0;
  uint16_t duration_ticks = 0;
  bool is_empty = true;

  // Calculate total duration from events
  void CalculateDuration();

  // Get event at tick position (or nullptr)
  const TrackEvent* GetEventAtTick(uint16_t tick) const;

  // Insert event maintaining tick order
  void InsertEvent(TrackEvent event);

  // Remove event at index
  void RemoveEvent(size_t index);
};

/**
 * @brief A segment containing 8 parallel tracks.
 *
 * Songs are composed of one or more segments that play sequentially.
 */
struct MusicSegment {
  std::array<MusicTrack, 8> tracks;
  uint16_t rom_address = 0;

  // Get the longest track duration
  uint16_t GetDuration() const {
    uint16_t max_duration = 0;
    for (const auto& track : tracks) {
      if (track.duration_ticks > max_duration) {
        max_duration = track.duration_ticks;
      }
    }
    return max_duration;
  }
};

/**
 * @brief A complete song composed of segments.
 */
struct MusicSong {
  std::string name;
  std::vector<MusicSegment> segments;
  int loop_point = -1;       // Segment index to loop to, -1 = no loop
  uint8_t bank = 0;          // 0=overworld, 1=dungeon, 2=credits
  uint16_t rom_address = 0;
  bool modified = false;

  // Get total song duration in ticks
  uint32_t GetTotalDuration() const {
    uint32_t total = 0;
    for (const auto& segment : segments) {
      total += segment.GetDuration();
    }
    return total;
  }

  // Check if song loops
  bool HasLoop() const { return loop_point >= 0; }
};

/**
 * @brief An instrument definition with ADSR envelope.
 */
struct MusicInstrument {
  uint8_t sample_index = 0;
  uint8_t attack = 0;        // Attack rate (0-15)
  uint8_t decay = 0;         // Decay rate (0-7)
  uint8_t sustain_level = 0; // Sustain level (0-7)
  uint8_t sustain_rate = 0;  // Sustain rate (0-31)
  uint8_t gain = 0;          // Gain mode/value
  uint16_t pitch_mult = 0;   // Pitch multiplier
  std::string name;

  // Pack ADSR bytes (AD, SR format)
  uint8_t GetADByte() const {
    return (attack & 0x0F) | ((decay & 0x07) << 4) | 0x80;
  }
  uint8_t GetSRByte() const {
    return (sustain_rate & 0x1F) | ((sustain_level & 0x07) << 5);
  }

  // Unpack ADSR from bytes
  void SetFromBytes(uint8_t ad, uint8_t sr) {
    attack = ad & 0x0F;
    decay = (ad >> 4) & 0x07;
    sustain_rate = sr & 0x1F;
    sustain_level = (sr >> 5) & 0x07;
  }
};

/**
 * @brief A BRR-encoded audio sample.
 */
struct MusicSample {
  std::vector<int16_t> pcm_data;   // Decoded PCM (for display/editing)
  std::vector<uint8_t> brr_data;   // Encoded BRR (for ROM)
  uint16_t loop_point = 0;         // Loop start in samples
  bool loops = false;
  std::string name;

  // Get duration in samples
  size_t GetSampleCount() const { return pcm_data.size(); }

  // Check if sample is loaded
  bool IsLoaded() const { return !pcm_data.empty() || !brr_data.empty(); }
};

// =============================================================================
// Default Instrument Names
// =============================================================================

constexpr const char* kDefaultInstrumentNames[] = {
    "Noise",     // $00
    "Tympani",   // $01
    "Trombone",  // $02
    "Ocarina",   // $03
    "Harp",      // $04
    "Splash",    // $05
    "Trumpet",   // $06
    "Horn",      // $07
    "Snare",     // $08
    "Choir",     // $09
    "Flute",     // $0A
    "Piano",     // $0B
    "Cymbal",    // $0C
    "Strings",   // $0D
    "Sawtooth",  // $0E
    "Sine"       // $0F
};

constexpr const char* kNoteNames[] = {
    "C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B"
};

// =============================================================================
// Inline Implementations
// =============================================================================

inline std::string Note::GetNoteName() const {
  if (IsTie()) return "---";
  if (IsRest()) return "...";
  if (!IsNote()) return "???";

  int semitone = GetSemitone();
  int octave = GetOctave();
  return std::string(kNoteNames[semitone]) + std::to_string(octave);
}

inline void MusicTrack::CalculateDuration() {
  duration_ticks = 0;
  for (const auto& event : events) {
    if (event.type == TrackEvent::Type::Note) {
      uint16_t end_tick = event.tick + event.note.duration;
      if (end_tick > duration_ticks) {
        duration_ticks = end_tick;
      }
    } else if (event.type == TrackEvent::Type::End) {
      if (event.tick > duration_ticks) {
        duration_ticks = event.tick;
      }
    }
  }
  is_empty = events.empty();
}

inline const TrackEvent* MusicTrack::GetEventAtTick(uint16_t tick) const {
  for (const auto& event : events) {
    if (event.tick == tick) return &event;
  }
  return nullptr;
}

inline void MusicTrack::InsertEvent(TrackEvent event) {
  auto it = events.begin();
  while (it != events.end() && it->tick <= event.tick) {
    ++it;
  }
  events.insert(it, event);
  is_empty = false;
  CalculateDuration();
}

inline void MusicTrack::RemoveEvent(size_t index) {
  if (index < events.size()) {
    events.erase(events.begin() + static_cast<ptrdiff_t>(index));
    is_empty = events.empty();
    CalculateDuration();
  }
}

}  // namespace music
}  // namespace zelda3
}  // namespace yaze

#endif  // YAZE_ZELDA3_MUSIC_SONG_DATA_H
