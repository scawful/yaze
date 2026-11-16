#include "core/project_workspace.h"

#include <chrono>
#include <cstdlib>
#include <ctime>
#include <fstream>
#include <optional>
#include <regex>
#include <sstream>
#include <system_error>

#include "absl/strings/ascii.h"
#include "absl/strings/match.h"
#include "absl/strings/str_cat.h"
#include "absl/strings/str_format.h"
#include "core/project.h"
#include "util/macro.h"
#include "util/rom_hash.h"

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

struct PreservedLine {
  std::string body;
  std::string ending;
};

std::vector<PreservedLine> SplitLinesPreservingEndings(absl::string_view text) {
  std::vector<PreservedLine> lines;
  size_t pos = 0;
  while (pos < text.size()) {
    size_t line_end = text.find('\n', pos);
    if (line_end == absl::string_view::npos) {
      std::string body(text.substr(pos));
      lines.push_back({std::move(body), ""});
      break;
    }
    if (line_end > pos && text[line_end - 1] == '\r') {
      lines.push_back(
          {std::string(text.substr(pos, line_end - pos - 1)), "\r\n"});
    } else {
      lines.push_back({std::string(text.substr(pos, line_end - pos)), "\n"});
    }
    pos = line_end + 1;
  }
  return lines;
}

std::string JoinPreservedLines(const std::vector<PreservedLine>& lines) {
  std::string out;
  for (const auto& line : lines) {
    out.append(line.body);
    out.append(line.ending);
  }
  return out;
}

std::string FormatLocalFileTime(fs::file_time_type ftime) {
  const auto sys_now = std::chrono::system_clock::now();
  const auto file_now = fs::file_time_type::clock::now();
  const auto sctp =
      std::chrono::time_point_cast<std::chrono::system_clock::duration>(
          ftime - file_now + sys_now);
  const std::time_t tt = std::chrono::system_clock::to_time_t(sctp);
  std::tm local_tm{};
#ifdef _WIN32
  localtime_s(&local_tm, &tt);
#else
  localtime_r(&tt, &local_tm);
#endif
  char buf[32] = {};
  if (std::strftime(buf, sizeof(buf), "%Y-%m-%d %H:%M:%S", &local_tm) == 0) {
    return "unknown";
  }
  return std::string(buf);
}

std::string DescribePathMtime(const fs::path& path) {
  if (path.empty()) {
    return "unknown";
  }
  std::error_code ec;
  if (!fs::is_regular_file(path, ec) || ec) {
    return "unknown";
  }
  const auto mtime = fs::last_write_time(path, ec);
  if (ec) {
    return "unknown";
  }
  return FormatLocalFileTime(mtime);
}

