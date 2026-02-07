#ifndef YAZE_ZELDA3_RESOURCE_LABELS_H
#define YAZE_ZELDA3_RESOURCE_LABELS_H

#include <cstdint>
#include <functional>
#include <string>
#include <unordered_map>
#include <vector>

#include "absl/status/status.h"
#include "core/hack_manifest.h"

namespace yaze {
namespace zelda3 {

/**
 * @enum ResourceType
 * @brief Enumeration of all supported resource types for labeling
 */
enum class ResourceType {
  kSprite,
  kRoom,
  kEntrance,
  kItem,
  kOverlord,
  kOverworldMap,
  kMusic,
  kGraphics,
  kRoomEffect,
  kRoomTag,
  kTileType,
};

/**
 * @brief Convert ResourceType enum to string key for storage
 */
std::string ResourceTypeToString(ResourceType type);

/**
 * @brief Convert string key to ResourceType enum
 */
ResourceType StringToResourceType(const std::string& type_str);

/**
 * @class ResourceLabelProvider
 * @brief Unified interface for accessing resource labels with project overrides
 *
 * This class provides a centralized way to access resource labels (names) for
 * various game resources. It supports:
 * - Project-specific custom labels
 * - Hyrule Magic naming convention toggle
 * - Vanilla/default labels as fallback
 * - Import/export from ZScream DefaultNames.txt format
 *
 * Resolution Priority: Project Labels -> Hyrule Magic (if enabled) -> Vanilla
 */
class ResourceLabelProvider {
 public:
  using LabelMap = std::unordered_map<std::string, std::string>;
  using ProjectLabels = std::unordered_map<std::string, LabelMap>;

  ResourceLabelProvider() = default;

  /**
   * @brief Set the project labels reference (typically from YazeProject)
   */
  void SetProjectLabels(ProjectLabels* labels) { project_labels_ = labels; }

  /**
   * @brief Set the hack manifest reference for ASM-defined labels
   */
  void SetHackManifest(const core::HackManifest* manifest) {
    hack_manifest_ = manifest;
  }

  /**
   * @brief Get a label for a resource by type and ID
   * @param type The resource type
   * @param id The numeric ID of the resource
   * @return The resolved label string
   *
   * Resolution order:
   * 1. Project-specific label if set
   * 2. Hyrule Magic label if prefer_hmagic_ is true
   * 3. Vanilla/default label
   */
  std::string GetLabel(ResourceType type, int id) const;

  /**
   * @brief Get a label using string type key (for compatibility)
   */
  std::string GetLabel(const std::string& type_str, int id) const;

  /**
   * @brief Set a project-specific label override
   */
  void SetProjectLabel(ResourceType type, int id, const std::string& label);

  /**
   * @brief Clear a project-specific label (revert to default)
   */
  void ClearProjectLabel(ResourceType type, int id);

  /**
   * @brief Check if a project-specific label exists
   */
  bool HasProjectLabel(ResourceType type, int id) const;

  /**
   * @brief Get the vanilla (default) label for a resource
   */
  std::string GetVanillaLabel(ResourceType type, int id) const;

  /**
   * @brief Get the Hyrule Magic label for a resource (sprites only)
   */
  std::string GetHMagicLabel(ResourceType type, int id) const;

  // =========================================================================
  // Hyrule Magic Naming Toggle
  // =========================================================================

  /**
   * @brief Set whether to prefer Hyrule Magic sprite names
   */
  void SetPreferHMagicNames(bool prefer) { prefer_hmagic_ = prefer; }

  /**
   * @brief Get whether Hyrule Magic names are preferred
   */
  bool PreferHMagicNames() const { return prefer_hmagic_; }

  // =========================================================================
  // ZScream DefaultNames.txt Import/Export
  // =========================================================================

