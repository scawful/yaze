#ifndef YAZE_CORE_WINDOW_H_
#define YAZE_CORE_WINDOW_H_

#include <SDL.h>

#include <memory>

#include "absl/status/status.h"
#include "absl/strings/str_format.h"
#include "app/gfx/backend/irenderer.h"
#include "app/gfx/core/bitmap.h"
#include "util/sdl_deleter.h"

namespace yaze {
namespace core {

struct Window {
  std::shared_ptr<SDL_Window> window_;
  SDL_AudioDeviceID audio_device_;
  std::shared_ptr<int16_t> audio_buffer_;
  bool active_ = true;
};

// Legacy CreateWindow (deprecated - use Controller::OnEntry instead)
// Kept for backward compatibility with test code
absl::Status CreateWindow(Window& window, gfx::IRenderer* renderer = nullptr,
                          int flags = SDL_WINDOW_RESIZABLE);
absl::Status HandleEvents(Window& window);
absl::Status ShutdownWindow(Window& window);

}  // namespace core
}  // namespace yaze
#endif  // YAZE_CORE_WINDOW_H_