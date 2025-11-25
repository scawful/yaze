/**
 * @file project_tool.h
 * @brief Project management tools for AI agents
 *
 * Provides tools for:
 * - Project state management (snapshots, versioning)
 * - Edit delta tracking and rollback
 * - Project export/import with checksums
 * - Project comparison and diff
 */

#ifndef YAZE_SRC_CLI_SERVICE_AGENT_TOOLS_PROJECT_TOOL_H_
#define YAZE_SRC_CLI_SERVICE_AGENT_TOOLS_PROJECT_TOOL_H_

#include <array>
#include <chrono>
#include <cstdint>
#include <map>
#include <string>
#include <vector>

#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "cli/service/agent/agent_context.h"
#include "cli/service/resources/command_handler.h"

namespace yaze {
namespace cli {
namespace agent {
namespace tools {

/**
 * @brief Binary format for .edits file
 *
 * File structure:
 * - Header (44 bytes)
 * - Edit records (variable length)
 *   - SerializedEdit header (8 bytes)
 *   - old_value bytes
 *   - new_value bytes
 */
struct EditFileHeader {
  uint32_t magic = 0x59415A45;  // "YAZE" in hex
  uint32_t version = 1;
  uint32_t edit_count;
  std::array<uint8_t, 32> base_rom_sha256;

  static constexpr uint32_t kMagic = 0x59415A45;
  static constexpr uint32_t kCurrentVersion = 1;
};

/**
 * @brief Serialized edit record in binary format
 */
struct SerializedEdit {
  uint32_t address;
  uint32_t length;
  // Followed by: old_value[length], new_value[length]
};

/**
 * @brief Project snapshot with edit deltas
 *
 * Snapshots store only edit deltas, not full ROM copies.
 * Includes checksum validation against base ROM.
 */
struct ProjectSnapshot {
  std::string name;
  std::string description;
  std::chrono::system_clock::time_point created;
  std::vector<RomEdit> edits;  // From AgentContext
  std::map<std::string, std::string> metadata;
  std::array<uint8_t, 32> rom_checksum;  // SHA-256 of base ROM

  // Serialization
  absl::Status SaveToFile(const std::string& filepath) const;
  static absl::StatusOr<ProjectSnapshot> LoadFromFile(
      const std::string& filepath);
};

/**
 * @brief Project manager for snapshot and version control
 *
 * Manages project state including:
 * - Named snapshots (checkpoints)
 * - Edit delta tracking
 * - ROM checksum validation
 * - Project metadata
 */
class ProjectManager {
 public:
  ProjectManager() = default;

  /**
   * @brief Initialize project directory structure
   * @param base_path Base directory for project (creates .yaze-project/)
   */
  absl::Status Initialize(const std::string& base_path);

  /**
   * @brief Create a named snapshot of current state
   * @param name Snapshot name
   * @param description Optional description
   * @param edits Edit list to snapshot
   * @param rom_checksum SHA-256 of base ROM
   */
  absl::Status CreateSnapshot(const std::string& name,
                               const std::string& description,
                               const std::vector<RomEdit>& edits,
                               const std::array<uint8_t, 32>& rom_checksum);

  /**
   * @brief Restore ROM to a named snapshot
   * @param name Snapshot name
   * @param rom ROM instance to restore
   */
  absl::Status RestoreSnapshot(const std::string& name, Rom* rom);

  /**
   * @brief List all available snapshots
   */
  std::vector<std::string> ListSnapshots() const;

  /**
   * @brief Get snapshot details
   * @param name Snapshot name
   */
  absl::StatusOr<ProjectSnapshot> GetSnapshot(const std::string& name) const;

  /**
   * @brief Delete a snapshot
   * @param name Snapshot name
   */
  absl::Status DeleteSnapshot(const std::string& name);

  /**
   * @brief Export project as portable archive
   * @param export_path Path for exported archive
   * @param include_rom Include ROM file in export
   */
  absl::Status ExportProject(const std::string& export_path,
                              bool include_rom = false);

