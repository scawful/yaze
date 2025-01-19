#include "cli/z3ed.h"

#include "asar-dll-bindings/c/asar.h"

#include "util/bps.h"

namespace yaze {
namespace cli {

absl::Status ApplyPatch::handle(const std::vector<std::string>& arg_vec) {
  std::string rom_filename = arg_vec[1];
  std::string patch_filename = arg_vec[2];
  RETURN_IF_ERROR(rom_.LoadFromFile(rom_filename))
  auto source = rom_.vector();
  std::ifstream patch_file(patch_filename, std::ios::binary);
  std::vector<uint8_t> patch;
  patch.resize(rom_.size());
  patch_file.read((char*)patch.data(), patch.size());

  // Apply patch
  std::vector<uint8_t> patched;
  util::ApplyBpsPatch(source, patch, patched);

  // Save patched file
  std::ofstream patched_rom("patched.sfc", std::ios::binary);
  patched_rom.write((char*)patched.data(), patched.size());
  patched_rom.close();
  return absl::OkStatus();
}

absl::Status AsarPatch::handle(const std::vector<std::string>& arg_vec) {
  std::string patch_filename = arg_vec[1];
  std::string rom_filename = arg_vec[2];
  RETURN_IF_ERROR(rom_.LoadFromFile(rom_filename))
  int buflen = rom_.vector().size();
  int romlen = rom_.vector().size();
  if (!asar_patch(patch_filename.c_str(), rom_filename.data(), buflen,
                  &romlen)) {
    std::string error_message = "Failed to apply patch: ";
    int num_errors = 0;
    const errordata* errors = asar_geterrors(&num_errors);
    for (int i = 0; i < num_errors; i++) {
      error_message += absl::StrFormat("%s", errors[i].fullerrdata);
    }
    return absl::InternalError(error_message);
  }
  return absl::OkStatus();
}

absl::Status CreatePatch::handle(const std::vector<std::string>& arg_vec) {
  std::vector<uint8_t> source;
  std::vector<uint8_t> target;
  std::vector<uint8_t> patch;
  // Create patch
  util::CreateBpsPatch(source, target, patch);

  // Save patch to file
  // std::ofstream patchFile("patch.bps", ios::binary);
  // patchFile.write(reinterpret_cast<const char*>(patch.data()),
  // patch.size()); patchFile.close();
  return absl::OkStatus();
}

}  // namespace cli
}  // namespace yaze