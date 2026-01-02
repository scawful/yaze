#include "zelda3/music/spc_serializer.h"

#include "absl/strings/str_format.h"

namespace yaze {
namespace zelda3 {
namespace music {

absl::StatusOr<SpcSerializer::SerializeResult> SpcSerializer::SerializeSong(
    const MusicSong& song, uint16_t base_address) {
  SerializeResult result;
  result.base_address = base_address;

  if (song.segments.empty()) {
    return absl::InvalidArgumentError("Song has no segments");
  }

  const size_t segment_count = song.segments.size();
  const uint16_t header_size =
      static_cast<uint16_t>(segment_count * 2 + 2 + (song.HasLoop() ? 2 : 0));
  const uint16_t segment_table_base = header_size;
  const uint16_t track_data_base =
      static_cast<uint16_t>(segment_table_base + segment_count * 16);

  struct TrackBlob {
    uint16_t offset = 0;
    std::vector<uint8_t> data;
  };

  std::vector<std::array<uint16_t, 8>> segment_track_ptrs(segment_count);
  std::vector<TrackBlob> track_blobs;
  track_blobs.reserve(segment_count * 2);

  uint32_t next_track_offset = track_data_base;

  for (size_t seg_index = 0; seg_index < segment_count; ++seg_index) {
    const auto& segment = song.segments[seg_index];
    auto& ptrs = segment_track_ptrs[seg_index];
    ptrs.fill(0);

    for (int ch = 0; ch < 8; ++ch) {
      const auto& track = segment.tracks[ch];
      if (track.is_empty || track.events.empty()) {
        ptrs[ch] = 0;
        continue;
      }

      auto track_bytes = SerializeTrack(track);
      if (track_bytes.empty()) {
        ptrs[ch] = 0;
        continue;
      }

      if (next_track_offset + track_bytes.size() > 0x10000) {
        return absl::ResourceExhaustedError(
            "Serialized track exceeds SPC address space");
      }

      ptrs[ch] = static_cast<uint16_t>(next_track_offset);

      TrackBlob blob;
      blob.offset = ptrs[ch];
      blob.data = std::move(track_bytes);
      track_blobs.push_back(std::move(blob));

      next_track_offset +=
          static_cast<uint32_t>(track_blobs.back().data.size());
    }
  }

  const uint32_t track_data_size = next_track_offset - track_data_base;
  const uint32_t total_size =
      header_size + segment_count * 16 + track_data_size;
  result.data.reserve(total_size);

  auto write_pointer = [&](uint16_t value, bool record) {
    result.data.push_back(value & 0xFF);
    result.data.push_back((value >> 8) & 0xFF);
    if (record && value != 0) {
      result.relocations.push_back(
          static_cast<uint16_t>(result.data.size() - 2));
    }
  };

  // Write segment pointers
  uint16_t segment_offset = segment_table_base + base_address;
  for (size_t i = 0; i < segment_count; ++i) {
    write_pointer(segment_offset, true);
    segment_offset = static_cast<uint16_t>(segment_offset + 16);
  }

  // End marker
  if (song.HasLoop()) {
    result.data.push_back(0xFF);
    result.data.push_back(0x00);
    uint16_t loop_target = static_cast<uint16_t>(
        segment_table_base + base_address + song.loop_point * 16);
    write_pointer(loop_target, true);
  } else {
    result.data.push_back(0x00);
    result.data.push_back(0x00);
  }

  // Segment track pointer tables
  for (const auto& ptrs : segment_track_ptrs) {
    for (int ch = 0; ch < 8; ++ch) {
      uint16_t pointer_value =
          (ptrs[ch] == 0) ? 0 : static_cast<uint16_t>(ptrs[ch] + base_address);
      write_pointer(pointer_value, ptrs[ch] != 0);
    }
  }

  // Track data
  for (const auto& blob : track_blobs) {
    result.data.insert(result.data.end(), blob.data.begin(), blob.data.end());
  }

  return result;
}

absl::StatusOr<SpcSerializer::SerializeResult>
SpcSerializer::SerializeSongFromSegment(const MusicSong& song,
                                        int start_segment,
                                        uint16_t base_address) {
  // Validate start segment
  if (start_segment < 0 ||
      start_segment >= static_cast<int>(song.segments.size())) {
    return absl::InvalidArgumentError(
        absl::StrFormat("Invalid start segment: %d (song has %zu segments)",
                        start_segment, song.segments.size()));
  }

  // If starting from segment 0, use the normal serialization
  if (start_segment == 0) {
    return SerializeSong(song, base_address);
  }

  // Create a temporary song with segments from start_segment onwards
  MusicSong partial_song;
  partial_song.name = song.name;
  partial_song.bank = song.bank;

  // Copy segments from start_segment to end
  for (size_t i = start_segment; i < song.segments.size(); ++i) {
    partial_song.segments.push_back(song.segments[i]);
  }

  // Adjust loop point if song has one
  if (song.HasLoop() && song.loop_point >= start_segment) {
    partial_song.loop_point = song.loop_point - start_segment;
  } else {
    partial_song.loop_point = -1;  // No loop or loop before start
  }

  // Serialize the partial song
  return SerializeSong(partial_song, base_address);
}

std::vector<uint8_t> SpcSerializer::SerializeTrack(const MusicTrack& track) {
  std::vector<uint8_t> data;
  uint8_t current_duration = 0;

  for (const auto& event : track.events) {
    switch (event.type) {
      case TrackEvent::Type::Note: {
        auto note_bytes = SerializeNote(event.note, &current_duration);
        data.insert(data.end(), note_bytes.begin(), note_bytes.end());
        break;
      }

      case TrackEvent::Type::Command: {
        auto cmd_bytes = SerializeCommand(event.command);
        data.insert(data.end(), cmd_bytes.begin(), cmd_bytes.end());
        break;
      }

      case TrackEvent::Type::End:
        data.push_back(kTrackEnd);
        break;

      default:
        break;
    }
  }

  // Ensure track ends with end marker
  if (data.empty() || data.back() != kTrackEnd) {
    data.push_back(kTrackEnd);
  }

  return data;
}

int SpcSerializer::CalculateRequiredSpace(const MusicSong& song) {
  int total = 0;

  // Song header
  total += static_cast<int>(song.segments.size()) * 2 + 2;
  if (song.HasLoop())
    total += 2;

  // Segments and tracks
  for (const auto& segment : song.segments) {
    total += 16;  // Track pointers

    for (const auto& track : segment.tracks) {
      if (!track.is_empty) {
        total += static_cast<int>(SerializeTrack(track).size());
      }
    }
  }

  return total;
}

std::vector<uint8_t> SpcSerializer::SerializeNote(const Note& note,
                                                  uint8_t* current_duration) {
  std::vector<uint8_t> data;

  // Output duration if different from current
  if (note.has_duration_prefix && note.duration != *current_duration) {
    data.push_back(note.duration);
    *current_duration = note.duration;

    // Output velocity if non-zero
    if (note.velocity != 0) {
      data.push_back(note.velocity);
    }
  }

  // Output pitch
  data.push_back(note.pitch);

  return data;
}

std::vector<uint8_t> SpcSerializer::SerializeCommand(
    const MusicCommand& command) {
  std::vector<uint8_t> data;

  data.push_back(command.opcode);

  int param_count = command.GetParamCount();
  for (int i = 0; i < param_count; ++i) {
    data.push_back(command.params[i]);
  }

  return data;
}

void SpcSerializer::ApplyBaseAddress(SerializeResult* result,
                                     uint16_t new_base_address) {
  if (!result)
    return;
  const int32_t delta = static_cast<int32_t>(new_base_address) -
                        static_cast<int32_t>(result->base_address);
  if (delta == 0) {
    return;
  }

  for (uint16_t offset : result->relocations) {
    if (offset + 1 >= result->data.size())
      continue;
    uint16_t value = static_cast<uint16_t>(result->data[offset] |
                                           (result->data[offset + 1] << 8));
    value = static_cast<uint16_t>(value + delta);
    result->data[offset] = value & 0xFF;
    result->data[offset + 1] = (value >> 8) & 0xFF;
  }

  result->base_address = new_base_address;
}

}  // namespace music
}  // namespace zelda3
}  // namespace yaze
