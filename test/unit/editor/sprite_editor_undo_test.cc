#include "app/editor/core/undo_manager.h"
#include "app/editor/sprite/sprite_undo_actions.h"

#include "gtest/gtest.h"

namespace yaze::editor {
namespace {

SpriteSnapshot MakeSnapshot(int sprite_index, const std::string& name,
                            int frame_index) {
  SpriteSnapshot snapshot;
  snapshot.sprite_index = sprite_index;
  snapshot.sprite.sprName = name;
  snapshot.sprite.property_health.Text = "8";
  snapshot.sprite.editor.Frames.emplace_back();
  snapshot.sprite.editor.Frames.back().Tiles.emplace_back();
  snapshot.sprite.editor.Frames.back().Tiles.back().id =
      static_cast<uint16_t>(frame_index);
  snapshot.sprite_path = "/tmp/test.zsm";
  snapshot.current_frame = frame_index;
  snapshot.current_animation_index = 0;
  snapshot.selected_tile_index = frame_index;
  snapshot.selected_routine_index = -1;
  snapshot.animation_playing = false;
  snapshot.frame_timer = 0.0f;
  snapshot.zsm_dirty = true;
  return snapshot;
}

TEST(SpriteUndoActionsTest, UndoRedoRestoresSpriteAndUiState) {
  SpriteSnapshot before = MakeSnapshot(0, "Before", 0);
  SpriteSnapshot after = MakeSnapshot(0, "After", 1);
  after.sprite.property_health.Text = "32";
  after.sprite.editor.Frames.back().Tiles.back().palette = 5;

  SpriteSnapshot restored;
  SpriteEditAction action(
      before, after,
      [&](const SpriteSnapshot& snapshot) { restored = snapshot; },
      "Edit sprite frame");

  ASSERT_TRUE(action.Undo().ok());
  EXPECT_EQ(restored.sprite.sprName, "Before");
  EXPECT_EQ(restored.current_frame, 0);
  EXPECT_EQ(restored.sprite.property_health.Text, "8");
  ASSERT_EQ(restored.sprite.editor.Frames.size(), 1u);
  ASSERT_EQ(restored.sprite.editor.Frames[0].Tiles.size(), 1u);
  EXPECT_EQ(restored.sprite.editor.Frames[0].Tiles[0].id, 0);

  ASSERT_TRUE(action.Redo().ok());
  EXPECT_EQ(restored.sprite.sprName, "After");
  EXPECT_EQ(restored.current_frame, 1);
  EXPECT_EQ(restored.sprite.property_health.Text, "32");
  ASSERT_EQ(restored.sprite.editor.Frames.size(), 1u);
  ASSERT_EQ(restored.sprite.editor.Frames[0].Tiles.size(), 1u);
  EXPECT_EQ(restored.sprite.editor.Frames[0].Tiles[0].id, 1);
  EXPECT_EQ(restored.sprite.editor.Frames[0].Tiles[0].palette, 5);
}

TEST(SpriteUndoActionsTest, UndoManagerIntegrationUsesDescriptionAndRestores) {
  UndoManager manager;
  SpriteSnapshot before = MakeSnapshot(1, "SpriteA", 0);
  SpriteSnapshot after = MakeSnapshot(1, "SpriteB", 2);
  SpriteSnapshot restored;

  manager.Push(std::make_unique<SpriteEditAction>(
      before, after, [&](const SpriteSnapshot& snapshot) { restored = snapshot; },
      "Rename sprite"));

  EXPECT_TRUE(manager.CanUndo());
  EXPECT_EQ(manager.GetUndoDescription(), "Rename sprite");

  ASSERT_TRUE(manager.Undo().ok());
  EXPECT_EQ(restored.sprite.sprName, "SpriteA");
  EXPECT_EQ(restored.current_frame, 0);
  EXPECT_TRUE(manager.CanRedo());

  ASSERT_TRUE(manager.Redo().ok());
  EXPECT_EQ(restored.sprite.sprName, "SpriteB");
  EXPECT_EQ(restored.current_frame, 2);
}

}  // namespace
}  // namespace yaze::editor
