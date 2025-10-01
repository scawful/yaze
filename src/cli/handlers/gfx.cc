#include "cli/z3ed.h"

namespace yaze {
namespace cli {

absl::Status GfxExport::Run(const std::vector<std::string>& arg_vec) {
  if (arg_vec.size() < 2) {
    return absl::InvalidArgumentError("Usage: gfx export-sheet <sheet_id> --to <file>");
  }

  // TODO: Implement gfx export logic
  std::cout << "Gfx export not yet implemented." << std::endl;

  return absl::OkStatus();
}

absl::Status GfxImport::Run(const std::vector<std::string>& arg_vec) {
  if (arg_vec.size() < 2) {
    return absl::InvalidArgumentError("Usage: gfx import-sheet <sheet_id> --from <file>");
  }

  // TODO: Implement gfx import logic
  std::cout << "Gfx import not yet implemented." << std::endl;

  return absl::OkStatus();
}

} // namespace cli
} // namespace yaze
