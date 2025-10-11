#include "cli/handlers/rom/project_commands.h"

#include "app/core/project.h"
#include "util/file_util.h"
#include "util/bps.h"
#include "util/macro.h"
#include <filesystem>
#include <iostream>

namespace yaze {
namespace cli {
namespace handlers {

absl::Status ProjectInitCommandHandler::Execute(Rom* rom, 
                                               const resources::ArgumentParser& parser,
                                               resources::OutputFormatter& formatter) {
  auto project_opt = parser.GetString("project_name");
  
  if (!project_opt.has_value()) {
    return absl::InvalidArgumentError("Missing required argument: project_name");
  }
  
  std::string project_name = project_opt.value();
  
  core::YazeProject project;
  auto status = project.Create(project_name, ".");
  if (!status.ok()) {
    return status;
  }

  formatter.AddField("status", "success");
  formatter.AddField("message", "Successfully initialized project: " + project_name);
  formatter.AddField("project_name", project_name);
  
  return absl::OkStatus();
}

absl::Status ProjectBuildCommandHandler::Execute(Rom* rom, 
                                                const resources::ArgumentParser& parser,
                                                resources::OutputFormatter& formatter) {
  core::YazeProject project;
  auto status = project.Open(".");
  if (!status.ok()) {
    return status;
  }

  Rom build_rom;
  status = build_rom.LoadFromFile(project.rom_filename);
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
    util::ApplyBpsPatch(build_rom.vector(), patch_data, patched_rom);
    build_rom.LoadFromData(patched_rom);
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
  
  // TODO: Implement ASM patching functionality
  // for (const auto& asm_file : asm_files) {
  //   // Apply ASM patches here
  // }

  std::string output_file = project.name + ".sfc";
  status = build_rom.SaveToFile({.save_new = true, .filename = output_file});
  if (!status.ok()) {
    return status;
  }

  formatter.AddField("status", "success");
  formatter.AddField("message", "Successfully built project: " + project.name);
  formatter.AddField("project_name", project.name);
  formatter.AddField("output_file", output_file);
  
  return absl::OkStatus();
}

}  // namespace handlers
}  // namespace cli
}  // namespace yaze
