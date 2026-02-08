#include "core/oracle_progression_loader.h"

#include <cstdint>
#include <fstream>
#include <vector>

#include "absl/strings/str_format.h"

namespace yaze::core {

absl::StatusOr<OracleProgressionState> LoadOracleProgressionFromSrmFile(
    const std::string& srm_path) {
  if (srm_path.empty()) {
    return absl::InvalidArgumentError("SRM path is empty");
  }

  std::ifstream file(srm_path, std::ios::binary);
  if (!file.is_open()) {
    return absl::NotFoundError(
        absl::StrFormat("Cannot open SRM file: %s", srm_path));
  }

  file.seekg(0, std::ios::end);
  const std::streampos end = file.tellg();
  if (end < 0) {
    return absl::InternalError(
        absl::StrFormat("Failed to read SRM file size: %s", srm_path));
  }

  std::vector<uint8_t> data(static_cast<size_t>(end));
  file.seekg(0, std::ios::beg);
  if (!data.empty()) {
    file.read(reinterpret_cast<char*>(data.data()),
              static_cast<std::streamsize>(data.size()));
    if (!file) {
      return absl::DataLossError(
          absl::StrFormat("Failed to read SRM file: %s", srm_path));
    }
  }

  return OracleProgressionState::ParseFromSRAM(data.data(), data.size());
}

}  // namespace yaze::core

