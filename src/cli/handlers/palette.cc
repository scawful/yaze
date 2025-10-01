#include "cli/z3ed.h"

namespace yaze {
namespace cli {

absl::Status PaletteExport::Run(const std::vector<std::string>& arg_vec) {
  if (arg_vec.size() < 2) {
    return absl::InvalidArgumentError("Usage: palette export --group <group> --id <id> --format <format>");
  }

  // TODO: Implement palette export logic
  std::cout << "Palette export not yet implemented." << std::endl;

  return absl::OkStatus();
}

absl::Status PaletteImport::Run(const std::vector<std::string>& arg_vec) {
  if (arg_vec.size() < 2) {
    return absl::InvalidArgumentError("Usage: palette import --group <group> --id <id> --from <file>");
  }

  // TODO: Implement palette import logic
  std::cout << "Palette import not yet implemented." << std::endl;

  return absl::OkStatus();
}

} // namespace cli
} // namespace yaze
