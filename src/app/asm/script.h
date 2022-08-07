#ifndef YAZE_APP_ASM_SCRIPT_H
#define YAZE_APP_ASM_SCRIPT_H

#include <asardll.h>

#include <array>
#include <cstdint>
#include <fstream>
#include <sstream>
#include <string>

#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "absl/strings/str_cat.h"
#include "absl/strings/string_view.h"
#include "app/core/constants.h"
#include "app/rom.h"

namespace yaze {
namespace app {
namespace snes_asm {

const std::string kMosaicChangeOffset = "$02AADB";
constexpr int kSNESToPCOffset = 0x138000;

class ScriptTemplate {
 public:
  virtual absl::Status ApplyPatchToROM(ROM& rom) = 0;
};

class Script : public ScriptTemplate {
 public:
  Script() { asar_init_with_dll_path("assets/libasar.dll"); }

  absl::Status ApplyPatchToROM(ROM& rom) override;
  absl::Status GenerateMosaicChangeAssembly(
      ROM& rom, char mosaic_tiles[core::kNumOverworldMaps], int routine_offset,
      int hook_offset = 0);

 private:
  int64_t patch_size_;
  std::string patch_filename_;
  std::string patch_contents_;
};

}  // namespace snes_asm
}  // namespace app
}  // namespace yaze

#endif