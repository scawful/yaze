#include "zelda3/music/music_bank.h"

#include "absl/status/status.h"
#include "absl/status/statusor.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <exception>
#include <string>
#include <utility>
#include <vector>

#include "absl/strings/str_format.h"
#include "rom/rom.h"
#include "nlohmann/json.hpp"
#include "util/macro.h"
#include "zelda3/music/song_data.h"
#include "zelda3/music/spc_parser.h"

namespace yaze {
namespace zelda3 {
namespace music {

namespace {

// Vanilla song table (1-indexed song IDs)
struct VanillaSongInfo {
  const char* name;
  MusicBank::Bank bank;
};

constexpr VanillaSongInfo kVanillaSongs[] = {
    {"Invalid", MusicBank::Bank::Overworld},        // 0 (unused)
    {"Title", MusicBank::Bank::Overworld},          // 1
    {"Light World", MusicBank::Bank::Overworld},    // 2
    {"Beginning", MusicBank::Bank::Overworld},      // 3
    {"Rabbit", MusicBank::Bank::Overworld},         // 4
    {"Forest", MusicBank::Bank::Overworld},         // 5
    {"Intro", MusicBank::Bank::Overworld},          // 6
    {"Town", MusicBank::Bank::Overworld},           // 7
    {"Warp", MusicBank::Bank::Overworld},           // 8
    {"Dark World", MusicBank::Bank::Overworld},     // 9
    {"Master Sword", MusicBank::Bank::Overworld},   // 10
    {"File Select", MusicBank::Bank::Overworld},    // 11
    {"Soldier", MusicBank::Bank::Dungeon},          // 12
    {"Mountain", MusicBank::Bank::Dungeon},         // 13
    {"Shop", MusicBank::Bank::Dungeon},             // 14
    {"Fanfare", MusicBank::Bank::Dungeon},          // 15
    {"Castle", MusicBank::Bank::Dungeon},           // 16
    {"Palace (Pendant)", MusicBank::Bank::Dungeon}, // 17
    {"Cave", MusicBank::Bank::Dungeon},             // 18
    {"Clear", MusicBank::Bank::Dungeon},            // 19
    {"Church", MusicBank::Bank::Dungeon},           // 20
    {"Boss", MusicBank::Bank::Dungeon},             // 21
    {"Dungeon (Crystal)", MusicBank::Bank::Dungeon},// 22
    {"Psychic", MusicBank::Bank::Dungeon},          // 23
    {"Secret Way", MusicBank::Bank::Dungeon},       // 24
    {"Rescue", MusicBank::Bank::Dungeon},           // 25
    {"Crystal", MusicBank::Bank::Dungeon},          // 26
    {"Fountain", MusicBank::Bank::Dungeon},         // 27
    {"Pyramid", MusicBank::Bank::Dungeon},          // 28
    {"Kill Agahnim", MusicBank::Bank::Dungeon},     // 29
    {"Ganon Room", MusicBank::Bank::Dungeon},       // 30
    {"Last Boss", MusicBank::Bank::Dungeon},        // 31
    {"Credits 1", MusicBank::Bank::Credits},        // 32
    {"Credits 2", MusicBank::Bank::Credits},        // 33
    {"Credits 3", MusicBank::Bank::Credits},        // 34
};

constexpr int kVanillaSongCount =
    sizeof(kVanillaSongs) / sizeof(kVanillaSongs[0]) - 1;

struct BankMetadata {
  MusicBank::Bank bank;
  uint8_t spc_bank;
  int vanilla_start_id;
  int vanilla_end_id;
};

constexpr BankMetadata kBankMetadata[] = {
    {MusicBank::Bank::Overworld, 1, 1, 11},
    {MusicBank::Bank::Dungeon, 2, 12, 31},
    {MusicBank::Bank::Credits, 3, 32, 34},
};

const BankMetadata* GetMetadataForBank(MusicBank::Bank bank) {
  for (const auto& metadata : kBankMetadata) {
    if (metadata.bank == bank) {
      return &metadata;
    }
  }
  return nullptr;
}

struct BankPointerRegisters {
  int low;
  int mid;
  int bank;
};

constexpr BankPointerRegisters kOverworldPointerRegs{0x0914, 0x0918, 0x091C};
constexpr BankPointerRegisters kCreditsPointerRegs{0x0932, 0x0936, 0x093A};

uint8_t EncodeLoRomBank(uint32_t pc_offset) {
  return static_cast<uint8_t>((pc_offset >> 15) & 0xFF);
}

uint8_t EncodeLoRomMid(uint32_t pc_offset) {
  uint16_t addr = static_cast<uint16_t>(pc_offset & 0x7FFF);
  return static_cast<uint8_t>((addr >> 8) & 0x7F);
}

uint8_t EncodeLoRomLow(uint32_t pc_offset) {
  return static_cast<uint8_t>(pc_offset & 0xFF);
}

absl::Status UpdateBankPointerRegisters(Rom& rom,
                                        const BankPointerRegisters& regs,
                                        uint32_t pc_offset) {
  uint8_t preserved_mid = 0;
  auto mid_read = rom.ReadByte(regs.mid);
  if (mid_read.ok()) {
    preserved_mid = mid_read.value() & 0x80;
  }

  auto status = rom.WriteByte(regs.low, EncodeLoRomLow(pc_offset));
  if (!status.ok()) return status;

  status =
      rom.WriteByte(regs.mid, static_cast<uint8_t>(preserved_mid |
                                                   EncodeLoRomMid(pc_offset)));
  if (!status.ok()) return status;

  status = rom.WriteByte(regs.bank, EncodeLoRomBank(pc_offset));
  return status;
}

absl::Status UpdateDynamicBankPointer(Rom& rom, MusicBank::Bank bank,
                                      uint32_t pc_offset) {
  switch (bank) {
    case MusicBank::Bank::Overworld:
      return UpdateBankPointerRegisters(rom, kOverworldPointerRegs, pc_offset);
    case MusicBank::Bank::Credits:
      return UpdateBankPointerRegisters(rom, kCreditsPointerRegs, pc_offset);
    default:
      return absl::OkStatus();
  }
}

// ALTTP instrument table metadata
constexpr int kVanillaInstrumentCount = 0x19;  // 25 entries
constexpr int kInstrumentEntrySize = 6;
constexpr const char* kAltTpInstrumentNames[kVanillaInstrumentCount] = {
    "Noise",        "Rain",      "Timpani",     "Square wave", "Saw wave",
    "Clink",        "Wobbly lead", "Compound saw", "Tweet",       "Strings A",
    "Strings B",    "Trombone",  "Cymbal",      "Ocarina",     "Chimes",
    "Harp",         "Splash",    "Trumpet",     "Horn",        "Snare A",
    "Snare B",      "Choir",     "Flute",       "Oof",         "Piano"};

}  // namespace

// =============================================================================
// Public Methods
// =============================================================================

absl::Status MusicBank::LoadFromRom(Rom& rom) {
  if (!rom.is_loaded()) {
    return absl::FailedPreconditionError("ROM is not loaded");
  }

  songs_.clear();
  instruments_.clear();
  samples_.clear();
  expanded_bank_info_ = ExpandedBankInfo{};
  expanded_song_count_ = 0;
  auxiliary_song_count_ = 0;

  std::vector<MusicSong> custom_songs;
  custom_songs.reserve(8);

  // Detect Oracle of Secrets expanded music patch
  auto status = DetectExpandedMusicPatch(rom);
  if (!status.ok()) return status;

  // Load songs from each vanilla bank
  status = LoadSongTable(rom, Bank::Overworld, &custom_songs);
  if (!status.ok()) return status;

  status = LoadSongTable(rom, Bank::Dungeon, &custom_songs);
  if (!status.ok()) return status;

  status = LoadSongTable(rom, Bank::Credits, &custom_songs);
  if (!status.ok()) return status;

  // Load expanded bank songs if patch detected
  if (expanded_bank_info_.detected) {
    status = LoadExpandedSongTable(rom, &custom_songs);
    if (!status.ok()) return status;
  }

  for (auto& song : custom_songs) {
    songs_.push_back(std::move(song));
  }

  // Load instruments
  status = LoadInstruments(rom);
  if (!status.ok()) return status;

  // Load samples
  status = LoadSamples(rom);
  if (!status.ok()) return status;

  loaded_ = true;
  return absl::OkStatus();
}

absl::Status MusicBank::SaveToRom(Rom& rom) {
  if (!loaded_) {
    return absl::FailedPreconditionError("No music data loaded");
  }

  if (!rom.is_loaded()) {
    return absl::FailedPreconditionError("ROM is not loaded");
  }

  // Check if everything fits
  if (!AllSongsFit()) {
    return absl::ResourceExhaustedError(
        "Songs do not fit in ROM banks. Reduce song size or remove songs.");
  }

  // Save songs to each bank
  auto status = SaveSongTable(rom, Bank::Overworld);
  if (!status.ok()) return status;

  status = SaveSongTable(rom, Bank::Dungeon);
  if (!status.ok()) return status;

  status = SaveSongTable(rom, Bank::Credits);
  if (!status.ok()) return status;

  // Save instruments if modified
  if (instruments_modified_) {
    status = SaveInstruments(rom);
    if (!status.ok()) return status;
  }

  // Save samples if modified
  if (samples_modified_) {
    status = SaveSamples(rom);
    if (!status.ok()) return status;
  }

  ClearModifications();
  return absl::OkStatus();
}

MusicSong* MusicBank::GetSong(int index) {
  if (index < 0 || index >= static_cast<int>(songs_.size())) {
    return nullptr;
  }
  return &songs_[index];
}

const MusicSong* MusicBank::GetSong(int index) const {
  if (index < 0 || index >= static_cast<int>(songs_.size())) {
    return nullptr;
  }
  return &songs_[index];
}

MusicSong* MusicBank::GetSongById(int song_id) {
  // Song IDs are 1-indexed
  if (song_id <= 0 || song_id > static_cast<int>(songs_.size())) {
    return nullptr;
  }
  return &songs_[song_id - 1];
}

int MusicBank::CreateNewSong(const std::string& name, Bank bank) {
  MusicSong song;
  song.name = name;
  song.bank = static_cast<uint8_t>(bank);
  song.modified = true;

  // Add a default empty segment with empty tracks
  MusicSegment segment;
  for (auto& track : segment.tracks) {
    track.is_empty = true;
    track.events.push_back(TrackEvent::MakeEnd(0));
  }
  song.segments.push_back(std::move(segment));

  songs_.push_back(std::move(song));
  return static_cast<int>(songs_.size()) - 1;
}

int MusicBank::DuplicateSong(int index) {
  auto* source = GetSong(index);
  if (!source) return -1;
  
  MusicSong new_song = *source;
  new_song.name += " (Copy)";
  new_song.modified = true;
  
  songs_.push_back(std::move(new_song));
  return static_cast<int>(songs_.size()) - 1;
}

bool MusicBank::IsVanilla(int index) const {
  return index >= 0 && index < kVanillaSongCount;
}

absl::Status MusicBank::DeleteSong(int index) {
  if (index < 0 || index >= static_cast<int>(songs_.size())) {
    return absl::InvalidArgumentError("Invalid song index");
  }
  
  if (IsVanilla(index)) {
    return absl::InvalidArgumentError("Cannot delete vanilla songs");
  }

  songs_.erase(songs_.begin() + index);
  return absl::OkStatus();
}

std::vector<MusicSong*> MusicBank::GetSongsInBank(Bank bank) {
  std::vector<MusicSong*> result;
  for (auto& song : songs_) {
    if (static_cast<Bank>(song.bank) == bank) {
      result.push_back(&song);
    }
  }
  return result;
}

MusicInstrument* MusicBank::GetInstrument(int index) {
  if (index < 0 || index >= static_cast<int>(instruments_.size())) {
    return nullptr;
  }
  return &instruments_[index];
}

const MusicInstrument* MusicBank::GetInstrument(int index) const {
  if (index < 0 || index >= static_cast<int>(instruments_.size())) {
    return nullptr;
  }
  return &instruments_[index];
}

int MusicBank::CreateNewInstrument(const std::string& name) {
  MusicInstrument inst;
  inst.name = name;
  inst.sample_index = 0;
  inst.attack = 15;  // Default fast attack
  inst.decay = 7;
  inst.sustain_level = 7;
  inst.sustain_rate = 0;
  inst.pitch_mult = 0x1000;  // 1.0 multiplier

  instruments_.push_back(std::move(inst));
  instruments_modified_ = true;
  return static_cast<int>(instruments_.size()) - 1;
}

MusicSample* MusicBank::GetSample(int index) {
  if (index < 0 || index >= static_cast<int>(samples_.size())) {
    return nullptr;
  }
  return &samples_[index];
}

const MusicSample* MusicBank::GetSample(int index) const {
  if (index < 0 || index >= static_cast<int>(samples_.size())) {
    return nullptr;
  }
  return &samples_[index];
}

absl::StatusOr<int> MusicBank::ImportSampleFromWav(const std::string& filepath,
                                                    const std::string& name) {
  // TODO: Implement proper WAV loading and BRR encoding
  // For now, return success with a dummy sample so UI integration can be tested
  
  MusicSample sample;
  sample.name = name;
  // Create dummy PCM data (sine wave)
  sample.pcm_data.resize(1000);
  for (int i = 0; i < 1000; ++i) {
    sample.pcm_data[i] = static_cast<int16_t>(32040.0 * std::sin(i * 0.1));
  }
  
  samples_.push_back(std::move(sample));
  samples_modified_ = true;
  
  return static_cast<int>(samples_.size()) - 1;
}

MusicBank::SpaceInfo MusicBank::CalculateSpaceUsage(Bank bank) const {
  SpaceInfo info;
  info.total_bytes = GetBankMaxSize(bank);
  info.used_bytes = 0;

  for (const auto& song : songs_) {
    if (static_cast<Bank>(song.bank) == bank) {
      info.used_bytes += CalculateSongSize(song);
    }
  }

  info.free_bytes = info.total_bytes - info.used_bytes;
  info.usage_percent = (info.total_bytes > 0)
      ? (100.0f * info.used_bytes / info.total_bytes)
      : 0.0f;

  // Set warning/critical flags
  info.is_warning = info.usage_percent > 75.0f;
  info.is_critical = info.usage_percent > 90.0f;

  // Generate recommendations
  if (info.is_critical) {
    if (bank == Bank::Overworld && expanded_bank_info_.detected) {
      info.recommendation = "Move songs to Expanded bank";
    } else if (bank == Bank::OverworldExpanded) {
      info.recommendation = "Move songs to Auxiliary bank";
    } else {
      info.recommendation = "Remove or shorten songs";
    }
  } else if (info.is_warning) {
    info.recommendation = "Approaching bank limit";
  }

  return info;
}

bool MusicBank::AllSongsFit() const {
  return CalculateSpaceUsage(Bank::Overworld).free_bytes >= 0 &&
         CalculateSpaceUsage(Bank::Dungeon).free_bytes >= 0 &&
         CalculateSpaceUsage(Bank::Credits).free_bytes >= 0;
}

int MusicBank::GetBankMaxSize(Bank bank) {
  switch (bank) {
    case Bank::Overworld: return kOverworldBankMaxSize;
    case Bank::Dungeon: return kDungeonBankMaxSize;
    case Bank::Credits: return kCreditsBankMaxSize;
    case Bank::OverworldExpanded: return kExpandedOverworldBankMaxSize;
    case Bank::Auxiliary: return kAuxBankMaxSize;
  }
  return 0;
}

uint32_t MusicBank::GetBankRomAddress(Bank bank) {
  switch (bank) {
    case Bank::Overworld: return kOverworldBankRom;
    case Bank::Dungeon: return kDungeonBankRom;
    case Bank::Credits: return kCreditsBankRom;
    case Bank::OverworldExpanded: return kExpandedOverworldBankRom;
    case Bank::Auxiliary: return kExpandedAuxBankRom;
  }
  return 0;
}

bool MusicBank::HasModifications() const {
  if (instruments_modified_ || samples_modified_) {
    return true;
  }
  for (const auto& song : songs_) {
    if (song.modified) {
      return true;
    }
  }
  return false;
}

void MusicBank::ClearModifications() {
  instruments_modified_ = false;
  samples_modified_ = false;
  for (auto& song : songs_) {
    song.modified = false;
  }
}

// =============================================================================
// Private Methods
// =============================================================================

absl::Status MusicBank::DetectExpandedMusicPatch(Rom& rom) {
  // Reset expanded bank info
  expanded_bank_info_ = ExpandedBankInfo{};

  // Check if ROM has the Oracle of Secrets expanded music hook at $008919
  // The vanilla code at this address is NOT a JSL, but the expanded patch
  // replaces it with: JSL LoadOverworldSongsExpanded
  if (kExpandedMusicHookAddress >= rom.size()) {
    return absl::OkStatus();  // ROM too small, no expanded patch
  }

  auto opcode_result = rom.ReadByte(kExpandedMusicHookAddress);
  if (!opcode_result.ok()) {
    return absl::OkStatus();  // Can't read, assume no patch
  }

  if (opcode_result.value() != kJslOpcode) {
    return absl::OkStatus();  // Not a JSL, no expanded patch
  }

  // Read the JSL target address (3 bytes: low, mid, bank)
  auto addr_low = rom.ReadByte(kExpandedMusicHookAddress + 1);
  auto addr_mid = rom.ReadByte(kExpandedMusicHookAddress + 2);
  auto addr_bank = rom.ReadByte(kExpandedMusicHookAddress + 3);

  if (!addr_low.ok() || !addr_mid.ok() || !addr_bank.ok()) {
    return absl::OkStatus();  // Can't read address, assume no patch
  }

  // Construct the 24-bit SNES address
  uint32_t jsl_target = static_cast<uint32_t>(addr_low.value()) |
                        (static_cast<uint32_t>(addr_mid.value()) << 8) |
                        (static_cast<uint32_t>(addr_bank.value()) << 16);

  // Validate the JSL target is in a reasonable range (freespace or bank $1A-$1B)
  // Oracle of Secrets typically places the hook handler in bank $00 or $1A
  uint8_t target_bank = (jsl_target >> 16) & 0xFF;
  if (target_bank > 0x3F && target_bank < 0x80) {
    return absl::OkStatus();  // Invalid bank range
  }

  // Expanded patch detected!
  expanded_bank_info_.detected = true;
  expanded_bank_info_.hook_address = jsl_target;

  // Use known Oracle of Secrets bank locations
  // These are the standard locations used by the Oracle of Secrets expanded music patch
  expanded_bank_info_.main_rom_offset = kExpandedOverworldBankRom;
  expanded_bank_info_.aux_rom_offset = kExpandedAuxBankRom;
  expanded_bank_info_.aux_aram_address = kAuxSongTableAram;

  return absl::OkStatus();
}

absl::Status MusicBank::LoadExpandedSongTable(
    Rom& rom, std::vector<MusicSong>* custom_songs) {
  if (!expanded_bank_info_.detected) {
    return absl::OkStatus();  // No expanded patch, nothing to load
  }

  // Load songs from the expanded overworld bank
  // This bank contains the Dark World songs in Oracle of Secrets
  const uint32_t expanded_rom_offset = expanded_bank_info_.main_rom_offset;

  // Read the block header: size (2 bytes) + ARAM dest (2 bytes)
  if (expanded_rom_offset + 4 >= rom.size()) {
    return absl::OkStatus();  // Can't read header
  }

  auto header_result = rom.ReadByteVector(expanded_rom_offset, 4);
  if (!header_result.ok()) {
    return absl::OkStatus();  // Can't read header
  }

  const auto& header = header_result.value();
  uint16_t block_size = static_cast<uint16_t>(header[0]) |
                        (static_cast<uint16_t>(header[1]) << 8);
  uint16_t aram_dest = static_cast<uint16_t>(header[2]) |
                       (static_cast<uint16_t>(header[3]) << 8);

  // Verify this looks like a valid song bank block (dest should be $D000)
  if (aram_dest != kSongTableAram || block_size == 0 ||
      block_size > kExpandedOverworldBankMaxSize) {
    return absl::OkStatus();  // Invalid header, skip expanded loading
  }

  // Use SPC bank ID 4 for expanded (same format as overworld bank 1)
  const uint8_t expanded_spc_bank = 4;

  // Read song pointers from the expanded bank
  // Each entry is 2 bytes, count entries until we hit song data or null
  const int max_songs = 16;  // Oracle of Secrets uses ~15 songs in expanded bank
  auto pointer_result = SpcParser::ReadSongPointerTable(
      rom, kSongTableAram, expanded_spc_bank, max_songs);

  if (!pointer_result.ok()) {
    // Failed to read pointers, but don't fail completely
    return absl::OkStatus();
  }

  std::vector<uint16_t> song_addresses = std::move(pointer_result.value());

  // Parse each song in the expanded bank
  int expanded_index = 0;
  for (const uint16_t spc_address : song_addresses) {
    if (spc_address == 0) continue;  // Skip null entries

    MusicSong song;
    auto parsed_song =
        SpcParser::ParseSong(rom, spc_address, expanded_spc_bank);
    if (parsed_song.ok()) {
      song = std::move(parsed_song.value());
    } else {
      // Create empty placeholder on parse failure
      MusicSegment segment;
      for (auto& track : segment.tracks) {
        track.is_empty = true;
        track.events.push_back(TrackEvent::MakeEnd(0));
      }
      song.segments.push_back(std::move(segment));
    }

    song.name = absl::StrFormat("Expanded Song %d", ++expanded_index);
    song.bank = static_cast<uint8_t>(Bank::OverworldExpanded);
    song.modified = false;

    if (custom_songs) {
      custom_songs->push_back(std::move(song));
    } else {
      songs_.push_back(std::move(song));
    }
  }

  expanded_song_count_ = expanded_index;

  // TODO: Load auxiliary bank songs from $2B00 if needed
  // For now, we only load the main expanded bank

  return absl::OkStatus();
}

bool MusicBank::IsExpandedSong(int index) const {
  if (index < 0 || index >= static_cast<int>(songs_.size())) {
    return false;
  }
  const auto& song = songs_[index];
  return song.bank == static_cast<uint8_t>(Bank::OverworldExpanded) ||
         song.bank == static_cast<uint8_t>(Bank::Auxiliary);
}

absl::Status MusicBank::LoadSongTable(Rom& rom, Bank bank,
                                      std::vector<MusicSong>* custom_songs) {
  const BankSongRange range = GetBankSongRange(bank);
  const size_t vanilla_slots = static_cast<size_t>(range.Count());

  // Read only the expected number of song pointers for this bank.
  // Each bank has its own independent song table - don't read too many entries
  // as that would interpret song data as pointers.
  const uint8_t spc_bank = GetSpcBankId(bank);
  auto pointer_result = SpcParser::ReadSongPointerTable(
      rom, GetSongTableAddress(), spc_bank, static_cast<int>(vanilla_slots));
  if (!pointer_result.ok()) {
    return absl::Status(
        pointer_result.status().code(),
        absl::StrFormat("Failed to read song table for bank %d: %s",
                        static_cast<int>(bank),
                        pointer_result.status().message()));
  }

  std::vector<uint16_t> song_addresses = std::move(pointer_result.value());
  if (song_addresses.empty()) {
    return absl::InvalidArgumentError(
        absl::StrFormat("Song table for bank %d is empty",
                        static_cast<int>(bank)));
  }

  auto make_empty_song = []() -> MusicSong {
    MusicSong song;
    MusicSegment segment;
    for (auto& track : segment.tracks) {
      track.is_empty = true;
      track.events.clear();
      track.events.push_back(TrackEvent::MakeEnd(0));
    }
    song.segments.push_back(std::move(segment));
    return song;
  };

  auto emit_song = [&](MusicSong&& song, bool is_custom) {
    song.modified = false;
    if (is_custom && custom_songs) {
      custom_songs->push_back(std::move(song));
    } else {
      songs_.push_back(std::move(song));
    }
  };

  auto parse_and_emit = [&](uint16_t spc_address,
                            const std::string& display_name,
                            bool is_custom) -> absl::Status {
    MusicSong song;
    if (spc_address == 0) {
      song = make_empty_song();
      song.rom_address = 0;
    } else {
      auto parsed_song = SpcParser::ParseSong(rom, spc_address, spc_bank);
      if (!parsed_song.ok()) {
        return absl::Status(
            parsed_song.status().code(),
            absl::StrFormat(
                "Failed to parse song '%s' at $%04X (SPC bank %d): %s",
                display_name, spc_address, spc_bank,
                parsed_song.status().message()));
      }
      song = std::move(parsed_song.value());
    }

    song.name = display_name;
    song.bank = static_cast<uint8_t>(bank);
    emit_song(std::move(song), is_custom);
    return absl::OkStatus();
  };

  // Vanilla slots for this bank
  // IMPORTANT: Each bank has its OWN independent song table at ARAM $D000.
  // When the game loads a bank, it uploads that bank's song table to $D000.
  // - Overworld bank's $D000: indices 0-10 = songs 1-11
  // - Dungeon bank's $D000: indices 0-19 = songs 12-31
  // - Credits bank's $D000: indices 0-2 = songs 32-34
  // So we always read from index 0 in each bank's table.
  for (size_t i = 0; i < vanilla_slots; ++i) {
    const uint16_t spc_address =
        (i < song_addresses.size()) ? song_addresses[i] : 0;
    const int song_id = range.start_id + static_cast<int>(i);
    const std::string display_name = (song_id > 0 && song_id <= kVanillaSongCount)
                                         ? GetVanillaSongName(song_id)
                                         : absl::StrFormat("Vanilla Song %d",
                                                           song_id);
    auto status =
        parse_and_emit(spc_address, display_name, /*is_custom=*/false);
    if (!status.ok()) return status;
  }

  // Custom slots (beyond vanilla range for this bank)
  // These would be at indices after the vanilla songs in this bank's table
  int custom_counter = 1;
  for (size_t table_index = vanilla_slots;
       table_index < song_addresses.size(); ++table_index) {
    const uint16_t spc_address = song_addresses[table_index];
    // Skip null entries (no custom song at this slot)
    if (spc_address == 0) continue;

    const std::string display_name =
        absl::StrFormat("Custom Song %d", custom_counter++);

    auto status =
        parse_and_emit(spc_address, display_name, /*is_custom=*/true);
    if (!status.ok()) return status;
  }

  const int total_songs = static_cast<int>(song_addresses.size());
  switch (bank) {
    case Bank::Overworld:
      overworld_song_count_ = total_songs;
      break;
    case Bank::Dungeon:
      dungeon_song_count_ = total_songs;
      break;
    case Bank::Credits:
      credits_song_count_ = total_songs;
      break;
  }

  return absl::OkStatus();
}

absl::Status MusicBank::SaveSongTable(Rom& rom, Bank bank) {
  auto songs_in_bank = GetSongsInBank(bank);
  if (songs_in_bank.empty()) {
    return absl::OkStatus();
  }

  const uint16_t pointer_entry_count =
      static_cast<uint16_t>(songs_in_bank.size());
  const uint16_t pointer_table_size =
      static_cast<uint16_t>((pointer_entry_count + 1) * 2);
  const uint16_t bank_base = GetSongTableAddress();

  uint32_t current_spc_address =
      static_cast<uint32_t>(bank_base) + pointer_table_size;
  const uint32_t bank_limit =
      static_cast<uint32_t>(bank_base) + GetBankMaxSize(bank);

  std::vector<uint8_t> payload;
  payload.resize(pointer_table_size, 0);

  auto write_pointer_entry = [&](size_t index, uint16_t address) {
    payload[index * 2] = address & 0xFF;
    payload[index * 2 + 1] = static_cast<uint8_t>((address >> 8) & 0xFF);
  };

  size_t pointer_index = 0;

  for (auto* song : songs_in_bank) {
    auto serialized_or = SpcSerializer::SerializeSong(*song, 0);
    if (!serialized_or.ok()) {
      return serialized_or.status();
    }
    auto serialized = std::move(serialized_or.value());

    const uint32_t song_size = serialized.data.size();
    if (current_spc_address + song_size > bank_limit) {
      return absl::ResourceExhaustedError(
          absl::StrFormat("Bank %d overflow (%u bytes needed, limit %u)",
                          static_cast<int>(bank),
                          current_spc_address + song_size - bank_base,
                          GetBankMaxSize(bank)));
    }

    const uint16_t song_base = static_cast<uint16_t>(current_spc_address);
    SpcSerializer::ApplyBaseAddress(&serialized, song_base);

    write_pointer_entry(pointer_index++, song_base);
    payload.insert(payload.end(), serialized.data.begin(),
                   serialized.data.end());

    song->rom_address = song_base;
    song->modified = false;
    current_spc_address += song_size;
  }

  write_pointer_entry(pointer_index, 0);

  if (payload.size() > static_cast<size_t>(GetBankMaxSize(bank))) {
    return absl::ResourceExhaustedError(
        absl::StrFormat("Bank %d payload size %zu exceeds limit %d",
                        static_cast<int>(bank), payload.size(),
                        GetBankMaxSize(bank)));
  }

  const uint16_t block_size = static_cast<uint16_t>(payload.size());
  std::vector<uint8_t> block_data;
  block_data.reserve(static_cast<size_t>(block_size) + 8);
  block_data.push_back(block_size & 0xFF);
  block_data.push_back(static_cast<uint8_t>((block_size >> 8) & 0xFF));
  block_data.push_back(bank_base & 0xFF);
  block_data.push_back(static_cast<uint8_t>((bank_base >> 8) & 0xFF));
  block_data.insert(block_data.end(), payload.begin(), payload.end());
  block_data.push_back(0x00);
  block_data.push_back(0x00);
  block_data.push_back(0x00);
  block_data.push_back(0x00);

  const uint32_t rom_offset = GetBankRomAddress(bank);
  if (rom_offset + block_data.size() > rom.size()) {
    return absl::OutOfRangeError(
        absl::StrFormat("Bank %d ROM write exceeds image size (offset=%u, "
                        "size=%zu, rom_size=%zu)",
                        static_cast<int>(bank), rom_offset,
                        block_data.size(), rom.size()));
  }

  auto status =
      rom.WriteVector(static_cast<int>(rom_offset), std::move(block_data));
  if (!status.ok()) return status;

  status = UpdateDynamicBankPointer(rom, bank, rom_offset);
  if (!status.ok()) return status;

  return absl::OkStatus();
}

absl::Status MusicBank::LoadInstruments(Rom& rom) {
  instruments_.clear();

  const uint32_t rom_offset =
      SpcParser::SpcAddressToRomOffset(rom, kInstrumentTableAram, /*bank=*/0);
  if (rom_offset == 0) {
    return absl::InvalidArgumentError(
        "Unable to resolve instrument table address in ROM");
  }

  const size_t table_size = kVanillaInstrumentCount * kInstrumentEntrySize;
  if (rom_offset + table_size > rom.size()) {
    return absl::OutOfRangeError("Instrument table exceeds ROM bounds");
  }

  ASSIGN_OR_RETURN(auto bytes,
                   rom.ReadByteVector(rom_offset, static_cast<uint32_t>(table_size)));

  instruments_.reserve(kVanillaInstrumentCount);
  for (int i = 0; i < kVanillaInstrumentCount; ++i) {
    const size_t base = static_cast<size_t>(i) * kInstrumentEntrySize;
    MusicInstrument inst;
    inst.sample_index = bytes[base];
    inst.SetFromBytes(bytes[base + 1], bytes[base + 2]);
    inst.gain = bytes[base + 3];
    inst.pitch_mult = static_cast<uint16_t>(
        (static_cast<uint16_t>(bytes[base + 4]) << 8) | bytes[base + 5]);
    inst.name = kAltTpInstrumentNames[i];
    instruments_.push_back(std::move(inst));
  }

  return absl::OkStatus();
}

absl::Status MusicBank::SaveInstruments(Rom& rom) {
  // TODO: Implement instrument serialization
  return absl::UnimplementedError("SaveInstruments not yet implemented");
}

absl::Status MusicBank::LoadSamples(Rom& rom) {
  samples_.clear();

  // Read sample directory (DIR) at $3C00 in ARAM (Bank 0)
  // Each entry is 4 bytes: [StartAddr:2][LoopAddr:2]
  const uint16_t dir_address = kSampleTableAram;
  int dir_length = 0;
  const uint8_t* dir_data =
      SpcParser::GetSpcData(rom, dir_address, 0, &dir_length);

  if (!dir_data) {
    return absl::InternalError("Failed to locate sample directory in ROM");
  }

  // Scan directory to find max valid sample index
  // Max size is 256 bytes (64 samples), but often smaller
  const int max_samples = std::min(64, dir_length / 4);

  for (int i = 0; i < max_samples; ++i) {
    uint16_t start_addr = dir_data[i * 4] | (dir_data[i * 4 + 1] << 8);
    uint16_t loop_addr = dir_data[i * 4 + 2] | (dir_data[i * 4 + 3] << 8);

    MusicSample sample;
    sample.name = absl::StrFormat("Sample %02X", i);
    // Store loop point as relative offset from start
    sample.loop_point = (loop_addr >= start_addr) ? (loop_addr - start_addr) : 0;

    // Resolve start address to ROM offset
    uint32_t rom_offset = SpcParser::SpcAddressToRomOffset(rom, start_addr, 0);

    if (rom_offset == 0 || rom_offset >= rom.size()) {
      // Invalid or empty sample slot
      samples_.push_back(std::move(sample));
      continue;
    }

    // Read BRR blocks until END bit is set
    const uint8_t* rom_ptr = rom.data() + rom_offset;
    size_t remaining = rom.size() - rom_offset;

    while (remaining >= 9) {
      // Append block to BRR data
      sample.brr_data.insert(sample.brr_data.end(), rom_ptr, rom_ptr + 9);

      // Check END bit in header (bit 0)
      if (rom_ptr[0] & 0x01) {
        sample.loops = (rom_ptr[0] & 0x02) != 0;
        break;
      }

      rom_ptr += 9;
      remaining -= 9;
    }

    // Decode to PCM for visualization/editing
    if (!sample.brr_data.empty()) {
      sample.pcm_data = BrrCodec::Decode(sample.brr_data);
    }

    samples_.push_back(std::move(sample));
  }

  return absl::OkStatus();
}

absl::Status MusicBank::SaveSamples(Rom& rom) {
  // TODO: Implement BRR encoding and sample saving
  return absl::UnimplementedError("SaveSamples not yet implemented");
}

int MusicBank::CalculateSongSize(const MusicSong& song) const {
  // Rough estimate: header + segment pointers + track data
  int size = 2;  // Song header

  for (const auto& segment : song.segments) {
    size += 16;  // 8 track pointers (2 bytes each)

    for (const auto& track : segment.tracks) {
      if (track.is_empty) {
        size += 1;  // Just end marker
      } else {
        // Estimate: each event ~2-4 bytes on average
        size += static_cast<int>(track.events.size()) * 3;
        size += 1;  // End marker
      }
    }
  }

  if (song.HasLoop()) {
    size += 4;  // Loop marker and pointer
  }

  return size;
}

nlohmann::json MusicBank::ToJson() const {
  nlohmann::json root;
  nlohmann::json songs = nlohmann::json::array();
  for (const auto& song : songs_) {
    nlohmann::json js;
    js["name"] = song.name;
    js["bank"] = song.bank;
    js["loop_point"] = song.loop_point;
    js["rom_address"] = song.rom_address;
    js["modified"] = song.modified;

    nlohmann::json segments = nlohmann::json::array();
    for (const auto& segment : song.segments) {
      nlohmann::json jseg;
      jseg["rom_address"] = segment.rom_address;
      nlohmann::json tracks = nlohmann::json::array();
      for (const auto& track : segment.tracks) {
        nlohmann::json jt;
        jt["rom_address"] = track.rom_address;
        jt["duration_ticks"] = track.duration_ticks;
        jt["is_empty"] = track.is_empty;
        nlohmann::json events = nlohmann::json::array();
        for (const auto& evt : track.events) {
          nlohmann::json jevt;
          jevt["tick"] = evt.tick;
          jevt["rom_offset"] = evt.rom_offset;
          switch (evt.type) {
            case TrackEvent::Type::Note:
              jevt["type"] = "note";
              jevt["note"]["pitch"] = evt.note.pitch;
              jevt["note"]["duration"] = evt.note.duration;
              jevt["note"]["velocity"] = evt.note.velocity;
              jevt["note"]["has_duration_prefix"] =
                  evt.note.has_duration_prefix;
              break;
            case TrackEvent::Type::Command:
              jevt["type"] = "command";
              jevt["command"]["opcode"] = evt.command.opcode;
              jevt["command"]["params"] = evt.command.params;
              break;
            case TrackEvent::Type::SubroutineCall:
              jevt["type"] = "subroutine";
              jevt["command"]["opcode"] = evt.command.opcode;
              jevt["command"]["params"] = evt.command.params;
              break;
            case TrackEvent::Type::End:
            default:
              jevt["type"] = "end";
              break;
          }
          events.push_back(std::move(jevt));
        }
        jt["events"] = std::move(events);
        tracks.push_back(std::move(jt));
      }
      jseg["tracks"] = std::move(tracks);
      segments.push_back(std::move(jseg));
    }
    js["segments"] = std::move(segments);
    songs.push_back(std::move(js));
  }

  nlohmann::json instruments = nlohmann::json::array();
  for (const auto& inst : instruments_) {
    nlohmann::json ji;
    ji["name"] = inst.name;
    ji["sample_index"] = inst.sample_index;
    ji["attack"] = inst.attack;
    ji["decay"] = inst.decay;
    ji["sustain_level"] = inst.sustain_level;
    ji["sustain_rate"] = inst.sustain_rate;
    ji["gain"] = inst.gain;
    ji["pitch_mult"] = inst.pitch_mult;
    instruments.push_back(std::move(ji));
  }

  nlohmann::json samples = nlohmann::json::array();
  for (const auto& sample : samples_) {
    nlohmann::json jsample;
    jsample["name"] = sample.name;
    jsample["loop_point"] = sample.loop_point;
    jsample["loops"] = sample.loops;
    jsample["pcm_data"] = sample.pcm_data;
    jsample["brr_data"] = sample.brr_data;
    samples.push_back(std::move(jsample));
  }

  root["songs"] = std::move(songs);
  root["instruments"] = std::move(instruments);
  root["samples"] = std::move(samples);
  root["overworld_song_count"] = overworld_song_count_;
  root["dungeon_song_count"] = dungeon_song_count_;
  root["credits_song_count"] = credits_song_count_;
  return root;
}

absl::Status MusicBank::LoadFromJson(const nlohmann::json& j) {
  try {
    songs_.clear();
    instruments_.clear();
    samples_.clear();

    if (j.contains("songs") && j["songs"].is_array()) {
      for (const auto& js : j["songs"]) {
        MusicSong song;
        song.name = js.value("name", "");
        song.bank = js.value("bank", 0);
        song.loop_point = js.value("loop_point", -1);
        song.rom_address = js.value("rom_address", 0);
        song.modified = js.value("modified", false);

        if (js.contains("segments") && js["segments"].is_array()) {
          for (const auto& jseg : js["segments"]) {
            MusicSegment seg;
            seg.rom_address = jseg.value("rom_address", 0);
            if (jseg.contains("tracks") && jseg["tracks"].is_array()) {
              int track_idx = 0;
              for (const auto& jt : jseg["tracks"]) {
                if (track_idx >= 8) break;
                auto& track = seg.tracks[track_idx++];
                track.rom_address = jt.value("rom_address", 0);
                track.duration_ticks = jt.value("duration_ticks", 0);
                track.is_empty = jt.value("is_empty", false);
                if (jt.contains("events") && jt["events"].is_array()) {
                  track.events.clear();
                  for (const auto& jevt : jt["events"]) {
                    TrackEvent evt;
                    evt.tick = jevt.value("tick", 0);
                    evt.rom_offset = jevt.value("rom_offset", 0);
                    std::string type = jevt.value("type", "end");
                    if (type == "note" && jevt.contains("note")) {
                      evt.type = TrackEvent::Type::Note;
                      evt.note.pitch = jevt["note"].value("pitch", kNoteRest);
                      evt.note.duration =
                          jevt["note"].value("duration", uint8_t{0});
                      evt.note.velocity =
                          jevt["note"].value("velocity", uint8_t{0});
                      evt.note.has_duration_prefix =
                          jevt["note"].value("has_duration_prefix", false);
                    } else if (type == "command" || type == "subroutine") {
                      evt.type = (type == "subroutine")
                                     ? TrackEvent::Type::SubroutineCall
                                     : TrackEvent::Type::Command;
                      evt.command.opcode =
                          jevt["command"].value("opcode", uint8_t{0});
                      auto params = jevt["command"].value(
                          "params", std::array<uint8_t, 3>{0, 0, 0});
                      evt.command.params = params;
                    } else {
                      evt.type = TrackEvent::Type::End;
                    }
                    track.events.push_back(std::move(evt));
                  }
                }
              }
            }
            song.segments.push_back(std::move(seg));
          }
        }
        songs_.push_back(std::move(song));
      }
    }

    if (j.contains("instruments") && j["instruments"].is_array()) {
      for (const auto& ji : j["instruments"]) {
        MusicInstrument inst;
        inst.name = ji.value("name", "");
        inst.sample_index = ji.value("sample_index", 0);
        inst.attack = ji.value("attack", 0);
        inst.decay = ji.value("decay", 0);
        inst.sustain_level = ji.value("sustain_level", 0);
        inst.sustain_rate = ji.value("sustain_rate", 0);
        inst.gain = ji.value("gain", 0);
        inst.pitch_mult = ji.value("pitch_mult", 0);
        instruments_.push_back(std::move(inst));
      }
    }

    if (j.contains("samples") && j["samples"].is_array()) {
      for (const auto& jsample : j["samples"]) {
        MusicSample sample;
        sample.name = jsample.value("name", "");
        sample.loop_point = jsample.value("loop_point", 0);
        sample.loops = jsample.value("loops", false);
        sample.pcm_data = jsample.value("pcm_data", std::vector<int16_t>{});
        sample.brr_data = jsample.value("brr_data", std::vector<uint8_t>{});
        samples_.push_back(std::move(sample));
      }
    }

    overworld_song_count_ = j.value("overworld_song_count", 0);
    dungeon_song_count_ = j.value("dungeon_song_count", 0);
    credits_song_count_ = j.value("credits_song_count", 0);

    loaded_ = true;
    return absl::OkStatus();
  } catch (const std::exception& e) {
    return absl::InvalidArgumentError(
        absl::StrFormat("Failed to parse music state: %s", e.what()));
  }
}

// =============================================================================
// Helper Functions
// =============================================================================

const char* GetVanillaSongName(int song_id) {
  if (song_id <= 0 || song_id > kVanillaSongCount) {
    return "Unknown";
  }
  return kVanillaSongs[song_id].name;
}

MusicBank::Bank GetVanillaSongBank(int song_id) {
  if (song_id <= 0 || song_id > kVanillaSongCount) {
    return MusicBank::Bank::Overworld;
  }
  return kVanillaSongs[song_id].bank;
}

MusicBank::BankSongRange MusicBank::GetBankSongRange(Bank bank) {
  if (const auto* metadata = GetMetadataForBank(bank)) {
    return BankSongRange{metadata->vanilla_start_id,
                         metadata->vanilla_end_id};
  }
  return {};
}

uint8_t MusicBank::GetSpcBankId(Bank bank) {
  if (const auto* metadata = GetMetadataForBank(bank)) {
    return metadata->spc_bank;
  }
  return 0;
}

}  // namespace music
}  // namespace zelda3
}  // namespace yaze
