#include "app/editor/overworld/entity/overworld_connection_adapter.h"

#include <algorithm>
#include <array>
#include <optional>
#include <string_view>
#include <tuple>
#include <utility>
#include <variant>
#include <vector>

#include "absl/status/status.h"
#include "gtest/gtest.h"

namespace yaze::editor {
namespace {

core::LyraTerrainProfile Profile() {
  core::LyraTerrainProfile profile;
  profile.tile_count = 4096;
  profile.tile16_definition_fingerprint = "definitions:connection-fixture";
  profile.graphics_fingerprint = "graphics:connection-fixture";
  profile.palette_fingerprint = "palette:connection-fixture";
  profile.animation_fingerprint = "animation:connection-fixture";
  return profile;
}

zelda3::OverworldEntrance Entrance(int parent, int x, int y,
                                   uint8_t destination, bool hole = false) {
  zelda3::OverworldEntrance record;
  record.map_id_ = parent;
  record.x_ = x;
  record.y_ = y;
  record.entrance_id_ = destination;
  record.is_hole_ = hole;
  return record;
}

auto ReturnFields(const zelda3::OverworldExit& record) {
  return std::make_tuple(
      record.map_id_, record.entity_id_, record.x_, record.y_, record.game_x_,
      record.game_y_, record.room_id_, record.map_pos_, record.x_player_,
      record.y_player_, record.x_scroll_, record.y_scroll_, record.x_camera_,
      record.y_camera_, record.scroll_mod_x_, record.scroll_mod_y_,
      record.door_type_1_, record.door_type_2_, record.is_hole_,
      record.deleted_, record.is_automatic_);
}

class OverworldConnectionAdapterTest : public ::testing::Test {
 protected:
  void SetUp() override {
    auto created = core::LyraTerrainDocument::Create(Profile(), 7);
    ASSERT_TRUE(created.ok()) << created.status();
    document_.emplace(std::move(*created));
    auto state = document_->authoring();
    state.maps[0].doors.push_back({1, 1, 2, 1});
    state.maps[0].doors.push_back({7, 3, 4, std::nullopt});
    state.maps[0].exits.push_back({1, 0x105, 5, 6, 1});
    state.maps[31].exits.push_back({9, 0x127, 31, 31, std::nullopt});
    state.maps[2].sprites.push_back({11, 0x42, 8, 9, 2});
    state.flight_points.push_back(
        {12, 0x27, 10, 11, "Unrelated", std::nullopt});
    ASSERT_TRUE(document_->SetAuthoring(document_->authoring(), state).ok());
    document_->MarkSaved();
  }

  OverworldConnectionRecords Records() const {
    return {{}, {}, {}, &document_->authoring()};
  }

  absl::StatusOr<OverworldConnectionView> Door(uint32_t id = 1) const {
    return ResolveOverworldConnection(Records(), ProjectEntranceReference{id});
  }

  absl::StatusOr<OverworldConnectionView> Return(uint32_t id = 1) const {
    return ResolveOverworldConnection(Records(), ProjectReturnReference{id});
  }

  static ProjectConnectionEdit EditAt(
      int map, int local_x, int local_y,
      OverworldConnectionDestination destination) {
    return {map, (map % 8) * 512 + local_x * 16,
            (map % 64 / 8) * 512 + local_y * 16, std::move(destination)};
  }