  /**
   * @brief Import project archive
   * @param archive_path Path to project archive
   */
  absl::Status ImportProject(const std::string& archive_path);

  /**
   * @brief Compare two snapshots
   * @param snapshot1 First snapshot name
   * @param snapshot2 Second snapshot name
   */
  absl::StatusOr<std::string> DiffSnapshots(const std::string& snapshot1,
                                             const std::string& snapshot2) const;

  /**
   * @brief Get project directory path
   */
  const std::string& GetProjectPath() const { return project_path_; }

  /**
   * @brief Check if project is initialized
   */
  bool IsInitialized() const { return !project_path_.empty(); }

 private:
  std::string project_path_;
  std::string snapshots_path_;
  std::map<std::string, ProjectSnapshot> snapshots_;

  absl::Status LoadSnapshots();
  absl::Status SaveProjectMetadata();
  std::string GetSnapshotFilePath(const std::string& name) const;
};

/**
 * @brief Utility functions for checksums and serialization
 */
class ProjectToolUtils {
 public:
  /**
   * @brief Compute SHA-256 checksum of ROM data
   * @param rom ROM instance
   * @return 32-byte SHA-256 hash
   */
  static std::array<uint8_t, 32> ComputeRomChecksum(const Rom* rom);

  /**
   * @brief Compute SHA-256 checksum of arbitrary data
   * @param data Data buffer
   * @param length Data length
   * @return 32-byte SHA-256 hash
   */
  static std::array<uint8_t, 32> ComputeSHA256(const uint8_t* data,
                                                size_t length);

  /**
   * @brief Format checksum as hex string
   * @param checksum 32-byte checksum
   * @return Hex string (64 characters)
   */
  static std::string FormatChecksum(const std::array<uint8_t, 32>& checksum);

  /**
   * @brief Format timestamp for display
   * @param time Time point
   * @return Formatted string (ISO 8601)
   */
  static std::string FormatTimestamp(
      const std::chrono::system_clock::time_point& time);

  /**
   * @brief Parse timestamp from string
   * @param timestamp ISO 8601 timestamp string
   * @return Time point
   */
  static absl::StatusOr<std::chrono::system_clock::time_point> ParseTimestamp(
      const std::string& timestamp);
};

/**
 * @brief Base class for project tools
 */
class ProjectToolBase : public resources::CommandHandler {
 protected:
  /**
   * @brief Get or create project manager from context
   */
  absl::StatusOr<ProjectManager*> GetProjectManager(
      AgentContext* context) const;

  /**
   * @brief Format edit list as text
   */
  std::string FormatEdits(const std::vector<RomEdit>& edits) const;

  /**
   * @brief Format snapshot info as text
   */
  std::string FormatSnapshot(const ProjectSnapshot& snapshot) const;
};

/**
 * @brief Show current project state and pending edits
 *
 * Usage: project-status [--format <json|text>]
 *
 * Displays:
 * - Project path and status
 * - Number of pending edits
 * - Recent snapshots
 * - ROM checksum
 */
class ProjectStatusTool : public ProjectToolBase {
 public:
  std::string GetName() const override { return "project-status"; }

  std::string GetDescription() const {
    return "Show current project state and pending edits";
  }

  std::string GetUsage() const override {
    return "project-status [--format <json|text>]";
  }

  bool RequiresLabels() const override { return false; }

 protected:
  absl::Status ValidateArgs(
      const resources::ArgumentParser& /*parser*/) override {
    return absl::OkStatus();
  }

  absl::Status Execute(Rom* rom, const resources::ArgumentParser& parser,
                       resources::OutputFormatter& formatter) override;
};

/**
 * @brief Create named checkpoint with edit deltas
 *
 * Usage: project-snapshot --name <name> [--description <desc>] [--format <json|text>]
 *
 * Creates a snapshot of current edit state.
 * Stores only deltas, not full ROM copy.
 */
class ProjectSnapshotTool : public ProjectToolBase {
 public:
  std::string GetName() const override { return "project-snapshot"; }

  std::string GetDescription() const {
    return "Create named checkpoint with edit deltas";
  }

