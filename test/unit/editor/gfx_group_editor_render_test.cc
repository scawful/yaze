#include "app/editor/graphics/gfx_group_editor_internal.h"

#include <cstdint>
#include <vector>

#include "app/gfx/core/bitmap.h"
#include "app/gfx/resource/arena.h"
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

}  // namespace
}  // namespace yaze::editor