  std::optional<core::LyraTerrainDocument> document_;
};

TEST_F(OverworldConnectionAdapterTest,
       SameNumericIdentityIsDistinctAcrossTablesKindsAndDestinations) {
  const std::array<OverworldConnectionReference, 5> references = {
      NativeEntranceReference{NativeEntranceTable::kEntrances, 1},
      NativeEntranceReference{NativeEntranceTable::kHoles, 1},
      NativeReturnReference{1}, ProjectEntranceReference{1},
      ProjectReturnReference{1}};
  for (size_t i = 0; i < references.size(); ++i) {
    for (size_t j = i + 1; j < references.size(); ++j) {
      EXPECT_NE(references[i], references[j]);
    }
  }
  std::vector<zelda3::OverworldEntrance> entrances{Entrance(0, 0, 0, 9),
                                                   Entrance(0, 16, 0, 1)};
  const OverworldConnectionRecords records{
      entrances, {}, {}, &document_->authoring()};
  auto native = ResolveOverworldConnection(records, references[0]);
  auto project = ResolveOverworldConnection(records, references[3]);
  auto project_return = ResolveOverworldConnection(records, references[4]);
  ASSERT_TRUE(native.ok()) << native.status();
  ASSERT_TRUE(project.ok()) << project.status();
  ASSERT_TRUE(project_return.ok()) << project_return.status();
  EXPECT_NE(native->reference, project->reference);
  EXPECT_EQ(
      std::get<DungeonEntranceDestination>(native->destination).entrance_id, 1);
  EXPECT_EQ(
      std::get<DungeonEntranceDestination>(project->destination).entrance_id,
      1);
  EXPECT_NE(project->destination, project_return->destination);
  EXPECT_NE(OverworldConnectionDestination(DungeonEntranceDestination{1}),
            OverworldConnectionDestination(WorldLinkDestination{1}));
}

TEST_F(OverworldConnectionAdapterTest,
       ResolvesNormalAndHoleRowsWithoutUsingDestinationAsIdentity) {
  std::vector<zelda3::OverworldEntrance> entrances{Entrance(0, 16, 32, 0x25)};
  std::vector<zelda3::OverworldEntrance> holes{Entrance(0, 48, 64, 0x27, true)};
  const OverworldConnectionRecords records{entrances, holes, {}, nullptr};
  auto normal = ResolveOverworldConnection(
      records, NativeEntranceReference{NativeEntranceTable::kEntrances, 0});
  auto hole = ResolveOverworldConnection(
      records, NativeEntranceReference{NativeEntranceTable::kHoles, 0});
  ASSERT_TRUE(normal.ok()) << normal.status();
  ASSERT_TRUE(hole.ok()) << hole.status();
  EXPECT_FALSE(normal->is_hole);
  EXPECT_TRUE(hole->is_hole);
  EXPECT_EQ(normal->world_x, 16);
  EXPECT_EQ(hole->world_y, 64);
  EXPECT_EQ(
      std::get<DungeonEntranceDestination>(normal->destination).entrance_id,
      0x25);
  EXPECT_EQ(std::get<DungeonEntranceDestination>(hole->destination).entrance_id,
            0x27);
  EXPECT_NE(normal->reference, hole->reference);
}

TEST_F(OverworldConnectionAdapterTest,
       CollectSkipsDeletedNativeRowsAndRetainsAllProjectKinds) {
  std::vector<zelda3::OverworldEntrance> entrances{Entrance(0, 0, 0, 1),
                                                   Entrance(0, 16, 0, 2)};
  entrances[0].deleted = true;
  std::vector<zelda3::OverworldEntrance> holes{Entrance(0, 32, 0, 3, true)};
  std::vector<zelda3::OverworldExit> returns(2);
  returns[0].deleted_ = true;
  const OverworldConnectionRecords records{entrances, holes, returns,
                                           &document_->authoring()};
  auto views = CollectOverworldConnections(records);
  ASSERT_TRUE(views.ok()) << views.status();
  ASSERT_EQ(views->size(), 7);
  EXPECT_EQ(views->at(0).reference,
            OverworldConnectionReference(
                NativeEntranceReference{NativeEntranceTable::kEntrances, 1}));
  EXPECT_EQ(views->at(1).reference,
            OverworldConnectionReference(
                NativeEntranceReference{NativeEntranceTable::kHoles, 0}));
  EXPECT_EQ(views->at(2).reference,
            OverworldConnectionReference(NativeReturnReference{1}));
}

TEST_F(OverworldConnectionAdapterTest,
       NativeReturnRetainsCameraVramFlagsAndSubtilePlayerPosition) {
  zelda3::OverworldExit record;
  record.map_id_ = 0x1E;
  record.entity_id_ = 87;
  record.x_ = 0x0FE1;
  record.y_ = 0x093F;
  record.game_x_ = 0x3E;
  record.game_y_ = 0x33;
  record.room_id_ = 0x105;
  record.map_pos_ = 0x49A2;
  record.x_player_ = 0x0FE1;
  record.y_player_ = 0x093F;
  record.x_scroll_ = 0x0E20;
  record.y_scroll_ = 0x0808;
  record.x_camera_ = 0x0F01;
  record.y_camera_ = 0x08D5;
  record.scroll_mod_x_ = 7;
  record.scroll_mod_y_ = 9;
  record.door_type_1_ = 0x8123;
  record.door_type_2_ = 0x4567;
  record.is_hole_ = true;
  record.is_automatic_ = false;
  std::vector<zelda3::OverworldExit> returns{record};
  const auto before = ReturnFields(returns[0]);
  auto view = ResolveOverworldConnection({{}, {}, returns, nullptr},
                                         NativeReturnReference{0});
  ASSERT_TRUE(view.ok()) << view.status();
  EXPECT_EQ(view->physical_map_id, 0x27);
  EXPECT_EQ(view->parent_map_id, 0x1E);
  EXPECT_EQ(view->world_x, 0x0FE1);
  EXPECT_EQ(view->world_y, 0x093F);
  EXPECT_EQ(ReturnFields(std::get<zelda3::OverworldExit>(view->record)),
            before);
  EXPECT_EQ(ReturnFields(returns[0]), before);
  EXPECT_EQ(std::get<DungeonReturnDestination>(view->destination),
            (DungeonReturnDestination{0x105, 0}));
}

TEST_F(OverworldConnectionAdapterTest,
       NativeParentAndPhysicalQuadrantsRemainSeparate) {
  const std::array<int, 4> expected_maps{0x1E, 0x1F, 0x26, 0x27};
  std::vector<zelda3::OverworldEntrance> entrances{
      Entrance(0x1E, 0x0C10, 0x0610, 1), Entrance(0x1E, 0x0E10, 0x0610, 1),
      Entrance(0x1E, 0x0C10, 0x0810, 1), Entrance(0x1E, 0x0E10, 0x0810, 1)};
  for (size_t row = 0; row < entrances.size(); ++row) {
    auto view = ResolveOverworldConnection(
        {entrances, {}, {}, nullptr},
        NativeEntranceReference{NativeEntranceTable::kEntrances, row});
    ASSERT_TRUE(view.ok()) << view.status();
    EXPECT_EQ(view->physical_map_id, expected_maps[row]);
    EXPECT_EQ(view->parent_map_id, 0x1E);
  }
}

TEST_F(OverworldConnectionAdapterTest,
       NativeMissingDeletedAndInvalidTablesRefuseWithoutMutation) {
  std::vector<zelda3::OverworldEntrance> entrances{Entrance(0, 0, 0, 1)};
  entrances[0].deleted = true;
  const OverworldConnectionRecords records{entrances, {}, {}, nullptr};
  EXPECT_TRUE(absl::IsNotFound(
      ResolveOverworldConnection(
          records, NativeEntranceReference{NativeEntranceTable::kEntrances, 0})
          .status()));
  EXPECT_TRUE(absl::IsNotFound(
      ResolveOverworldConnection(
          records, NativeEntranceReference{NativeEntranceTable::kHoles, 0})
          .status()));
  EXPECT_TRUE(absl::IsInvalidArgument(
      ResolveOverworldConnection(
          records,
          NativeEntranceReference{static_cast<NativeEntranceTable>(99), 0})
          .status()));
  EXPECT_TRUE(entrances[0].deleted);
}

TEST_F(OverworldConnectionAdapterTest, NativeReturnRowsStopAt78) {
  std::vector<zelda3::OverworldExit> returns(80);
  for (auto& record : returns)
    record.deleted_ = true;
  returns[78].deleted_ = false;
  returns[79].deleted_ = false;
  const OverworldConnectionRecords records{{}, {}, returns, nullptr};
  EXPECT_TRUE(
      ResolveOverworldConnection(records, NativeReturnReference{78}).ok());
  EXPECT_TRUE(absl::IsNotFound(
      ResolveOverworldConnection(records, NativeReturnReference{79}).status()));
}

TEST_F(OverworldConnectionAdapterTest, ProjectA0AndBfUseWorldPixelCoordinates) {
  auto door = Door();
  auto exit = Return(9);
  ASSERT_TRUE(door.ok()) << door.status();
  ASSERT_TRUE(exit.ok()) << exit.status();
  EXPECT_EQ(door->physical_map_id, 0xA0);
  EXPECT_EQ(door->world_x, 16);
  EXPECT_EQ(door->world_y, 0x0820);
  EXPECT_FALSE(door->parent_map_id.has_value());
  EXPECT_EQ(exit->physical_map_id, 0xBF);
  EXPECT_EQ(exit->world_x, 4080);
  EXPECT_EQ(exit->world_y, 4080);
  EXPECT_EQ(std::get<DungeonReturnDestination>(exit->destination).row,
            std::nullopt);
}

TEST_F(OverworldConnectionAdapterTest,
       ProjectGameplayAreaExposesExplicitParent) {
  auto state = document_->authoring();
  state.gameplay_areas.push_back(
      {0xA0, core::LyraGameplayAreaSize::kLarge, {0xA0, 0xA1, 0xA8, 0xA9}});
  state.maps[0].doors.clear();
  state.maps[9].doors.push_back({1, 31, 0, 0x25});
  ASSERT_TRUE(document_->SetAuthoring(document_->authoring(), state).ok());
  auto view = Door();
  ASSERT_TRUE(view.ok()) << view.status();
  EXPECT_EQ(view->physical_map_id, 0xA9);
  EXPECT_EQ(view->parent_map_id, 0xA0);
  EXPECT_EQ(view->world_x, 1008);
  EXPECT_EQ(view->world_y, 2560);
}

TEST_F(OverworldConnectionAdapterTest,
       ProjectIdentityResolvesAfterVectorReorderAndMapMove) {
  auto state = document_->authoring();
  std::reverse(state.maps[0].doors.begin(), state.maps[0].doors.end());
  const auto moved = state.maps[0].doors.back();
  state.maps[0].doors.pop_back();
  state.maps[31].doors.push_back(moved);
  ASSERT_TRUE(document_->SetAuthoring(document_->authoring(), state).ok());
  auto view = Door();
  ASSERT_TRUE(view.ok()) << view.status();
  EXPECT_EQ(view->physical_map_id, 0xBF);
  EXPECT_EQ(std::get<core::LyraDoorPlacement>(view->record), moved);
  EXPECT_EQ(std::get<ProjectEntranceReference>(view->reference).id, 1u);
  auto unaffected = Door(7);
  ASSERT_TRUE(unaffected.ok()) << unaffected.status();
  EXPECT_EQ(unaffected->physical_map_id, 0xA0);
}

TEST_F(OverworldConnectionAdapterTest,
       ProjectMissingZeroAndAmbiguousIdsRefuseAcrossMaps) {
  EXPECT_TRUE(absl::IsNotFound(Door(99).status()));
  EXPECT_TRUE(absl::IsInvalidArgument(Door(0).status()));
  EXPECT_TRUE(absl::IsFailedPrecondition(
      ResolveOverworldConnection({}, ProjectEntranceReference{1}).status()));
  auto state = document_->authoring();
  state.maps[31].doors.push_back({1, 3, 4, 2});
  EXPECT_TRUE(absl::IsFailedPrecondition(
      ResolveOverworldConnection({{}, {}, {}, &state},
                                 ProjectEntranceReference{1})
          .status()));
  EXPECT_TRUE(absl::IsFailedPrecondition(
      CollectOverworldConnections({{}, {}, {}, &state}).status()));
  state = document_->authoring();
  state.maps[31].exits.push_back({1, 7, 1, 2, std::nullopt});
  EXPECT_TRUE(absl::IsFailedPrecondition(
      ResolveOverworldConnection({{}, {}, {}, &state},
                                 ProjectReturnReference{1})
          .status()));
}

TEST_F(OverworldConnectionAdapterTest,
       PreparingMoveEditsOnlyAValidatedValueCopy) {
  auto expected = Door();
  ASSERT_TRUE(expected.ok()) << expected.status();
  const auto before = document_->authoring();
  const auto serialized_before = document_->Serialize();
  auto plan = PrepareProjectConnectionEdit(
      *document_, *expected,
      EditAt(0xBF, 30, 29, DungeonEntranceDestination{0xFE}));
  ASSERT_TRUE(plan.ok()) << plan.status();
  EXPECT_EQ(plan->before, before);
  EXPECT_EQ(document_->authoring(), before);
  EXPECT_EQ(document_->Serialize(), serialized_before);
  EXPECT_FALSE(document_->dirty());
  EXPECT_EQ(plan->after.maps[0].doors.size(), 1);
  ASSERT_EQ(plan->after.maps[31].doors.size(), 1);
  EXPECT_EQ(plan->after.maps[31].doors[0],
            (core::LyraDoorPlacement{1, 30, 29, 0xFE}));
  EXPECT_EQ(plan->after.maps[0].exits, before.maps[0].exits);
  EXPECT_EQ(plan->after.maps[2], before.maps[2]);
  EXPECT_EQ(plan->after.flight_points, before.flight_points);
}

TEST_F(OverworldConnectionAdapterTest,
       BeforeAfterRestorationAndFreshParseRetainStableIdAndOtherData) {
  auto expected = Return();
  ASSERT_TRUE(expected.ok()) << expected.status();
  const auto before_maps = document_->maps();
  const auto before = document_->authoring();
  auto plan = PrepareProjectConnectionEdit(
      *document_, *expected,
      EditAt(0xBF, 29, 28, DungeonReturnDestination{0x127, 78}));
  ASSERT_TRUE(plan.ok()) << plan.status();
  ASSERT_TRUE(document_->SetAuthoring(plan->before, plan->after).ok());
  EXPECT_TRUE(document_->dirty());
  auto moved = Return();
  ASSERT_TRUE(moved.ok()) << moved.status();
  EXPECT_EQ(moved->physical_map_id, 0xBF);
  EXPECT_EQ(std::get<core::LyraExitPlacement>(moved->record),
            (core::LyraExitPlacement{1, 0x127, 29, 28, 78}));
  ASSERT_TRUE(document_->SetAuthoring(plan->after, plan->before).ok());
  EXPECT_EQ(document_->authoring(), before);
  EXPECT_FALSE(document_->dirty());
  ASSERT_TRUE(document_->SetAuthoring(plan->before, plan->after).ok());
  const auto serialized = document_->Serialize();
  auto fresh = core::LyraTerrainDocument::Parse(serialized, Profile());
  ASSERT_TRUE(fresh.ok()) << fresh.status();
  EXPECT_EQ(fresh->authoring(), plan->after);
  EXPECT_EQ(fresh->maps(), before_maps);
  EXPECT_EQ(fresh->authoring().maps[2], before.maps[2]);
  EXPECT_EQ(fresh->authoring().flight_points, before.flight_points);
  EXPECT_FALSE(fresh->dirty());
  auto reopened = ResolveOverworldConnection({{}, {}, {}, &fresh->authoring()},
                                             ProjectReturnReference{1});
  ASSERT_TRUE(reopened.ok()) << reopened.status();
  EXPECT_EQ(reopened->physical_map_id, 0xBF);
  EXPECT_EQ(std::get<ProjectReturnReference>(reopened->reference).id, 1u);
}

TEST_F(OverworldConnectionAdapterTest,
       StaleSameIdWithDifferentValueRefusesAfterDeleteAndReuse) {
  auto expected = Door();
  ASSERT_TRUE(expected.ok()) << expected.status();
  auto removed = document_->authoring();
  removed.maps[0].doors.erase(removed.maps[0].doors.begin());
  ASSERT_TRUE(document_->SetAuthoring(document_->authoring(), removed).ok());
  auto reused = removed;
  reused.maps[0].doors.push_back({1, 9, 10, 0x27});
  ASSERT_TRUE(document_->SetAuthoring(removed, reused).ok());
  auto plan = PrepareProjectConnectionEdit(
      *document_, *expected,
      EditAt(0xA0, 11, 12, DungeonEntranceDestination{1}));
  EXPECT_TRUE(absl::IsFailedPrecondition(plan.status()));
  EXPECT_EQ(document_->authoring(), reused);
}

TEST_F(OverworldConnectionAdapterTest, StaleMapMoveAndRemovedIdentityRefuse) {
  auto expected = Return();
  ASSERT_TRUE(expected.ok()) << expected.status();
  auto state = document_->authoring();
  state.maps[1].exits.push_back(state.maps[0].exits[0]);
  state.maps[0].exits.clear();
  ASSERT_TRUE(document_->SetAuthoring(document_->authoring(), state).ok());
  const auto edit = EditAt(0xA0, 3, 4, DungeonReturnDestination{1, 1});
  EXPECT_TRUE(absl::IsFailedPrecondition(
      PrepareProjectConnectionEdit(*document_, *expected, edit).status()));
  state.maps[1].exits.clear();
  ASSERT_TRUE(document_->SetAuthoring(document_->authoring(), state).ok());
  EXPECT_TRUE(absl::IsNotFound(
      PrepareProjectConnectionEdit(*document_, *expected, edit).status()));
}

TEST_F(OverworldConnectionAdapterTest,
       UnassignedEntranceAndReturnRowStayUnassignedWhenMoved) {
  auto expected_door = Door(7);
  auto expected_return = Return(9);
  ASSERT_TRUE(expected_door.ok()) << expected_door.status();
  ASSERT_TRUE(expected_return.ok()) << expected_return.status();
  EXPECT_TRUE(
      std::holds_alternative<std::monostate>(expected_door->destination));
  auto door_plan = PrepareProjectConnectionEdit(
      *document_, *expected_door, EditAt(0xA1, 0, 0, std::monostate{}));
  ASSERT_TRUE(door_plan.ok()) << door_plan.status();
  EXPECT_EQ(door_plan->after.maps[1].doors[0].entrance_id, std::nullopt);
  auto return_plan = PrepareProjectConnectionEdit(
      *document_, *expected_return,
      EditAt(0xBE, 1, 1, DungeonReturnDestination{0x127, std::nullopt}));
  ASSERT_TRUE(return_plan.ok()) << return_plan.status();
  EXPECT_EQ(return_plan->after.maps[30].exits[0].exit_row, std::nullopt);
  EXPECT_EQ(return_plan->after.maps[30].exits[0].room_id, 0x127);
}

TEST_F(OverworldConnectionAdapterTest,
       EntranceDestinationAcceptsFeAndRejectsFf) {
  auto expected = Door();
  ASSERT_TRUE(expected.ok()) << expected.status();
  const auto before = document_->authoring();
  for (const int id : {0, 0xFE}) {
    auto plan = PrepareProjectConnectionEdit(
        *document_, *expected,
        EditAt(0xA0, 1, 2, DungeonEntranceDestination{id}));
    ASSERT_TRUE(plan.ok()) << plan.status();
    EXPECT_EQ(plan->after.maps[0].doors[0].entrance_id, id);
  }
  for (const int id : {-1, 0xFF, 0x183}) {
    SCOPED_TRACE(id);
    EXPECT_TRUE(absl::IsInvalidArgument(
        PrepareProjectConnectionEdit(
            *document_, *expected,
            EditAt(0xA0, 1, 2, DungeonEntranceDestination{id}))
            .status()));
  }
  EXPECT_EQ(document_->authoring(), before);
}

TEST_F(OverworldConnectionAdapterTest,
       ReturnDestinationAccepts78AndRejects79OrSpecialIdAsRow) {
  auto expected = Return();
  ASSERT_TRUE(expected.ok()) << expected.status();
  const auto before = document_->authoring();
  for (const int row : {0, 78}) {
    EXPECT_TRUE(PrepareProjectConnectionEdit(
                    *document_, *expected,
                    EditAt(0xA0, 5, 6, DungeonReturnDestination{0x127, row}))
                    .ok());
  }
  for (const int row : {-1, 79, 0x183}) {
    SCOPED_TRACE(row);
    EXPECT_TRUE(absl::IsInvalidArgument(
        PrepareProjectConnectionEdit(
            *document_, *expected,
            EditAt(0xA0, 5, 6, DungeonReturnDestination{0x127, row}))
            .status()));
  }
  EXPECT_TRUE(absl::IsInvalidArgument(
      PrepareProjectConnectionEdit(
          *document_, *expected,
          EditAt(0xA0, 5, 6, DungeonReturnDestination{0x128, 1}))
          .status()));
  EXPECT_EQ(document_->authoring(), before);
}

TEST_F(OverworldConnectionAdapterTest,
       SpecialExit0183RefusesSourceOwnedSerializationForBothKinds) {
  const auto before = document_->authoring();
  for (const OverworldConnectionReference reference :
       {OverworldConnectionReference(ProjectEntranceReference{1}),
        OverworldConnectionReference(ProjectReturnReference{1})}) {
    auto expected = ResolveOverworldConnection(Records(), reference);
    ASSERT_TRUE(expected.ok()) << expected.status();
    auto plan = PrepareProjectConnectionEdit(
        *document_, *expected, EditAt(0xA0, 1, 2, WorldLinkDestination{0x183}));
    ASSERT_TRUE(absl::IsFailedPrecondition(plan.status()));
    EXPECT_NE(plan.status().message().find("source-owned"),
              std::string_view::npos);
  }
  EXPECT_EQ(document_->authoring(), before);
}

TEST_F(OverworldConnectionAdapterTest, CrossKindDestinationsAreNotCoerced) {
  auto door = Door();
  auto exit = Return();
  ASSERT_TRUE(door.ok()) << door.status();
  ASSERT_TRUE(exit.ok()) << exit.status();
  EXPECT_TRUE(absl::IsInvalidArgument(
      PrepareProjectConnectionEdit(
          *document_, *door, EditAt(0xA0, 1, 2, DungeonReturnDestination{1, 1}))
          .status()));
  EXPECT_TRUE(absl::IsInvalidArgument(
      PrepareProjectConnectionEdit(
          *document_, *exit, EditAt(0xA0, 5, 6, DungeonEntranceDestination{1}))
          .status()));
  EXPECT_TRUE(absl::IsInvalidArgument(
      PrepareProjectConnectionEdit(*document_, *exit,
                                   EditAt(0xA0, 5, 6, std::monostate{}))
          .status()));
}

TEST_F(OverworldConnectionAdapterTest,
       DestinationDoorCollisionRefusesWithoutChangingSourceOrDirtyState) {
  auto expected = Door();
  ASSERT_TRUE(expected.ok()) << expected.status();
  const auto before = document_->authoring();
  auto plan = PrepareProjectConnectionEdit(
      *document_, *expected, EditAt(0xA0, 3, 4, DungeonEntranceDestination{1}));
  ASSERT_TRUE(absl::IsInvalidArgument(plan.status()));
  EXPECT_NE(plan.status().message().find("Duplicate door position"),
            std::string_view::npos);
  EXPECT_EQ(document_->authoring(), before);
  EXPECT_FALSE(document_->dirty());
}

TEST_F(OverworldConnectionAdapterTest,
       ProjectPositionsMustBeAlignedAndBelongToDeclaredTailMap) {
  auto expected = Door();
  ASSERT_TRUE(expected.ok()) << expected.status();
  auto unaligned = EditAt(0xA0, 1, 2, DungeonEntranceDestination{1});
  ++unaligned.world_x;
  auto wrong_map = EditAt(0xA1, 1, 2, DungeonEntranceDestination{1});
  wrong_map.physical_map_id = 0xA0;
  auto outside = EditAt(0xBF, 31, 31, DungeonEntranceDestination{1});
  outside.world_y = 4096;
  for (const auto& edit : {unaligned, wrong_map, outside,
                           EditAt(0x9F, 1, 2, DungeonEntranceDestination{1}),
                           EditAt(0xC0, 1, 2, DungeonEntranceDestination{1})}) {
    EXPECT_TRUE(absl::IsInvalidArgument(
        PrepareProjectConnectionEdit(*document_, *expected, edit).status()));
  }
  EXPECT_FALSE(document_->dirty());
}

TEST_F(OverworldConnectionAdapterTest,
       CommitRejectsPlanAfterUnrelatedDocumentChanges) {
  auto expected = Door();
  ASSERT_TRUE(expected.ok()) << expected.status();
  auto plan = PrepareProjectConnectionEdit(
      *document_, *expected, EditAt(0xA1, 4, 5, DungeonEntranceDestination{2}));
  ASSERT_TRUE(plan.ok()) << plan.status();
  auto concurrent = document_->authoring();
  concurrent.maps[2].sprites[0].sprite_id = 0x43;
  ASSERT_TRUE(document_->SetAuthoring(document_->authoring(), concurrent).ok());
  EXPECT_TRUE(absl::IsFailedPrecondition(
      document_->SetAuthoring(plan->before, plan->after)));
  EXPECT_EQ(document_->authoring(), concurrent);
}

TEST_F(OverworldConnectionAdapterTest, NativeReferencesCannotUseProjectCommit) {
  std::vector<zelda3::OverworldEntrance> entrances{Entrance(0, 0, 0, 1)};
  auto expected = ResolveOverworldConnection(
      {entrances, {}, {}, nullptr},
      NativeEntranceReference{NativeEntranceTable::kEntrances, 0});
  ASSERT_TRUE(expected.ok()) << expected.status();
  EXPECT_TRUE(absl::IsInvalidArgument(
      PrepareProjectConnectionEdit(
          *document_, *expected,
          EditAt(0xA0, 1, 2, DungeonEntranceDestination{1}))
          .status()));
  EXPECT_FALSE(document_->dirty());
}

TEST_F(OverworldConnectionAdapterTest,
       ActiveTerrainStrokeRefusesPlanWithoutFinishingOrCancellingIt) {
  auto expected = Door();
  ASSERT_TRUE(expected.ok()) << expected.status();
  const auto before = document_->authoring();
  ASSERT_TRUE(document_->BeginStroke().ok());
  ASSERT_TRUE(document_->Paint(0, 0, 8).ok());
  auto plan = PrepareProjectConnectionEdit(
      *document_, *expected, EditAt(0xA1, 4, 5, DungeonEntranceDestination{2}));
  EXPECT_TRUE(absl::IsFailedPrecondition(plan.status()));
  EXPECT_TRUE(document_->stroke_active());
  EXPECT_EQ(document_->authoring(), before);
  EXPECT_EQ(document_->MapTile(0xA0, 0, 0).value(), 8);
  document_->CancelStroke();
}

}  // namespace
}  // namespace yaze::editor
