#ifndef YAZE_APP_ZELDA3_DUNGEON_DUNGEON_EDITOR_SYSTEM_H
#define YAZE_APP_ZELDA3_DUNGEON_DUNGEON_EDITOR_SYSTEM_H

#include <functional>
#include <memory>
#include <optional>
#include <unordered_map>
#include <vector>

#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "app/gfx/core/bitmap.h"
#include "app/gfx/types/snes_palette.h"
#include "app/platform/window.h"
#include "app/rom.h"
#include "dungeon_object_editor.h"
#include "zelda3/dungeon/room.h"
#include "zelda3/dungeon/room_object.h"
#include "zelda3/sprite/sprite.h"

namespace yaze {
namespace zelda3 {

/**
 * @brief Comprehensive dungeon editing system
 *
 * This class provides a complete dungeon editing solution including:
 * - Object editing (walls, floors, decorations)
 * - Sprite management (enemies, NPCs, interactive elements)
 * - Item placement and management
 * - Entrance/exit data editing
 * - Door configuration
 * - Chest and treasure management
 * - Room properties and metadata
 * - Dungeon-wide settings
 */
class DungeonEditorSystem {
 public:
  // Editor modes
  enum class EditorMode {
    kObjects,     // Object editing mode
    kSprites,     // Sprite editing mode
    kItems,       // Item placement mode
    kEntrances,   // Entrance/exit editing mode
    kDoors,       // Door configuration mode
    kChests,      // Chest management mode
    kProperties,  // Room properties mode
    kGlobal       // Dungeon-wide settings mode
  };

  // Sprite types and categories
  enum class SpriteType {
    kEnemy,        // Hostile entities
    kNPC,          // Non-player characters
    kInteractive,  // Interactive objects
    kDecoration,   // Decorative sprites
    kBoss,         // Boss entities
    kSpecial       // Special purpose sprites
  };

  // Item types
  enum class ItemType {
    kWeapon,   // Swords, bows, etc.
    kTool,     // Hookshot, bombs, etc.
    kKey,      // Keys and key items
    kHeart,    // Heart containers and pieces
    kRupee,    // Currency
    kBottle,   // Bottles and contents
    kUpgrade,  // Capacity upgrades
    kSpecial   // Special items
  };

  // Entrance/exit types
  enum class EntranceType {
    kNormal,  // Standard room entrance
    kStairs,  // Staircase connection
    kDoor,    // Door connection
    kCave,    // Cave entrance
    kWarp,    // Warp/teleport
    kBoss,    // Boss room entrance
    kSpecial  // Special entrance type
  };

  // Editor state
  struct EditorState {
    EditorMode current_mode = EditorMode::kObjects;
    int current_room_id = 0;
    bool is_dirty = false;  // Has unsaved changes
    bool auto_save_enabled = true;
    std::chrono::steady_clock::time_point last_save_time;
  };

  // Sprite editing data
  struct SpriteData {
    int sprite_id;
    std::string name;
    DungeonEditorSystem::SpriteType type;
    int x, y;
    int layer;
    std::unordered_map<std::string, std::string> properties;
    bool is_active = true;
  };

  // Item placement data
  struct ItemData {
    int item_id;
    DungeonEditorSystem::ItemType type;
    std::string name;
    int x, y;
    int room_id;
    bool is_hidden = false;
    std::unordered_map<std::string, std::string> properties;
  };

  // Entrance/exit data
  struct EntranceData {
    int entrance_id;
    DungeonEditorSystem::EntranceType type;
    std::string name;
    int source_room_id;
    int target_room_id;
    int source_x, source_y;
    int target_x, target_y;
    bool is_bidirectional = true;
    std::unordered_map<std::string, std::string> properties;
  };

  // Door configuration data
  struct DoorData {
    int door_id;
    std::string name;
    int room_id;
    int x, y;
    int direction;  // 0=up, 1=right, 2=down, 3=left
    int target_room_id;
    int target_x, target_y;
    bool requires_key = false;
    int key_type = 0;
    bool is_locked = false;
    std::unordered_map<std::string, std::string> properties;
  };

  // Chest data
  struct ChestData {
    int chest_id;
    int room_id;
    int x, y;
    bool is_big_chest = false;
    int item_id;
    int item_quantity = 1;
    bool is_opened = false;
    std::unordered_map<std::string, std::string> properties;
  };

