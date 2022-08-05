#include "script.h"

#include <asar.h>

#include <array>
#include <fstream>
#include <sstream>

#include "absl/status/statusor.h"
#include "absl/strings/string_view.h"
#include "app/core/constants.h"

namespace yaze {
namespace app {
namespace snes_asm {

absl::StatusOr<absl::string_view> GenerateMosaicChangeAssembly(
    MosaicArray mosaic_tiles) {
  std::fstream file("assets/asm/mosaic_change.asm",
                    std::ios::out | std::ios::in);
  if (!file.is_open()) {
    return absl::InvalidArgumentError(
        "Couldn't open mosaic change template file");
  }
  std::stringstream assembly;

  assembly << absl::StrCat("org ", kDefaultMosaicHook);
  assembly << file.rdbuf();

  file.close();
  return assembly.str();
}

}  // namespace snes_asm
}  // namespace app
}  // namespace yaze