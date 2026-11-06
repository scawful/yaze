#ifndef YAZE_ZELDA3_OVERWORLD_OVERWORLD_LAYOUT_H_
#define YAZE_ZELDA3_OVERWORLD_OVERWORLD_LAYOUT_H_

#include <array>
#include <cstdint>
#include <optional>
#include <string>
#include <vector>

#include "absl/status/status.h"
#include "core/rom_settings.h"
#include "rom/lyra_layout_descriptor.h"
#include "rom/rom.h"
#include "zelda.h"
#include "zelda3/overworld/overworld_version_helper.h"
#include "zelda3/overworld/zscream_custom_layout.h"

namespace yaze::zelda3 {

struct RomPcOffset {
  int value = -1;
  RomPcOffset() = default;
  explicit RomPcOffset(int pc) : value(pc) {}
};

struct SnesLoRomAddress {
  uint32_t value = 0;
  SnesLoRomAddress() = default;
  explicit SnesLoRomAddress(uint32_t snes) : value(snes) {}
};

enum class OverworldLayoutDomain {
  kTerrainPointers,
  kTile16,
  kTile32,
  kMapProperties,
  kSprites,
  kMusic,
  kMessages,
  kEntities,
  kGeometry,
  kCollisions,
  kLyraDescriptor,
};

enum class OverworldLayoutOperation {
  kInspect,
  kMutateModel,
  kSerialize,
};

enum class MapSlotStorage {
  kStandard160,
  kExpanded192,
};

enum class TerrainPointerSource {
  kVanilla160EntryTables,
  kEaRelocated192EntryTables,
  kLyraDescriptorStreams,
};

enum class LayoutTableAddressSpace {
  kRomPc,
  kSnesLoRom,
  kSram,
};

enum class LayoutTableStatus {
  kValid,
  kTruncated,
  kUnavailable,
};

struct OverworldLayoutTableRecord {
  OverworldLayoutDomain domain = OverworldLayoutDomain::kTerrainPointers;
  LayoutTableAddressSpace address_space = LayoutTableAddressSpace::kRomPc;
  RomPcOffset pc;
  int first_id = 0;
  int entry_count = 0;
  int stride_bytes = 0;
  LayoutTableStatus status = LayoutTableStatus::kUnavailable;
};

struct OverworldLayoutOptions {
  core::RomAddressOverrides address_overrides;
  zelda3_version rom_version = zelda3_version::US;
  std::optional<int> expanded_ptr_marker_pc;
  std::optional<uint8_t> expanded_ptr_magic;
  std::optional<int> expanded_ptr_high_pc;
  std::optional<int> expanded_ptr_low_pc;
  std::optional<int> map16_expanded_pc;
  std::optional<int> map32_tr_expanded_pc;
  std::optional<int> map32_bl_expanded_pc;
  std::optional<int> map32_br_expanded_pc;
  std::optional<int> entrance_expanded_flag_pc;

  static OverworldLayoutOptions Defaults();
  static OverworldLayoutOptions WithProjectRomSettings();
  int Address(const std::string& key, int fallback) const;
};

struct OverworldLayout {
  enum class LayoutKind {
    kLegacy,
    kOlderTailEa,
    kLyraOffline,
    kZScreamPerSheet,
    kUnknownCustom,
  };

  LayoutKind layout_kind = LayoutKind::kLegacy;
  zscream::LayoutDialect zscream_layout = zscream::LayoutDialect::kUnmarked;
  rom::LyraLayoutInspection lyra_layout;
  uint8_t asm_version = 0xFF;
  OverworldVersion version = OverworldVersion::kVanilla;
  bool is_vanilla = true;
  bool supports_expanded_space = false;
  bool supports_area_enum = false;
  bool supports_custom_bg_colors = false;
  bool has_expanded_tile16 = false;
  bool has_expanded_tile32 = false;
  bool has_expanded_entrances = false;
  bool v3_pool_codec_valid = false;

  MapSlotStorage map_storage = MapSlotStorage::kStandard160;
  int storage_slot_capacity = 160;
  int materialized_model_slots = 160;
  int special_storage_slots = 32;
  std::optional<uint16_t> descriptor_active_count;

  bool ea_marker_byte_present = false;
  bool ea_loader_operands_present = false;
  bool tail_marker_present = false;
  bool ea_tail_operands_validated = false;
  bool ea_tail_tables_in_rom = false;
  bool ea_tail_pointer_streams_valid = false;
  TerrainPointerSource terrain_pointer_source =
      TerrainPointerSource::kVanilla160EntryTables;
  RomPcOffset terrain_high_table_pc;
  RomPcOffset terrain_low_table_pc;
  int terrain_table_entries = 160;
  RomPcOffset tile16_pc;
  std::array<RomPcOffset, 4> tile32_pcs;

  bool v5_collision_table_valid = false;
  bool integrated_ea_terrain_writer = false;

  std::vector<OverworldLayoutTableRecord> table_records;
  std::vector<std::string> diagnostics;

  bool has_validated_ea_tail() const {
    return ea_tail_operands_validated && ea_tail_tables_in_rom &&
           ea_tail_pointer_streams_valid;
  }

  bool uses_zs_v3_catalogue() const;
  const OverworldLayoutTableRecord* FindTableRecord(
      OverworldLayoutDomain domain) const;

  absl::Status CheckOperation(OverworldLayoutDomain domain,
                              OverworldLayoutOperation operation, int map_id,
                              int phase) const;

  uint8_t catalogue_asm_version() const;
};

OverworldLayout InspectOverworldLayout(const Rom& rom,
                                       const OverworldLayoutOptions& options);

}  // namespace yaze::zelda3

#endif  // YAZE_ZELDA3_OVERWORLD_OVERWORLD_LAYOUT_H_
