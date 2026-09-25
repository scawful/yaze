#ifndef YAZE_CORE_GFX_SHEET_POLICY_ADAPTER_H_
#define YAZE_CORE_GFX_SHEET_POLICY_ADAPTER_H_

#include "absl/status/statusor.h"
#include "core/hack_manifest.h"
#include "zelda3/gfx_sheet_storage.h"

namespace yaze::core {

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

}  // namespace yaze::core

#endif  // YAZE_CORE_GFX_SHEET_POLICY_ADAPTER_H_