  explicit DungeonEditorSystem(Rom* rom);
  ~DungeonEditorSystem() = default;

  // System initialization and management
  absl::Status Initialize();
  absl::Status LoadDungeon(int dungeon_id);
  absl::Status SaveDungeon();
  absl::Status SaveRoom(int room_id);
  absl::Status ReloadRoom(int room_id);

  // Mode management
  void SetEditorMode(EditorMode mode);
  EditorMode GetEditorMode() const;

  // Room management
  absl::Status SetCurrentRoom(int room_id);
  int GetCurrentRoom() const;
  absl::StatusOr<Room> GetRoom(int room_id);
  absl::Status CreateRoom(int room_id, const std::string& name = "");
  absl::Status DeleteRoom(int room_id);
  absl::Status DuplicateRoom(int source_room_id, int target_room_id);

  // Object editing (delegated to DungeonObjectEditor)
  std::shared_ptr<DungeonObjectEditor> GetObjectEditor();
  absl::Status SetObjectEditorMode();

  // Sprite management
  absl::Status AddSprite(const SpriteData& sprite_data);
  absl::Status RemoveSprite(int sprite_id);
  absl::Status UpdateSprite(int sprite_id, const SpriteData& sprite_data);
  absl::StatusOr<SpriteData> GetSprite(int sprite_id);
  absl::StatusOr<std::vector<SpriteData>> GetSpritesByRoom(int room_id);
  absl::StatusOr<std::vector<SpriteData>> GetSpritesByType(
      DungeonEditorSystem::SpriteType type);
  absl::Status MoveSprite(int sprite_id, int new_x, int new_y);
  absl::Status SetSpriteActive(int sprite_id, bool active);

  // Item management
  absl::Status AddItem(const ItemData& item_data);
  absl::Status RemoveItem(int item_id);
  absl::Status UpdateItem(int item_id, const ItemData& item_data);
  absl::StatusOr<ItemData> GetItem(int item_id);
  absl::StatusOr<std::vector<ItemData>> GetItemsByRoom(int room_id);
  absl::StatusOr<std::vector<ItemData>> GetItemsByType(
      DungeonEditorSystem::ItemType type);
  absl::Status MoveItem(int item_id, int new_x, int new_y);
  absl::Status SetItemHidden(int item_id, bool hidden);

  // Entrance/exit management
  absl::Status AddEntrance(const EntranceData& entrance_data);
  absl::Status RemoveEntrance(int entrance_id);
  absl::Status UpdateEntrance(int entrance_id,
                              const EntranceData& entrance_data);
  absl::StatusOr<EntranceData> GetEntrance(int entrance_id);
  absl::StatusOr<std::vector<EntranceData>> GetEntrancesByRoom(int room_id);
  absl::StatusOr<std::vector<EntranceData>> GetEntrancesByType(
      DungeonEditorSystem::EntranceType type);
  absl::Status ConnectRooms(int room1_id, int room2_id, int x1, int y1, int x2,
                            int y2);
  absl::Status DisconnectRooms(int room1_id, int room2_id);

  // Door management
  absl::Status AddDoor(const DoorData& door_data);
  absl::Status RemoveDoor(int door_id);
  absl::Status UpdateDoor(int door_id, const DoorData& door_data);
  absl::StatusOr<DoorData> GetDoor(int door_id);
  absl::StatusOr<std::vector<DoorData>> GetDoorsByRoom(int room_id);
  absl::Status SetDoorLocked(int door_id, bool locked);
  absl::Status SetDoorKeyRequirement(int door_id, bool requires_key,
                                     int key_type);

  // Chest management
  absl::Status AddChest(const ChestData& chest_data);
  absl::Status RemoveChest(int chest_id);
  absl::Status UpdateChest(int chest_id, const ChestData& chest_data);
  absl::StatusOr<ChestData> GetChest(int chest_id);
  absl::StatusOr<std::vector<ChestData>> GetChestsByRoom(int room_id);
  absl::Status SetChestItem(int chest_id, int item_id, int quantity);
  absl::Status SetChestOpened(int chest_id, bool opened);

  // Room properties and metadata
  struct RoomProperties {
    int room_id;
    std::string name;
    std::string description;
    int dungeon_id;
    int floor_level;
    bool is_boss_room = false;
    bool is_save_room = false;
    bool is_shop_room = false;
    int music_id = 0;
    int ambient_sound_id = 0;
    std::unordered_map<std::string, std::string> custom_properties;
  };

