#include "absl/strings/str_format.h"
#include "core/asar_wrapper.h"

// Temporary stub implementation until ASAR library build is fixed
// All methods return UnimplementedError

namespace yaze {
namespace core {

AsarWrapper::AsarWrapper() : initialized_(false) {}

AsarWrapper::~AsarWrapper() {}

absl::Status AsarWrapper::Initialize() {
  return absl::UnimplementedError(
      "ASAR library not available - build needs fixing");
}

void AsarWrapper::Shutdown() {
  initialized_ = false;
}

std::string AsarWrapper::GetVersion() const {
  return "ASAR disabled";
}

int AsarWrapper::GetApiVersion() const {
  return 0;
}

void AsarWrapper::Reset() {
  patches_applied_.clear();
}

absl::Status AsarWrapper::ApplyPatch(
    const std::string& patch_content, std::vector<uint8_t>& rom_data,
    const std::vector<std::string>& include_paths) {
  return absl::UnimplementedError(
      "ASAR library not available - build needs fixing");
}

absl::Status AsarWrapper::ApplyPatchFromFile(const std::string& patch_file,
                                             std::vector<uint8_t>& rom_data) {
  return absl::UnimplementedError(
      "ASAR library not available - build needs fixing");
}

std::vector<AsarWrapper::AsarError> AsarWrapper::GetErrors() const {
  return std::vector<AsarError>();
}

std::vector<AsarWrapper::AsarWarning> AsarWrapper::GetWarnings() const {
  return std::vector<AsarWarning>();
}

std::vector<AsarWrapper::AsarSymbol> AsarWrapper::GetSymbols() const {
  return std::vector<AsarSymbol>();
}

const std::vector<std::string>& AsarWrapper::GetAppliedPatches() const {
  return patches_applied_;
}

}  // namespace core
}  // namespace yaze
