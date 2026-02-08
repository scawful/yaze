#ifndef YAZE_CORE_HACK_MANIFEST_H
#define YAZE_CORE_HACK_MANIFEST_H

#include <cstdint>
#include <optional>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

#include "absl/status/status.h"

namespace yaze::core {

/**
 * @brief Ownership classification for ROM addresses and banks.
 *
 * The hack manifest distinguishes between addresses that:
 * - yaze can freely edit (vanilla data in the dev ROM)
 * - asar patches on every build (hook addresses — yaze edits are lost)
 * - asar fully owns (expanded banks — don't exist in dev ROM)
 * - both yaze and asar write (shared banks like ZSCustomOverworld)
 */
enum class AddressOwnership : uint8_t {
  kVanillaSafe,   // Yaze can edit; asar doesn't touch
  kHookPatched,   // Asar patches this; yaze edits are overwritten on build
  kAsmOwned,      // Entire bank owned by ASM hack
  kShared,        // Both yaze and ASM write (e.g., ZSCustomOverworld)
  kAsmExpansion,  // ROM expansion bank — only exists in patched ROM
  kRam,           // WRAM definition (not ROM data)
  kMirror,        // HiROM mirror of vanilla bank
};

std::string AddressOwnershipToString(AddressOwnership ownership);

/**
 * @brief A contiguous protected ROM region owned by the ASM hack.
 */
struct ProtectedRegion {
  uint32_t start;
  uint32_t end;
  int hook_count;
  std::string module;
};

/**
 * @brief An expanded bank with ownership classification.
 */
struct OwnedBank {
  uint8_t bank;
  uint32_t bank_start;
  uint32_t bank_end;
  AddressOwnership ownership;
  std::string ownership_note;
};

/**
 * @brief A room tag entry from the dispatch table.
 */
struct RoomTagEntry {
  uint8_t tag_id;
  uint32_t address;
  std::string name;
  std::string purpose;
  std::string source;
  std::string feature_flag;  // Empty if always enabled
  bool enabled = true;
};

/**
 * @brief A compile-time feature flag.
 */
struct FeatureFlag {
  std::string name;
  int value;
  bool enabled;
  std::string source;
};

/**
 * @brief A custom SRAM variable definition.
 */
struct SramVariable {
  std::string name;
  uint32_t address;
  std::string purpose;
};

/**
 * @brief Message range information for the expanded message system.
 */
struct MessageLayout {
  uint32_t hook_address;       // $0ED436
  uint32_t data_start;         // $2F8000
  uint32_t data_end;           // $2FFFFF
  uint16_t first_expanded_id;  // $18D
  uint16_t last_expanded_id;   // $1D1
  int expanded_count;
  int vanilla_count;  // 397
};

/**
 * @brief Build pipeline information.
 */
struct BuildPipeline {
  std::string dev_rom;
  std::string patched_rom;
  std::string assembler;
  std::string entry_point;
  std::string build_script;
};

/**
 * @brief A conflict detected when yaze wants to write to an ASM-owned address.
 */
struct WriteConflict {
  uint32_t address;
  AddressOwnership ownership;
  std::string module;  // From protected region, if applicable
};

// ─── Project Registry (Dungeon + Overworld data) ─────────────────────────

/**
 * @brief A room within a dungeon, with spatial and metadata info.
 */
struct DungeonRoom {
  int id;
  std::string name;
  int grid_row, grid_col;
  std::string type;  // "entrance", "boss", "mini_boss", "connector", "normal"
  int palette, blockset, spriteset;
  uint8_t tag1, tag2;
};

/**
 * @brief A connection between two rooms (stair, holewarp, or door).
 */
struct DungeonConnection {
  int from_room, to_room;
  std::string label;
  std::string direction;  // For doors: "north"/"south"/"east"/"west"
};

/**
 * @brief A complete dungeon entry with rooms and connections.
 */
struct DungeonEntry {
  std::string id;            // "D4"
  std::string name;          // "Zora Temple"
  std::string vanilla_name;  // "Thieves' Town"
  std::vector<DungeonRoom> rooms;
  std::vector<DungeonConnection> stairs;
  std::vector<DungeonConnection> holewarps;
  std::vector<DungeonConnection> doors;
};

/**
 * @brief An overworld area from the overworld registry.
 */
struct OverworldArea {
  int area_id;
  std::string name;
  std::string world;  // "LW", "DW", "SW"
  int grid_row, grid_col;
};

/**
 * @brief Project-level registry data loaded from dungeons.json + overworld.json.
 *
 * This data supplements the hack manifest with game-world structural info
 * that yaze uses for dungeon map visualization and room labeling.
 */
struct ProjectRegistry {
  std::vector<DungeonEntry> dungeons;
  std::vector<OverworldArea> overworld_areas;
  std::unordered_map<std::string, std::string> room_labels;  // "0x06" -> label
};

/**
 * @class HackManifest
 * @brief Loads and queries the hack manifest JSON for yaze-ASM integration.
 *
 * The manifest describes which ROM addresses belong to the ASM hack layer
 * versus the yaze editing layer. This enables yaze to:
 *
 * 1. Skip writing to hook addresses (asar overwrites them anyway)
 * 2. Warn when the user edits shared data that requires a rebuild
 * 3. Display room tag labels and SRAM variable names
 * 4. Show feature flag status in the project settings panel
 *
 * Usage:
 *   HackManifest manifest;
 *   auto status = manifest.LoadFromFile("hack_manifest.json");
 *   if (status.ok()) {
 *     auto ownership = manifest.ClassifyAddress(0x0085C4);
 *     // ownership == AddressOwnership::kHookPatched
 *
 *     auto tag_name = manifest.GetRoomTagLabel(0x39);
 *     // tag_name == "CustomTag"
 *   }
 */
class HackManifest {
 public:
  HackManifest() = default;

