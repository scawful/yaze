#include "app/editor/sprite/sprite_editor_internal.h"

#include <cstdint>
#include <vector>

#include "app/gfx/core/bitmap.h"
#include "app/gfx/resource/arena.h"
#include "gtest/gtest.h"

namespace yaze::editor {
namespace {

constexpr int kVanillaWidth = 128;
constexpr int kVanillaHeight = 128;
constexpr int kSpriteDepth = 8;

class SpriteEditorPreviewTest : public ::testing::Test {
 protected:
  void SetUp() override { gfx::Arena::Get().ClearTextureQueue(); }
  void TearDown() override { gfx::Arena::Get().ClearTextureQueue(); }
};

TEST_F(SpriteEditorPreviewTest, FirstCallCreatesActiveBitmapAndQueuesCreate) {
  gfx::Bitmap bmp;  // Default: surface=nullptr, texture=nullptr, !is_active().
  std::vector<uint8_t> buffer(kVanillaWidth * kVanillaHeight, 0);

  const size_t before = gfx::Arena::Get().texture_command_queue_size();
  internal::EnsureSpritePreviewBitmapReady(bmp, kVanillaWidth, kVanillaHeight,
                                           kSpriteDepth, buffer);
  const size_t after = gfx::Arena::Get().texture_command_queue_size();

  EXPECT_TRUE(bmp.is_active());
  EXPECT_NE(bmp.surface(), nullptr);
  EXPECT_EQ(after, before + 1)
      << "First call must queue exactly one CREATE for the texture; "
         "without this, canvas_rendering silently skips the draw.";
  EXPECT_EQ(bmp.metadata().purpose,
            gfx::Bitmap::BitmapPurpose::kCompositeOutput)
      << "Sprite preview bitmaps are post-render composites (pixels come "
         "from SpriteDrawer, not direct user paint). Stamping the purpose "
         "lets consumers tell composite outputs apart from editable "
         "scratchpads without consulting caller convention.";
}

TEST_F(SpriteEditorPreviewTest, RepeatedCallIsNoOpForActiveBitmap) {
  gfx::Bitmap bmp;
  std::vector<uint8_t> buffer(kVanillaWidth * kVanillaHeight, 0);
  internal::EnsureSpritePreviewBitmapReady(bmp, kVanillaWidth, kVanillaHeight,
                                           kSpriteDepth, buffer);
  const size_t after_first = gfx::Arena::Get().texture_command_queue_size();

  internal::EnsureSpritePreviewBitmapReady(bmp, kVanillaWidth, kVanillaHeight,
                                           kSpriteDepth, buffer);

  EXPECT_EQ(gfx::Arena::Get().texture_command_queue_size(), after_first)
      << "Subsequent calls on an already-active bitmap must short-circuit "
         "and not re-queue CREATE.";
  EXPECT_TRUE(bmp.is_active());
}

TEST_F(SpriteEditorPreviewTest, EmptyBufferLeavesBitmapInactive) {
  // Bitmap::Create with empty data sets active_=false and bails out.
  // The helper must not stamp purpose or queue CREATE when init failed.
  gfx::Bitmap bmp;
  std::vector<uint8_t> empty;

  const size_t before = gfx::Arena::Get().texture_command_queue_size();
  internal::EnsureSpritePreviewBitmapReady(bmp, kVanillaWidth, kVanillaHeight,
                                           kSpriteDepth, empty);

  EXPECT_FALSE(bmp.is_active());
  EXPECT_EQ(bmp.texture(), nullptr);
  EXPECT_EQ(gfx::Arena::Get().texture_command_queue_size(), before)
      << "An inactive bitmap (failed Create) must not queue CREATE; the "
         "Arena would otherwise process a CREATE for a bitmap with no "
         "surface and silently fail again later.";
}

}  // namespace
}  // namespace yaze::editor
