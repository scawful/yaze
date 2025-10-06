#include "app/core/project.h"
#include "cli/cli.h"
#include "util/file_util.h"
#include "util/bps.h"
#include <filesystem>
#ifndef _WIN32
#include <glob.h>
#endif

namespace yaze {
namespace cli {

absl::Status ProjectInit::Run(const std::vector<std::string>& arg_vec) {
  if (arg_vec.empty()) {
    return absl::InvalidArgumentError("Usage: project init <project_name>");
  }

  std::string project_name = arg_vec[0];
  core::YazeProject project;
  auto status = project.Create(project_name, ".");
  if (!status.ok()) {
    return status;
  }

  std::cout << "✅ Successfully initialized project: " << project_name
            << std::endl;

  return absl::OkStatus();
}

absl::Status ProjectBuild::Run(const std::vector<std::string>& arg_vec) {
  core::YazeProject project;
  auto status = project.Open(".");
  if (!status.ok()) {
    return status;
  }

  Rom rom;
  status = rom.LoadFromFile(project.rom_filename);
  if (!status.ok()) {
    return status;
  }

  // Apply BPS patches - cross-platform with std::filesystem
  namespace fs = std::filesystem;
  std::vector<std::string> bps_files;
  
  try {
    for (const auto& entry : fs::directory_iterator(project.patches_folder)) {
      if (entry.path().extension() == ".bps") {
        bps_files.push_back(entry.path().string());
      }
    }
  } catch (const fs::filesystem_error& e) {
    // Patches folder doesn't exist or not accessible
  }
  
  for (const auto& patch_file : bps_files) {
    std::vector<uint8_t> patch_data;
    auto patch_contents = util::LoadFile(patch_file);
    std::copy(patch_contents.begin(), patch_contents.end(),
              std::back_inserter(patch_data));
    std::vector<uint8_t> patched_rom;
    util::ApplyBpsPatch(rom.vector(), patch_data, patched_rom);
    rom.LoadFromData(patched_rom);
  }

  // Run asar on assembly files - cross-platform
  std::vector<std::string> asm_files;
  try {
    for (const auto& entry : fs::directory_iterator(project.patches_folder)) {
      if (entry.path().extension() == ".asm") {
        asm_files.push_back(entry.path().string());
      }
    }
  } catch (const fs::filesystem_error& e) {
    // No asm files
  }
  
  for (const auto& asm_file : asm_files) {
    AsarPatch asar_patch;
    auto status = asar_patch.Run({asm_file});
    if (!status.ok()) {
      return status;
    }
  }

  std::string output_file = project.name + ".sfc";
  status = rom.SaveToFile({.save_new = true, .filename = output_file});
  if (!status.ok()) {
    return status;
  }

  std::cout << "✅ Successfully built project: " << project.name << std::endl;
  std::cout << "   Output ROM: " << output_file << std::endl;

  return absl::OkStatus();
}

}  // namespace cli
}  // namespace yaze
