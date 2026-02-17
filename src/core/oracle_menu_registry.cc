#include "core/oracle_menu_registry.h"

#include <algorithm>
#include <cctype>
#include <fstream>
#include <regex>
#include <sstream>
#include <string>
#include <unordered_map>
#include <vector>

#include "absl/status/status.h"
#include "absl/strings/ascii.h"
#include "absl/strings/str_format.h"
#include "absl/strings/strip.h"
#include "util/macro.h"

namespace yaze::core {

namespace {

std::string NormalizePathString(const std::filesystem::path& path) {
  return path.lexically_normal().generic_string();
}

std::string RelativePathString(const std::filesystem::path& base,
                               const std::filesystem::path& path) {
  std::error_code ec;
  auto rel = std::filesystem::relative(path, base, ec);
  if (ec) {
    return NormalizePathString(path);
  }
  return NormalizePathString(rel);
}

std::string Lowercase(std::string text) {
  std::transform(text.begin(), text.end(), text.begin(),
                 [](unsigned char c) { return std::tolower(c); });
  return text;
}

bool IsPathWithinRoot(const std::filesystem::path& root,
                      const std::filesystem::path& candidate) {
  std::error_code ec;
  const std::filesystem::path canonical_root =
      std::filesystem::weakly_canonical(root, ec);
  if (ec) {
    return false;
  }

  const std::filesystem::path canonical_candidate =
      std::filesystem::weakly_canonical(candidate, ec);
  if (ec) {
    return false;
  }

  const std::filesystem::path rel =
      canonical_candidate.lexically_relative(canonical_root);
  if (rel.empty() || rel.is_absolute()) {
    return false;
  }
  for (const auto& part : rel) {
    if (part == "..") {
      return false;
    }
  }
  return true;
}

bool ContainsDrawToken(const std::string& value) {
  return Lowercase(value).find("draw") != std::string::npos;
}

bool ParseGlobalLabel(const std::string& line, std::string* label_out) {
  static const std::regex kPattern(
      R"(^\s*([A-Za-z_][A-Za-z0-9_]*)\s*:\s*(?:;.*)?$)");
  std::smatch match;
  if (!std::regex_match(line, match, kPattern)) {
    return false;
  }
  *label_out = match[1].str();
  return true;
}

bool ParseLocalLabel(const std::string& line, std::string* label_out) {
  static const std::regex kPattern(
      R"(^\s*(\.[A-Za-z_][A-Za-z0-9_]*)\s*(?::\s*)?(?:;.*)?$)");
  std::smatch match;
  if (!std::regex_match(line, match, kPattern)) {
    return false;
  }
  *label_out = match[1].str();
  return true;
}

bool ParseIncbinLine(const std::string& line, std::string* inline_label_out,
                     std::string* bin_path_out) {
  static const std::regex kPattern(
      R"(^\s*(?:([A-Za-z_][A-Za-z0-9_]*)\s*:\s*)?incbin\s+\"([^\"]+)\".*$)",
      std::regex::icase);
  std::smatch match;
  if (!std::regex_match(line, match, kPattern)) {
    return false;
  }
  *inline_label_out = match[1].str();
  *bin_path_out = match[2].str();
  return true;
}

bool ParseMenuOffsetLine(const std::string& line, int* row_out, int* col_out,
                         std::string* note_out) {
  static const std::regex kPattern(
      R"(^\s*dw\s+menu_offset\(\s*([0-9]+)\s*,\s*([0-9]+)\s*\)(.*)$)",
      std::regex::icase);
  std::smatch match;
  if (!std::regex_match(line, match, kPattern)) {
    return false;
  }

  *row_out = std::stoi(match[1].str());
  *col_out = std::stoi(match[2].str());

  std::string trailing = match[3].str();
  const size_t semicolon = trailing.find(';');
  if (semicolon != std::string::npos) {
    *note_out =
        std::string(absl::StripAsciiWhitespace(trailing.substr(semicolon + 1)));
  } else {
    note_out->clear();
  }
  return true;
}

bool ParseDrawReference(const std::string& line, std::string* target_out) {
  static const std::regex kPattern(
      R"(\b(?:JSR|JSL|JMP)\s+([A-Za-z_][A-Za-z0-9_\.]*))", std::regex::icase);
  std::smatch match;
  if (!std::regex_search(line, match, kPattern)) {
    return false;
  }

  const std::string target = match[1].str();
  if (!ContainsDrawToken(target)) {
    return false;
  }
  *target_out = target;
  return true;
}

absl::StatusOr<std::vector<std::string>> ReadLines(
    const std::filesystem::path& path, std::string* newline_out,
    bool* trailing_newline_out) {
  std::ifstream in(path, std::ios::binary);
  if (!in.is_open()) {
    return absl::NotFoundError(
        absl::StrFormat("Cannot open file: %s", path.string()));
  }

  std::stringstream buffer;
  buffer << in.rdbuf();
  if (!in.good() && !in.eof()) {
    return absl::InternalError(
        absl::StrFormat("Failed reading file: %s", path.string()));
  }

  const std::string content = buffer.str();
  *newline_out = content.find("\r\n") != std::string::npos ? "\r\n" : "\n";
  *trailing_newline_out =
      !content.empty() &&
      (content.back() == '\n' ||
       (content.size() >= 2 &&
        content.substr(content.size() - 2) == std::string("\r\n")));

  std::vector<std::string> lines;
  lines.reserve(512);

  size_t cursor = 0;
  while (cursor < content.size()) {
    size_t end = content.find('\n', cursor);
    if (end == std::string::npos) {
      std::string line = content.substr(cursor);
      if (!line.empty() && line.back() == '\r') {
        line.pop_back();
      }
      lines.push_back(std::move(line));
      break;
    }
    std::string line = content.substr(cursor, end - cursor);
    if (!line.empty() && line.back() == '\r') {
      line.pop_back();
    }
    lines.push_back(std::move(line));
    cursor = end + 1;
  }

  if (lines.empty()) {
    lines.emplace_back();
  }

  return lines;
}

absl::Status WriteLines(const std::filesystem::path& path,
                        const std::vector<std::string>& lines,
                        absl::string_view newline, bool trailing_newline) {
  std::ofstream out(path, std::ios::binary | std::ios::trunc);
  if (!out.is_open()) {
    return absl::PermissionDeniedError(
        absl::StrFormat("Cannot open file for writing: %s", path.string()));
  }

  for (size_t i = 0; i < lines.size(); ++i) {
    out << lines[i];
    if (i + 1 < lines.size()) {
      out << newline;
    }
  }
  if (trailing_newline) {
    out << newline;
  }

  if (!out.good()) {
    return absl::InternalError(
        absl::StrFormat("Failed writing file: %s", path.string()));
  }
  return absl::OkStatus();
}

}  // namespace

absl::StatusOr<std::filesystem::path> ResolveOracleProjectRoot(
    const std::filesystem::path& start_path) {
  namespace fs = std::filesystem;

  std::error_code ec;
  fs::path current = start_path;
  if (current.empty()) {
    current = fs::current_path(ec);
    if (ec) {
      return absl::InternalError(absl::StrFormat(
          "Failed to read current directory: %s", ec.message()));
    }
  }
  current = fs::absolute(current, ec);
  if (ec) {
    return absl::InvalidArgumentError(
        absl::StrFormat("Invalid start path: %s", start_path.string()));
  }
  if (fs::is_regular_file(current, ec) && !ec) {
    current = current.parent_path();
  }

  while (!current.empty()) {
    fs::path menu_entry = current / "Menu" / "menu.asm";
    if (fs::exists(menu_entry, ec) && !ec) {
      fs::path canonical = fs::weakly_canonical(current, ec);
      if (!ec) {
        return canonical;
      }
      return current;
    }
    fs::path parent = current.parent_path();
    if (parent == current) {
      break;
    }
    current = parent;
  }

  return absl::NotFoundError(
      "Could not find Oracle project root (expected Menu/menu.asm)");
}

absl::StatusOr<OracleMenuRegistry> BuildOracleMenuRegistry(
    const std::filesystem::path& project_root) {
  namespace fs = std::filesystem;

  ASSIGN_OR_RETURN(const fs::path root, ResolveOracleProjectRoot(project_root));
  const fs::path menu_dir = root / "Menu";
  std::error_code ec;
  if (!fs::exists(menu_dir, ec) || ec || !fs::is_directory(menu_dir, ec)) {
    return absl::NotFoundError(
        absl::StrFormat("Missing Menu directory: %s", menu_dir.string()));
  }

  OracleMenuRegistry registry;
  registry.project_root = root;

  std::vector<fs::path> asm_paths;
  fs::recursive_directory_iterator it(
      menu_dir, fs::directory_options::skip_permission_denied, ec);
  fs::recursive_directory_iterator end;
  if (ec) {
    return absl::InternalError(absl::StrFormat(
        "Unable to enumerate Menu directory: %s", ec.message()));
  }
  for (; it != end; it.increment(ec)) {
    if (ec) {
      ec.clear();
      continue;
    }
    if (!it->is_regular_file()) {
      continue;
    }
    const std::string ext = Lowercase(it->path().extension().string());
    if (ext == ".asm") {
      asm_paths.push_back(it->path());
    }
  }

  std::sort(asm_paths.begin(), asm_paths.end(),
            [](const fs::path& a, const fs::path& b) {
              return a.generic_string() < b.generic_string();
            });

  std::unordered_map<std::string, int> draw_references;
  std::unordered_map<std::string, size_t> draw_indices;

  for (const auto& asm_path : asm_paths) {
    const std::string asm_rel = RelativePathString(root, asm_path);
    registry.asm_files.push_back(asm_rel);

    std::ifstream file(asm_path);
    if (!file.is_open()) {
      registry.warnings.push_back(
          absl::StrFormat("Failed to open %s", asm_rel));
      continue;
    }

    std::unordered_map<std::string, int> component_indices;
    std::string current_global_label;
    std::string line;
    int line_no = 0;
    while (std::getline(file, line)) {
      ++line_no;

      std::string global_label;
      if (ParseGlobalLabel(line, &global_label)) {
        current_global_label = global_label;
        component_indices[current_global_label] = 0;
        if (ContainsDrawToken(global_label)) {
          const std::string key = asm_rel + ":" + global_label;
          if (draw_indices.find(key) == draw_indices.end()) {
            draw_indices[key] = registry.draw_routines.size();
            registry.draw_routines.push_back(
                {.label = global_label, .asm_path = asm_rel, .line = line_no});
          }
        }
      }

      std::string local_label;
      if (ParseLocalLabel(line, &local_label) &&
          ContainsDrawToken(local_label)) {
        std::string full_label = local_label;
        if (!current_global_label.empty()) {
          full_label = current_global_label + local_label;
        }
        const std::string key = asm_rel + ":" + full_label;
        if (draw_indices.find(key) == draw_indices.end()) {
          draw_indices[key] = registry.draw_routines.size();
          registry.draw_routines.push_back({.label = full_label,
                                            .asm_path = asm_rel,
                                            .line = line_no,
                                            .local = true});
        }
      }

      std::string inline_label;
      std::string bin_path;
      if (ParseIncbinLine(line, &inline_label, &bin_path)) {
        const std::string label =
            !inline_label.empty() ? inline_label : current_global_label;
        fs::path resolved =
            (asm_path.parent_path() / bin_path).lexically_normal();
        std::error_code stat_ec;
        const bool exists = fs::exists(resolved, stat_ec) && !stat_ec;
        uintmax_t size_bytes = 0;
        if (exists) {
          size_bytes = fs::file_size(resolved, stat_ec);
          if (stat_ec) {
            size_bytes = 0;
          }
        }
        registry.bins.push_back(
            {.label = label,
             .asm_path = asm_rel,
             .line = line_no,
             .bin_path = bin_path,
             .resolved_bin_path = RelativePathString(root, resolved),
             .exists = exists,
             .size_bytes = size_bytes});
      }

      int row = 0;
      int col = 0;
      std::string note;
      if (ParseMenuOffsetLine(line, &row, &col, &note) &&
          !current_global_label.empty()) {
        const int index = component_indices[current_global_label]++;
        registry.components.push_back({.table_label = current_global_label,
                                       .index = index,
                                       .row = row,
                                       .col = col,
                                       .note = note,
                                       .asm_path = asm_rel,
                                       .line = line_no});
      }

      std::string draw_target;
      if (ParseDrawReference(line, &draw_target)) {
        draw_references[draw_target]++;
      }
    }
  }

  for (auto& routine : registry.draw_routines) {
    auto it_ref = draw_references.find(routine.label);
    if (it_ref != draw_references.end()) {
      routine.references = it_ref->second;
      continue;
    }

    // Fallback for local labels represented as "Global.local"
    const size_t dot = routine.label.find('.');
    if (dot != std::string::npos) {
      const std::string local = routine.label.substr(dot);
      it_ref = draw_references.find(local);
      if (it_ref != draw_references.end()) {
        routine.references = it_ref->second;
      }
    }
  }

  std::sort(registry.bins.begin(), registry.bins.end(),
            [](const OracleMenuBinEntry& a, const OracleMenuBinEntry& b) {
              if (a.asm_path != b.asm_path) {
                return a.asm_path < b.asm_path;
              }
              return a.line < b.line;
            });
  std::sort(registry.draw_routines.begin(), registry.draw_routines.end(),
            [](const OracleMenuDrawRoutine& a, const OracleMenuDrawRoutine& b) {
              if (a.asm_path != b.asm_path) {
                return a.asm_path < b.asm_path;
              }
              return a.line < b.line;
            });
  std::sort(registry.components.begin(), registry.components.end(),
            [](const OracleMenuComponent& a, const OracleMenuComponent& b) {
              if (a.asm_path != b.asm_path) {
                return a.asm_path < b.asm_path;
              }
              return a.line < b.line;
            });

  return registry;
}

absl::StatusOr<OracleMenuComponentEditResult> SetOracleMenuComponentOffset(
    const std::filesystem::path& project_root,
    const std::string& asm_relative_path, const std::string& table_label,
    int index, int row, int col, bool write_changes) {
  namespace fs = std::filesystem;

  if (asm_relative_path.empty()) {
    return absl::InvalidArgumentError("--asm path is required");
  }
  if (table_label.empty()) {
    return absl::InvalidArgumentError("--table is required");
  }
  if (index < 0) {
    return absl::InvalidArgumentError("--index must be >= 0");
  }
  if (row < 0 || col < 0) {
    return absl::InvalidArgumentError("--row and --col must be >= 0");
  }

  ASSIGN_OR_RETURN(const fs::path root, ResolveOracleProjectRoot(project_root));

  fs::path asm_path = fs::path(asm_relative_path);
  if (!asm_path.is_absolute()) {
    asm_path = root / asm_path;
  }
  asm_path = asm_path.lexically_normal();

  std::error_code ec;
  if (!fs::exists(asm_path, ec) || ec || !fs::is_regular_file(asm_path, ec)) {
    return absl::NotFoundError(
        absl::StrFormat("ASM file not found: %s", asm_path.string()));
  }
  if (!IsPathWithinRoot(root, asm_path)) {
    return absl::PermissionDeniedError(absl::StrFormat(
        "ASM path escapes project root: %s", asm_path.string()));
  }

  std::string newline;
  bool trailing_newline = false;
  ASSIGN_OR_RETURN(auto lines,
                   ReadLines(asm_path, &newline, &trailing_newline));

  bool in_table = false;
  int current_index = 0;
  int target_line = -1;
  int old_row = -1;
  int old_col = -1;
  std::string old_line;
  std::string new_line;
  static const std::regex kRewritePattern(
      R"(^(\s*dw\s+menu_offset\(\s*)([0-9]+)(\s*,\s*)([0-9]+)(\s*\).*)$)",
      std::regex::icase);

  for (size_t i = 0; i < lines.size(); ++i) {
    const std::string& line = lines[i];

    std::string global_label;
    if (ParseGlobalLabel(line, &global_label)) {
      if (global_label == table_label) {
        in_table = true;
        current_index = 0;
      } else if (in_table) {
        break;
      }
    }

    if (!in_table) {
      continue;
    }

    int parsed_row = 0;
    int parsed_col = 0;
    std::string note;
    if (!ParseMenuOffsetLine(line, &parsed_row, &parsed_col, &note)) {
      continue;
    }

    if (current_index == index) {
      std::smatch match;
      if (!std::regex_match(line, match, kRewritePattern)) {
        return absl::InternalError(absl::StrFormat(
            "Matched menu_offset but rewrite failed at %s:%d",
            RelativePathString(root, asm_path), static_cast<int>(i + 1)));
      }

      target_line = static_cast<int>(i + 1);
      old_row = parsed_row;
      old_col = parsed_col;
      old_line = line;
      new_line = absl::StrFormat("%s%d%s%d%s", match[1].str(), row,
                                 match[3].str(), col, match[5].str());
      break;
    }
    ++current_index;
  }

  if (target_line < 0) {
    return absl::NotFoundError(
        absl::StrFormat("Could not find %s[%d] in %s", table_label, index,
                        RelativePathString(root, asm_path)));
  }

  OracleMenuComponentEditResult result;
  result.asm_path = RelativePathString(root, asm_path);
  result.line = target_line;
  result.table_label = table_label;
  result.index = index;
  result.old_row = old_row;
  result.old_col = old_col;
  result.new_row = row;
  result.new_col = col;
  result.old_line = old_line;
  result.new_line = new_line;
  result.changed = (old_line != new_line);
  result.write_applied = false;

  if (!write_changes) {
    return result;
  }

  if (result.changed) {
    lines[static_cast<size_t>(target_line - 1)] = new_line;
    RETURN_IF_ERROR(WriteLines(asm_path, lines, newline, trailing_newline));
  }

  result.write_applied = true;
  return result;
}

}  // namespace yaze::core
