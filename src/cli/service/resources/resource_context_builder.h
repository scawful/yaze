#ifndef YAZE_CLI_SERVICE_RESOURCE_CONTEXT_BUILDER_H_
#define YAZE_CLI_SERVICE_RESOURCE_CONTEXT_BUILDER_H_

#include <map>
#include <string>

#include "absl/status/statusor.h"
#include "app/rom.h"

namespace yaze {
namespace cli {

/**
 * @brief Builds contextual information from ROM resources for AI prompts.
 *
 * This class extracts user-defined labels from the ROM's ResourceLabelManager
 * and formats them into human-readable context that can be injected into
 * AI prompts. This enables AI to use meaningful names like "eastern_palace"
 * instead of opaque IDs like "0x02".
 *
 * Example usage:
 *   ResourceContextBuilder builder(rom);
 *   std::string context = builder.BuildResourceContext().value();
 *   // Context contains formatted labels for all resource types
 */
class ResourceContextBuilder {
 public:
  explicit ResourceContextBuilder(Rom* rom) : rom_(rom) {}

  /**
   * @brief Build a complete resource context string for AI prompts.
   *
   * Extracts all ResourceLabels from the ROM and formats them into
   * a structured text format suitable for AI consumption.
   *
   * Example output:
   * ```
   * === AVAILABLE RESOURCES ===
   *
   * Overworld Maps:
   *   - 0: "Light World" (user: "hyrule_overworld")
   *   - 1: "Dark World" (user: "dark_world")
   *
   * Dungeons:
   *   - 0x00: "Hyrule Castle" (user: "castle")
   *   - 0x02: "Eastern Palace" (user: "east_palace")
   *
   * Common Tile16s:
   *   - 0x020: Grass
   *   - 0x02E: Tree
   *   - 0x14C: Water (top)
   * ```
   *
   * @return Formatted resource context string
   */
  absl::StatusOr<std::string> BuildResourceContext();

  /**
   * @brief Get labels for a specific resource category.
   *
   * @param category Resource type ("overworld", "dungeon", "entrance", etc.)
   * @return Map of ID -> label for that category
   */
  absl::StatusOr<std::map<std::string, std::string>> GetLabels(
      const std::string& category);

  /**
   * @brief Export all labels to JSON format.
   *
   * Creates a structured JSON representation of all resources
   * for potential use by AI services.
   *
   * @return JSON string with all resource labels
   */
  absl::StatusOr<std::string> ExportToJson();

 private:
  Rom* rom_;

  /**
   * @brief Extract overworld map labels.
   *
   * Returns formatted string like:
   * ```
   * Overworld Maps:
   *   - 0: "Light World" (user: "hyrule_overworld")
   *   - 1: "Dark World" (user: "dark_world")
   * ```
   */
  std::string ExtractOverworldLabels();

  /**
   * @brief Extract dungeon labels.
   *
   * Returns formatted string like:
   * ```
   * Dungeons:
   *   - 0x00: "Hyrule Castle" (user: "castle")
   *   - 0x02: "Eastern Palace" (user: "east_palace")
   * ```
   */
  std::string ExtractDungeonLabels();

  /**
   * @brief Extract entrance labels.
   *
   * Returns formatted string like:
   * ```
   * Entrances:
   *   - 0x00: "Link's House" (user: "starting_house")
   *   - 0x01: "Sanctuary" (user: "church")
   * ```
   */
  std::string ExtractEntranceLabels();

  /**
   * @brief Extract room labels.
   *
   * Returns formatted string like:
   * ```
   * Rooms:
   *   - 0x00_0x10: "Eastern Palace Boss Room"
   *   - 0x04_0x05: "Desert Palace Treasure Room"
   * ```
   */
  std::string ExtractRoomLabels();

  /**
   * @brief Extract sprite labels.
   *
   * Returns formatted string like:
   * ```
   * Sprites:
   *   - 0x00: "Soldier" (user: "green_soldier")
   *   - 0x01: "Octorok" (user: "red_octorok")
   * ```
   */
  std::string ExtractSpriteLabels();

  /**
   * @brief Add common tile16 reference for AI.
   *
   * Provides a quick reference of common tile16 IDs that AI
   * can use without needing to search through the entire tileset.
   *
   * Returns formatted string like:
   * ```
   * Common Tile16s:
   *   - 0x020: Grass
   *   - 0x022: Dirt
   *   - 0x02E: Tree
   *   - 0x14C: Water (top edge)
   *   - 0x14D: Water (middle)
   * ```
   */
  std::string GetCommonTile16Reference();
};

}  // namespace cli
}  // namespace yaze

#endif  // YAZE_CLI_SERVICE_RESOURCE_CONTEXT_BUILDER_H_
