#include "cli/handlers/rom/project_bundle_verify_commands.h"

#include <cctype>
#include <filesystem>
#include <fstream>
#include <ios>
#include <string>
#include <vector>

#include "absl/status/status.h"
#include "absl/strings/str_format.h"
#include "core/project.h"
#include "nlohmann/json.hpp"
#include "util/rom_hash.h"

namespace yaze::cli::handlers {

namespace fs = std::filesystem;

namespace {

// Normalize a hex hash string: trim whitespace, lowercase.
std::string NormalizeHash(const std::string& hash) {
  std::string result;
  result.reserve(hash.size());
  for (char ch : hash) {
    if (ch == ' ' || ch == '\t' || ch == '\n' || ch == '\r') continue;
    result += static_cast<char>(std::tolower(static_cast<unsigned char>(ch)));
  }
  return result;
}

struct CheckResult {
  std::string name;
  std::string status;  // "pass" | "warn" | "fail"
  std::string detail;
};

bool HasAbsolutePaths(const project::YazeProject& proj) {
  auto is_abs = [](const std::string& p) {
    return !p.empty() && fs::path(p).is_absolute();
  };
  return is_abs(proj.rom_filename) || is_abs(proj.code_folder) ||
         is_abs(proj.assets_folder) || is_abs(proj.patches_folder) ||
         is_abs(proj.labels_filename) || is_abs(proj.custom_objects_folder);
}

std::vector<std::string> ListAbsolutePaths(
    const project::YazeProject& proj) {
  std::vector<std::string> results;
  auto check = [&](const std::string& field_name, const std::string& value) {
    if (!value.empty() && fs::path(value).is_absolute()) {
      results.push_back(
          absl::StrFormat("%s = %s", field_name, value));
    }
  };
  check("rom_filename", proj.rom_filename);
  check("code_folder", proj.code_folder);
  check("assets_folder", proj.assets_folder);
  check("patches_folder", proj.patches_folder);
  check("labels_filename", proj.labels_filename);
  check("custom_objects_folder", proj.custom_objects_folder);
  return results;
}

}  // namespace

resources::CommandHandler::Descriptor
ProjectBundleVerifyCommandHandler::Describe() const {
  Descriptor desc;
  desc.display_name = "Project Bundle Verify";
  desc.summary =
      "Verify the structural integrity and portability of a .yaze project "
      "file or .yazeproj bundle directory. Checks path existence, config "
      "parsing, reference sanity, and ROM accessibility.";
  desc.todo_reference = "todo#project-bundle-infra";
  desc.entries = {
      {"--project",
       "Path to .yaze project file or .yazeproj bundle directory (required)",
       ""},
      {"--check-rom-hash",
       "Verify ROM SHA1 hash against manifest.json (if present in bundle)",
       ""},
      {"--report",
       "Write full JSON summary to this path in addition to stdout", ""},
  };
  return desc;
}

absl::Status ProjectBundleVerifyCommandHandler::ValidateArgs(
    const resources::ArgumentParser& parser) {
  auto project_path = parser.GetString("project");
  if (!project_path.has_value() || project_path->empty()) {
    return absl::InvalidArgumentError(
        "project-bundle-verify: --project is required");
  }

  // Probe --report path writability.
  if (auto rp = parser.GetString("report");
      rp.has_value() && !rp->empty()) {
    const fs::path rp_path(*rp);
    std::error_code ec;
    const bool existed_before = fs::exists(rp_path, ec);
    std::ofstream probe(*rp, std::ios::out | std::ios::binary | std::ios::app);
    if (!probe.is_open()) {
      return absl::PermissionDeniedError(absl::StrFormat(
          "project-bundle-verify: cannot open report file for writing: %s",
          *rp));
    }
    probe.close();
    if (!existed_before) {
      fs::remove(rp_path, ec);
    }
  }
  return absl::OkStatus();
}

absl::Status ProjectBundleVerifyCommandHandler::Execute(
    Rom* /*rom*/, const resources::ArgumentParser& parser,
    resources::OutputFormatter& formatter) {
  const std::string project_path = *parser.GetString("project");
  std::vector<CheckResult> checks;
  bool any_fail = false;

  // ------------------------------------------------------------------
  // Check 1: Path exists
  // ------------------------------------------------------------------
  {
    std::error_code ec;
    bool exists = fs::exists(project_path, ec);
    if (!exists || ec) {
      checks.push_back(
          {"path_exists", "fail",
           absl::StrFormat("Path does not exist: %s", project_path)});
      any_fail = true;
    } else {
      checks.push_back({"path_exists", "pass", project_path});
    }
  }

  // ------------------------------------------------------------------
  // Check 2: Recognized format
  // ------------------------------------------------------------------
  std::string resolved_path = project_path;
  bool is_bundle = false;
  {
    fs::path fsp(project_path);
    std::string ext = fsp.extension().string();

    // Try bundle root resolution (handles paths inside bundles too)
    std::string bundle_root =
        project::YazeProject::ResolveBundleRoot(project_path);
    if (!bundle_root.empty()) {
      resolved_path = bundle_root;
      is_bundle = true;
    } else if (ext == ".yazeproj") {
      // Accept .yazeproj by extension (directory may or may not exist yet)
      resolved_path = project_path;
      is_bundle = true;
    } else if (ext != ".yaze" && ext != ".zsproj") {
      checks.push_back(
          {"format_recognized", "fail",
           absl::StrFormat("Unrecognized project format: %s", ext)});
      any_fail = true;
    }

    if (!any_fail) {
      checks.push_back(
          {"format_recognized", "pass",
           is_bundle ? "yazeproj bundle" : absl::StrFormat("file (%s)", ext)});
    }
  }

  // ------------------------------------------------------------------
  // Check 3: Bundle structure (yazeproj only)
  // ------------------------------------------------------------------
  if (is_bundle && !any_fail) {
    fs::path bundle_dir(resolved_path);
    fs::path project_yaze = bundle_dir / "project.yaze";
    std::error_code ec;
    if (fs::exists(project_yaze, ec) && !ec) {
      checks.push_back({"bundle_project_yaze", "pass",
                         "project.yaze found in bundle root"});
    } else {
      checks.push_back(
          {"bundle_project_yaze", "warn",
           "project.yaze missing — will be auto-created on open"});
    }
  }

  // ------------------------------------------------------------------
  // Check 4: Project parses
  // ------------------------------------------------------------------
  project::YazeProject proj;
  bool parse_ok = false;
  if (!any_fail) {
    auto status = proj.Open(resolved_path);
    if (status.ok()) {
      parse_ok = true;
      checks.push_back(
          {"project_parses", "pass",
           absl::StrFormat("name=%s", proj.name)});
    } else {
      checks.push_back(
          {"project_parses", "fail",
           absl::StrFormat("Parse failed: %s",
                           std::string(status.message()))});
      any_fail = true;
    }
  }

  // ------------------------------------------------------------------
  // Check 5: Path portability (warnings for absolute paths)
  // ------------------------------------------------------------------
  if (parse_ok) {
    if (HasAbsolutePaths(proj)) {
      auto abs_paths = ListAbsolutePaths(proj);
      std::string detail = "Absolute paths reduce portability: ";
      for (size_t i = 0; i < abs_paths.size(); ++i) {
        if (i > 0) detail += "; ";
        detail += abs_paths[i];
      }
      checks.push_back({"path_portability", "warn", detail});
    } else {
      checks.push_back(
          {"path_portability", "pass", "All paths are relative"});
    }
  }

  // ------------------------------------------------------------------
  // Check 6: ROM accessibility
  // ------------------------------------------------------------------
  if (parse_ok && !proj.rom_filename.empty()) {
    std::string rom_abs = proj.GetAbsolutePath(proj.rom_filename);
    std::error_code ec;
    if (fs::exists(rom_abs, ec) && !ec) {
      auto fsize = fs::file_size(rom_abs, ec);
      if (ec) {
        checks.push_back({"rom_accessible", "warn",
                           absl::StrFormat("ROM exists but size unreadable: %s",
                                           rom_abs)});
      } else {
        checks.push_back(
            {"rom_accessible", "pass",
             absl::StrFormat("%s (%zu bytes)", rom_abs, fsize)});
      }
    } else {
      checks.push_back(
          {"rom_accessible", "fail",
           absl::StrFormat("ROM not found: %s (resolved: %s)",
                           proj.rom_filename, rom_abs)});
      any_fail = true;
    }
  } else if (parse_ok) {
    checks.push_back({"rom_accessible", "warn", "No ROM path in project"});
  }

  // ------------------------------------------------------------------
  // Check 7: ROM hash verification (optional, --check-rom-hash)
  // ------------------------------------------------------------------
  if (parser.HasFlag("check-rom-hash") && parse_ok) {
    if (!is_bundle) {
      checks.push_back({"rom_hash_check", "warn",
                         "Hash check only supported for .yazeproj bundles"});
    } else {
      fs::path manifest_path = fs::path(resolved_path) / "manifest.json";
      std::error_code ec;
      if (!fs::exists(manifest_path, ec) || ec) {
        checks.push_back({"rom_hash_check", "warn",
                           "No manifest.json in bundle (hash unavailable)"});
      } else {
        std::ifstream mf(manifest_path);
        auto manifest = nlohmann::json::parse(mf, nullptr, false);
        if (manifest.is_discarded()) {
          checks.push_back({"rom_hash_check", "fail",
                             "manifest.json parse failed"});
          any_fail = true;
        } else {
          std::string raw_expected =
              manifest.value("rom_sha1", std::string{});
          if (raw_expected.empty()) {
            checks.push_back({"rom_hash_check", "warn",
                               "No rom_sha1 field in manifest.json"});
          } else {
            std::string expected_sha1 = NormalizeHash(raw_expected);
            std::string rom_abs = proj.GetAbsolutePath(proj.rom_filename);
            std::string actual_sha1 = util::ComputeFileSha1Hex(rom_abs);
            if (actual_sha1.empty()) {
              checks.push_back(
                  {"rom_hash_check", "fail",
                   absl::StrFormat("Cannot read ROM for hashing: %s",
                                   rom_abs)});
              any_fail = true;
            } else if (NormalizeHash(actual_sha1) == expected_sha1) {
              checks.push_back(
                  {"rom_hash_check", "pass",
                   absl::StrFormat("SHA1 match: %s", actual_sha1)});
            } else {
              checks.push_back(
                  {"rom_hash_check", "fail",
                   absl::StrFormat("SHA1 mismatch: expected=%s actual=%s",
                                   expected_sha1, actual_sha1)});
              any_fail = true;
            }
          }
        }
      }
    }
  }

  // ------------------------------------------------------------------
  // Emit results
  // ------------------------------------------------------------------
  bool overall_ok = !any_fail;
  int pass_count = 0, warn_count = 0, fail_count = 0;
  for (const auto& chk : checks) {
    if (chk.status == "pass") ++pass_count;
    else if (chk.status == "warn") ++warn_count;
    else ++fail_count;
  }

  formatter.AddField("ok", overall_ok);
  formatter.AddField("status",
                     overall_ok ? std::string("pass") : std::string("fail"));
  formatter.AddField("project_path", resolved_path);
  formatter.AddField("is_bundle", is_bundle);
  formatter.AddField("pass_count", pass_count);
  formatter.AddField("warn_count", warn_count);
  formatter.AddField("fail_count", fail_count);

  formatter.BeginArray("checks");
  for (const auto& chk : checks) {
    formatter.BeginObject("");
    formatter.AddField("name", chk.name);
    formatter.AddField("status", chk.status);
    formatter.AddField("detail", chk.detail);
    formatter.EndObject();
  }
  formatter.EndArray();

  // ------------------------------------------------------------------
  // Write report file
  // ------------------------------------------------------------------
  if (auto rp = parser.GetString("report");
      rp.has_value() && !rp->empty()) {
    // Build a standalone JSON report (formatter may be text mode)
    nlohmann::json report;
    report["ok"] = overall_ok;
    report["status"] = overall_ok ? "pass" : "fail";
    report["project_path"] = resolved_path;
    report["is_bundle"] = is_bundle;
    report["pass_count"] = pass_count;
    report["warn_count"] = warn_count;
    report["fail_count"] = fail_count;
    nlohmann::json checks_json = nlohmann::json::array();
    for (const auto& chk : checks) {
      checks_json.push_back({{"name", chk.name},
                              {"status", chk.status},
                              {"detail", chk.detail}});
    }
    report["checks"] = std::move(checks_json);

    std::ofstream report_file(
        *rp, std::ios::out | std::ios::binary | std::ios::trunc);
    if (!report_file.is_open()) {
      return absl::PermissionDeniedError(absl::StrFormat(
          "project-bundle-verify: cannot open report file: %s", *rp));
    }
    report_file << report.dump(2) << "\n";
    if (!report_file.good()) {
      return absl::InternalError(absl::StrFormat(
          "project-bundle-verify: failed writing report: %s", *rp));
    }
  }

  if (!overall_ok) {
    return absl::FailedPreconditionError(absl::StrFormat(
        "project-bundle-verify: %d check(s) failed", fail_count));
  }
  return absl::OkStatus();
}

}  // namespace yaze::cli::handlers
