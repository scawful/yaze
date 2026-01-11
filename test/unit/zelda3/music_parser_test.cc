#include "zelda3/music/spc_parser.h"

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include "zelda3/music/music_bank.h"
#include "zelda3/music/song_data.h"

namespace yaze {
namespace test {

using namespace yaze::zelda3::music;

namespace {

constexpr uint16_t kSerializerBaseAddress = 0xD000;

std::vector<uint8_t> SerializeSingleTrack(const MusicTrack& track) {
  MusicSong song;
  MusicSegment segment;
  segment.tracks[0] = track;
  segment.tracks[0].is_empty = false;
  for (int i = 1; i < 8; ++i) {
    segment.tracks[i].is_empty = true;
    segment.tracks[i].events.clear();
  }
  song.segments.push_back(segment);

  auto result = SpcSerializer::SerializeSong(song, kSerializerBaseAddress);
  EXPECT_TRUE(result.ok()) << result.status();
  if (!result.ok()) {
    return {};
  }

  const auto segment_count = song.segments.size();
  const auto header_size = segment_count * 2 + 2 + (song.HasLoop() ? 2 : 0);
  const auto track_data_base = header_size + segment_count * 16;
  if (result->data.size() < track_data_base) {
    ADD_FAILURE() << "Serialized data shorter than expected header size.";
    return {};
  }

  return std::vector<uint8_t>(result->data.begin() + track_data_base,
                              result->data.end());
}

}  // namespace

// =============================================================================
// Song Data Tests
// =============================================================================

class SongDataTest : public ::testing::Test {
 protected:
  void SetUp() override {}
};

TEST_F(SongDataTest, NoteGetNoteName_ValidPitches) {
  Note note;

  // C1 = 0x80
  note.pitch = 0x80;
  EXPECT_EQ(note.GetNoteName(), "C1");

  // C#1 = 0x81
  note.pitch = 0x81;
  EXPECT_EQ(note.GetNoteName(), "C#1");

  // D1 = 0x82
  note.pitch = 0x82;
  EXPECT_EQ(note.GetNoteName(), "D1");

  // C2 = 0x8C
  note.pitch = 0x8C;
  EXPECT_EQ(note.GetNoteName(), "C2");

  // A4 = 0xAD (concert pitch)
  // Calculation: 0x80 + (octave-1)*12 + semitone
  // A is semitone 9, so A4 = 0x80 + 3*12 + 9 = 0x80 + 36 + 9 = 0xAD
  note.pitch = 0xAD;
  EXPECT_EQ(note.GetNoteName(), "A4");

  // B6 = 0xC7 (highest note)
  note.pitch = 0xC7;
  EXPECT_EQ(note.GetNoteName(), "B6");
}

TEST_F(SongDataTest, NoteGetNoteName_SpecialValues) {
  Note note;

  // Tie
  note.pitch = kNoteTie;
  EXPECT_EQ(note.GetNoteName(), "---");

  // Rest
  note.pitch = kNoteRest;
  EXPECT_EQ(note.GetNoteName(), "...");
}

TEST_F(SongDataTest, NoteHelpers) {
  Note note;

  note.pitch = 0x8C;  // C2
  EXPECT_TRUE(note.IsNote());
  EXPECT_FALSE(note.IsTie());
  EXPECT_FALSE(note.IsRest());
  EXPECT_EQ(note.GetOctave(), 2);
  EXPECT_EQ(note.GetSemitone(), 0);  // C

  note.pitch = 0x8F;  // D#2
  EXPECT_EQ(note.GetOctave(), 2);
  EXPECT_EQ(note.GetSemitone(), 3);  // D#

  note.pitch = kNoteTie;
  EXPECT_FALSE(note.IsNote());
  EXPECT_TRUE(note.IsTie());
  EXPECT_FALSE(note.IsRest());

  note.pitch = kNoteRest;
  EXPECT_FALSE(note.IsNote());
  EXPECT_FALSE(note.IsTie());
  EXPECT_TRUE(note.IsRest());
}

TEST_F(SongDataTest, MusicCommandParamCount) {
  MusicCommand cmd;

  cmd.opcode = 0xE0;  // SetInstrument
  EXPECT_EQ(cmd.GetParamCount(), 1);

  cmd.opcode = 0xE3;  // VibratoOn
  EXPECT_EQ(cmd.GetParamCount(), 3);

  cmd.opcode = 0xE4;  // VibratoOff
  EXPECT_EQ(cmd.GetParamCount(), 0);

  cmd.opcode = 0xEF;  // CallSubroutine
  EXPECT_EQ(cmd.GetParamCount(), 3);
}

TEST_F(SongDataTest, MusicCommandSubroutine) {
  MusicCommand cmd;
  cmd.opcode = 0xEF;
  cmd.params = {0x00, 0xD0, 0x02};  // Address $D000, repeat 2

  EXPECT_TRUE(cmd.IsSubroutine());
  EXPECT_EQ(cmd.GetSubroutineAddress(), 0xD000);
  EXPECT_EQ(cmd.GetSubroutineRepeatCount(), 2);
}

TEST_F(SongDataTest, TrackEventFactory) {
  auto note_event = TrackEvent::MakeNote(100, 0x8C, 72, 0x40);
  EXPECT_EQ(note_event.type, TrackEvent::Type::Note);
  EXPECT_EQ(note_event.tick, 100);
  EXPECT_EQ(note_event.note.pitch, 0x8C);
  EXPECT_EQ(note_event.note.duration, 72);
  EXPECT_EQ(note_event.note.velocity, 0x40);

  auto cmd_event = TrackEvent::MakeCommand(50, 0xE0, 0x0B);
  EXPECT_EQ(cmd_event.type, TrackEvent::Type::Command);
  EXPECT_EQ(cmd_event.tick, 50);
  EXPECT_EQ(cmd_event.command.opcode, 0xE0);
  EXPECT_EQ(cmd_event.command.params[0], 0x0B);

  auto end_event = TrackEvent::MakeEnd(200);
  EXPECT_EQ(end_event.type, TrackEvent::Type::End);
  EXPECT_EQ(end_event.tick, 200);
}

TEST_F(SongDataTest, MusicTrackDuration) {
  MusicTrack track;
  track.events.push_back(TrackEvent::MakeNote(0, 0x8C, 72));
  track.events.push_back(TrackEvent::MakeNote(72, 0x8E, 36));
  track.events.push_back(TrackEvent::MakeEnd(108));

  track.CalculateDuration();

  EXPECT_EQ(track.duration_ticks, 108);
  EXPECT_FALSE(track.is_empty);
}

TEST_F(SongDataTest, MusicSegmentDuration) {
  MusicSegment segment;

  // Track 0: 100 ticks
  segment.tracks[0].events.push_back(TrackEvent::MakeNote(0, 0x8C, 100));
  segment.tracks[0].CalculateDuration();

  // Track 1: 200 ticks
  segment.tracks[1].events.push_back(TrackEvent::MakeNote(0, 0x8C, 200));
  segment.tracks[1].CalculateDuration();

  // Other tracks empty
  for (int i = 2; i < 8; ++i) {
    segment.tracks[i].is_empty = true;
    segment.tracks[i].duration_ticks = 0;
  }

  EXPECT_EQ(segment.GetDuration(), 200);
}

// =============================================================================
// SpcParser Tests
// =============================================================================

class SpcParserTest : public ::testing::Test {
 protected:
  void SetUp() override {}
};

TEST_F(SpcParserTest, GetCommandParamCount) {
  EXPECT_EQ(SpcParser::GetCommandParamCount(0xE0), 1);  // SetInstrument
  EXPECT_EQ(SpcParser::GetCommandParamCount(0xE4), 0);  // VibratoOff
  EXPECT_EQ(SpcParser::GetCommandParamCount(0xE7), 1);  // SetTempo
  EXPECT_EQ(SpcParser::GetCommandParamCount(0xEF), 3);  // CallSubroutine
  EXPECT_EQ(SpcParser::GetCommandParamCount(0x80), 0);  // Not a command
}

TEST_F(SpcParserTest, IsNotePitch) {
  EXPECT_TRUE(SpcParser::IsNotePitch(0x80));   // C1
  EXPECT_TRUE(SpcParser::IsNotePitch(0xC7));   // B6
  EXPECT_TRUE(SpcParser::IsNotePitch(0xC8));   // Tie
  EXPECT_TRUE(SpcParser::IsNotePitch(0xC9));   // Rest
  EXPECT_FALSE(SpcParser::IsNotePitch(0x7F));  // Duration
  EXPECT_FALSE(SpcParser::IsNotePitch(0xE0));  // Command
}

TEST_F(SpcParserTest, IsDuration) {
  EXPECT_TRUE(SpcParser::IsDuration(0x00));
  EXPECT_TRUE(SpcParser::IsDuration(0x48));   // Quarter note
  EXPECT_TRUE(SpcParser::IsDuration(0x7F));   // Max duration
  EXPECT_FALSE(SpcParser::IsDuration(0x80));  // Note
  EXPECT_FALSE(SpcParser::IsDuration(0xE0));  // Command
}

TEST_F(SpcParserTest, IsCommand) {
  EXPECT_TRUE(SpcParser::IsCommand(0xE0));
  EXPECT_TRUE(SpcParser::IsCommand(0xFF));
  EXPECT_FALSE(SpcParser::IsCommand(0xDF));
  EXPECT_FALSE(SpcParser::IsCommand(0x80));
}

// =============================================================================
// SpcSerializer Tests
// =============================================================================

class SpcSerializerTest : public ::testing::Test {
 protected:
  void SetUp() override {}
};

TEST_F(SpcSerializerTest, SerializeNote) {
  MusicTrack track;
  TrackEvent first = TrackEvent::MakeNote(0, 0x8C, 0x48);
  first.note.has_duration_prefix = true;
  track.events.push_back(first);

  TrackEvent second = TrackEvent::MakeNote(72, 0x8E, 0x48);
  second.note.has_duration_prefix = true;
  track.events.push_back(second);
  track.events.push_back(TrackEvent::MakeEnd(144));

  auto bytes = SerializeSingleTrack(track);

  // Should output duration + pitch, then pitch, then end marker.
  ASSERT_EQ(bytes.size(), 4u);
  EXPECT_EQ(bytes[0], 0x48);
  EXPECT_EQ(bytes[1], 0x8C);
  EXPECT_EQ(bytes[2], 0x8E);
  EXPECT_EQ(bytes[3], 0x00);
}

TEST_F(SpcSerializerTest, SerializeCommand) {
  MusicTrack track;
  track.events.push_back(TrackEvent::MakeCommand(0, 0xE0, 0x0B));
  track.events.push_back(TrackEvent::MakeEnd(72));

  auto bytes = SerializeSingleTrack(track);

  ASSERT_EQ(bytes.size(), 3u);
  EXPECT_EQ(bytes[0], 0xE0);
  EXPECT_EQ(bytes[1], 0x0B);
  EXPECT_EQ(bytes[2], 0x00);
}

TEST_F(SpcSerializerTest, SerializeTrack) {
  MusicTrack track;

  // SetInstrument(Piano)
  track.events.push_back(TrackEvent::MakeCommand(0, 0xE0, 0x0B));

  // SetChannelVolume(192)
  track.events.push_back(TrackEvent::MakeCommand(0, 0xED, 0xC0));

  // Quarter note C2 with duration prefix
  TrackEvent note_event = TrackEvent::MakeNote(0, 0x8C, 0x48);
  note_event.note.has_duration_prefix = true;
  track.events.push_back(note_event);

  // End
  track.events.push_back(TrackEvent::MakeEnd(72));

  auto bytes = SerializeSingleTrack(track);

  // Expected: E0 0B ED C0 48 8C 00
  ASSERT_EQ(bytes.size(), 7u);
  EXPECT_EQ(bytes[0], 0xE0);
  EXPECT_EQ(bytes[1], 0x0B);
  EXPECT_EQ(bytes[2], 0xED);
  EXPECT_EQ(bytes[3], 0xC0);
  EXPECT_EQ(bytes[4], 0x48);
  EXPECT_EQ(bytes[5], 0x8C);
  EXPECT_EQ(bytes.back(), 0x00);  // End marker
}

// =============================================================================
// BrrCodec Tests
// =============================================================================

class BrrCodecTest : public ::testing::Test {
 protected:
  void SetUp() override {}
};

TEST_F(BrrCodecTest, EncodeDecodeRoundtrip) {
  // Create a simple sine wave
  std::vector<int16_t> original;
  for (int i = 0; i < 64; ++i) {
    double t = i / 64.0 * 2 * 3.14159;
    original.push_back(static_cast<int16_t>(sin(t) * 10000));
  }

  // Encode to BRR
  auto brr = BrrCodec::Encode(original);
  EXPECT_GT(brr.size(), 0);

  // Decode back
  auto decoded = BrrCodec::Decode(brr);
  EXPECT_GT(decoded.size(), 0);

  // Should be similar (BRR is lossy, so allow some error)
  ASSERT_EQ(decoded.size(), original.size());

  int max_error = 0;
  for (size_t i = 0; i < original.size(); ++i) {
    int error = abs(original[i] - decoded[i]);
    if (error > max_error)
      max_error = error;
  }

  // BRR compression should keep error reasonable
  EXPECT_LT(max_error, 5000);  // Allow up to ~15% error
}

// =============================================================================
// MusicBank Tests
// =============================================================================

class MusicBankTest : public ::testing::Test {
 protected:
  void SetUp() override {}
};

TEST_F(MusicBankTest, GetVanillaSongName) {
  EXPECT_STREQ(GetVanillaSongName(1), "Title");
  EXPECT_STREQ(GetVanillaSongName(2), "Light World");
  EXPECT_STREQ(GetVanillaSongName(12), "Soldier");
  EXPECT_STREQ(GetVanillaSongName(21), "Boss");
  EXPECT_STREQ(GetVanillaSongName(0), "Unknown");
  EXPECT_STREQ(GetVanillaSongName(100), "Unknown");
}

TEST_F(MusicBankTest, GetVanillaSongBank) {
  EXPECT_EQ(GetVanillaSongBank(1), MusicBank::Bank::Overworld);
  EXPECT_EQ(GetVanillaSongBank(11), MusicBank::Bank::Overworld);
  EXPECT_EQ(GetVanillaSongBank(12), MusicBank::Bank::Dungeon);
  EXPECT_EQ(GetVanillaSongBank(31), MusicBank::Bank::Dungeon);
  EXPECT_EQ(GetVanillaSongBank(32), MusicBank::Bank::Credits);
}

TEST_F(MusicBankTest, BankMaxSize) {
  EXPECT_EQ(MusicBank::GetBankMaxSize(MusicBank::Bank::Overworld),
            kOverworldBankMaxSize);
  EXPECT_EQ(MusicBank::GetBankMaxSize(MusicBank::Bank::Dungeon),
            kDungeonBankMaxSize);
  EXPECT_EQ(MusicBank::GetBankMaxSize(MusicBank::Bank::Credits),
            kCreditsBankMaxSize);
}

TEST_F(MusicBankTest, CreateNewSong) {
  MusicBank bank;

  int index = bank.CreateNewSong("Test Song", MusicBank::Bank::Overworld);
  EXPECT_GE(index, 0);

  auto* song = bank.GetSong(index);
  ASSERT_NE(song, nullptr);
  EXPECT_EQ(song->name, "Test Song");
  EXPECT_EQ(song->bank, static_cast<uint8_t>(MusicBank::Bank::Overworld));
  EXPECT_TRUE(song->modified);
  EXPECT_EQ(song->segments.size(), 1);
}

TEST_F(MusicBankTest, SpaceCalculation) {
  MusicBank bank;

  // Empty bank
  auto space = bank.CalculateSpaceUsage(MusicBank::Bank::Overworld);
  EXPECT_EQ(space.used_bytes, 0);
  EXPECT_EQ(space.free_bytes, kOverworldBankMaxSize);
  EXPECT_EQ(space.total_bytes, kOverworldBankMaxSize);
  EXPECT_EQ(space.usage_percent, 0.0f);

  // Add a song
  bank.CreateNewSong("Test", MusicBank::Bank::Overworld);

  space = bank.CalculateSpaceUsage(MusicBank::Bank::Overworld);
  EXPECT_GT(space.used_bytes, 0);
  EXPECT_LT(space.free_bytes, kOverworldBankMaxSize);
}

// =============================================================================
// Direct SPC Bank Mapping Tests
// =============================================================================

class DirectSpcMappingTest : public ::testing::Test {
 protected:
  void SetUp() override {}