std::string FindNewestBackupInDir(const fs::path& dir) {
  if (dir.empty()) {
    return "";
  }
  std::error_code ec;
  if (!fs::is_directory(dir, ec) || ec) {
    return "";
  }
  fs::path newest_path;
  fs::file_time_type newest_time{};
  bool found = false;
  for (fs::directory_iterator it(dir, ec), end; !ec && it != end;
       it.increment(ec)) {
    std::error_code entry_ec;
    if (!it->is_regular_file(entry_ec) || entry_ec) {
      continue;
    }
    const std::string name = it->path().filename().string();
    if (name.empty() || name.front() == '.' || absl::EndsWith(name, ".tmp")) {
      continue;
    }
    const auto mtime = fs::last_write_time(it->path(), entry_ec);
    if (entry_ec) {
      continue;
    }
    if (!found || mtime > newest_time) {
      found = true;
      newest_time = mtime;
      newest_path = it->path();
    }
  }
  if (!found) {
    return "";
  }
  return absl::StrFormat("%s (%s)", newest_path.filename().string(),
                         FormatLocalFileTime(newest_time));
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

absl::StatusOr<std::string> UpdateProjectDescriptorKey(
    const fs::path& project_file, absl::string_view section,
    absl::string_view key, absl::string_view value) {
  if (project_file.empty() || section.empty() || key.empty()) {
    return absl::InvalidArgumentError(
        "Project file, section, and key are required");
  }
  if (value.find('\n') != absl::string_view::npos ||
      value.find('\r') != absl::string_view::npos) {
    return absl::InvalidArgumentError(
        "Descriptor value must not contain newline characters");
  }
  std::error_code ec;
  if (!fs::is_regular_file(project_file, ec) || ec ||
      fs::file_size(project_file, ec) > 16 * 1024 * 1024 || ec) {
    return absl::FailedPreconditionError(absl::StrFormat(
        "Project descriptor is missing, unreadable, or oversized: %s",
        project_file.string()));
  }
  std::string before;
  {
    std::ifstream in(project_file, std::ios::binary);
    if (!in) {
      return absl::FailedPreconditionError(
          absl::StrFormat("Cannot read %s", project_file.string()));
    }
    before.assign(std::istreambuf_iterator<char>(in),
                  std::istreambuf_iterator<char>());
    if (in.bad()) {
      return absl::DataLossError(
          absl::StrFormat("Read failed for %s", project_file.string()));
    }
  }
  const absl::string_view trimmed_all = absl::StripAsciiWhitespace(before);
  if (trimmed_all.empty() || trimmed_all.front() == '{') {
    return absl::FailedPreconditionError(
        absl::StrFormat("Project descriptor is not an INI project file: %s",
                        project_file.string()));
  }

  std::vector<PreservedLine> lines = SplitLinesPreservingEndings(before);
  const std::string newline =
      before.find("\r\n") != std::string::npos ? "\r\n" : "\n";
  std::optional<size_t> matched_line;
  std::optional<size_t> section_header_line;
  std::optional<size_t> last_nonblank_in_section;
  bool in_target = false;
  for (size_t i = 0; i < lines.size(); ++i) {
    const absl::string_view trimmed = absl::StripAsciiWhitespace(lines[i].body);
    if (trimmed.empty() || trimmed.front() == '#' || trimmed.front() == ';') {
      continue;
    }
    if (trimmed.front() == '[' && trimmed.back() == ']') {
      const absl::string_view sec_name =
          absl::StripAsciiWhitespace(trimmed.substr(1, trimmed.size() - 2));
      in_target = (sec_name == section);
      if (in_target) {
        if (section_header_line.has_value()) {
          return absl::FailedPreconditionError(
              absl::StrFormat("%s has duplicate [%s] sections",
                              project_file.string(), section));
        }
        section_header_line = i;
        last_nonblank_in_section = i;
      }
      continue;
    }
    if (!in_target) {
      continue;
    }
    last_nonblank_in_section = i;
    const size_t eq = lines[i].body.find('=');
    if (eq == std::string::npos) {
      continue;
    }
    const absl::string_view line_key = absl::StripAsciiWhitespace(
        absl::string_view(lines[i].body).substr(0, eq));
    if (line_key == key) {
      if (matched_line.has_value()) {
        return absl::FailedPreconditionError(
            absl::StrFormat("%s has multiple '%s' entries in [%s]",
                            project_file.string(), key, section));
      }
      matched_line = i;
    }
  }

  const std::string assignment = absl::StrCat(key, "=", value);
  if (matched_line.has_value()) {
    lines[*matched_line].body = assignment;
  } else if (last_nonblank_in_section.has_value()) {
    const size_t idx = *last_nonblank_in_section;
    if (lines[idx].ending.empty()) {
      lines[idx].ending = newline;
    }
    const std::string ins_ending = lines[idx].ending;
    lines.insert(lines.begin() + static_cast<std::ptrdiff_t>(idx + 1),
                 PreservedLine{assignment, ins_ending});
  } else {
    if (!lines.empty() && lines.back().ending.empty()) {
      lines.back().ending = newline;
    }
    if (!lines.empty() &&
        !absl::StripAsciiWhitespace(lines.back().body).empty()) {
      lines.push_back(PreservedLine{"", newline});
    }
    lines.push_back(PreservedLine{absl::StrCat("[", section, "]"), newline});
    lines.push_back(PreservedLine{assignment, newline});
  }

  const std::string updated = JoinPreservedLines(lines);
  if (updated == before) {
    return updated;
  }

  fs::path write_target = project_file;
  if (fs::is_symlink(project_file, ec) && !ec) {
    const fs::path resolved = fs::canonical(project_file, ec);
    if (!ec) {
      write_target = resolved;
    }
  }
  std::error_code perm_ec;
  const auto prior_status = fs::status(write_target, perm_ec);
  const bool had_perms = !perm_ec && fs::exists(prior_status);
  RETURN_IF_ERROR(WriteProjectFileAtomically(write_target.string(), updated,
                                             /*replace_existing=*/true));
  if (had_perms) {
    fs::permissions(write_target, prior_status.permissions(), perm_ec);
  }
  return updated;
}

RomIdentityMismatchInfo InspectRomIdentityMismatch(
    const uint8_t* rom_data, size_t rom_size, absl::string_view rom_filename,
    const YazeProject& project) {
  RomIdentityMismatchInfo info;
  info.expected_hash = std::string(
      absl::StripAsciiWhitespace(project.rom_metadata.expected_hash));

  std::string effective_rom_path(rom_filename);
  if (effective_rom_path.empty() && !project.rom_filename.empty()) {
    effective_rom_path = project.GetAbsolutePath(project.rom_filename);
  }
  info.rom_path = effective_rom_path;
  info.rom_mtime = DescribePathMtime(effective_rom_path);

  if (rom_data != nullptr && rom_size > 0) {
    const auto identity = util::ComputeRomIdentityHashes(rom_data, rom_size);
    info.loaded_sha1 = identity.sha1;
    info.loaded_crc32 = identity.crc32;
    std::string normalized_expected = absl::AsciiStrToLower(info.expected_hash);
    if (absl::StartsWith(normalized_expected, "0x")) {
      normalized_expected.erase(0, 2);
    }
    if (normalized_expected.size() == 8) {
      info.loaded_hash = identity.crc32;
    } else {
      info.loaded_hash =
          !identity.sha1.empty() ? identity.sha1 : identity.crc32;
    }
  }

  std::string backup_summary;
  if (!project.filepath.empty()) {
    const fs::path parent = fs::path(project.filepath).parent_path();
    if (parent.extension() == ".yazeproj") {
      backup_summary = FindNewestBackupInDir(parent / "backups");
    }
  }
  if (backup_summary.empty() && !project.rom_backup_folder.empty()) {
    backup_summary = FindNewestBackupInDir(
        fs::path(project.GetAbsolutePath(project.rom_backup_folder)));
  }
  info.newest_backup = backup_summary.empty() ? "(none)" : backup_summary;
  return info;
}

std::string FormatRomIdentityMismatchFields(
    const RomIdentityMismatchInfo& info) {
  std::string out = absl::StrFormat(
      "expected_hash=%s, loaded_hash=%s",
      info.expected_hash.empty() ? "(unset)" : info.expected_hash,
      info.loaded_hash.empty() ? "unknown" : info.loaded_hash);
  if (!info.loaded_sha1.empty() && !info.loaded_crc32.empty()) {
    absl::StrAppend(&out, absl::StrFormat(" (sha1=%s, crc32=%s)",
                                          info.loaded_sha1, info.loaded_crc32));
  }
  absl::StrAppend(
      &out, absl::StrFormat(
                ", rom_mtime=%s, newest_backup=%s",
                info.rom_mtime.empty() ? "unknown" : info.rom_mtime,
                info.newest_backup.empty() ? "(none)" : info.newest_backup));
  return out;
}

}  // namespace yaze::project
