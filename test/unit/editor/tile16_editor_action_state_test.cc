#include "app/editor/overworld/tile16_editor_action_state.h"

#include "gtest/gtest.h"

namespace yaze::editor {
namespace {

TEST(Tile16EditorActionStateTest, NoPendingNoUndoDisablesStagedActions) {
  const auto state = ComputeTile16ActionControlState(
      /*has_pending=*/false,
      /*current_tile_pending=*/false,
      /*can_undo=*/false);

  EXPECT_FALSE(state.can_write_pending);
  EXPECT_FALSE(state.can_discard_all);
  EXPECT_FALSE(state.can_discard_current);
  EXPECT_FALSE(state.can_undo);
}

TEST(Tile16EditorActionStateTest,
     PendingQueueEnablesWriteAndDiscardAllButNotCurrent) {
  const auto state = ComputeTile16ActionControlState(
      /*has_pending=*/true,
      /*current_tile_pending=*/false,
      /*can_undo=*/true);

  EXPECT_TRUE(state.can_write_pending);
  EXPECT_TRUE(state.can_discard_all);
  EXPECT_FALSE(state.can_discard_current);
  EXPECT_TRUE(state.can_undo);
}

TEST(Tile16EditorActionStateTest,
     CurrentTilePendingEnablesCurrentDiscardIndependently) {
  const auto state = ComputeTile16ActionControlState(
      /*has_pending=*/false,
      /*current_tile_pending=*/true,
      /*can_undo=*/true);

  EXPECT_FALSE(state.can_write_pending);
  EXPECT_FALSE(state.can_discard_all);
  EXPECT_TRUE(state.can_discard_current);
  EXPECT_TRUE(state.can_undo);
}

TEST(Tile16EditorActionStateTest, FullPendingStateEnablesAllRelevantActions) {
  const auto state = ComputeTile16ActionControlState(
      /*has_pending=*/true,
      /*current_tile_pending=*/true,
      /*can_undo=*/false);

  EXPECT_TRUE(state.can_write_pending);
  EXPECT_TRUE(state.can_discard_all);
  EXPECT_TRUE(state.can_discard_current);
  EXPECT_FALSE(state.can_undo);
}

}  // namespace
}  // namespace yaze::editor
