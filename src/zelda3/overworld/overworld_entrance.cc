#include "zelda3/overworld/overworld_entrance.h"

#include <vector>

#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "app/rom.h"
#include "util/macro.h"

namespace yaze::zelda3 {

absl::StatusOr<std::vector<OverworldEntrance>> LoadEntrances(Rom* rom) {
  std::vector<OverworldEntrance> entrances;
  int ow_entrance_map_ptr = kOverworldEntranceMap;
  int ow_entrance_pos_ptr = kOverworldEntrancePos;
  int ow_entrance_id_ptr = kOverworldEntranceEntranceId;
  int num_entrances = 129;

  // Check if expanded entrance data is actually present in ROM
  // The flag position should contain 0xB8 for vanilla, something else for
  // expanded
  if (rom->data()[kOverworldEntranceExpandedFlagPos] != 0xB8) {
    // ROM has expanded entrance data - use expanded addresses
    ow_entrance_map_ptr = kOverworldEntranceMapExpanded;
    ow_entrance_pos_ptr = kOverworldEntrancePosExpanded;
    ow_entrance_id_ptr = kOverworldEntranceEntranceIdExpanded;
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
    bool deleted = false;
    if (map_pos == 0xFFFF) {
      deleted = true;
    }
    entrances.emplace_back(
        (x * 16) + (((map_id % 64) - (((map_id % 64) / 8) * 8)) * 512),
        (y * 16) + (((map_id % 64) / 8) * 512), entrance_id, map_id, map_pos,
        deleted);
  }

  return entrances;
}

absl::StatusOr<std::vector<OverworldEntrance>> LoadHoles(Rom* rom) {
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
  }
  return holes;
}

absl::Status SaveEntrances(Rom* rom,
                           const std::vector<OverworldEntrance>& entrances,
                           bool expanded_entrances) {
  auto write_entrance = [&](int index, uint32_t map_addr, uint32_t pos_addr,
                            uint32_t id_addr) -> absl::Status {
    // Mirrors ZeldaFullEditor/Save.cs::SaveOWEntrances (see lines ~1081-1085)
    // where MapID and MapPos are written as 16-bit words and EntranceID as a
    // byte.
    RETURN_IF_ERROR(rom->WriteShort(map_addr, entrances[index].map_id_));
    RETURN_IF_ERROR(rom->WriteShort(pos_addr, entrances[index].map_pos_));
    RETURN_IF_ERROR(rom->WriteByte(id_addr, entrances[index].entrance_id_));
    return absl::OkStatus();
  };

  // Always keep the legacy tables in sync for pure vanilla ROMs so e.g. Hyrule
  // Magic expects them. ZScream does the same in SaveOWEntrances.
  for (int i = 0; i < kNumOverworldEntrances; ++i) {
    RETURN_IF_ERROR(write_entrance(i, kOverworldEntranceMap + (i * 2),
                                   kOverworldEntrancePos + (i * 2),
                                   kOverworldEntranceEntranceId + i));
  }

  if (expanded_entrances) {
    // For ZS v3+ ROMs, mirror writes into the expanded tables the way
    // ZeldaFullEditor does when the ASM patch is active.
    for (int i = 0; i < kNumOverworldEntrances; ++i) {
      RETURN_IF_ERROR(write_entrance(i, kOverworldEntranceMapExpanded + (i * 2),
                                     kOverworldEntrancePosExpanded + (i * 2),
                                     kOverworldEntranceEntranceIdExpanded + i));
    }
  }

  return absl::OkStatus();
}

absl::Status SaveHoles(Rom* rom, const std::vector<OverworldEntrance>& holes) {
  for (int i = 0; i < kNumOverworldHoles; ++i) {
    RETURN_IF_ERROR(
        rom->WriteShort(kOverworldHoleArea + (i * 2), holes[i].map_id_));

    // ZeldaFullEditor/Data/Overworld.cs::LoadHoles() adds 0x400 when loading
    // (see lines ~1006-1014). SaveOWEntrances subtracts it before writing
    // (Save.cs lines ~1088-1092). We replicate that here so vanilla ROMs
    // receive the expected values.
    uint16_t rom_map_pos = static_cast<uint16_t>(holes[i].map_pos_ >= 0x400
                                                     ? holes[i].map_pos_ - 0x400
                                                     : holes[i].map_pos_);
    RETURN_IF_ERROR(rom->WriteShort(kOverworldHolePos + (i * 2), rom_map_pos));
    RETURN_IF_ERROR(
        rom->WriteByte(kOverworldHoleEntrance + i, holes[i].entrance_id_));
  }

  return absl::OkStatus();
}

}  // namespace yaze::zelda3