  absl::Status SetRoomProperties(int room_id, const RoomProperties& properties);
  absl::StatusOr<RoomProperties> GetRoomProperties(int room_id);

  // Dungeon-wide settings
  struct DungeonSettings {
    int dungeon_id;
    std::string name;
    std::string description;
    int total_rooms;
    int starting_room_id;
    int boss_room_id;
    int music_theme_id;
    int color_palette_id;
    bool has_map = true;
    bool has_compass = true;
    bool has_big_key = true;
    std::unordered_map<std::string, std::string> custom_settings;
  };

  absl::Status SetDungeonSettings(const DungeonSettings& settings);
  absl::StatusOr<DungeonSettings> GetDungeonSettings();

  // Validation and error checking
  absl::Status ValidateRoom(int room_id);
  absl::Status ValidateDungeon();
  std::vector<std::string> GetValidationErrors(int room_id);
  std::vector<std::string> GetDungeonValidationErrors();

  // Rendering and preview
  absl::StatusOr<gfx::Bitmap> RenderRoom(int room_id);
  absl::StatusOr<gfx::Bitmap> RenderRoomPreview(int room_id, EditorMode mode);
  absl::StatusOr<gfx::Bitmap> RenderDungeonMap();

  // Import/Export functionality
  absl::Status ImportRoomFromFile(const std::string& file_path, int room_id);
  absl::Status ExportRoomToFile(int room_id, const std::string& file_path);
  absl::Status ImportDungeonFromFile(const std::string& file_path);
  absl::Status ExportDungeonToFile(const std::string& file_path);

  // Undo/Redo system
  absl::Status Undo();
  absl::Status Redo();
  bool CanUndo() const;
  bool CanRedo() const;
  void ClearHistory();

  // Event callbacks
  using RoomChangedCallback = std::function<void(int room_id)>;
  using SpriteChangedCallback = std::function<void(int sprite_id)>;
  using ItemChangedCallback = std::function<void(int item_id)>;
  using EntranceChangedCallback = std::function<void(int entrance_id)>;
  using DoorChangedCallback = std::function<void(int door_id)>;
  using ChestChangedCallback = std::function<void(int chest_id)>;
  using ModeChangedCallback = std::function<void(EditorMode mode)>;
  using ValidationCallback =
      std::function<void(const std::vector<std::string>& errors)>;

  void SetRoomChangedCallback(RoomChangedCallback callback);
  void SetSpriteChangedCallback(SpriteChangedCallback callback);
  void SetItemChangedCallback(ItemChangedCallback callback);
  void SetEntranceChangedCallback(EntranceChangedCallback callback);
  void SetDoorChangedCallback(DoorChangedCallback callback);
  void SetChestChangedCallback(ChestChangedCallback callback);
  void SetModeChangedCallback(ModeChangedCallback callback);
  void SetValidationCallback(ValidationCallback callback);

  // Getters
  EditorState GetEditorState() const;
  Rom* GetROM() const;
  bool IsDirty() const;
  bool HasUnsavedChanges() const;

  // ROM management
  void SetROM(Rom* rom);

 private:
  // Internal helper methods
  absl::Status InitializeObjectEditor();
  absl::Status InitializeSpriteSystem();
  absl::Status InitializeItemSystem();
  absl::Status InitializeEntranceSystem();
  absl::Status InitializeDoorSystem();
  absl::Status InitializeChestSystem();

  // Data management
  absl::Status LoadRoomData(int room_id);
  absl::Status SaveRoomData(int room_id);
  absl::Status LoadSpriteData();
  absl::Status SaveSpriteData();
  absl::Status LoadItemData();
  absl::Status SaveItemData();
  absl::Status LoadEntranceData();
  absl::Status SaveEntranceData();
  absl::Status LoadDoorData();
  absl::Status SaveDoorData();
  absl::Status LoadChestData();
  absl::Status SaveChestData();

  // Validation helpers
  absl::Status ValidateSprite(const SpriteData& sprite);
  absl::Status ValidateItem(const ItemData& item);
  absl::Status ValidateEntrance(const EntranceData& entrance);
  absl::Status ValidateDoor(const DoorData& door);
  absl::Status ValidateChest(const ChestData& chest);

  // ID generation
  int GenerateSpriteId();
  int GenerateItemId();
  int GenerateEntranceId();
  int GenerateDoorId();
  int GenerateChestId();

