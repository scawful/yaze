#include "cli/handlers/rom/project_bundle_verify_commands.h"

#include <algorithm>
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
#include "rom/rom.h"
#include "util/rom_hash.h"

namespace yaze::cli::handlers {

namespace fs = std::filesystem;

namespace {

// Normalize a hex hash string: trim whitespace, lowercase.
std::string NormalizeHash(const std::string& hash) {
  std::string result;
  result.reserve(hash.size());
  for (char ch : hash) {
    if (ch == ' ' || ch == '\t' || ch == '\n' || ch == '\r')
      continue;
    result += static_cast<char>(std::tolower(static_cast<unsigned char>(ch)));
  }
  return result;
}

bool IsHexHash(const std::string& hash) {
  return !hash.empty() &&
         std::all_of(hash.begin(), hash.end(),
                     [](unsigned char ch) { return std::isxdigit(ch); });
}

struct CheckResult {
  std::string name;
  std::string status;  // "pass" | "warn" | "fail"
  std::string detail;
};

std::string Trim(std::string value) {
  auto is_space = [](unsigned char ch) {
    return std::isspace(ch);
  };
  while (!value.empty() && is_space(value.front())) {
    value.erase(value.begin());
  }
  while (!value.empty() && is_space(value.back())) {
    value.pop_back();
  }
  return value;
}

bool IsAbsoluteConfigPath(const std::string& value) {
  if (value.empty()) {
    return false;
  }
  const auto is_windows_separator = [](char ch) {
    return ch == '/' || ch == '\\';
  };
  // std::filesystem only recognizes the host platform's absolute-path syntax.
  // Detect Windows paths explicitly so a project authored on Windows is still
  // audited correctly on macOS/Linux.
  if (value.size() >= 2 && is_windows_separator(value[0]) &&
      is_windows_separator(value[1])) {
    return true;  // UNC or Windows device path.
  }
  if (fs::path(value).is_absolute()) {
    return true;
  }
  return value.size() >= 3 &&
         std::isalpha(static_cast<unsigned char>(value[0])) &&
         value[1] == ':' && (value[2] == '/' || value[2] == '\\');
}

std::string StripMatchingQuotes(std::string value) {
  value = Trim(std::move(value));
  if (value.size() >= 2 && ((value.front() == '"' && value.back() == '"') ||
                            (value.front() == '\'' && value.back() == '\''))) {
    return value.substr(1, value.size() - 2);
  }
  return value;
}

std::vector<std::string> ConfigPathValues(const std::string& key,
                                          const std::string& raw_value) {
  if (key != "additional_roms") {
    return {StripMatchingQuotes(raw_value)};
  }

  std::vector<std::string> values;
  size_t begin = 0;
  while (begin <= raw_value.size()) {
    const size_t comma = raw_value.find(',', begin);
    const size_t count =
        comma == std::string::npos ? std::string::npos : comma - begin;
    std::string value = StripMatchingQuotes(raw_value.substr(begin, count));
    if (!value.empty()) {
      values.push_back(std::move(value));
    }
    if (comma == std::string::npos) {
      break;
    }
    begin = comma + 1;
  }
  return values;
}

std::vector<std::string> ListAbsolutePathsInProjectFile(
    const fs::path& project_file) {
  std::vector<std::string> results;
  std::ifstream input(project_file);
  if (!input.is_open()) {
    return results;
  }

  bool in_files_section = false;
  std::string line;
  while (std::getline(input, line)) {
    line = Trim(line);
    if (line.empty() || line[0] == '#' || line[0] == ';') {
      continue;
    }
    if (line.front() == '[' && line.back() == ']') {
      in_files_section = line == "[files]";
      continue;
    }
    if (!in_files_section) {
      continue;
    }

    const size_t equals = line.find('=');
    if (equals == std::string::npos) {
      continue;
    }
    const std::string key = Trim(line.substr(0, equals));
    const std::string raw_value = line.substr(equals + 1);
    for (const auto& value : ConfigPathValues(key, raw_value)) {
      if (IsAbsoluteConfigPath(value)) {
        results.push_back(absl::StrFormat("%s = %s", key, value));
      }
    }
  }
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
       "Verify bundle manifest rom_sha1 or standalone expected_hash "
       "(CRC32/SHA1)",
       ""},
      {"--report", "Write full JSON summary to this path in addition to stdout",
       ""},
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
  if (auto rp = parser.GetString("report"); rp.has_value() && !rp->empty()) {
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
      checks.push_back(
          {"bundle_project_yaze", "pass", "project.yaze found in bundle root"});
    } else {
      checks.push_back({"bundle_project_yaze", "warn",
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
          {"project_parses", "pass", absl::StrFormat("name=%s", proj.name)});
    } else {
      checks.push_back(
          {"project_parses", "fail",
           absl::StrFormat("Parse failed: %s", std::string(status.message()))});
      any_fail = true;
    }
  }

