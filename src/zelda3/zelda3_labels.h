#ifndef YAZE_APP_ZELDA3_ZELDA3_LABELS_H
#define YAZE_APP_ZELDA3_ZELDA3_LABELS_H

#include <cstdint>
#include <map>
#include <string>
#include <unordered_map>
#include <vector>

namespace yaze {
namespace zelda3 {

// Which names to show for tile types (TILEATTR values, $7EFE00). Oracle of
// Secrets gives some vanilla and unused values its own meaning.
enum class TileTypeNameSet : uint8_t {
  kVanilla,
  kOracleOfSecrets,
};

/**
 * @struct Zelda3Labels
 * @brief Centralized default labels for all Zelda 3 resources
 *
 * This structure contains all the default names/labels for various game
 * resources. These labels are embedded directly into the project file format
 * and are always available to the AI agents (Ollama/Gemini) without requiring
 * external files.
 */
struct Zelda3Labels {
  // Dungeon/Room names (296 rooms total)
  static const std::vector<std::string>& GetRoomNames();

  // Entrance names (133 entrances)
  static const std::vector<std::string>& GetEntranceNames();

  // Sprite names (256 sprites)
  static const std::vector<std::string>& GetSpriteNames();

  // Overlord names (14 overlords)
  static const std::vector<std::string>& GetOverlordNames();

  // Overworld map names (160 maps: 64 light world + 64 dark world + 32 special)
  static const std::vector<std::string>& GetOverworldMapNames();

  // Item names (all collectible items)
  static const std::vector<std::string>& GetItemNames();

  // Vanilla item-receipt bytes (chest rewards). ID 00 is Fighter Sword.
  // Separate from inventory labels; do not index GetItemNames with receipts.
  static const std::vector<std::string>& GetItemReceiptNames();

  // Music track names
  static const std::vector<std::string>& GetMusicTrackNames();

  // Graphics sheet names
  static const std::vector<std::string>& GetGraphicsSheetNames();

  // Room object type names
  static const std::vector<std::string>& GetType1RoomObjectNames();
  static const std::vector<std::string>& GetType2RoomObjectNames();
  static const std::vector<std::string>& GetType3RoomObjectNames();

  // Room effect names
  static const std::vector<std::string>& GetRoomEffectNames();

  // Room tag names
  static const std::vector<std::string>& GetRoomTagNames();

  // Tile type names
  static const std::vector<std::string>& GetTileTypeNames();
  // The 256 tile type names for `set`: Oracle names where Oracle defines the
  // value, vanilla names elsewhere.
  static const std::vector<std::string>& GetTileTypeNames(TileTypeNameSet set);
  // Oracle of Secrets name for `value`, or nullptr when Oracle uses the
  // vanilla meaning.
  static const char* GetOracleTileTypeName(uint8_t value);

  /**
   * @brief Convert all labels to a structured map for project embedding
   * @return Map of resource type -> (id -> name) for all resources
   */
  static std::unordered_map<std::string,
                            std::unordered_map<std::string, std::string>>
  ToResourceLabels();

  /**
   * @brief Get a label by resource type and ID
   * @param resource_type The type of resource (e.g., "room", "entrance",
   * "sprite")
   * @param id The numeric ID of the resource
   * @param default_value Fallback value if label not found
   * @return The label string
   */
  static std::string GetLabel(const std::string& resource_type, int id,
                              const std::string& default_value = "");
};

}  // namespace zelda3
}  // namespace yaze

#endif  // YAZE_APP_ZELDA3_ZELDA3_LABELS_H
