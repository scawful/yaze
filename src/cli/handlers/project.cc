#include "cli/z3ed.h"
#include "app/core/project.h"
#include "core/platform/file_dialog.h"
#include "util/bps.h"
#include <glob.h>

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

  std::cout << "✅ Successfully initialized project: " << project_name << std::endl;

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

  // Apply BPS patches
  glob_t glob_result;
  std::string pattern = project.patches_folder + "/*.bps";
  glob(pattern.c_str(), GLOB_TILDE, NULL, &glob_result);
  for(unsigned int i=0; i<glob_result.gl_pathc; ++i){
    std::string patch_file = glob_result.gl_pathv[i];
    std::vector<uint8_t> patch_data;
    auto patch_contents = core::LoadFile(patch_file);
    std::copy(patch_contents.begin(), patch_contents.end(), std::back_inserter(patch_data));
    std::vector<uint8_t> patched_rom;
    util::ApplyBpsPatch(rom.vector(), patch_data, patched_rom);
    rom.LoadFromData(patched_rom);
  }

  // Run asar on assembly files
  glob_t glob_result_asm;
  std::string pattern_asm = project.patches_folder + "/*.asm";
  glob(pattern_asm.c_str(), GLOB_TILDE, NULL, &glob_result_asm);
  for(unsigned int i=0; i<glob_result_asm.gl_pathc; ++i){
      AsarPatch asar_patch;
      auto status = asar_patch.Run({glob_result_asm.gl_pathv[i]});
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

