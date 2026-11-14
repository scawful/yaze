#ifndef YAZE_CORE_PROPOSAL_WORKSPACE_H_
#define YAZE_CORE_PROPOSAL_WORKSPACE_H_

#include <filesystem>
#include <string>

#include "absl/status/statusor.h"

namespace yaze::core {

struct ProposalWorkspace {
  std::filesystem::path root;
  std::filesystem::path project_file;
  std::filesystem::path rom_file;
  std::string proposal_id;
  std::string domain;
  std::string context_note;
  int target = -1;
  int graphics_entrance_slot = -1;
  int unbound_references = 0;
  std::string baseline_rom_sha256;
};

// Read a prepared packet and verify its preserved baseline. This does not
// import proposal pixels, load a ROM model, or manufacture a save receipt.
absl::StatusOr<ProposalWorkspace> LoadProposalWorkspace(
    const std::filesystem::path& packet_root);

// Export saved-file changes and the user's note. Call normal Save first in
// the UI; this operation never saves models or writes the project/base ROM.
absl::StatusOr<std::filesystem::path> ExportProposalFeedback(
    const ProposalWorkspace& workspace, const std::string& note);

}  // namespace yaze::core
#endif  // YAZE_CORE_PROPOSAL_WORKSPACE_H_
