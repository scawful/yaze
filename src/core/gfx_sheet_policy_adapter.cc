#include "core/gfx_sheet_policy_adapter.h"

#include <string>
#include <vector>

#include "absl/status/status.h"
#include "absl/strings/str_format.h"
#include "absl/strings/str_join.h"
#include "rom/snes.h"
#include "util/macro.h"
#include "util/rom_hash.h"

namespace yaze::core {

absl::StatusOr<zelda3::ResolvedGfxSheetLayout> ResolveGfxSheetLayout(
    const Rom& rom, const HackManifest& manifest,
    const RomAddressOverrides& overrides) {
  if (!rom.is_loaded()) {
    return absl::FailedPreconditionError("ROM not loaded");
  }

  zelda3::ResolvedGfxSheetLayout resolved;
  const auto& declared = manifest.graphics_sheet_layout().pointer_layout;
  if (declared) {
    resolved.tables = {declared->bank_pc, declared->high_pc, declared->low_pc};
    resolved.physical_table_rows = declared->physical_table_rows;
    resolved.usable_sheet_count = declared->usable_sheet_count;
    resolved.default_sheet_id = declared->default_sheet_id;
    resolved.rom_sha1 = declared->rom_sha1;
    resolved.rom_role = declared->rom_role;
    resolved.source_path = declared->source_path;
    resolved.source_sha1 = declared->source_sha1;
    resolved.explicit_layout = true;
  }

  const auto bank = overrides.GetAddress(RomAddressKey::kOverworldGfxPtr1);
  const auto high = overrides.GetAddress(RomAddressKey::kOverworldGfxPtr2);
  const auto low = overrides.GetAddress(RomAddressKey::kOverworldGfxPtr3);
  const bool any_override =
      bank.has_value() || high.has_value() || low.has_value();
  if (any_override && !(bank && high && low)) {
    return absl::FailedPreconditionError(
        "Graphics pointer overrides must declare all three tables");
  }
  if (any_override) {
    if (declared &&
        (*bank != resolved.tables.bank || *high != resolved.tables.high ||
         *low != resolved.tables.low)) {
      return absl::FailedPreconditionError(
          "Graphics pointer overrides conflict with the manifest layout");
    }
    resolved.tables = {*bank, *high, *low};
  } else if (!declared && zelda3::UsesJapaneseGfxSheetTables(rom)) {
    return absl::FailedPreconditionError(
        "Japanese graphics tables require an explicit layout or complete "
        "pointer overrides");
  }

  RETURN_IF_ERROR(zelda3::ValidateResolvedGfxSheetLayout(rom, resolved));
  return resolved;
}

absl::StatusOr<zelda3::ResolvedGfxSheetLayout> ResolveGfxSheetLayout(
    const Rom& rom, const project::YazeProject& project) {
  if (!rom.is_loaded()) {
    return absl::FailedPreconditionError("ROM not loaded");
  }
  if (!project.rom_metadata.expected_hash.empty() &&
      util::ComputeRomIdentityHashes(rom.data(), rom.size())
              .Match(project.rom_metadata.expected_hash) ==
          util::RomHashMatch::kMismatch) {
    return absl::FailedPreconditionError(
        "Loaded ROM does not match the project's expected ROM identity");
  }
  const auto& declared =
      project.hack_manifest.graphics_sheet_layout().pointer_layout;
  if (declared) {
    const auto role = project.rom_metadata.role;
    const bool matches = (declared->rom_role == "development" &&
                          role == project::RomRole::kDev) ||
                         (declared->rom_role == "compiled" &&
                          (role == project::RomRole::kPatched ||
                           role == project::RomRole::kRelease));
    if (!matches) {
      return absl::FailedPreconditionError(
          "Graphics pointer layout role conflicts with the project ROM role");
    }
  }
  return ResolveGfxSheetLayout(rom, project.hack_manifest,
                               project.rom_address_overrides);
}

absl::StatusOr<zelda3::GfxSheetWritePolicy> BuildGfxSheetWritePolicy(
    const HackManifest& manifest) {
  zelda3::GfxSheetWritePolicy policy;
  if (!manifest.loaded()) {
    return policy;
  }

  const GraphicsSheetLayout& layout = manifest.graphics_sheet_layout();
  for (const SnesAddressRange& range : layout.allocation_regions) {
    if ((range.start & 0xFFFFu) < 0x8000u) {
      return absl::InvalidArgumentError(absl::StrFormat(
          "Graphics allocation start $%06X is not a mapped LoROM address",
          range.start));
    }
    const uint32_t begin = SnesToPc(range.start);
    // The half-open end may sit exactly on the next bank's $8000.
    const uint32_t end = SnesToPc(range.end);
    if (begin >= end) {
      return absl::InvalidArgumentError(absl::StrFormat(
          "Graphics allocation range [$%06X, $%06X) is empty in PC space",
          range.start, range.end));
    }
    policy.allocation_regions.push_back({begin, end});
  }
  policy.reserved_sheets.insert(layout.reserved_sheets.begin(),
                                layout.reserved_sheets.end());

  const HackManifest* manifest_ptr = &manifest;
  policy.check_write = [manifest_ptr](uint32_t begin,
                                      uint32_t end) -> absl::Status {
    const auto conflicts = manifest_ptr->AnalyzePcWriteRanges({{begin, end}});
    if (conflicts.empty()) {
      return absl::OkStatus();
    }
    std::vector<std::string> parts;
    for (const WriteConflict& conflict : conflicts) {
      parts.push_back(absl::StrFormat(
          "$%06X (%s%s%s)", conflict.address,
          AddressOwnershipToString(conflict.ownership),
          conflict.module.empty() ? "" : ": ", conflict.module));
    }
    return absl::FailedPreconditionError(absl::StrFormat(
        "Graphics write [0x%06X, 0x%06X) touches ASM-owned bytes: %s", begin,
        end, absl::StrJoin(parts, ", ")));
  };
  return policy;
}

absl::StatusOr<zelda3::GfxSheetWritePolicy> BuildGfxSheetWritePolicy(
    const project::YazeProject& project) {
  ASSIGN_OR_RETURN(zelda3::GfxSheetWritePolicy policy,
                   BuildGfxSheetWritePolicy(project.hack_manifest));
  const auto& settings = project.graphics_sheets;
  policy.reserved_sheets.insert(settings.reserved_sheets.begin(),
                                settings.reserved_sheets.end());
  for (const auto& [sheet, blocks] : settings.reserved_blocks) {
    auto& merged = policy.reserved_blocks[sheet];
    merged.insert(merged.end(), blocks.begin(), blocks.end());
  }
  return policy;
}

zelda3::GfxSheetInventoryOptions BuildGfxSheetInventoryOptions(
    const project::YazeProject* project) {
  zelda3::GfxSheetInventoryOptions options;
  if (project == nullptr) {
    return options;
  }
  const auto& settings = project->graphics_sheets;
  options.reserved_sheets.insert(settings.reserved_sheets.begin(),
                                 settings.reserved_sheets.end());
  options.flagged_sheets.insert(settings.flagged_sheets.begin(),
                                settings.flagged_sheets.end());
  options.reserved_blocks = settings.reserved_blocks;
  const auto& manifest_reserved =
      project->hack_manifest.graphics_sheet_layout().reserved_sheets;
  options.reserved_sheets.insert(manifest_reserved.begin(),
                                 manifest_reserved.end());
  return options;
}

}  // namespace yaze::core
