#ifndef YAZE_SRC_CLI_HANDLERS_GAME_PROGRESSION_COMMANDS_H_
#define YAZE_SRC_CLI_HANDLERS_GAME_PROGRESSION_COMMANDS_H_

#include <map>
#include <set>
#include <string>
#include <vector>

#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "cli/handlers/game/dungeon_find_commands.h"
#include "cli/service/resources/command_handler.h"

namespace yaze::cli::handlers {

// One door from Room::GetDoors(), with its index in that list.
struct ProgressionDoor {
  int index = 0;
  int type = 0;       // zelda3::DoorType value
  int direction = 0;  // 0 north, 1 south, 2 west, 3 east
  int along = 0;      // tile coordinate along the wall
};

// Everything the key balance and the progression audit read from one room.
// Plain data so tests can build rooms without a ROM.
struct ProgressionRoom {
  int room_id = 0;
  // Room census owner: "D1" from a project registry, "ID02"-style derived
  // owners, or the interior owner. Empty: the census assigns no owner.
  std::string owner_id;
  std::string owner_name;
  bool interior = false;
  DungeonFindRoom content;  // header, sprites, chests, pot items
  std::vector<ProgressionDoor> doors;
  std::vector<int> object_ids;        // unique, sorted
  std::vector<int> collision_values;  // custom collision overrides, unique
};

// ---------------------------------------------------------------------------
// dungeon-key-balance

// Item IDs that are keys, resolved from yaze's label tables by name.
struct KeyItemIds {
  std::set<int> small_key_receipts;  // chest receipt IDs ("Small Key")
  std::set<int> big_key_receipts;    // chest receipt IDs ("Big Key")
  std::set<int> small_key_pots;      // pot secret-item IDs ("Key")
  std::set<int> big_key_pots;        // pot secret-item IDs ("Big Key")
};

KeyItemIds ResolveKeyItemIds();

struct KeySource {
  int room_id = 0;
  std::string kind;  // chest, pot, sprite
  int index = 0;     // element index in the room list
  int raw = 0;       // receipt ID, pot item ID or sprite ID
  bool big = false;
  std::string name;
};

struct KeySink {
  int room_id = 0;
  // small_key_door, small_key_stairs, big_key_door, big_chest
  std::string kind;
  int index = 0;  // door index or chest index
  int raw = 0;    // door type or chest receipt ID
  bool big = false;
  // The other side of a two-sided key door: one lock, one key.
  int paired_room = -1;
  int paired_index = -1;
};

struct DungeonKeyBalance {
  std::string owner_id;
  std::string owner_name;
  bool interior = false;
  std::vector<int> rooms;
  std::vector<KeySource> sources;
  std::vector<KeySink> sinks;
  int small_keys = 0;
  int small_key_doors = 0;       // locks: a two-sided door counts once
  int small_key_door_sides = 0;  // raw door entries
  int balance = 0;               // small_keys - small_key_doors
  int big_key_sources = 0;
  int big_key_sinks = 0;
  // UnopenableBigKeyDoor (0x2C) entries: listed, not counted as sinks.
  std::vector<int> unopenable_big_key_door_rooms;
  // ok, short, no_big_key or surplus (in that priority: short first).
  std::string status;
  std::vector<std::string> notes;
};

// Groups rooms by owner_id (rooms without an owner are skipped) and counts
// key sources and sinks.
std::vector<DungeonKeyBalance> ComputeDungeonKeyBalances(
    const std::vector<ProgressionRoom>& rooms, const KeyItemIds& ids);

// ---------------------------------------------------------------------------
// progression-audit

// Feature keys, the shared vocabulary of rooms, gates and rules:
//   dark, big_chest,
//   door:0xNN, door_class:<small_key|big_key|bombable|shutter>,
//   tag:0xNN, sprite:0xNN, object:0xNNN, collision:0xNN
std::vector<std::string> ProgressionRoomFeatures(const ProgressionRoom& room);

// Normalizes "sprite:8f", "Sprite:$8F" or kind "sprite" + id "0x8F" to
// "sprite:0x8F". `id` is empty for whole-key forms.
absl::StatusOr<std::string> NormalizeProgressionFeature(
    const std::string& feature, const std::string& id = "");

struct ProgressionGate {
  std::string feature;  // normalized key
  std::vector<std::string> required_items;
};

struct ProgressionRegion {
  std::string name;
  std::set<std::string> owners;  // census owner IDs ("D1")
  std::set<int> rooms;
  std::set<std::string> includes;  // other region names
};

enum class ProgressionRuleKind {
  kForbidFeature,
  kForbidRequires,
  kRequireSource
};

struct ProgressionRule {
  std::string name;
  ProgressionRuleKind kind = ProgressionRuleKind::kForbidFeature;
  std::string value;  // feature key or item name
  std::vector<std::string> regions;
};

struct ProgressionSpec {
  std::vector<ProgressionGate> gates;
  std::map<std::string, ProgressionRegion> regions;
  std::vector<ProgressionRule> rules;
  bool chest_sources = true;
  bool pot_sources = true;
  // Item names for receipt / pot IDs whose label is not the spec's name.
  std::map<int, std::string> chest_item_names;
  std::map<int, std::string> pot_item_names;
};

absl::StatusOr<ProgressionSpec> ParseProgressionSpec(const std::string& json);

struct ProgressionRoomResult {
  int room_id = 0;
  std::string owner_id;
  std::vector<std::string> regions;
  std::vector<std::string> features;        // all features
  std::vector<std::string> gated_features;  // features with a gate
  std::vector<std::string> required_items;  // unique
  std::vector<std::string> gives;           // "Name (chest 0)" sources
  std::vector<std::string> give_items;      // item names, unique
};

struct ProgressionItemResult {
  std::vector<int> required_by;
  std::vector<int> given_by;
};

struct ProgressionRuleResult {
  std::string name;
  std::string kind;
  std::string value;
  std::vector<std::string> regions;
  bool pass = true;
  std::vector<int> offending_rooms;
  std::string detail;
};

struct ProgressionAudit {
  std::vector<ProgressionRoomResult> rooms;  // every owned room
  std::map<std::string, ProgressionItemResult> items;
  std::vector<ProgressionRuleResult> rules;
  int rules_failed = 0;
};

ProgressionAudit RunProgressionAudit(const std::vector<ProgressionRoom>& rooms,
                                     const ProgressionSpec& spec);

/**
 * @brief Per-dungeon small/big key sources and sinks with a status that a
 * build can gate on (exit non-zero on short or no_big_key).
 */
class DungeonKeyBalanceCommandHandler : public resources::CommandHandler {
 public:
  std::string GetName() const override { return "dungeon-key-balance"; }
  std::string GetDescription() const {
    return "Count small/big key sources and sinks per dungeon; fails when a "
           "dungeon is short of small keys or has big-key sinks without a "
           "big key";
  }
  std::string GetUsage() const override {
    return "dungeon-key-balance [--dungeon <owner id|hex dungeon id>] "
           "[--project <folder|.yaze>] [--format <json|table>]";
  }

  absl::Status ValidateArgs(const resources::ArgumentParser& parser) override;

  absl::Status Execute(Rom* rom, const resources::ArgumentParser& parser,
                       resources::OutputFormatter& formatter) override;
};

/**
 * @brief Checks a project-owned progression spec (gates, regions, rules)
 * against features read from every dungeon room.
 */
class ProgressionAuditCommandHandler : public resources::CommandHandler {
 public:
  std::string GetName() const override { return "progression-audit"; }
  std::string GetDescription() const {
    return "Map room features (dark, doors, tags, sprites, objects, custom "
           "collision) to required items through a spec, and check its rules";
  }
  std::string GetUsage() const override {
    return "progression-audit --spec <json> [--project <folder|.yaze>] "
           "[--all-rooms] [--room <hex id>] [--format <json|text>]";
  }

  absl::Status ValidateArgs(const resources::ArgumentParser& parser) override;

  absl::Status Execute(Rom* rom, const resources::ArgumentParser& parser,
                       resources::OutputFormatter& formatter) override;
};

}  // namespace yaze::cli::handlers

#endif  // YAZE_SRC_CLI_HANDLERS_GAME_PROGRESSION_COMMANDS_H_
