#include "gtest/gtest.h"

#include <array>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <vector>

#include "core/features.h"
#include "rom/rom.h"
#include "zelda3/dungeon/dungeon_object_editor.h"
#include "zelda3/dungeon/object_layer_semantics.h"
#include "zelda3/dungeon/room.h"

namespace yaze {
namespace zelda3 {

namespace {

class ScopedCustomObjectRoutingState {
 public:
  ScopedCustomObjectRoutingState()
      : previous_state_(CustomObjectManager::Get().SnapshotState()),
        previous_enabled_(core::FeatureFlags::get().kEnableCustomObjects) {
    const auto nonce =
        std::chrono::steady_clock::now().time_since_epoch().count();
    directory_ = std::filesystem::temp_directory_path() /
                 ("yaze_object_layer_semantics_" + std::to_string(nonce));
    std::filesystem::create_directories(directory_);
    CustomObjectManager::Get().Initialize(directory_.string());
    core::FeatureFlags::get().kEnableCustomObjects = true;
  }

  ~ScopedCustomObjectRoutingState() {
    core::FeatureFlags::get().kEnableCustomObjects = previous_enabled_;
    CustomObjectManager::Get().RestoreState(previous_state_);
    std::error_code error;
    std::filesystem::remove_all(directory_, error);
  }

  void WriteOneTileObject(const std::string& filename) const {
    const std::array<uint8_t, 6> bytes = {
        0x01, 0x00,  // one tile, no row jump
        0x40, 0x08,  // tile word 0x0840
        0x00, 0x00,  // terminator
    };
    std::ofstream output(directory_ / filename, std::ios::binary);
    output.write(reinterpret_cast<const char*>(bytes.data()), bytes.size());
  }

