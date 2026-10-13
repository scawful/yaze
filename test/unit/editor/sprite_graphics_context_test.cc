#include "app/editor/sprite/sprite_graphics_context.h"

#include "app/editor/sprite/zsprite.h"
#include "gtest/gtest.h"
#include "zelda3/game_data.h"

namespace yaze::editor {
namespace {

gfx::PaletteGroup TestPalettes() {
  gfx::PaletteGroup group;
  for (int row = 0; row < 8; ++row) {
    gfx::SnesPalette palette;
    for (int color = 0; color < 16; ++color) {
      palette.AddColor(gfx::SnesColor(row * 16 + color, 0, 0));
    }
    group.AddPalette(palette);
  }
  return group;
}

TEST(SpriteGraphicsContextTest, SelectedOamUsesMappedSlotAndExactPalette) {
  zelda3::GameData data;
  data.graphics_buffer.assign(223 * 4096, 0);
  std::array<uint8_t, 8> slots{0x73, 0x74, 0x79, 0x7A, 0x83, 0x84, 0x85, 0x86};
  zsprite::Frame frame;
  frame.Tiles.emplace_back(128, 112, false, false, 0x100, 1, false, 3);
  frame.Tiles.emplace_back(128, 112, false, false, 0x13F, 5, true, 3);
  auto result = sprite_graphics_context::Make(frame, 7, 1, slots,
                                              TestPalettes(), &data, "Pose");
  ASSERT_TRUE(result.ok()) << result.status();
  ASSERT_EQ(result->targets.size(), 4);
  EXPECT_EQ(result->owner, &data);
  EXPECT_EQ(result->targets[0].sheet_id, 0x83);
  EXPECT_EQ(result->targets[0].tile_index, 63);
  EXPECT_EQ(result->targets[1].sheet_id, 0x84);
  EXPECT_EQ(result->targets[1].tile_index, 0);
  EXPECT_EQ(result->targets[2].sheet_id, 0x84);
  EXPECT_EQ(result->targets[2].tile_index, 15);
  EXPECT_EQ(result->targets[0].palette[1].rgb().x, 81);
  EXPECT_NE(result->label.find("frame 7 / OAM 1"), std::string::npos);
}

TEST(SpriteGraphicsContextTest, RejectsOutOfBoundsAndUnmappedSources) {
  zelda3::GameData data;
  data.graphics_buffer.assign(223 * 4096, 0);
  std::array<uint8_t, 8> slots{0x73, 0x74, 0x79, 0x7A, 0x83, 0x84, 0x85, 0x86};
  zsprite::Frame frame;
  frame.Tiles.emplace_back(128, 112, false, false, 0x1FF, 0, true, 3);
  EXPECT_FALSE(sprite_graphics_context::Make(frame, 0, 0, slots, TestPalettes(),
                                             &data, "Pose")
                   .ok());
  frame.Tiles[0].id = 0x100;
  slots[4] = 218;  // 2bpp storage cannot be edited in a 3bpp context.
  EXPECT_FALSE(sprite_graphics_context::Make(frame, 0, 0, slots, TestPalettes(),
                                             &data, "Pose")
                   .ok());
  slots[4] = 0x83;
  EXPECT_FALSE(sprite_graphics_context::Make(frame, 0, 1, slots, TestPalettes(),
                                             &data, "Pose")
                   .ok());
}

TEST(SpriteGraphicsContextTest, CapturedFrameRendersLiveOwningSessionPixels) {
  zelda3::GameData first;
  zelda3::GameData other;
  first.graphics_buffer.assign(223 * 4096, 0);
  other.graphics_buffer.assign(223 * 4096, 0);
  std::array<uint8_t, 8> slots{0x73, 0x74, 0x79, 0x7A, 0x83, 0x84, 0x85, 0x86};
  zsprite::Frame frame;
  frame.Tiles.emplace_back(128, 112, false, false, 0x100, 2, false, 3);
  const size_t pixel = 128 * 256 + 128;
  first.graphics_buffer[0x83 * 4096] = 3;
  other.graphics_buffer[0x83 * 4096] = 6;
  auto first_image = sprite_graphics_context::RenderCapturedFrame(
      frame, slots, TestPalettes(), &first);
  auto other_image = sprite_graphics_context::RenderCapturedFrame(
      frame, slots, TestPalettes(), &other);
  ASSERT_TRUE(first_image.ok()) << first_image.status();
  ASSERT_TRUE(other_image.ok()) << other_image.status();
  EXPECT_EQ(first_image->pixels[pixel], 35);
  EXPECT_EQ(other_image->pixels[pixel], 38);
  first.graphics_buffer[0x83 * 4096] = 5;
  auto edited = sprite_graphics_context::RenderCapturedFrame(
      frame, slots, TestPalettes(), &first);
  ASSERT_TRUE(edited.ok()) << edited.status();
  EXPECT_EQ(edited->pixels[pixel], 37);
  EXPECT_EQ(other_image->pixels[pixel], 38);
}

TEST(SpriteGraphicsContextTest, UncommittedStrokeAndSelectedFrameStayIsolated) {
  zelda3::GameData data;
  data.graphics_buffer.assign(223 * 4096, 0);
  std::array<uint8_t, 8> slots{0x73, 0x74, 0x79, 0x7A, 0x83, 0x84, 0x85, 0x86};
  zsprite::Frame first_frame;
  first_frame.Tiles.emplace_back(128, 112, false, false, 0x100, 1, false, 3);
  zsprite::Frame second_frame;
  second_frame.Tiles.emplace_back(128, 112, false, false, 0x101, 3, false, 3);
  zelda3::GraphicsSheetStore::Edit stroke(data.sheet_store, 0x83);
  ASSERT_TRUE(stroke.ok());
  ASSERT_TRUE(stroke.Set(0, 0, 2));
  ASSERT_TRUE(stroke.Set(8, 0, 6));
  const size_t pixel = 128 * 256 + 128;
  auto first = sprite_graphics_context::RenderCapturedFrame(
      first_frame, slots, TestPalettes(), &data);
  auto second = sprite_graphics_context::RenderCapturedFrame(
      second_frame, slots, TestPalettes(), &data);
  ASSERT_TRUE(first.ok()) << first.status();
  ASSERT_TRUE(second.ok()) << second.status();
  EXPECT_EQ(first->pixels[pixel], 18);
  EXPECT_EQ(second->pixels[pixel], 54);
  EXPECT_EQ(data.sheet_store.Revision(0x83), 0);
  auto selected = sprite_graphics_context::Make(second_frame, 1, 0, slots,
                                                TestPalettes(), &data, "Walk");
  ASSERT_TRUE(selected.ok()) << selected.status();
  EXPECT_NE(selected->label.find("frame 1"), std::string::npos);
  EXPECT_EQ(selected->targets[0].tile_index, 1);
  EXPECT_EQ(selected->targets[0].palette[6].rgb().x, 54);
}

}  // namespace
}  // namespace yaze::editor
