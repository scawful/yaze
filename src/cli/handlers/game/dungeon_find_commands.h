#ifndef YAZE_SRC_CLI_HANDLERS_GAME_DUNGEON_FIND_COMMANDS_H_
#define YAZE_SRC_CLI_HANDLERS_GAME_DUNGEON_FIND_COMMANDS_H_

#include <cstdint>
#include <optional>
#include <string>
#include <vector>

#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "cli/service/resources/command_handler.h"

namespace yaze::zelda3 {
class Room;
}  // namespace yaze::zelda3

namespace yaze::cli::handlers {

// One element kind in a dungeon-find row.
enum class DungeonFindKind { kChest, kPot, kSprite, kHeader };

const char* DungeonFindKindName(DungeonFindKind kind);

// A --sprite or --item value: a hex ID, or a case-insensitive name substring.
// A value is an ID when it has a 0x or $ prefix, or when it is only hex digits
// and holds at least one decimal digit ("E4", "12"). Anything else ("Lamp",
// "Bee", "eyegore") is a name.
struct DungeonFindMatch {
  std::optional<int> id;
  std::string name_lower;
};

absl::StatusOr<DungeonFindMatch> ParseDungeonFindMatch(const std::string& value,
                                                       int max_id);

struct DungeonFindFilter {
  // Element filters. Rows come from the elements that match all of them.
  std::optional<DungeonFindMatch> sprite;
  std::optional<DungeonFindMatch> item;  // chest receipt IDs and pot item IDs
  bool key_drop = false;                 // sprites carrying a key drop
  // Room filters. A room must match all of them.
  std::optional<int> tag;  // tag1 or tag2
  bool dark = false;       // header byte0 bit0 set
  std::optional<int> spriteset;
  // Restricts the kinds a query returns. Empty: chest, pot and sprite rows
  // when an element filter is set, else one header row per room.
  std::vector<DungeonFindKind> kinds;
};

absl::Status ValidateDungeonFindFilter(const DungeonFindFilter& filter);

struct DungeonFindSprite {
  int id = 0;
  int x = 0;
  int y = 0;
  int subtype = 0;
  int layer = 0;
  int key_drop = 0;          // 0 none, 1 small key, 2 big key
  std::string name;          // project label when the project renames it
  std::string vanilla_name;  // vanilla label; --sprite names match either
};

struct DungeonFindChest {
  int id = 0;
  bool is_big = false;
  std::string name;
};

struct DungeonFindPot {
  int id = 0;
  uint16_t position = 0;
  int tile_x = 0;
  int tile_y = 0;
  std::string name;
};

// Everything dungeon-find needs from one room, read with the same Room
// loaders as dungeon-room-header, dungeon-list-sprites, dungeon-list-chests
// and dungeon-list-pot-items.
struct DungeonFindRoom {
  int room_id = 0;
  bool is_dark = false;
  int spriteset = 0;
  int tag1 = 0;
  int tag2 = 0;
  std::vector<DungeonFindSprite> sprites;
  std::vector<DungeonFindChest> chests;
  std::vector<DungeonFindPot> pots;
};

struct DungeonFindRow {
  int room_id = 0;
  DungeonFindKind kind = DungeonFindKind::kHeader;
  int index = 0;  // element index in its room list; 0 for header rows
  std::optional<int> x;
  std::optional<int> y;
  int raw = 0;  // sprite ID, chest receipt ID, pot item ID, or spriteset
  std::string name;
  // Kind-specific fields.
  bool is_big_chest = false;
  int key_drop = 0;
  int subtype = 0;
  int layer = 0;
  uint16_t pot_position = 0;
};

// Pure filter: returns the rows one room contributes. Unit-tested without a
// ROM.
std::vector<DungeonFindRow> FindDungeonRows(const DungeonFindRoom& room,
                                            const DungeonFindFilter& filter);

// Converts a loaded room (header, sprites, chests and pot items) into the
// dungeon-find view. Shared with dungeon-key-balance and progression-audit.
DungeonFindRoom MakeDungeonFindRoom(const zelda3::Room& room, int room_id);

// Pot-item name for a secret-item ID (0x00-0x16 and the 0x80+ even IDs).
std::string DungeonPotItemName(int id);

/**
 * @brief Search every dungeon room for sprites, chest/pot items, tags,
 * dark rooms or spritesets.
 */
class DungeonFindCommandHandler : public resources::CommandHandler {
 public:
  std::string GetName() const override { return "dungeon-find"; }
  std::string GetDescription() const {
    return "Search all dungeon rooms for sprites, key drops, chest and pot "
           "items, tags, dark rooms or spritesets";
  }
  std::string GetUsage() const override {
    return "dungeon-find [--sprite <hex id|name>] [--item <hex id|name>] "
           "[--key-drop] [--tag <hex id>] [--dark] [--spriteset <hex id>] "
           "[--kind <chest|pot|sprite|header>[,...]] [--room <hex id>] "
           "[--sprite-registry <path>] [--format <json|text>]";
  }

  absl::Status ValidateArgs(const resources::ArgumentParser& parser) override;

  absl::Status Execute(Rom* rom, const resources::ArgumentParser& parser,
                       resources::OutputFormatter& formatter) override;
};

/**
 * @brief List every regular dungeon entrance (0x00-0x84) with its room and
 * dungeon ID, read through RoomEntrance and the resolved entrance layout.
 */
class DungeonListEntrancesCommandHandler : public resources::CommandHandler {
 public:
  std::string GetName() const override { return "dungeon-list-entrances"; }
  std::string GetDescription() const {
    return "List regular dungeon entrances with room, dungeon and exit IDs";
  }
  std::string GetUsage() const override {
    return "dungeon-list-entrances [--room <hex id>] [--dungeon <hex id>] "
           "[--format <json|text>]";
  }

  absl::Status ValidateArgs(const resources::ArgumentParser& parser) override;

  absl::Status Execute(Rom* rom, const resources::ArgumentParser& parser,
                       resources::OutputFormatter& formatter) override;
};

}  // namespace yaze::cli::handlers

#endif  // YAZE_SRC_CLI_HANDLERS_GAME_DUNGEON_FIND_COMMANDS_H_
