#include "app/editor/dungeon/dungeon_room_transfer.h"

#include <initializer_list>
#include <set>
#include <stdexcept>
#include <string>
#include <vector>

#include "nlohmann/json.hpp"
#include "util/macro.h"

namespace yaze::editor {
namespace {

using Json = nlohmann::json;

void Keys(const Json& value, std::initializer_list<const char*> keys) {
  if (!value.is_object() || value.size() != keys.size()) {
    throw std::invalid_argument(
        "Unexpected, missing, or unknown room document fields");
  }
  for (const char* key : keys) {
    if (!value.contains(key)) {
      throw std::invalid_argument(std::string("Missing required field: ") +
                                  key);
    }
  }
}

int Integer(const Json& value, int minimum, int maximum) {
  if (value.is_number_unsigned()) {
    const uint64_t number = value.get<uint64_t>();
    if (number <= static_cast<uint64_t>(maximum) &&
        (minimum <= 0 || number >= static_cast<uint64_t>(minimum))) {
      return static_cast<int>(number);
    }
  } else if (value.is_number_integer()) {
    const int64_t number = value.get<int64_t>();
    if (number >= minimum && number <= maximum)
      return static_cast<int>(number);
  }
  throw std::invalid_argument(
      "Expected an integer inside the field's ROM range");
}

bool Boolean(const Json& value) {
  if (!value.is_boolean())
    throw std::invalid_argument("Expected a boolean field");
  return value.get<bool>();
}

const Json& Array(const Json& value, size_t maximum) {
  if (!value.is_array() || value.size() > maximum) {
    throw std::invalid_argument("Expected an array within its room limit");
  }
  return value;
}

template <size_t N>
std::array<uint8_t, N> ByteArray(const Json& value, int maximum = 255) {
  Array(value, N);
  if (value.size() != N)
    throw std::invalid_argument("Incorrect fixed array length");
  std::array<uint8_t, N> result{};
  for (size_t i = 0; i < N; ++i)
    result[i] = Integer(value[i], 0, maximum);
  return result;
}

Json MetadataToJson(const zelda3::Room::MetadataSnapshot& m) {
  return {{"palette", m.palette},
          {"blockset", m.blockset},
          {"spriteset", m.spriteset},
          {"layout", m.layout},
          {"floor1", m.floor1},
          {"floor2", m.floor2},
          {"message", m.message},
          {"bg2", static_cast<int>(m.bg2)},
          {"layer2_mode", m.layer2_mode},
          {"layer_merging",
           {{"id", m.layer_merging.ID},
            {"name", m.layer_merging.Name},
            {"on_top", m.layer_merging.Layer2OnTop},
            {"translucent", m.layer_merging.Layer2Translucent},
            {"visible", m.layer_merging.Layer2Visible}}},
          {"is_dark", m.is_dark},
          {"is_light", m.is_light},
          {"collision", static_cast<int>(m.collision)},
          {"effect", static_cast<int>(m.effect)},
          {"tag1", static_cast<int>(m.tag1)},
          {"tag2", static_cast<int>(m.tag2)},
          {"holewarp", m.holewarp},
          {"pit_target_layer", m.pit_target_layer},
          {"staircase_rooms", m.staircase_rooms},
          {"staircase_planes", m.staircase_planes}};
}

zelda3::Room::MetadataSnapshot MetadataFromJson(const Json& j) {
  Keys(j, {"palette",
           "blockset",
           "spriteset",
           "layout",
           "floor1",
           "floor2",
           "message",
           "bg2",
           "layer2_mode",
           "layer_merging",
           "is_dark",
           "is_light",
           "collision",
           "effect",
           "tag1",
           "tag2",
           "holewarp",
           "pit_target_layer",
           "staircase_rooms",
           "staircase_planes"});
  zelda3::Room::MetadataSnapshot m;
  m.palette = Integer(j.at("palette"), 0, 255);
  m.blockset = Integer(j.at("blockset"), 0, 255);
  m.spriteset = Integer(j.at("spriteset"), 0, 255);
  m.layout = Integer(j.at("layout"), 0, 7);
  m.floor1 = Integer(j.at("floor1"), 0, 15);
  m.floor2 = Integer(j.at("floor2"), 0, 15);
  m.message = Integer(j.at("message"), 0, 65535);
  m.bg2 = static_cast<background2>(Integer(j.at("bg2"), 0, 8));
  m.layer2_mode = Integer(j.at("layer2_mode"), 0, 7);
  const auto& merging = j.at("layer_merging");
  Keys(merging, {"id", "name", "on_top", "translucent", "visible"});
  if (!merging.at("name").is_string() ||
      merging.at("name").get_ref<const std::string&>().size() > 64) {
    throw std::invalid_argument("Invalid layer merging name");
  }
  m.layer_merging = zelda3::LayerMergeType(
      Integer(merging.at("id"), 0, 8), merging.at("name").get<std::string>(),
      Boolean(merging.at("visible")), Boolean(merging.at("on_top")),
      Boolean(merging.at("translucent")));
  m.is_dark = Boolean(j.at("is_dark"));
  m.is_light = Boolean(j.at("is_light"));
  m.collision =
      static_cast<zelda3::CollisionKey>(Integer(j.at("collision"), 0, 7));
  m.effect = static_cast<zelda3::EffectKey>(Integer(j.at("effect"), 0, 255));
  m.tag1 = static_cast<zelda3::TagKey>(Integer(j.at("tag1"), 0, 255));
  m.tag2 = static_cast<zelda3::TagKey>(Integer(j.at("tag2"), 0, 255));
  m.holewarp = Integer(j.at("holewarp"), 0, 255);
  m.pit_target_layer = Integer(j.at("pit_target_layer"), 0, 3);
  m.staircase_rooms = ByteArray<4>(j.at("staircase_rooms"));
  m.staircase_planes = ByteArray<4>(j.at("staircase_planes"), 3);
  return m;
}

}  // namespace

absl::StatusOr<std::string> SerializeDungeonRoomDocument(
    const DungeonRoomDocument& document) {
  RETURN_IF_ERROR(ValidateDungeonRoomDocumentForInterchange(document));
  try {
    Json j{{"format", "yaze.room"},
           {"version", 1},
           {"source_room_id", document.source_room_id},
           {"metadata", MetadataToJson(document.metadata)},
           {"objects", Json::array()},
           {"chests", Json::array()},
           {"doors", Json::array()},
           {"sprites", Json::array()},
           {"pot_items", Json::array()},
           {"collision",
            {{"has_data", document.collision.has_data},
             {"tiles", document.collision.tiles}}},
           {"water",
            {{"has_data", document.water.has_data},
             {"sram_bit_mask", document.water.sram_bit_mask},
             {"tiles", document.water.tiles}}}};
    for (const auto& o : document.contents.objects) {
      j["objects"].push_back(
          {{"id", o.id_},
           {"x", o.x_},
           {"y", o.y_},
           {"size", o.size_},
           {"layer", o.GetLayerValue()},
           {"options", static_cast<int>(o.options())},
           {"all_bgs", o.all_bgs_},
           {"lit", o.lit_},
           // Physical table slots are save provenance, not portable content.
           {"block_load_order", zelda3::RoomObject::kBlockLoadOrderNew},
           {"block_behavior_layer", o.block_behavior_layer()},
           {"torch_reserved_bit", o.torch_reserved_bit()}});
    }
    for (auto chest : document.contents.chests) {
      j["chests"].push_back({{"id", chest.id}, {"big", chest.size}});
    }
    for (auto door : document.contents.doors) {
      j["doors"].push_back({{"position", door.position},
                            {"type", static_cast<int>(door.type)},
                            {"direction", static_cast<int>(door.direction)},
                            {"byte1", door.byte1},
                            {"byte2", door.byte2}});
    }
    for (auto s : document.contents.sprites) {
      j["sprites"].push_back({{"id", s.id},
                              {"x", s.x},
                              {"y", s.y},
                              {"subtype", s.subtype},
                              {"layer", s.layer},
                              {"key_drop", s.key_drop},
                              {"deleted", s.deleted}});
    }
    for (auto item : document.contents.items) {
      j["pot_items"].push_back(
          {{"position", item.position}, {"item", item.item}});
    }
    auto serialized = j.dump(2);
    if (serialized.size() > kMaxDungeonRoomDocumentBytes) {
      return absl::ResourceExhaustedError("Room document exceeds 1 MiB");
    }
    return serialized;
  } catch (const Json::exception& error) {
    return absl::InvalidArgumentError(
        std::string("Cannot serialize room document: ") + error.what());
  }
}

absl::StatusOr<DungeonRoomDocument> ParseDungeonRoomDocument(
    const std::string& text) {
  if (text.size() > kMaxDungeonRoomDocumentBytes) {
    return absl::ResourceExhaustedError("Room document exceeds 1 MiB");
  }
  try {
    std::vector<std::set<std::string>> object_keys;
    const auto j = Json::parse(
        text, [&](int depth, Json::parse_event_t event, Json& value) {
          if (depth > 32)
            throw std::invalid_argument("Room document nesting is too deep");
          if (event == Json::parse_event_t::object_start)
            object_keys.emplace_back();
          if (event == Json::parse_event_t::key &&
              !object_keys.back().insert(value.get<std::string>()).second) {
            throw std::invalid_argument("Duplicate room document field");
          }
          if (event == Json::parse_event_t::object_end)
            object_keys.pop_back();
          return true;
        });
    Keys(j, {"format", "version", "source_room_id", "metadata", "objects",
             "chests", "doors", "sprites", "pot_items", "collision", "water"});
    if (!j.at("format").is_string() || j.at("format") != "yaze.room" ||
        Integer(j.at("version"), 1, 1) != 1) {
      throw std::invalid_argument(
          "Unsupported room document format or version");
    }
    DungeonRoomDocument document;
    document.source_room_id =
        Integer(j.at("source_room_id"), 0, zelda3::kNumberOfRooms - 1);
    document.metadata = MetadataFromJson(j.at("metadata"));
    for (const auto& o : Array(j.at("objects"), zelda3::kMaxTileObjects)) {
      Keys(o,
           {"id", "x", "y", "size", "layer", "options", "all_bgs", "lit",
            "block_load_order", "block_behavior_layer", "torch_reserved_bit"});
      zelda3::RoomObject object(
          Integer(o.at("id"), 0, 4095), Integer(o.at("x"), 0, 63),
          Integer(o.at("y"), 0, 63), Integer(o.at("size"), 0, 255),
          Integer(o.at("layer"), 0, 2));
      object.set_options(
          static_cast<zelda3::ObjectOption>(Integer(o.at("options"), 0, 63)));
      object.all_bgs_ = Boolean(o.at("all_bgs"));
      object.lit_ = Boolean(o.at("lit"));
      // Accept older v1 documents but never import a foreign physical slot.
      (void)Integer(o.at("block_load_order"), -1, 65535);
      object.set_block_load_order(zelda3::RoomObject::kBlockLoadOrderNew);
      object.set_block_behavior_layer(
          Integer(o.at("block_behavior_layer"), 0, 1));
      object.set_torch_reserved_bit(Integer(o.at("torch_reserved_bit"), 0, 1));
      document.contents.objects.push_back(std::move(object));
    }
    for (const auto& c : Array(j.at("chests"), zelda3::kMaxChests)) {
      Keys(c, {"id", "big"});
      document.contents.chests.push_back(
          {static_cast<uint8_t>(Integer(c.at("id"), 0, 255)),
           Boolean(c.at("big"))});
    }
    for (const auto& d : Array(j.at("doors"), zelda3::kMaxDoors)) {
      Keys(d, {"position", "type", "direction", "byte1", "byte2"});
      document.contents.doors.push_back(
          {static_cast<uint8_t>(Integer(d.at("position"), 0, 15)),
           static_cast<zelda3::DoorType>(Integer(d.at("type"), 0, 255)),
           static_cast<zelda3::DoorDirection>(Integer(d.at("direction"), 0, 3)),
           static_cast<uint8_t>(Integer(d.at("byte1"), 0, 255)),
           static_cast<uint8_t>(Integer(d.at("byte2"), 0, 255))});
    }
    for (const auto& s : Array(j.at("sprites"), zelda3::kMaxTotalSprites)) {
      Keys(s, {"id", "x", "y", "subtype", "layer", "key_drop", "deleted"});
      document.contents.sprites.push_back(
          {static_cast<uint8_t>(Integer(s.at("id"), 0, 255)),
           Integer(s.at("x"), 0, 31), Integer(s.at("y"), 0, 31),
           Integer(s.at("subtype"), 0, 31), Integer(s.at("layer"), 0, 1),
           Integer(s.at("key_drop"), 0, 2), Boolean(s.at("deleted"))});
    }
    for (const auto& i : Array(j.at("pot_items"), 4096)) {
      Keys(i, {"position", "item"});
      document.contents.items.push_back(
          {static_cast<uint16_t>(Integer(i.at("position"), 0, 65535)),
           static_cast<uint8_t>(Integer(i.at("item"), 0, 255))});
    }
    const auto& collision = j.at("collision");
    Keys(collision, {"has_data", "tiles"});
    document.collision.has_data = Boolean(collision.at("has_data"));
    document.collision.tiles = ByteArray<4096>(collision.at("tiles"));
    const auto& water = j.at("water");
    Keys(water, {"has_data", "sram_bit_mask", "tiles"});
    document.water.has_data = Boolean(water.at("has_data"));
    document.water.sram_bit_mask = Integer(water.at("sram_bit_mask"), 0, 255);
    document.water.tiles = ByteArray<4096>(water.at("tiles"), 1);
    RETURN_IF_ERROR(ValidateDungeonRoomDocumentForInterchange(document));
    return document;
  } catch (const Json::exception& error) {
    return absl::InvalidArgumentError(std::string("Invalid room document: ") +
                                      error.what());
  } catch (const std::invalid_argument& error) {
    return absl::InvalidArgumentError(std::string("Invalid room document: ") +
                                      error.what());
  }
}

}  // namespace yaze::editor
