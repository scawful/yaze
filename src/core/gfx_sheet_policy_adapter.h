#ifndef YAZE_CORE_GFX_SHEET_POLICY_ADAPTER_H_
#define YAZE_CORE_GFX_SHEET_POLICY_ADAPTER_H_

#include "absl/status/statusor.h"
#include "core/hack_manifest.h"
#include "core/project.h"
#include "core/rom_settings.h"
#include "zelda3/gfx_sheet_inventory.h"
#include "zelda3/gfx_sheet_storage.h"

namespace yaze::core {

// Resolves read-only graphics layout metadata against the actual loaded ROM.
// Explicit layouts require a matching ROM identity and declared source origin.
// Legacy address overrides remain supported as a complete three-table set;
// they cannot override a manifest layout or enable expanded sheet writes.
// This function does not consult the process-global RomSettings singleton.
absl::StatusOr<zelda3::ResolvedGfxSheetLayout> ResolveGfxSheetLayout(
    const Rom& rom, const HackManifest& manifest,
    const RomAddressOverrides& overrides = {});

// Also verifies an existing project ROM hash and the explicit layout's role.
// A role string alone never authorizes writes or source-file publication.
absl::StatusOr<zelda3::ResolvedGfxSheetLayout> ResolveGfxSheetLayout(
    const Rom& rom, const project::YazeProject& project);

// Builds the graphics-sheet write policy from a hack manifest:
// allocation regions and reserved sheets come from `graphics_sheet_regions`,
// and every write is refused when AnalyzePcWriteRanges() reports it touches
// ASM-owned bytes (protected hooks, owned banks, expansion banks).
//
// An unloaded manifest yields an empty policy: in-place writes only, no
// relocation. The returned check_write keeps a pointer to `manifest`, so the
// manifest must outlive the policy.
absl::StatusOr<zelda3::GfxSheetWritePolicy> BuildGfxSheetWritePolicy(
    const HackManifest& manifest);

// The manifest policy above plus the project's `[graphics_sheets]` rules:
// reserved sheets are merged, reserved 16x16 blocks are added.
absl::StatusOr<zelda3::GfxSheetWritePolicy> BuildGfxSheetWritePolicy(
    const project::YazeProject& project);

// Reserved sheets (project `[graphics_sheets]` plus the hack manifest),
// flagged sheets and reserved 16x16 blocks for the sheet inventory. A null
// project yields empty options.
zelda3::GfxSheetInventoryOptions BuildGfxSheetInventoryOptions(
    const project::YazeProject* project);

}  // namespace yaze::core

#endif  // YAZE_CORE_GFX_SHEET_POLICY_ADAPTER_H_
