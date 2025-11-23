#ifndef YAZE_SRC_CLI_SERVICE_AGENT_TOOLS_MEMORY_INSPECTOR_TOOL_H_
#define YAZE_SRC_CLI_SERVICE_AGENT_TOOLS_MEMORY_INSPECTOR_TOOL_H_

#include <cstdint>
#include <map>
#include <optional>
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
 * @brief ALTTP Memory Map Constants
 *
 * Known memory regions in A Link to the Past for intelligent analysis.
 */
struct ALTTPMemoryMap {
  // WRAM regions ($7E0000-$7FFFFF)
  static constexpr uint32_t kWRAMStart = 0x7E0000;
  static constexpr uint32_t kWRAMEnd = 0x7FFFFF;

  // System variables
  static constexpr uint32_t kGameMode = 0x7E0010;
  static constexpr uint32_t kSubmodule = 0x7E0011;
  static constexpr uint32_t kNmiFlag = 0x7E0012;
  static constexpr uint32_t kFrameCounter = 0x7E001A;

  // Player/Link state
  static constexpr uint32_t kLinkXLow = 0x7E0022;
  static constexpr uint32_t kLinkXHigh = 0x7E0023;
  static constexpr uint32_t kLinkYLow = 0x7E0020;
  static constexpr uint32_t kLinkYHigh = 0x7E0021;
  static constexpr uint32_t kLinkState = 0x7E005D;
  static constexpr uint32_t kLinkDirection = 0x7E002F;
  static constexpr uint32_t kLinkLayer = 0x7E00EE;

  // Sprite tables (16 sprites max)
  static constexpr uint32_t kSpriteYLow = 0x7E0D00;
  static constexpr uint32_t kSpriteXLow = 0x7E0D10;
  static constexpr uint32_t kSpriteYHigh = 0x7E0D20;
  static constexpr uint32_t kSpriteXHigh = 0x7E0D30;
  static constexpr uint32_t kSpriteState = 0x7E0DD0;
  static constexpr uint32_t kSpriteType = 0x7E0E20;
  static constexpr uint32_t kSpriteHealth = 0x7E0E50;
  static constexpr int kMaxSprites = 16;

  // OAM buffer
  static constexpr uint32_t kOAMBuffer = 0x7E0800;
  static constexpr uint32_t kOAMBufferEnd = 0x7E0A1F;

  // Save data / Inventory
  static constexpr uint32_t kSRAMStart = 0x7EF000;
  static constexpr uint32_t kSRAMEnd = 0x7EF4FF;
  static constexpr uint32_t kPlayerHealth = 0x7EF36D;
  static constexpr uint32_t kPlayerMaxHealth = 0x7EF36C;
  static constexpr uint32_t kPlayerRupees = 0x7EF360;
  static constexpr uint32_t kInventoryStart = 0x7EF340;

  // Current location
  static constexpr uint32_t kOverworldArea = 0x7E008A;
  static constexpr uint32_t kDungeonRoom = 0x7E00A0;
  static constexpr uint32_t kIndoors = 0x7E001B;

  // Check if address is in WRAM
  static bool IsWRAM(uint32_t addr) {
    return addr >= kWRAMStart && addr <= kWRAMEnd;
  }

  // Check if address is in sprite table
  static bool IsSpriteTable(uint32_t addr) {
    return addr >= 0x7E0D00 && addr <= 0x7E0FFF;
  }

  // Check if address is in save data
  static bool IsSaveData(uint32_t addr) {
    return addr >= kSRAMStart && addr <= kSRAMEnd;
  }
};

/**
 * @brief Memory region descriptor for AI-friendly output
 */
struct MemoryRegionInfo {
  std::string name;
  std::string description;
  uint32_t start_address;
  uint32_t end_address;
  std::string data_type;  // "byte", "word", "struct", "array"
};

/**
 * @brief Detected anomaly in memory
 */
struct MemoryAnomaly {
  uint32_t address;
  std::string type;        // "out_of_bounds", "null_pointer", "corruption"
  std::string description;
  int severity;  // 1-5, 5 being most severe
};

/**
 * @brief Pattern match result
 */
struct PatternMatch {
  uint32_t address;
  std::vector<uint8_t> matched_bytes;
  std::string context;  // Memory region name
};

/**
 * @brief Base class for memory inspection tools
 */
class MemoryInspectorBase : public resources::CommandHandler {
 protected:
  /**
   * @brief Get description for a memory address
   */
  std::string DescribeAddress(uint32_t address) const;

  /**
   * @brief Identify data type at address
   */
  std::string IdentifyDataType(uint32_t address) const;

  /**
   * @brief Get known memory regions
   */
  std::vector<MemoryRegionInfo> GetKnownRegions() const;

  /**
   * @brief Format bytes as hex string
   */
  std::string FormatHex(const std::vector<uint8_t>& data,
                        int bytes_per_line = 16) const;

  /**
   * @brief Format bytes as ASCII (printable chars only)
   */
  std::string FormatAscii(const std::vector<uint8_t>& data) const;

  /**
   * @brief Parse address from string (supports hex with $ or 0x prefix)
   */
  absl::StatusOr<uint32_t> ParseAddress(const std::string& addr_str) const;
};

/**
 * @brief Analyze a memory region with structure awareness
 *
 * Usage: memory-analyze --address <addr> --length <len> [--format <json|text>]
 *
 * Provides intelligent analysis of memory regions:
 * - Identifies known ALTTP structures (sprites, player state, etc.)
 * - Parses structured data into named fields
 * - Detects anomalies and potential issues
 */
class MemoryAnalyzeTool : public MemoryInspectorBase {
 public:
  std::string GetName() const override { return "memory-analyze"; }