  /**
   * @brief Load manifest from a JSON file path.
   */
  absl::Status LoadFromFile(const std::string& filepath);

  /**
   * @brief Load manifest from a JSON string.
   */
  absl::Status LoadFromString(const std::string& json_content);

  /**
   * @brief Check if the manifest has been loaded.
   */
  [[nodiscard]] bool loaded() const { return loaded_; }

  /**
   * @brief Clear any loaded manifest state.
   *
   * This is primarily used when switching projects to ensure we never keep a
   * stale manifest across loads.
   */
  void Clear() { Reset(); }

  // ─── Address Classification ───────────────────────────────

  /**
   * @brief Classify a ROM address by ownership.
   *
   * Checks in order:
   * 1. Is it in an owned/shared/expansion bank?
   * 2. Is it in a protected region (vanilla hook)?
   * 3. Otherwise: vanilla safe
   */
  [[nodiscard]] AddressOwnership ClassifyAddress(uint32_t address) const;

  /**
   * @brief Check if a ROM write at this address would be overwritten by asar.
   *
   * Returns true for hook_patched, asm_owned, and asm_expansion addresses.
   * For shared addresses, returns false (yaze can write, but must rebuild).
   */
  [[nodiscard]] bool IsWriteOverwritten(uint32_t address) const;

  /**
   * @brief Check if an address is in a protected region.
   */
  [[nodiscard]] bool IsProtected(uint32_t address) const;

  /**
   * @brief Get the bank ownership for a given bank number.
   */
  [[nodiscard]] std::optional<AddressOwnership> GetBankOwnership(
      uint8_t bank) const;

  // ─── Room Tags ────────────────────────────────────────────

  /**
   * @brief Get the human-readable label for a room tag ID.
   */
  [[nodiscard]] std::string GetRoomTagLabel(uint8_t tag_id) const;

  /**
   * @brief Get the full room tag entry for a tag ID.
   */
  [[nodiscard]] std::optional<RoomTagEntry> GetRoomTag(uint8_t tag_id) const;

  /**
   * @brief Get all room tags.
   */
  [[nodiscard]] const std::vector<RoomTagEntry>& room_tags() const {
    return room_tags_;
  }

  // ─── Feature Flags ────────────────────────────────────────

  [[nodiscard]] bool IsFeatureEnabled(const std::string& flag_name) const;

