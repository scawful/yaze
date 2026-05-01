#include "integration/overworld_editor_test.h"

#include <vector>

namespace yaze {
namespace test {

namespace {

void ExpectTile16Equals(const gfx::Tile16& lhs, const gfx::Tile16& rhs) {
  EXPECT_EQ(gfx::TileInfoToWord(lhs.tile0_), gfx::TileInfoToWord(rhs.tile0_));
  EXPECT_EQ(gfx::TileInfoToWord(lhs.tile1_), gfx::TileInfoToWord(rhs.tile1_));
  EXPECT_EQ(gfx::TileInfoToWord(lhs.tile2_), gfx::TileInfoToWord(rhs.tile2_));
  EXPECT_EQ(gfx::TileInfoToWord(lhs.tile3_), gfx::TileInfoToWord(rhs.tile3_));
}

}  // namespace

TEST_F(OverworldEditorTest, LoadAndSave) {
  // Verify initial state
  EXPECT_TRUE(overworld_editor_->IsRomLoaded());

  // Perform Save
  auto status = overworld_editor_->Save();
  EXPECT_TRUE(status.ok()) << "Save failed: " << status.message();
}

TEST_F(OverworldEditorTest, SwitchMaps) {
  // Test switching maps
  overworld_editor_->set_current_map(0);
  overworld_editor_->Update();  // Trigger sync
  EXPECT_EQ(overworld_editor_->overworld().current_map_id(), 0);

  overworld_editor_->set_current_map(1);
  overworld_editor_->Update();  // Trigger sync
  EXPECT_EQ(overworld_editor_->overworld().current_map_id(), 1);
}

TEST_F(OverworldEditorTest, CopyFailsWithoutSelection) {
  // Copy with no tile selected (tile16 = -1) should fail gracefully
  overworld_editor_->set_current_tile16(-1);
  auto status = overworld_editor_->Copy();
  EXPECT_FALSE(status.ok());
  EXPECT_TRUE(absl::IsFailedPrecondition(status));
}

TEST_F(OverworldEditorTest, PasteFailsWithEmptyClipboard) {
  // Paste with empty clipboard should fail gracefully
  ASSERT_TRUE(shared_clipboard_ != nullptr);
  EXPECT_FALSE(shared_clipboard_->has_overworld_tile16);
  auto status = overworld_editor_->Paste();
  EXPECT_FALSE(status.ok());
  EXPECT_TRUE(absl::IsFailedPrecondition(status));
}

TEST_F(OverworldEditorTest, ClipboardRoundTripSingleTile) {
  // Set a current tile to copy
  overworld_editor_->set_current_tile16(42);

  // Copy should succeed with a selected tile
  auto copy_status = overworld_editor_->Copy();
  ASSERT_TRUE(copy_status.ok()) << "Copy failed: " << copy_status.message();

  // Verify clipboard state
  EXPECT_TRUE(shared_clipboard_->has_overworld_tile16);
  ASSERT_EQ(shared_clipboard_->overworld_tile16_ids.size(), 1);
  EXPECT_EQ(shared_clipboard_->overworld_tile16_ids[0], 42);
  EXPECT_EQ(shared_clipboard_->overworld_width, 1);
  EXPECT_EQ(shared_clipboard_->overworld_height, 1);
}

TEST_F(OverworldEditorTest, RequestTile16SelectionUpdatesEditorSelection) {
  constexpr int kTile = 15;

  overworld_editor_->RequestTile16Selection(kTile);

  EXPECT_EQ(overworld_editor_->tile16_editor().current_tile16(), kTile);
}

TEST_F(OverworldEditorTest,
       RequestTile16SelectionWithStagedCurrentTileOpensEditorGuard) {
  constexpr int kFromTile = 15;
  constexpr int kTargetTile = 16;
  auto& tile16_editor = overworld_editor_->tile16_editor();

  ASSERT_TRUE(tile16_editor.SetCurrentTile(kFromTile).ok());
  ASSERT_TRUE(
      tile16_editor
          .ApplyPaletteToQuadrant(
              /*quadrant=*/0,
              static_cast<uint8_t>((tile16_editor.current_palette() + 1) % 8))
          .ok());
  ASSERT_TRUE(tile16_editor.is_tile_modified(kFromTile));
  window_manager_->RegisterWindow(window_manager_->GetActiveSessionId(),
                                  editor::OverworldPanelIds::kTile16Editor,
                                  "Tile16 Editor", "", "Overworld");

  overworld_editor_->RequestTile16Selection(kTargetTile);

  EXPECT_EQ(tile16_editor.current_tile16(), kFromTile);
  EXPECT_EQ(tile16_editor.pending_changes_count(), 1);
  ASSERT_NE(window_manager_, nullptr);
  EXPECT_TRUE(
      window_manager_->IsWindowOpen(window_manager_->GetActiveSessionId(),
                                    editor::OverworldPanelIds::kTile16Editor));
}

TEST_F(OverworldEditorTest, CommitTile16ChangesRefreshesOverworldState) {
  constexpr int kTile = 21;
  auto& tile16_editor = overworld_editor_->tile16_editor();

  const auto original = rom_->ReadTile16(kTile, zelda3::kTile16Ptr);
  ASSERT_TRUE(original.ok());
  ASSERT_TRUE(tile16_editor.SetCurrentTile(kTile).ok());
  const uint8_t target_palette =
      static_cast<uint8_t>((original->tile0_.palette_ + 1) % 8);

  ASSERT_TRUE(tile16_editor.ApplyPaletteToAll(target_palette).ok());
  ASSERT_TRUE(tile16_editor.is_tile_modified(kTile));
  ASSERT_TRUE(tile16_editor.CommitAllChanges().ok());

  const auto committed = rom_->ReadTile16(kTile, zelda3::kTile16Ptr);
  ASSERT_TRUE(committed.ok());
  EXPECT_FALSE(*committed == *original);
  ASSERT_GT(overworld_editor_->overworld().tiles16().size(),
            static_cast<size_t>(kTile));
  ExpectTile16Equals(overworld_editor_->overworld().tiles16()[kTile],
                     *committed);

  const auto* map = overworld_editor_->overworld().overworld_map(
      overworld_editor_->current_map_id());
  ASSERT_NE(map, nullptr);
  EXPECT_EQ(overworld_editor_->tile16_blockset().atlas.palette(),
            map->current_palette());
}

TEST_F(OverworldEditorTest,
       RefreshTile16BlocksetSyncsTile8SourceGraphicsToCurrentMap) {
  ASSERT_TRUE(overworld_editor_->overworld().EnsureMapBuilt(0).ok());
  const auto* initial_map = overworld_editor_->overworld().overworld_map(0);
  ASSERT_NE(initial_map, nullptr);
  const std::vector<uint8_t> initial_graphics = initial_map->current_graphics();
  ASSERT_FALSE(initial_graphics.empty());

  int target_map = -1;
  for (int map_id = 1; map_id < zelda3::kNumOverworldMaps; ++map_id) {
    auto status = overworld_editor_->overworld().EnsureMapBuilt(map_id);
    if (!status.ok()) {
      continue;
    }

    const auto* candidate =
        overworld_editor_->overworld().overworld_map(map_id);
    if (candidate && !candidate->current_graphics().empty() &&
        candidate->current_graphics() != initial_graphics) {
      target_map = map_id;
      break;
    }
  }
  ASSERT_NE(target_map, -1)
      << "Test ROM did not expose a map with distinct source graphics";

  overworld_editor_->SelectMapForEditing(target_map);
  ASSERT_EQ(overworld_editor_->current_map_id(), target_map);

  const auto* target = overworld_editor_->overworld().overworld_map(target_map);
  ASSERT_NE(target, nullptr);
  EXPECT_EQ(overworld_editor_->current_gfx_bmp_for_testing().depth(), 8);
  EXPECT_EQ(overworld_editor_->current_gfx_bmp_for_testing().vector(),
            target->current_graphics());
}

TEST_F(OverworldEditorTest,
       ItemSelectionWorkflowDuplicateNudgeDeleteMaintainsContinuity) {
  auto* items = overworld_editor_->overworld().mutable_all_items();
  ASSERT_NE(items, nullptr);

  items->clear();
  items->emplace_back(/*id=*/0x40, /*room_map_id=*/0x00, /*x=*/96, /*y=*/112,
                      /*bg2=*/false);
  items->emplace_back(/*id=*/0x41, /*room_map_id=*/0x00, /*x=*/400, /*y=*/400,
                      /*bg2=*/false);

  const zelda3::OverworldItem source_identity = items->at(0);
  ASSERT_TRUE(overworld_editor_->SelectItemByIdentity(source_identity));
  ASSERT_NE(overworld_editor_->GetSelectedItem(), nullptr);

  ASSERT_TRUE(overworld_editor_->DuplicateSelectedItem(/*offset_x=*/16,
                                                       /*offset_y=*/0));
  ASSERT_EQ(items->size(), 3u);

  auto* selected = overworld_editor_->GetSelectedItem();
  ASSERT_NE(selected, nullptr);
  EXPECT_EQ(selected->x_, 112);
  EXPECT_EQ(selected->y_, 112);

  ASSERT_TRUE(overworld_editor_->NudgeSelectedItem(/*delta_x=*/1,
                                                   /*delta_y=*/-1));
  selected = overworld_editor_->GetSelectedItem();
  ASSERT_NE(selected, nullptr);
  EXPECT_EQ(selected->x_, 113);
  EXPECT_EQ(selected->y_, 111);
  EXPECT_EQ(selected->game_x_, 7);
  EXPECT_EQ(selected->game_y_, 6);

  ASSERT_TRUE(overworld_editor_->DeleteSelectedItem());
  ASSERT_EQ(items->size(), 2u);
  selected = overworld_editor_->GetSelectedItem();
  ASSERT_NE(selected, nullptr);
  EXPECT_EQ(selected->id_, 0x40);
  EXPECT_EQ(selected->x_, 96);
  EXPECT_EQ(selected->y_, 112);
}

TEST_F(OverworldEditorTest, ItemMultiDeleteUndoRedoRestoresListAndSelection) {
  auto* items = overworld_editor_->overworld().mutable_all_items();
  ASSERT_NE(items, nullptr);

  items->clear();
  items->emplace_back(/*id=*/0x40, /*room_map_id=*/0x00, /*x=*/96, /*y=*/112,
                      /*bg2=*/false);
  items->emplace_back(/*id=*/0x41, /*room_map_id=*/0x00, /*x=*/120, /*y=*/112,
                      /*bg2=*/false);
  items->emplace_back(/*id=*/0x42, /*room_map_id=*/0x00, /*x=*/200, /*y=*/112,
                      /*bg2=*/false);

  ASSERT_TRUE(overworld_editor_->SelectItemByIdentity(items->at(1)));
  ASSERT_TRUE(overworld_editor_->DeleteSelectedItem());
  ASSERT_EQ(items->size(), 2u);
  ASSERT_NE(overworld_editor_->GetSelectedItem(), nullptr);
  EXPECT_EQ(overworld_editor_->GetSelectedItem()->id_, 0x40);

  ASSERT_TRUE(overworld_editor_->DeleteSelectedItem());
  ASSERT_EQ(items->size(), 1u);
  ASSERT_NE(overworld_editor_->GetSelectedItem(), nullptr);
  EXPECT_EQ(overworld_editor_->GetSelectedItem()->id_, 0x42);

  ASSERT_TRUE(overworld_editor_->Undo().ok());
  ASSERT_EQ(items->size(), 2u);
  ASSERT_NE(overworld_editor_->GetSelectedItem(), nullptr);
  EXPECT_EQ(overworld_editor_->GetSelectedItem()->id_, 0x40);

  ASSERT_TRUE(overworld_editor_->Undo().ok());
  ASSERT_EQ(items->size(), 3u);
  ASSERT_NE(overworld_editor_->GetSelectedItem(), nullptr);
  EXPECT_EQ(overworld_editor_->GetSelectedItem()->id_, 0x41);

  ASSERT_TRUE(overworld_editor_->Redo().ok());
  ASSERT_EQ(items->size(), 2u);
  ASSERT_NE(overworld_editor_->GetSelectedItem(), nullptr);
  EXPECT_EQ(overworld_editor_->GetSelectedItem()->id_, 0x40);

  ASSERT_TRUE(overworld_editor_->Redo().ok());
  ASSERT_EQ(items->size(), 1u);
  ASSERT_NE(overworld_editor_->GetSelectedItem(), nullptr);
  EXPECT_EQ(overworld_editor_->GetSelectedItem()->id_, 0x42);
}

TEST_F(OverworldEditorTest, ItemNudgeSequenceUndoRedoRestoresCoordinates) {
  auto* items = overworld_editor_->overworld().mutable_all_items();
  ASSERT_NE(items, nullptr);

  items->clear();
  items->emplace_back(/*id=*/0x55, /*room_map_id=*/0x00, /*x=*/160, /*y=*/176,
                      /*bg2=*/false);

  ASSERT_TRUE(overworld_editor_->SelectItemByIdentity(items->at(0)));
  ASSERT_TRUE(overworld_editor_->NudgeSelectedItem(/*delta_x=*/16,
                                                   /*delta_y=*/0));
  ASSERT_TRUE(overworld_editor_->NudgeSelectedItem(/*delta_x=*/0,
                                                   /*delta_y=*/-16));

  auto* selected = overworld_editor_->GetSelectedItem();
  ASSERT_NE(selected, nullptr);
  EXPECT_EQ(selected->x_, 176);
  EXPECT_EQ(selected->y_, 160);
  EXPECT_EQ(selected->game_x_, 11);
  EXPECT_EQ(selected->game_y_, 10);

  ASSERT_TRUE(overworld_editor_->Undo().ok());
  selected = overworld_editor_->GetSelectedItem();
  ASSERT_NE(selected, nullptr);
  EXPECT_EQ(selected->x_, 176);
  EXPECT_EQ(selected->y_, 176);
  EXPECT_EQ(selected->game_x_, 11);
  EXPECT_EQ(selected->game_y_, 11);

  ASSERT_TRUE(overworld_editor_->Undo().ok());
  selected = overworld_editor_->GetSelectedItem();
  ASSERT_NE(selected, nullptr);
  EXPECT_EQ(selected->x_, 160);
  EXPECT_EQ(selected->y_, 176);
  EXPECT_EQ(selected->game_x_, 10);
  EXPECT_EQ(selected->game_y_, 11);

  ASSERT_TRUE(overworld_editor_->Redo().ok());
  selected = overworld_editor_->GetSelectedItem();
  ASSERT_NE(selected, nullptr);
  EXPECT_EQ(selected->x_, 176);
  EXPECT_EQ(selected->y_, 176);

  ASSERT_TRUE(overworld_editor_->Redo().ok());
  selected = overworld_editor_->GetSelectedItem();
  ASSERT_NE(selected, nullptr);
  EXPECT_EQ(selected->x_, 176);
  EXPECT_EQ(selected->y_, 160);
}

TEST_F(OverworldEditorTest, StaleItemSelectionClearsWhenBackingItemIsRemoved) {
  auto* items = overworld_editor_->overworld().mutable_all_items();
  ASSERT_NE(items, nullptr);

  items->clear();
  items->emplace_back(/*id=*/0x51, /*room_map_id=*/0x00, /*x=*/160, /*y=*/176,
                      /*bg2=*/false);

  const zelda3::OverworldItem selected_identity = items->at(0);
  ASSERT_TRUE(overworld_editor_->SelectItemByIdentity(selected_identity));
  ASSERT_NE(overworld_editor_->GetSelectedItem(), nullptr);

  items->clear();
  EXPECT_EQ(overworld_editor_->GetSelectedItem(), nullptr);
  EXPECT_FALSE(overworld_editor_->selected_item_identity().has_value());
  EXPECT_FALSE(overworld_editor_->DeleteSelectedItem());
}

TEST_F(OverworldEditorTest, SelectingNonItemEntityClearsItemSelectionState) {
  auto* items = overworld_editor_->overworld().mutable_all_items();
  ASSERT_NE(items, nullptr);

  items->clear();
  items->emplace_back(/*id=*/0x51, /*room_map_id=*/0x00, /*x=*/160, /*y=*/176,
                      /*bg2=*/false);

  ASSERT_TRUE(overworld_editor_->SelectItemByIdentity(items->at(0)));
  ASSERT_TRUE(overworld_editor_->selected_item_identity().has_value());
  ASSERT_NE(overworld_editor_->GetSelectedItem(), nullptr);

  zelda3::OverworldExit exit(/*room_id=*/0x12, /*map_id=*/0x00,
                             /*vram_location=*/0x0000, /*y_scroll=*/0x0000,
                             /*x_scroll=*/0x0000, /*player_y=*/0x0100,
                             /*player_x=*/0x0120, /*camera_y=*/0x0000,
                             /*camera_x=*/0x0000, /*scroll_mod_y=*/0x00,
                             /*scroll_mod_x=*/0x00, /*door_type_1=*/0x0000,
                             /*door_type_2=*/0x0000, /*deleted=*/false);
  overworld_editor_->SetCurrentEntity(&exit);

  EXPECT_EQ(overworld_editor_->current_entity(), &exit);
  EXPECT_FALSE(overworld_editor_->selected_item_identity().has_value());
  EXPECT_EQ(overworld_editor_->GetSelectedItem(), nullptr);
  EXPECT_FALSE(overworld_editor_->DeleteSelectedItem());
}

}  // namespace test
}  // namespace yaze
