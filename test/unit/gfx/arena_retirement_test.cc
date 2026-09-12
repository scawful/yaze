#include "app/gfx/resource/arena.h"

#include <stdexcept>
#include <vector>

#include "framework/mock_renderer.h"
#include "gmock/gmock.h"
#include "gtest/gtest.h"

namespace yaze::gfx {
namespace {

using ::testing::NiceMock;

size_t ActiveSurfaceCount(const Arena& arena) {
  return arena.GetSurfaceCount() - arena.GetPooledSurfaceCount();
}

TEST(ArenaRetirementTest,
     DetachesSurfaceCancelsCommandsAndDefersOpaqueHandleDestruction) {
  Arena& arena = Arena::Get();
  arena.ClearTextureQueue();
  const size_t active_surfaces_before = ActiveSurfaceCount(arena);

  Bitmap retiring_bitmap(8, 8, 8, std::vector<uint8_t>(64, 1));
  Bitmap surviving_bitmap(8, 8, 8, std::vector<uint8_t>(64, 2));
  ASSERT_EQ(ActiveSurfaceCount(arena), active_surfaces_before + 2);
  int texture_storage = 0;
  const TextureHandle texture = &texture_storage;
  retiring_bitmap.set_texture(texture);

  arena.QueueTextureCommand(Arena::TextureCommandType::CREATE,
                            &retiring_bitmap);
  arena.QueueTextureCommand(Arena::TextureCommandType::UPDATE,
                            &retiring_bitmap);
  arena.QueueTextureCommand(Arena::TextureCommandType::DESTROY,
                            &retiring_bitmap);
  arena.QueueTextureCommand(Arena::TextureCommandType::UPDATE,
                            &surviving_bitmap);
  ASSERT_EQ(arena.texture_command_queue_size(), 4u);

  arena.RetireBitmap(retiring_bitmap);

  EXPECT_EQ(retiring_bitmap.surface(), nullptr);
  EXPECT_EQ(retiring_bitmap.texture(), nullptr);
  EXPECT_FALSE(retiring_bitmap.is_active());
  EXPECT_EQ(ActiveSurfaceCount(arena), active_surfaces_before + 1);
  EXPECT_EQ(arena.texture_command_queue_size(), 1u)
      << "Only commands for the retired Bitmap address should be removed";
  EXPECT_EQ(arena.retired_texture_handle_count(), 1u);

  arena.RetireBitmap(retiring_bitmap);
  EXPECT_EQ(ActiveSurfaceCount(arena), active_surfaces_before + 1);
  EXPECT_EQ(arena.texture_command_queue_size(), 1u);
  EXPECT_EQ(arena.retired_texture_handle_count(), 1u)
      << "Retiring the same detached Bitmap twice must be a no-op";

  NiceMock<test::MockRenderer> renderer;
  EXPECT_CALL(renderer, DestroyTexture(texture)).Times(1);
  EXPECT_EQ(arena.DrainRetiredBitmaps(&renderer), 1u);
  EXPECT_EQ(arena.retired_texture_handle_count(), 0u);

  arena.RetireBitmap(surviving_bitmap);
  EXPECT_EQ(arena.texture_command_queue_size(), 0u);
  EXPECT_EQ(ActiveSurfaceCount(arena), active_surfaces_before);
}

TEST(ArenaRetirementTest, FailedHandleDestroyRemainsQueuedForRetry) {
  Arena& arena = Arena::Get();
  arena.ClearTextureQueue();

  Bitmap bitmap(8, 8, 8, std::vector<uint8_t>(64, 1));
  int texture_storage = 0;
  const TextureHandle texture = &texture_storage;
  bitmap.set_texture(texture);
  arena.RetireBitmap(bitmap);

  NiceMock<test::MockRenderer> renderer;
  EXPECT_CALL(renderer, DestroyTexture(texture))
      .WillOnce(::testing::Throw(std::runtime_error("destroy failed")))
      .WillOnce(::testing::Return());

  EXPECT_EQ(arena.DrainRetiredBitmaps(&renderer), 0u);
  EXPECT_EQ(arena.retired_texture_handle_count(), 1u);
  EXPECT_EQ(arena.DrainRetiredBitmaps(&renderer), 1u);
  EXPECT_EQ(arena.retired_texture_handle_count(), 0u);
}

}  // namespace
}  // namespace yaze::gfx
