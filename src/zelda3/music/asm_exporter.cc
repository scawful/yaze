#include "zelda3/music/asm_exporter.h"

#include <fstream>
#include <sstream>

#include "absl/strings/str_format.h"

namespace yaze {
namespace zelda3 {
namespace music {

absl::StatusOr<std::string> AsmExporter::ExportSong(
    const MusicSong& song, const AsmExportOptions& options) {
  std::ostringstream out;

  // File header comment
  if (options.include_comments) {
    out << "; =============================================================================\n";
    out << "; " << song.name << "\n";
    out << "; Exported from yaze music editor\n";
    out << "; Format: Oracle of Secrets music_macros.asm compatible\n";
    out << "; =============================================================================\n\n";
  }

  // Generate song header
  out << GenerateHeader(song, options);
  out << "\n";

  // Generate channel pointer table
  out << GenerateChannelPointers(song, options);
  out << "\n";

  // Generate channel data for each segment
  for (size_t seg_idx = 0; seg_idx < song.segments.size(); ++seg_idx) {
    const auto& segment = song.segments[seg_idx];

    if (options.include_comments && song.segments.size() > 1) {
      out << "; --- Segment " << seg_idx << " ---\n";
    }

    for (int ch = 0; ch < 8; ++ch) {
      const auto& track = segment.tracks[ch];
      if (track.is_empty) continue;

      out << GenerateChannelData(track, ch, options);
      out << "\n";
    }
  }

  return out.str();
}

absl::Status AsmExporter::ExportToFile(const MusicSong& song,
                                       const std::string& path,
                                       const AsmExportOptions& options) {
  auto result = ExportSong(song, options);
  if (!result.ok()) {
    return result.status();
  }

  std::ofstream file(path);
  if (!file.is_open()) {
    return absl::FailedPreconditionError(
        absl::StrFormat("Failed to open file for writing: %s", path));
  }

  file << result.value();
  file.close();

  return absl::OkStatus();
}

std::string AsmExporter::GenerateHeader(const MusicSong& song,
                                        const AsmExportOptions& options) {
  std::ostringstream out;

  // Song label
  out << options.label_prefix << ":\n";

  // ARAM address constant
  out << absl::StrFormat("!ARAMAddr = $%04X\n", options.base_aram_address);

  // Section pointers (simplified - intro and loop point to same for now)
  // In a full implementation, we'd calculate actual segment offsets
  uint16_t intro_offset = 0x0A;  // After header
  uint16_t loop_offset = 0x1A;   // After intro

  out << absl::StrFormat("dw !ARAMAddr+$%02X      ; Intro section\n",
                         intro_offset);
  if (song.HasLoop()) {
    out << absl::StrFormat("dw !ARAMAddr+$%02X      ; Loop section\n",
                           loop_offset);
  } else {
    out << "dw $00FF              ; Default fade\n";
  }
  out << "dw !ARAMAddr+$02      ; Loop start\n";
  out << "dw $0000              ; Reserved\n";

  return out.str();
}

std::string AsmExporter::GenerateChannelPointers(
    const MusicSong& song, const AsmExportOptions& options) {
  std::ostringstream out;

  out << ".Channels\n";
  out << absl::StrFormat("!ARAMC = !ARAMAddr-%s\n", options.label_prefix);

  // Generate 8 channel pointers
  for (int ch = 0; ch < 8; ++ch) {
    bool has_data = false;
    for (const auto& segment : song.segments) {
      if (!segment.tracks[ch].is_empty) {
        has_data = true;
        break;
      }
    }

    if (has_data) {
      out << absl::StrFormat("dw .Channel%d+!ARAMC\n", ch);
    } else {
      out << "dw $0000              ; Unused\n";
    }
  }

  return out.str();
}

std::string AsmExporter::GenerateChannelData(const MusicTrack& track,
                                             int channel_index,
                                             const AsmExportOptions& options) {
  std::ostringstream out;

  out << absl::StrFormat(".Channel%d\n", channel_index);

  uint8_t current_duration = 0;

  for (const auto& event : track.events) {
    std::string event_asm = ConvertEventToAsm(event, options, current_duration);
    if (!event_asm.empty()) {
      out << "  " << event_asm << "\n";
    }
  }

  // End marker
  out << "  db End\n";

  return out.str();
}

std::string AsmExporter::ConvertEventToAsm(const TrackEvent& event,
                                           const AsmExportOptions& options,
                                           uint8_t& current_duration) {
  switch (event.type) {
    case TrackEvent::Type::Note:
      return ConvertNoteToAsm(event.note, options, current_duration);
    case TrackEvent::Type::Command:
    case TrackEvent::Type::SubroutineCall:
      return ConvertCommandToAsm(event.command, options);
    case TrackEvent::Type::End:
      return "";  // Will be added by GenerateChannelData
    default:
      return "";
  }
}

std::string AsmExporter::ConvertNoteToAsm(const Note& note,
                                          const AsmExportOptions& options,
                                          uint8_t& current_duration) {
  std::ostringstream out;

  // Check if duration changed
  if (note.has_duration_prefix && note.duration != current_duration) {
    const char* duration_const = GetDurationConstant(note.duration);
    if (duration_const) {
      if (note.velocity != 0) {
        out << absl::StrFormat("%%SetDurationN(%s, $%02X)\n  ", duration_const,
                               note.velocity);
      } else {
        out << absl::StrFormat("%%SetDuration(%s)\n  ", duration_const);
      }
    } else {
      // Raw duration byte
      out << absl::StrFormat("db $%02X\n  ", note.duration);
    }
    current_duration = note.duration;
  }

  // Note name
  std::string note_name = GetNoteName(note.pitch);
  out << "db " << note_name;

  return out.str();
}

std::string AsmExporter::ConvertCommandToAsm(const MusicCommand& cmd,
                                             const AsmExportOptions& options) {
  std::ostringstream out;

  // Special handling for SetInstrument with macro support
  if (options.use_instrument_macros && cmd.opcode == 0xE0) {
    const char* macro = GetInstrumentMacro(cmd.params[0]);
    if (macro) {
      return macro;
    }
  }

  // Get the command macro name
  const char* macro_name = GetCommandMacro(cmd.opcode);
  if (!macro_name) {
    // Unknown command, output raw bytes
    out << absl::StrFormat("db $%02X", cmd.opcode);
    int param_count = cmd.GetParamCount();
    for (int i = 0; i < param_count; ++i) {
      out << absl::StrFormat(", $%02X", cmd.params[i]);
    }
    return out.str();
  }

  // Format the macro call
  int param_count = GetCommandParamCount(cmd.opcode);
  out << "%" << macro_name << "(";

  for (int i = 0; i < param_count; ++i) {
    if (i > 0) out << ", ";
    out << absl::StrFormat("$%02X", cmd.params[i]);
  }

  out << ")";
  return out.str();
}

const char* AsmExporter::GetDurationConstant(uint8_t duration) {
  for (const auto& dc : kDurationConstants) {
    if (dc.value == duration) {
      return dc.name;
    }
  }
  return nullptr;
}

std::string AsmExporter::GetNoteName(uint8_t pitch) {
  // Special values
  if (pitch == kNoteTie) return "Tie";
  if (pitch == kNoteRest) return "Rest";
  if (pitch == kTrackEnd) return "End";

  // Regular notes: C1 = 0x80, B6 = 0xC7
  if (pitch < kNoteMinPitch || pitch > kNoteMaxPitch) {
    return absl::StrFormat("$%02X", pitch);  // Unknown, output raw
  }

  int offset = pitch - kNoteMinPitch;
  int octave = (offset / 12) + 1;
  int semitone = offset % 12;

  static const char* const note_names[] = {"C",  "Cs", "D",  "Ds", "E",  "F",
                                           "Fs", "G",  "Gs", "A",  "As", "B"};

  return absl::StrFormat("%s%d", note_names[semitone], octave);
}

const char* AsmExporter::GetInstrumentMacro(uint8_t instrument_id) {
  for (const auto& im : kInstrumentMacros) {
    if (im.id == instrument_id) {
      return im.macro;
    }
  }
  return nullptr;
}

const char* AsmExporter::GetCommandMacro(uint8_t opcode) {
  switch (opcode) {
    case 0xE0: return "SetInstrument";
    case 0xE1: return "SetPan";
    case 0xE2: return "PanFade";
    case 0xE3: return "VibratoOn";
    case 0xE4: return "VibratoOff";
    case 0xE5: return "SetMasterVolume";
    case 0xE6: return "MasterVolumeFade";
    case 0xE7: return "SetTempo";
    case 0xE8: return "TempoFade";
    case 0xE9: return "GlobalTranspose";
    case 0xEA: return "ChannelTranspose";
    case 0xEB: return "TremoloOn";
    case 0xEC: return "TremoloOff";
    case 0xED: return "SetChannelVolume";
    case 0xEE: return "ChannelVolumeFade";
    case 0xEF: return "CallSubroutine";
    case 0xF0: return "VibratoFade";
    case 0xF1: return "PitchEnvelopeTo";
    case 0xF2: return "PitchEnvelopeFrom";
    case 0xF3: return "PitchEnvelopeOff";
    case 0xF4: return "Tuning";
    case 0xF5: return "EchoVBits";
    case 0xF6: return "EchoOff";
    case 0xF7: return "EchoParams";
    case 0xF8: return "EchoVolumeFade";
    case 0xF9: return "PitchSlide";
    case 0xFA: return "PercussionPatch";
    default: return nullptr;
  }
}

int AsmExporter::GetCommandParamCount(uint8_t opcode) {
  if (opcode < 0xE0) return 0;
  return kCommandParamCount[opcode - 0xE0];
}

}  // namespace music
}  // namespace zelda3
}  // namespace yaze
