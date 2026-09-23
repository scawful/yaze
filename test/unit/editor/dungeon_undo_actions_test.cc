#include "app/editor/dungeon/dungeon_undo_actions.h"

#include <algorithm>

#include <gtest/gtest.h>

#include "app/editor/dungeon/dungeon_canvas_viewer.h"
#include "app/editor/dungeon/dungeon_editor_v2.h"
#include "app/gfx/resource/arena.h"
#include "core/features.h"
#include "imgui/imgui.h"
#include "rom/snes.h"
#include "zelda3/dungeon/dungeon_rom_addresses.h"

namespace yaze::editor {

class DungeonEditorEntityUndoTestPeer {
 public:
  static DungeonCanvasViewer* Viewer(DungeonEditorV2& editor, int room_id) {
    editor.current_room_id_ = room_id;
    return editor.GetViewerForRoom(room_id);
  }
  static absl::Status Restore(DungeonEditorV2& editor, int room_id,
                              MutationDomain domain,
                              const DungeonEntitySnapshot& snapshot) {
    return editor.RestoreRoomEntities(room_id, domain, snapshot);
  }
  static void NavigateToObject(DungeonEditorV2& editor, int room_id,
                               size_t index, int object_id) {
    editor.NavigateToPlacedObject(room_id, index, object_id);
  }
};

namespace {

TEST(DungeonUndoActionsTest, CustomCollisionUndoRedoRestoresMap) {
  zelda3::CustomCollisionMap before;
  before.has_data = true;
  before.tiles[0] = 0x08;

  zelda3::CustomCollisionMap after = before;
  after.tiles[0] = 0x1B;

  zelda3::CustomCollisionMap restored;
  int restored_room_id = -1;

  DungeonCustomCollisionAction action(
      /*room_id=*/0x25, before, after,
      [&](int room_id, const zelda3::CustomCollisionMap& map) {
        restored_room_id = room_id;
        restored = map;
      });

  ASSERT_TRUE(action.Undo().ok());
  EXPECT_EQ(restored_room_id, 0x25);
  EXPECT_TRUE(restored.has_data);
  EXPECT_EQ(restored.tiles[0], 0x08);

  ASSERT_TRUE(action.Redo().ok());
  EXPECT_EQ(restored_room_id, 0x25);
  EXPECT_TRUE(restored.has_data);
  EXPECT_EQ(restored.tiles[0], 0x1B);
}

TEST(DungeonUndoActionsTest, CustomCollisionBatchUndoRedoRestoresEveryRoom) {
  zelda3::CustomCollisionMap empty;
  zelda3::CustomCollisionMap room_25;
  room_25.has_data = true;
  room_25.tiles[10] = 0xB0;
  zelda3::CustomCollisionMap room_26;
  room_26.has_data = true;
  room_26.tiles[20] = 0xB7;

  const std::vector<DungeonCustomCollisionSnapshot> before = {{0x25, empty},
                                                              {0x26, empty}};
  const std::vector<DungeonCustomCollisionSnapshot> after = {{0x25, room_25},
                                                             {0x26, room_26}};
  std::vector<DungeonCustomCollisionSnapshot> restored;

  DungeonCustomCollisionBatchAction action(
      before, after,
      [&](const std::vector<DungeonCustomCollisionSnapshot>& snapshots) {
        restored = snapshots;
        return absl::OkStatus();
      });

  EXPECT_EQ(action.Description(), "Generate minecart collision for 2 rooms");
  ASSERT_TRUE(action.Undo().ok());
  ASSERT_EQ(restored.size(), 2u);
  EXPECT_EQ(restored[0].room_id, 0x25);
  EXPECT_FALSE(restored[0].map.has_data);
  EXPECT_EQ(restored[1].room_id, 0x26);
  EXPECT_FALSE(restored[1].map.has_data);

  ASSERT_TRUE(action.Redo().ok());
  ASSERT_EQ(restored.size(), 2u);
  EXPECT_EQ(restored[0].map.tiles[10], 0xB0);
  EXPECT_EQ(restored[1].map.tiles[20], 0xB7);
}

TEST(DungeonUndoActionsTest, CustomCollisionBatchRequiresRestoreCallback) {
  DungeonCustomCollisionBatchAction action({}, {}, {});
  EXPECT_TRUE(absl::IsInternal(action.Undo()));
  EXPECT_TRUE(absl::IsInternal(action.Redo()));
}

TEST(DungeonUndoActionsTest, WaterFillUndoRedoRestoresSnapshot) {
  WaterFillSnapshot before;
  before.sram_bit_mask = 0x02;
  before.offsets = {0u, 64u, 65u};

  WaterFillSnapshot after = before;
  after.offsets.push_back(66u);

  WaterFillSnapshot restored;
  int restored_room_id = -1;

  DungeonWaterFillAction action(
      /*room_id=*/0x27, before, after,
      [&](int room_id, const WaterFillSnapshot& snap) {
        restored_room_id = room_id;
        restored = snap;
      });

  ASSERT_TRUE(action.Undo().ok());
  EXPECT_EQ(restored_room_id, 0x27);
  EXPECT_EQ(restored.sram_bit_mask, 0x02);
  EXPECT_EQ(restored.offsets, before.offsets);

  ASSERT_TRUE(action.Redo().ok());
  EXPECT_EQ(restored_room_id, 0x27);
  EXPECT_EQ(restored.sram_bit_mask, 0x02);
  EXPECT_EQ(restored.offsets, after.offsets);
}

class DungeonEntityUndoLifecycleTest : public ::testing::TestWithParam<bool> {
 protected:
  void SetUp() override {
    previous_context_ = ImGui::GetCurrentContext();
    context_ = ImGui::CreateContext();
    previous_workbench_ = core::FeatureFlags::get().dungeon.kUseWorkbench;
    core::FeatureFlags::get().dungeon.kUseWorkbench = GetParam();
    ASSERT_TRUE(rom_.LoadFromData(std::vector<uint8_t>(0x200000, 0)).ok());
    const int layout = SnesToPc(zelda3::kRoomLayoutPointers.front());
    ASSERT_TRUE(rom_.WriteWord(layout, 0xFFFF).ok());
    editor_ = std::make_unique<DungeonEditorV2>(&rom_);
    room_ = &editor_->rooms()[0];
    room_->SetLoaded(true);
    room_->SetTileObjects({});
    viewer_ = DungeonEditorEntityUndoTestPeer::Viewer(*editor_, 0);
    viewer_->RefreshRomBackedState(&rom_, nullptr, &editor_->rooms(), 0);
    room_->ClearSaveDirtyState();
  }
  void TearDown() override {
    editor_.reset();
    gfx::Arena::Get().ClearTextureQueue();
    core::FeatureFlags::get().dungeon.kUseWorkbench = previous_workbench_;
    ImGui::DestroyContext(context_);
    ImGui::SetCurrentContext(previous_context_);
  }
  InteractionCoordinator& coordinator() {
    return viewer_->object_interaction().entity_coordinator();
  }
  size_t UndoDepth() const { return editor_->undo_manager().UndoStackSize(); }

