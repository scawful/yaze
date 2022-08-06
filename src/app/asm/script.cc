#include "script.h"

#include <interface-lib.h>

#include <array>
#include <cstdint>
#include <fstream>
#include <sstream>
#include <string>

#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "absl/strings/string_view.h"
#include "app/core/constants.h"
#include "app/rom.h"

namespace yaze {
namespace app {
namespace snes_asm {

absl::Status Script::ApplyPatchToROM(ROM& rom) {
  char* data = (char*) rom.data();
  int size = 0;
  if (!asar_patch(patch_filename_.c_str(), data, patch_size_, &size)) {
    return absl::InternalError("Unable to apply patch");
  }
  return absl::OkStatus();
}

absl::StatusOr<absl::string_view> Script::GenerateMosaicChangeAssembly(
    std::array<int, core::kNumOverworldMaps> mosaic_tiles) {
  std::fstream file("assets/asm/mosaic_change.asm",
                    std::ios::out | std::ios::in);
  if (!file.is_open()) {
    return absl::InvalidArgumentError(
        "Couldn't open mosaic change template file");
  }
  std::stringstream assembly;
  assembly << "org ";
  assembly << kDefaultMosaicHook;
  assembly << file.rdbuf();

  file.close();
  return assembly.str();
}

}  // namespace snes_asm
}  // namespace app
}  // namespace yaze