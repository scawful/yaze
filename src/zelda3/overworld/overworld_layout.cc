#include "zelda3/overworld/overworld_layout.h"

#include <limits>

#include "absl/strings/str_format.h"
#include "core/rom_settings.h"
#include "rom/snes.h"
#include "util/macro.h"
#include "zelda3/common.h"
#include "zelda3/overworld/overworld.h"
#include "zelda3/overworld/overworld_allocation.h"
#include "zelda3/overworld/overworld_entrance.h"
#include "zelda3/overworld/overworld_sprite_io.h"

namespace yaze::zelda3 {
namespace {

constexpr uint32_t kVanillaMap32HighPc = 0x1794D;
constexpr uint32_t kVanillaMap32LowPc = 0x17B2D;
constexpr uint32_t kPoolParentPc = 0x140998;
constexpr int kPoolParentSpan = 0xC0;

bool IsValidPc(const Rom& rom, int pc) {
  return pc >= 0 && static_cast<size_t>(pc) < rom.size();
}

bool IsValidSpan(const Rom& rom, int pc, int bytes) {
  return bytes >= 0 && IsValidPc(rom, pc) &&
         static_cast<size_t>(bytes) <= rom.size() - static_cast<size_t>(pc);
}

bool IsRomSnesAddress(uint32_t address) {
  const uint32_t bank = address >> 16;
  return address <= 0xFFFFFF && (address & 0x8000) != 0 && bank != 0x7E &&
         bank != 0x7F;
}

uint8_t ReadByteOr(const Rom& rom, int pc, uint8_t fallback) {
  return IsValidPc(rom, pc) ? rom.data()[pc] : fallback;
}

uint32_t ReadLongOrZero(const Rom& rom, int pc) {
  if (!IsValidSpan(rom, pc, 3)) {
    return 0;
  }
  return static_cast<uint32_t>(rom.data()[pc]) |
         (static_cast<uint32_t>(rom.data()[pc + 1]) << 8) |
         (static_cast<uint32_t>(rom.data()[pc + 2]) << 16);
}

int ResolvePc(const std::optional<int>& override_pc, int default_pc) {
  return override_pc.has_value() ? *override_pc : default_pc;
}

uint8_t ResolveMagic(const std::optional<uint8_t>& override_magic,
                     uint8_t default_magic) {
  return override_magic.has_value() ? *override_magic : default_magic;
}

int ResolveEntranceFlagPc(const OverworldLayoutOptions& options) {
  return ResolvePc(options.entrance_expanded_flag_pc,
                   kOverworldEntranceExpandedFlagPos);
}

OverworldVersion VersionFromDialect(zscream::LayoutDialect dialect) {
  switch (dialect) {
    case zscream::LayoutDialect::kVanilla:
    case zscream::LayoutDialect::kUnmarked:
      return OverworldVersion::kVanilla;
    case zscream::LayoutDialect::kZSCustomV1:
      return OverworldVersion::kZSCustomV1;
    case zscream::LayoutDialect::kZSCustomV2:
      return OverworldVersion::kZSCustomV2;
    case zscream::LayoutDialect::kZSCustomV3:
      return OverworldVersion::kZSCustomV3;
    case zscream::LayoutDialect::kZScreamV5:
    case zscream::LayoutDialect::kUnknown:
      return OverworldVersion::kVanilla;
  }
  return OverworldVersion::kVanilla;
}

bool DialectSupportsAreaEnum(zscream::LayoutDialect dialect) {
  return dialect == zscream::LayoutDialect::kZSCustomV3;
}

bool DialectSupportsCustomBg(zscream::LayoutDialect dialect) {
  return dialect == zscream::LayoutDialect::kZSCustomV2 ||
         dialect == zscream::LayoutDialect::kZSCustomV3 ||
         dialect == zscream::LayoutDialect::kZScreamV5;
}

bool DialectSupportsExpandedSpace(zscream::LayoutDialect dialect) {
  return dialect == zscream::LayoutDialect::kZSCustomV1 ||
         dialect == zscream::LayoutDialect::kZSCustomV2 ||
         dialect == zscream::LayoutDialect::kZSCustomV3 ||
         dialect == zscream::LayoutDialect::kZScreamV5;
}

bool ValidateV3PoolTablesSpan(const Rom& rom) {
  return IsValidSpan(rom, static_cast<int>(kPoolParentPc), kPoolParentSpan);
}

bool ValidateEaOperands(const Rom& rom, const TailMapExpansionPlan& plan,
                        int high_pc, int low_pc) {
  for (size_t i = 0; i < plan.operand_snes.size(); ++i) {
    const uint32_t pc = SnesToPc(plan.operand_snes[i]);
    if (pc < 1 || !IsValidPc(rom, static_cast<int>(pc + 2))) {
      return false;
    }
    if (rom.data()[pc - 1] != 0xBF) {
      return false;
    }
    const uint32_t current = ReadLongOrZero(rom, static_cast<int>(pc));
    const int expected_pc = (i % 4 < 2 ? high_pc : low_pc) + (i % 2);
    if (!IsRomSnesAddress(current) ||
        SnesToPc(current) != static_cast<uint32_t>(expected_pc)) {
      return false;
    }
  }
  return true;
}

bool ValidateEaPointerTableSpan(const Rom& rom, int high_pc, int low_pc,
                                int entries) {
  const int table_bytes = entries * 3;
  return IsValidSpan(rom, high_pc, table_bytes) &&
         IsValidSpan(rom, low_pc, table_bytes);
}

bool ValidateTerrainPointerStreams(const Rom& rom, int high_pc, int low_pc,
                                   int entries) {
  if (!ValidateEaPointerTableSpan(rom, high_pc, low_pc, entries)) {
    return false;
  }
  for (int index = 0; index < entries; ++index) {
    const uint32_t high_snes = ReadLongOrZero(rom, high_pc + index * 3);
    const uint32_t low_snes = ReadLongOrZero(rom, low_pc + index * 3);
    if (!IsRomSnesAddress(high_snes) || !IsRomSnesAddress(low_snes)) {
      return false;
    }
    const uint32_t high_stream_pc = SnesToPc(high_snes);
    const uint32_t low_stream_pc = SnesToPc(low_snes);
    const MapStreamScan high_scan =
        ScanMapStream(rom.data(), rom.size(), high_stream_pc);
    const MapStreamScan low_scan =
        ScanMapStream(rom.data(), rom.size(), low_stream_pc);
    if (!high_scan.terminated || !low_scan.terminated ||
        !high_scan.error.empty() || !low_scan.error.empty() ||
        high_scan.decoded_bytes != Map32PerScreen ||
        low_scan.decoded_bytes != Map32PerScreen) {
      return false;
    }
  }
  return true;
}

void AppendDiagnostic(OverworldLayout* layout, std::string message) {
  layout->diagnostics.push_back(std::move(message));
}

void SetTableRecord(OverworldLayout* layout, OverworldLayoutDomain domain,
                    int pc, int first_id, int count, int stride,
                    LayoutTableStatus status) {
  OverworldLayoutTableRecord record;
  record.domain = domain;
  record.address_space = LayoutTableAddressSpace::kRomPc;
  record.pc = RomPcOffset(pc);
  record.first_id = first_id;
  record.entry_count = count;
  record.stride_bytes = stride;
  record.status = status;
  layout->table_records.push_back(record);
}

LayoutTableStatus SpanStatus(const Rom& rom, int pc, int bytes) {
  return IsValidSpan(rom, pc, bytes) ? LayoutTableStatus::kValid
                                     : LayoutTableStatus::kTruncated;
}

absl::Status RefusePartialEa(const OverworldLayout& layout,
                             OverworldLayoutOperation operation) {
  if ((layout.ea_marker_byte_present || layout.ea_loader_operands_present) &&
      !layout.has_validated_ea_tail() &&
      operation != OverworldLayoutOperation::kInspect) {
    return absl::FailedPreconditionError(
        "EA tail marker or loader state is incomplete; refusing terrain "
        "mutation/serialization on legacy pointer tables");
  }
  return absl::OkStatus();
}

absl::Status RefuseDomainTable(const OverworldLayout& layout,
                               OverworldLayoutDomain domain) {
  bool found = false;
  for (const auto& record : layout.table_records) {
    if (record.domain != domain) {
      continue;
    }
    found = true;
    if (record.status == LayoutTableStatus::kUnavailable) {
      return absl::FailedPreconditionError(
          "Required overworld table is unavailable on this layout");
    }
    if (record.status == LayoutTableStatus::kTruncated) {
      return absl::OutOfRangeError(
          "Required overworld table span is truncated in ROM");
    }
  }
  if (!found) {
    return absl::FailedPreconditionError(absl::StrFormat(
        "No layout table record for domain %d", static_cast<int>(domain)));
  }
  return absl::OkStatus();
}

}  // namespace

OverworldLayoutOptions OverworldLayoutOptions::Defaults() {
  return OverworldLayoutOptions{};
}

int OverworldLayoutOptions::Address(const std::string& key,
                                    int fallback) const {
  const auto value = address_overrides.GetAddress(key);
  if (!value)
    return fallback;
  return *value <= static_cast<uint32_t>(std::numeric_limits<int>::max())
             ? static_cast<int>(*value)
             : -1;
}

OverworldLayoutOptions OverworldLayoutOptions::WithProjectRomSettings() {
  OverworldLayoutOptions options;
  const auto& settings = core::RomSettings::Get();
  options.address_overrides = settings.address_overrides();
  options.expanded_ptr_marker_pc = static_cast<int>(
      settings.GetAddressOr(core::RomAddressKey::kOverworldExpandedPtrMarker,
                            kExpandedPtrTableMarker));
  options.expanded_ptr_magic = static_cast<uint8_t>(settings.GetAddressOr(
      core::RomAddressKey::kOverworldExpandedPtrMagic, kExpandedPtrTableMagic));
  options.expanded_ptr_high_pc = static_cast<int>(settings.GetAddressOr(
      core::RomAddressKey::kOverworldExpandedPtrHigh, kExpandedPtrTableHigh));
  options.expanded_ptr_low_pc = static_cast<int>(settings.GetAddressOr(
      core::RomAddressKey::kOverworldExpandedPtrLow, kExpandedPtrTableLow));
  options.map16_expanded_pc = static_cast<int>(settings.GetAddressOr(
      core::RomAddressKey::kOverworldMap16Expanded, kMap16TilesExpanded));
  options.map32_tr_expanded_pc = static_cast<int>(settings.GetAddressOr(
      core::RomAddressKey::kOverworldMap32TrExpanded, kMap32TileTRExpanded));
  options.map32_bl_expanded_pc = static_cast<int>(settings.GetAddressOr(
      core::RomAddressKey::kOverworldMap32BlExpanded, kMap32TileBLExpanded));
  options.map32_br_expanded_pc = static_cast<int>(settings.GetAddressOr(
      core::RomAddressKey::kOverworldMap32BrExpanded, kMap32TileBRExpanded));
  options.entrance_expanded_flag_pc = static_cast<int>(
      settings.GetAddressOr(core::RomAddressKey::kOverworldEntranceFlagExpanded,
                            kOverworldEntranceExpandedFlagPos));
  return options;
}

bool OverworldLayout::uses_zs_v3_catalogue() const {
  if (zscream_layout == zscream::LayoutDialect::kZSCustomV3) {
    return true;
  }
  if (zscream_layout == zscream::LayoutDialect::kZScreamV5 &&
      v3_pool_codec_valid) {
    return true;
  }
  return false;
}

const OverworldLayoutTableRecord* OverworldLayout::FindTableRecord(
    OverworldLayoutDomain domain) const {
  for (const auto& record : table_records) {
    if (record.domain == domain) {
      return &record;
    }
  }
  return nullptr;
}

uint8_t OverworldLayout::catalogue_asm_version() const {
  if (uses_zs_v3_catalogue()) {
    return 0x03;
  }
  switch (zscream_layout) {
    case zscream::LayoutDialect::kZSCustomV2:
      return 0x02;
    case zscream::LayoutDialect::kZSCustomV1:
      return 0x01;
    default:
      return 0xFF;
  }
}

absl::Status OverworldLayout::CheckOperation(OverworldLayoutDomain domain,
                                             OverworldLayoutOperation operation,
                                             int map_id, int phase) const {
  if (map_id < -1 || map_id >= 0xC0) {
    return absl::FailedPreconditionError(absl::StrFormat(
        "Map 0x%02X is outside supported overworld IDs", map_id));
  }

  if (lyra_layout.presence == rom::LyraLayoutPresence::kInvalid) {
    return absl::FailedPreconditionError(
        lyra_layout.error.empty() ? "Invalid Lyra overworld descriptor"
                                  : lyra_layout.error);
  }
  if (lyra_layout.presence == rom::LyraLayoutPresence::kValid) {
    if (domain == OverworldLayoutDomain::kLyraDescriptor &&
        operation == OverworldLayoutOperation::kInspect) {
      return absl::OkStatus();
    }
    if (domain == OverworldLayoutDomain::kTerrainPointers &&
        operation == OverworldLayoutOperation::kSerialize &&
        lyra_layout.a0_terrain_write_supported && map_id == 0xA0) {
      return absl::OkStatus();
    }
    if (operation != OverworldLayoutOperation::kInspect) {
      return absl::FailedPreconditionError(
          "Lyra descriptor overworld layout is inspection-only for this "
          "operation");
    }
    if (domain == OverworldLayoutDomain::kTerrainPointers) {
      return absl::OkStatus();
    }
    if (domain == OverworldLayoutDomain::kTile16 ||
        domain == OverworldLayoutDomain::kTile32) {
      return RefuseDomainTable(*this, domain);
    }
    return absl::FailedPreconditionError(
        "Descriptor preview does not provide this overworld domain");
  }

  const int map_limit =
      operation == OverworldLayoutOperation::kInspect &&
              domain == OverworldLayoutDomain::kTerrainPointers
          ? storage_slot_capacity
          : materialized_model_slots;
  if (map_id >= 0 && map_id >= map_limit) {
    return absl::FailedPreconditionError(
        absl::StrFormat("Map 0x%02X is outside the materialized %d-map model",
                        map_id, materialized_model_slots));
  }

  if (zscream_layout == zscream::LayoutDialect::kUnknown &&
      operation != OverworldLayoutOperation::kInspect) {
    return absl::FailedPreconditionError(
        "Unknown custom overworld marker; refusing serialization");
  }

  if (domain == OverworldLayoutDomain::kCollisions) {
    if (zscream_layout == zscream::LayoutDialect::kZScreamV5) {
      if (operation == OverworldLayoutOperation::kSerialize) {
        return absl::FailedPreconditionError(
            "ZScream V5 per-sheet collision writes are not supported");
      }
      if (operation == OverworldLayoutOperation::kInspect &&
          !v5_collision_table_valid) {
        return absl::FailedPreconditionError(
            "ZScream V5 per-sheet collision table is truncated");
      }
      return RefuseDomainTable(*this, domain);
    }
    return RefuseDomainTable(*this, domain);
  }

  if (domain == OverworldLayoutDomain::kTerrainPointers ||
      domain == OverworldLayoutDomain::kTile16 ||
      domain == OverworldLayoutDomain::kTile32) {
    RETURN_IF_ERROR(RefusePartialEa(*this, operation));
    if (operation != OverworldLayoutOperation::kInspect &&
        has_validated_ea_tail() && !integrated_ea_terrain_writer) {
      return absl::FailedPreconditionError(
          "EA tail-map terrain serialization requires the integrated relocated "
          "writer; refusing legacy pointer-table writes");
    }
    return RefuseDomainTable(*this, domain);
  }

  if (domain == OverworldLayoutDomain::kMapProperties &&
      operation == OverworldLayoutOperation::kMutateModel && map_id >= 0) {
    if (map_id >= kSpecialWorldMapIdStart + special_storage_slots &&
        map_id < kSpecialWorldMapIdStart + 0x40) {
      return absl::FailedPreconditionError(absl::StrFormat(
          "Special map 0x%02X is outside the detected %d-slot special storage",
          map_id, special_storage_slots));
    }
  }

  if (domain == OverworldLayoutDomain::kSprites) {
    if (phase < 0 || phase > 2) {
      return absl::InvalidArgumentError("Sprite phase must be 0, 1, or 2");
    }
    const auto sprite_layout = ResolveOverworldSpriteLayout(*this);
    if (map_id >= 0 && map_id >= sprite_layout.counts[phase]) {
      return absl::FailedPreconditionError(
          absl::StrFormat("Sprite phase %d supports maps 0..%d on this layout",
                          phase, sprite_layout.counts[phase] - 1));
    }
  }

  switch (domain) {
    case OverworldLayoutDomain::kTile16:
    case OverworldLayoutDomain::kTile32:
    case OverworldLayoutDomain::kMapProperties:
    case OverworldLayoutDomain::kSprites:
    case OverworldLayoutDomain::kMusic:
    case OverworldLayoutDomain::kMessages:
    case OverworldLayoutDomain::kEntities:
    case OverworldLayoutDomain::kGeometry:
      return RefuseDomainTable(*this, domain);
    default:
      break;
  }

  return absl::OkStatus();
}

OverworldLayout InspectOverworldLayout(const Rom& rom,
                                       const OverworldLayoutOptions& options) {
  OverworldLayout layout;
  layout.zscream_layout = zscream::DetectLayout(rom);
  layout.lyra_layout = rom.InspectLyraLayout();
  layout.asm_version = OverworldVersionHelper::GetAsmVersion(rom);
  layout.version = VersionFromDialect(layout.zscream_layout);
  layout.is_vanilla =
      layout.zscream_layout == zscream::LayoutDialect::kVanilla ||
      layout.zscream_layout == zscream::LayoutDialect::kUnmarked;
  layout.v3_pool_codec_valid =
      layout.zscream_layout == zscream::LayoutDialect::kZSCustomV3 ||
      (layout.zscream_layout == zscream::LayoutDialect::kZScreamV5 &&
       ValidateV3PoolTablesSpan(rom));

  if (layout.zscream_layout == zscream::LayoutDialect::kZScreamV5 &&
      layout.v3_pool_codec_valid) {
    layout.version = OverworldVersion::kZSCustomV3;
    layout.is_vanilla = false;
  }

  layout.supports_expanded_space =
      DialectSupportsExpandedSpace(layout.zscream_layout);
  layout.supports_area_enum = DialectSupportsAreaEnum(layout.zscream_layout);
  if (layout.v3_pool_codec_valid)
    layout.supports_area_enum = true;
  layout.supports_custom_bg_colors =
      DialectSupportsCustomBg(layout.zscream_layout);

  const int entrance_flag_pc = ResolveEntranceFlagPc(options);
  const uint8_t tile16_flag = ReadByteOr(rom, kMap16ExpandedFlagPos, 0x0F);
  const uint8_t tile32_flag = ReadByteOr(rom, kMap32ExpandedFlagPos, 0x04);
  const uint8_t entrance_flag = ReadByteOr(rom, entrance_flag_pc, 0xB8);

  layout.has_expanded_tile16 =
      !layout.is_vanilla && (layout.supports_area_enum || tile16_flag != 0x0F);
  layout.has_expanded_tile32 =
      !layout.is_vanilla && (layout.supports_area_enum || tile32_flag != 0x04);
  layout.has_expanded_entrances = !layout.is_vanilla && entrance_flag != 0xB8;

  layout.materialized_model_slots = kNumOverworldMaps;
  layout.special_storage_slots = 0x20;
  layout.storage_slot_capacity = kNumOverworldMaps;
  layout.map_storage = MapSlotStorage::kStandard160;
  layout.terrain_table_entries = kNumOverworldMaps;
  layout.terrain_pointer_source = TerrainPointerSource::kVanilla160EntryTables;
  layout.terrain_high_table_pc =
      RomPcOffset(static_cast<int>(kVanillaMap32HighPc));
  layout.terrain_low_table_pc =
      RomPcOffset(static_cast<int>(kVanillaMap32LowPc));

  const int map16_pc =
      ResolvePc(options.map16_expanded_pc, kMap16TilesExpanded);
  const int tile16_bytes = layout.has_expanded_tile16
                               ? NumberOfMap16Ex * 8
                               : kNumTile16Individual * 8;
  const int tile16_pc = layout.has_expanded_tile16 ? map16_pc : kMap16Tiles;
  layout.tile16_pc = RomPcOffset(tile16_pc);
  SetTableRecord(
      &layout, OverworldLayoutDomain::kTile16, tile16_pc, 0,
      layout.has_expanded_tile16 ? NumberOfMap16Ex : kNumTile16Individual, 8,
      SpanStatus(rom, tile16_pc, tile16_bytes));

  const int map32_tr =
      ResolvePc(options.map32_tr_expanded_pc, kMap32TileTRExpanded);
  const int map32_bl =
      ResolvePc(options.map32_bl_expanded_pc, kMap32TileBLExpanded);
  const int map32_br =
      ResolvePc(options.map32_br_expanded_pc, kMap32TileBRExpanded);
  const int map32_bytes = layout.has_expanded_tile32
                              ? kMap32TileStorageBytesExpanded
                              : kMap32TileStorageBytesVanilla;
  const auto& constants = kVersionConstantsMap.at(options.rom_version);
  layout.tile32_pcs = {
      RomPcOffset(constants.kMap32TileTL),
      RomPcOffset(layout.has_expanded_tile32 ? map32_tr
                                             : constants.kMap32TileTR),
      RomPcOffset(layout.has_expanded_tile32 ? map32_bl
                                             : constants.kMap32TileBL),
      RomPcOffset(layout.has_expanded_tile32 ? map32_br
                                             : constants.kMap32TileBR)};
  for (const auto pc : layout.tile32_pcs) {
    SetTableRecord(&layout, OverworldLayoutDomain::kTile32, pc.value, 0,
                   map32_bytes / kMap32BytesPerPackedGroup, 6,
                   SpanStatus(rom, pc.value, map32_bytes));
  }

  if (layout.lyra_layout.presence != rom::LyraLayoutPresence::kAbsent) {
    layout.layout_kind = OverworldLayout::LayoutKind::kLyraOffline;
    layout.map_storage = MapSlotStorage::kExpanded192;
    layout.storage_slot_capacity = layout.lyra_layout.capacity;
    layout.materialized_model_slots = layout.lyra_layout.capacity;
    layout.special_storage_slots = 0x40;
    layout.descriptor_active_count = layout.lyra_layout.active_count;
    layout.terrain_pointer_source =
        TerrainPointerSource::kLyraDescriptorStreams;
    if (layout.lyra_layout.presence == rom::LyraLayoutPresence::kInvalid) {
      AppendDiagnostic(&layout, layout.lyra_layout.error);
    }
    return layout;
  }

  if (layout.zscream_layout == zscream::LayoutDialect::kZScreamV5) {
    layout.layout_kind = OverworldLayout::LayoutKind::kZScreamPerSheet;
    layout.v5_collision_table_valid =
        zscream::ValidateSheetCollisionTable(rom).ok();
    if (!layout.v5_collision_table_valid) {
      AppendDiagnostic(&layout, "ZScream V5 collision table truncated");
    }
  } else if (layout.zscream_layout == zscream::LayoutDialect::kUnknown) {
    layout.layout_kind = OverworldLayout::LayoutKind::kUnknownCustom;
    AppendDiagnostic(&layout, absl::StrFormat("Unknown ZScream marker 0x%02X",
                                              layout.asm_version));
  }

  const int marker_pc =
      ResolvePc(options.expanded_ptr_marker_pc, kExpandedPtrTableMarker);
  const uint8_t marker_magic =
      ResolveMagic(options.expanded_ptr_magic, kExpandedPtrTableMagic);
  const int high_pc =
      ResolvePc(options.expanded_ptr_high_pc, kExpandedPtrTableHigh);
  const int low_pc =
      ResolvePc(options.expanded_ptr_low_pc, kExpandedPtrTableLow);

  layout.ea_marker_byte_present =
      layout.supports_area_enum && zscream::CanUseTailExpansion(rom) &&
      ReadByteOr(rom, marker_pc, 0x00) == marker_magic;

  if (layout.ea_marker_byte_present && !layout.supports_area_enum) {
    AppendDiagnostic(&layout,
                     "EA tail marker present but layout dialect is not "
                     "ZSCustomOverworld V3");
    layout.ea_marker_byte_present = false;
  }

  const TailMapExpansionPlan plan;
  // Loader operands remain authoritative when the marker is removed. Do not
  // revert to legacy writes while any loader still reads the relocated tables.
  for (size_t i = 0; i < plan.operand_snes.size(); ++i) {
    const uint32_t current =
        ReadLongOrZero(rom, static_cast<int>(SnesToPc(plan.operand_snes[i])));
    const int64_t expected =
        static_cast<int64_t>(i % 4 < 2 ? high_pc : low_pc) + (i % 2);
    if (IsRomSnesAddress(current) &&
        (static_cast<int64_t>(SnesToPc(current)) == expected ||
         SnesToPc(current) == SnesToPc(plan.operand_patched[i]))) {
      layout.ea_loader_operands_present = true;
    }
  }
  layout.ea_tail_tables_in_rom =
      layout.ea_marker_byte_present &&
      ValidateEaPointerTableSpan(rom, high_pc, low_pc, kExpandedMapCount);
  layout.ea_tail_operands_validated =
      layout.ea_tail_tables_in_rom &&
      ValidateEaOperands(rom, plan, high_pc, low_pc);
  layout.ea_tail_pointer_streams_valid =
      layout.ea_tail_operands_validated && layout.ea_tail_tables_in_rom &&
      ValidateTerrainPointerStreams(rom, high_pc, low_pc, kExpandedMapCount);

  if (layout.ea_marker_byte_present && !layout.ea_tail_operands_validated) {
    AppendDiagnostic(&layout,
                     "EA tail marker without validated loader operands");
  }
  if (!layout.ea_marker_byte_present && layout.ea_loader_operands_present) {
    AppendDiagnostic(&layout,
                     "Relocated map loaders remain without the EA marker");
  }
  if (layout.ea_marker_byte_present && !layout.ea_tail_tables_in_rom) {
    AppendDiagnostic(&layout, "EA relocated pointer tables truncated in ROM");
  }
  if (layout.ea_marker_byte_present && layout.ea_tail_tables_in_rom &&
      !layout.ea_tail_pointer_streams_valid) {
    AppendDiagnostic(&layout,
                     "EA relocated pointer streams failed ScanMapStream "
                     "validation");
  }

  if (layout.has_validated_ea_tail()) {
    layout.layout_kind = OverworldLayout::LayoutKind::kOlderTailEa;
    layout.map_storage = MapSlotStorage::kExpanded192;
    layout.storage_slot_capacity = kExpandedMapCount;
    layout.special_storage_slots = 0x40;
    layout.terrain_table_entries = kExpandedMapCount;
    layout.terrain_pointer_source =
        TerrainPointerSource::kEaRelocated192EntryTables;
    layout.terrain_high_table_pc = RomPcOffset(high_pc);
    layout.terrain_low_table_pc = RomPcOffset(low_pc);
  }

  layout.tail_marker_present = layout.has_validated_ea_tail();
  if (layout.ea_marker_byte_present || layout.ea_loader_operands_present) {
    // Family attribution is distinct from validated capacity and write support.
    layout.layout_kind = OverworldLayout::LayoutKind::kOlderTailEa;
  }

  const int terrain_entries =
      layout.has_validated_ea_tail() ? kExpandedMapCount : kNumOverworldMaps;
  const LayoutTableStatus terrain_status =
      (layout.ea_marker_byte_present || layout.ea_loader_operands_present) &&
              !layout.has_validated_ea_tail()
          ? LayoutTableStatus::kTruncated
          : (ValidateEaPointerTableSpan(rom, layout.terrain_high_table_pc.value,
                                        layout.terrain_low_table_pc.value,
                                        terrain_entries) &&
                     (!layout.has_validated_ea_tail() ||
                      layout.ea_tail_pointer_streams_valid)
                 ? LayoutTableStatus::kValid
                 : LayoutTableStatus::kTruncated);
  SetTableRecord(&layout, OverworldLayoutDomain::kTerrainPointers,
                 layout.terrain_high_table_pc.value, 0, terrain_entries, 3,
                 terrain_status);

  const auto sprite_layout = ResolveOverworldSpriteLayout(layout);
  for (int phase = 0; phase < 3; ++phase) {
    SetTableRecord(
        &layout, OverworldLayoutDomain::kSprites,
        static_cast<int>(sprite_layout.tables[phase]), 0,
        sprite_layout.counts[phase], 2,
        SpanStatus(rom, static_cast<int>(sprite_layout.tables[phase]),
                   sprite_layout.counts[phase] * 2));
  }

  if (layout.zscream_layout == zscream::LayoutDialect::kZScreamV5) {
    SetTableRecord(&layout, OverworldLayoutDomain::kCollisions,
                   static_cast<int>(zscream::kSheetCollisionTablePc), 0,
                   static_cast<int>(zscream::kCollisionSheetCount),
                   static_cast<int>(zscream::kCollisionsPerSheet),
                   layout.v5_collision_table_valid
                       ? LayoutTableStatus::kValid
                       : LayoutTableStatus::kTruncated);
  } else {
    SetTableRecord(&layout, OverworldLayoutDomain::kCollisions,
                   overworldTilesType, 0, kNumTileTypes, 1,
                   SpanStatus(rom, overworldTilesType, kNumTileTypes));
  }

  for (const auto& table :
       OverworldAreaTableCatalogue(layout.catalogue_asm_version())) {
    if (!table.in_rom)
      continue;
    std::optional<OverworldLayoutDomain> domain;
    if (table.group == "gfx")
      domain = OverworldLayoutDomain::kMapProperties;
    else if (table.group == "transition" || table.id == "pool_parent" ||
             table.id == "parent" || table.id == "area_size")
      domain = OverworldLayoutDomain::kGeometry;
    else if (table.group == "music")
      domain = OverworldLayoutDomain::kMusic;
    else if (table.group == "text")
      domain = OverworldLayoutDomain::kMessages;
    else if (table.group == "items")
      domain = OverworldLayoutDomain::kEntities;
    if (!domain)
      continue;
    int pc = static_cast<int>(table.pc);
    if (table.id == "pool_parent")
      pc =
          options.Address(core::RomAddressKey::kOverworldMapParentExpanded, pc);
    else if (table.id == "pool_sign_message")
      pc = options.Address(core::RomAddressKey::kOverworldMessagesExpanded, pc);
    else if (table.id == "pool_by_screen_1")
      pc = options.Address(core::RomAddressKey::kOverworldScreenChange1Expanded,
                           pc);
    else if (table.id == "pool_by_screen_2")
      pc = options.Address(core::RomAddressKey::kOverworldScreenChange2Expanded,
                           pc);
    else if (table.id == "pool_by_screen_3")
      pc = options.Address(core::RomAddressKey::kOverworldScreenChange3Expanded,
                           pc);
    else if (table.id == "pool_by_screen_4")
      pc = options.Address(core::RomAddressKey::kOverworldScreenChange4Expanded,
                           pc);
    else if (table.id == "pool_trans_pos_x")
      pc = options.Address(
          core::RomAddressKey::kOverworldTransitionPosXExpanded, pc);
    else if (table.id == "pool_trans_pos_y")
      pc = options.Address(
          core::RomAddressKey::kOverworldTransitionPosYExpanded, pc);
    const int count = table.last_area - table.first_area + 1;
    SetTableRecord(&layout, *domain, pc, table.first_area, count,
                   table.entry_bytes,
                   SpanStatus(rom, pc, count * table.entry_bytes));
    if (table.id == "music_lw") {
      for (int phase = 1; phase < 4; ++phase)
        SetTableRecord(&layout, *domain, pc + phase * count, 0, count, 1,
                       SpanStatus(rom, pc + phase * count, count));
    }
  }
  const int entrance_count = layout.has_expanded_entrances ? 256 : 129;
  const std::array<int, 3> entrance_tables =
      layout.has_expanded_entrances
          ? std::array<
                int, 3>{options.Address(
                            core::RomAddressKey::kOverworldEntranceMapExpanded,
                            kOverworldEntranceMapExpanded),
                        options.Address(
                            core::RomAddressKey::kOverworldEntrancePosExpanded,
                            kOverworldEntrancePosExpanded),
                        options.Address(
                            core::RomAddressKey::kOverworldEntranceIdExpanded,
                            kOverworldEntranceEntranceIdExpanded)}
          : std::array<int, 3>{kOverworldEntranceMap, kOverworldEntrancePos,
                               kOverworldEntranceEntranceId};
  for (int i = 0; i < 3; ++i) {
    const int stride = i == 2 ? 1 : 2;
    SetTableRecord(
        &layout, OverworldLayoutDomain::kEntities, entrance_tables[i], 0,
        entrance_count, stride,
        SpanStatus(rom, entrance_tables[i], entrance_count * stride));
  }
  for (const int pc : {kOverworldSpriteset, kOverworldSpritePaletteIds}) {
    SetTableRecord(&layout, OverworldLayoutDomain::kMapProperties, pc, 0, 256,
                   1, SpanStatus(rom, pc, 256));
  }

  if (layout.zscream_layout == zscream::LayoutDialect::kUnknown) {
    for (auto& record : layout.table_records) {
      if (record.domain != OverworldLayoutDomain::kCollisions &&
          record.domain != OverworldLayoutDomain::kLyraDescriptor) {
        record.status = LayoutTableStatus::kUnavailable;
      }
    }
  }

  return layout;
}

}  // namespace yaze::zelda3
