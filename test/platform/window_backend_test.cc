#include <gtest/gtest.h>
#include "app/platform/iwindow.h"

namespace yaze {
namespace platform {
namespace test {

TEST(WindowBackendFactoryTest, DefaultTypeRespectsBuildFlag) {
  WindowBackendType default_type = WindowBackendFactory::GetDefaultType();

#ifdef YAZE_USE_SDL3
  EXPECT_EQ(default_type, WindowBackendType::SDL3);
  EXPECT_TRUE(WindowBackendFactory::IsAvailable(WindowBackendType::SDL3));
  EXPECT_FALSE(WindowBackendFactory::IsAvailable(WindowBackendType::SDL2));
#else
  EXPECT_EQ(default_type, WindowBackendType::SDL2);
  EXPECT_TRUE(WindowBackendFactory::IsAvailable(WindowBackendType::SDL2));
  EXPECT_FALSE(WindowBackendFactory::IsAvailable(WindowBackendType::SDL3));
#endif
}

TEST(WindowBackendFactoryTest, CreateAutoReturnsDefault) {
  auto backend = WindowBackendFactory::Create(WindowBackendType::Auto);
  ASSERT_NE(backend, nullptr);
  
#ifdef YAZE_USE_SDL3
  EXPECT_EQ(backend->GetSDLVersion(), 3);
#else
  // SDL2 version check might vary, but shouldn't be 3
  // Typically SDL2 returns 2xxx
  EXPECT_NE(backend->GetSDLVersion(), 3);
#endif
}

}  // namespace test
}  // namespace platform
}  // namespace yaze