  /**
   * @brief Import labels from ZScream DefaultNames.txt format
   * @param content The file content to parse
   * @return Status indicating success or parse errors
   *
   * Parses sections:
   * - [Sprites Names] -> sprite labels
   * - [Rooms Names] -> room labels
   * - [Chests Items] -> item labels
   * - [Tags Names] -> room_tag labels
   */
  absl::Status ImportFromZScreamFormat(const std::string& content);

  /**
   * @brief Export project labels to ZScream DefaultNames.txt format
   * @return The formatted file content
   */
  std::string ExportToZScreamFormat() const;

  /**
   * @brief Import sprite labels from Oracle of Secrets registry.csv format
   * @param csv_content The CSV file content (name,id,paths,group,notes,allow_dupe)
   * @return Status indicating success or parse errors
   *
   * CSV format: name,id,paths,group,notes,allow_dupe
   * Example: Sprite_Manhandla,$88,Sprites/Bosses/manhandla.asm,manhandla,,
   */
  absl::Status ImportOracleSpriteRegistry(const std::string& csv_content);

  // =========================================================================
  // Utility Methods
  // =========================================================================

  /**
   * @brief Get the count of resources for a given type
   */
  int GetResourceCount(ResourceType type) const;

  /**
   * @brief Get all project labels for a given type
   */
  const LabelMap* GetProjectLabelsForType(ResourceType type) const;

  /**
   * @brief Clear all project labels
   */
  void ClearAllProjectLabels();

  /**
   * @brief Get all project labels (read-only)
   */
  const ProjectLabels* GetAllProjectLabels() const { return project_labels_; }

 private:
  // Project-specific label overrides (owned by YazeProject)
  ProjectLabels* project_labels_ = nullptr;

  // Hack manifest reference (owned by YazeProject)
  const core::HackManifest* hack_manifest_ = nullptr;

  // Whether to prefer Hyrule Magic sprite names
  bool prefer_hmagic_ = true;

  // Parse a single line in ZScream format
  bool ParseZScreamLine(const std::string& line, const std::string& section,
                        int& line_index);
};

// ============================================================================
// Global Provider Instance
// ============================================================================

/**
 * @brief Get the global ResourceLabelProvider instance
 *
 * This instance is shared across the application and should be initialized
 * with project labels when a project is opened.
 */
ResourceLabelProvider& GetResourceLabels();

/**
 * @brief Convenience function to get a sprite label
 */
inline std::string GetSpriteLabel(int id) {
  return GetResourceLabels().GetLabel(ResourceType::kSprite, id);
}

/**
 * @brief Convenience function to get a room label
 */
inline std::string GetRoomLabel(int id) {
  return GetResourceLabels().GetLabel(ResourceType::kRoom, id);
}

/**
 * @brief Convenience function to get an item label
 */
inline std::string GetItemLabel(int id) {
  return GetResourceLabels().GetLabel(ResourceType::kItem, id);
}

/**
 * @brief Convenience function to get an entrance label
 */
inline std::string GetEntranceLabel(int id) {
  return GetResourceLabels().GetLabel(ResourceType::kEntrance, id);
}

/**
 * @brief Convenience function to get an overlord label
 */
inline std::string GetOverlordLabel(int id) {
  return GetResourceLabels().GetLabel(ResourceType::kOverlord, id);
}

/**
 * @brief Convenience function to get an overworld map label
 */
inline std::string GetOverworldMapLabel(int id) {
  return GetResourceLabels().GetLabel(ResourceType::kOverworldMap, id);
}

/**
 * @brief Convenience function to get a music track label
 */
inline std::string GetMusicLabel(int id) {
  return GetResourceLabels().GetLabel(ResourceType::kMusic, id);
}

/**
 * @brief Convenience function to get a room tag label
 */
inline std::string GetRoomTagLabel(int id) {
  return GetResourceLabels().GetLabel(ResourceType::kRoomTag, id);
}

}  // namespace zelda3
}  // namespace yaze

#endif  // YAZE_ZELDA3_RESOURCE_LABELS_H
