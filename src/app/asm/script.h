#ifndef YAZE_APP_ASM_SCRIPT_H
#define YAZE_APP_ASM_SCRIPT_H

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

constexpr char kDefaultMosaicHook[] = "$02AADB";

class Script {
 public:
  Script() = default;

  absl::Status ApplyPatchToROM(ROM& rom);

  absl::StatusOr<absl::string_view> GenerateMosaicChangeAssembly(
      std::array<int, core::kNumOverworldMaps> mosaic_tiles);

 private:
  int64_t patch_size_;
  std::string patch_filename_;
  std::string patch_contents_;
};

}  // namespace snes_asm
}  // namespace app
}  // namespace yaze

#endif