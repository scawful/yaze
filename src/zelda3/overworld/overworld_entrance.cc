#include "zelda3/overworld/overworld_entrance.h"

#include <algorithm>
#include <cstddef>
#include <utility>
#include <vector>

#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "rom/rom.h"
#include "util/macro.h"
#include "zelda3/overworld/overworld_version_helper.h"

namespace yaze::zelda3 {
namespace {

bool HasExpandedEntranceTables(const Rom& rom,
                               const core::RomAddressOverrides& addresses) {
  if (rom.vector().size() <= OverworldCustomASMHasBeenApplied) {
    return false;
  }
  const auto version = OverworldVersionHelper::GetVersion(rom);
  if (version == OverworldVersion::kVanilla) {
    return false;
  }
  const auto flag_addr =
      addresses.GetAddress(core::RomAddressKey::kOverworldEntranceFlagExpanded)
          .value_or(kOverworldEntranceExpandedFlagPos);
  if (flag_addr >= rom.vector().size()) {
    return false;
  }
  return rom.data()[flag_addr] != 0xB8;
}

}  // namespace

absl::StatusOr<std::vector<OverworldEntrance>> LoadEntrances(Rom* rom) {
  return LoadEntrances(rom, core::RomSettings::Get().address_overrides());
}

absl::StatusOr<std::vector<OverworldEntrance>> LoadEntrances(
    Rom* rom, const core::RomAddressOverrides& addresses) {
  if (!rom)
    return absl::InvalidArgumentError("Cannot load entrances without a ROM");
  std::vector<OverworldEntrance> entrances;
  int ow_entrance_map_ptr = kOverworldEntranceMap;
  int ow_entrance_pos_ptr = kOverworldEntrancePos;
  int ow_entrance_id_ptr = kOverworldEntranceEntranceId;
  int num_entrances = 129;

  // Check if expanded entrance data is actually present in ROM
  // The flag position should contain 0xB8 for vanilla, something else for
  // expanded
  const bool expanded = HasExpandedEntranceTables(*rom, addresses);
  if (expanded) {
    // ROM has expanded entrance data - use expanded addresses
    ow_entrance_map_ptr =
        addresses.GetAddress(core::RomAddressKey::kOverworldEntranceMapExpanded)
            .value_or(kOverworldEntranceMapExpanded);
    ow_entrance_pos_ptr =
        addresses.GetAddress(core::RomAddressKey::kOverworldEntrancePosExpanded)
            .value_or(kOverworldEntrancePosExpanded);
    ow_entrance_id_ptr =
        addresses.GetAddress(core::RomAddressKey::kOverworldEntranceIdExpanded)
            .value_or(kOverworldEntranceEntranceIdExpanded);
    num_entrances = 256;  // Expanded entrance count
  }
  // Otherwise use vanilla addresses (already set above)

  for (int i = 0; i < num_entrances; i++) {
    ASSIGN_OR_RETURN(auto map_id, rom->ReadWord(ow_entrance_map_ptr + (i * 2)));
    ASSIGN_OR_RETURN(auto map_pos,
                     rom->ReadWord(ow_entrance_pos_ptr + (i * 2)));
    ASSIGN_OR_RETURN(auto entrance_id, rom->ReadByte(ow_entrance_id_ptr + i));
    int p = map_pos >> 1;
    int x = (p % 64);
    int y = (p >> 6);
    const bool deleted = map_pos == 0xFFFF;
    const bool hole = expanded && !deleted && (map_id & 0x8000) != 0;
    // Preserve unused rows' raw map word; live expanded rows encode the
    // entrance kind separately from the area number.
    const uint16_t area = expanded && !deleted ? map_id & 0x7FFF : map_id;
    entrances.emplace_back((x * 16) + ((area % 8) * 512),
                           (y * 16) + (((area % 64) / 8) * 512), entrance_id,
                           area, map_pos, hole);
    entrances.back().deleted = deleted;
    entrances.back().entity_id_ = i;
  }

  return entrances;
}

absl::StatusOr<std::vector<OverworldEntrance>> LoadHoles(Rom* rom) {
  if (!rom)
    return absl::InvalidArgumentError("Cannot load holes without a ROM");
  constexpr int kNumHoles = 0x13;
  std::vector<OverworldEntrance> holes;
  for (int i = 0; i < kNumHoles; i++) {
    ASSIGN_OR_RETURN(auto map_id, rom->ReadWord(kOverworldHoleArea + (i * 2)));
    ASSIGN_OR_RETURN(auto map_pos, rom->ReadWord(kOverworldHolePos + (i * 2)));
    ASSIGN_OR_RETURN(auto entrance_id,
                     rom->ReadByte(kOverworldHoleEntrance + i));
    int p = (map_pos + 0x400) >> 1;
    int x = (p % 64);
    int y = (p >> 6);
    holes.emplace_back(
        (x * 16) + (((map_id % 64) - (((map_id % 64) / 8) * 8)) * 512),
        (y * 16) + (((map_id % 64) / 8) * 512), entrance_id, map_id,
        (uint16_t)(map_pos + 0x400), true);
    holes.back().deleted = map_pos == 0xFFFF;
    holes.back().entity_id_ = i;
    holes.back().RememberHoleSourceState(i);
  }
  return holes;
}

absl::Status SaveEntrances(Rom* rom,
                           const std::vector<OverworldEntrance>& entrances,
                           bool expanded_entrances) {
  if (!rom)
    return absl::InvalidArgumentError("Cannot save entrances without a ROM");
  if (entrances.size() > (expanded_entrances
                              ? 256u
                              : static_cast<size_t>(kNumOverworldEntrances))) {
    return absl::InvalidArgumentError(
        "Too many records for the selected entrance table");
  }
  const size_t count = std::min<size_t>(
      expanded_entrances ? 256 : kNumOverworldEntrances, entrances.size());
  const size_t map_base = expanded_entrances ? GetOverworldEntranceMapExpanded()
                                             : kOverworldEntranceMap;
  const size_t pos_base = expanded_entrances ? GetOverworldEntrancePosExpanded()
                                             : kOverworldEntrancePos;
  const size_t id_base = expanded_entrances ? GetOverworldEntranceIdExpanded()
                                            : kOverworldEntranceEntranceId;
  const size_t size = rom->vector().size();
  for (const auto& range : {std::pair<size_t, size_t>{map_base, count * 2},
                            {pos_base, count * 2},
                            {id_base, count}}) {
    if (range.first > size || range.second > size - range.first) {
      return absl::OutOfRangeError(
          "ROM does not contain complete entrance tables");
    }
  }
  auto write_entrance = [&](int index, uint32_t map_addr, uint32_t pos_addr,
                            uint32_t id_addr) -> absl::Status {
    // Mirrors ZeldaFullEditor/Save.cs::SaveOWEntrances (see lines ~1081-1085)
    // where MapID and MapPos are written as 16-bit words and EntranceID as a
    // byte.
    const auto& record = entrances[index];
    const uint16_t map_word =
        expanded_entrances && !record.deleted
            ? (record.map_id_ & 0x7FFF) | (record.is_hole_ ? 0x8000 : 0)
            : record.map_id_;
    RETURN_IF_ERROR(rom->WriteShort(map_addr, map_word));
    RETURN_IF_ERROR(
        rom->WriteShort(pos_addr, record.deleted ? 0xFFFF : record.map_pos_));
    RETURN_IF_ERROR(rom->WriteByte(id_addr, entrances[index].entrance_id_));
    return absl::OkStatus();
  };

  // Write only the selected layout; inactive tables are not independent
  // storage and may have been repurposed by an expansion.
  for (size_t i = 0; i < count; ++i) {
    RETURN_IF_ERROR(
        write_entrance(i, map_base + i * 2, pos_base + i * 2, id_base + i));
  }

  return absl::OkStatus();
}

absl::Status SaveHoles(Rom* rom, const std::vector<OverworldEntrance>& holes) {
  if (!rom || holes.size() < kNumOverworldHoles) {
    return absl::InvalidArgumentError(
        "Saving holes requires a ROM and all 19 records");
  }
  const size_t size = rom->vector().size();
  if (size < kOverworldHolePos + kNumOverworldHoles * 2 ||
      size < kOverworldHoleArea + kNumOverworldHoles * 2 ||
      size < kOverworldHoleEntrance + kNumOverworldHoles) {
    return absl::OutOfRangeError("ROM does not contain complete hole tables");
  }
  if (size > OverworldCustomASMHasBeenApplied &&
      HasExpandedEntranceTables(*rom,
                                core::RomSettings::Get().address_overrides())) {
    return absl::FailedPreconditionError(
        "Expanded holes must be saved through their entrance table rows");
  }
  for (int i = 0; i < kNumOverworldHoles; ++i) {
    RETURN_IF_ERROR(
        rom->WriteShort(kOverworldHoleArea + (i * 2), holes[i].map_id_));

    // ZeldaFullEditor/Data/Overworld.cs::LoadHoles() adds 0x400 when loading
    // (see lines ~1006-1014). SaveOWEntrances subtracts it before writing
    // (Save.cs lines ~1088-1092). We replicate that here so vanilla ROMs
    // receive the expected values.
    uint16_t rom_map_pos =
        holes[i].deleted ? 0xFFFF
                         : static_cast<uint16_t>(holes[i].map_pos_ >= 0x400
                                                     ? holes[i].map_pos_ - 0x400
                                                     : holes[i].map_pos_);
    RETURN_IF_ERROR(rom->WriteShort(kOverworldHolePos + (i * 2), rom_map_pos));
    RETURN_IF_ERROR(
        rom->WriteByte(kOverworldHoleEntrance + i, holes[i].entrance_id_));
  }

  return absl::OkStatus();
}

}  // namespace yaze::zelda3