  [[nodiscard]] const std::vector<FeatureFlag>& feature_flags() const {
    return feature_flags_;
  }

  // ─── SRAM ─────────────────────────────────────────────────

  [[nodiscard]] std::string GetSramVariableName(uint32_t address) const;

  [[nodiscard]] const std::vector<SramVariable>& sram_variables() const {
    return sram_variables_;
  }

  // ─── Messages ─────────────────────────────────────────────

  [[nodiscard]] const MessageLayout& message_layout() const {
    return message_layout_;
  }

  [[nodiscard]] bool IsExpandedMessage(uint16_t message_id) const;

  // ─── Protected Regions ──────────────────────────────────

  [[nodiscard]] const std::vector<ProtectedRegion>& protected_regions() const {
    return protected_regions_;
  }

  [[nodiscard]] const std::unordered_map<uint8_t, OwnedBank>& owned_banks()
      const {
    return owned_banks_;
  }

  // ─── Write Conflict Analysis ─────────────────────────────

  /**
   * @brief Analyze a set of address ranges for write conflicts.
   *
   * For each range [start, end), checks if any addresses are owned by the
   * ASM layer. Returns one WriteConflict per conflicting region (not per byte).
   * Useful for pre-save diagnostics in the dungeon/overworld editors.
   */
  [[nodiscard]] std::vector<WriteConflict> AnalyzeWriteRanges(
      const std::vector<std::pair<uint32_t, uint32_t>>& ranges) const;

  /**
   * @brief Analyze a set of PC-offset ranges for write conflicts.
   *
   * Editors typically operate on raw ROM offsets (PC). The hack manifest uses
   * SNES addresses (LoROM) as emitted by `org $XXXXXX` in ASM.
   *
   * This helper converts PC ranges to SNES ranges (splitting at LoROM bank
   * boundaries) and then runs the same conflict analysis.
   */
  [[nodiscard]] std::vector<WriteConflict> AnalyzePcWriteRanges(
      const std::vector<std::pair<uint32_t, uint32_t>>& pc_ranges) const;

  // ─── Project Registry ────────────────────────────────────

  /**
   * @brief Load project registry data (dungeons.json, overworld.json,
   * oracle_room_labels.json) from the code folder.
   */
  absl::Status LoadProjectRegistry(const std::string& code_folder);

  [[nodiscard]] const ProjectRegistry& project_registry() const {
    return project_registry_;
  }

  [[nodiscard]] bool HasProjectRegistry() const {
    return !project_registry_.dungeons.empty();
  }

  // ─── Build Pipeline ───────────────────────────────────────

  [[nodiscard]] const BuildPipeline& build_pipeline() const {
    return build_pipeline_;
  }

  // ─── Metadata ─────────────────────────────────────────────

  [[nodiscard]] const std::string& hack_name() const { return hack_name_; }
  [[nodiscard]] int manifest_version() const { return manifest_version_; }
  [[nodiscard]] int total_hooks() const { return total_hooks_; }

 private:
  void Reset();

  bool loaded_ = false;
  int manifest_version_ = 0;
  std::string hack_name_;
  int total_hooks_ = 0;

  // Protected regions (vanilla bank hooks) — sorted by start address
  std::vector<ProtectedRegion> protected_regions_;

  // Bank ownership lookup
  std::unordered_map<uint8_t, OwnedBank> owned_banks_;

  // Room tag lookup by tag ID
  std::unordered_map<uint8_t, RoomTagEntry> room_tag_map_;
  std::vector<RoomTagEntry> room_tags_;

  // Feature flags by name
  std::unordered_map<std::string, FeatureFlag> feature_flag_map_;
  std::vector<FeatureFlag> feature_flags_;

  // SRAM variable lookup by address
  std::unordered_map<uint32_t, SramVariable> sram_map_;
  std::vector<SramVariable> sram_variables_;

  // Message layout
  MessageLayout message_layout_{};

  // Build pipeline
  BuildPipeline build_pipeline_;

  // Project registry (loaded from code_folder JSON files)
  ProjectRegistry project_registry_;
};

}  // namespace yaze::core

#endif  // YAZE_CORE_HACK_MANIFEST_H
