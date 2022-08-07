#include "script.h"

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

static auto string_replace(std::string &str, const std::string &from,
                           const std::string &to) -> bool {
  size_t start = str.find(from);
  if (start == std::string::npos) return false;

  str.replace(start, from.length(), to);
  return true;
}

std::string GenerateBytePool(char mosaic_tiles[core::kNumOverworldMaps]) {
  std::string to_return = "";
  int column = 0;
  for (int i = 0; i < core::kNumOverworldMaps; ++i) {
    std::string to_add = "";

    // if start of line, define byte
    if (i == 0 || i % 8 == 0) {
      to_add += "  db ";
    }

    // set byte
    to_add += "$00";
    if (mosaic_tiles[i] > 0) {
      if (i == 0 || i % 8 == 0) {
        to_add = "  db $01";
      } else {
        to_add = "$01";
      }
    }

    // newline or comma separated
    if (column == 7) {
      column = 0;
      to_add += " \n";
    } else {
      column++;
      to_add += ", ";
    }

    to_return += to_add;
  }
  return to_return;
}

absl::Status Script::ApplyPatchToROM(ROM &rom) {
  if (patch_contents_.empty() || patch_filename_.empty()) {
    return absl::InvalidArgumentError("No patch loaded!");
  }

  char *data = (char *)rom.data();
  int size = rom.GetSize();
  int count = 0;
  if (!asar_patch(patch_filename_.c_str(), data, patch_size_, &size)) {
    auto asar_error = asar_geterrors(&count);
    auto full_error = asar_error->fullerrdata;
    return absl::InternalError(absl::StrCat("ASAR Error: ", full_error));
  }
  return absl::OkStatus();
}

absl::Status Script::GenerateMosaicChangeAssembly(
    ROM &rom, char mosaic_tiles[core::kNumOverworldMaps], int routine_offset,
    int hook_offset) {
  std::fstream file("assets/asm/mosaic_change.asm",
                    std::ios::out | std::ios::in);
  if (!file.is_open()) {
    return absl::InvalidArgumentError(
        "Couldn't open mosaic change template file");
  }

  std::stringstream assembly;
  assembly << file.rdbuf();
  file.close();

  auto assembly_string = assembly.str();

  if (!hook_offset) {
    // TODO: TESTME
    if (!string_replace(assembly_string, "<HOOK>",
                        absl::StrFormat("$%06x", hook_offset))) {
      return absl::InternalError(
          "Mosaic template did not have proper `<HOOK>` to replace.");
    }
  } else {
    if (!string_replace(assembly_string, "<HOOK>", kMosaicChangeOffset)) {
      return absl::InternalError(
          "Mosaic template did not have proper `<HOOK>` to replace.");
    }
  }

  if (!string_replace(
          assembly_string, "<EXPANDED_SPACE>",
          absl::StrFormat("$%x", routine_offset + kSNESToPCOffset))) {
    return absl::InternalError(
        "Mosaic template did not have proper `<EXPANDED_SPACE>` to replace.");
  }

  assembly_string += GenerateBytePool(mosaic_tiles);
  patch_filename_ = "assets/asm/mosaic_change_generated.asm";
  std::ofstream new_file(patch_filename_, std::ios::out);
  if (new_file.is_open()) {
    new_file.write(assembly_string.c_str(), assembly_string.size());
    new_file.close();
  }

  return ApplyPatchToROM(rom);
}

}  // namespace snes_asm
}  // namespace app
}  // namespace yaze