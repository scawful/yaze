#include "cli/service/agent/tools/filesystem_tool.h"

#include <chrono>
#include <fstream>
#include <iomanip>
#include <sstream>

#include "absl/strings/str_cat.h"
#include "absl/strings/str_format.h"
#include "absl/strings/str_split.h"

namespace yaze {
namespace cli {
namespace agent {
namespace tools {

namespace fs = std::filesystem;

// ============================================================================
// FileSystemToolBase Implementation
// ============================================================================

absl::StatusOr<fs::path> FileSystemToolBase::ValidatePath(
    const std::string& path_str) const {
  if (path_str.empty()) {
    return absl::InvalidArgumentError("Path cannot be empty");
  }

  // Check for path traversal attempts
  if (path_str.find("..") != std::string::npos) {
    return absl::InvalidArgumentError(
        "Path traversal (..) is not allowed for security reasons");
  }

  fs::path path;
  std::error_code ec;

  // Convert to absolute path
  if (fs::path(path_str).is_relative()) {
    path = fs::absolute(GetProjectRoot() / path_str, ec);
  } else {
    path = fs::absolute(path_str, ec);
  }

  if (ec) {
    return absl::InvalidArgumentError(
        absl::StrCat("Failed to resolve path: ", ec.message()));
  }

  // Normalize the path (resolve symlinks, remove redundant separators)
  path = fs::canonical(path, ec);
  if (ec && ec != std::errc::no_such_file_or_directory) {
    // Allow non-existent files for exists checks
    path = fs::weakly_canonical(path, ec);
    if (ec) {
      return absl::InvalidArgumentError(
          absl::StrCat("Failed to normalize path: ", ec.message()));
    }
  }

  // Verify the path is within the project directory
  if (!IsPathInProject(path)) {
    return absl::PermissionDeniedError(
        absl::StrCat("Access denied: Path '", path.string(),
                     "' is outside the project directory"));
  }

  return path;
}

fs::path FileSystemToolBase::GetProjectRoot() const {
  // Look for common project markers to find the root
  fs::path current = fs::current_path();
  fs::path root = current;

  // Walk up the directory tree looking for project markers
  while (!root.empty() && root != root.root_path()) {
    // Check for yaze-specific markers
    if (fs::exists(root / "CMakeLists.txt") &&
        fs::exists(root / "src" / "yaze.cc")) {
      return root;
    }
    // Also check for .git directory as a fallback
    if (fs::exists(root / ".git")) {
      // Verify this is the yaze project
      if (fs::exists(root / "src" / "cli") &&
          fs::exists(root / "src" / "app")) {
        return root;
      }
    }
    root = root.parent_path();
  }

  // Default to current directory if project root not found
  return current;
}

bool FileSystemToolBase::IsPathInProject(const fs::path& path) const {
  fs::path project_root = GetProjectRoot();
  fs::path normalized_path = fs::weakly_canonical(path);
  fs::path normalized_root = fs::canonical(project_root);

  // Check if path starts with project root
  auto path_str = normalized_path.string();
  auto root_str = normalized_root.string();

  return path_str.find(root_str) == 0;
}

std::string FileSystemToolBase::FormatFileSize(uintmax_t size_bytes) const {
  const char* units[] = {"B", "KB", "MB", "GB", "TB"};
  int unit_index = 0;
  double size = static_cast<double>(size_bytes);

  while (size >= 1024.0 && unit_index < 4) {
    size /= 1024.0;
    unit_index++;
  }

  if (unit_index == 0) {
    return absl::StrFormat("%d %s", static_cast<int>(size), units[unit_index]);
  } else {
    return absl::StrFormat("%.2f %s", size, units[unit_index]);
  }
}

std::string FileSystemToolBase::FormatTimestamp(
    const fs::file_time_type& time) const {
  // Convert file_time_type to system_clock time_point
  auto sctp = std::chrono::time_point_cast<std::chrono::system_clock::duration>(
      time - fs::file_time_type::clock::now() +
      std::chrono::system_clock::now());

  // Convert to time_t for formatting
  std::time_t tt = std::chrono::system_clock::to_time_t(sctp);

  // Format the time
  std::stringstream ss;
  ss << std::put_time(std::localtime(&tt), "%Y-%m-%d %H:%M:%S");
  return ss.str();
}

// ============================================================================
// FileSystemListTool Implementation
// ============================================================================

absl::Status FileSystemListTool::ValidateArgs(
    const resources::ArgumentParser& parser) {
  return parser.RequireArgs({"path"});
}

absl::Status FileSystemListTool::Execute(
    Rom* rom, const resources::ArgumentParser& parser,
    resources::OutputFormatter& formatter) {

  auto path_str = parser.GetString("path").value_or(".");
  bool recursive = parser.HasFlag("recursive");

  // Validate and normalize the path
  auto path_result = ValidatePath(path_str);
  if (!path_result.ok()) {
    return path_result.status();
  }
  fs::path dir_path = *path_result;

  // Check if the path exists and is a directory
  std::error_code ec;
  if (!fs::exists(dir_path, ec)) {
    return absl::NotFoundError(
        absl::StrCat("Directory not found: ", dir_path.string()));
  }

  if (!fs::is_directory(dir_path, ec)) {
    return absl::InvalidArgumentError(
        absl::StrCat("Path is not a directory: ", dir_path.string()));
  }

  formatter.BeginObject("Directory Listing");
  formatter.AddField("path", dir_path.string());
  formatter.AddField("recursive", recursive ? "true" : "false");

  std::vector<std::map<std::string, std::string>> entries;

  // List directory contents
  if (recursive) {
    for (const auto& entry : fs::recursive_directory_iterator(
             dir_path, fs::directory_options::skip_permission_denied, ec)) {
      if (ec) {
        continue;  // Skip inaccessible entries
      }

      std::map<std::string, std::string> file_info;
      file_info["name"] = entry.path().filename().string();
      file_info["path"] = fs::relative(entry.path(), dir_path).string();
      file_info["type"] = entry.is_directory() ? "directory" : "file";

      if (entry.is_regular_file()) {
        file_info["size"] = FormatFileSize(entry.file_size());
      }

      entries.push_back(file_info);
    }
  } else {
    for (const auto& entry : fs::directory_iterator(
             dir_path, fs::directory_options::skip_permission_denied, ec)) {
      if (ec) {
        continue;  // Skip inaccessible entries
      }

      std::map<std::string, std::string> file_info;
      file_info["name"] = entry.path().filename().string();
      file_info["type"] = entry.is_directory() ? "directory" : "file";

      if (entry.is_regular_file()) {
        file_info["size"] = FormatFileSize(entry.file_size());
      }

      entries.push_back(file_info);
    }
  }

  // Sort entries: directories first, then files, alphabetically
  std::sort(entries.begin(), entries.end(),
            [](const auto& a, const auto& b) {
              if (a.at("type") != b.at("type")) {
                return a.at("type") == "directory";
              }
              return a.at("name") < b.at("name");
            });

  // Add entries to formatter
  formatter.BeginArray("entries");
  for (const auto& entry : entries) {
    formatter.BeginObject();
    for (const auto& [key, value] : entry) {
      formatter.AddField(key, value);
    }
    formatter.EndObject();
  }
  formatter.EndArray();

  formatter.AddField("total_entries", std::to_string(entries.size()));
  formatter.EndObject();

  return absl::OkStatus();
}

// ============================================================================
// FileSystemReadTool Implementation
// ============================================================================

absl::Status FileSystemReadTool::ValidateArgs(
    const resources::ArgumentParser& parser) {
  return parser.RequireArgs({"path"});
}

absl::Status FileSystemReadTool::Execute(
    Rom* rom, const resources::ArgumentParser& parser,
    resources::OutputFormatter& formatter) {

  auto path_str = parser.GetString("path").value();
  int max_lines = parser.GetInt("lines").value_or(-1);
  int offset = parser.GetInt("offset").value_or(0);

  // Validate and normalize the path
  auto path_result = ValidatePath(path_str);
  if (!path_result.ok()) {
    return path_result.status();
  }
  fs::path file_path = *path_result;

  // Check if the file exists and is a regular file
  std::error_code ec;
  if (!fs::exists(file_path, ec)) {
    return absl::NotFoundError(
        absl::StrCat("File not found: ", file_path.string()));
  }

  if (!fs::is_regular_file(file_path, ec)) {
    return absl::InvalidArgumentError(
        absl::StrCat("Path is not a file: ", file_path.string()));
  }

  // Check if it's a text file
  if (!IsTextFile(file_path)) {
    return absl::InvalidArgumentError(
        absl::StrCat("File appears to be binary: ", file_path.string(),
                     ". Only text files can be read."));
  }

  // Read the file
  std::ifstream file(file_path, std::ios::in);
  if (!file) {
    return absl::InternalError(
        absl::StrCat("Failed to open file: ", file_path.string()));
  }

  formatter.BeginObject("File Contents");
  formatter.AddField("path", file_path.string());
  formatter.AddField("size", FormatFileSize(fs::file_size(file_path)));

  std::vector<std::string> lines;
  std::string line;
  int line_num = 0;

  // Skip to offset
  while (line_num < offset && std::getline(file, line)) {
    line_num++;
  }

  // Read lines
  while (std::getline(file, line)) {
    if (max_lines > 0 && lines.size() >= static_cast<size_t>(max_lines)) {
      break;
    }
    lines.push_back(line);
  }

  formatter.AddField("lines_read", std::to_string(lines.size()));
  formatter.AddField("starting_line", std::to_string(offset + 1));

  // Add content
  if (parser.GetString("format").value_or("text") == "json") {
    formatter.BeginArray("content");
    for (const auto& content_line : lines) {
      formatter.AddArrayItem(content_line);
    }
    formatter.EndArray();
  } else {
    std::stringstream content;
    for (size_t i = 0; i < lines.size(); ++i) {
      content << lines[i];
      if (i < lines.size() - 1) {
        content << "\n";
      }
    }
    formatter.AddField("content", content.str());
  }

  formatter.EndObject();

  return absl::OkStatus();
}

bool FileSystemReadTool::IsTextFile(const fs::path& path) const {
  // Check file extension first
  std::string ext = path.extension().string();
  std::transform(ext.begin(), ext.end(), ext.begin(), ::tolower);

  // Common text file extensions
  std::set<std::string> text_extensions = {
      ".txt", ".md", ".cc", ".cpp", ".c", ".h", ".hpp", ".py", ".js", ".ts",
      ".json", ".xml", ".yaml", ".yml", ".toml", ".ini", ".cfg", ".conf",
      ".sh", ".bash", ".zsh", ".fish", ".cmake", ".mk", ".makefile",
      ".html", ".css", ".scss", ".sass", ".less", ".jsx", ".tsx",
      ".rs", ".go", ".java", ".kt", ".swift", ".rb", ".pl", ".php",
      ".lua", ".vim", ".el", ".lisp", ".clj", ".hs", ".ml", ".fs",
      ".asm", ".s", ".S", ".proto", ".thrift", ".graphql", ".sql",
      ".gitignore", ".dockerignore", ".editorconfig", ".eslintrc"
  };

  if (text_extensions.count(ext) > 0) {
    return true;
  }

  // For unknown extensions, check the first few bytes
  std::ifstream file(path, std::ios::binary);
  if (!file) {
    return false;
  }

  // Read first 512 bytes to check for binary content
  char buffer[512];
  file.read(buffer, sizeof(buffer));
  std::streamsize bytes_read = file.gcount();

  // Check for null bytes (common in binary files)
  for (std::streamsize i = 0; i < bytes_read; ++i) {
    if (buffer[i] == '\0') {
      return false;  // Binary file
    }
    // Also check for other non-printable characters
    // (excluding common whitespace)
    if (!std::isprint(buffer[i]) &&
        buffer[i] != '\n' &&
        buffer[i] != '\r' &&
        buffer[i] != '\t') {
      return false;
    }
  }

  return true;
}

// ============================================================================
// FileSystemExistsTool Implementation
// ============================================================================

absl::Status FileSystemExistsTool::ValidateArgs(
    const resources::ArgumentParser& parser) {
  return parser.RequireArgs({"path"});
}

absl::Status FileSystemExistsTool::Execute(
    Rom* rom, const resources::ArgumentParser& parser,
    resources::OutputFormatter& formatter) {

  auto path_str = parser.GetString("path").value();

  // Validate and normalize the path
  auto path_result = ValidatePath(path_str);
  if (!path_result.ok()) {
    // For exists check, we want to handle permission denied specially
    if (absl::IsPermissionDenied(path_result.status())) {
      return path_result.status();
    }
    // Other errors might mean the file doesn't exist
    formatter.BeginObject("File Exists Check");
    formatter.AddField("path", path_str);
    formatter.AddField("exists", "false");
    formatter.AddField("error", std::string(path_result.status().message()));
    formatter.EndObject();
    return absl::OkStatus();
  }

  fs::path check_path = *path_result;
  std::error_code ec;
  bool exists = fs::exists(check_path, ec);

  formatter.BeginObject("File Exists Check");
  formatter.AddField("path", check_path.string());
  formatter.AddField("exists", exists ? "true" : "false");

  if (exists) {
    if (fs::is_directory(check_path, ec)) {
      formatter.AddField("type", "directory");
    } else if (fs::is_regular_file(check_path, ec)) {
      formatter.AddField("type", "file");
    } else if (fs::is_symlink(check_path, ec)) {
      formatter.AddField("type", "symlink");
    } else {
      formatter.AddField("type", "other");
    }
  }

  formatter.EndObject();

  return absl::OkStatus();
}

// ============================================================================
// FileSystemInfoTool Implementation
// ============================================================================

absl::Status FileSystemInfoTool::ValidateArgs(
    const resources::ArgumentParser& parser) {
  return parser.RequireArgs({"path"});
}

absl::Status FileSystemInfoTool::Execute(
    Rom* rom, const resources::ArgumentParser& parser,
    resources::OutputFormatter& formatter) {

  auto path_str = parser.GetString("path").value();

  // Validate and normalize the path
  auto path_result = ValidatePath(path_str);
  if (!path_result.ok()) {
    return path_result.status();
  }
  fs::path info_path = *path_result;

  // Check if the path exists
  std::error_code ec;
  if (!fs::exists(info_path, ec)) {
    return absl::NotFoundError(
        absl::StrCat("Path not found: ", info_path.string()));
  }

  formatter.BeginObject("File Information");
  formatter.AddField("path", info_path.string());
  formatter.AddField("name", info_path.filename().string());
  formatter.AddField("parent", info_path.parent_path().string());

  // Type
  if (fs::is_directory(info_path, ec)) {
    formatter.AddField("type", "directory");

    // Count entries in directory
    size_t entry_count = 0;
    for (auto& _ : fs::directory_iterator(info_path, ec)) {
      entry_count++;
    }
    formatter.AddField("entries", std::to_string(entry_count));
  } else if (fs::is_regular_file(info_path, ec)) {
    formatter.AddField("type", "file");
    formatter.AddField("extension", info_path.extension().string());

    // File size
    auto size = fs::file_size(info_path, ec);
    formatter.AddField("size_bytes", std::to_string(size));
    formatter.AddField("size", FormatFileSize(size));
  } else if (fs::is_symlink(info_path, ec)) {
    formatter.AddField("type", "symlink");
    auto target = fs::read_symlink(info_path, ec);
    if (!ec) {
      formatter.AddField("target", target.string());
    }
  } else {
    formatter.AddField("type", "other");
  }

  // Timestamps
  auto last_write = fs::last_write_time(info_path, ec);
  if (!ec) {
    formatter.AddField("modified", FormatTimestamp(last_write));
  }

  // Permissions
  formatter.AddField("permissions", GetPermissionString(info_path));

  // Additional info
  formatter.AddField("absolute_path", fs::absolute(info_path).string());
  formatter.AddField("is_hidden",
                     info_path.filename().string().starts_with(".") ? "true" : "false");

  formatter.EndObject();

  return absl::OkStatus();
}

std::string FileSystemInfoTool::GetPermissionString(
    const fs::path& path) const {
  std::error_code ec;
  auto perms = fs::status(path, ec).permissions();

  if (ec) {
    return "unknown";
  }

  std::string result;

  // Owner permissions
  result += (perms & fs::perms::owner_read) != fs::perms::none ? 'r' : '-';
  result += (perms & fs::perms::owner_write) != fs::perms::none ? 'w' : '-';
  result += (perms & fs::perms::owner_exec) != fs::perms::none ? 'x' : '-';

  // Group permissions
  result += (perms & fs::perms::group_read) != fs::perms::none ? 'r' : '-';
  result += (perms & fs::perms::group_write) != fs::perms::none ? 'w' : '-';
  result += (perms & fs::perms::group_exec) != fs::perms::none ? 'x' : '-';

  // Others permissions
  result += (perms & fs::perms::others_read) != fs::perms::none ? 'r' : '-';
  result += (perms & fs::perms::others_write) != fs::perms::none ? 'w' : '-';
  result += (perms & fs::perms::others_exec) != fs::perms::none ? 'x' : '-';

  return result;
}

}  // namespace tools
}  // namespace agent
}  // namespace cli
}  // namespace yaze