  // ------------------------------------------------------------------
  // Check 5: Path portability (warnings for absolute paths)
  // ------------------------------------------------------------------
  if (parse_ok) {
    const fs::path project_config =
        is_bundle ? fs::path(resolved_path) / "project.yaze"
                  : fs::path(resolved_path);
    const auto abs_paths = ListAbsolutePathsInProjectFile(project_config);
    if (!abs_paths.empty()) {
      std::string detail = "Absolute paths reduce portability: ";
      for (size_t i = 0; i < abs_paths.size(); ++i) {
        if (i > 0)
          detail += "; ";
        detail += abs_paths[i];
      }
      checks.push_back({"path_portability", "warn", detail});
    } else {
      checks.push_back({"path_portability", "pass", "All paths are relative"});
    }
  }

  // ------------------------------------------------------------------
  // Check 6: Explicit hack manifest readiness
  // ------------------------------------------------------------------
  if (parse_ok && !proj.hack_manifest_file.empty()) {
    const std::string manifest_abs =
        proj.GetAbsolutePath(proj.hack_manifest_file);
    std::error_code ec;
    if (!fs::exists(manifest_abs, ec) || ec) {
      checks.push_back(
          {"hack_manifest_ready", "fail",
           absl::StrFormat("Configured hack manifest not found: %s",
                           manifest_abs)});
      any_fail = true;
    } else if (!proj.hack_manifest.loaded()) {
      checks.push_back(
          {"hack_manifest_ready", "fail",
           absl::StrFormat("Configured hack manifest failed to load: %s",
                           manifest_abs)});
      any_fail = true;
    } else {
      checks.push_back({"hack_manifest_ready", "pass",
                        absl::StrFormat("Loaded configured hack manifest: %s",
                                        manifest_abs)});
    }
  }

  // ------------------------------------------------------------------
  // Check 7: ROM accessibility
  // ------------------------------------------------------------------
  if (parse_ok && !proj.rom_filename.empty()) {
    std::string rom_abs = proj.GetAbsolutePath(proj.rom_filename);
    std::error_code ec;
    if (fs::exists(rom_abs, ec) && !ec) {
      auto fsize = fs::file_size(rom_abs, ec);
      if (ec) {
        checks.push_back(
            {"rom_accessible", "warn",
             absl::StrFormat("ROM exists but size unreadable: %s", rom_abs)});
      } else {
        checks.push_back({"rom_accessible", "pass",
                          absl::StrFormat("%s (%zu bytes)", rom_abs, fsize)});
      }
    } else {
      checks.push_back({"rom_accessible", "fail",
                        absl::StrFormat("ROM not found: %s (resolved: %s)",
                                        proj.rom_filename, rom_abs)});
      any_fail = true;
    }
  } else if (parse_ok) {
    checks.push_back({"rom_accessible", "warn", "No ROM path in project"});
  }

