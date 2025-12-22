#ifndef YAZE_SRC_CLI_SERVICE_AGENT_TOOLS_FILESYSTEM_TOOL_H_
#define YAZE_SRC_CLI_SERVICE_AGENT_TOOLS_FILESYSTEM_TOOL_H_

#include <filesystem>
#include <string>

#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "cli/service/resources/command_handler.h"

namespace yaze {
namespace cli {
namespace agent {
namespace tools {

/**
 * @brief Base class for filesystem operations
 *
 * Provides common functionality for filesystem tools including:
 * - Path validation and sanitization
 * - Project directory restriction
 * - Security checks (path traversal protection)
 */
class FileSystemToolBase : public resources::CommandHandler {
 protected:
  bool RequiresRom() const override { return false; }

  /**
   * @brief Validate and normalize a path for safe access
   *
   * Ensures the path:
   * - Is within the project directory
   * - Does not contain path traversal attempts (..)
   * - Is normalized to an absolute path
   */
  absl::StatusOr<std::filesystem::path> ValidatePath(const std::string& path_str) const;

  /**
   * @brief Get the project root directory
   *
   * Returns the root directory of the yaze project.
   * Defaults to current working directory if not explicitly set.
   */
  std::filesystem::path GetProjectRoot() const;

  /**
   * @brief Check if a path is within the project directory
   */
  bool IsPathInProject(const std::filesystem::path& path) const;

  /**
   * @brief Format file size for human-readable output
   */
  std::string FormatFileSize(uintmax_t size_bytes) const;

  /**
   * @brief Format timestamp for readable output
   */
  std::string FormatTimestamp(const std::filesystem::file_time_type& time) const;
};

/**
 * @brief List files and directories in a given path
 *
 * Usage: filesystem-list --path <directory> [--recursive] [--format <json|text>]
 *
 * Security: Restricted to project directory
 */
class FileSystemListTool : public FileSystemToolBase {
 public:
  std::string GetName() const override { return "filesystem-list"; }

  std::string GetDescription() const {
    return "List files and directories in a given path";
  }

  std::string GetUsage() const override {
    return "filesystem-list --path <directory> [--recursive] [--format <json|text>]";
  }

 protected:
  absl::Status ValidateArgs(const resources::ArgumentParser& parser) override;

  absl::Status Execute(Rom* rom, const resources::ArgumentParser& parser,
                      resources::OutputFormatter& formatter) override;

  bool RequiresLabels() const override { return false; }
};

/**
 * @brief Read the contents of a file
 *
 * Usage: filesystem-read --path <file> [--lines <count>] [--offset <start>] [--format <json|text>]
 *
 * Security: Restricted to project directory, text files only
 */
class FileSystemReadTool : public FileSystemToolBase {
 public:
  std::string GetName() const override { return "filesystem-read"; }

  std::string GetDescription() const {
    return "Read the contents of a file";
  }

  std::string GetUsage() const override {
    return "filesystem-read --path <file> [--lines <count>] [--offset <start>] [--format <json|text>]";
  }

 protected:
  absl::Status ValidateArgs(const resources::ArgumentParser& parser) override;

  absl::Status Execute(Rom* rom, const resources::ArgumentParser& parser,
                      resources::OutputFormatter& formatter) override;

  bool RequiresLabels() const override { return false; }

 private:
  /**
   * @brief Check if a file is likely text (not binary)
   */
  bool IsTextFile(const std::filesystem::path& path) const;
};

/**
 * @brief Check if a file or directory exists
 *
 * Usage: filesystem-exists --path <file|directory> [--format <json|text>]
 *
 * Security: Restricted to project directory
 */
class FileSystemExistsTool : public FileSystemToolBase {
 public:
  std::string GetName() const override { return "filesystem-exists"; }

  std::string GetDescription() const {
    return "Check if a file or directory exists";
  }

  std::string GetUsage() const override {
    return "filesystem-exists --path <file|directory> [--format <json|text>]";
  }

 protected:
  absl::Status ValidateArgs(const resources::ArgumentParser& parser) override;

  absl::Status Execute(Rom* rom, const resources::ArgumentParser& parser,
                      resources::OutputFormatter& formatter) override;

  bool RequiresLabels() const override { return false; }
};

/**
 * @brief Get detailed information about a file or directory
 *
 * Usage: filesystem-info --path <file|directory> [--format <json|text>]
 *
 * Returns: size, type, permissions, timestamps, etc.
 * Security: Restricted to project directory
 */
class FileSystemInfoTool : public FileSystemToolBase {
 public:
  std::string GetName() const override { return "filesystem-info"; }

  std::string GetDescription() const {
    return "Get detailed information about a file or directory";
  }

  std::string GetUsage() const override {
    return "filesystem-info --path <file|directory> [--format <json|text>]";
  }

 protected:
  absl::Status ValidateArgs(const resources::ArgumentParser& parser) override;

  absl::Status Execute(Rom* rom, const resources::ArgumentParser& parser,
                      resources::OutputFormatter& formatter) override;

  bool RequiresLabels() const override { return false; }

 private:
  /**
   * @brief Get permission string (Unix-style: rwxrwxrwx)
   */
  std::string GetPermissionString(const std::filesystem::path& path) const;
};

}  // namespace tools
}  // namespace agent
}  // namespace cli
}  // namespace yaze

#endif  // YAZE_SRC_CLI_SERVICE_AGENT_TOOLS_FILESYSTEM_TOOL_H_
