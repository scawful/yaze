#include "core/project_native_import.h"

#include <filesystem>
#include <fstream>
#include <iterator>
#include <map>
#include <utility>

#include "rom/lyra_layout_descriptor.h"
#include "util/json.h"
#include "util/macro.h"
#include "util/rom_hash.h"

namespace yaze::project {
namespace {
namespace fs = std::filesystem;

absl::StatusOr<std::string> ReadImportFile(const fs::path& path) {
  std::error_code error;
  const auto size = fs::file_size(path, error);
  if (error || !fs::is_regular_file(path, error) || error ||
      size > 16 * 1024 * 1024) {
    return absl::FailedPreconditionError(
        "Native import requires bounded regular input files: " + path.string());
  }
  std::ifstream input(path, std::ios::binary);
  if (!input)
    return absl::FailedPreconditionError("Cannot read native import input: " +
                                         path.string());
  std::string bytes{std::istreambuf_iterator<char>(input), {}};
  if (input.bad() || bytes.size() != size)
    return absl::AbortedError("Native import input changed while reading: " +
                              path.string());
  return bytes;
}

absl::Status RequireImport(bool condition, const std::string& reason) {
  return condition ? absl::OkStatus()
                   : absl::FailedPreconditionError("Native import: " + reason);
}
}  // namespace

absl::StatusOr<std::string> NativeOverworldProjectImportPlan::Publish(
    core::SourceArtifactReadbackValidator validator) {
  return project.SaveWithSourceArtifacts(updates, std::move(validator),
                                         expected_descriptor);
}

absl::StatusOr<NativeOverworldProjectImportPlan>
PrepareNativeOverworldProjectImport(
    const YazeProject& project, const std::string& prepared_rom,
    const std::string& import_receipt,
    const std::string& expected_descriptor_sha256,
    const NativeOverworldImportBuildSettings& build) {
#if !defined(YAZE_WITH_JSON) || defined(__EMSCRIPTEN__)
  return absl::UnimplementedError(
      "Native project import requires desktop JSON publication support");
#else
  RETURN_IF_ERROR(RequireImport(
      project.project_opened() && !project.filepath.empty() &&
          !project.rom_filename.empty() && !project.lyra_terrain_file.empty(),
      "a saved project with a legacy terrain source is required"));
  RETURN_IF_ERROR(RequireImport(
      project.rom_metadata.write_policy != RomWritePolicy::kReadOnly,
      "the project is read-only"));
  RETURN_IF_ERROR(
      RequireImport(!project.native_save_producer_file.empty() &&
                        !project.native_save_transfer_proof_file.empty(),
                    "retained producer and transfer references must require a "
                    "fresh native Save"));
  NativeOverworldProjectImportPlan plan;
  plan.project = project;
  ASSIGN_OR_RETURN(plan.expected_descriptor, ReadImportFile(project.filepath));
  RETURN_IF_ERROR(RequireImport(
      !expected_descriptor_sha256.empty() &&
          core::ComputeSourceArtifactSha256(plan.expected_descriptor) ==
              expected_descriptor_sha256,
      "saved project descriptor does not match the selected identity"));
  RETURN_IF_ERROR(
      project.ValidateSavedSourceBindings(plan.expected_descriptor));

  // Guard every input in the same publication transaction, including the
  // preparation receipt and separately staged engine sources. Aliases between
  // protected inputs and outputs are rejected by the existing publisher.
  std::map<fs::path, std::string> guarded;
  const auto guard = [&](const fs::path& path) -> absl::StatusOr<std::string> {
    const auto resolved = fs::weakly_canonical(path);
    if (const auto existing = guarded.find(resolved); existing != guarded.end())
      return existing->second;
    ASSIGN_OR_RETURN(auto bytes, ReadImportFile(path));
    plan.updates.push_back({fs::absolute(path), bytes, bytes});
    guarded.emplace(resolved, bytes);
    return bytes;
  };
  const auto source_path = project.GetAbsolutePath(project.rom_filename);
  const auto terrain_path = project.GetAbsolutePath(project.lyra_terrain_file);
  ASSIGN_OR_RETURN(const auto source, ReadImportFile(source_path));
  ASSIGN_OR_RETURN(const auto terrain, guard(terrain_path));
  ASSIGN_OR_RETURN(const auto image, guard(prepared_rom));
  ASSIGN_OR_RETURN(const auto receipt_text, guard(import_receipt));
  const auto* bytes = reinterpret_cast<const uint8_t*>(image.data());
  const auto layout = rom::InspectLyraLayout(bytes, image.size());
  RETURN_IF_ERROR(RequireImport(
      rom::IsNativeSpecialOverworldLayout(layout) && layout.revision == 4 &&
          layout.capacity == 192 && layout.has_authoring_metadata &&
          layout.active_count == 192,
      "candidate must be a complete native192 image: " + layout.error));
  plan.image_sha256 = core::ComputeSourceArtifactSha256(image);

  try {
    const auto receipt = Json::parse(receipt_text);
    const auto& inspection = receipt.at("inspection");
    const auto projection_hash =
        inspection.at("native_source_sha256").get<std::string>();
    RETURN_IF_ERROR(RequireImport(
        inspection.at("validated_maps").get<int>() == 192 &&
            inspection.at("original_terrain_maps_preserved").get<int>() ==
                160 &&
            inspection.at("checked_tail_terrain_maps").get<int>() == 32 &&
            inspection.at("checked_tail_asset_states").get<int>() == 96 &&
            inspection.at("gameplay_area_count").get<int>() > 0 &&
            inspection.at("gameplay_area_count") ==
                receipt.at("gameplay_area_count") &&
            projection_hash.size() == 64 &&
            projection_hash.find_first_not_of("0123456789abcdef") ==
                std::string::npos,
        "complete final terrain and asset inspection is required"));
    RETURN_IF_ERROR(RequireImport(
        receipt.at("complete_image").get<bool>() &&
            receipt.at("original_geometry_preserved").get<bool>() &&
            receipt.at("original_stream_pools_preserved").get<bool>() &&
            receipt.at("appearance_activation_ready").get<bool>() &&
            receipt.at("capacity").get<int>() == layout.capacity &&
            receipt.at("descriptor_revision").get<int>() == layout.revision &&
            receipt.at("byte_count").get<size_t>() == image.size() &&
            receipt.at("native_payload_count").get<size_t>() ==
                layout.entries.size() &&
            receipt.at("source_rom_sha256").get<std::string>() ==
                core::ComputeSourceArtifactSha256(source) &&
            receipt.at("source_terrain_sha256").get<std::string>() ==
                core::ComputeSourceArtifactSha256(terrain) &&
            receipt.at("image_sha256").get<std::string>() == plan.image_sha256,
        "preparation receipt is incomplete or belongs to different inputs"));

    const auto code = fs::weakly_canonical(project.GetAbsolutePath(
        build.code_folder.empty() ? project.code_folder : build.code_folder));
    const auto& sources = receipt.at("engine").at("source_files");
    RETURN_IF_ERROR(RequireImport(sources.is_object() && !sources.empty(),
                                  "engine source bindings are missing"));
    for (const auto& [relative, hash] : sources.items()) {
      const fs::path member(relative);
      const auto path = fs::weakly_canonical(code / member);
      const auto within = path.lexically_relative(code);
      RETURN_IF_ERROR(RequireImport(
          !member.is_absolute() && !within.empty() && *within.begin() != "..",
          "engine source escapes its selected folder"));
      ASSIGN_OR_RETURN(const auto contents, guard(path));
      RETURN_IF_ERROR(RequireImport(
          core::ComputeSourceArtifactSha256(contents) ==
              hash.get<std::string>(),
          "engine source differs from the prepared image: " + relative));
    }
    if (!build.code_folder.empty()) {
      const auto target =
          fs::weakly_canonical(project.GetAbsolutePath(build.build_target));
      const auto relative = target.lexically_relative(code / "Roms");
      RETURN_IF_ERROR(RequireImport(
          !relative.empty() && !relative.is_absolute() &&
              *relative.begin() != ".." && relative.parent_path().empty(),
          "refreshed build output must belong to the selected source"));
      for (const auto& path :
           {code / "Scripts/Build/build_rom.sh",
            code / "Scripts/Build/authoring_bundle_build.py",
            fs::path(project.GetAbsolutePath(build.asm_entry_point))}) {
        ASSIGN_OR_RETURN(const auto protected_source, guard(path));
        (void)protected_source;
      }
    }

    const auto manifest_ref = build.hack_manifest_file.empty()
                                  ? project.hack_manifest_file
                                  : build.hack_manifest_file;
    if (!manifest_ref.empty()) {
      const auto path = project.GetAbsolutePath(manifest_ref);
      ASSIGN_OR_RETURN(const auto before, ReadImportFile(path));
      auto manifest = Json::parse(before);
      core::HackManifest parsed;
      RETURN_IF_ERROR(parsed.LoadFromString(before));
      const auto& pipeline = parsed.build_pipeline();
      const auto target = project.GetAbsolutePath(build.code_folder.empty()
                                                      ? project.build_target
                                                      : build.build_target);
      RETURN_IF_ERROR(RequireImport(
          !pipeline.dev_rom.empty() &&
              fs::weakly_canonical(project.GetAbsolutePath(pipeline.dev_rom)) ==
                  fs::weakly_canonical(source_path) &&
              !pipeline.patched_rom.empty() &&
              fs::weakly_canonical(project.GetAbsolutePath(
                  pipeline.patched_rom)) == fs::weakly_canonical(target),
          "hack manifest development ROM and output must match the adopted "
          "project"));
      // The existing manifest remains the writer-policy authority. Refresh its
      // ROM identity while preserving all ownership, allocation and source data.
      manifest["rom"]["dev_rom_sha1"] =
          util::ComputeSha1Hex(bytes, image.size());
      manifest["rom"]["dev_rom_size"] = image.size();
      const auto after = manifest.dump(2) + "\n";
      RETURN_IF_ERROR(plan.project.hack_manifest.LoadFromString(after));
      plan.project.hack_manifest_file = path;
      plan.updates.push_back({path, before, after});
    } else {
      RETURN_IF_ERROR(
          RequireImport(!project.hack_manifest.loaded(),
                        "loaded hack manifest needs a persisted binding"));
    }

    const auto bundle = YazeProject::ResolveBundleRoot(project.filepath);
    if (!bundle.empty()) {
      const auto manifest_path = fs::path(bundle) / "manifest.json";
      ASSIGN_OR_RETURN(const auto before, ReadImportFile(manifest_path));
      auto manifest = Json::parse(before);
      RETURN_IF_ERROR(RequireImport(manifest.is_object(),
                                    "bundle manifest must be an object"));
      const auto sha1 = util::ComputeSha1Hex(bytes, image.size());
      manifest["romChecksum"] = sha1;
      if (manifest.contains("rom_sha1"))
        manifest["rom_sha1"] = sha1;
      plan.updates.push_back({manifest_path, before, manifest.dump(2) + "\n"});
    }
  } catch (const std::exception& error) {
    return absl::FailedPreconditionError(
        std::string("Native import metadata is invalid: ") + error.what());
  }

  // The ROM stays at its established backing path. This avoids leaving the
  // project dependent on a scratch candidate and preserves strict dev bindings.
  plan.updates.push_back({source_path, source, image});
  plan.project.rom_metadata.expected_hash =
      util::ComputeRomHash(bytes, image.size());
  plan.project.lyra_terrain_file.clear();
  plan.project.lyra_build_request_file.clear();
  if (!build.code_folder.empty()) {
    RETURN_IF_ERROR(RequireImport(
        !build.build_script.empty() && !build.build_target.empty() &&
            !build.asm_entry_point.empty(),
        "source refresh needs complete build bindings"));
    plan.project.code_folder = project.GetAbsolutePath(build.code_folder);
    plan.project.build_script = build.build_script;
    plan.project.build_target = project.GetAbsolutePath(build.build_target);
    plan.project.output_folder =
        fs::path(plan.project.build_target).parent_path().string();
    plan.project.asm_entry_point =
        project.GetAbsolutePath(build.asm_entry_point);
    plan.project.git_repository = plan.project.code_folder;
  }
  std::vector<fs::path> targets{project.filepath};
  for (const auto& update : plan.updates)
    targets.push_back(update.target);
  RETURN_IF_ERROR(core::ValidateSourceArtifactPublicationTargets(targets));
  return plan;
#endif
}
}  // namespace yaze::project