  std::string GetUsage() const override {
    return "project-snapshot --name <name> [--description <desc>] [--format <json|text>]";
  }

  bool RequiresLabels() const override { return false; }

 protected:
  absl::Status ValidateArgs(const resources::ArgumentParser& parser) override {
    return parser.RequireArgs({"name"});
  }

  absl::Status Execute(Rom* rom, const resources::ArgumentParser& parser,
                       resources::OutputFormatter& formatter) override;
};

/**
 * @brief Restore ROM to named checkpoint
 *
 * Usage: project-restore --name <name> [--format <json|text>]
 *
 * Restores ROM to a previously saved snapshot.
 * Validates ROM checksum before applying edits.
 */
class ProjectRestoreTool : public ProjectToolBase {
 public:
  std::string GetName() const override { return "project-restore"; }

  std::string GetDescription() const {
    return "Restore ROM to named checkpoint";
  }

  std::string GetUsage() const override {
    return "project-restore --name <name> [--format <json|text>]";
  }

  bool RequiresLabels() const override { return false; }

 protected:
  absl::Status ValidateArgs(const resources::ArgumentParser& parser) override {
    return parser.RequireArgs({"name"});
  }

  absl::Status Execute(Rom* rom, const resources::ArgumentParser& parser,
                       resources::OutputFormatter& formatter) override;
};

/**
 * @brief Export project as portable archive
 *
 * Usage: project-export --path <path> [--include-rom] [--format <json|text>]
 *
 * Exports project with all snapshots and metadata.
 * Optionally includes ROM file.
 */
class ProjectExportTool : public ProjectToolBase {
 public:
  std::string GetName() const override { return "project-export"; }

  std::string GetDescription() const {
    return "Export project as portable archive";
  }

  std::string GetUsage() const override {
    return "project-export --path <path> [--include-rom] [--format <json|text>]";
  }

  bool RequiresLabels() const override { return false; }

 protected:
  absl::Status ValidateArgs(const resources::ArgumentParser& parser) override {
    return parser.RequireArgs({"path"});
  }

  absl::Status Execute(Rom* rom, const resources::ArgumentParser& parser,
                       resources::OutputFormatter& formatter) override;
};

/**
 * @brief Import project archive
 *
 * Usage: project-import --path <path> [--format <json|text>]
 *
 * Imports a previously exported project archive.
 */
class ProjectImportTool : public ProjectToolBase {
 public:
  std::string GetName() const override { return "project-import"; }

  std::string GetDescription() const { return "Import project archive"; }

  std::string GetUsage() const override {
    return "project-import --path <path> [--format <json|text>]";
  }

  bool RequiresLabels() const override { return false; }

 protected:
  absl::Status ValidateArgs(const resources::ArgumentParser& parser) override {
    return parser.RequireArgs({"path"});
  }

  absl::Status Execute(Rom* rom, const resources::ArgumentParser& parser,
                       resources::OutputFormatter& formatter) override;
};

/**
 * @brief Compare two project states
 *
 * Usage: project-diff --snapshot1 <name> --snapshot2 <name> [--format <json|text>]
 *
 * Compares two snapshots and shows differences.
 */
class ProjectDiffTool : public ProjectToolBase {
 public:
  std::string GetName() const override { return "project-diff"; }

  std::string GetDescription() const { return "Compare two project states"; }

  std::string GetUsage() const override {
    return "project-diff --snapshot1 <name> --snapshot2 <name> [--format <json|text>]";
  }

  bool RequiresLabels() const override { return false; }

 protected:
  absl::Status ValidateArgs(const resources::ArgumentParser& parser) override {
    return parser.RequireArgs({"snapshot1", "snapshot2"});
  }

  absl::Status Execute(Rom* rom, const resources::ArgumentParser& parser,
                       resources::OutputFormatter& formatter) override;
};

}  // namespace tools
}  // namespace agent
}  // namespace cli
}  // namespace yaze

#endif  // YAZE_SRC_CLI_SERVICE_AGENT_TOOLS_PROJECT_TOOL_H_
