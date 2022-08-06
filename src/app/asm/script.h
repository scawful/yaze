#ifndef YAZE_APP_ASM_SCRIPT_H
#define YAZE_APP_ASM_SCRIPT_H

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

constexpr char kDefaultMosaicHook[] = "$02AADB";

class Script {
 public:
  absl::StatusOr<absl::string_view> GenerateMosaicChangeAssembly(
      std::array<int, core::kNumOverworldMaps> mosaic_tiles);
};

}  // namespace snes_asm
}  // namespace app
}  // namespace yaze

#endif