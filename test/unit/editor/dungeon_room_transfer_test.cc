#include "app/editor/dungeon/dungeon_room_transfer.h"

#include <limits>
#include <string>

#include "gtest/gtest.h"
#include "nlohmann/json.hpp"

namespace yaze::editor {
namespace {

using Json = nlohmann::json;

zelda3::RoomObject Object(int id, int x = 8, int y = 8, int layer = 0) {
  return zelda3::RoomObject(id, x, y, zelda3::CanonicalRoomObjectSize(id, 0),
                            layer);
}

DungeonRoomDocument Document() {
  DungeonRoomDocument document;
  document.source_room_id = 5;
  document.contents.objects = {Object(0xF99), Object(0x21)};
  document.contents.chests = {{0xF1, false}};
  zelda3::RoomObject torch(0x150, 10, 12, 0, 1);
  torch.set_options(zelda3::ObjectOption::Torch);
  torch.lit_ = true;
  torch.set_torch_reserved_bit(1);
  document.contents.objects.push_back(torch);
  zelda3::RoomObject block(0xE00, 4, 6, 0, 0);
  block.set_options(zelda3::ObjectOption::Block);
  block.set_block_behavior_layer(1);
  block.set_block_load_order(17);
  document.contents.objects.push_back(block);
  document.contents.doors = {zelda3::Room::Door::FromRomBytes(0x63, 2)};
  document.contents.sprites = {{8, 3, 4, 2, 1, 2, true}};
  document.contents.items = {{0x0408, 0xFE}};
  document.metadata.palette = 0x47;
  document.metadata.floor1 = 3;
  document.metadata.holewarp = 9;
  document.metadata.pit_target_layer = 2;
  document.metadata.staircase_rooms = {11, 12, 13, 14};
  document.metadata.staircase_planes = {0, 1, 2, 3};
  document.collision.has_data = true;
  document.collision.tiles[72] = 0xFE;
  document.water.has_data = true;
  document.water.tiles[73] = 1;
  document.water.sram_bit_mask = 4;
  return document;
}

Json DocumentJson() {
  const auto serialized = SerializeDungeonRoomDocument(Document());
  EXPECT_TRUE(serialized.ok()) << serialized.status();
  return serialized.ok() ? Json::parse(*serialized) : Json();
}

TEST(DungeonRoomTransferTest, JsonRoundTripPreservesSpecialFieldsAndRawValues) {
  auto document = Document();
  document.metadata.palette = 0xFF;
  document.metadata.spriteset = 0xFD;
  document.metadata.message = 0xFFFF;
  document.metadata.effect = static_cast<zelda3::EffectKey>(0xFE);
  document.metadata.tag2 = static_cast<zelda3::TagKey>(0xFC);
  const auto serialized = SerializeDungeonRoomDocument(document);
  ASSERT_TRUE(serialized.ok()) << serialized.status();
  const auto parsed = ParseDungeonRoomDocument(*serialized);
  ASSERT_TRUE(parsed.ok()) << parsed.status();
  EXPECT_TRUE(SameDungeonRoomDocument(document, *parsed));
  EXPECT_EQ(parsed->source_room_id, 5);
  EXPECT_EQ(parsed->contents.objects[3].block_load_order(), 17);
  EXPECT_EQ(parsed->contents.objects[2].torch_reserved_bit(), 1);
  EXPECT_TRUE(parsed->contents.sprites[0].deleted);
  EXPECT_EQ(parsed->metadata.pit_target_layer, 2);
  EXPECT_EQ(parsed->contents.objects[0].rom(), nullptr);
}

TEST(DungeonRoomTransferTest, CaptureDetachesObjectCachesAndBorrowedRom) {
  Rom rom;
  zelda3::Room room(2, &rom);
  auto object = Object(0x21);
  object.SetRom(&rom);
  object.tiles_loaded_ = true;
  object.preview_object_data_ = {1, 2, 3};
  room.SetTileObjects({object});
  const auto snapshot = CaptureDungeonRoomDocument(room);
  ASSERT_EQ(snapshot.contents.objects.size(), 1);
  EXPECT_EQ(snapshot.contents.objects[0].rom(), nullptr);
  EXPECT_FALSE(snapshot.contents.objects[0].tiles_loaded_);
  EXPECT_TRUE(snapshot.contents.objects[0].preview_object_data_.empty());
  EXPECT_EQ(room.GetTileObjects()[0].rom(), &rom);
}

TEST(DungeonRoomTransferTest,
     FlaggedPotPositionsSurviveJsonAndSelectedTransfer) {
  auto document = Document();
  document.contents.items = {{0x13CC, 0x0A}, {0x2660, 0x0B}, {0xA660, 0x88}};
  const auto json = SerializeDungeonRoomDocument(document);
  ASSERT_TRUE(json.ok()) << json.status();
  const auto parsed = ParseDungeonRoomDocument(*json);
  ASSERT_TRUE(parsed.ok()) << parsed.status();
  zelda3::Room target(7, nullptr);
  const auto plan = PlanDungeonRoomTransfer(target, *parsed, {kTransferItems});
  ASSERT_TRUE(plan.ok()) << plan.status();
  ApplyDungeonRoomDocument(target, plan->after);
  ASSERT_EQ(target.GetPotItems().size(), 3);
  for (size_t i = 0; i < document.contents.items.size(); ++i) {
    EXPECT_EQ(target.GetPotItems()[i].position,
              document.contents.items[i].position);
    EXPECT_EQ(target.GetPotItems()[i].item, document.contents.items[i].item);
  }
}

TEST(DungeonRoomTransferTest,
     RejectsNewMissingSpritesetButPreservesLegacyBytes) {
  zelda3::Room target(7, nullptr);
  auto document = Document();
  document.metadata.spriteset = 0x4F;
  ASSERT_TRUE(
      PlanDungeonRoomTransfer(target, document, {kTransferMetadata}).ok());
  for (uint8_t spriteset : {0x50, 0x8F, 0xFD}) {
    document.metadata.spriteset = spriteset;
    const auto json = SerializeDungeonRoomDocument(document);
    ASSERT_TRUE(json.ok()) << json.status();
    const auto parsed = ParseDungeonRoomDocument(*json);
    ASSERT_TRUE(parsed.ok()) << parsed.status();
    EXPECT_FALSE(
        PlanDungeonRoomTransfer(target, *parsed, {kTransferMetadata}).ok());
    EXPECT_TRUE(
        PlanDungeonRoomTransfer(target, *parsed, {kTransferItems}).ok());
  }
  target.SetSpriteset(0xFD);
  const auto original = CaptureDungeonRoomDocument(target);
  EXPECT_TRUE(
      PlanDungeonRoomTransfer(target, document, {kTransferMetadata}).ok());
  ApplyDungeonRoomDocument(target, Document());
  ApplyDungeonRoomDocument(target, original);
  EXPECT_EQ(target.spriteset(), 0xFD);
}

TEST(DungeonRoomTransferTest, DefaultPlanPreservesDestinationsAndOverlays) {
  zelda3::Room target(7, nullptr);
  target.SetHolewarp(42);
  target.SetPitsTargetLayer(3);
  target.SetStaircaseRoom(1, 55);
  target.SetStaircasePlane(1, 2);
  target.custom_collision().has_data = true;
  target.custom_collision().tiles[7] = 8;
  target.SetWaterFillTile(9, 10, true);
  const auto before = CaptureDungeonRoomDocument(target);
  const auto source = Document();
  const auto plan = PlanDungeonRoomTransfer(target, source);
  ASSERT_TRUE(plan.ok()) << plan.status();
  EXPECT_TRUE(plan->changed());
  EXPECT_EQ(plan->after.metadata.holewarp, 42);
  EXPECT_EQ(plan->after.metadata.pit_target_layer, 3);
  EXPECT_EQ(plan->after.metadata.staircase_rooms[1], 55);
  EXPECT_EQ(plan->after.metadata.staircase_planes[1], 2);
  EXPECT_EQ(plan->after.collision.tiles, before.collision.tiles);
  EXPECT_EQ(plan->after.water.tiles, before.water.tiles);
  EXPECT_EQ(plan->after.metadata.palette, 0x47);
  EXPECT_EQ(plan->after.contents.objects[3].block_load_order(), -1);
  EXPECT_EQ(source.contents.objects[3].block_load_order(), 17);
  EXPECT_TRUE(
      SameDungeonRoomDocument(before, CaptureDungeonRoomDocument(target)));
}

TEST(DungeonRoomTransferTest,
     ExplicitAllDomainPlanPublishesAndRestoresExactly) {
  Rom rom;
  zelda3::Room target(7, &rom);
  target.SetTileObjects({Object(0x30)});
  target.SetPitsTargetLayer(3);
  target.ClearSaveDirtyState();
  const auto plan =
      PlanDungeonRoomTransfer(target, Document(), {kTransferAll, true});
  ASSERT_TRUE(plan.ok()) << plan.status();
  EXPECT_FALSE(target.HasUnsavedChanges());
  ApplyDungeonRoomDocument(target, plan->after);
  EXPECT_TRUE(
      SameDungeonRoomDocument(plan->after, CaptureDungeonRoomDocument(target)));
  EXPECT_EQ(target.GetTileObjects()[0].rom(), &rom);
  EXPECT_EQ(target.WaterFillTileCount(), 1);
  EXPECT_EQ(target.water_fill_sram_bit_mask(), 4);
  EXPECT_TRUE(target.torches_dirty());
  EXPECT_TRUE(target.blocks_dirty());
  EXPECT_TRUE(target.chests_dirty());
  EXPECT_TRUE(target.sprites_dirty());
  EXPECT_TRUE(target.pot_items_dirty());
  EXPECT_TRUE(target.custom_collision_dirty());
  EXPECT_TRUE(target.water_fill_dirty());
  ApplyDungeonRoomDocument(target, plan->before);
  EXPECT_TRUE(SameDungeonRoomDocument(plan->before,
                                      CaptureDungeonRoomDocument(target)));
  EXPECT_EQ(target.WaterFillTileCount(), 0);
}

TEST(DungeonRoomTransferTest, PartialPlanPreservesExcludedLegacyData) {
  zelda3::Room target(7, nullptr);
  target.GetDoors().push_back(zelda3::Room::Door::FromRomBytes(0xFC, 0xFE));
  target.GetChests().push_back(
      {0xDE, false});  // Existing unmatched legacy data.
  const auto before = CaptureDungeonRoomDocument(target);
  const auto plan =
      PlanDungeonRoomTransfer(target, Document(), {kTransferSprites});
  ASSERT_TRUE(plan.ok()) << plan.status();
  EXPECT_EQ(plan->after.contents.doors[0].byte1, 0xFC);
  EXPECT_EQ(plan->after.contents.chests[0].id, 0xDE);
  ApplyDungeonRoomDocument(target, plan->after);
  EXPECT_FALSE(target.object_stream_dirty());
  EXPECT_FALSE(target.chests_dirty());
  EXPECT_TRUE(target.sprites_dirty());
  EXPECT_EQ(target.GetDoors()[0].byte1, before.contents.doors[0].byte1);
}

TEST(DungeonRoomTransferTest, UnchangedDocumentDoesNotDirtyRoom) {
  zelda3::Room target(7, nullptr);
  target.SetTileObjects({Object(0x21)});
  target.ClearSaveDirtyState();
  const auto source = CaptureDungeonRoomDocument(target);
  const auto plan =
      PlanDungeonRoomTransfer(target, source, {kTransferAll, true});
  ASSERT_TRUE(plan.ok()) << plan.status();
  EXPECT_FALSE(plan->changed());
  ApplyDungeonRoomDocument(target, plan->after);
  EXPECT_FALSE(target.HasUnsavedChanges());
}

TEST(DungeonRoomTransferTest, HistoryPreservesLegacyOverlayBytes) {
  zelda3::Room room(7, nullptr);
  room.SetWaterFillTile(1, 1, true);
  room.water_fill_zone().tiles[65] = 2;
  const auto original = CaptureDungeonRoomDocument(room);
  ApplyDungeonRoomDocument(room, Document());
  ApplyDungeonRoomDocument(room, original);
  EXPECT_EQ(room.water_fill_zone().tiles[65], 2);
  EXPECT_EQ(room.WaterFillTileCount(), 1);
  EXPECT_TRUE(
      SameDungeonRoomDocument(original, CaptureDungeonRoomDocument(room)));
}

TEST(DungeonRoomTransferTest, MetadataSnapshotRestoresPitPlaneIndependently) {
  zelda3::Room room(7, nullptr);
  room.SetPitsTargetLayer(3);
  const auto before = room.CaptureMetadataSnapshot();
  room.SetPitsTargetLayer(0);
  room.ClearSaveDirtyState();
  room.RestoreMetadataSnapshot(before);
  EXPECT_EQ(room.CaptureMetadataSnapshot().pit_target_layer, 3);
  EXPECT_TRUE(room.header_dirty());
  EXPECT_FALSE(room.object_stream_dirty());
}

TEST(DungeonRoomTransferTest, SelectionAndProvenanceAreNotInterchangeState) {
  auto a = Document();
  auto b = a;
  b.source_room_id = 33;
  b.contents.selected_objects = {0, 2};
  b.contents.selected_entities = {{EntityType::Door, 0}};
  EXPECT_TRUE(SameDungeonRoomDocument(a, b));
  const auto encoded = SerializeDungeonRoomDocument(b);
  ASSERT_TRUE(encoded.ok()) << encoded.status();
  const auto decoded = ParseDungeonRoomDocument(*encoded);
  ASSERT_TRUE(decoded.ok()) << decoded.status();
  EXPECT_TRUE(decoded->contents.selected_objects.empty());
  EXPECT_TRUE(decoded->contents.selected_entities.empty());
}

TEST(DungeonRoomTransferTest, RejectsInconsistentDerivedLayerMetadata) {
  auto j = DocumentJson();
  j["metadata"]["layer_merging"]["id"] = 6;
  EXPECT_FALSE(ParseDungeonRoomDocument(j.dump()).ok());
  j = DocumentJson();
  j["metadata"]["is_dark"] = true;
  EXPECT_FALSE(ParseDungeonRoomDocument(j.dump()).ok());
  j = DocumentJson();
  j["metadata"]["layer2_mode"] = 6;
  EXPECT_FALSE(ParseDungeonRoomDocument(j.dump()).ok());
  j = DocumentJson();
  j["metadata"]["layer_merging"]["visible"] = false;
  EXPECT_FALSE(ParseDungeonRoomDocument(j.dump()).ok());
}

TEST(DungeonRoomTransferTest, DarkRoomHiddenModeAndLegacyUndoRemainExact) {
  zelda3::Room room(7, nullptr);
  room.SetLayer2Mode(5);
  room.SetBg2(background2::DarkRoom);
  const auto original = CaptureDungeonRoomDocument(room);
  const auto encoded = SerializeDungeonRoomDocument(original);
  ASSERT_TRUE(encoded.ok()) << encoded.status();
  const auto decoded = ParseDungeonRoomDocument(*encoded);
  ASSERT_TRUE(decoded.ok()) << decoded.status();
  EXPECT_EQ(decoded->metadata.layer2_mode, 5);
  EXPECT_EQ(decoded->metadata.layer_merging, zelda3::LayerMerge08);
  EXPECT_TRUE(SameDungeonRoomDocument(original, *decoded));

  auto legacy = original;
  legacy.metadata.layer_merging = zelda3::LayerMerge06;
  ApplyDungeonRoomDocument(room, legacy);
  EXPECT_TRUE(
      SameDungeonRoomDocument(legacy, CaptureDungeonRoomDocument(room)));
  EXPECT_FALSE(ValidateDungeonRoomDocument(legacy).ok());
  const auto partial =
      PlanDungeonRoomTransfer(room, Document(), {kTransferSprites});
  ASSERT_TRUE(partial.ok()) << partial.status();
  EXPECT_EQ(partial->after.metadata, legacy.metadata);
}

TEST(DungeonRoomTransferTest, RejectsChestMappingAndSharedSlotOrder) {
  auto source = Document();
  source.contents.chests.clear();
  EXPECT_FALSE(ValidateDungeonRoomDocument(source).ok());
  source = Document();
  source.contents.objects.insert(source.contents.objects.begin(),
                                 Object(0xF98));
  EXPECT_FALSE(ValidateDungeonRoomDocument(source).ok());
}

TEST(DungeonRoomTransferTest, RejectsCapacityWithoutChangingTarget) {
  zelda3::Room target(7, nullptr);
  const auto before = CaptureDungeonRoomDocument(target);
  auto source = Document();
  source.contents.doors.resize(zelda3::kMaxDoors + 1,
                               source.contents.doors.front());
  const auto plan = PlanDungeonRoomTransfer(target, source);
  EXPECT_FALSE(plan.ok());
  EXPECT_TRUE(
      SameDungeonRoomDocument(before, CaptureDungeonRoomDocument(target)));
  EXPECT_FALSE(target.HasUnsavedChanges());
}

TEST(DungeonRoomTransferTest, RejectsUnknownDomains) {
  zelda3::Room target(7, nullptr);
  EXPECT_FALSE(PlanDungeonRoomTransfer(target, Document(), {0x8000}).ok());
}

TEST(DungeonRoomTransferTest, RejectsNoncanonicalSpecialObjectIdentity) {
  auto source = Document();
  source.contents.objects[2].id_ = 0x21;
  EXPECT_FALSE(ValidateDungeonRoomDocument(source).ok());
  source = Document();
  source.contents.objects[2].x_ = 63;
  EXPECT_FALSE(ValidateDungeonRoomDocument(source).ok());
}

class DungeonRoomDocumentInvalidTest : public ::testing::TestWithParam<int> {};

TEST_P(DungeonRoomDocumentInvalidTest, RejectsMalformedFieldWithoutNarrowing) {
  auto j = DocumentJson();
  switch (GetParam()) {
    case 0:
      j["version"] = 2;
      break;
    case 1:
      j["format"] = "other.room";
      break;
    case 2:
      j["objects"][0]["x"] = -1;
      break;
    case 3:
      j["objects"][0]["x"] = 256;
      break;
    case 4:
      j["objects"][0]["x"] = 2.0;
      break;
    case 5:
      j["objects"][0]["x"] = true;
      break;
    case 6:
      j["objects"][0]["x"] = "8";
      break;
    case 7:
      j["objects"][0]["x"] = std::numeric_limits<uint64_t>::max();
      break;
    case 8:
      j["objects"][0]["id"] = 0xF8;
      break;
    case 9:
      j["objects"][0]["size"] = 255;
      break;
    case 10:
      j["objects"][0]["unknown"] = 0;
      break;
    case 11:
      j.erase("water");
      break;
    case 12:
      j["unknown"] = 0;
      break;
    case 13:
      j["chests"][0]["big"] = 0;
      break;
    case 14:
      j["doors"][0]["position"] = 16;
      break;
    case 15:
      j["doors"][0]["byte1"] = 0xFF;
      break;
    case 16:
      j["sprites"][0]["key_drop"] = 3;
      break;
    case 17:
      j["sprites"][0]["y"] = 31;
      j["sprites"][0]["subtype"] = 24;
      break;
    case 18:
      j["pot_items"][0]["position"] = 0xFFFF;
      break;
    case 19:
      j["metadata"]["pit_target_layer"] = 4;
      break;
    case 20:
      j["metadata"]["staircase_planes"] = Json::array({0, 1, 2});
      break;
    case 21:
      j["metadata"]["floor1"] = 16;
      break;
    case 22:
      j["collision"]["tiles"].push_back(0);
      break;
    case 23:
      j["water"]["tiles"][0] = 2;
      break;
    case 24:
      j["water"]["sram_bit_mask"] = 3;
      break;
    case 25:
      j["collision"]["has_data"] = false;
      break;
    case 26:
      j["water"]["has_data"] = false;
      break;
    case 27:
      j["objects"] = Json::array();
      break;
    case 28:
      j["source_room_id"] = 296;
      break;
    case 29:
      j["objects"][2]["options"] = 12;
      break;
    case 30:
      j["objects"][2]["torch_reserved_bit"] = 2;
      break;
    case 31:
      j["sprites"] = Json::object();
      break;
  }
  EXPECT_FALSE(ParseDungeonRoomDocument(j.dump()).ok());
}

INSTANTIATE_TEST_SUITE_P(StrictSchema, DungeonRoomDocumentInvalidTest,
                         ::testing::Range(0, 32));

TEST(DungeonRoomTransferTest, RejectsDuplicateKeysAndLegacyTemplate) {
  auto encoded = SerializeDungeonRoomDocument(Document());
  ASSERT_TRUE(encoded.ok()) << encoded.status();
  encoded->insert(1, "\"version\":1,");
  EXPECT_FALSE(ParseDungeonRoomDocument(*encoded).ok());
  EXPECT_FALSE(ParseDungeonRoomDocument(R"({"version":1,"objects":[]})").ok());
  EXPECT_FALSE(ParseDungeonRoomDocument("{").ok());
}

TEST(DungeonRoomTransferTest, RejectsOversizedAndDeeplyNestedDocuments) {
  EXPECT_FALSE(ParseDungeonRoomDocument(
                   std::string(kMaxDungeonRoomDocumentBytes + 1, ' '))
                   .ok());
  EXPECT_FALSE(ParseDungeonRoomDocument(std::string(40, '[') + "0" +
                                        std::string(40, ']'))
                   .ok());
}

}  // namespace
}  // namespace yaze::editor
