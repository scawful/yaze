#include "core/gfx_sheet_policy_adapter.h"

#include <string>
#include <vector>

#include "absl/status/status.h"
#include "absl/strings/str_format.h"
#include "absl/strings/str_join.h"
#include "rom/snes.h"
#include "util/macro.h"

namespace yaze::core {

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