  // Helper to test bank ROM offset mapping
  // Note: These match the logic in MusicEditor::GetBankRomOffset
  uint32_t GetBankRomOffset(uint8_t bank) const {
    constexpr uint32_t kSoundBankOffsets[] = {
        0xC8000,  // ROM Bank 0 (common) - driver + samples + instruments
        0xD1EF5,  // ROM Bank 1 (overworld songs)
        0xD8000,  // ROM Bank 2 (dungeon songs)
        0xD5380   // ROM Bank 3 (credits songs)
    };
    if (bank < 4) {
      return kSoundBankOffsets[bank];
    }
    return kSoundBankOffsets[0];
  }

  // Helper to convert song.bank enum to ROM bank
  uint8_t SongBankToRomBank(uint8_t song_bank) const {
    // song.bank: 0=overworld, 1=dungeon, 2=credits
    // ROM bank:  1=overworld, 2=dungeon, 3=credits
    return song_bank + 1;
  }

  // Helper to test song index in bank calculation
  // Matches MusicEditor::GetSongIndexInBank
  int GetSongIndexInBank(int song_id, uint8_t bank) const {
    switch (bank) {
      case 0:                 // Overworld
        return song_id - 1;   // Songs 1-11 → 0-10
      case 1:                 // Dungeon
        return song_id - 12;  // Songs 12-31 → 0-19
      case 2:                 // Credits
        return song_id - 32;  // Songs 32-34 → 0-2
      default:
        return 0;
    }
  }
};

TEST_F(DirectSpcMappingTest, BankRomOffsets) {
  // ROM Bank 0: Common bank (driver, samples, instruments)
  EXPECT_EQ(GetBankRomOffset(0), 0xC8000u);

  // ROM Bank 1: Overworld songs
  EXPECT_EQ(GetBankRomOffset(1), 0xD1EF5u);

  // ROM Bank 2: Dungeon songs
  EXPECT_EQ(GetBankRomOffset(2), 0xD8000u);

  // ROM Bank 3: Credits songs
  EXPECT_EQ(GetBankRomOffset(3), 0xD5380u);

  // Invalid bank should return common
  EXPECT_EQ(GetBankRomOffset(99), 0xC8000u);
}

TEST_F(DirectSpcMappingTest, SongBankToRomBankMapping) {
  // song.bank 0 (overworld) → ROM bank 1 (0xD1EF5)
  EXPECT_EQ(SongBankToRomBank(0), 1);
  EXPECT_EQ(GetBankRomOffset(SongBankToRomBank(0)), 0xD1EF5u);

  // song.bank 1 (dungeon) → ROM bank 2 (0xD8000)
  EXPECT_EQ(SongBankToRomBank(1), 2);
  EXPECT_EQ(GetBankRomOffset(SongBankToRomBank(1)), 0xD8000u);

  // song.bank 2 (credits) → ROM bank 3 (0xD5380)
  EXPECT_EQ(SongBankToRomBank(2), 3);
  EXPECT_EQ(GetBankRomOffset(SongBankToRomBank(2)), 0xD5380u);
}

TEST_F(DirectSpcMappingTest, OverworldSongIndices) {
  // Overworld songs: 1-11 (global ID) → 0-10 (bank index)
  EXPECT_EQ(GetSongIndexInBank(1, 0), 0);    // Title
  EXPECT_EQ(GetSongIndexInBank(2, 0), 1);    // Light World
  EXPECT_EQ(GetSongIndexInBank(11, 0), 10);  // File Select
}

TEST_F(DirectSpcMappingTest, DungeonSongIndices) {
  // Dungeon songs: 12-31 (global ID) → 0-19 (bank index)
  EXPECT_EQ(GetSongIndexInBank(12, 1), 0);   // Soldier
  EXPECT_EQ(GetSongIndexInBank(13, 1), 1);   // Mountain
  EXPECT_EQ(GetSongIndexInBank(21, 1), 9);   // Boss
  EXPECT_EQ(GetSongIndexInBank(31, 1), 19);  // Last Boss
}

TEST_F(DirectSpcMappingTest, CreditsSongIndices) {
  // Credits songs: 32-34 (global ID) → 0-2 (bank index)
  EXPECT_EQ(GetSongIndexInBank(32, 2), 0);  // Credits 1
  EXPECT_EQ(GetSongIndexInBank(33, 2), 1);  // Credits 2
  EXPECT_EQ(GetSongIndexInBank(34, 2), 2);  // Credits 3
}

TEST_F(DirectSpcMappingTest, BankIndexConsistency) {
  // Verify bank index is non-negative for all vanilla songs
  for (int song_id = 1; song_id <= 11; ++song_id) {
    int index = GetSongIndexInBank(song_id, 0);
    EXPECT_GE(index, 0) << "Overworld song " << song_id
                        << " should have non-negative index";
    EXPECT_LE(index, 10) << "Overworld song " << song_id << " should be <= 10";
  }

  for (int song_id = 12; song_id <= 31; ++song_id) {
    int index = GetSongIndexInBank(song_id, 1);
    EXPECT_GE(index, 0) << "Dungeon song " << song_id
                        << " should have non-negative index";
    EXPECT_LE(index, 19) << "Dungeon song " << song_id << " should be <= 19";
  }

  for (int song_id = 32; song_id <= 34; ++song_id) {
    int index = GetSongIndexInBank(song_id, 2);
    EXPECT_GE(index, 0) << "Credits song " << song_id
                        << " should have non-negative index";
    EXPECT_LE(index, 2) << "Credits song " << song_id << " should be <= 2";
  }
}

}  // namespace test
}  // namespace yaze
