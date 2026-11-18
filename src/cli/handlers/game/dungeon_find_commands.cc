#include "cli/handlers/game/dungeon_find_commands.h"

#include <algorithm>
#include <cctype>
#include <map>

#include "absl/strings/ascii.h"
#include "absl/strings/match.h"
#include "absl/strings/str_cat.h"
#include "absl/strings/str_format.h"
#include "absl/strings/str_split.h"
#include "cli/handlers/game/dungeon_commands.h"
#include "cli/util/hex_util.h"
#include "rom/rom.h"
#include "util/macro.h"
#include "zelda3/dungeon/dungeon_rom_addresses.h"
#include "zelda3/dungeon/entrance_layout.h"
#include "zelda3/dungeon/room.h"
#include "zelda3/dungeon/room_entrance.h"
#include "zelda3/overworld/overworld_item.h"
#include "zelda3/resource_labels.h"

namespace yaze::cli::handlers {

namespace {

bool HasAnyElementFilter(const DungeonFindFilter& filter) {
  return filter.sprite.has_value() || filter.item.has_value() ||
         filter.key_drop;
}

bool HasAnyRoomFilter(const DungeonFindFilter& filter) {
  return filter.tag.has_value() || filter.dark || filter.spriteset.has_value();
}

bool KindRequested(const DungeonFindFilter& filter, DungeonFindKind kind) {
  if (filter.kinds.empty()) {
    if (kind == DungeonFindKind::kHeader) {
      return !HasAnyElementFilter(filter);
    }
    return HasAnyElementFilter(filter);
  }
  return std::find(filter.kinds.begin(), filter.kinds.end(), kind) !=
         filter.kinds.end();
}

bool NameMatches(const DungeonFindMatch& match, const std::string& name) {
  return !match.name_lower.empty() &&
         absl::StrContains(absl::AsciiStrToLower(name), match.name_lower);
}

bool Matches(const DungeonFindMatch& match, int id, const std::string& name,
             const std::string& alt_name = "") {
  if (match.id.has_value()) {
    return *match.id == id;
  }
  return NameMatches(match, name) || NameMatches(match, alt_name);
}

absl::StatusOr<int> ParseHexArg(const resources::ArgumentParser& parser,
                                const std::string& name, int max_value) {
  const std::string value = parser.GetString(name).value();
  int parsed = 0;
  if (!util::ParseHexString(value, &parsed) || parsed < 0 ||
      parsed > max_value) {
    return absl::InvalidArgumentError(
        absl::StrFormat("--%s must be a hex value 0x00-0x%X, got '%s'", name,
                        max_value, value));
  }
  return parsed;
}

absl::StatusOr<DungeonFindKind> ParseKind(absl::string_view text) {
  const std::string lower = absl::AsciiStrToLower(text);
  if (lower == "chest")
    return DungeonFindKind::kChest;
  if (lower == "pot")
    return DungeonFindKind::kPot;
  if (lower == "sprite")
    return DungeonFindKind::kSprite;
  if (lower == "header")
    return DungeonFindKind::kHeader;
  return absl::InvalidArgumentError(absl::StrFormat(
      "--kind '%s' is not one of chest, pot, sprite, header", text));
}

absl::StatusOr<DungeonFindFilter> ParseFilter(
    const resources::ArgumentParser& parser) {
  DungeonFindFilter filter;
  if (auto value = parser.GetString("sprite")) {
    auto match = ParseDungeonFindMatch(*value, 0xFF);
    if (!match.ok())
      return match.status();
    filter.sprite = *match;
  }
  if (auto value = parser.GetString("item")) {
    auto match = ParseDungeonFindMatch(*value, 0xFF);
    if (!match.ok())
      return match.status();
    filter.item = *match;
  }
  filter.key_drop = parser.HasFlag("key-drop");
  filter.dark = parser.HasFlag("dark");
  if (parser.GetString("tag").has_value()) {
    auto tag = ParseHexArg(parser, "tag", 0xFF);
    if (!tag.ok())
      return tag.status();
    filter.tag = *tag;
  }
  if (parser.GetString("spriteset").has_value()) {
    auto spriteset = ParseHexArg(parser, "spriteset", 0xFF);
    if (!spriteset.ok())
      return spriteset.status();
    filter.spriteset = *spriteset;
  }
  if (auto value = parser.GetString("kind")) {
    for (absl::string_view part :
         absl::StrSplit(*value, ',', absl::SkipWhitespace())) {
      auto kind = ParseKind(absl::StripAsciiWhitespace(part));
      if (!kind.ok())
        return kind.status();
      if (std::find(filter.kinds.begin(), filter.kinds.end(), *kind) ==
          filter.kinds.end()) {
        filter.kinds.push_back(*kind);
      }
    }
  }
  return filter;
}

DungeonFindRoom ReadRoom(Rom* rom, int room_id) {
  // The same loaders as dungeon-room-header (header fields),
  // dungeon-list-sprites (sprites incl. key_drop), dungeon-list-chests and
  // dungeon-list-pot-items.
  zelda3::Room room = zelda3::LoadRoomHeaderFromRom(rom, room_id);
  room.LoadSprites();
  room.LoadChests();
  room.LoadPotItems();
  return MakeDungeonFindRoom(room, room_id);
}

void AddMatchField(resources::OutputFormatter& formatter,
                   const std::string& name,
                   const std::optional<DungeonFindMatch>& match) {
  if (!match.has_value()) {
    return;
  }
  if (match->id.has_value()) {
    formatter.AddHexField(name, *match->id, 2);
  } else {
    formatter.AddField(name, match->name_lower);
  }
}

}  // namespace

const char* DungeonFindKindName(DungeonFindKind kind) {
  switch (kind) {
    case DungeonFindKind::kChest:
      return "chest";
    case DungeonFindKind::kPot:
      return "pot";
    case DungeonFindKind::kSprite:
      return "sprite";
    case DungeonFindKind::kHeader:
      return "header";
  }
  return "unknown";
}

absl::StatusOr<DungeonFindMatch> ParseDungeonFindMatch(const std::string& value,
                                                       int max_id) {
  const std::string trimmed(absl::StripAsciiWhitespace(value));
  if (trimmed.empty()) {
    return absl::InvalidArgumentError("Empty --sprite/--item value");
  }
  const bool prefixed =
      trimmed.front() == '$' || absl::StartsWithIgnoreCase(trimmed, "0x");
  const bool all_hex = std::all_of(trimmed.begin(), trimmed.end(), [](char c) {
    return std::isxdigit(static_cast<unsigned char>(c)) != 0;
  });
  const bool has_decimal_digit = std::any_of(
      trimmed.begin(), trimmed.end(),
      [](char c) { return std::isdigit(static_cast<unsigned char>(c)) != 0; });

  DungeonFindMatch match;
  if (prefixed || (all_hex && has_decimal_digit)) {
    int id = 0;
    if (!util::ParseHexString(trimmed, &id) || id < 0 || id > max_id) {
      return absl::InvalidArgumentError(
          absl::StrFormat("'%s' is not a hex ID 0x00-0x%X", trimmed, max_id));
    }
    match.id = id;
  } else {
    match.name_lower = absl::AsciiStrToLower(trimmed);
  }
  return match;
}

absl::Status ValidateDungeonFindFilter(const DungeonFindFilter& filter) {
  if (!HasAnyElementFilter(filter) && !HasAnyRoomFilter(filter) &&
      filter.kinds.empty()) {
    return absl::InvalidArgumentError(
        "dungeon-find needs at least one of --sprite, --item, --key-drop, "
        "--tag, --dark, --spriteset or --kind");
  }
  if (!HasAnyElementFilter(filter)) {
    return absl::OkStatus();
  }
  const bool sprite_group = filter.sprite.has_value() || filter.key_drop;
  const bool item_group = filter.item.has_value();
  for (DungeonFindKind kind : filter.kinds) {
    const bool applies =
        (kind == DungeonFindKind::kSprite && sprite_group) ||
        ((kind == DungeonFindKind::kChest || kind == DungeonFindKind::kPot) &&
         item_group);
    if (!applies) {
      return absl::InvalidArgumentError(absl::StrFormat(
          "--kind %s has no matching filter: --sprite/--key-drop select "
          "sprite rows, --item selects chest and pot rows",
          DungeonFindKindName(kind)));
    }
  }
  return absl::OkStatus();
}

std::string DungeonPotItemName(int id) {
  size_t index = zelda3::kSecretItemNames.size();
  if (id >= 0 && id <= 0x16) {
    index = static_cast<size_t>(id);
  } else if (id >= 0x80 && (id & 1) == 0) {
    index = 23 + static_cast<size_t>((id - 0x80) / 2);
  }
  if (index < zelda3::kSecretItemNames.size()) {
    return std::string(
        absl::StripTrailingAsciiWhitespace(zelda3::kSecretItemNames[index]));
  }
  return absl::StrFormat("Unknown 0x%02X", id);
}

DungeonFindRoom MakeDungeonFindRoom(const zelda3::Room& room, int room_id) {
  DungeonFindRoom data;
  data.room_id = room_id;
  data.is_dark = room.IsDark();
  data.spriteset = room.spriteset();
  data.tag1 = static_cast<int>(room.tag1());
  data.tag2 = static_cast<int>(room.tag2());
  for (const auto& sprite : room.GetSprites()) {
    const auto label = zelda3::ResolveSpriteLabel(sprite.id());
    data.sprites.push_back({.id = static_cast<int>(sprite.id()),
                            .x = static_cast<int>(sprite.x()),
                            .y = static_cast<int>(sprite.y()),
                            .subtype = static_cast<int>(sprite.subtype()),
                            .layer = static_cast<int>(sprite.layer()),
                            .key_drop = static_cast<int>(sprite.key_drop()),
                            .name = label.name,
                            .vanilla_name = label.default_name});
  }
  for (const auto& chest : room.GetChests()) {
    data.chests.push_back(
        {.id = static_cast<int>(chest.id),
         .is_big = static_cast<bool>(chest.size),
         .name = zelda3::GetItemReceiptLabel(static_cast<uint8_t>(chest.id))});
  }
  for (const auto& pot : room.GetPotItems()) {
    data.pots.push_back({.id = pot.item,
                         .position = pot.position,
                         .tile_x = pot.GetTileX(),
                         .tile_y = pot.GetTileY(),
                         .name = DungeonPotItemName(pot.item)});
  }
  return data;
}

std::vector<DungeonFindRow> FindDungeonRows(const DungeonFindRoom& room,
                                            const DungeonFindFilter& filter) {
  std::vector<DungeonFindRow> rows;
  if (filter.dark && !room.is_dark) {
    return rows;
  }
  if (filter.spriteset.has_value() && room.spriteset != *filter.spriteset) {
    return rows;
  }
  if (filter.tag.has_value() && room.tag1 != *filter.tag &&
      room.tag2 != *filter.tag) {
    return rows;
  }

  if (KindRequested(filter, DungeonFindKind::kHeader)) {
    DungeonFindRow row;
    row.room_id = room.room_id;
    row.kind = DungeonFindKind::kHeader;
    row.raw = room.spriteset;
    rows.push_back(row);
  }

  const bool sprite_group = filter.sprite.has_value() || filter.key_drop;
  const bool item_group = filter.item.has_value();

  std::vector<DungeonFindRow> sprite_rows;
  for (size_t i = 0; i < room.sprites.size(); ++i) {
    const DungeonFindSprite& sprite = room.sprites[i];
    if (filter.sprite.has_value() &&
        !Matches(*filter.sprite, sprite.id, sprite.name, sprite.vanilla_name)) {
      continue;
    }
    if (filter.key_drop && sprite.key_drop == 0) {
      continue;
    }
    DungeonFindRow row;
    row.room_id = room.room_id;
    row.kind = DungeonFindKind::kSprite;
    row.index = static_cast<int>(i);
    row.x = sprite.x;
    row.y = sprite.y;
    row.raw = sprite.id;
    row.name = sprite.name;
    row.key_drop = sprite.key_drop;
    row.subtype = sprite.subtype;
    row.layer = sprite.layer;
    sprite_rows.push_back(row);
  }

  std::vector<DungeonFindRow> chest_rows;
  for (size_t i = 0; i < room.chests.size(); ++i) {
    const DungeonFindChest& chest = room.chests[i];
    if (filter.item.has_value() &&
        !Matches(*filter.item, chest.id, chest.name)) {
      continue;
    }
    DungeonFindRow row;
    row.room_id = room.room_id;
    row.kind = DungeonFindKind::kChest;
    row.index = static_cast<int>(i);
    row.raw = chest.id;
    row.name = chest.name;
    row.is_big_chest = chest.is_big;
    chest_rows.push_back(row);
  }

  std::vector<DungeonFindRow> pot_rows;
  for (size_t i = 0; i < room.pots.size(); ++i) {
    const DungeonFindPot& pot = room.pots[i];
    if (filter.item.has_value() && !Matches(*filter.item, pot.id, pot.name)) {
      continue;
    }
    DungeonFindRow row;
    row.room_id = room.room_id;
    row.kind = DungeonFindKind::kPot;
    row.index = static_cast<int>(i);
    row.x = pot.tile_x;
    row.y = pot.tile_y;
    row.raw = pot.id;
    row.name = pot.name;
    row.pot_position = pot.position;
    pot_rows.push_back(row);
  }

  // --sprite/--key-drop and --item together: the room must have a match in
  // both groups (for example an Eyegore and a key pot in the same room).
  if (sprite_group && item_group &&
      (sprite_rows.empty() || (chest_rows.empty() && pot_rows.empty()))) {
    return rows;
  }

  const bool all_elements = !HasAnyElementFilter(filter);
  auto append = [&](DungeonFindKind kind, bool group_active,
                    const std::vector<DungeonFindRow>& source) {
    if (!KindRequested(filter, kind) || (!group_active && !all_elements)) {
      return;
    }
    rows.insert(rows.end(), source.begin(), source.end());
  };
  append(DungeonFindKind::kChest, item_group, chest_rows);
  append(DungeonFindKind::kPot, item_group, pot_rows);
  append(DungeonFindKind::kSprite, sprite_group, sprite_rows);
  return rows;
}

absl::Status DungeonFindCommandHandler::ValidateArgs(
    const resources::ArgumentParser& parser) {
  auto filter = ParseFilter(parser);
  if (!filter.ok()) {
    return filter.status();
  }
  if (parser.GetString("room").has_value()) {
    auto room = ParseHexArg(parser, "room", zelda3::kNumberOfRooms - 1);
    if (!room.ok())
      return room.status();
  }
  return ValidateDungeonFindFilter(*filter);
}

absl::Status DungeonFindCommandHandler::Execute(
    Rom* rom, const resources::ArgumentParser& parser,
    resources::OutputFormatter& formatter) {
  RETURN_IF_ERROR(LoadDungeonSpriteRegistryArg(parser));
  auto filter_or = ParseFilter(parser);
  if (!filter_or.ok()) {
    return filter_or.status();
  }
  const DungeonFindFilter filter = *filter_or;
  RETURN_IF_ERROR(ValidateDungeonFindFilter(filter));

  int start_room = 0;
  int end_room = zelda3::kNumberOfRooms - 1;
  if (parser.GetString("room").has_value()) {
    auto room = ParseHexArg(parser, "room", zelda3::kNumberOfRooms - 1);
    if (!room.ok())
      return room.status();
    start_room = end_room = *room;
  }

  formatter.BeginObject("Dungeon Find");
  formatter.BeginObject("filters");
  AddMatchField(formatter, "sprite", filter.sprite);
  AddMatchField(formatter, "item", filter.item);
  if (filter.key_drop)
    formatter.AddField("key_drop", true);
  if (filter.tag)
    formatter.AddHexField("tag", *filter.tag, 2);
  if (filter.dark)
    formatter.AddField("dark", true);
  if (filter.spriteset)
    formatter.AddHexField("spriteset", *filter.spriteset, 2);
  if (!filter.kinds.empty()) {
    formatter.BeginArray("kinds");
    for (DungeonFindKind kind : filter.kinds) {
      formatter.AddArrayItem(DungeonFindKindName(kind));
    }
    formatter.EndArray();
  }
  formatter.EndObject();
  formatter.AddField(
      "units",
      "sprite x/y: 16-pixel room grid (0-31); pot x/y: 8-pixel tiles "
      "(0-63); chests have no position. is_dark = header byte0 bit0. "
      "Chest raw = item receipt ID; pot raw = secret item ID");

  int rooms_matched = 0;
  std::map<std::string, int> by_kind;
  int row_count = 0;

  formatter.BeginArray("rows");
  for (int room_id = start_room; room_id <= end_room; ++room_id) {
    const DungeonFindRoom room = ReadRoom(rom, room_id);
    const std::vector<DungeonFindRow> rows = FindDungeonRows(room, filter);
    if (rows.empty()) {
      continue;
    }
    ++rooms_matched;
    for (const DungeonFindRow& row : rows) {
      ++row_count;
      ++by_kind[DungeonFindKindName(row.kind)];
      formatter.BeginObject();
      formatter.AddHexField("room", row.room_id, 3);
      formatter.AddField("room_name", zelda3::GetRoomLabel(row.room_id));
      formatter.AddField("kind", DungeonFindKindName(row.kind));
      switch (row.kind) {
        case DungeonFindKind::kHeader:
          formatter.AddField("is_dark", room.is_dark);
          formatter.AddHexField("spriteset", room.spriteset, 2);
          formatter.AddHexField("tag1", room.tag1, 2);
          formatter.AddField("tag1_name", zelda3::GetRoomTagLabel(room.tag1));
          formatter.AddHexField("tag2", room.tag2, 2);
          formatter.AddField("tag2_name", zelda3::GetRoomTagLabel(room.tag2));
          break;
        case DungeonFindKind::kSprite:
          formatter.AddField("index", row.index);
          formatter.AddField("x", *row.x);
          formatter.AddField("y", *row.y);
          formatter.AddHexField("raw", row.raw, 2);
          formatter.AddField("name", row.name);
          formatter.AddField("subtype", row.subtype);
          formatter.AddField("layer", row.layer);
          if (row.key_drop > 0) {
            formatter.AddField("key_drop", row.key_drop);
            formatter.AddField("key_drop_kind",
                               row.key_drop == 2 ? "big" : "small");
          }
          break;
        case DungeonFindKind::kChest:
          formatter.AddField("index", row.index);
          formatter.AddHexField("raw", row.raw, 2);
          formatter.AddField("name", row.name);
          formatter.AddField("is_big_chest", row.is_big_chest);
          break;
        case DungeonFindKind::kPot:
          formatter.AddField("index", row.index);
          formatter.AddField("x", *row.x);
          formatter.AddField("y", *row.y);
          formatter.AddHexField("raw", row.raw, 2);
          formatter.AddField("name", row.name);
          formatter.AddHexField("position", row.pot_position, 4);
          break;
      }
      formatter.EndObject();
    }
  }
  formatter.EndArray();

  formatter.BeginObject("summary");
  formatter.AddField("rooms_scanned", end_room - start_room + 1);
  formatter.AddField("rooms_matched", rooms_matched);
  formatter.AddField("row_count", row_count);
  for (const auto& [kind, count] : by_kind) {
    formatter.AddField(absl::StrCat(kind, "_rows"), count);
  }
  formatter.EndObject();
  formatter.EndObject();
  return absl::OkStatus();
}

namespace {

absl::StatusOr<std::optional<int>> ParseOptionalHexArg(
    const resources::ArgumentParser& parser, const std::string& name,
    int max_value) {
  auto value = parser.GetString(name);
  if (!value.has_value()) {
    return std::optional<int>();
  }
  int parsed = 0;
  if (!util::ParseHexString(*value, &parsed) || parsed < 0 ||
      parsed > max_value) {
    return absl::InvalidArgumentError(absl::StrFormat(
        "--%s must be a hex value in [0x00, 0x%X]", name, max_value));
  }
  return std::optional<int>(parsed);
}

}  // namespace

absl::Status DungeonListEntrancesCommandHandler::ValidateArgs(
    const resources::ArgumentParser& parser) {
  RETURN_IF_ERROR(ParseOptionalHexArg(parser, "room", 0xFFFF).status());
  RETURN_IF_ERROR(ParseOptionalHexArg(parser, "dungeon", 0xFF).status());
  return absl::OkStatus();
}

absl::Status DungeonListEntrancesCommandHandler::Execute(
    Rom* rom, const resources::ArgumentParser& parser,
    resources::OutputFormatter& formatter) {
  ASSIGN_OR_RETURN(auto room_filter,
                   ParseOptionalHexArg(parser, "room", 0xFFFF));
  ASSIGN_OR_RETURN(auto dungeon_filter,
                   ParseOptionalHexArg(parser, "dungeon", 0xFF));
  ASSIGN_OR_RETURN(auto layout, zelda3::ResolveEntranceLayout(rom->vector()));

  formatter.BeginObject("Dungeon Entrances");
  formatter.AddField("layout",
                     layout.kind == zelda3::EntranceLayoutKind::kZscreamExpanded
                         ? "zscream_expanded"
                         : "legacy");
  formatter.AddField("entrance_count", zelda3::kNumRegularDungeonEntrances);
  int row_count = 0;
  formatter.BeginArray("entrances");
  for (int id = 0; id < zelda3::kNumRegularDungeonEntrances; ++id) {
    zelda3::RoomEntrance entrance(rom, static_cast<uint8_t>(id), false);
    RETURN_IF_ERROR(entrance.load_status());
    if (room_filter.has_value() && entrance.room_ != *room_filter) {
      continue;
    }
    if (dungeon_filter.has_value() && entrance.dungeon_id_ != *dungeon_filter) {
      continue;
    }
    formatter.BeginObject();
    formatter.AddHexField("entrance", id, 2);
    formatter.AddHexField("room", entrance.room_, 3);
    formatter.AddField("room_name", zelda3::GetRoomLabel(entrance.room_));
    formatter.AddHexField("dungeon_id", entrance.dungeon_id_, 2);
    formatter.AddHexField("exit", entrance.exit_, 4);
    formatter.AddHexField("blockset", entrance.blockset_, 2);
    formatter.AddHexField("floor", entrance.floor_, 2);
    formatter.AddHexField("music", entrance.music_, 2);
    formatter.AddField("x", entrance.x_position_);
    formatter.AddField("y", entrance.y_position_);
    formatter.EndObject();
    ++row_count;
  }
  formatter.EndArray();
  formatter.AddField("row_count", row_count);
  formatter.EndObject();
  return absl::OkStatus();
}

}  // namespace yaze::cli::handlers
