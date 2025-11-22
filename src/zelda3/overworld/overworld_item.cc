#include "zelda3/overworld/overworld_item.h"

#include <algorithm>
#include <cstdint>
#include <vector>

#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "app/rom.h"
#include "app/snes.h"
#include "util/log.h"
#include "util/macro.h"
#include "zelda3/common.h"
#include "zelda3/overworld/overworld_map.h"

namespace yaze::zelda3 {

absl::StatusOr<std::vector<OverworldItem>> LoadItems(
    Rom* rom, std::vector<OverworldMap>& overworld_maps) {
  std::vector<OverworldItem> items;

  // Version 0x03 of the OW ASM added item support for the SW.
  uint8_t asm_version = (*rom)[OverworldCustomASMHasBeenApplied];

  // Determine max number of overworld maps based on actual ASM version
  // Only use expanded maps (0xA0) if v3+ ASM is actually applied
  int max_ow =
      (asm_version >= 0x03 && asm_version != 0xFF) ? kNumOverworldMaps : 0x80;

  ASSIGN_OR_RETURN(uint32_t pointer_snes,
                   rom->ReadLong(zelda3::overworldItemsAddress));
  uint32_t item_pointer_address =
      SnesToPc(pointer_snes);  // 0x1BC2F9 -> 0x0DC2F9

  for (int i = 0; i < max_ow; i++) {
    ASSIGN_OR_RETURN(uint8_t bank_byte,
                     rom->ReadByte(zelda3::overworldItemsAddressBank));
    int bank = bank_byte & 0x7F;

    ASSIGN_OR_RETURN(uint8_t addr_low,
                     rom->ReadByte(item_pointer_address + (i * 2)));
    ASSIGN_OR_RETURN(uint8_t addr_high,
                     rom->ReadByte(item_pointer_address + (i * 2) + 1));

    uint32_t addr = (bank << 16) +      // 1B
                    (addr_high << 8) +  // F9
                    addr_low;           // 3C
    addr = SnesToPc(addr);

    // Check if this is a large map and skip if not the parent
    if (overworld_maps[i].area_size() != zelda3::AreaSizeEnum::SmallArea) {
      if (overworld_maps[i].parent() != (uint8_t)i) {
        continue;
      }
    }

    while (true) {
      ASSIGN_OR_RETURN(uint8_t b1, rom->ReadByte(addr));
      ASSIGN_OR_RETURN(uint8_t b2, rom->ReadByte(addr + 1));
      ASSIGN_OR_RETURN(uint8_t b3, rom->ReadByte(addr + 2));

      if (b1 == 0xFF && b2 == 0xFF) {
        break;
      }

      int p = (((b2 & 0x1F) << 8) + b1) >> 1;

      int x = p % 0x40;  // Use 0x40 instead of 64 to match ZS
      int y = p >> 6;

      int fakeID = i % 0x40;  // Use modulo 0x40 to match ZS

      int sy = fakeID / 8;
      int sx = fakeID - (sy * 8);

      items.emplace_back(b3, (uint16_t)i, (x * 16) + (sx * 512),
                         (y * 16) + (sy * 512), false);
      auto size = items.size();

      items[size - 1].game_x_ = (uint8_t)x;
      items[size - 1].game_y_ = (uint8_t)y;
      addr += 3;
    }
  }
  return items;
}

absl::Status SaveItems(Rom* rom, const std::vector<OverworldItem>& items) {
  const int pointer_count = zelda3::kNumOverworldMaps;

  std::vector<std::vector<OverworldItem>> room_items(pointer_count);

  // Reset bomb door lookup table used by special item (0x86)
  for (int i = 0; i < zelda3::kNumOverworldMaps; ++i) {
    RETURN_IF_ERROR(rom->WriteShort(
        zelda3::kOverworldBombDoorItemLocationsNew + (i * 2), 0x0000));
  }

  for (const OverworldItem& item : items) {
    if (item.deleted)
      continue;

    const int map_index = static_cast<int>(item.room_map_id_);
    if (map_index < 0 || map_index >= pointer_count) {
      LOG_WARN(
          "Overworld::SaveItems",
          "Skipping item with map index %d outside pointer table (size=%d)",
          map_index, pointer_count);
      continue;
    }

    room_items[map_index].push_back(item);

    if (item.id_ == 0x86) {
      const int lookup_index =
          std::min(map_index, zelda3::kNumOverworldMaps - 1);
      RETURN_IF_ERROR(rom->WriteShort(
          zelda3::kOverworldBombDoorItemLocationsNew + (lookup_index * 2),
          static_cast<uint16_t>((item.game_x_ + (item.game_y_ * 64)) * 2)));
    }
  }

  // Prepare pointer reuse cache
  std::vector<int> item_pointers(pointer_count, -1);
  std::vector<int> item_pointers_reuse(pointer_count, -1);

  for (int i = 0; i < pointer_count; ++i) {
    item_pointers_reuse[i] = -1;
    for (int ci = 0; ci < i; ++ci) {
      if (room_items[i].empty()) {
        item_pointers_reuse[i] = -2;  // reuse empty terminator
        break;
      }

      if (CompareItemsArrays(room_items[i], room_items[ci])) {
        item_pointers_reuse[i] = ci;
        break;
      }
    }
  }

  // Item data always lives in the vanilla data block
  int data_pos = zelda3::kOverworldItemsStartDataNew;
  int empty_pointer = -1;

  for (int i = 0; i < pointer_count; ++i) {
    if (item_pointers_reuse[i] == -1) {
      item_pointers[i] = data_pos;
      for (const OverworldItem& item : room_items[i]) {
        const uint16_t map_pos =
            static_cast<uint16_t>(((item.game_y_ << 6) + item.game_x_) << 1);
        const uint32_t data =
            static_cast<uint32_t>(map_pos & 0xFF) |
            (static_cast<uint32_t>((map_pos >> 8) & 0xFF) << 8) |
            (static_cast<uint32_t>(item.id_) << 16);

        RETURN_IF_ERROR(rom->WriteLong(data_pos, data));
        data_pos += 3;
      }

      empty_pointer = data_pos;
      RETURN_IF_ERROR(rom->WriteShort(data_pos, 0xFFFF));
      data_pos += 2;
    } else if (item_pointers_reuse[i] == -2) {
      if (empty_pointer < 0) {
        item_pointers[i] = data_pos;
        empty_pointer = data_pos;
        RETURN_IF_ERROR(rom->WriteShort(data_pos, 0xFFFF));
        data_pos += 2;
      } else {
        item_pointers[i] = empty_pointer;
      }
    } else {
      item_pointers[i] = item_pointers[item_pointers_reuse[i]];
    }
  }

  if (data_pos > kOverworldItemsEndData) {
    return absl::AbortedError("Too many items");
  }

  // Update pointer table metadata to the expanded location used by ZScream
  RETURN_IF_ERROR(rom->WriteLong(zelda3::overworldItemsAddress,
                                 PcToSnes(zelda3::kOverworldItemsPointersNew)));
  RETURN_IF_ERROR(rom->WriteByte(
      zelda3::overworldItemsAddressBank,
      static_cast<uint8_t>(
          (PcToSnes(zelda3::kOverworldItemsStartDataNew) >> 16) & 0xFF)));

  // Clear pointer table (write zero) to avoid stale values when pointer count
  // shrinks
  for (int i = 0; i < zelda3::kNumOverworldMaps; ++i) {
    RETURN_IF_ERROR(
        rom->WriteShort(zelda3::kOverworldItemsPointersNew + (i * 2), 0x0000));
  }

  for (int i = 0; i < pointer_count; ++i) {
    const uint32_t snes_addr = PcToSnes(item_pointers[i]);
    RETURN_IF_ERROR(
        rom->WriteShort(zelda3::kOverworldItemsPointersNew + (i * 2),
                        static_cast<uint16_t>(snes_addr & 0xFFFF)));
  }

  util::logf("End of Items : %d", data_pos);

  return absl::OkStatus();
}

}  // namespace yaze::zelda3