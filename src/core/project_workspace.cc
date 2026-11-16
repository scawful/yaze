#include "core/project_workspace.h"

#include <cstdlib>
#include <fstream>
#include <regex>
#include <sstream>
#include <system_error>

#include "absl/strings/ascii.h"
#include "absl/strings/match.h"
#include "absl/strings/str_format.h"

namespace yaze::project {
namespace {

namespace fs = std::filesystem;

std::string ResolveAgainst(const fs::path& base, const std::string& value) {
  if (value.empty()) {
    return "";
  }
  fs::path path(value);
  if (path.is_relative()) {
    path = base / path;
  }
  return path.lexically_normal().string();
}

}  // namespace

fs::path ProjectFileForPath(const fs::path& path) {
  std::error_code ec;
  if (path.extension() == ".yazeproj" && fs::is_directory(path, ec)) {
    return path / "project.yaze";
  }
  return path;
}

ProjectWorkspaceFolders ReadProjectWorkspaceFolders(
    const fs::path& project_file) {
  ProjectWorkspaceFolders folders;
  std::ifstream in(project_file);
  if (!in) {
    return folders;
  }
  const fs::path base = project_file.parent_path();
  std::string line;
  bool in_workspace = false;
  while (std::getline(in, line)) {
    const std::string trimmed(absl::StripAsciiWhitespace(line));
    if (trimmed.empty() || trimmed[0] == '#' || trimmed[0] == ';') {
      continue;
    }
    if (trimmed.front() == '[' && trimmed.back() == ']') {
      in_workspace = trimmed == "[workspace]";
      continue;
    }
    if (!in_workspace) {
      continue;
    }
    const size_t eq = trimmed.find('=');
    if (eq == std::string::npos) {
      continue;
    }
    const std::string key(absl::StripAsciiWhitespace(trimmed.substr(0, eq)));
    const std::string value(absl::StripAsciiWhitespace(trimmed.substr(eq + 1)));
    if (key == "app_data_folder") {
      folders.app_data_folder = ResolveAgainst(base, value);
    } else if (key == "proposal_folder") {
      folders.proposal_folder = ResolveAgainst(base, value);
    } else if (key == "sandbox_folder") {
      folders.sandbox_folder = ResolveAgainst(base, value);
    }
  }
  return folders;
}

bool ReadProjectEditorControl(const fs::path& project_file) {
  std::ifstream in(project_file);
  if (!in) {
    return false;
  }
  std::string line;
  bool in_rom = false;
  bool enabled = false;
  while (std::getline(in, line)) {
    const std::string trimmed(absl::StripAsciiWhitespace(line));
    if (trimmed.empty() || trimmed[0] == '#' || trimmed[0] == ';') {
      continue;
    }
    if (trimmed.front() == '[' && trimmed.back() == ']') {
      in_rom = trimmed == "[rom]";
      continue;
    }
    const size_t eq = trimmed.find('=');
    if (!in_rom || eq == std::string::npos) {
      continue;
    }
    if (absl::StripAsciiWhitespace(trimmed.substr(0, eq)) == "editor_control") {
      const std::string value = absl::AsciiStrToLower(
          absl::StripAsciiWhitespace(trimmed.substr(eq + 1)));
      enabled =
          value == "true" || value == "1" || value == "yes" || value == "on";
    }
  }
  return enabled;
}

std::vector<std::pair<std::string, std::string>>
ApplyProjectWorkspaceEnvironment(const fs::path& rom_or_project) {
  std::vector<std::pair<std::string, std::string>> applied;
  if (rom_or_project.empty()) {
    return applied;
  }
  const fs::path project_file = ProjectFileForPath(rom_or_project);
  if (project_file.extension() != ".yaze") {
    return applied;
  }
  const ProjectWorkspaceFolders folders =
      ReadProjectWorkspaceFolders(project_file);
  const std::pair<const char*, const std::string*> entries[] = {
      {kAppDataDirEnv, &folders.app_data_folder},
      {kProposalRootEnv, &folders.proposal_folder},
      {kSandboxRootEnv, &folders.sandbox_folder},
  };
  for (const auto& [name, value] : entries) {
    if (value->empty()) {
      continue;
    }
    if (const char* existing = std::getenv(name);
        existing != nullptr && existing[0] != '\0') {
      continue;
    }
    std::error_code ec;
    fs::create_directories(*value, ec);
    if (ec) {
      continue;
    }
#ifdef _WIN32
    _putenv_s(name, value->c_str());
#else
    setenv(name, value->c_str(), /*overwrite=*/0);
#endif
    applied.emplace_back(name, *value);
  }
  return applied;
}

absl::Status UpdateBundleManifestRomChecksum(const fs::path& bundle_dir,
                                             const std::string& new_sha1) {
  static const std::regex kSha1(R"(^[0-9a-fA-F]{40}$)");
  if (!std::regex_match(new_sha1, kSha1)) {
    return absl::InvalidArgumentError(
        absl::StrFormat("not a sha1: '%s'", new_sha1));
  }
  const fs::path manifest = bundle_dir / "manifest.json";
  std::ifstream in(manifest, std::ios::binary);
  if (!in) {
    return absl::NotFoundError(
        absl::StrFormat("no manifest: %s", manifest.string()));
  }
  std::ostringstream buffer;
  buffer << in.rdbuf();
  in.close();
  const std::string text = buffer.str();

  static const std::regex kChecksum(
      R"re("romChecksum"\s*:\s*"([0-9a-fA-F]*)")re");
  const auto begin = std::sregex_iterator(text.begin(), text.end(), kChecksum);
  const auto count = std::distance(begin, std::sregex_iterator());
  if (count != 1) {
    return absl::FailedPreconditionError(
        absl::StrFormat("%s has %d romChecksum entries; expected 1",
                        manifest.string(), static_cast<int>(count)));
  }
  // Splice by position. regex_replace with "$1" + sha would read a sha that
  // starts with a digit as a two-digit group reference ("$13...").
  const std::smatch& match = *begin;
  std::string updated = text;
  updated.replace(static_cast<size_t>(match.position(1)),
                  static_cast<size_t>(match.length(1)), new_sha1);
  if (updated == text) {
    return absl::OkStatus();
  }

  const fs::path tmp = manifest.string() + ".tmp";
  {
    std::ofstream out(tmp, std::ios::binary | std::ios::trunc);
    if (!out) {
      return absl::PermissionDeniedError(
          absl::StrFormat("cannot write %s", tmp.string()));
    }
    out << updated;
    if (!out) {
      return absl::DataLossError(
          absl::StrFormat("short write to %s", tmp.string()));
    }
  }
  std::error_code ec;
  fs::rename(tmp, manifest, ec);
  if (ec) {
    fs::remove(tmp, ec);
    return absl::InternalError(absl::StrFormat(
        "cannot replace %s: %s", manifest.string(), ec.message()));
  }
  return absl::OkStatus();
}

std::string RomHashToRecordOnOpen(const std::string& expected_hash,
                                  const std::string& rom_sha1,
                                  const std::string& rom_crc32) {
  std::string expected =
      absl::AsciiStrToLower(absl::StripAsciiWhitespace(expected_hash));
  if (absl::StartsWith(expected, "0x")) {
    expected.erase(0, 2);
  }
  if (expected.empty()) {
    return "";
  }
  const std::string target =
      absl::AsciiStrToLower(rom_sha1.empty() ? rom_crc32 : rom_sha1);
  if (target.empty() || target == expected) {
    return "";
  }
  return target;
}

}  // namespace yaze::project