  // ------------------------------------------------------------------
  // Check 8: ROM hash verification (optional, --check-rom-hash)
  // ------------------------------------------------------------------
  if (parser.HasFlag("check-rom-hash") && parse_ok) {
    std::string raw_expected;
    bool hash_metadata_ready = true;

    if (is_bundle) {
      fs::path manifest_path = fs::path(resolved_path) / "manifest.json";
      std::error_code ec;
      if (!fs::exists(manifest_path, ec) || ec) {
        checks.push_back({"rom_hash_check", "warn",
                          "No manifest.json in bundle (hash unavailable)"});
        hash_metadata_ready = false;
      } else {
        std::ifstream mf(manifest_path);
        auto manifest = nlohmann::json::parse(mf, nullptr, false);
        if (manifest.is_discarded()) {
          checks.push_back(
              {"rom_hash_check", "fail", "manifest.json parse failed"});
          any_fail = true;
          hash_metadata_ready = false;
        } else {
          raw_expected = manifest.value("rom_sha1", std::string{});
          if (raw_expected.empty()) {
            checks.push_back({"rom_hash_check", "warn",
                              "No rom_sha1 field in manifest.json"});
            hash_metadata_ready = false;
          }
        }
      }
    } else {
      raw_expected = proj.rom_metadata.expected_hash;
      if (raw_expected.empty()) {
        checks.push_back({"rom_hash_check", "warn",
                          "No expected_hash in standalone project"});
        hash_metadata_ready = false;
      }
    }

    if (hash_metadata_ready) {
      const std::string expected = NormalizeHash(raw_expected);
      const std::string rom_abs = proj.GetAbsolutePath(proj.rom_filename);
      std::string actual;
      std::string algorithm;

      if (is_bundle) {
        // Bundle manifests explicitly declare a raw-file SHA1. Keep that
        // contract distinct from standalone project hashes, which describe
        // the header-stripped ROM buffer loaded by the editor.
        algorithm = "SHA1";
        if (expected.size() != 40 || !IsHexHash(expected)) {
          checks.push_back(
              {"rom_hash_check", "fail",
               "manifest.json rom_sha1 must be 40 hexadecimal characters"});
          any_fail = true;
          hash_metadata_ready = false;
        } else {
          actual = util::ComputeFileSha1Hex(rom_abs);
        }
      } else if ((expected.size() == 8 || expected.size() == 40) &&
                 IsHexHash(expected)) {
        algorithm = expected.size() == 8 ? "CRC32" : "SHA1";
        Rom loaded_rom;
        Rom::LoadOptions load_options;
        load_options.load_resource_labels = false;
        const auto load_status = loaded_rom.LoadFromFile(rom_abs, load_options);
        if (!load_status.ok()) {
          checks.push_back(
              {"rom_hash_check", "fail",
               absl::StrFormat("Cannot load ROM for hashing: %s (%s)", rom_abs,
                               load_status.message())});
          any_fail = true;
          hash_metadata_ready = false;
        } else if (expected.size() == 8) {
          actual = util::ComputeRomHash(loaded_rom.data(), loaded_rom.size());
        } else {
          actual = util::ComputeSha1Hex(loaded_rom.data(), loaded_rom.size());
        }
      } else {
        checks.push_back(
            {"rom_hash_check", "fail",
             "Standalone expected_hash must be an 8-character CRC32 or "
             "40-character SHA1 hexadecimal digest"});
        any_fail = true;
        hash_metadata_ready = false;
      }

      if (hash_metadata_ready) {
        if (actual.empty()) {
          checks.push_back(
              {"rom_hash_check", "fail",
               absl::StrFormat("Cannot read ROM for hashing: %s", rom_abs)});
          any_fail = true;
        } else if (NormalizeHash(actual) == expected) {
          checks.push_back(
              {"rom_hash_check", "pass",
               absl::StrFormat("%s match: %s", algorithm, actual)});
        } else {
          checks.push_back(
              {"rom_hash_check", "fail",
               absl::StrFormat("%s mismatch: expected=%s actual=%s", algorithm,
                               expected, actual)});
          any_fail = true;
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
    if (chk.status == "pass")
      ++pass_count;
    else if (chk.status == "warn")
      ++warn_count;
    else
      ++fail_count;
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
  if (auto rp = parser.GetString("report"); rp.has_value() && !rp->empty()) {
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
      checks_json.push_back(
          {{"name", chk.name}, {"status", chk.status}, {"detail", chk.detail}});
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
