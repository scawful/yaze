#include "app/editor/graphics/panels/graphics_editor_panels.h"
#include "app/editor/graphics/polyhedral_editor_panel.h"

#include "gtest/gtest.h"

namespace yaze::editor {

namespace {

void NoOpDraw() {}

}  // namespace

TEST(GraphicsEditorWindowIds, WrapperPanelsUseStableIds) {
  GraphicsSheetBrowserPanel sheet(NoOpDraw);
  EXPECT_EQ(sheet.GetId(), "graphics.sheet_browser_v2");

  GraphicsPixelEditorPanel pixel(NoOpDraw);
  EXPECT_EQ(pixel.GetId(), "graphics.pixel_editor");

  GraphicsPaletteControlsPanel palette(NoOpDraw);
  EXPECT_EQ(palette.GetId(), "graphics.palette_controls");

  GraphicsLinkSpritePanel link(NoOpDraw);
  EXPECT_EQ(link.GetId(), "graphics.link_sprite_editor");

  GraphicsGfxGroupPanel groups(NoOpDraw);
  EXPECT_EQ(groups.GetId(), "graphics.gfx_group_editor");

  GraphicsPalettesetPanel palettesets(NoOpDraw);
  EXPECT_EQ(palettesets.GetId(), "graphics.paletteset_editor");

  GraphicsPrototypeViewerPanel proto(NoOpDraw);
  EXPECT_EQ(proto.GetId(), "graphics.prototype_viewer");

  GraphicsPolyhedralPanel polyhedral(NoOpDraw);
  EXPECT_EQ(polyhedral.GetId(), "graphics.polyhedral");
}

TEST(GraphicsEditorWindowIds, PolyhedralPanelMatchesWrapperId) {
  PolyhedralEditorPanel panel(nullptr);
  EXPECT_EQ(panel.GetId(), "graphics.polyhedral");
}

}  // namespace yaze::editor