  std::string GetDescription() const {
    return "Analyze a memory region with structure awareness";
  }

  std::string GetUsage() const override {
    return "memory-analyze --address <addr> --length <len> [--format <json|text>]";
  }

 protected:
  absl::Status ValidateArgs(const resources::ArgumentParser& parser) override;

  absl::Status Execute(Rom* rom, const resources::ArgumentParser& parser,
                       resources::OutputFormatter& formatter) override;

  bool RequiresLabels() const override { return false; }

 private:
  // Analyze sprite table entry
  std::map<std::string, std::string> AnalyzeSpriteEntry(
      int sprite_index, const std::vector<uint8_t>& wram) const;

  // Analyze player state
  std::map<std::string, std::string> AnalyzePlayerState(
      const std::vector<uint8_t>& wram) const;

  // Analyze game mode
  std::map<std::string, std::string> AnalyzeGameMode(
      const std::vector<uint8_t>& wram) const;
};

/**
 * @brief Search for byte patterns in memory
 *
 * Usage: memory-search --pattern <hex> [--start <addr>] [--end <addr>] [--format <json|text>]
 *
 * Features:
 * - Wildcard support (use ?? for any byte)
 * - Multiple match reporting
 * - Context-aware results
 */
class MemorySearchTool : public MemoryInspectorBase {
 public:
  std::string GetName() const override { return "memory-search"; }

  std::string GetDescription() const {
    return "Search for byte patterns in memory";
  }

  std::string GetUsage() const override {
    return "memory-search --pattern <hex> [--start <addr>] [--end <addr>] "
           "[--max-results <n>] [--format <json|text>]";
  }

 protected:
  absl::Status ValidateArgs(const resources::ArgumentParser& parser) override;

  absl::Status Execute(Rom* rom, const resources::ArgumentParser& parser,
                       resources::OutputFormatter& formatter) override;

  bool RequiresLabels() const override { return false; }

 private:
  // Parse pattern string (supports wildcards)
  absl::StatusOr<std::pair<std::vector<uint8_t>, std::vector<bool>>>
  ParsePattern(const std::string& pattern_str) const;

  // Match pattern against memory
  std::vector<PatternMatch> FindMatches(
      const std::vector<uint8_t>& memory, uint32_t base_address,
      const std::vector<uint8_t>& pattern,
      const std::vector<bool>& mask, int max_results) const;
};

/**
 * @brief Compare memory regions or detect changes
 *
 * Usage: memory-compare --address <addr> --expected <hex> [--format <json|text>]
 *    OR: memory-compare --address <addr> --baseline <addr2> --length <len>
 *
 * Features:
 * - Compare against expected values
 * - Compare two memory regions
 * - Highlight differences
 */
class MemoryCompareTool : public MemoryInspectorBase {
 public:
  std::string GetName() const override { return "memory-compare"; }

  std::string GetDescription() const {
    return "Compare memory regions or against expected values";
  }

  std::string GetUsage() const override {
    return "memory-compare --address <addr> --expected <hex> [--format <json|text>]";
  }

 protected:
  absl::Status ValidateArgs(const resources::ArgumentParser& parser) override;

  absl::Status Execute(Rom* rom, const resources::ArgumentParser& parser,
                       resources::OutputFormatter& formatter) override;

  bool RequiresLabels() const override { return false; }
};

/**
 * @brief Check memory for anomalies and corruption
 *
 * Usage: memory-check [--region <region_name>] [--format <json|text>]
 *
 * Scans known memory regions for:
 * - Out of bounds values
 * - Null pointer dereferences
 * - Corrupted structures
 * - Invalid sprite states
 */
class MemoryCheckTool : public MemoryInspectorBase {
 public:
  std::string GetName() const override { return "memory-check"; }

  std::string GetDescription() const {
    return "Check memory for anomalies and corruption";
  }

  std::string GetUsage() const override {
    return "memory-check [--region <region_name>] [--format <json|text>]";
  }

 protected:
  absl::Status ValidateArgs(const resources::ArgumentParser& parser) override;

  absl::Status Execute(Rom* rom, const resources::ArgumentParser& parser,
                       resources::OutputFormatter& formatter) override;

  bool RequiresLabels() const override { return false; }

 private:
  // Check sprite table for anomalies
  std::vector<MemoryAnomaly> CheckSpriteTable(
      const std::vector<uint8_t>& wram) const;

  // Check player state for anomalies
  std::vector<MemoryAnomaly> CheckPlayerState(
      const std::vector<uint8_t>& wram) const;

  // Check game mode for anomalies
  std::vector<MemoryAnomaly> CheckGameMode(
      const std::vector<uint8_t>& wram) const;
};

/**
 * @brief List known memory regions and their descriptions
 *
 * Usage: memory-regions [--filter <pattern>] [--format <json|text>]
 *
 * Provides a reference of known ALTTP memory locations.
 */
class MemoryRegionsTool : public MemoryInspectorBase {
 public:
  std::string GetName() const override { return "memory-regions"; }

  std::string GetDescription() const {
    return "List known memory regions and their descriptions";
  }

  std::string GetUsage() const override {
    return "memory-regions [--filter <pattern>] [--format <json|text>]";
  }

 protected:
  absl::Status ValidateArgs(const resources::ArgumentParser& parser) override;

  absl::Status Execute(Rom* rom, const resources::ArgumentParser& parser,
                       resources::OutputFormatter& formatter) override;

  bool RequiresLabels() const override { return false; }
};

}  // namespace tools
}  // namespace agent
}  // namespace cli
}  // namespace yaze

#endif  // YAZE_SRC_CLI_SERVICE_AGENT_TOOLS_MEMORY_INSPECTOR_TOOL_H_
