#include "cli/handlers/rom/project_bundle_archive_commands.h"

#include <cstring>
#include <filesystem>
#include <fstream>
#include <ios>
#include <string>
#include <vector>

#include "absl/status/status.h"
#include "absl/strings/str_format.h"
#include "miniz.h"

namespace yaze::cli::handlers {

namespace fs = std::filesystem;

namespace {

bool HasTraversalPathComponent(const fs::path& path) {
  for (const auto& component : path) {
    if (component == "..") {
      return true;
    }
  }
  return false;
}

}  // namespace

// ============================================================================
// Pack
// ============================================================================

resources::CommandHandler::Descriptor
ProjectBundlePackCommandHandler::Describe() const {
  Descriptor desc;
  desc.display_name = "Project Bundle Pack";
  desc.summary =
      "Pack a .yazeproj bundle directory into a .zip archive for "
      "cross-platform sharing (Windows/macOS/Linux). Preserves the "
      "bundle root folder name inside the archive.";
  desc.todo_reference = "todo#project-bundle-infra";
  desc.entries = {
      {"--project", "Path to .yazeproj bundle directory (required)", ""},
      {"--out", "Output .zip file path (required)", ""},
      {"--overwrite", "Overwrite existing output file", ""},
  };
  return desc;
}

absl::Status ProjectBundlePackCommandHandler::ValidateArgs(
    const resources::ArgumentParser& parser) {
  auto project = parser.GetString("project");
  if (!project.has_value() || project->empty()) {
    return absl::InvalidArgumentError(
        "project-bundle-pack: --project is required");
  }

  auto out = parser.GetString("out");
  if (!out.has_value() || out->empty()) {
    return absl::InvalidArgumentError(
        "project-bundle-pack: --out is required");
  }
  return absl::OkStatus();
}

absl::Status ProjectBundlePackCommandHandler::Execute(
    Rom* /*rom*/, const resources::ArgumentParser& parser,
    resources::OutputFormatter& formatter) {
  const std::string project_path = *parser.GetString("project");
  const std::string out_path = *parser.GetString("out");
  const bool overwrite = parser.HasFlag("overwrite");

  // Validate input is a .yazeproj directory
  std::error_code fserr;
  fs::path bundle_dir(project_path);
  if (bundle_dir.extension() != ".yazeproj") {
    formatter.AddField("ok", false);
    formatter.AddField("error", std::string("Input must be a .yazeproj directory"));
    return absl::InvalidArgumentError(
        "project-bundle-pack: input must be a .yazeproj directory");
  }
  if (!fs::is_directory(bundle_dir, fserr) || fserr) {
    formatter.AddField("ok", false);
    formatter.AddField("error",
                       absl::StrFormat("Not a directory: %s", project_path));
    return absl::NotFoundError(
        absl::StrFormat("project-bundle-pack: not a directory: %s",
                        project_path));
  }

  // Check output doesn't exist (unless --overwrite)
  if (fs::exists(out_path, fserr) && !overwrite) {
    formatter.AddField("ok", false);
    formatter.AddField("error",
                       std::string("Output file exists (use --overwrite)"));
    return absl::AlreadyExistsError(
        "project-bundle-pack: output file exists; use --overwrite");
  }

  // Collect all files in the bundle
  std::vector<fs::path> entries;
  for (auto it = fs::recursive_directory_iterator(
           bundle_dir, fs::directory_options::skip_permission_denied, fserr);
       it != fs::recursive_directory_iterator(); ++it) {
    if (it->is_regular_file(fserr) && !fserr) {
      entries.push_back(it->path());
    }
  }

  // Create the zip archive using miniz
  mz_zip_archive zip;
  std::memset(&zip, 0, sizeof(zip));

  if (!mz_zip_writer_init_file(&zip, out_path.c_str(), 0)) {
    formatter.AddField("ok", false);
    formatter.AddField("error",
                       absl::StrFormat("Cannot create zip: %s", out_path));
    return absl::InternalError(
        absl::StrFormat("project-bundle-pack: cannot create zip: %s",
                        out_path));
  }

  // Use the bundle directory name as the archive root
  std::string bundle_name = bundle_dir.filename().string();
  int files_added = 0;

  for (const auto& entry : entries) {
    // Compute relative path inside zip: BundleName/relative/path
    fs::path rel = fs::relative(entry, bundle_dir.parent_path(), fserr);
    if (fserr) continue;
    // Use generic_string() for forward-slash paths in zip (cross-platform safe)
    std::string archive_name = rel.generic_string();

    // Read file contents
    std::ifstream infile(entry, std::ios::binary | std::ios::ate);
    if (!infile.is_open()) continue;
    auto file_size = infile.tellg();
    infile.seekg(0, std::ios::beg);
    std::vector<char> buffer(static_cast<size_t>(file_size));
    infile.read(buffer.data(), file_size);

    if (!mz_zip_writer_add_mem(&zip, archive_name.c_str(), buffer.data(),
                                buffer.size(), MZ_DEFAULT_COMPRESSION)) {
      mz_zip_writer_end(&zip);
      fs::remove(out_path, fserr);
      formatter.AddField("ok", false);
      formatter.AddField("error",
                         absl::StrFormat("Failed to add: %s", archive_name));
      return absl::InternalError(
          absl::StrFormat("project-bundle-pack: failed to add: %s",
                          archive_name));
    }
    ++files_added;
  }

  if (!mz_zip_writer_finalize_archive(&zip)) {
    mz_zip_writer_end(&zip);
    fs::remove(out_path, fserr);
    formatter.AddField("ok", false);
    formatter.AddField("error", std::string("Failed to finalize archive"));
    return absl::InternalError(
        "project-bundle-pack: failed to finalize archive");
  }
  mz_zip_writer_end(&zip);

  auto archive_size = fs::file_size(out_path, fserr);

  formatter.AddField("ok", true);
  formatter.AddField("status", std::string("pass"));
  formatter.AddField("archive_path", out_path);
  formatter.AddField("bundle_name", bundle_name);
  formatter.AddField("files_packed", files_added);
  formatter.AddField("archive_bytes",
                     static_cast<int>(fserr ? 0 : archive_size));

  return absl::OkStatus();
}

// ============================================================================
// Unpack
// ============================================================================

resources::CommandHandler::Descriptor
ProjectBundleUnpackCommandHandler::Describe() const {
  Descriptor desc;
  desc.display_name = "Project Bundle Unpack";
  desc.summary =
      "Unpack a .zip archive into a .yazeproj bundle directory. Enforces "
      "path traversal safety (rejects .. and absolute-path entries).";
  desc.todo_reference = "todo#project-bundle-infra";
  desc.entries = {
      {"--archive", "Path to .zip archive (required)", ""},
      {"--out", "Output directory where bundle will be extracted (required)",
       ""},
      {"--overwrite", "Overwrite existing output directory", ""},
      {"--dry-run",
       "Validate archive structure without extracting files", ""},
      {"--keep-partial-output",
       "Keep extracted files on failure (for debugging). Default: clean up.",
       ""},
  };
  return desc;
}

absl::Status ProjectBundleUnpackCommandHandler::ValidateArgs(
    const resources::ArgumentParser& parser) {
  auto archive = parser.GetString("archive");
  if (!archive.has_value() || archive->empty()) {
    return absl::InvalidArgumentError(
        "project-bundle-unpack: --archive is required");
  }

  auto out = parser.GetString("out");
  if (!out.has_value() || out->empty()) {
    return absl::InvalidArgumentError(
        "project-bundle-unpack: --out is required");
  }
  return absl::OkStatus();
}

absl::Status ProjectBundleUnpackCommandHandler::Execute(
    Rom* /*rom*/, const resources::ArgumentParser& parser,
    resources::OutputFormatter& formatter) {
  const std::string archive_path = *parser.GetString("archive");
  const std::string out_dir = *parser.GetString("out");
  const bool overwrite = parser.HasFlag("overwrite");
  const bool keep_partial = parser.HasFlag("keep-partial-output");
  const bool dry_run = parser.HasFlag("dry-run");

  std::error_code fserr;
  auto add_cleanup_field = [&]() {
    if (dry_run) {
      formatter.AddField("cleanup", std::string("skipped (dry-run)"));
      return;
    }
    if (keep_partial) {
      formatter.AddField("cleanup",
                         std::string("skipped (--keep-partial-output)"));
      return;
    }
    std::error_code cleanup_err;
    if (!fs::exists(out_dir, cleanup_err) || cleanup_err) {
      formatter.AddField(
          "cleanup",
          cleanup_err
              ? std::string("failed: " + cleanup_err.message())
              : std::string("not-needed"));
      return;
    }
    fs::remove_all(out_dir, cleanup_err);
    formatter.AddField("cleanup", cleanup_err
                                   ? std::string("failed: " +
                                                 cleanup_err.message())
                                   : std::string("removed"));
  };

  // Validate archive exists
  if (!fs::exists(archive_path, fserr) || fserr) {
    formatter.AddField("ok", false);
    formatter.AddField("error",
                       absl::StrFormat("Archive not found: %s", archive_path));
    return absl::NotFoundError(
        absl::StrFormat("project-bundle-unpack: archive not found: %s",
                        archive_path));
  }

  // Check output directory
  if (fs::exists(out_dir, fserr) && !overwrite) {
    formatter.AddField("ok", false);
    formatter.AddField("error",
                       std::string("Output exists (use --overwrite)"));
    return absl::AlreadyExistsError(
        "project-bundle-unpack: output exists; use --overwrite");
  }

  // When --overwrite, remove stale content before extraction to prevent
  // leftover files from a previous unpack polluting the result.
  if (overwrite && !dry_run && fs::exists(out_dir, fserr)) {
    fs::remove_all(out_dir, fserr);
  }

  // Open zip for reading
  mz_zip_archive zip;
  std::memset(&zip, 0, sizeof(zip));

  if (!mz_zip_reader_init_file(&zip, archive_path.c_str(), 0)) {
    formatter.AddField("ok", false);
    formatter.AddField("error",
                       absl::StrFormat("Cannot open zip: %s", archive_path));
    return absl::InternalError(
        absl::StrFormat("project-bundle-unpack: cannot open zip: %s",
                        archive_path));
  }

  int num_files = static_cast<int>(mz_zip_reader_get_num_files(&zip));
  int files_counted = 0;
  int files_extracted = 0;
  std::string detected_bundle_name;
  bool mixed_roots = false;
  bool has_project_yaze = false;  // Tracks if any entry is <root>/project.yaze

  for (int idx = 0; idx < num_files; ++idx) {
    mz_zip_archive_file_stat file_stat;
    if (!mz_zip_reader_file_stat(&zip, static_cast<mz_uint>(idx),
                                  &file_stat)) {
      continue;
    }

    std::string entry_name(file_stat.m_filename);

    // Skip directory entries
    if (mz_zip_reader_is_file_a_directory(
            &zip, static_cast<mz_uint>(idx))) {
      continue;
    }

    // ---- Path traversal safety ----
    // Reject entries with explicit ".." path components.
    if (HasTraversalPathComponent(fs::path(entry_name))) {
      mz_zip_reader_end(&zip);
      formatter.AddField("ok", false);
      formatter.AddField("error",
                         absl::StrFormat("Path traversal rejected: %s",
                                        entry_name));
      add_cleanup_field();
      return absl::FailedPreconditionError(
          absl::StrFormat(
              "project-bundle-unpack: path traversal rejected: %s",
              entry_name));
    }
    // Reject absolute paths
    if (fs::path(entry_name).is_absolute()) {
      mz_zip_reader_end(&zip);
      formatter.AddField("ok", false);
      formatter.AddField("error",
                         absl::StrFormat("Absolute path rejected: %s",
                                        entry_name));
      add_cleanup_field();
      return absl::FailedPreconditionError(
          absl::StrFormat(
              "project-bundle-unpack: absolute path rejected: %s",
              entry_name));
    }

    // Detect bundle root name and enforce a single-root archive.
    {
      fs::path entry_path(entry_name);
      auto root_component = *entry_path.begin();
      if (detected_bundle_name.empty()) {
        detected_bundle_name = root_component.string();
      } else if (detected_bundle_name != root_component.string()) {
        mixed_roots = true;
      }
      // Check for project.yaze directly under the root
      fs::path relative_to_root = entry_path.lexically_relative(root_component);
      if (relative_to_root == "project.yaze") {
        has_project_yaze = true;
      }
    }

    ++files_counted;

    // In dry-run mode, validate entries but don't write files.
    if (dry_run) {
      continue;
    }

    // Construct output path and extract file
    fs::path dest = fs::path(out_dir) / entry_name;
    fs::create_directories(dest.parent_path(), fserr);

    size_t uncomp_size = 0;
    void* data = mz_zip_reader_extract_to_heap(
        &zip, static_cast<mz_uint>(idx), &uncomp_size, 0);
    if (!data) {
      mz_zip_reader_end(&zip);
      formatter.AddField("ok", false);
      formatter.AddField("error",
                         absl::StrFormat("Failed to extract: %s", entry_name));
      add_cleanup_field();
      return absl::InternalError(
          absl::StrFormat("project-bundle-unpack: failed to extract: %s",
                          entry_name));
    }

    std::ofstream outfile(dest, std::ios::binary | std::ios::trunc);
    if (!outfile.is_open()) {
      mz_free(data);
      mz_zip_reader_end(&zip);
      formatter.AddField("ok", false);
      formatter.AddField("error",
                         absl::StrFormat("Cannot write: %s", dest.string()));
      add_cleanup_field();
      return absl::InternalError(
          absl::StrFormat("project-bundle-unpack: cannot write: %s",
                          dest.string()));
    }
    outfile.write(static_cast<const char*>(data), uncomp_size);
    outfile.close();
    mz_free(data);
    ++files_extracted;
  }

  mz_zip_reader_end(&zip);

  // Determine extracted bundle path
  std::string bundle_path;
  if (!detected_bundle_name.empty()) {
    bundle_path = (fs::path(out_dir) / detected_bundle_name).string();
  }

  // Check if result is (or would be) a valid .yazeproj
  bool is_valid_bundle = false;
  if (!bundle_path.empty()) {
    fs::path bp(bundle_path);
    if (dry_run) {
      // Structural dry-run: root must end in .yazeproj AND archive must
      // contain project.yaze under that root.
      is_valid_bundle =
          bp.extension() == ".yazeproj" && has_project_yaze && !mixed_roots;
    } else {
      is_valid_bundle =
          bp.extension() == ".yazeproj" && fs::is_directory(bp, fserr) &&
          has_project_yaze && !mixed_roots;
    }
  }

  formatter.AddField("ok", is_valid_bundle);
  formatter.AddField("status",
                     is_valid_bundle ? std::string("pass")
                                     : std::string("fail"));
  formatter.AddField("archive_path", archive_path);
  formatter.AddField("output_directory", out_dir);
  formatter.AddField("bundle_path", bundle_path);
  formatter.AddField("bundle_name", detected_bundle_name);
  formatter.AddField("files_counted", files_counted);
  formatter.AddField("files_extracted", files_extracted);
  formatter.AddField("is_valid_bundle", is_valid_bundle);
  formatter.AddField("dry_run", dry_run);

  if (!is_valid_bundle) {
    if (mixed_roots) {
      formatter.AddField("error",
                         std::string("Archive contains multiple root folders"));
    } else if (!has_project_yaze) {
      formatter.AddField(
          "error",
          std::string("Bundle is missing required root file: project.yaze"));
    }
    // Clean up partial output unless --keep-partial-output is set.
    add_cleanup_field();
    return absl::FailedPreconditionError(
        "project-bundle-unpack: extracted archive is not a valid "
        ".yazeproj bundle");
  }
  return absl::OkStatus();
}

}  // namespace yaze::cli::handlers
