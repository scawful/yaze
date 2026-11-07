#include "cli/handlers/rom/project_commands.h"

#include <filesystem>
#include <fstream>
#include <iostream>

#include "core/asar_wrapper.h"
#include "core/project.h"
#include "core/project_native_import.h"
#include "util/bps.h"
#include "util/file_util.h"
#include "util/macro.h"

namespace yaze {
namespace cli {
namespace handlers {

resources::CommandHandler::Descriptor
ProjectAdoptNativeOverworldCommandHandler::Describe() const {
  Descriptor descriptor;
  descriptor.display_name = "Import Native Special Overworld";
  descriptor.summary =
      "Validate a complete initial-import image against its saved project, "
      "terrain and staged engine. Dry-run by default; --write publishes the "
      "ROM, descriptor and bundle hash together. Reopen and Save in yaze "
      "before Build. Does not compile, merge source or qualify gameplay.";
  descriptor.entries = {
      {"--project", "Saved .yaze descriptor or .yazeproj bundle", ""},
      {"--candidate", "Complete native ROM prepared by the importer", ""},
      {"--import-receipt", "Hash-bound initial-import receipt", ""},
      {"--expect-project-sha256", "Exact saved descriptor SHA256", ""},
      {"--write", "Publish after validation (default: dry run)", ""},
      {"--code-folder",
       "Separately staged engine source; never copied or merged", ""},
      {"--build-command", "Build command for the staged source adapter", ""},
      {"--build-target", "Compiled ROM output for the staged source", ""},
      {"--asm-entry", "Assembly entry point in the staged source", ""},
      {"--hack-manifest",
       "Staged manifest with matching development ROM/output", ""},
  };
  return descriptor;
}

absl::Status ProjectAdoptNativeOverworldCommandHandler::ValidateArgs(
    const resources::ArgumentParser& parser) {
  return parser.RequireArgs(
      {"project", "candidate", "import-receipt", "expect-project-sha256"});
}

absl::Status ProjectAdoptNativeOverworldCommandHandler::Execute(
    Rom*, const resources::ArgumentParser& parser,
    resources::OutputFormatter& formatter) {
  project::YazeProject project;
  RETURN_IF_ERROR(project.Open(*parser.GetString("project")));
  project::NativeOverworldImportBuildSettings build;
  build.code_folder = parser.GetString("code-folder").value_or("");
  build.build_script = parser.GetString("build-command").value_or("");
  build.build_target = parser.GetString("build-target").value_or("");
  build.asm_entry_point = parser.GetString("asm-entry").value_or("");
  build.hack_manifest_file = parser.GetString("hack-manifest").value_or("");
  ASSIGN_OR_RETURN(auto plan,
                   project::PrepareNativeOverworldProjectImport(
                       project, *parser.GetString("candidate"),
                       *parser.GetString("import-receipt"),
                       *parser.GetString("expect-project-sha256"), build));
  const bool publish = parser.HasFlag("write");
  if (publish) {
    RETURN_IF_ERROR(plan.Publish().status());
  }
  formatter.AddField("status", publish ? "adopted" : "ready");
  formatter.AddField("dry_run", !publish);
  formatter.AddField("project", project.filepath);
  formatter.AddField("rom", project.rom_filename);
  formatter.AddField("image_sha256", plan.image_sha256);
  formatter.AddField("requires_editor_save", true);
  formatter.AddField("runtime_qualified", false);
  return absl::OkStatus();
}

absl::Status ProjectInitCommandHandler::Execute(
    Rom* rom, const resources::ArgumentParser& parser,
    resources::OutputFormatter& formatter) {
  auto project_opt = parser.GetString("project_name");

  if (!project_opt.has_value()) {
    return absl::InvalidArgumentError(
        "Missing required argument: project_name");
  }

  std::string project_name = project_opt.value();

  project::YazeProject project;
  auto status = project.Create(project_name, ".");
  if (!status.ok()) {
    return status;
  }

  formatter.AddField("status", "success");
  formatter.AddField("message",
                     "Successfully initialized project: " + project_name);
  formatter.AddField("project_name", project_name);

  return absl::OkStatus();
}

absl::Status ProjectBuildCommandHandler::Execute(
    Rom* rom, const resources::ArgumentParser& parser,
    resources::OutputFormatter& formatter) {
  project::YazeProject project;
  auto status = project.Open(".");
  if (!status.ok()) {
    return status;
  }
  if (project.rom_metadata.write_policy == project::RomWritePolicy::kReadOnly) {
    return absl::PermissionDeniedError(
        "Project builds are disabled by the project's read-only policy");
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
    std::ifstream patch_stream(patch_file, std::ios::binary);
    if (!patch_stream.is_open()) {
      return absl::InternalError("Failed to open BPS patch: " + patch_file);
    }

    std::vector<uint8_t> patch_data(
        (std::istreambuf_iterator<char>(patch_stream)),
        std::istreambuf_iterator<char>());
    if (patch_data.empty()) {
      return absl::InvalidArgumentError("BPS patch is empty: " + patch_file);
    }

    std::vector<uint8_t> patched_rom;
    auto apply_status =
        util::ApplyBpsPatch(build_rom.vector(), patch_data, patched_rom);
    if (!apply_status.ok()) {
      return absl::InternalError("Failed to apply BPS patch " + patch_file +
                                 ": " + std::string(apply_status.message()));
    }

    auto load_patched_status = build_rom.LoadFromData(patched_rom);
    if (!load_patched_status.ok()) {
      return absl::InternalError("Failed to load patched ROM after " +
                                 patch_file + ": " +
                                 std::string(load_patched_status.message()));
    }
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

  // Apply ASM patches using Asar
  if (!asm_files.empty()) {
    core::AsarWrapper asar;
    auto init_status = asar.Initialize();
    if (!init_status.ok()) {
      formatter.AddField("warning",
                         "Asar not available, skipping ASM patches: " +
                             std::string(init_status.message()));
    } else {
      for (const auto& asm_file : asm_files) {
        auto rom_data = build_rom.vector();
        auto result = asar.ApplyPatch(asm_file, rom_data);

        if (!result.ok()) {
          return absl::InternalError("ASM patch failed for " + asm_file + ": " +
                                     std::string(result.status().message()));
        }

        if (result->success) {
          build_rom.LoadFromData(rom_data);
          formatter.AddField("asm_applied", asm_file);

          // Log extracted symbols count
          if (!result->symbols.empty()) {
            formatter.AddField(
                "symbols_" + fs::path(asm_file).stem().string(),
                std::to_string(result->symbols.size()) + " symbols");
          }
        } else {
          // Log errors but continue with other patches
          std::string error_msg = "Errors in " + asm_file + ":";
          for (const auto& error : result->errors) {
            error_msg += "\n  " + error;
          }
          formatter.AddField("error", error_msg);
          return absl::InternalError(error_msg);
        }
      }
    }
  }

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
