#include "core/dungeon_stream_layout_adapter.h"

#include <cstdint>
#include <string>

#include "absl/status/status.h"
#include "rom/snes.h"
#include "util/macro.h"
#include "zelda3/dungeon/dungeon_rom_addresses.h"

namespace yaze::core {
namespace {

absl::Status ValidateMappedLoRomAddress(uint32_t address,
                                        const char* description) {
  if ((address & 0xFFFFu) < 0x8000u) {
    return absl::InvalidArgumentError(std::string(description) +
                                      " is not a mapped LoROM address");
  }
  return absl::OkStatus();
}

}  // namespace

absl::StatusOr<zelda3::DungeonStreamLayout> ToDungeonStreamAllocatorLayout(
    DungeonStreamType stream_type, const DungeonStreamLayout& manifest_layout) {
  zelda3::DungeonStreamLayout allocator_layout;
  switch (stream_type) {
    case DungeonStreamType::kObjects:
      allocator_layout.kind = zelda3::DungeonStreamKind::kObject;
      break;
    case DungeonStreamType::kSprites:
      allocator_layout.kind = zelda3::DungeonStreamKind::kSprite;
      break;
    case DungeonStreamType::kPotItems:
      allocator_layout.kind = zelda3::DungeonStreamKind::kPotItem;
      break;
    default:
      return absl::InvalidArgumentError("Unknown dungeon stream type");
  }

  RETURN_IF_ERROR(ValidateMappedLoRomAddress(manifest_layout.pointer_table,
                                             "Dungeon pointer table"));
  allocator_layout.pointer_table_pc = SnesToPc(manifest_layout.pointer_table);
  allocator_layout.pointer_count = manifest_layout.pointer_count;

  switch (manifest_layout.pointer_encoding) {
    case DungeonPointerEncoding::kLong24:
      if (manifest_layout.pointer_bank.has_value()) {
        return absl::InvalidArgumentError(
            "long24 dungeon layout must not define pointer_bank");
      }
      allocator_layout.pointer_encoding =
          zelda3::DungeonPointerEncoding::kLong24;
      break;
    case DungeonPointerEncoding::kBank16:
      if (!manifest_layout.pointer_bank.has_value()) {
        return absl::InvalidArgumentError(
            "bank16 dungeon layout requires pointer_bank");
      }
      allocator_layout.pointer_encoding =
          zelda3::DungeonPointerEncoding::kFixedBank16;
      allocator_layout.pointer_bank = *manifest_layout.pointer_bank;
      break;
    default:
      return absl::InvalidArgumentError(
          "Unknown dungeon pointer encoding in manifest");
  }

  allocator_layout.data_ranges.reserve(manifest_layout.data_regions.size());
  for (const SnesAddressRange& range : manifest_layout.data_regions) {
    RETURN_IF_ERROR(
        ValidateMappedLoRomAddress(range.start, "Dungeon data range start"));
    RETURN_IF_ERROR(
        ValidateMappedLoRomAddress(range.end, "Dungeon data range end"));
    const uint32_t begin = SnesToPc(range.start);
    const uint32_t end = SnesToPc(range.end);
    if (begin >= end) {
      return absl::InvalidArgumentError(
          "Dungeon data range must be non-empty in PC space");
    }
    allocator_layout.data_ranges.push_back({begin, end});
  }

  allocator_layout.allocation_ranges.reserve(
      manifest_layout.allocation_regions.size());
  for (const SnesAddressRange& range : manifest_layout.allocation_regions) {
    RETURN_IF_ERROR(ValidateMappedLoRomAddress(
        range.start, "Dungeon allocation range start"));
    RETURN_IF_ERROR(
        ValidateMappedLoRomAddress(range.end, "Dungeon allocation range end"));
    const uint32_t begin = SnesToPc(range.start);
    const uint32_t end = SnesToPc(range.end);
    if (begin >= end) {
      return absl::InvalidArgumentError(
          "Dungeon allocation range must be non-empty in PC space");
    }
    // Room::SaveSprites() still discovers physical stream bounds through the
    // vanilla bank-09 hard end. Keep manifest-backed COW allocations inside
    // that same boundary so a relocated stream remains readable on the next
    // save. Layout-aware sprite discovery can relax this restriction later.
    if (stream_type == DungeonStreamType::kSprites &&
        end > static_cast<uint32_t>(zelda3::kSpritesDataEndExclusive)) {
      return absl::InvalidArgumentError(
          "Sprite allocation range extends beyond the legacy sprite data "
          "boundary");
    }
    allocator_layout.allocation_ranges.push_back({begin, end});
  }

  return allocator_layout;
}

}  // namespace yaze::core
