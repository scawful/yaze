#include <fstream>

#include "asar-dll-bindings/c/asar.h"
#include "cli/z3ed.h"
#include "cli/tui/asar_patch.h"
#include "util/bps.h"
#include "app/core/asar_wrapper.h"
#include "absl/flags/flag.h"
#include "absl/flags/declare.h"
#include "absl/strings/str_format.h"
#include "absl/strings/str_join.h"

ABSL_DECLARE_FLAG(std::string, rom);

namespace yaze {
namespace cli {

absl::Status ApplyPatch::Run(const std::vector<std::string>& arg_vec) {
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

AsarPatch::AsarPatch() {}

absl::Status AsarPatch::Run(const std::vector<std::string>& arg_vec) {
  if (arg_vec.empty()) {
    return absl::InvalidArgumentError("Usage: patch apply-asar <patch.asm>");
  }
  const std::string& patch_file = arg_vec[0];

  std::string rom_file = absl::GetFlag(FLAGS_rom);
  if (rom_file.empty()) {
    return absl::InvalidArgumentError("ROM file must be provided via --rom flag.");
  }

  rom_.LoadFromFile(rom_file);
  if (!rom_.is_loaded()) {
    return absl::AbortedError("Failed to load ROM.");
  }

  core::AsarWrapper wrapper;
  auto init_status = wrapper.Initialize();
  if (!init_status.ok()) {
    return init_status;
  }

  auto rom_data = rom_.vector();
  auto patch_result = wrapper.ApplyPatch(patch_file, rom_data);

  if (!patch_result.ok()) {
    return patch_result.status();
  }

  const auto& result = patch_result.value();
  if (!result.success) {
    return absl::AbortedError(absl::StrJoin(result.errors, "; "));
  }

  rom_.LoadFromData(rom_data);
  auto save_status = rom_.SaveToFile({.save_new = false});
  if (!save_status.ok()) {
    return save_status;
  }

  std::cout << "✅ Patch applied successfully and ROM saved to: " << rom_.filename() << std::endl;
  return absl::OkStatus();
}

void AsarPatch::RunTUI(ftxui::ScreenInteractive& screen) {
  // TODO: Implement Asar patch TUI
  (void)screen; // Suppress unused parameter warning
}

// ... rest of the file


absl::Status CreatePatch::Run(const std::vector<std::string>& arg_vec) {
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