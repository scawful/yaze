#include "script.h"

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
#include "app/core/common.h"
#include "app/core/constants.h"
#include "app/rom.h"

namespace yaze {
namespace app {
namespace snes_asm {

absl::Status Script::ApplyPatchToROM(ROM &rom) {
  if (patch_contents_.empty() || patch_filename_.empty()) {
    return absl::InvalidArgumentError("No patch loaded!");
  }
  int count = 0;
  auto data = (char *)rom.data();
  int size = rom.size();
  if (!asar_patch(patch_filename_.c_str(), data, patch_size_, &size)) {
    auto asar_error = asar_geterrors(&count);
    auto full_error = asar_error->fullerrdata;
    return absl::InternalError(absl::StrCat("ASAR Error: ", full_error));
  }
  return absl::OkStatus();
}

absl::Status Script::PatchOverworldMosaic(
    ROM &rom, char mosaic_tiles[core::kNumOverworldMaps], int routine_offset,
    int hook_offset) {
  for (int i = 0; i < core::kNumOverworldMaps; i++) {
    if (mosaic_tiles[i]) {
      rom[core::overworldCustomMosaicArray + i] = 0x01;
    } else {
      rom[core::overworldCustomMosaicArray + i] = 0x00;
    }
  }
  std::fstream file("assets/asm/mosaic_change.asm",
                    std::ios::out | std::ios::in);
  if (!file.is_open()) {
    return absl::InvalidArgumentError(
        "Unable to open mosaic change assembly source");
  }
  std::stringstream assembly;
  assembly << file.rdbuf();
  file.close();
  auto assembly_string = assembly.str();
  if (!core::StringReplace(assembly_string, "<HOOK>", kMosaicChangeOffset)) {
    return absl::InternalError(
        "Mosaic template did not have proper `<HOOK>` to replace.");
  }
  if (!core::StringReplace(
          assembly_string, "<EXPANDED_SPACE>",
          absl::StrFormat("$%x", routine_offset + kSNESToPCOffset))) {
    return absl::InternalError(
        "Mosaic template did not have proper `<EXPANDED_SPACE>` to replace.");
  }
  patch_contents_ = assembly_string;
  return ApplyPatchToROM(rom);
}

}  // namespace snes_asm
}  // namespace app
}  // namespace yaze