  Rom rom_;
  zelda3::GameData game_data_;
  std::unique_ptr<DungeonEditorV2> editor_;
  zelda3::Room* room_ = nullptr;
  DungeonCanvasViewer* viewer_ = nullptr;
  ImGuiContext* previous_context_ = nullptr;
  ImGuiContext* context_ = nullptr;
  bool previous_workbench_ = false;
};

TEST_P(DungeonEntityUndoLifecycleTest,
       SpritePropertiesUndoRedoRestoreIdentityAndDirtyState) {
  room_->GetSprites().emplace_back(0x09, 4, 6, 0, 0);
  coordinator().SelectEntity(EntityType::Sprite, 0);
  ASSERT_TRUE(
      coordinator().sprite_handler().UpdateSprite(0, 0x01, 8, 9, 7, 1, 2));
  ASSERT_EQ(UndoDepth(), 1u);
  EXPECT_TRUE(room_->GetSprites()[0].IsOverlord());
  EXPECT_TRUE(room_->sprites_dirty());
  // Saving between editing and undo must not make the restored room clean.
  room_->ClearSpritesDirty();
  ASSERT_TRUE(editor_->Undo().ok());
  EXPECT_EQ(room_->GetSprites()[0].id(), 0x09);
  EXPECT_EQ(room_->GetSprites()[0].x(), 4);
  EXPECT_EQ(room_->GetSprites()[0].subtype(), 0);
  EXPECT_FALSE(room_->GetSprites()[0].IsOverlord());
  EXPECT_EQ(room_->GetSprites()[0].key_drop(), 0);
  EXPECT_TRUE(room_->sprites_dirty());
  EXPECT_EQ(coordinator().GetSelectedEntity(),
            (SelectedEntity{EntityType::Sprite, 0}));
  ASSERT_TRUE(editor_->Redo().ok());
  EXPECT_EQ(room_->GetSprites()[0].x(), 8);
  EXPECT_EQ(room_->GetSprites()[0].y(), 9);
  EXPECT_EQ(room_->GetSprites()[0].layer(), 1);
  EXPECT_EQ(room_->GetSprites()[0].key_drop(), 2);
  EXPECT_TRUE(room_->GetSprites()[0].IsOverlord());
}

TEST_P(DungeonEntityUndoLifecycleTest, DoorAndItemEditsRestoreOnlyTheirDomain) {
  const auto original = zelda3::Room::Door::FromRomBytes(0x04, 0x00);
  room_->GetDoors().push_back(original);
  room_->GetPotItems().push_back({0x0408, 1});
  coordinator().SelectEntity(EntityType::Door, 0);
  ASSERT_TRUE(coordinator().door_handler().UpdateDoor(
      0, zelda3::DoorType::SmallKeyDoor, zelda3::DoorDirection::South, 1));
  const auto encoded = room_->GetDoors()[0].EncodeBytes();
  EXPECT_EQ(room_->GetDoors()[0].byte1, encoded.first);
  EXPECT_EQ(room_->GetDoors()[0].byte2, encoded.second);
  coordinator().SelectEntity(EntityType::Item, 0);
  ASSERT_TRUE(coordinator().item_handler().UpdateItem(0, 7, 48, 80));
  ASSERT_EQ(UndoDepth(), 2u);
  ASSERT_TRUE(editor_->Undo().ok());
  EXPECT_EQ(room_->GetPotItems()[0].position, 0x0408);
  EXPECT_EQ(room_->GetPotItems()[0].item, 1);
  EXPECT_EQ(room_->GetDoors()[0].type, zelda3::DoorType::SmallKeyDoor);
  EXPECT_TRUE(room_->pot_items_dirty());
  ASSERT_TRUE(editor_->Undo().ok());
  EXPECT_EQ(room_->GetDoors()[0].byte1, original.byte1);
  EXPECT_EQ(room_->GetDoors()[0].byte2, original.byte2);
  EXPECT_EQ(coordinator().GetSelectedEntity(),
            (SelectedEntity{EntityType::Door, 0}));
  EXPECT_TRUE(room_->object_stream_dirty());
  ASSERT_TRUE(editor_->Redo().ok());
  ASSERT_TRUE(editor_->Redo().ok());
  EXPECT_EQ(room_->GetPotItems()[0].GetPixelX(), 48);
  EXPECT_EQ(room_->GetPotItems()[0].GetPixelY(), 80);
}

TEST_P(DungeonEntityUndoLifecycleTest,
       FlaggedPotTypeEditUndoRetainsRawPosition) {
  room_->GetPotItems().push_back({0x2660, 1});
  coordinator().SelectEntity(EntityType::Item, 0);
  ASSERT_TRUE(coordinator().item_handler().MutateItemType(0, 6));
  ASSERT_EQ(UndoDepth(), 1u);
  EXPECT_EQ(room_->GetPotItems()[0].position, 0x2660);
  EXPECT_EQ(room_->GetPotItems()[0].item, 6);
  room_->ClearPotItemsDirty();
  ASSERT_TRUE(editor_->Undo().ok());
  EXPECT_EQ(room_->GetPotItems()[0].position, 0x2660);
  EXPECT_EQ(room_->GetPotItems()[0].item, 1);
  EXPECT_TRUE(room_->pot_items_dirty());
  ASSERT_TRUE(editor_->Redo().ok());
  EXPECT_EQ(room_->GetPotItems()[0].position, 0x2660);
  EXPECT_EQ(room_->GetPotItems()[0].item, 6);
}

TEST_P(DungeonEntityUndoLifecycleTest,
       SpritePlacementDragAndDeleteHaveSeparateUndoActions) {
  auto& handler = coordinator().sprite_handler();
  handler.SetSpriteId(0x09);
  handler.BeginPlacement();
  ASSERT_TRUE(handler.HandleClick(64, 96));
  handler.CancelPlacement();
  ASSERT_EQ(UndoDepth(), 1u);
  ASSERT_EQ(room_->GetSprites().size(), 1u);
  ASSERT_TRUE(coordinator().HandleClick(65, 97));
  coordinator().HandleRelease();
  EXPECT_EQ(UndoDepth(), 1u);  // Plain selection must not dirty undo history.
  ASSERT_TRUE(coordinator().HandleClick(65, 97));
  coordinator().HandleDrag(ImVec2(96, 128), ImVec2(31, 31));
  coordinator().HandleDrag(ImVec2(112, 144), ImVec2(16, 16));
  EXPECT_EQ(UndoDepth(), 1u);
  coordinator().HandleRelease();
  EXPECT_EQ(UndoDepth(), 2u);
  EXPECT_EQ(room_->GetSprites()[0].x(), 7);
  coordinator().DeleteSelectedEntity();
  EXPECT_EQ(UndoDepth(), 3u);
  EXPECT_TRUE(room_->GetSprites().empty());
  EXPECT_FALSE(coordinator().HasEntitySelection());
  ASSERT_TRUE(editor_->Undo().ok());
  EXPECT_EQ(room_->GetSprites()[0].x(), 7);
  EXPECT_EQ(coordinator().GetSelectedEntity(),
            (SelectedEntity{EntityType::Sprite, 0}));
  ASSERT_TRUE(editor_->Undo().ok());
  EXPECT_EQ(room_->GetSprites()[0].x(), 4);
  ASSERT_TRUE(editor_->Undo().ok());
  EXPECT_TRUE(room_->GetSprites().empty());
  ASSERT_TRUE(editor_->Redo().ok());
  ASSERT_TRUE(editor_->Redo().ok());
  ASSERT_TRUE(editor_->Redo().ok());
  EXPECT_TRUE(room_->GetSprites().empty());
  EXPECT_FALSE(coordinator().HasEntitySelection());
}

TEST_P(DungeonEntityUndoLifecycleTest,
       GroupDragWithinOneDomainRecordsOneAction) {
  room_->GetSprites().emplace_back(0x09, 4, 6, 0, 0);
  room_->GetSprites().emplace_back(0x0A, 8, 10, 0, 1);
  coordinator().SetSelectedEntities(
      {{EntityType::Sprite, 0}, {EntityType::Sprite, 1}});
  coordinator().BeginSelectionDrag(ImVec2(64, 96));
  coordinator().HandleDrag(ImVec2(80, 112), ImVec2(16, 16));
  coordinator().HandleDrag(ImVec2(96, 128), ImVec2(16, 16));
  EXPECT_EQ(UndoDepth(), 0u);
  coordinator().HandleRelease();
  EXPECT_EQ(UndoDepth(), 1u);
  EXPECT_EQ(room_->GetSprites()[0].x(), 6);
  EXPECT_EQ(room_->GetSprites()[1].x(), 10);
  ASSERT_TRUE(editor_->Undo().ok());
  EXPECT_EQ(room_->GetSprites()[0].x(), 4);
  EXPECT_EQ(room_->GetSprites()[1].x(), 8);
  EXPECT_EQ(coordinator().GetSelectedEntities().size(), 2u);
}

TEST_P(DungeonEntityUndoLifecycleTest,
       InvalidAndUnchangedEditsDoNotCreateHistory) {
  room_->GetDoors().push_back(zelda3::Room::Door::FromRomBytes(0x00, 0x00));
  room_->GetSprites().emplace_back(0x09, 4, 6, 0, 0);
  room_->GetPotItems().push_back({0x0408, 1});
  EXPECT_FALSE(
      coordinator().sprite_handler().UpdateSprite(0, 9, 4, 6, 0, 0, 0));
  EXPECT_FALSE(
      coordinator().sprite_handler().UpdateSprite(0, 9, 4, 6, 0, 2, 0));
  EXPECT_FALSE(
      coordinator().sprite_handler().UpdateSprite(0, 9, 4, 6, 32, 0, 0));
  EXPECT_FALSE(
      coordinator().sprite_handler().UpdateSprite(3, 9, 4, 6, 0, 0, 0));
  EXPECT_FALSE(coordinator().item_handler().UpdateItem(0, 1, 32, 64));
  EXPECT_FALSE(coordinator().item_handler().UpdateItem(0, 2, 33, 64));
  EXPECT_FALSE(coordinator().door_handler().UpdateDoor(
      0, zelda3::DoorType::NormalDoor, zelda3::DoorDirection::North, 0));
  EXPECT_FALSE(coordinator().door_handler().UpdateDoor(
      0, zelda3::DoorType::SmallKeyDoor, zelda3::DoorDirection::North, 255));
  auto* ctx = coordinator().sprite_handler().context();
  ctx->NotifyMutation(MutationDomain::kSprites);
  ctx->NotifyInvalidateCache(MutationDomain::kSprites);
  EXPECT_EQ(UndoDepth(), 0u);
  EXPECT_FALSE(room_->sprites_dirty());
  EXPECT_FALSE(room_->pot_items_dirty());
  EXPECT_FALSE(room_->object_stream_dirty());
}

TEST_P(DungeonEntityUndoLifecycleTest,
       UndoTargetsOriginalRoomAfterViewerSwitch) {
  room_->GetSprites().emplace_back(0x09, 4, 6, 0, 0);
  coordinator().SelectEntity(EntityType::Sprite, 0);
  ASSERT_TRUE(coordinator().sprite_handler().UpdateSprite(0, 9, 8, 6, 0, 0, 0));
  auto& other = editor_->rooms()[1];
  other.SetLoaded(true);
  other.GetSprites().emplace_back(0x0A, 12, 14, 0, 1);
  viewer_->RefreshRomBackedState(&rom_, nullptr, &editor_->rooms(), 1);
  coordinator().SelectEntity(EntityType::Sprite, 0);
  ASSERT_TRUE(editor_->Undo().ok());
  EXPECT_EQ(room_->GetSprites()[0].x(), 4);
  EXPECT_EQ(other.GetSprites()[0].x(), 12);
  EXPECT_EQ(coordinator().GetSelectedEntity(),
            (SelectedEntity{EntityType::Sprite, 0}));
  EXPECT_EQ(viewer_->current_room_id(), 1);
  EXPECT_TRUE(DungeonEditorEntityUndoTestPeer::Restore(
                  *editor_, -1, MutationDomain::kSprites, {})
                  .code() == absl::StatusCode::kInvalidArgument);
}

TEST_P(DungeonEntityUndoLifecycleTest,
       RoomChangeFinalizesTileDragBeforeNextEntityEdit) {
  room_->AddTileObject(zelda3::RoomObject{0x01, 4, 4, 0x00, 0});
  auto& interaction = viewer_->object_interaction();
  interaction.SetSelectedObjects({0});
  interaction.mode_manager().SetMode(InteractionMode::DraggingObjects);
  coordinator().tile_handler().InitDrag(ImVec2(32, 32));
  coordinator().tile_handler().HandleDrag(ImVec2(40, 32), ImVec2(8, 0));
  ASSERT_EQ(room_->GetTileObjects()[0].x(), 5);
  ASSERT_EQ(UndoDepth(), 0u);

  auto& other = editor_->rooms()[1];
  other.SetLoaded(true);
  other.GetSprites().emplace_back(0x0A, 12, 14, 0, 1);
  // A retained canvas can change its binding before mouse release. Its old
  // transaction must finish while the callbacks still identify room 0.
  interaction.SetCurrentRoom(&editor_->rooms(), 1);
  EXPECT_EQ(UndoDepth(), 1u);
  coordinator().HandleRelease();
  EXPECT_EQ(UndoDepth(), 1u);
  ASSERT_TRUE(
      coordinator().sprite_handler().UpdateSprite(0, 0x0A, 13, 14, 0, 1, 0));
  ASSERT_EQ(UndoDepth(), 2u);

  ASSERT_TRUE(editor_->Undo().ok());
  EXPECT_EQ(other.GetSprites()[0].x(), 12);
  EXPECT_EQ(room_->GetTileObjects()[0].x(), 5);
  ASSERT_TRUE(editor_->Undo().ok());
  EXPECT_EQ(room_->GetTileObjects()[0].x(), 4);
  ASSERT_TRUE(editor_->Redo().ok());
  EXPECT_EQ(room_->GetTileObjects()[0].x(), 5);
  EXPECT_EQ(other.GetSprites()[0].x(), 12);
  ASSERT_TRUE(editor_->Redo().ok());
  EXPECT_EQ(other.GetSprites()[0].x(), 13);
}

TEST_P(DungeonEntityUndoLifecycleTest,
       CoverageNavigationBindsDestinationBeforeSelecting) {
  room_->AddTileObject(zelda3::RoomObject{0x01, 4, 4, 0x00, 0});
  viewer_->object_interaction().SetSelectedObjects({0});
  auto& destination = editor_->rooms()[1];
  destination.SetLoaded(true);
  destination.AddTileObject(zelda3::RoomObject{0x02, 8, 8, 0x00, 0});
  destination.AddTileObject(zelda3::RoomObject{0x03, 12, 12, 0x00, 0});

  DungeonEditorEntityUndoTestPeer::NavigateToObject(*editor_, 1, 1, 0x03);
  auto* destination_viewer =
      DungeonEditorEntityUndoTestPeer::Viewer(*editor_, 1);
  auto& interaction = destination_viewer->object_interaction();
  EXPECT_EQ(interaction.entity_coordinator()
                .tile_handler()
                .context()
                ->current_room_id,
            1);
  EXPECT_EQ(interaction.GetSelectedObjectIndices(), std::vector<size_t>{1});
  // The canvas repeats this binding when it draws the destination room.
  interaction.SetCurrentRoom(&editor_->rooms(), 1);
  EXPECT_EQ(interaction.GetSelectedObjectIndices(), std::vector<size_t>{1});
  EXPECT_EQ(UndoDepth(), 0u);
}

TEST_P(DungeonEntityUndoLifecycleTest,
       ReservedSpriteEncodingsRejectWholeMovementBeforeMutation) {
  auto& handler = coordinator().sprite_handler();
  room_->GetSprites().emplace_back(9, 4, 30, 24, 1);
  room_->GetSprites().emplace_back(10, 8, 12, 0, 0);
  EXPECT_FALSE(handler.UpdateSprite(0, 9, 4, 31, 24, 1, 0));
  EXPECT_FALSE(handler.UpdateSprite(0, 9, 4, 31, 31, 1, 0));
  EXPECT_FALSE(handler.UpdateSprite(0, 0xE4, 0, 29, 24, 1, 0));
  EXPECT_FALSE(handler.UpdateSprite(0, 0xE4, 0, 30, 24, 1, 0));
  EXPECT_EQ(UndoDepth(), 0u);
  coordinator().SelectEntity(EntityType::Sprite, 0);
  EXPECT_FALSE(handler.NudgeSelected(0, 1));
  coordinator().SetSelectedEntities(
      {{EntityType::Sprite, 1}, {EntityType::Sprite, 0}});
  EXPECT_FALSE(coordinator().NudgeSelected(0, 1));
  EXPECT_EQ(room_->GetSprites()[1].y(), 12);
  coordinator().BeginSelectionDrag(ImVec2(64, 96));
  coordinator().HandleDrag(ImVec2(64, 112), ImVec2(0, 16));
  coordinator().HandleRelease();
  EXPECT_EQ(room_->GetSprites()[0].y(), 30);
  EXPECT_EQ(room_->GetSprites()[1].y(), 12);
  EXPECT_EQ(UndoDepth(), 0u);
  EXPECT_FALSE(room_->sprites_dirty());
  // E4 is an ordinary editable ID outside the exact reserved marker encoding.
  EXPECT_TRUE(handler.UpdateSprite(0, 0xE4, 1, 30, 24, 1, 0));
  EXPECT_EQ(UndoDepth(), 1u);
}

TEST_P(DungeonEntityUndoLifecycleTest,
       ItemAndKeyDropPixelsRefreshOnEditUndoAndRedo) {
  game_data_.graphics_buffer.assign(zelda3::kNumGfxSheets * 4096, 0);
  for (auto& ids : game_data_.main_blockset_ids) {
    ids.fill(0);
  }
  for (auto& ids : game_data_.room_blockset_ids) {
    ids.fill(0);
  }
  for (auto& ids : game_data_.spriteset_ids) {
    ids.fill(0);
  }
  for (auto& ids : game_data_.paletteset_ids) {
    ids.fill(0);
  }
  gfx::SnesPalette palette;
  for (int i = 0; i < 90; ++i) {
    palette.AddColor(gfx::SnesColor(i + 1));
  }
  game_data_.palette_groups.dungeon_main.AddPalette(palette);
  editor_->SetGameData(&game_data_);
  room_->GetPotItems().push_back({0x0408, 1});
  room_->GetSprites().emplace_back(9, 20, 20, 0, 0);
  room_->MarkObjectsDirty();
  room_->RenderRoomGraphics();
  room_->ClearSaveDirtyState();
  auto pixel = [this](int x, int y) {
    const auto& bitmap = room_->object_bg1_buffer().bitmap();
    return bitmap.data()[y * bitmap.width() + x];
  };
  auto color_count = [this](uint8_t color) {
    const auto& bitmap = room_->object_bg1_buffer().bitmap();
    return std::count(bitmap.data(), bitmap.data() + bitmap.size(), color);
  };
  ASSERT_TRUE(room_->object_bg1_buffer().bitmap().is_active());
  ASSERT_EQ(room_->object_bg1_buffer().bitmap().width(), 512);
  ASSERT_EQ(pixel(34, 66), 30);
  ASSERT_TRUE(coordinator().item_handler().UpdateItem(0, 6, 48, 80));
  EXPECT_EQ(pixel(34, 66), 255);
  EXPECT_EQ(pixel(50, 82), 5);
  ASSERT_TRUE(editor_->Undo().ok());
  EXPECT_EQ(pixel(34, 66), 30);
  EXPECT_EQ(pixel(50, 82), 255);
  ASSERT_TRUE(editor_->Redo().ok());
  EXPECT_EQ(pixel(34, 66), 255);
  EXPECT_EQ(pixel(50, 82), 5);
  ASSERT_EQ(color_count(50), 0);
  ASSERT_TRUE(
      coordinator().sprite_handler().UpdateSprite(0, 9, 20, 20, 0, 0, 1));
  EXPECT_EQ(color_count(50), 16);
  ASSERT_TRUE(editor_->Undo().ok());
  EXPECT_EQ(color_count(50), 0);
  ASSERT_TRUE(editor_->Redo().ok());
  EXPECT_EQ(color_count(50), 16);
  EXPECT_TRUE(room_->sprites_dirty());
  EXPECT_TRUE(room_->pot_items_dirty());
  EXPECT_FALSE(room_->object_stream_dirty());
}

TEST_P(DungeonEntityUndoLifecycleTest,
       SpritePasteRedoRestoresInsertedSelectionAndMetadata) {
  room_->GetSprites().emplace_back(9, 4, 6, 3, 1);
  room_->GetSprites()[0].set_key_drop(2);
  auto& interaction = viewer_->object_interaction();
  interaction.SelectEntity(EntityType::Sprite, 0);
  interaction.HandleCopySelected();
  ImGui::GetIO().MousePos = ImVec2(-1000, -1000);
  interaction.HandlePasteObjects();
  ASSERT_EQ(room_->GetSprites().size(), 2u);
  ASSERT_EQ(UndoDepth(), 1u);
  EXPECT_EQ(coordinator().GetSelectedEntity(),
            (SelectedEntity{EntityType::Sprite, 1}));
  EXPECT_EQ(room_->GetSprites()[1].x(), 5);
  EXPECT_EQ(room_->GetSprites()[1].y(), 7);
  EXPECT_EQ(room_->GetSprites()[1].subtype(), 3);
  EXPECT_EQ(room_->GetSprites()[1].layer(), 1);
  EXPECT_EQ(room_->GetSprites()[1].key_drop(), 2);
  ASSERT_TRUE(editor_->Undo().ok());
  ASSERT_EQ(room_->GetSprites().size(), 1u);
  EXPECT_EQ(coordinator().GetSelectedEntity(),
            (SelectedEntity{EntityType::Sprite, 0}));
  ASSERT_TRUE(editor_->Redo().ok());
  ASSERT_EQ(room_->GetSprites().size(), 2u);
  EXPECT_EQ(coordinator().GetSelectedEntity(),
            (SelectedEntity{EntityType::Sprite, 1}));
  EXPECT_EQ(room_->GetSprites()[1].key_drop(), 2);
}

TEST_P(DungeonEntityUndoLifecycleTest,
       ItemPasteRedoRestoresInsertedSelectionAndType) {
  room_->GetPotItems().push_back({0x0408, 6});
  auto& interaction = viewer_->object_interaction();
  interaction.SelectEntity(EntityType::Item, 0);
  interaction.HandleCopySelected();
  ImGui::GetIO().MousePos = ImVec2(-1000, -1000);
  interaction.HandlePasteObjects();
  ASSERT_EQ(room_->GetPotItems().size(), 2u);
  ASSERT_EQ(UndoDepth(), 1u);
  EXPECT_EQ(coordinator().GetSelectedEntity(),
            (SelectedEntity{EntityType::Item, 1}));
  // Unified clipboard translates both axes on the shared 16px grid.
  EXPECT_EQ(room_->GetPotItems()[1].GetPixelX(), 48);
  EXPECT_EQ(room_->GetPotItems()[1].GetPixelY(), 80);
  EXPECT_EQ(room_->GetPotItems()[1].item, 6);
  ASSERT_TRUE(editor_->Undo().ok());
  ASSERT_EQ(room_->GetPotItems().size(), 1u);
  EXPECT_EQ(coordinator().GetSelectedEntity(),
            (SelectedEntity{EntityType::Item, 0}));
  ASSERT_TRUE(editor_->Redo().ok());
  ASSERT_EQ(room_->GetPotItems().size(), 2u);
  EXPECT_EQ(coordinator().GetSelectedEntity(),
            (SelectedEntity{EntityType::Item, 1}));
  EXPECT_EQ(room_->GetPotItems()[1].item, 6);
}

TEST_P(DungeonEntityUndoLifecycleTest,
       SpritePasteRejectsTranslatedTerminatorWithoutMutation) {
  room_->GetSprites().emplace_back(9, 4, 30, 24, 1);
  auto& interaction = viewer_->object_interaction();
  interaction.SelectEntity(EntityType::Sprite, 0);
  interaction.HandleCopySelected();
  ImGui::GetIO().MousePos = ImVec2(-1000, -1000);
  interaction.HandlePasteObjects();
  ASSERT_EQ(room_->GetSprites().size(), 1u);
  EXPECT_EQ(UndoDepth(), 0u);
  EXPECT_FALSE(room_->sprites_dirty());
  EXPECT_EQ(coordinator().GetSelectedEntity(),
            (SelectedEntity{EntityType::Sprite, 0}));
  EXPECT_EQ(room_->GetSprites()[0].x(), 4);
  EXPECT_EQ(room_->GetSprites()[0].y(), 30);
}

INSTANTIATE_TEST_SUITE_P(ViewerModes, DungeonEntityUndoLifecycleTest,
                         ::testing::Bool(), [](const auto& param) {
                           return param.param ? "Workbench" : "Standalone";
                         });

}  // namespace
}  // namespace yaze::editor
