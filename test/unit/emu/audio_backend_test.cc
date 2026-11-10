#include "app/emu/audio/audio_backend.h"

#include <gtest/gtest.h>

#include "app/platform/sdl_compat.h"

namespace yaze::emu::audio {
namespace {

TEST(AudioBackendTest, NullBackendReportsNoError) {
  NullAudioBackend backend;
  ASSERT_TRUE(backend.Initialize(AudioConfig{}));
  EXPECT_TRUE(backend.GetLastError().empty());
}

#ifndef YAZE_USE_SDL3
TEST(AudioBackendTest, Sdl2BackendKeepsReasonWhenDeviceDoesNotOpen) {
  // SDL refuses to open a device while its audio subsystem is down, which
  // stands in for a build or host with no usable audio driver.
  SDL_QuitSubSystem(SDL_INIT_AUDIO);

  SDL2AudioBackend backend;
  EXPECT_FALSE(backend.Initialize(AudioConfig{}));
  EXPECT_FALSE(backend.IsInitialized());
  EXPECT_FALSE(backend.GetLastError().empty());
}
#endif

}  // namespace
}  // namespace yaze::emu::audio