  // Member variables
  Rom* rom_;
  std::shared_ptr<DungeonObjectEditor> object_editor_;

  EditorState editor_state_;
  DungeonSettings dungeon_settings_;

  // Data storage
  std::unordered_map<int, Room> rooms_;
  std::unordered_map<int, SpriteData> sprites_;
  std::unordered_map<int, ItemData> items_;
  std::unordered_map<int, EntranceData> entrances_;
  std::unordered_map<int, DoorData> doors_;
  std::unordered_map<int, ChestData> chests_;
  std::unordered_map<int, RoomProperties> room_properties_;

  // ID counters
  int next_sprite_id_ = 1;
  int next_item_id_ = 1;
  int next_entrance_id_ = 1;
  int next_door_id_ = 1;
  int next_chest_id_ = 1;

  // Event callbacks
  RoomChangedCallback room_changed_callback_;
  SpriteChangedCallback sprite_changed_callback_;
  ItemChangedCallback item_changed_callback_;
  EntranceChangedCallback entrance_changed_callback_;
  DoorChangedCallback door_changed_callback_;
  ChestChangedCallback chest_changed_callback_;
  ModeChangedCallback mode_changed_callback_;
  ValidationCallback validation_callback_;

  // Undo/Redo system
  struct UndoPoint {
    EditorState state;
    std::unordered_map<int, Room> rooms;
    std::unordered_map<int, SpriteData> sprites;
    std::unordered_map<int, ItemData> items;
    std::unordered_map<int, EntranceData> entrances;
    std::unordered_map<int, DoorData> doors;
    std::unordered_map<int, ChestData> chests;
    std::chrono::steady_clock::time_point timestamp;
  };

  std::vector<UndoPoint> undo_history_;
  std::vector<UndoPoint> redo_history_;
  static constexpr size_t kMaxUndoHistory = 100;
};

/**
 * @brief Factory function to create dungeon editor system
 */
std::unique_ptr<DungeonEditorSystem> CreateDungeonEditorSystem(Rom* rom);

/**
 * @brief Sprite type utilities
 */
namespace SpriteTypes {

/**
 * @brief Get sprite information by ID
 */
struct SpriteInfo {
  int id;
  std::string name;
  DungeonEditorSystem::SpriteType type;
  std::string description;
  int default_layer;
  std::vector<std::pair<std::string, std::string>> default_properties;
  bool is_interactive;
  bool is_hostile;
  int difficulty_rating;
};

absl::StatusOr<SpriteInfo> GetSpriteInfo(int sprite_id);
std::vector<SpriteInfo> GetAllSpriteInfos();
std::vector<SpriteInfo> GetSpritesByType(DungeonEditorSystem::SpriteType type);
absl::StatusOr<std::string> GetSpriteCategory(int sprite_id);

}  // namespace SpriteTypes

/**
 * @brief Item type utilities
 */
namespace ItemTypes {

/**
 * @brief Get item information by ID
 */
struct ItemInfo {
  int id;
  std::string name;
  DungeonEditorSystem::ItemType type;
  std::string description;
  int rarity;
  int value;
  std::vector<std::pair<std::string, std::string>> default_properties;
  bool is_stackable;
  int max_stack_size;
};

absl::StatusOr<ItemInfo> GetItemInfo(int item_id);
std::vector<ItemInfo> GetAllItemInfos();
std::vector<ItemInfo> GetItemsByType(DungeonEditorSystem::ItemType type);
absl::StatusOr<std::string> GetItemCategory(int item_id);

}  // namespace ItemTypes

/**
 * @brief Entrance type utilities
 */
namespace EntranceTypes {

/**
 * @brief Get entrance information by ID
 */
struct EntranceInfo {
  int id;
  std::string name;
  DungeonEditorSystem::EntranceType type;
  std::string description;
  std::vector<std::pair<std::string, std::string>> default_properties;
  bool requires_key;
  int key_type;
  bool is_bidirectional;
};

absl::StatusOr<EntranceInfo> GetEntranceInfo(int entrance_id);
std::vector<EntranceInfo> GetAllEntranceInfos();
std::vector<EntranceInfo> GetEntrancesByType(
    DungeonEditorSystem::EntranceType type);

}  // namespace EntranceTypes

}  // namespace zelda3
}  // namespace yaze

#endif  // YAZE_APP_ZELDA3_DUNGEON_DUNGEON_EDITOR_SYSTEM_H
