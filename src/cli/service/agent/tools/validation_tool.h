/**
 * @file validation_tool.h
 * @brief ROM validation and integrity checking tools for AI agents
 *
 * Provides tools for:
 * - ROM integrity checks (checksums, headers)
 * - Game data validation (sprite bounds, tile references)
 * - Compatibility checks for patches
 */

#ifndef YAZE_SRC_CLI_SERVICE_AGENT_TOOLS_VALIDATION_TOOL_H_
#define YAZE_SRC_CLI_SERVICE_AGENT_TOOLS_VALIDATION_TOOL_H_

#include <cstdint>
#include <string>
#include <vector>

#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "cli/service/resources/command_handler.h"

namespace yaze {
namespace cli {
namespace agent {
namespace tools {

/**
 * @brief Validation result with severity level
 */
struct ValidationIssue {
  enum class Severity { kInfo, kWarning, kError, kCritical };

  Severity severity;
  std::string category;  // "header", "checksum", "sprites", "tiles", etc.
  std::string message;
  uint32_t address = 0;  // Relevant address (if applicable)

  std::string SeverityString() const {
    switch (severity) {
      case Severity::kInfo:
        return "info";
      case Severity::kWarning:
        return "warning";
      case Severity::kError:
        return "error";
      case Severity::kCritical:
        return "critical";
    }
    return "unknown";
  }
};

/**
 * @brief Base class for validation tools
 */
class ValidationToolBase : public resources::CommandHandler {
 protected:
  /**
   * @brief Format validation issues as JSON
   */
  std::string FormatIssuesAsJson(
      const std::vector<ValidationIssue>& issues) const;

  /**
   * @brief Format validation issues as text
   */
  std::string FormatIssuesAsText(
      const std::vector<ValidationIssue>& issues) const;
};

/**
 * @brief Validate ROM header and checksums
 *
 * Checks:
 * - ROM size and type (LoROM/HiROM)
 * - Internal checksum validity
 * - Header fields (title, version, etc.)
 * - SNES header structure
 *
 * Usage: rom-validate --rom=<path> [--format=<json|text>]
 */
class RomValidateTool : public ValidationToolBase {
 public:
  std::string GetName() const override { return "rom-validate"; }

  std::string GetDescription() const {
    return "Validate ROM header, checksums, and structure";
  }

  std::string GetUsage() const override {
    return "rom-validate --rom=<path> [--format=<json|text>]";
  }

 protected:
  absl::Status ValidateArgs(const resources::ArgumentParser& parser) override {
    return parser.RequireArgs({"rom"});
  }

  absl::Status Execute(Rom* rom, const resources::ArgumentParser& parser,
                       resources::OutputFormatter& formatter) override;

 public:
  // Validation methods (public for use by ValidateAllTool)
  std::vector<ValidationIssue> ValidateHeader(Rom* rom);
  std::vector<ValidationIssue> ValidateChecksum(Rom* rom);
  std::vector<ValidationIssue> ValidateSize(Rom* rom);
};

/**
 * @brief Validate game data structures
 *
 * Checks:
 * - Sprite positions are within bounds
 * - Tile references point to valid tiles
 * - Palette indices are valid
 * - Entrance/exit data consistency
 *
 * Usage: data-validate --type=<sprites|tiles|palettes|all> [--format=<json|text>]
 */
class DataValidateTool : public ValidationToolBase {
 public:
  std::string GetName() const override { return "data-validate"; }

  std::string GetDescription() const {
    return "Validate game data structures (sprites, tiles, palettes)";
  }

  std::string GetUsage() const override {
    return "data-validate --type=<sprites|tiles|palettes|all> [--format=<json|text>]";
  }

 protected:
  absl::Status ValidateArgs(const resources::ArgumentParser& parser) override {
    return parser.RequireArgs({"type"});
  }

  absl::Status Execute(Rom* rom, const resources::ArgumentParser& parser,
                       resources::OutputFormatter& formatter) override;

 public:
  // Validation methods (public for use by ValidateAllTool)
  std::vector<ValidationIssue> ValidateSprites(Rom* rom);
  std::vector<ValidationIssue> ValidateTiles(Rom* rom);
  std::vector<ValidationIssue> ValidatePalettes(Rom* rom);
  std::vector<ValidationIssue> ValidateEntrances(Rom* rom);
};

/**
 * @brief Check ROM compatibility with patches
 *
 * Checks:
 * - Required free space regions
 * - Conflicting ASM hooks
 * - Version compatibility
 * - Expansion requirements
 *
 * Usage: patch-check --patch=<path> [--format=<json|text>]
 */
class PatchCheckTool : public ValidationToolBase {
 public:
  std::string GetName() const override { return "patch-check"; }

  std::string GetDescription() const {
    return "Check ROM compatibility with a patch file";
  }

  std::string GetUsage() const override {
    return "patch-check --patch=<path> [--format=<json|text>]";
  }

 protected:
  absl::Status ValidateArgs(const resources::ArgumentParser& parser) override {
    return parser.RequireArgs({"patch"});
  }

  absl::Status Execute(Rom* rom, const resources::ArgumentParser& parser,
                       resources::OutputFormatter& formatter) override;

 private:
  std::vector<ValidationIssue> CheckFreeSpace(Rom* rom);
  std::vector<ValidationIssue> CheckHooks(Rom* rom,
                                          const std::string& patch_path);
};

/**
 * @brief Run comprehensive validation suite
 *
 * Runs all validation checks and produces a detailed report.
 *
 * Usage: validate-all [--strict] [--format=<json|text>]
 */
class ValidateAllTool : public ValidationToolBase {
 public:
  std::string GetName() const override { return "validate-all"; }

  std::string GetDescription() const {
    return "Run comprehensive validation suite on ROM";
  }

  std::string GetUsage() const override {
    return "validate-all [--strict] [--format=<json|text>]";
  }

 protected:
  absl::Status ValidateArgs(
      const resources::ArgumentParser& /*parser*/) override {
    return absl::OkStatus();
  }

  absl::Status Execute(Rom* rom, const resources::ArgumentParser& parser,
                       resources::OutputFormatter& formatter) override;
};

}  // namespace tools
}  // namespace agent
}  // namespace cli
}  // namespace yaze

#endif  // YAZE_SRC_CLI_SERVICE_AGENT_TOOLS_VALIDATION_TOOL_H_

