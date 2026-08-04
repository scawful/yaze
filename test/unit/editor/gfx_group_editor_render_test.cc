#include "app/editor/graphics/gfx_group_editor_internal.h"

#include <cstdint>
#include <vector>

#include "app/editor/graphics/screen_editor_internal.h"
#include "app/gfx/core/bitmap.h"
#include "app/gfx/resource/arena.h"
#include "framework/mock_renderer.h"
#include "gmock/gmock.h"
#include "gtest/gtest.h"

namespace yaze::editor {
namespace {

constexpr int kSheetWidth = 128;
constexpr int kSheetHeight = 32;
constexpr int kSheetDepth = 8;

gfx::Bitmap MakeSheetBitmap() {
  gfx::Bitmap bmp;
  std::vector<uint8_t> pixels(kSheetWidth * kSheetHeight, 0);
  bmp.Create(kSheetWidth, kSheetHeight, kSheetDepth, pixels);
  return bmp;
}

class GfxGroupEditorRenderTest : public ::testing::Test {
 protected:
  void SetUp() override { gfx::Arena::Get().ClearTextureQueue(); }
  void TearDown() override { gfx::Arena::Get().ClearTextureQueue(); }
};

TEST_F(GfxGroupEditorRenderTest, EmptyBitmapIsNoOp) {
  gfx::Bitmap empty;  // No surface, no texture.

  internal::EnsureSheetTextureQueued(empty);

  EXPECT_EQ(gfx::Arena::Get().texture_command_queue_size(), 0u);
  EXPECT_FALSE(empty.is_active());
}

TEST_F(GfxGroupEditorRenderTest, TexturelessSheetGetsCreateQueued) {
  gfx::Bitmap sheet = MakeSheetBitmap();
  ASSERT_NE(sheet.surface(), nullptr)
      << "Bitmap::Create should allocate surface";
  ASSERT_EQ(sheet.texture(), nullptr)
      << "No renderer is wired in unit tests, so texture must remain null";

  const size_t before = gfx::Arena::Get().texture_command_queue_size();
  internal::EnsureSheetTextureQueued(sheet);
  const size_t after = gfx::Arena::Get().texture_command_queue_size();

  EXPECT_EQ(after, before + 1)
      << "Helper must queue exactly one CREATE for a textureless sheet";
  EXPECT_TRUE(sheet.is_active());
  EXPECT_EQ(sheet.metadata().purpose, gfx::Bitmap::BitmapPurpose::kPreview)
      << "Gfx-group viewer sheets are read-only previews; the helper must "
         "stamp purpose so consumers can tell preview bitmaps from editable "
         "ones without consulting the caller.";
}

TEST_F(GfxGroupEditorRenderTest, RepeatedCallsKeepQueuingWhileTextureMissing) {
  // The Arena drains commands in ProcessTextureQueue; until that runs the
  // sheet still has no texture, so calling the helper again still queues.
  // This is acceptable: ProcessTextureQueue is idempotent on duplicate
  // CREATE entries for the same bitmap.
  gfx::Bitmap sheet = MakeSheetBitmap();

  internal::EnsureSheetTextureQueued(sheet);
  internal::EnsureSheetTextureQueued(sheet);

  EXPECT_EQ(gfx::Arena::Get().texture_command_queue_size(), 2u);
}

TEST_F(GfxGroupEditorRenderTest, InactiveButSurfacedSheetGetsActivated) {
  gfx::Bitmap sheet = MakeSheetBitmap();
  sheet.set_active(false);  // Simulate a sheet that lost its active flag.
  ASSERT_NE(sheet.surface(), nullptr);

  internal::EnsureSheetTextureQueued(sheet);

  EXPECT_TRUE(sheet.is_active());
  EXPECT_EQ(gfx::Arena::Get().texture_command_queue_size(), 1u);
}

TEST_F(GfxGroupEditorRenderTest,
       BitmapRecreationInvalidatesOldCommandsAndReplacesTextureInOrder) {
  ::testing::NiceMock<yaze::test::MockRenderer> renderer;
  gfx::Bitmap bitmap = MakeSheetBitmap();
  int old_texture_storage = 0;
  int new_texture_storage = 0;
  auto old_texture = reinterpret_cast<gfx::TextureHandle>(&old_texture_storage);
  auto new_texture = reinterpret_cast<gfx::TextureHandle>(&new_texture_storage);

  EXPECT_CALL(renderer, CreateTexture(kSheetWidth, kSheetHeight))
      .WillOnce(::testing::Return(old_texture));
  EXPECT_CALL(renderer, UpdateTexture(old_texture, ::testing::Ref(bitmap)));
  bitmap.CreateTexture();
  gfx::Arena::Get().ProcessTextureQueue(&renderer);
  ASSERT_EQ(bitmap.texture(), old_texture);
  ASSERT_FALSE(gfx::Arena::Get().HasPendingTextures());
  ASSERT_TRUE(::testing::Mock::VerifyAndClearExpectations(&renderer));

  const uint32_t old_generation = bitmap.generation();
  bitmap.UpdateTexture();  // Must become stale when Create() replaces data.
  std::vector<uint8_t> replacement_pixels(kSheetWidth * kSheetHeight, 7);
  bitmap.Create(kSheetWidth, kSheetHeight, kSheetDepth, replacement_pixels);
  bitmap.CreateTexture();

  EXPECT_NE(bitmap.generation(), old_generation);
  ASSERT_EQ(bitmap.texture(), old_texture)
      << "The old handle must remain available to deferred DESTROY";

  {
    ::testing::InSequence sequence;
    EXPECT_CALL(renderer, DestroyTexture(old_texture));
    EXPECT_CALL(renderer, CreateTexture(kSheetWidth, kSheetHeight))
        .WillOnce(::testing::Return(new_texture));
    EXPECT_CALL(renderer, UpdateTexture(new_texture, ::testing::Ref(bitmap)));
  }

  // Processing the real deferred queue (rather than inspecting it) makes this
  // an ASan-friendly lifetime regression: stale UPDATE is discarded, then the
  // retained Bitmap owner receives DESTROY followed by replacement CREATE.
  gfx::Arena::Get().ProcessTextureQueue(&renderer);

  EXPECT_FALSE(gfx::Arena::Get().HasPendingTextures());
  EXPECT_EQ(bitmap.texture(), new_texture);
  EXPECT_EQ(bitmap.vector().front(), 7);
}

// EnsureCompositeBitmapTextureQueued mirrors EnsureSheetTextureQueued but
// stamps purpose=kCompositeOutput and additionally honors modified() to queue
// UPDATE for re-rendered composites. These tests pin the truth table that
// fixes the A3/A4 first-frame composite race; if the helper changes shape,
// these tests force the consumer to be re-validated.
constexpr int kCompositeWidth = 256;
constexpr int kCompositeHeight = 256;

gfx::Bitmap MakeCompositeBitmap() {
  gfx::Bitmap bmp;
  std::vector<uint8_t> pixels(kCompositeWidth * kCompositeHeight, 0);
  bmp.Create(kCompositeWidth, kCompositeHeight, 8, pixels);
  return bmp;
}

TEST_F(GfxGroupEditorRenderTest, EnsureCompositeBitmapEmptyIsNoOp) {
  gfx::Bitmap empty;  // No surface (TitleScreen::Create hasn't run yet).

  internal::EnsureCompositeBitmapTextureQueued(empty);

  EXPECT_EQ(gfx::Arena::Get().texture_command_queue_size(), 0u);
  EXPECT_FALSE(empty.is_active());
}

TEST_F(GfxGroupEditorRenderTest,
       EnsureCompositeBitmapQueuesCreateOnFirstSight) {
  gfx::Bitmap composite = MakeCompositeBitmap();
  ASSERT_NE(composite.surface(), nullptr);
  ASSERT_EQ(composite.texture(), nullptr);

  internal::EnsureCompositeBitmapTextureQueued(composite);

  EXPECT_EQ(gfx::Arena::Get().texture_command_queue_size(), 1u)
      << "First call on a textureless composite must queue exactly one CREATE";
  EXPECT_TRUE(composite.is_active());
  EXPECT_EQ(composite.metadata().purpose,
            gfx::Bitmap::BitmapPurpose::kCompositeOutput)
      << "Composites must be stamped as kCompositeOutput so canvas_rendering's "
         "diagnostic log identifies them as composite outputs (room renderer, "
         "title screen layer composite) rather than editable scratchpads.";
  EXPECT_FALSE(composite.modified())
      << "Helper clears modified after queueing CREATE so the next frame's "
         "modified() check doesn't re-queue UPDATE for the same pixels.";
}

TEST_F(GfxGroupEditorRenderTest,
       EnsureCompositeBitmapInactiveButSurfacedGetsActivated) {
  gfx::Bitmap composite = MakeCompositeBitmap();
  composite.set_active(false);
  ASSERT_NE(composite.surface(), nullptr);

  internal::EnsureCompositeBitmapTextureQueued(composite);

  EXPECT_TRUE(composite.is_active());
  EXPECT_EQ(gfx::Arena::Get().texture_command_queue_size(), 1u);
}

TEST_F(GfxGroupEditorRenderTest,
       EnsureCompositeBitmapPinsPurposeWithoutQueueingTwice) {
  // Repeated calls before the queue drains keep queueing CREATE (the Arena's
  // CREATE branch is idempotent on duplicates), but each call still stamps
  // purpose. This is the slice-1 contract; slice-7 inherits it.
  gfx::Bitmap composite = MakeCompositeBitmap();

  internal::EnsureCompositeBitmapTextureQueued(composite);
  internal::EnsureCompositeBitmapTextureQueued(composite);

  EXPECT_EQ(gfx::Arena::Get().texture_command_queue_size(), 2u);
  EXPECT_EQ(composite.metadata().purpose,
            gfx::Bitmap::BitmapPurpose::kCompositeOutput);
}

}  // namespace
}  // namespace yaze::editor
