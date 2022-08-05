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

using MosaicArray = std::array<int, core::kNumOverworldMaps>;
constexpr char kDefaultMosaicHook[] = "$02AADB";

absl::StatusOr<absl::string_view> GenerateMosaicChangeAssembly(
    MosaicArray mosaic_tiles);

}  // namespace snes_asm
}  // namespace app
}  // namespace yaze

#endif