 private:
  CustomObjectManager::State previous_state_;
  bool previous_enabled_;
  std::filesystem::path directory_;
};

struct AuditedRoutingCase {
  int object_id;
  int routine_id;
  ObjectRenderRouting routing;
};

constexpr std::array<AuditedRoutingCase, 22> kAuditedRoutingCases = {{
    {0x130, DrawRoutineIds::kAutoStairs, ObjectRenderRouting::kFullBothBg1Bg2},
    {0x131, DrawRoutineIds::kAutoStairs, ObjectRenderRouting::kFullBothBg1Bg2},
    {0xF9B, DrawRoutineIds::kAutoStairs, ObjectRenderRouting::kFullBothBg1Bg2},
    {0xF9C, DrawRoutineIds::kAutoStairs, ObjectRenderRouting::kFullBothBg1Bg2},
    {0x132, DrawRoutineIds::kAutoStairs, ObjectRenderRouting::kStoredPlacement},
    {0x133, DrawRoutineIds::kAutoStairs, ObjectRenderRouting::kStoredPlacement},
    {0xF9D, DrawRoutineIds::kAutoStairs, ObjectRenderRouting::kStoredPlacement},
    {0xFB3, DrawRoutineIds::kAutoStairs, ObjectRenderRouting::kStoredPlacement},
    {0x138, DrawRoutineIds::kSpiralStairsGoingUpUpper,
     ObjectRenderRouting::kStoredPlacement},
    {0x139, DrawRoutineIds::kSpiralStairsGoingDownUpper,
     ObjectRenderRouting::kStoredPlacement},
    {0xF9E, DrawRoutineIds::kStraightInterRoomStairs,
     ObjectRenderRouting::kFixedBg1},
    {0xF9F, DrawRoutineIds::kStraightInterRoomStairs,
     ObjectRenderRouting::kFixedBg1},
    {0xFA0, DrawRoutineIds::kStraightInterRoomStairs,
     ObjectRenderRouting::kFixedBg1},
    {0xFA1, DrawRoutineIds::kStraightInterRoomStairs,
     ObjectRenderRouting::kFixedBg1},
    {0x13A, DrawRoutineIds::kSpiralStairsGoingUpLower,
     ObjectRenderRouting::kStoredPlacement},
    {0x13B, DrawRoutineIds::kSpiralStairsGoingDownLower,
     ObjectRenderRouting::kStoredPlacement},
    {0xFA6, DrawRoutineIds::kStraightInterRoomStairs,
     ObjectRenderRouting::kMixedBg1Bg2},
    {0xFA7, DrawRoutineIds::kStraightInterRoomStairs,
     ObjectRenderRouting::kMixedBg1Bg2},
    {0xFA8, DrawRoutineIds::kStraightInterRoomStairs,
     ObjectRenderRouting::kMixedBg1Bg2},
    {0xFA9, DrawRoutineIds::kStraightInterRoomStairs,
     ObjectRenderRouting::kMixedBg1Bg2},
    {0xFAD, DrawRoutineIds::kAgahnimsAltar, ObjectRenderRouting::kFixedBg1},
    {0xFD4, DrawRoutineIds::kFortuneTellerRoom, ObjectRenderRouting::kFixedBg1},
}};

EffectiveBgLayer ExpectedEffectiveLayer(ObjectRenderRouting routing,
                                        uint8_t stored_stream) {
  switch (routing) {
    case ObjectRenderRouting::kStoredPlacement:
      return stored_stream == 1 ? EffectiveBgLayer::kBg2
                                : EffectiveBgLayer::kBg1;
    case ObjectRenderRouting::kFixedBg1:
      return EffectiveBgLayer::kBg1;
    case ObjectRenderRouting::kFixedBg2:
      return EffectiveBgLayer::kBg2;
    case ObjectRenderRouting::kFullBothBg1Bg2:
    case ObjectRenderRouting::kMixedBg1Bg2:
      return EffectiveBgLayer::kBothBg1Bg2;
  }
  return EffectiveBgLayer::kBg1;
}

}  // namespace

TEST(ObjectLayerSemanticsTest, CeilingRoutineIsSingleLayer) {
  RoomObject obj(/*id=*/0x00, /*x=*/0, /*y=*/0, /*size=*/0, /*layer=*/0);

  const auto sem = GetObjectLayerSemantics(obj);
  EXPECT_FALSE(sem.draws_to_both_bgs);
  EXPECT_EQ(sem.effective_bg_layer, EffectiveBgLayer::kBg1);
  EXPECT_EQ(sem.render_routing, ObjectRenderRouting::kStoredPlacement);
}

TEST(ObjectLayerSemanticsTest, RoutineMetadataCanForceBothBgForType2Objects) {
  // Type 2 corners (0x108-0x10F) use RoomDraw_4x4Corner_BothBG in usdasm.
  // Our RoomObject doesn't set all_bgs_ for these IDs, so this must come from
  // DrawRoutineRegistry metadata.
  RoomObject obj(/*id=*/0x108, /*x=*/0, /*y=*/0, /*size=*/0, /*layer=*/0);

  EXPECT_FALSE(obj.all_bgs_);
  const auto sem = GetObjectLayerSemantics(obj);
  EXPECT_TRUE(sem.draws_to_both_bgs);
  EXPECT_EQ(sem.effective_bg_layer, EffectiveBgLayer::kBothBg1Bg2);
  EXPECT_EQ(sem.render_routing, ObjectRenderRouting::kFullBothBg1Bg2);
}

TEST(ObjectLayerSemanticsTest, AllBgsOverrideForcesBothBg) {
  RoomObject obj(/*id=*/0x21, /*x=*/0, /*y=*/0, /*size=*/0, /*layer=*/1);
  obj.all_bgs_ = true;

  EXPECT_TRUE(obj.all_bgs_);
  const auto sem = GetObjectLayerSemantics(obj);
  EXPECT_TRUE(sem.draws_to_both_bgs);
  EXPECT_EQ(sem.effective_bg_layer, EffectiveBgLayer::kBothBg1Bg2);
  EXPECT_EQ(sem.render_routing, ObjectRenderRouting::kFullBothBg1Bg2);
}

TEST(ObjectLayerSemanticsTest, NonBothBgUsesStoredLayer) {
  RoomObject obj(/*id=*/0x21, /*x=*/0, /*y=*/0, /*size=*/0, /*layer=*/1);

  const auto sem = GetObjectLayerSemantics(obj);
  EXPECT_FALSE(sem.draws_to_both_bgs);
  EXPECT_EQ(sem.effective_bg_layer, EffectiveBgLayer::kBg2);
  EXPECT_EQ(sem.render_routing, ObjectRenderRouting::kStoredPlacement);
}

TEST(ObjectLayerSemanticsTest,
     ActiveCustomOverridePreemptsBuiltInFixedLayerRouting) {
  ScopedCustomObjectRoutingState custom_state;
  custom_state.WriteOneTileObject("override.bin");
  CustomObjectManager::Get().SetObjectFileMap({{0xFAD, {"override.bin"}}});

  RoomObject object(/*id=*/0xFAD, /*x=*/0, /*y=*/0, /*size=*/0,
                    /*layer=*/1);
  const auto built_in = GetObjectLayerSemantics(object);
  ASSERT_EQ(built_in.render_routing, ObjectRenderRouting::kFixedBg1);
  ASSERT_EQ(built_in.effective_bg_layer, EffectiveBgLayer::kBg1);

  const auto effective = GetEffectiveObjectLayerSemantics(
      object, /*allow_track_corner_aliases=*/true);
  EXPECT_TRUE(effective.custom_override_active);
  EXPECT_EQ(effective.render_routing, ObjectRenderRouting::kStoredPlacement);
  EXPECT_EQ(effective.effective_bg_layer, EffectiveBgLayer::kBg2);

  object.all_bgs_ = true;
  const auto both = GetEffectiveObjectLayerSemantics(
      object, /*allow_track_corner_aliases=*/true);
  EXPECT_TRUE(both.custom_override_active);
  EXPECT_EQ(both.render_routing, ObjectRenderRouting::kFullBothBg1Bg2);
  EXPECT_EQ(both.effective_bg_layer, EffectiveBgLayer::kBothBg1Bg2);
}

TEST(ObjectLayerSemanticsTest,
     TrackCornerCustomOverrideHonorsRoomAliasPermission) {
  ScopedCustomObjectRoutingState custom_state;
  custom_state.WriteOneTileObject("track_corner_tl.bin");
  CustomObjectManager::Get().SetObjectFileMap(
      {{0x31,
        {"unused_lr.bin", "unused_ud.bin", "track_corner_tl.bin",
         "unused_tr.bin", "unused_bl.bin", "unused_br.bin"}}});

  RoomObject corner(/*id=*/0x100, /*x=*/0, /*y=*/0, /*size=*/0,
                    /*layer=*/1);
  EXPECT_FALSE(HasActiveCustomObjectOverride(
      corner, /*allow_track_corner_aliases=*/false));
  EXPECT_TRUE(HasActiveCustomObjectOverride(
      corner, /*allow_track_corner_aliases=*/true));

  const auto built_in =
      GetEffectiveObjectLayerSemantics(corner,
                                       /*allow_track_corner_aliases=*/false);
  EXPECT_FALSE(built_in.custom_override_active);
  EXPECT_EQ(built_in.effective_bg_layer, EffectiveBgLayer::kBg2);
  const auto custom =
      GetEffectiveObjectLayerSemantics(corner,
                                       /*allow_track_corner_aliases=*/true);
  EXPECT_TRUE(custom.custom_override_active);
  EXPECT_EQ(custom.effective_bg_layer, EffectiveBgLayer::kBg2);
  EXPECT_EQ(custom.render_routing, ObjectRenderRouting::kStoredPlacement);
}

TEST(ObjectLayerSemanticsTest,
     AuditedObjectIdsReportRoutingAcrossAllStoredStreams) {
  for (const auto& test_case : kAuditedRoutingCases) {
    for (uint8_t stored_stream : {uint8_t{0}, uint8_t{1}, uint8_t{2}}) {
      SCOPED_TRACE(::testing::Message()
                   << "object=0x" << std::hex << test_case.object_id
                   << " stored_stream=" << std::dec
                   << static_cast<int>(stored_stream));
      RoomObject obj(test_case.object_id, /*x=*/0, /*y=*/0, /*size=*/0,
                     stored_stream);

      const auto sem = GetObjectLayerSemantics(obj);
      EXPECT_EQ(sem.routine_id, test_case.routine_id);
      EXPECT_EQ(sem.render_routing, test_case.routing);
      EXPECT_EQ(sem.effective_bg_layer,
                ExpectedEffectiveLayer(test_case.routing, stored_stream));
      // These object-aware routes do not rewrite the legacy all_bgs_/routine
      // metadata compatibility field.
      EXPECT_FALSE(sem.draws_to_both_bgs);
    }
  }
}

TEST(ObjectLayerSemanticsTest,
     AllBgsOverrideTakesPriorityOverFixedAndMixedRouting) {
  for (int object_id : {0xF9E, 0xFA6}) {
    RoomObject obj(object_id, /*x=*/0, /*y=*/0, /*size=*/0, /*layer=*/1);
    obj.all_bgs_ = true;

    const auto sem = GetObjectLayerSemantics(obj);
    EXPECT_TRUE(sem.draws_to_both_bgs);
    EXPECT_EQ(sem.render_routing, ObjectRenderRouting::kFullBothBg1Bg2);
    EXPECT_EQ(sem.effective_bg_layer, EffectiveBgLayer::kBothBg1Bg2);
  }
}

TEST(ObjectLayerSemanticsTest, RoutingLabelsAndTokensAreStable) {
  struct LabelCase {
    ObjectRenderRouting routing;
    const char* label;
    const char* token;
  };
  constexpr std::array<LabelCase, 5> kCases = {{
      {ObjectRenderRouting::kStoredPlacement, "Stored placement", "stored"},
      {ObjectRenderRouting::kFixedBg1, "BG1 (fixed)", "fixed_bg1"},
      {ObjectRenderRouting::kFixedBg2, "BG2 (fixed)", "fixed_bg2"},
      {ObjectRenderRouting::kFullBothBg1Bg2, "Both BGs (full)", "full_both"},
      {ObjectRenderRouting::kMixedBg1Bg2, "Mixed BG1/BG2", "mixed"},
  }};

  for (const auto& test_case : kCases) {
    EXPECT_STREQ(ObjectRenderRoutingLabel(test_case.routing), test_case.label);
    EXPECT_STREQ(ObjectRenderRoutingToken(test_case.routing), test_case.token);
  }

  ObjectLayerSemantics stored_bg1;
  stored_bg1.effective_bg_layer = EffectiveBgLayer::kBg1;
  EXPECT_STREQ(ObjectRenderRoutingDisplayLabel(stored_bg1),
               "BG1 (stored placement)");
  stored_bg1.effective_bg_layer = EffectiveBgLayer::kBg2;
  EXPECT_STREQ(ObjectRenderRoutingDisplayLabel(stored_bg1),
               "BG2 (stored placement)");
}

TEST(DungeonObjectLayerGuardrailsTest,
     SingleAndBatchStreamChangesIncludeBothBgObjects) {
  Rom rom;
  Room room(/*room_id=*/0, &rom);

  ASSERT_TRUE(room.AddObject(RoomObject(0x03, 0, 0, 0, 0)).ok());
  ASSERT_TRUE(room.AddObject(RoomObject(0x21, 0, 0, 0, 0)).ok());

  DungeonObjectEditor editor(&rom);
  editor.SetExternalRoom(&room);

  std::vector<size_t> indices = {0, 1};
  ASSERT_TRUE(editor.BatchChangeObjectLayer(indices, /*new_layer=*/1).ok());

  // BothBG controls buffer fan-out, not the object's ROM stream or draw order.
  EXPECT_EQ(room.GetTileObject(0).layer_, RoomObject::LayerType::BG2);
  EXPECT_EQ(room.GetTileObject(1).layer_, RoomObject::LayerType::BG2);

  ASSERT_TRUE(editor.ChangeObjectLayer(0, /*new_layer=*/0).ok());
  EXPECT_EQ(room.GetTileObject(0).layer_, RoomObject::LayerType::BG1);
  ASSERT_TRUE(editor.ChangeObjectLayer(0, /*new_layer=*/1).ok());
  ASSERT_EQ(room.GetTileObjectCount(), 2);
  EXPECT_EQ(room.GetTileObject(0).id_, 0x21);
  EXPECT_EQ(room.GetTileObject(1).id_, 0x03);
  EXPECT_EQ(room.GetTileObject(1).layer_, RoomObject::LayerType::BG2);

  const std::vector<uint8_t> expected = {
      0xFF, 0xFF,             // Empty primary stream.
      0x00, 0x00, 0x21,       // Ordinary object in BG2 overlay stream.
      0x00, 0x00, 0x03,       // BothBG object appended to that stream.
      0xFF, 0xFF,             // End BG2 overlay stream.
      0xF0, 0xFF, 0xFF, 0xFF  // Empty BG1 overlay and door list.
  };
  EXPECT_EQ(room.EncodeObjects(), expected);
}

TEST(DungeonObjectLayerGuardrailsTest,
     FixedAndMixedRoutesKeepStoredStreamIdentityAndSelection) {
  Rom rom;
  Room room(/*room_id=*/0, &rom);
  ASSERT_TRUE(room.AddObject(RoomObject(0xF9E, 0, 0, 0, 0)).ok());
  ASSERT_TRUE(room.AddObject(RoomObject(0x21, 0, 0, 0, 0)).ok());
  ASSERT_TRUE(room.AddObject(RoomObject(0x22, 0, 0, 0, 1)).ok());
  ASSERT_TRUE(room.AddObject(RoomObject(0xFA6, 0, 0, 0, 0)).ok());

  DungeonObjectEditor editor(&rom);
  editor.SetExternalRoom(&room);
  ASSERT_TRUE(editor.AddToSelection(0).ok());
  ASSERT_TRUE(editor.AddToSelection(3).ok());
  ASSERT_TRUE(editor.BatchChangeObjectLayer({0, 3}, /*new_layer=*/1).ok());

  ASSERT_EQ(room.GetTileObjectCount(), 4);
  EXPECT_EQ(room.GetTileObject(0).id_, 0x21);
  EXPECT_EQ(room.GetTileObject(1).id_, 0x22);
  EXPECT_EQ(room.GetTileObject(2).id_, 0xF9E);
  EXPECT_EQ(room.GetTileObject(3).id_, 0xFA6);
  EXPECT_EQ(room.GetTileObject(2).GetLayerValue(), 1);
  EXPECT_EQ(room.GetTileObject(3).GetLayerValue(), 1);
  EXPECT_EQ(editor.GetSelection().selected_objects,
            (std::vector<size_t>{2, 3}));

  EXPECT_EQ(GetObjectLayerSemantics(room.GetTileObject(2)).render_routing,
            ObjectRenderRouting::kFixedBg1);
  EXPECT_EQ(GetObjectLayerSemantics(room.GetTileObject(3)).render_routing,
            ObjectRenderRouting::kMixedBg1Bg2);

  const std::vector<uint8_t> expected = {
      0x00, 0x00, 0x21, 0xFF, 0xFF, 0x00, 0x00, 0x22, 0x02, 0x03,
      0xF9, 0x02, 0x01, 0xFA, 0xFF, 0xFF, 0xF0, 0xFF, 0xFF, 0xFF,
  };
  EXPECT_EQ(room.EncodeObjects(), expected);
}

TEST(DungeonObjectLayerGuardrailsTest,
     SingleStreamChangeAppendsAfterExistingTargetAndRemapsSelection) {
  Rom rom;
  Room room(/*room_id=*/0, &rom);
  ASSERT_TRUE(room.AddObject(RoomObject(0x03, 0, 0, 0, 0)).ok());
  ASSERT_TRUE(room.AddObject(RoomObject(0x21, 0, 0, 0, 0)).ok());
  ASSERT_TRUE(room.AddObject(RoomObject(0x22, 0, 0, 0, 1)).ok());
  ASSERT_TRUE(room.AddObject(RoomObject(0x23, 0, 0, 0, 1)).ok());

  DungeonObjectEditor editor(&rom);
  editor.SetExternalRoom(&room);
  ASSERT_TRUE(editor.AddToSelection(0).ok());

  size_t callback_index = 0;
  editor.SetObjectChangedCallback(
      [&](size_t index, const RoomObject&) { callback_index = index; });
  ASSERT_TRUE(editor.ChangeObjectLayer(0, /*new_layer=*/1).ok());

  ASSERT_EQ(room.GetTileObjectCount(), 4);
  EXPECT_EQ(room.GetTileObject(0).id_, 0x21);
  EXPECT_EQ(room.GetTileObject(1).id_, 0x22);
  EXPECT_EQ(room.GetTileObject(2).id_, 0x23);
  EXPECT_EQ(room.GetTileObject(3).id_, 0x03);
  ASSERT_EQ(editor.GetSelection().selected_objects.size(), 1);
  EXPECT_EQ(editor.GetSelection().selected_objects[0], 3);
  EXPECT_EQ(callback_index, 3);

  const std::vector<uint8_t> expected = {
      0x00, 0x00, 0x21, 0xFF, 0xFF, 0x00, 0x00, 0x22, 0x00, 0x00,
      0x23, 0x00, 0x00, 0x03, 0xFF, 0xFF, 0xF0, 0xFF, 0xFF, 0xFF,
  };
  EXPECT_EQ(room.EncodeObjects(), expected);
}

TEST(DungeonObjectLayerGuardrailsTest,
     SpecialOnlyLayerChangePreservesRoomStreamOrder) {
  Rom rom;
  Room room(/*room_id=*/0, &rom);
  ASSERT_TRUE(room.AddObject(RoomObject(0x22, 0, 0, 0, 1)).ok());
  ASSERT_TRUE(room.AddObject(RoomObject(0x21, 0, 0, 0, 0)).ok());
  RoomObject torch(0x150, 1, 1, 0, 0);
  torch.set_options(ObjectOption::Torch);
  ASSERT_TRUE(room.AddObject(torch).ok());

  DungeonObjectEditor editor(&rom);
  editor.SetExternalRoom(&room);
  ASSERT_TRUE(editor.ChangeObjectLayer(2, /*new_layer=*/1).ok());

  ASSERT_EQ(room.GetTileObjectCount(), 3);
  EXPECT_EQ(room.GetTileObject(0).id_, 0x22);
  EXPECT_EQ(room.GetTileObject(1).id_, 0x21);
  EXPECT_EQ(room.GetTileObject(2).id_, 0x150);
  EXPECT_EQ(room.GetTileObject(2).GetLayerValue(), 1);
}

TEST(DungeonObjectLayerGuardrailsTest,
     MixedBatchRejectsUnsupportedSpecialTargetAtomically) {
  Rom rom;
  Room room(/*room_id=*/0, &rom);
  RoomObject torch(0x150, 1, 1, 0, 0);
  torch.set_options(ObjectOption::Torch);
  ASSERT_TRUE(room.AddObject(torch).ok());
  ASSERT_TRUE(room.AddObject(RoomObject(0x21, 0, 0, 0, 0)).ok());
  room.ClearSaveDirtyState();

  DungeonObjectEditor editor(&rom);
  editor.SetExternalRoom(&room);
  const auto status = editor.BatchChangeObjectLayer({0, 1}, /*new_layer=*/2);

  EXPECT_FALSE(status.ok());
  EXPECT_EQ(room.GetTileObject(0).GetLayerValue(), 0);
  EXPECT_EQ(room.GetTileObject(1).GetLayerValue(), 0);
  EXPECT_FALSE(room.torches_dirty());
  EXPECT_FALSE(room.object_stream_dirty());
}

}  // namespace zelda3
}  // namespace yaze
