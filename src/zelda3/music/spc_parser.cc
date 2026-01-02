#include "zelda3/music/spc_parser.h"

#include <algorithm>
#include <cstring>

#include "absl/strings/str_format.h"

namespace yaze {
namespace zelda3 {
namespace music {

// =============================================================================
// SpcParser Implementation
// =============================================================================

absl::StatusOr<MusicSong> SpcParser::ParseSong(Rom& rom, uint16_t address,
                                                uint8_t bank) {
  if (!rom.is_loaded()) {
    return absl::FailedPreconditionError("ROM is not loaded");
  }

  if (address == 0) {
    return absl::InvalidArgumentError("Invalid song address");
  }

  // Get pointer to song data
  int data_length = 0;
  const uint8_t* song_data = GetSpcData(rom, address, bank, &data_length);
  if (!song_data) {
    return absl::NotFoundError(
        absl::StrFormat("Song data not found at address $%04X bank %d",
                        address, bank));
  }

  MusicSong song;
  song.rom_address = address;
  song.bank = bank;

  // Parse segment table
  // Song format: list of 16-bit segment pointers, terminated by value < 256
  size_t offset = 0;
  std::vector<uint16_t> segment_addresses;

  while (offset + 1 < static_cast<size_t>(data_length)) {
    uint16_t segment_ptr = song_data[offset] | (song_data[offset + 1] << 8);

    // Check for end marker (value < 256 indicates end or loop)
    if (segment_ptr < 256) {
      if (segment_ptr > 0) {
        // This is a loop indicator
        // Next word is the loop target segment pointer
        if (offset + 3 < static_cast<size_t>(data_length)) {
          uint16_t loop_target = song_data[offset + 2] | (song_data[offset + 3] << 8);
          // Find which segment this loops to
          for (size_t i = 0; i < segment_addresses.size(); ++i) {
            if (segment_addresses[i] == loop_target) {
              song.loop_point = static_cast<int>(i);
              break;
            }
          }
        }
      }
      break;
    }

    segment_addresses.push_back(segment_ptr);
    offset += 2;
  }

  // Parse each segment
  for (uint16_t seg_addr : segment_addresses) {
    MusicSegment segment;
    segment.rom_address = seg_addr;

    const uint8_t* seg_data = GetSpcData(rom, seg_addr, bank, nullptr);
    if (!seg_data) {
      // Skip invalid segments
      song.segments.push_back(std::move(segment));
      continue;
    }

    // Each segment has 8 track pointers (16 bytes)
    for (int ch = 0; ch < 8; ++ch) {
      uint16_t track_addr = seg_data[ch * 2] | (seg_data[ch * 2 + 1] << 8);

      if (track_addr == 0) {
        segment.tracks[ch].is_empty = true;
        continue;
      }

      auto track_result = ParseTrack(rom, track_addr, bank);
      if (track_result.ok()) {
        segment.tracks[ch] = std::move(*track_result);
      } else {
        // Mark as empty on error
        segment.tracks[ch].is_empty = true;
      }
    }

    song.segments.push_back(std::move(segment));
  }

  return song;
}

absl::StatusOr<std::vector<uint16_t>> SpcParser::ReadSongPointerTable(
    Rom& rom, uint16_t table_address, uint8_t bank, int max_entries) {
  if (!rom.is_loaded()) {
    return absl::FailedPreconditionError("ROM is not loaded");
  }

  int data_length = 0;
  const uint8_t* table_data =
      GetSpcData(rom, table_address, bank, &data_length);
  if (!table_data) {
    return absl::NotFoundError(absl::StrFormat(
        "Song pointer table not found at $%04X for bank %d", table_address,
        bank));
  }

  if (data_length < 2) {
    return absl::InvalidArgumentError(
        "Song pointer table is too small to contain entries");
  }

  std::vector<uint16_t> pointers;
  pointers.reserve(static_cast<size_t>(max_entries));

  // Each entry is a little-endian SPC address.
  // Note: Null entries ($0000) are valid - they indicate "no song at this slot"
  // for that bank. Different banks share the same song table address space but
  // have different songs at each slot.
  int entries_read = 0;
  for (int offset = 0; offset + 1 < data_length && entries_read < max_entries;
       offset += 2) {
    uint16_t entry = table_data[offset] | (table_data[offset + 1] << 8);
    pointers.push_back(entry);
    ++entries_read;
  }

  return pointers;
}

absl::StatusOr<MusicTrack> SpcParser::ParseTrack(Rom& rom, uint16_t address,
                                                  uint8_t bank, int max_ticks) {
  ParseContext ctx;
  ctx.rom = &rom;
  ctx.current_bank = bank;
  ctx.bank_offset = kSoundBankOffsets[bank % 4];
  ctx.max_parse_depth = 100;
  ctx.current_depth = 0;

  return ParseTrackInternal(ctx, address, max_ticks);
}

absl::StatusOr<MusicTrack> SpcParser::ParseTrackInternal(ParseContext& ctx,
                                                          uint16_t address,
                                                          int max_ticks) {
  if (!ctx.rom || !ctx.rom->is_loaded()) {
    return absl::FailedPreconditionError("ROM is not loaded");
  }

  if (address == 0) {
    MusicTrack empty_track;
    empty_track.is_empty = true;
    return empty_track;
  }

  // Check for infinite loop
  if (std::find(ctx.visited_addresses.begin(), ctx.visited_addresses.end(),
                address) != ctx.visited_addresses.end()) {
    return absl::InvalidArgumentError(
        absl::StrFormat("Circular reference detected at $%04X", address));
  }
  ctx.visited_addresses.push_back(address);

  // Check depth limit
  if (ctx.current_depth >= ctx.max_parse_depth) {
    return absl::ResourceExhaustedError("Maximum parse depth exceeded");
  }
  ++ctx.current_depth;

  int data_length = 0;
  const uint8_t* track_data = GetSpcData(*ctx.rom, address, ctx.current_bank,
                                          &data_length);
  if (!track_data) {
    --ctx.current_depth;
    return absl::NotFoundError(
        absl::StrFormat("Track data not found at $%04X", address));
  }

  MusicTrack track;
  track.rom_address = address;
  track.is_empty = false;

  uint16_t current_tick = 0;
  uint8_t current_duration = 0;
  uint8_t current_velocity = 0;
  size_t pos = 0;

  while (pos < static_cast<size_t>(data_length) && current_tick < max_ticks) {
    uint8_t byte = track_data[pos];

    // End of track
    if (byte == kTrackEnd) {
      track.events.push_back(TrackEvent::MakeEnd(current_tick));
      break;
    }

    // Duration byte (< 0x80)
    if (IsDuration(byte)) {
      current_duration = byte;
      ++pos;

      // Check for velocity byte
      if (pos < static_cast<size_t>(data_length)) {
        uint8_t next = track_data[pos];
        if (IsDuration(next)) {
          current_velocity = next;
          ++pos;
        }
      }
      continue;
    }

    // Note/Rest/Tie (0x80-0xC9)
    if (IsNotePitch(byte)) {
      TrackEvent event;
      event.type = TrackEvent::Type::Note;
      event.tick = current_tick;
      event.rom_offset = address + static_cast<uint16_t>(pos);
      event.note.pitch = byte;
      event.note.duration = current_duration;
      event.note.velocity = current_velocity;
      event.note.has_duration_prefix = true;

      track.events.push_back(event);

      // Only advance time for notes and rests, not ties
      if (byte != kNoteTie) {
        current_tick += current_duration;
      }

      ++pos;
      continue;
    }

    // Command (0xE0-0xFF)
    if (IsCommand(byte)) {
      uint8_t opcode = byte;
      int param_count = GetCommandParamCount(opcode);

      TrackEvent event;
      event.type = TrackEvent::Type::Command;
      event.tick = current_tick;
      event.rom_offset = address + static_cast<uint16_t>(pos);
      event.command.opcode = opcode;

      ++pos;

      // Read parameters
      for (int i = 0; i < param_count && pos < static_cast<size_t>(data_length); ++i) {
        event.command.params[i] = track_data[pos];
        ++pos;
      }

      // Handle subroutine calls specially
      if (opcode == 0xEF) {
        uint16_t sub_addr = event.command.GetSubroutineAddress();
        uint8_t repeat = event.command.GetSubroutineRepeatCount();

        // Parse subroutine to calculate duration
        auto sub_result = ParseSubroutine(ctx, sub_addr, repeat,
                                          max_ticks - current_tick);
        if (sub_result.ok()) {
          current_tick += *sub_result;
        }
      }

      track.events.push_back(event);
      continue;
    }

    // Unknown byte, skip
    ++pos;
  }

  track.CalculateDuration();
  --ctx.current_depth;
  return track;
}

absl::StatusOr<int> SpcParser::ParseSubroutine(ParseContext& ctx,
                                                uint16_t address,
                                                int repeat_count,
                                                int remaining_ticks) {
  if (repeat_count == 0) return 0;

  // Parse the subroutine track once
  auto track_result = ParseTrackInternal(ctx, address,
                                          remaining_ticks / repeat_count);
  if (!track_result.ok()) {
    return track_result.status();
  }

  // Total duration is track duration * repeat count
  return static_cast<int>(track_result->duration_ticks) * repeat_count;
}

uint32_t SpcParser::SpcAddressToRomOffset(Rom& rom, uint16_t spc_address,
                                           uint8_t bank) {
  // The game's sound data is stored in blocks with headers:
  // [size:2][spc_addr:2][data:size]
  // We search through the blocks to find where the SPC address maps to ROM.
  //
  // ROM Layout (from usdasm disassembly):
  // - Banks 0, 1, 3 (Common, Overworld, Credits) are all contiguous starting
  //   at $19:8000 (PC 0xC8000). The game uploads different portions to ARAM.
  // - Bank 2 (Dungeon) is separate at $1B:8000 (PC 0xD8000).
  //
  // Song table blocks that load to ARAM $D000:
  // - Overworld: PC 0xD1EF5 (within the 0xC8000 region)
  // - Credits:   PC 0xD5380 (within the 0xC8000 region)
  // - Dungeon:   PC 0xD8000 (start of dungeon region)

  // Detect SMC header (ROM size % 0x8000 == 0x200 means header present)
  size_t rom_size = rom.size();
  uint32_t header_offset = (rom_size % 0x8000 == 0x200) ? 0x200 : 0;

  uint32_t bank_offset = 0;

  // Get bank base offset for block search
  // Note: Banks 1 and 3 both have blocks that load to $D000, so we need to
  // start searching from the correct block for each bank.
  switch (bank) {
    case 0:
      // Bank 0 (common/SPC program): Search from start of common region
      bank_offset = 0xC8000 + header_offset;
      break;
    case 1:
      // Bank 1 (Overworld): Song table block starts at 0xD1EF5
      // (from usdasm: $1A:9EF5 -> PC 0xD1EF5)
      bank_offset = 0xD1EF5 + header_offset;
      break;
    case 2:
      // Bank 2 (Dungeon): Separate region at 0xD8000
      // (from usdasm: $1B:8000 -> PC 0xD8000)
      bank_offset = 0xD8000 + header_offset;
      break;
    case 3:
      // Bank 3 (Credits): Song table block starts at 0xD5380
      // (from usdasm: $1A:D380 -> PC 0xD5380)
      bank_offset = 0xD5380 + header_offset;
      break;
    default:
      bank_offset = 0xC8000 + header_offset;
      break;
  }

  // Validate bank offset
  if (bank_offset >= rom_size) {
    return 0;
  }

  // Search through the bank's block table
  uint32_t rom_ptr = bank_offset;
  const uint8_t* rom_data = rom.data();

  for (int iterations = 0; iterations < 1000; ++iterations) {  // Safety limit
    if (rom_ptr + 4 >= rom_size) break;

    uint16_t block_size = rom_data[rom_ptr] | (rom_data[rom_ptr + 1] << 8);
    uint16_t block_addr = rom_data[rom_ptr + 2] | (rom_data[rom_ptr + 3] << 8);

    // End of table (size 0 or invalid)
    if (block_size == 0 || block_size > 0x10000) break;

    rom_ptr += 4;

    // Check if address falls within this block
    if (spc_address >= block_addr && spc_address < block_addr + block_size) {
      return rom_ptr + (spc_address - block_addr);
    }

    rom_ptr += block_size;
  }

  // Not found in specified bank, try bank 0 (common data)
  if (bank != 0) {
    return SpcAddressToRomOffset(rom, spc_address, 0);
  }

  return 0;  // Not found
}

const uint8_t* SpcParser::GetSpcData(Rom& rom, uint16_t spc_address,
                                      uint8_t bank, int* out_length) {
  uint32_t rom_offset = SpcAddressToRomOffset(rom, spc_address, bank);
  if (rom_offset == 0 || rom_offset >= rom.size()) {
    if (out_length) *out_length = 0;
    return nullptr;
  }

  // Calculate remaining length (rough estimate)
  if (out_length) {
    // Return a safe default length
    *out_length = static_cast<int>((std::min)(
        static_cast<size_t>(0x1000), rom.size() - rom_offset));
  }

  return rom.data() + rom_offset;
}



// =============================================================================
// BrrCodec Implementation
// =============================================================================

std::vector<int16_t> BrrCodec::Decode(const std::vector<uint8_t>& brr_data,
                                       int* loop_start) {
  std::vector<int16_t> pcm;

  if (brr_data.size() < 9) {
    return pcm;
  }

  int prev1 = 0, prev2 = 0;

  for (size_t block = 0; block < brr_data.size(); block += 9) {
    if (block + 9 > brr_data.size()) break;

    uint8_t header = brr_data[block];
    int range = (header >> 4) & 0x0F;
    int filter = (header >> 2) & 0x03;
    bool end = (header & 0x01) != 0;
    bool loop = (header & 0x02) != 0;

    for (int i = 0; i < 8; ++i) {
      uint8_t byte = brr_data[block + 1 + i];

      for (int nibble = 0; nibble < 2; ++nibble) {
        int sample = (nibble == 0) ? (byte >> 4) : (byte & 0x0F);

        // Sign extend
        if (sample >= 8) sample -= 16;

        // Apply range
        sample <<= range;

        // Apply filter
        switch (filter) {
          case 1:
            sample += (prev1 * kFilter1[1]) >> kFilter2[1];
            break;
          case 2:
            sample += (prev1 * kFilter1[2]) >> kFilter2[2];
            sample -= (prev2 * kFilter3[2]) >> 4;
            break;
          case 3:
            sample += (prev1 * kFilter1[3]) >> kFilter2[3];
            sample -= (prev2 * kFilter3[3]) >> 4;
            break;
        }

        // Clamp
        if (sample > 0x7FFF) sample = 0x7FFF;
        if (sample < -0x8000) sample = -0x8000;

        pcm.push_back(static_cast<int16_t>(sample));

        prev2 = prev1;
        prev1 = sample;
      }
    }

    if (end) {
      if (loop && loop_start) {
        // Loop point would need to be calculated from the source BRR
        *loop_start = 0;  // Placeholder
      }
      break;
    }
  }

  return pcm;
}

std::vector<uint8_t> BrrCodec::Encode(const std::vector<int16_t>& pcm_data,
                                       int loop_start) {
  std::vector<uint8_t> brr;

  // Pad to multiple of 16 samples
  std::vector<int16_t> padded = pcm_data;
  while (padded.size() % 16 != 0) {
    padded.push_back(0);
  }

  int prev1 = 0, prev2 = 0;

  for (size_t i = 0; i < padded.size(); i += 16) {
    // Find best range and filter for this block
    int best_range = 0;
    int best_filter = 0;
    int best_error = INT_MAX;

    for (int range = 0; range < 13; ++range) {
      for (int filter = 0; filter < 4; ++filter) {
        int error = 0;
        int p1 = prev1, p2 = prev2;

        for (int j = 0; j < 16; ++j) {
          int sample = padded[i + j];

          // Apply inverse filter
          int predicted = 0;
          switch (filter) {
            case 1:
              predicted = (p1 * kFilter1[1]) >> kFilter2[1];
              break;
            case 2:
              predicted = (p1 * kFilter1[2]) >> kFilter2[2];
              predicted -= (p2 * kFilter3[2]) >> 4;
              break;
            case 3:
              predicted = (p1 * kFilter1[3]) >> kFilter2[3];
              predicted -= (p2 * kFilter3[3]) >> 4;
              break;
          }

          int diff = sample - predicted;
          int encoded = (diff >> range);

          // Clamp to 4 bits
          if (encoded > 7) encoded = 7;
          if (encoded < -8) encoded = -8;

          // Reconstruct
          int reconstructed = (encoded << range) + predicted;
          error += (sample - reconstructed) * (sample - reconstructed);

          p2 = p1;
          p1 = reconstructed;
        }

        if (error < best_error) {
          best_error = error;
          best_range = range;
          best_filter = filter;
        }
      }
    }

    // Encode block with best parameters
    uint8_t header = (best_range << 4) | (best_filter << 2);
    if (i + 16 >= padded.size()) {
      header |= 0x01;  // End flag
      if (loop_start >= 0) {
        header |= 0x02;  // Loop flag
      }
    }

    brr.push_back(header);

    for (int j = 0; j < 8; ++j) {
      int s1 = padded[i + j * 2];
      int s2 = padded[i + j * 2 + 1];

      // Apply inverse filter and encode
      int predicted1 = 0, predicted2 = 0;
      switch (best_filter) {
        case 1:
          predicted1 = (prev1 * kFilter1[1]) >> kFilter2[1];
          break;
        case 2:
          predicted1 = (prev1 * kFilter1[2]) >> kFilter2[2];
          predicted1 -= (prev2 * kFilter3[2]) >> 4;
          break;
        case 3:
          predicted1 = (prev1 * kFilter1[3]) >> kFilter2[3];
          predicted1 -= (prev2 * kFilter3[3]) >> 4;
          break;
      }

      int enc1 = ((s1 - predicted1) >> best_range);
      if (enc1 > 7) enc1 = 7;
      if (enc1 < -8) enc1 = -8;
      enc1 &= 0x0F;

      int reconstructed1 = (((enc1 >= 8) ? enc1 - 16 : enc1) << best_range) + predicted1;
      prev2 = prev1;
      prev1 = reconstructed1;

      switch (best_filter) {
        case 1:
          predicted2 = (prev1 * kFilter1[1]) >> kFilter2[1];
          break;
        case 2:
          predicted2 = (prev1 * kFilter1[2]) >> kFilter2[2];
          predicted2 -= (prev2 * kFilter3[2]) >> 4;
          break;
        case 3:
          predicted2 = (prev1 * kFilter1[3]) >> kFilter2[3];
          predicted2 -= (prev2 * kFilter3[3]) >> 4;
          break;
      }

      int enc2 = ((s2 - predicted2) >> best_range);
      if (enc2 > 7) enc2 = 7;
      if (enc2 < -8) enc2 = -8;
      enc2 &= 0x0F;

      int reconstructed2 = (((enc2 >= 8) ? enc2 - 16 : enc2) << best_range) + predicted2;
      prev2 = prev1;
      prev1 = reconstructed2;

      brr.push_back((enc1 << 4) | enc2);
    }
  }

  return brr;
}

}  // namespace music
}  // namespace zelda3
}  // namespace yaze
