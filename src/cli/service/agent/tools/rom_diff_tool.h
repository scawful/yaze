/**
 * @file rom_diff_tool.h
 * @brief ROM comparison and diff analysis tools for AI agents
 *
 * Provides semantic comparison between ROMs, identifying changes
 * in game elements like tiles, sprites, palettes, and code.
 */

#ifndef YAZE_SRC_CLI_SERVICE_AGENT_TOOLS_ROM_DIFF_TOOL_H_
#define YAZE_SRC_CLI_SERVICE_AGENT_TOOLS_ROM_DIFF_TOOL_H_

#include <cstdint>
#include <string>
#include <vector>

#include "absl/status/status.h"
#include "cli/service/resources/command_handler.h"

namespace yaze {
namespace cli {
namespace agent {
namespace tools {

/**
 * @brief A single difference between two ROMs
 */
struct RomDiff {
  uint32_t address;
  uint32_t length;
  std::vector<uint8_t> old_value;
  std::vector<uint8_t> new_value;
  std::string category;  // "tiles", "sprites", "code", "data", etc.
  std::string description;
};

/**
 * @brief Summary of differences between two ROMs
 */
struct DiffSummary {
  size_t total_bytes_changed = 0;
  size_t num_regions = 0;
  std::map<std::string, int> changes_by_category;
  std::vector<RomDiff> diffs;
};

/**
 * @brief Compare two ROM files and identify differences
 *
 * Usage: rom-diff --rom1=<path> --rom2=<path> [--semantic] [--format=<json|text>]
 */
class RomDiffTool : public resources::CommandHandler {
 public:
  std::string GetName() const override { return "rom-diff"; }

  std::string GetDescription() const {
    return "Compare two ROM files and identify differences";
  }

  std::string GetUsage() const override {
    return "rom-diff --rom1=<path> --rom2=<path> [--semantic] [--format=<json|text>]";
  }

 protected:
  absl::Status ValidateArgs(const resources::ArgumentParser& parser) override {
    return parser.RequireArgs({"rom1", "rom2"});
  }

  absl::Status Execute(Rom* rom, const resources::ArgumentParser& parser,
                       resources::OutputFormatter& formatter) override;

  bool RequiresRom() const override { return false; }

 private:
  DiffSummary ComputeDiff(const std::vector<uint8_t>& rom1,
                          const std::vector<uint8_t>& rom2,
                          bool semantic);

  std::string CategorizeAddress(uint32_t address) const;
  std::string DescribeChange(uint32_t address, const std::vector<uint8_t>& old_val,
                            const std::vector<uint8_t>& new_val) const;

  std::string FormatAsJson(const DiffSummary& summary) const;
  std::string FormatAsText(const DiffSummary& summary) const;
};

/**
 * @brief Analyze what game elements changed between two ROMs
 *
 * Provides higher-level semantic analysis of changes.
 *
 * Usage: rom-changes --rom1=<path> --rom2=<path> [--format=<json|text>]
 */
class RomChangesTool : public resources::CommandHandler {
 public:
  std::string GetName() const override { return "rom-changes"; }

  std::string GetDescription() const {
    return "Analyze what game elements changed between two ROMs";
  }

  std::string GetUsage() const override {
    return "rom-changes --rom1=<path> --rom2=<path> [--format=<json|text>]";
  }

 protected:
  absl::Status ValidateArgs(const resources::ArgumentParser& parser) override {
    return parser.RequireArgs({"rom1", "rom2"});
  }

  absl::Status Execute(Rom* rom, const resources::ArgumentParser& parser,
                       resources::OutputFormatter& formatter) override;

  bool RequiresRom() const override { return false; }
};

}  // namespace tools
}  // namespace agent
}  // namespace cli
}  // namespace yaze

#endif  // YAZE_SRC_CLI_SERVICE_AGENT_TOOLS_ROM_DIFF_TOOL_H_

