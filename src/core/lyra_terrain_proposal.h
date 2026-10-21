#ifndef YAZE_CORE_LYRA_TERRAIN_PROPOSAL_H_
#define YAZE_CORE_LYRA_TERRAIN_PROPOSAL_H_

#include <array>
#include <cstddef>
#include <cstdint>
#include <string>
#include <string_view>

#include "absl/status/statusor.h"
#include "core/lyra_terrain_document.h"

namespace yaze::core {

// Identity of the source the editor actually loaded. The trusted caller derives
// every field from loaded data; a proposal's own declarations are only ever
// compared against this value and never grant authority.
struct LyraTerrainProposalBinding {
  std::string game_id;
  std::string adapter_id;
  std::string rom_sha256;
  std::string definition_fingerprint;
  std::string profile_fingerprint;
  uint32_t tile_count = 0;
};

inline constexpr char kLyraMapAssetProposalAdapterId[] =
    "lyra-sidecar-v3-map-assets-v1";

// Canonical per-map profile identity shared with project_proposal.py. The caller
// must resolve all three asset bindings from the loaded native source; this
// pure helper does not make document declarations authoritative.
absl::StatusOr<std::string> LyraMapProposalProfileFingerprint(
    const LyraTerrainDocument& document, int map_id,
    const std::array<std::string, 3>& resolved_asset_bindings);

inline constexpr size_t kMaxLyraTerrainProposalBytes = 1024 * 1024;

// Admits a schema-1 "lyra_tile16_proposal_bundle" whose report must equal the
// scripts/pixel_art/footprint_proposal.py plan recomputed from the bundled
// target, footprint and report placements. The whole 32x32 target must equal
// the document map. Returns the exact cell changes without touching the
// document; the caller applies them through its existing history. A complete
// Tile32 inventory is refused as unsupported and nothing is allocated.
absl::StatusOr<LyraTerrainStroke> AdmitLyraTerrainProposal(
    std::string_view bundle_json, const LyraTerrainDocument& document,
    const LyraTerrainProposalBinding& actual_binding);

// Adds an explicitly versioned map-assets adapter without changing the legacy
// shared-profile contract. Both identities are derived by the trusted caller.
absl::StatusOr<LyraTerrainStroke> AdmitLyraTerrainProposal(
    std::string_view bundle_json, const LyraTerrainDocument& document,
    const LyraTerrainProposalBinding& actual_binding,
    const LyraTerrainProposalBinding& actual_map_binding);

}  // namespace yaze::core

#endif  // YAZE_CORE_LYRA_TERRAIN_PROPOSAL_H_
