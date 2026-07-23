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

  EXPECT_FALSE(WindowBackendFactory::IsAvailable(WindowBackendType::GLFW));
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

TEST(WindowBackendFactoryTest, CreateGlfwFallsBackToDefault) {
  auto glfw_backend = WindowBackendFactory::Create(WindowBackendType::GLFW);
  auto default_backend = WindowBackendFactory::Create(WindowBackendType::Auto);
  ASSERT_NE(glfw_backend, nullptr);
  ASSERT_NE(default_backend, nullptr);
  EXPECT_EQ(glfw_backend->GetBackendName(), default_backend->GetBackendName());
}

TEST(WindowBackendFactoryTest, NativeQuitEventDoesNotDeactivateBackend) {
  const bool initialized_events = SDL_WasInit(SDL_INIT_EVENTS) != 0;
#ifdef YAZE_USE_SDL3
  if (!initialized_events && !SDL_InitSubSystem(SDL_INIT_EVENTS)) {
    GTEST_SKIP() << "SDL event subsystem unavailable: " << SDL_GetError();
  }
#else
  if (!initialized_events && SDL_InitSubSystem(SDL_INIT_EVENTS) != 0) {
    GTEST_SKIP() << "SDL event subsystem unavailable: " << SDL_GetError();
  }
#endif

  auto backend = WindowBackendFactory::Create(WindowBackendType::Auto);
  ASSERT_NE(backend, nullptr);
  backend->SetActive(true);

  SDL_Event native_event{};
#ifdef YAZE_USE_SDL3
  native_event.type = SDL_EVENT_QUIT;
  ASSERT_TRUE(SDL_PushEvent(&native_event)) << SDL_GetError();
#else
  native_event.type = SDL_QUIT;
  ASSERT_EQ(SDL_PushEvent(&native_event), 1) << SDL_GetError();
#endif

  WindowEvent event;
  ASSERT_TRUE(backend->PollEvent(event));
  EXPECT_EQ(event.type, WindowEventType::Quit);
  EXPECT_TRUE(backend->IsActive());

  backend.reset();
  if (!initialized_events) {
    SDL_QuitSubSystem(SDL_INIT_EVENTS);
  }
}

}  // namespace test
}  // namespace platform
}  // namespace yaze
