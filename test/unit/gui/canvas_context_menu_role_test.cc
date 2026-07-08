#include "app/gui/canvas/canvas_types.h"

#include <gtest/gtest.h>

namespace yaze {
namespace gui {
namespace {

// CanvasRole describes what the canvas IS (preview vs editable). The
// context-menu code at canvas_context_menu.cc:106-113 gates "Pixel Format"
// (Reformat to 4BPP/8BPP) and "Edit Palette" entries on
// RoleAllowsBitmapMutation: those operations would corrupt the underlying
// data for read-only canvases whose bitmap is a shared view.
//
// This test pins the gate's truth table. Adding a new CanvasRole variant
// requires deciding whether mutation is allowed; tightening or loosening
// the gate forces the test to be updated alongside the code.
TEST(CanvasContextMenuRoleTest, EditableScratchpadAllowsMutation) {
  EXPECT_TRUE(RoleAllowsBitmapMutation(CanvasRole::kEditableScratchpad));
}

TEST(CanvasContextMenuRoleTest, CompositeOutputAllowsMutation) {
  // Composite output bitmaps are produced by the editor (room renderer,
  // title screen layer composite). They're owned by the editor, not a
  // shared view — Reformat/SetPalette is destructive but in-bounds.
  EXPECT_TRUE(RoleAllowsBitmapMutation(CanvasRole::kCompositeOutput));
}

TEST(CanvasContextMenuRoleTest, PreviewOnlyForbidsMutation) {
  // gfx-group sheet thumbnails, sprite atlas previews, sheet-browser
  // tiles. Bitmap is a borrowed Arena reference; reformatting it would
  // corrupt every other view of the same sheet.
  EXPECT_FALSE(RoleAllowsBitmapMutation(CanvasRole::kPreviewOnly));
}

TEST(CanvasContextMenuRoleTest, SelectionSourceForbidsMutation) {
  // Tile pickers, font atlas selectors. Read-only by contract — the
  // canvas exists to let the user click a region, not to edit it.
  EXPECT_FALSE(RoleAllowsBitmapMutation(CanvasRole::kSelectionSource));
}

}  // namespace
}  // namespace gui
}  // namespace yaze
