#include "zelda3/dungeon/water_fill_zone.h"

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <fstream>
#include <limits>
#include <optional>
#include <regex>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

#if defined(YAZE_WITH_JSON)
#include "nlohmann/json.hpp"
#endif

#include "absl/status/status.h"
#include "absl/strings/str_format.h"
#include "rom/rom.h"
#include "rom/snes.h"
#include "rom/write_fence.h"
#include "util/macro.h"
#include "zelda3/dungeon/dungeon_rom_addresses.h"

namespace yaze::zelda3 {

namespace {

constexpr int kRoomCount = kNumberOfRooms;
constexpr int kGridSize = 64;
constexpr int kGridTiles = kGridSize * kGridSize;
constexpr uint16_t kCollisionSingleTileMarker = 0xF0F0;
constexpr uint16_t kCollisionEndMarker = 0xFFFF;

bool IsSingleBitMask(uint8_t mask) {
  return mask != 0 && (mask & (mask - 1)) == 0;
}

absl::Status ValidateZone(const WaterFillZoneEntry& z) {
  if (z.room_id < 0 || z.room_id >= kRoomCount) {
    return absl::OutOfRangeError("WaterFillZoneEntry room_id out of range");
  }
  if (!IsSingleBitMask(z.sram_bit_mask)) {
    return absl::InvalidArgumentError(
        "WaterFillZoneEntry sram_bit_mask must be a single bit");
  }
  if (z.fill_offsets.size() > 255) {
    return absl::InvalidArgumentError(
        "WaterFillZoneEntry fill_offsets exceeds 255 tiles (db count limit)");
  }
  for (uint16_t off : z.fill_offsets) {
    if (off >= kGridTiles) {
      return absl::OutOfRangeError(
          absl::StrFormat("WaterFillZoneEntry offset out of range: %u", off));
    }
  }
  return absl::OkStatus();
}

std::vector<WaterFillZoneEntry> DedupAndSort(
    std::vector<WaterFillZoneEntry> zones) {
  // Ensure stable output for deterministic ROM writes.
  std::sort(zones.begin(), zones.end(),
            [](const WaterFillZoneEntry& a, const WaterFillZoneEntry& b) {
              return a.room_id < b.room_id;
            });
  for (auto& z : zones) {
    std::sort(z.fill_offsets.begin(), z.fill_offsets.end());
    z.fill_offsets.erase(
        std::unique(z.fill_offsets.begin(), z.fill_offsets.end()),
        z.fill_offsets.end());
  }
  return zones;
}

std::optional<std::string> GuessSymbolPathFromRom(const std::string& rom_path) {
  if (rom_path.empty()) {
    return std::nullopt;
  }

  // Common convention in this repo: <rom>.sfc -> <rom>.sym
  // Example: oos168x.sfc -> oos168x.sym
  auto dot = rom_path.find_last_of('.');
  if (dot == std::string::npos) {
    return std::nullopt;
  }
  std::string sym = rom_path.substr(0, dot) + ".sym";
  std::ifstream f(sym);
  if (!f.good()) {
    return std::nullopt;
  }
  return sym;
}

absl::StatusOr<uint32_t> FindCustomCollisionRoomEndPc(
    const std::vector<uint8_t>& rom_data, uint32_t start_pc) {
  if (start_pc >= rom_data.size()) {
    return absl::OutOfRangeError("Collision data pointer out of range");
  }

  size_t cursor = static_cast<size_t>(start_pc);
  bool single_mode = false;
  while (cursor + 1 < rom_data.size()) {
    const uint16_t val =
        static_cast<uint16_t>(rom_data[cursor] | (rom_data[cursor + 1] << 8));
    cursor += 2;

    if (val == kCollisionEndMarker) {
      break;
    }
    if (val == kCollisionSingleTileMarker) {
      single_mode = true;
      continue;
    }

    if (!single_mode) {
      if (cursor + 1 >= rom_data.size()) {
        return absl::OutOfRangeError("Collision rectangle header out of range");
      }
      const uint8_t w = rom_data[cursor];
      const uint8_t h = rom_data[cursor + 1];
      cursor += 2;
      cursor += static_cast<size_t>(w) * static_cast<size_t>(h);
    } else {
      if (cursor >= rom_data.size()) {
        return absl::OutOfRangeError("Collision single tile out of range");
      }
      cursor += 1;
    }
  }

  return static_cast<uint32_t>(cursor);
}

absl::Status ValidateCustomCollisionDoesNotOverlapWaterFillReserved(
    const std::vector<uint8_t>& rom_data) {
  const int ptrs_size = kNumberOfRooms * 3;
  if (kCustomCollisionRoomPointers + ptrs_size >
      static_cast<int>(rom_data.size())) {
    // Vanilla ROMs don't have the expanded collision bank or pointer table.
    // The caller will already fail if the reserved region isn't present.
    return absl::OkStatus();
  }

  for (int room_id = 0; room_id < kNumberOfRooms; ++room_id) {
    const int ptr_offset = kCustomCollisionRoomPointers + (room_id * 3);
    if (ptr_offset + 2 >= static_cast<int>(rom_data.size())) {
      return absl::OutOfRangeError("Collision pointer table out of range");
    }

    const uint32_t snes_ptr =
        rom_data[ptr_offset] | (rom_data[ptr_offset + 1] << 8) |
        (rom_data[ptr_offset + 2] << 16);
    if (snes_ptr == 0) {
      continue;
    }

    const uint32_t pc = SnesToPc(snes_ptr);
    if (pc >= static_cast<uint32_t>(kWaterFillTableStart) &&
        pc < static_cast<uint32_t>(kWaterFillTableEnd)) {
      return absl::FailedPreconditionError(absl::StrFormat(
          "Custom collision pointer for room 0x%02X overlaps WaterFill reserved region (pc=0x%06X)",
          room_id, pc));
    }

    ASSIGN_OR_RETURN(const uint32_t end_pc,
                     FindCustomCollisionRoomEndPc(rom_data, pc));
    if (end_pc > static_cast<uint32_t>(kCustomCollisionDataSoftEnd)) {
      return absl::FailedPreconditionError(absl::StrFormat(
          "Custom collision data for room 0x%02X overlaps WaterFill reserved region (end=0x%06X, reserved_start=0x%06X)",
          room_id, end_pc, kCustomCollisionDataSoftEnd));
    }
  }

  return absl::OkStatus();
}

}  // namespace

absl::StatusOr<std::vector<WaterFillZoneEntry>> LoadWaterFillTable(Rom* rom) {
  if (!rom || !rom->is_loaded()) {
    return absl::InvalidArgumentError("ROM not loaded");
  }

  const auto& data = rom->vector();
  if (kWaterFillTableEnd > static_cast<int>(data.size())) {
    // On vanilla ROMs (no expanded collision bank), just treat as absent.
    return std::vector<WaterFillZoneEntry>{};
  }

  // ROM safety: ensure custom collision does not overlap the reserved WaterFill
  // table region. This catches corrupted ROM layouts early and prevents the
  // editor from presenting potentially invalid authoring state.
  RETURN_IF_ERROR(ValidateCustomCollisionDoesNotOverlapWaterFillReserved(data));

  const uint8_t zone_count = data[static_cast<size_t>(kWaterFillTableStart)];
  if (zone_count == 0) {
    return std::vector<WaterFillZoneEntry>{};
  }

  // Heuristic: we only have 8 bits in $7EF411 per current runtime design.
  // If the region contains random bytes, this avoids false-positives.
  if (zone_count > 8) {
    return std::vector<WaterFillZoneEntry>{};
  }

  const size_t header_size = 1u + (static_cast<size_t>(zone_count) * 4u);
  if (header_size > static_cast<size_t>(kWaterFillTableReservedSize)) {
    return absl::FailedPreconditionError(
        "WaterFill table header exceeds reserved region");
  }

  struct Entry {
    int room_id;
    uint8_t mask;
    uint16_t data_off;
  };
  std::vector<Entry> entries;
  entries.reserve(zone_count);

  std::unordered_map<int, size_t> room_to_index;
  for (size_t i = 0; i < zone_count; ++i) {
    size_t off = static_cast<size_t>(kWaterFillTableStart) + 1u + (i * 4u);
    int room_id = data[off];
    uint8_t mask = data[off + 1];
    uint16_t data_off = static_cast<uint16_t>(data[off + 2] | (data[off + 3] << 8));

    if (room_id < 0 || room_id >= kRoomCount) {
      return absl::FailedPreconditionError(
          absl::StrFormat("WaterFill entry room_id invalid: %d", room_id));
    }
    if (!IsSingleBitMask(mask)) {
      return absl::FailedPreconditionError(
          absl::StrFormat("WaterFill entry invalid sram mask: 0x%02X", mask));
    }
    if (room_to_index.contains(room_id)) {
      return absl::FailedPreconditionError(
          absl::StrFormat("Duplicate WaterFill entry for room 0x%02X", room_id));
    }
    room_to_index[room_id] = entries.size();

    // data_off is relative to table start.
    if (data_off >= kWaterFillTableReservedSize) {
      return absl::FailedPreconditionError(
          absl::StrFormat("WaterFill entry data offset out of range: 0x%04X",
                          data_off));
    }
    if (data_off < header_size) {
      return absl::FailedPreconditionError(
          "WaterFill entry data offset overlaps table header");
    }

    entries.push_back(Entry{room_id, mask, data_off});
  }

  std::vector<WaterFillZoneEntry> zones;
  zones.reserve(entries.size());
  for (const auto& e : entries) {
    const size_t data_pos =
        static_cast<size_t>(kWaterFillTableStart) + static_cast<size_t>(e.data_off);
    if (data_pos >= static_cast<size_t>(kWaterFillTableEnd)) {
      return absl::FailedPreconditionError("WaterFill entry data_pos out of range");
    }
    const uint8_t tile_count = data[data_pos];
    if (tile_count > 255) {
      return absl::FailedPreconditionError("WaterFill tile_count invalid");
    }
    const size_t needed = 1u + static_cast<size_t>(tile_count) * 2u;
    if (data_pos + needed > static_cast<size_t>(kWaterFillTableEnd)) {
      return absl::FailedPreconditionError(
          "WaterFill entry tile data exceeds reserved region");
    }

    WaterFillZoneEntry z;
    z.room_id = e.room_id;
    z.sram_bit_mask = e.mask;
    z.fill_offsets.reserve(tile_count);
    for (size_t i = 0; i < tile_count; ++i) {
      const size_t o = data_pos + 1u + (i * 2u);
      uint16_t off = static_cast<uint16_t>(data[o] | (data[o + 1] << 8));
      if (off >= kGridTiles) {
        return absl::FailedPreconditionError(
            absl::StrFormat("WaterFill offset out of range: %u", off));
      }
      z.fill_offsets.push_back(off);
    }

    zones.push_back(std::move(z));
  }

  zones = DedupAndSort(std::move(zones));
  for (const auto& z : zones) {
    RETURN_IF_ERROR(ValidateZone(z));
  }
  return zones;
}

absl::Status WriteWaterFillTable(Rom* rom,
                                 const std::vector<WaterFillZoneEntry>& zones) {
  if (!rom || !rom->is_loaded()) {
    return absl::InvalidArgumentError("ROM not loaded");
  }

  const auto& rom_data = rom->vector();
  if (kWaterFillTableEnd > static_cast<int>(rom_data.size())) {
    return absl::OutOfRangeError(
        "WaterFill reserved region not present in this ROM");
  }

  // Safety: never clobber legacy/custom collision data that might already
  // occupy the reserved tail region.
  RETURN_IF_ERROR(ValidateCustomCollisionDoesNotOverlapWaterFillReserved(rom_data));

  // Enforce current SRAM bitfield capacity.
  if (zones.size() > 8) {
    return absl::InvalidArgumentError(
        "Too many water fill zones: max 8 (fits in $7EF411 bitfield)");
  }

  // Validate + normalize.
  auto normalized = DedupAndSort(zones);
  std::unordered_map<int, uint8_t> seen_rooms;
  std::unordered_map<uint8_t, int> seen_masks;
  for (const auto& z : normalized) {
    RETURN_IF_ERROR(ValidateZone(z));
    if (seen_rooms.contains(z.room_id)) {
      return absl::InvalidArgumentError(
          absl::StrFormat("Duplicate water fill zone for room 0x%02X", z.room_id));
    }
    seen_rooms[z.room_id] = z.sram_bit_mask;
    if (seen_masks.contains(z.sram_bit_mask)) {
      return absl::InvalidArgumentError(
          absl::StrFormat("Duplicate SRAM mask 0x%02X used by rooms 0x%02X and 0x%02X",
                          z.sram_bit_mask, seen_masks[z.sram_bit_mask],
                          z.room_id));
    }
    seen_masks[z.sram_bit_mask] = z.room_id;
  }

  std::vector<uint8_t> bytes;
  bytes.reserve(static_cast<size_t>(kWaterFillTableReservedSize));
  bytes.push_back(static_cast<uint8_t>(normalized.size()));

  // Header entries with placeholder offsets.
  for (const auto& z : normalized) {
    bytes.push_back(static_cast<uint8_t>(z.room_id & 0xFF));
    bytes.push_back(z.sram_bit_mask);
    bytes.push_back(0x00);  // data_offset lo
    bytes.push_back(0x00);  // data_offset hi
  }

  // Data sections.
  for (size_t i = 0; i < normalized.size(); ++i) {
    const auto& z = normalized[i];
    const uint16_t data_off = static_cast<uint16_t>(bytes.size());
    const size_t entry_off = 1u + (i * 4u);
    bytes[entry_off + 2] = data_off & 0xFF;
    bytes[entry_off + 3] = (data_off >> 8) & 0xFF;

    bytes.push_back(static_cast<uint8_t>(z.fill_offsets.size()));
    for (uint16_t off : z.fill_offsets) {
      bytes.push_back(off & 0xFF);
      bytes.push_back((off >> 8) & 0xFF);
    }
  }

  if (bytes.size() > static_cast<size_t>(kWaterFillTableReservedSize)) {
    return absl::ResourceExhaustedError(absl::StrFormat(
        "WaterFill table too large (%zu bytes), reserved=%d", bytes.size(),
        kWaterFillTableReservedSize));
  }

  // Write full reserved region (zero-filled tail) for determinism.
  std::vector<uint8_t> region(static_cast<size_t>(kWaterFillTableReservedSize),
                              0);
  std::copy(bytes.begin(), bytes.end(), region.begin());

  // Save-time guardrails: this writer must only touch the reserved WaterFill
  // region, never other ROM content.
  yaze::rom::WriteFence fence;
  RETURN_IF_ERROR(
      fence.Allow(static_cast<uint32_t>(kWaterFillTableStart),
                  static_cast<uint32_t>(kWaterFillTableEnd), "WaterFillTable"));
  yaze::rom::ScopedWriteFence scope(rom, &fence);
  return rom->WriteVector(kWaterFillTableStart, std::move(region));
}

absl::StatusOr<std::vector<WaterFillZoneEntry>> LoadLegacyWaterGateZones(
    Rom* rom, const std::string& symbol_path) {
  if (!rom || !rom->is_loaded()) {
    return absl::InvalidArgumentError("ROM not loaded");
  }

  std::string path = symbol_path;
  if (path.empty()) {
    if (auto guess = GuessSymbolPathFromRom(rom->filename()); guess.has_value()) {
      path = *guess;
    }
  }
  if (path.empty()) {
    return std::vector<WaterFillZoneEntry>{};
  }

  std::ifstream file(path);
  if (!file.is_open()) {
    return std::vector<WaterFillZoneEntry>{};
  }

  std::optional<uint32_t> room25_snes;
  std::optional<uint32_t> room27_snes;

  // WLA symbol file format: "BB:AAAA Label".
  const std::regex re(R"(^\s*([0-9A-Fa-f]{2}):([0-9A-Fa-f]{4})\s+(.+?)\s*$)");
  std::string line;
  while (std::getline(file, line)) {
    std::smatch m;
    if (!std::regex_match(line, m, re)) {
      continue;
    }
    const int bank = std::stoi(m[1].str(), nullptr, 16);
    const int addr = std::stoi(m[2].str(), nullptr, 16);
    const std::string label = m[3].str();

    if (label == "Oracle_WaterGate_Room25_Data") {
      room25_snes = static_cast<uint32_t>((bank << 16) | addr);
    } else if (label == "Oracle_WaterGate_Room27_Data") {
      room27_snes = static_cast<uint32_t>((bank << 16) | addr);
    }
  }

  const auto& data = rom->vector();
  auto read_zone = [&](int room_id, uint8_t mask,
                       std::optional<uint32_t> snes_opt)
      -> absl::StatusOr<std::optional<WaterFillZoneEntry>> {
    if (!snes_opt.has_value()) {
      return std::optional<WaterFillZoneEntry>{};
    }
    const uint32_t pc = SnesToPc(*snes_opt);
    if (pc >= data.size()) {
      return absl::OutOfRangeError("Legacy water gate data pointer out of range");
    }
    const uint8_t count = data[pc];
    const size_t needed = 1u + static_cast<size_t>(count) * 2u;
    if (pc + needed > data.size()) {
      return absl::OutOfRangeError("Legacy water gate data exceeds ROM");
    }

    WaterFillZoneEntry z;
    z.room_id = room_id;
    z.sram_bit_mask = mask;
    z.fill_offsets.reserve(count);
    for (size_t i = 0; i < count; ++i) {
      const size_t o = pc + 1u + (i * 2u);
      uint16_t off = static_cast<uint16_t>(data[o] | (data[o + 1] << 8));
      if (off >= kGridTiles) {
        return absl::FailedPreconditionError(
            absl::StrFormat("Legacy water gate offset out of range: %u", off));
      }
      z.fill_offsets.push_back(off);
    }

    RETURN_IF_ERROR(ValidateZone(z));
    return std::optional<WaterFillZoneEntry>(std::move(z));
  };

  std::vector<WaterFillZoneEntry> zones;

  // Legacy mapping from water_collision.asm:
  // bit 0 = room 0x27, bit 1 = room 0x25
  ASSIGN_OR_RETURN(auto z27, read_zone(0x27, 0x01, room27_snes));
  ASSIGN_OR_RETURN(auto z25, read_zone(0x25, 0x02, room25_snes));

  if (z27.has_value()) zones.push_back(std::move(*z27));
  if (z25.has_value()) zones.push_back(std::move(*z25));

  zones = DedupAndSort(std::move(zones));
  return zones;
}

absl::Status NormalizeWaterFillZoneMasks(std::vector<WaterFillZoneEntry>* zones) {
  if (zones == nullptr) {
    return absl::InvalidArgumentError("zones is null");
  }
  if (zones->size() > 8) {
    return absl::InvalidArgumentError(absl::StrFormat(
        "Too many water fill zones: %zu (max 8 fits in $7EF411 bitfield)",
        zones->size()));
  }

  // Ensure stable room ordering + deterministic offset layout.
  *zones = DedupAndSort(std::move(*zones));

  std::unordered_map<int, bool> seen_rooms;
  uint8_t used_masks = 0;
  std::vector<WaterFillZoneEntry*> unassigned;
  unassigned.reserve(zones->size());

  for (auto& z : *zones) {
    if (z.room_id < 0 || z.room_id >= kRoomCount) {
      return absl::OutOfRangeError(
          absl::StrFormat("WaterFill room_id out of range: %d", z.room_id));
    }
    if (seen_rooms.contains(z.room_id)) {
      return absl::InvalidArgumentError(
          absl::StrFormat("Duplicate water fill zone for room 0x%02X", z.room_id));
    }
    seen_rooms[z.room_id] = true;

    const uint8_t mask = z.sram_bit_mask;
    const bool valid =
        (mask != 0) && IsSingleBitMask(mask) && ((used_masks & mask) == 0);
    if (valid) {
      used_masks |= mask;
    } else {
      z.sram_bit_mask = 0;
      unassigned.push_back(&z);
    }
  }

  constexpr uint8_t kBits[8] = {0x01, 0x02, 0x04, 0x08,
                                0x10, 0x20, 0x40, 0x80};
  for (auto* z : unassigned) {
    uint8_t assigned = 0;
    for (uint8_t bit : kBits) {
      if ((used_masks & bit) == 0) {
        assigned = bit;
        break;
      }
    }
    if (assigned == 0) {
      return absl::ResourceExhaustedError(
          "No free SRAM bits left in $7EF411 for water fill zones");
    }
    z->sram_bit_mask = assigned;
    used_masks |= assigned;
  }

  return absl::OkStatus();
}

absl::StatusOr<std::string> DumpWaterFillZonesToJsonString(
    const std::vector<WaterFillZoneEntry>& zones) {
#if !defined(YAZE_WITH_JSON)
  return absl::UnimplementedError(
      "JSON support not enabled. Build with -DYAZE_WITH_JSON=ON");
#else
  using json = nlohmann::json;

  std::vector<WaterFillZoneEntry> sorted = zones;
  std::sort(sorted.begin(), sorted.end(),
            [](const WaterFillZoneEntry& a, const WaterFillZoneEntry& b) {
              return a.room_id < b.room_id;
            });
  for (auto& z : sorted) {
    std::sort(z.fill_offsets.begin(), z.fill_offsets.end());
    z.fill_offsets.erase(
        std::unique(z.fill_offsets.begin(), z.fill_offsets.end()),
        z.fill_offsets.end());
  }

  json root;
  root["version"] = 1;
  json arr = json::array();
  for (const auto& z : sorted) {
    json item;
    item["room_id"] = absl::StrFormat("0x%02X", z.room_id);
    item["mask"] = absl::StrFormat("0x%02X", z.sram_bit_mask);
    item["offsets"] = z.fill_offsets;
    arr.push_back(std::move(item));
  }
  root["zones"] = std::move(arr);
  return root.dump(/*indent=*/2);
#endif
}

absl::StatusOr<std::vector<WaterFillZoneEntry>> LoadWaterFillZonesFromJsonString(
    const std::string& json_content) {
#if !defined(YAZE_WITH_JSON)
  return absl::UnimplementedError(
      "JSON support not enabled. Build with -DYAZE_WITH_JSON=ON");
#else
  using json = nlohmann::json;

  json root;
  try {
    root = json::parse(json_content);
  } catch (const json::parse_error& e) {
    return absl::InvalidArgumentError(
        std::string("JSON parse error: ") + e.what());
  }

  const int version = root.value("version", 1);
  if (version != 1) {
    return absl::InvalidArgumentError(
        absl::StrFormat("Unsupported water fill JSON version: %d", version));
  }
  if (!root.contains("zones") || !root["zones"].is_array()) {
    return absl::InvalidArgumentError("Missing or invalid 'zones' array");
  }

  auto parse_int = [](const json& v) -> std::optional<int> {
    if (v.is_number_integer()) {
      return v.get<int>();
    }
    if (v.is_number_unsigned()) {
      const auto u = v.get<unsigned int>();
      if (u > static_cast<unsigned int>(std::numeric_limits<int>::max())) {
        return std::nullopt;
      }
      return static_cast<int>(u);
    }
    if (v.is_string()) {
      try {
        size_t idx = 0;
        const std::string s = v.get<std::string>();
        const int parsed = std::stoi(s, &idx, 0);
        if (idx == s.size()) {
          return parsed;
        }
      } catch (...) {
      }
    }
    return std::nullopt;
  };

  std::vector<WaterFillZoneEntry> zones;
  zones.reserve(root["zones"].size());

  std::unordered_map<int, bool> seen_rooms;
  for (const auto& item : root["zones"]) {
    if (!item.is_object()) {
      continue;
    }

    const json& room_v =
        item.contains("room_id") ? item["room_id"]
        : item.contains("room")  ? item["room"]
                                 : json();
    const auto room_id_opt = parse_int(room_v);
    if (!room_id_opt.has_value() || *room_id_opt < 0 ||
        *room_id_opt >= kNumberOfRooms) {
      return absl::InvalidArgumentError("Invalid room_id in water fill JSON");
    }
    const int room_id = *room_id_opt;

    if (seen_rooms.contains(room_id)) {
      return absl::InvalidArgumentError(
          absl::StrFormat("Duplicate room_id in water fill JSON: 0x%02X",
                          room_id));
    }
    seen_rooms[room_id] = true;

    const json& mask_v =
        item.contains("mask")          ? item["mask"]
        : item.contains("sram_mask")   ? item["sram_mask"]
        : item.contains("sram_bit_mask") ? item["sram_bit_mask"]
                                         : json();
    uint8_t mask = 0;
    if (!mask_v.is_null()) {
      const auto m_opt = parse_int(mask_v);
      if (!m_opt.has_value() || *m_opt < 0 || *m_opt > 0xFF) {
        return absl::InvalidArgumentError(
            absl::StrFormat("Invalid mask for room 0x%02X", room_id));
      }
      mask = static_cast<uint8_t>(*m_opt);
    }

    // Allow 0 (Auto) or a single-bit mask.
    if (mask != 0 && !IsSingleBitMask(mask)) {
      return absl::InvalidArgumentError(
          absl::StrFormat("Invalid mask 0x%02X for room 0x%02X", mask,
                          room_id));
    }

    const json& offsets_v =
        item.contains("offsets")      ? item["offsets"]
        : item.contains("fill_offsets") ? item["fill_offsets"]
                                        : json::array();
    if (!offsets_v.is_array()) {
      return absl::InvalidArgumentError(
          absl::StrFormat("Invalid offsets array for room 0x%02X", room_id));
    }

    WaterFillZoneEntry z;
    z.room_id = room_id;
    z.sram_bit_mask = mask;
    z.fill_offsets.reserve(offsets_v.size());
    for (const auto& off_v : offsets_v) {
      const auto o_opt = parse_int(off_v);
      if (!o_opt.has_value() || *o_opt < 0 || *o_opt >= kGridTiles) {
        return absl::InvalidArgumentError(
            absl::StrFormat("Invalid offset for room 0x%02X", room_id));
      }
      z.fill_offsets.push_back(static_cast<uint16_t>(*o_opt));
      if (z.fill_offsets.size() > 255) {
        return absl::InvalidArgumentError(absl::StrFormat(
            "WaterFill offsets exceed 255 tiles for room 0x%02X", room_id));
      }
    }

    std::sort(z.fill_offsets.begin(), z.fill_offsets.end());
    z.fill_offsets.erase(
        std::unique(z.fill_offsets.begin(), z.fill_offsets.end()),
        z.fill_offsets.end());

    zones.push_back(std::move(z));
  }

  if (zones.size() > 8) {
    return absl::InvalidArgumentError(
        absl::StrFormat("Too many water fill zones in JSON: %zu (max 8)",
                        zones.size()));
  }

  zones = DedupAndSort(std::move(zones));
  return zones;
#endif
}

}  // namespace yaze::zelda3
