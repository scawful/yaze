#include "zelda3/overworld/overworld_upgrade_system.h"

#include <algorithm>
#include <array>
#include <exception>
#include <filesystem>
#include <string>
#include <utility>
#include <vector>

#include "absl/strings/str_format.h"
#include "core/asar_wrapper.h"
#include "rom/rom.h"
#include "rom/snes.h"
#include "rom/transaction.h"
#include "util/file_util.h"
#include "util/macro.h"
#include "zelda3/overworld/overworld.h"
#include "zelda3/overworld/overworld_geometry.h"
#include "zelda3/overworld/overworld_item.h"
#include "zelda3/overworld/overworld_sprite_io.h"

namespace yaze::zelda3 {
namespace {

constexpr int kPoolBegin = OverworldCustomAreaSpecificBGPalette;
constexpr int kPoolEnd = kOverworldMapParentIdExpanded;
constexpr int kLegacyNorthTarget = 0x13EE2;
constexpr int kLegacyWestTarget = 0x13F62;
constexpr int kLegacyBombDoorTable = 0x16DC5;
constexpr std::array<int, 8> kLegacyGeometryTables = {
    kOverworldScreenTileMapChangeByScreen1,
    kOverworldScreenTileMapChangeByScreen2,
    kOverworldScreenTileMapChangeByScreen3,
    kOverworldScreenTileMapChangeByScreen4,
    kOverworldTransitionPositionY,
    kOverworldTransitionPositionX,
    kLegacyNorthTarget,
    kLegacyWestTarget};
constexpr std::array<int, 8> kExpandedGeometryTables = {
    kOverworldScreenTileMapChangeByScreen1Expanded,
    kOverworldScreenTileMapChangeByScreen2Expanded,
    kOverworldScreenTileMapChangeByScreen3Expanded,
    kOverworldScreenTileMapChangeByScreen4Expanded,
    kOverworldTransitionPositionYExpanded,
    kOverworldTransitionPositionXExpanded,
    transition_target_northExpanded,
    transition_target_westExpanded};
constexpr std::array<int, 3> kExpandedSpriteTables = {0x141438, 0x141578,
                                                      0x1416B8};

// These are the existing ZSCustom tables, not a new serialized upgrade format.
struct UpgradeData {
  bool custom_v2 = false;
  std::vector<OverworldGeometryRow> geometry;
  std::vector<uint8_t> pool;
  std::array<uint8_t, kNumOverworldMaps> graphics{};
  std::array<uint8_t, kNumOverworldMaps> palettes{};
  std::array<uint8_t, 32> special_sprite_graphics{};
  std::array<uint8_t, 32> special_sprite_palettes{};
  std::array<std::array<uint16_t, kNumOverworldMaps>, 3> sprites{};
  std::array<uint16_t, kNumOverworldMaps> messages{};
  std::array<uint16_t, kNumOverworldMaps> item_pointers{};
  std::array<uint16_t, kNumOverworldMaps> bomb_door_locations{};
};

absl::Status RequireSpan(const Rom& rom, int pc, int bytes) {
  if (pc < 0 || bytes < 0 || static_cast<size_t>(pc) > rom.size() ||
      static_cast<size_t>(bytes) > rom.size() - pc)
    return absl::OutOfRangeError(absl::StrFormat(
        "Incomplete upgrade source table at PC 0x%06X (%d bytes)", pc, bytes));
  return absl::OkStatus();
}

uint16_t Word(const std::vector<uint8_t>& bytes, int pc) {
  return bytes[pc] | (bytes[pc + 1] << 8);
}
void PutWord(std::vector<uint8_t>& bytes, int pc, uint16_t value) {
  bytes[pc] = value & 0xFF;
  bytes[pc + 1] = value >> 8;
}

absl::StatusOr<UpgradeData> CaptureUpgradeData(const Rom& rom) {
  UpgradeData result;
  const auto dialect = zscream::DetectLayout(rom);
  result.custom_v2 = dialect == zscream::LayoutDialect::kZSCustomV2;
  if (dialect != zscream::LayoutDialect::kVanilla &&
      dialect != zscream::LayoutDialect::kUnmarked && !result.custom_v2)
    return absl::FailedPreconditionError(
        "Bundled upgrade supports vanilla and the legacy-table ZSCustom v2 "
        "layout only; other layouts require their own migration");
  RETURN_IF_ERROR(RequireSpan(rom, 0, 0x100000));
  if ((rom.vector()[0x7FD5] & 0xEF) != 0x20 || rom.vector()[0x7FD9] != 0x01)
    return absl::FailedPreconditionError(
        "Bundled upgrade requires the US LoROM table layout");
  if (rom.size() != 0x100000 && rom.size() != 0x200000 &&
      rom.size() != 0x400000)
    return absl::FailedPreconditionError(
        "Bundled upgrade supports 1, 2 or 4 MiB US LoROM images");
  if (result.custom_v2)
    RETURN_IF_ERROR(RequireSpan(rom, kPoolBegin,
                                kOverworldMessagesExpanded + 320 - kPoolBegin));
  const auto& bytes = rom.vector();
  // The conventional v2 pool reserves these future-v3 tables but does not
  // populate them. Preserve authored v2 pools; refuse dormant/custom contents
  // here rather than silently replacing another producer's data. This is a
  // migration precondition for fixed ZSCustom spans, not a free-space search.
  const int destination_begin = result.custom_v2 ? kPoolEnd : kPoolBegin;
  // Bundled v2 functions begin at1412B8 (not at v3's141940). That occupied
  // code is verified separately against its producer, not treated as a pool.
  const int destination_end =
      std::min<int>(result.custom_v2 ? 0x1412B8 : 0x141938, bytes.size());
  for (int pc = destination_begin; pc < destination_end; ++pc)
    if (bytes[pc] != 0 && bytes[pc] != 0xFF)
      return absl::FailedPreconditionError(absl::StrFormat(
          "Existing data at PC 0x%06X has no supported bundled-upgrade "
          "migration; preserve/reconcile its owner first",
          pc));
  // The bundled v2 patch retains the legacy sign loader. A marker alone does
  // not prove that a custom relocated message table is the live source.
  constexpr std::array<uint8_t, 4> sign_loader = {0xA8, 0xB9, 0x1D, 0xF5};
  if (!std::equal(sign_loader.begin(), sign_loader.end(),
                  bytes.begin() + 0x3B518))
    return absl::FailedPreconditionError(
        "Custom sign-message loader requires its own upgrade migration");
  std::vector<OverworldAreaGeometry> geometry;
  geometry.reserve(kNumOverworldMaps);
  for (int map = 0; map < kNumOverworldMaps; ++map) {
    int parent = map;
    auto size = AreaSizeEnum::SmallArea;
    if (map < kSpecialWorldMapIdStart) {
      const uint8_t old_size = bytes[kOverworldScreenSize + map];
      if (old_size > 1)
        return absl::FailedPreconditionError(absl::StrFormat(
            "Map 0x%02X has unsupported legacy size 0x%02X", map, old_size));
      if (old_size == 0) {
        size = AreaSizeEnum::LargeArea;
        const int local = bytes[kOverworldMapParentId + (map & 0x3F)];
        if (local >= 64)
          return absl::FailedPreconditionError(
              "Legacy parent is outside its world");
        parent = (map & 0x40) + local;
      }
    } else if (map == 0x81 || map == 0x82 || map == 0x89 || map == 0x8A) {
      parent = 0x81;
      size = AreaSizeEnum::LargeArea;
    }
    const int delta = map - parent;
    geometry.push_back({parent, size, (delta / 8) * 2 + delta % 8});
  }
  ASSIGN_OR_RETURN(result.geometry, PlanExpandedOverworldGeometry(geometry));
  // Legacy runtime shares these 64-entry tables between LW and DW. Preserve
  // authored values exactly, including overrides the normal planner won't
  // regenerate. The special world has its own hardcoded legacy loader; its
  // newly addressable normal-screen rows use the existing geometry planner.
  for (int map = 0; map < kSpecialWorldMapIdStart; ++map)
    for (size_t table = 0; table < kLegacyGeometryTables.size(); ++table)
      result.geometry[map].words[table] =
          Word(bytes, kLegacyGeometryTables[table] + (map & 0x3F) * 2);

  if (result.custom_v2) {
    result.pool.assign(bytes.begin() + kPoolBegin, bytes.begin() + kPoolEnd);
  } else {
    result.pool.assign(kPoolEnd - kPoolBegin, 0);
    // New optional pools are initialized but remain disabled. Default source
    // sheet groups are required by the fallback loader even with custom groups
    // disabled; these are the bundled ZSCustom fallback groups, not map art.
    constexpr std::array<uint8_t, 24> defaults = {
        0x3A, 0x3B, 0x3C, 0x3D, 0x53, 0x4D, 0x3E, 0x5B, 0x42, 0x43, 0x44, 0x45,
        0x2F, 0x30, 0x3F, 0x59, 0x3A, 0x3B, 0x3C, 0x3D, 0x47, 0x48, 0x3E, 0x5B};
    std::copy(
        defaults.begin(), defaults.end(),
        result.pool.begin() + OverworldCustomDefaultGFXGroups - kPoolBegin);
    result.pool[0x140146 - kPoolBegin] = 0xFF;  // Beginning rain.
    result.pool[0x140147 - kPoolBegin] = 0xFF;  // Mire rain event.
    PutWord(result.pool, 0x140149 - kPoolBegin, 0x2669);
    for (int map = 0; map < kNumOverworldMaps; ++map) {
      PutWord(result.pool, map * 2,
              map < 64    ? 0x2669
              : map < 128 ? 0x2A32
                          : 0x19C6);
      result.pool[OverworldCustomMainPaletteArray - kPoolBegin + map] =
          map >= 64 && map < 128 ? 1 : 0;
      result.pool[OverworldCustomAnimatedGFXArray - kPoolBegin + map] =
          map >= 64 && map < 128 ? 0x59 : 0x5B;
      PutWord(result.pool,
              OverworldCustomSubscreenOverlayArray - kPoolBegin + map * 2,
              0x00FF);
      std::fill_n(result.pool.begin() + OverworldCustomTileGFXGroupArray -
                      kPoolBegin + map * 8,
                  8, 0xFF);
    }
  }
  // Mosaic was unused in the bundled v2 runtime. Retain its authored pool
  // bytes but do not newly activate them by installing v3.
  result.pool[OverworldCustomMosaicEnabled - kPoolBegin] = 0;

  for (int map = 0; map < kNumOverworldMaps; ++map) {
    result.graphics[map] = bytes[kAreaGfxIdPtr + (map < 128 ? map : 0)];
    // Legacy runtime reads the old palette table, not the v3 location that
    // still holds sprite-loading bounds before this upgrade.
    result.palettes[map] =
        bytes[kOverworldMapPaletteIds + (map < 128 ? map : 0)];
    result.messages[map] =
        Word(bytes, kOverworldMessageIds + (map < 144 ? map : 0) * 2);
  }
  for (int local = 0; local < 16; ++local) {
    result.graphics[128 + local] = bytes[kOverworldSpecialGFXGroup + local];
    result.palettes[128 + local] = bytes[kOverworldSpecialPALGroup + local];
    result.special_sprite_graphics[local] =
        bytes[kOverworldSpecialSpriteGFXGroup + local];
    result.special_sprite_palettes[local] =
        bytes[kOverworldSpecialSpritePalette + local];
  }

  // Reuse the strict maintained sprite codec and its actual legacy table
  // counts. Newly supported phase/map slots point at an existing empty list;
  // no storage is allocated and every existing list/pointer is retained.
  const auto layout = GetOverworldSpriteLayout(rom);
  uint16_t empty = 0;
  for (int phase = 0; phase < 3; ++phase) {
    for (int map = 0; map < layout.counts[phase]; ++map) {
      const int pc = layout.tables[phase] + map * 2;
      ASSIGN_OR_RETURN(auto stream, ReadOverworldSpriteList(rom, pc));
      result.sprites[phase][map] = Word(bytes, pc);
      if (stream.size() == 1 && stream[0] == 0xFF)
        empty = result.sprites[phase][map];
    }
  }
  if (empty == 0)
    return absl::FailedPreconditionError(
        "Upgrade needs an existing empty sprite list for newly supported "
        "slots");
  for (int phase = 0; phase < 3; ++phase)
    std::fill(result.sprites[phase].begin() + layout.counts[phase],
              result.sprites[phase].end(), empty);

  // V3 reuses the old transition tables for item/bomb-door pointers. Preserve
  // list data and aliases in their existing bank instead of repacking it.
  const uint32_t item_table = Word(bytes, overworldItemsAddress) |
                              (bytes[overworldItemsAddress + 2] << 16);
  if (SnesToPc(item_table) != kOverworldItemsPointers ||
      (bytes[overworldItemsAddressBank] & 0x7F) != 0x1B)
    return absl::FailedPreconditionError(
        "Relocated/custom legacy item tables require their own migration");
  Rom item_source(rom);
  std::vector<OverworldMap> item_maps;
  item_maps.reserve(kNumOverworldMaps);
  for (int map = 0; map < kNumOverworldMaps; ++map) {
    item_maps.emplace_back(map, &item_source, nullptr,
                           core::RomAddressOverrides{});
    item_maps.back().SetParent(result.geometry[map].parent);
    item_maps.back().SetAreaSize(result.geometry[map].size);
    if (map < 128) {
      result.item_pointers[map] =
          Word(bytes, kOverworldItemsPointers + map * 2);
      result.bomb_door_locations[map] =
          Word(bytes, kLegacyBombDoorTable + map * 2);
      if (result.geometry[map].parent == map &&
          (result.item_pointers[map] < 0xC3F9 ||
           result.item_pointers[map] >= 0xC89C))
        return absl::FailedPreconditionError(
            "Legacy item pointer is outside its owned list block");
    }
  }
  // Bound the maintained decoder to the source item block plus its loader
  // operands. Truncating this private copy cannot change the original ROM.
  item_source.Expand(overworldItemsAddressBank + 1);
  ASSIGN_OR_RETURN(const auto items, LoadItems(&item_source, item_maps));
  std::array<int, 128> counts{};
  for (const auto& item : items)
    ++counts[item.room_map_id_];
  uint16_t empty_item = 0;
  for (int map = 0; map < 128; ++map) {
    if (result.geometry[map].parent != map)
      continue;
    const int terminator =
        0xD0000 + result.item_pointers[map] + counts[map] * 3;
    if (terminator + 2 > kOverworldItemsEndData ||
        Word(bytes, terminator) != 0xFFFF)
      return absl::FailedPreconditionError(
          "Legacy item list crosses its owned boundary");
    empty_item = static_cast<uint16_t>(terminator & 0xFFFF);
  }
  std::fill(result.item_pointers.begin() + 128, result.item_pointers.end(),
            empty_item);
  return result;
}

absl::Status VerifyBundledV2Source(const Rom& source, core::AsarWrapper& asar,
                                   const std::filesystem::path& asm_directory) {
  auto probe = source.vector();
  ASSIGN_OR_RETURN(
      const auto patched,
      asar.ApplyPatch((asm_directory / "ZSCustomOverworld.asm").string(),
                      probe));
  if (!patched.success || probe.size() != source.size())
    return absl::FailedPreconditionError(
        "Cannot verify this source against the bundled legacy-table v2 "
        "producer");
  for (size_t pc = 0; pc < probe.size(); ++pc) {
    if ((pc >= kPoolBegin && pc < kPoolEnd) || (pc >= 0x7FDC && pc < 0x7FE0))
      continue;
    if (probe[pc] != source.vector()[pc])
      return absl::FailedPreconditionError(absl::StrFormat(
          "Modified v2 hook/function at PC 0x%06X requires its own migration; "
          "bundled upgrade will not discard it",
          pc));
  }
  return absl::OkStatus();
}

absl::Status MigrateUpgradeData(const UpgradeData& data,
                                std::vector<uint8_t>& bytes) {
  if (bytes.size() < 0x141938)
    return absl::OutOfRangeError(
        "Patched ROM does not cover the ZSCustom pools");
  std::copy(data.pool.begin(), data.pool.end(), bytes.begin() + kPoolBegin);
  bytes[OverworldCustomASMHasBeenApplied] = 3;
  for (int map = 0; map < kNumOverworldMaps; ++map) {
    bytes[kOverworldScreenSize + map] =
        static_cast<uint8_t>(data.geometry[map].size);
    bytes[kOverworldMapParentIdExpanded + map] = data.geometry[map].parent;
    for (size_t table = 0; table < kExpandedGeometryTables.size(); ++table)
      PutWord(bytes, kExpandedGeometryTables[table] + map * 2,
              data.geometry[map].words[table]);
    bytes[kAreaGfxIdPtr + map] = data.graphics[map];
    bytes[kOverworldPalettesScreenToSetNew + map] = data.palettes[map];
    PutWord(bytes, kOverworldMessagesExpanded + map * 2, data.messages[map]);
    for (int phase = 0; phase < 3; ++phase)
      PutWord(bytes, kExpandedSpriteTables[phase] + map * 2,
              data.sprites[phase][map]);
  }
  std::copy(data.special_sprite_graphics.begin(),
            data.special_sprite_graphics.end(),
            bytes.begin() + kOverworldSpecialSpriteGfxGroupExpandedTemp);
  std::copy(data.special_sprite_palettes.begin(),
            data.special_sprite_palettes.end(),
            bytes.begin() + kOverworldSpecialSpritePaletteExpandedTemp);
  for (int map = 0; map < kNumOverworldMaps; ++map) {
    PutWord(bytes, kOverworldItemsPointersNew + map * 2,
            data.item_pointers[map]);
    PutWord(bytes, kOverworldBombDoorItemLocationsNew + map * 2,
            data.bomb_door_locations[map]);
  }
  const uint32_t new_item_table = PcToSnes(kOverworldItemsPointersNew);
  PutWord(bytes, overworldItemsAddress, new_item_table & 0xFFFF);
  bytes[overworldItemsAddress + 2] = new_item_table >> 16;
  // Read back using the real v3 map model. This has no terrain or graphics
  // decompression and does not rely on the old marker remaining available.
  Rom check;
  Rom::LoadOptions options;
  options.strip_header = false;
  options.load_resource_labels = false;
  RETURN_IF_ERROR(check.LoadFromData(bytes, options));
  for (int map = 0; map < kNumOverworldMaps; ++map) {
    OverworldMap reopened(map, &check, nullptr, core::RomAddressOverrides{});
    if (reopened.parent() != data.geometry[map].parent ||
        reopened.area_size() != data.geometry[map].size)
      return absl::DataLossError(
          "Upgraded area topology did not reopen unchanged");
  }
  // Asar fixes the checksum before the migrated data is published. Supported
  // images are power-of-two LoROM sizes, so the final checksum is the byte sum
  // with the four checksum/complement bytes contributing a fixed 0x1FE.
  uint8_t size_exponent = 0;
  for (size_t kib = bytes.size() / 1024; kib > 1; kib >>= 1)
    ++size_exponent;
  bytes[0x7FD7] = size_exponent;
  uint32_t checksum = 0x1FE;
  for (size_t pc = 0; pc < bytes.size(); ++pc)
    if (pc < 0x7FDC || pc >= 0x7FE0)
      checksum += bytes[pc];
  PutWord(bytes, 0x7FDC, static_cast<uint16_t>(checksum) ^ 0xFFFF);
  PutWord(bytes, 0x7FDE, static_cast<uint16_t>(checksum));
  return absl::OkStatus();
}

}  // namespace

OverworldUpgradeSystem::OverworldUpgradeSystem(Rom& rom,
                                               std::string asm_directory)
    : rom_(rom), asm_directory_(std::move(asm_directory)) {}

absl::Status OverworldUpgradeSystem::ApplyZSCustomOverworldASM(
    int target_version) {
  if (target_version < 2 || target_version > 3)
    return absl::InvalidArgumentError("Target ZSCustom version must be 2 or 3");
  if (target_version == 2)
    return absl::FailedPreconditionError(
        "The bundled v2 installer does not migrate its required catalogue "
        "tables; use the preserving v3 upgrade");
  RETURN_IF_ERROR(rom_.CheckLayoutSaveable());
  if (rom_.InspectLyraLayout().presence != rom::LyraLayoutPresence::kAbsent)
    return absl::FailedPreconditionError(
        "A native/private special-overworld project cannot use the bundled "
        "upgrade");
  if (rom_.write_fence_depth() != 0)
    return absl::FailedPreconditionError(
        "Bundled upgrade requires an unrestricted ROM editing session");
  const auto dialect = zscream::DetectLayout(rom_);
  if (dialect == zscream::LayoutDialect::kZSCustomV3)
    return absl::AlreadyExistsError("ROM is already ZSCustomOverworld v3");
  if (dialect == zscream::LayoutDialect::kZSCustomV2 && target_version == 2)
    return absl::AlreadyExistsError("ROM is already ZSCustomOverworld v2");
  ASSIGN_OR_RETURN(const UpgradeData data, CaptureUpgradeData(rom_));
  const std::string filename =
      target_version == 3 ? "yaze.asm" : "ZSCustomOverworld.asm";
  const std::filesystem::path asm_path =
      asm_directory_.empty()
          ? std::filesystem::path(util::GetResourcePath("asm/" + filename))
          : std::filesystem::path(asm_directory_) / filename;
  if (!std::filesystem::exists(asm_path))
    return absl::NotFoundError("Bundled upgrade ASM not found: " +
                               asm_path.string());

  // Everything up to publication is private: errors (including ordinary
  // status returns) leave bytes, size, filename and dirty state unchanged.
  try {
    core::AsarWrapper asar;
    RETURN_IF_ERROR(asar.Initialize());
    if (data.custom_v2)
      RETURN_IF_ERROR(
          VerifyBundledV2Source(rom_, asar, asm_path.parent_path()));
    auto candidate = rom_.vector();
    candidate.resize(std::max<size_t>(candidate.size(), 0x200000), 0);
    ASSIGN_OR_RETURN(const auto patched,
                     asar.ApplyPatch(asm_path.string(), candidate));
    if (!patched.success) {
      std::string errors = "Bundled upgrade ASM failed:";
      for (const auto& error : patched.errors)
        errors += "\n" + error;
      return absl::InternalError(errors);
    }
    RETURN_IF_ERROR(MigrateUpgradeData(data, candidate));
    ScopedRomTransaction transaction(rom_);
    rom_.Expand(static_cast<int>(candidate.size()));
    RETURN_IF_ERROR(rom_.WriteVector(0, candidate));
    rom_.set_dirty(true);
    transaction.Commit();
    return absl::OkStatus();
  } catch (const std::exception& e) {
    return absl::InternalError(std::string("Bundled upgrade failed: ") +
                               e.what());
  }
}

}  // namespace yaze::zelda3
