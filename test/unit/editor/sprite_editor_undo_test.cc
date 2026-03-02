#include "app/editor/sprite/sprite_undo_actions.h"

#include <gtest/gtest.h>

#include "app/editor/core/undo_manager.h"

namespace yaze::editor {
namespace {

// Helper to create a minimal ZSprite with some identifiable data.
zsprite::ZSprite MakeTestSprite(const std::string& name, int health) {
  zsprite::ZSprite sprite;
  sprite.Reset();
  sprite.sprName = name;
  sprite.property_sprname.Text = name;
  sprite.property_health.Text = std::to_string(health);

  // Add a frame with one tile so we have frame data to verify.
  zsprite::Frame frame;
  zsprite::OamTile tile;
  tile.x = 10;
  tile.y = 20;
  tile.id = 42;
  tile.palette = 3;
  frame.Tiles.push_back(tile);
  sprite.editor.Frames.push_back(frame);

  // Add a simple animation.
  sprite.animations.emplace_back(0, 0, 1, "Idle");

  return sprite;
}

TEST(SpriteEditActionTest, UndoRestoresBeforeSnapshot) {
  SpriteSnapshot before;
  before.sprite_index = 0;
  before.current_frame = 0;
  before.current_animation_index = 0;
  before.sprite_data = MakeTestSprite("Original", 100);

  SpriteSnapshot after;
  after.sprite_index = 0;
  after.current_frame = 1;
  after.current_animation_index = 0;
  after.sprite_data = MakeTestSprite("Modified", 50);

  SpriteSnapshot restored;
  SpriteEditAction action(before, after,
                          [&](const SpriteSnapshot& s) { restored = s; });

  ASSERT_TRUE(action.Undo().ok());
  EXPECT_EQ(restored.sprite_index, 0);
  EXPECT_EQ(restored.current_frame, 0);
  EXPECT_EQ(restored.sprite_data.sprName, "Original");
  EXPECT_EQ(restored.sprite_data.property_health.Text, "100");
}

TEST(SpriteEditActionTest, RedoRestoresAfterSnapshot) {
  SpriteSnapshot before;
  before.sprite_index = 0;
  before.current_frame = 0;
  before.current_animation_index = 0;
  before.sprite_data = MakeTestSprite("Original", 100);

  SpriteSnapshot after;
  after.sprite_index = 0;
  after.current_frame = 1;
  after.current_animation_index = 0;
  after.sprite_data = MakeTestSprite("Modified", 50);

  SpriteSnapshot restored;
  SpriteEditAction action(before, after,
                          [&](const SpriteSnapshot& s) { restored = s; });

  ASSERT_TRUE(action.Redo().ok());
  EXPECT_EQ(restored.sprite_index, 0);
  EXPECT_EQ(restored.current_frame, 1);
  EXPECT_EQ(restored.sprite_data.sprName, "Modified");
  EXPECT_EQ(restored.sprite_data.property_health.Text, "50");
}

TEST(SpriteEditActionTest, UndoRedoRoundTrip) {
  SpriteSnapshot before;
  before.sprite_index = 0;
  before.current_frame = 0;
  before.current_animation_index = 0;
  before.sprite_data = MakeTestSprite("Before", 200);

  SpriteSnapshot after;
  after.sprite_index = 0;
  after.current_frame = 2;
  after.current_animation_index = 1;
  after.sprite_data = MakeTestSprite("After", 75);
  // Add extra frame data to differentiate
  after.sprite_data.editor.Frames.emplace_back();

  SpriteSnapshot restored;
  SpriteEditAction action(before, after,
                          [&](const SpriteSnapshot& s) { restored = s; });

  // Redo applies the "after" state
  ASSERT_TRUE(action.Redo().ok());
  EXPECT_EQ(restored.sprite_data.sprName, "After");
  EXPECT_EQ(restored.sprite_data.editor.Frames.size(), 2u);
  EXPECT_EQ(restored.current_frame, 2);

  // Undo restores the "before" state
  ASSERT_TRUE(action.Undo().ok());
  EXPECT_EQ(restored.sprite_data.sprName, "Before");
  EXPECT_EQ(restored.sprite_data.editor.Frames.size(), 1u);
  EXPECT_EQ(restored.current_frame, 0);
}

TEST(SpriteEditActionTest, FrameDataPreservedAcrossUndoRedo) {
  SpriteSnapshot before;
  before.sprite_index = 0;
  before.sprite_data = MakeTestSprite("Test", 10);

  SpriteSnapshot after;
  after.sprite_index = 0;
  after.sprite_data = MakeTestSprite("Test", 10);
  // Modify a tile in the after state
  after.sprite_data.editor.Frames[0].Tiles[0].x = 99;
  after.sprite_data.editor.Frames[0].Tiles[0].y = 88;

  SpriteSnapshot restored;
  SpriteEditAction action(before, after,
                          [&](const SpriteSnapshot& s) { restored = s; });

  ASSERT_TRUE(action.Redo().ok());
  ASSERT_EQ(restored.sprite_data.editor.Frames.size(), 1u);
  ASSERT_EQ(restored.sprite_data.editor.Frames[0].Tiles.size(), 1u);
  EXPECT_EQ(restored.sprite_data.editor.Frames[0].Tiles[0].x, 99);
  EXPECT_EQ(restored.sprite_data.editor.Frames[0].Tiles[0].y, 88);

  ASSERT_TRUE(action.Undo().ok());
  EXPECT_EQ(restored.sprite_data.editor.Frames[0].Tiles[0].x, 10);
  EXPECT_EQ(restored.sprite_data.editor.Frames[0].Tiles[0].y, 20);
}

TEST(SpriteEditActionTest, BooleanPropertiesPreserved) {
  SpriteSnapshot before;
  before.sprite_index = 0;
  before.sprite_data = MakeTestSprite("Flags", 1);
  before.sprite_data.property_blockable.IsChecked = false;
  before.sprite_data.property_isboss.IsChecked = false;

  SpriteSnapshot after;
  after.sprite_index = 0;
  after.sprite_data = MakeTestSprite("Flags", 1);
  after.sprite_data.property_blockable.IsChecked = true;
  after.sprite_data.property_isboss.IsChecked = true;

  SpriteSnapshot restored;
  SpriteEditAction action(before, after,
                          [&](const SpriteSnapshot& s) { restored = s; });

  ASSERT_TRUE(action.Redo().ok());
  EXPECT_TRUE(restored.sprite_data.property_blockable.IsChecked);
  EXPECT_TRUE(restored.sprite_data.property_isboss.IsChecked);

  ASSERT_TRUE(action.Undo().ok());
  EXPECT_FALSE(restored.sprite_data.property_blockable.IsChecked);
  EXPECT_FALSE(restored.sprite_data.property_isboss.IsChecked);
}

TEST(SpriteEditActionTest, NoRestoreCallbackReturnsError) {
  SpriteSnapshot before;
  SpriteSnapshot after;
  SpriteEditAction action(before, after, nullptr);

  EXPECT_FALSE(action.Undo().ok());
  EXPECT_FALSE(action.Redo().ok());
}

TEST(SpriteEditActionTest, DescriptionIncludesSpriteName) {
  SpriteSnapshot before;
  before.sprite_index = 0;
  before.sprite_data.sprName = "Guard";

  SpriteSnapshot after;
  after.sprite_index = 0;
  after.sprite_data.sprName = "Guard";

  SpriteEditAction action(before, after, [](const SpriteSnapshot&) {});

  EXPECT_NE(action.Description().find("Guard"), std::string::npos);
}

TEST(SpriteEditActionTest, DescriptionFallsBackToIndex) {
  SpriteSnapshot before;
  before.sprite_index = 3;
  before.sprite_data.sprName = "";

  SpriteSnapshot after;
  after.sprite_index = 3;
  after.sprite_data.sprName = "";

  SpriteEditAction action(before, after, [](const SpriteSnapshot&) {});

  EXPECT_NE(action.Description().find("3"), std::string::npos);
}

TEST(SpriteEditActionTest, UndoManagerIntegration) {
  UndoManager manager;

  SpriteSnapshot before;
  before.sprite_index = 0;
  before.sprite_data = MakeTestSprite("V1", 100);

  SpriteSnapshot after;
  after.sprite_index = 0;
  after.sprite_data = MakeTestSprite("V2", 200);

  SpriteSnapshot restored;
  auto restore_fn = [&](const SpriteSnapshot& s) {
    restored = s;
  };

  manager.Push(std::make_unique<SpriteEditAction>(before, after, restore_fn));

  EXPECT_TRUE(manager.CanUndo());
  EXPECT_FALSE(manager.CanRedo());

  ASSERT_TRUE(manager.Undo().ok());
  EXPECT_EQ(restored.sprite_data.sprName, "V1");
  EXPECT_TRUE(manager.CanRedo());

  ASSERT_TRUE(manager.Redo().ok());
  EXPECT_EQ(restored.sprite_data.sprName, "V2");
}

}  // namespace
}  // namespace yaze::editor
