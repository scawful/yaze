#ifndef YAZE_APP_ASM_SCRIPT_H
#define YAZE_APP_ASM_SCRIPT_H

#include <asar/interface-lib.h>

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
  virtual ~ScriptTemplate() = default;
  virtual absl::Status ApplyPatchToROM(ROM& rom) = 0;
  virtual absl::Status PatchOverworldMosaic(
      ROM& rom, char mosaic_tiles[core::kNumOverworldMaps], int routine_offset,
      int hook_offset = 0) = 0;
};

class Script : public ScriptTemplate {
 public:
  absl::Status ApplyPatchToROM(ROM& rom) override;
  absl::Status PatchOverworldMosaic(
      ROM& rom, char mosaic_tiles[core::kNumOverworldMaps], int routine_offset,
      int hook_offset = 0) override;

 private:
  int64_t patch_size_;
  std::string patch_filename_;
  std::string patch_contents_;
};

}  // namespace snes_asm
}  // namespace app
}  // namespace yaze

#endif