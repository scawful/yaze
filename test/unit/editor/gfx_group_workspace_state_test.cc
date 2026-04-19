#include "app/editor/graphics/gfx_group_editor.h"

#include "gtest/gtest.h"

namespace yaze::editor {

TEST(GfxGroupWorkspaceState, SharedBetweenGfxGroupEditors) {
  GfxGroupWorkspaceState shared;
  GfxGroupEditor a;
  GfxGroupEditor b;
  a.SetWorkspaceState(&shared);
  b.SetWorkspaceState(&shared);

  a.SetSelectedBlockset(0x11);
  EXPECT_EQ(b.workspace_state().selected_blockset, 0x11);

  b.SetSelectedRoomset(0x22);
  EXPECT_EQ(a.workspace_state().selected_roomset, 0x22);
}

}  // namespace yaze::editor
