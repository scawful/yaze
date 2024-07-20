#if defined(_WIN32)
#define main SDL_main
#elif __APPLE__
#include "app/core/platform/app_delegate.h"
#endif

#include <SDL.h>
#include <absl/status/status.h>
#include <absl/strings/str_format.h>
#include <failure_signal_handler.h>
#include <imgui/imgui.h>
#include <imgui_memory_editor.h>
#include <rom.h>

#include <memory>
#include <string>
#include <vector>

#include "absl/base/internal/raw_logging.h"
#include "absl/base/macros.h"
#include "absl/container/flat_hash_map.h"
#include "absl/debugging/failure_signal_handler.h"
#include "absl/debugging/leak_check.h"
#include "absl/debugging/stacktrace.h"
#include "absl/debugging/symbolize.h"
#include "absl/flags/flag.h"
#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "absl/strings/str_format.h"
#include "app/emu/snes.h"
#include "app/rom.h"

using namespace yaze::app;

struct sdl_deleter {
  void operator()(SDL_Window *p) const {
    if (p) {
      SDL_DestroyWindow(p);
    }
  }
  void operator()(SDL_Renderer *p) const {
    if (p) {
      SDL_DestroyRenderer(p);
    }
  }
  void operator()(SDL_Texture *p) const { SDL_DestroyTexture(p); }
};

int main(int argc, char **argv) {
  absl::InitializeSymbolizer(argv[0]);

  absl::FailureSignalHandlerOptions options;
  options.symbolize_stacktrace = true;
  options.alarm_on_failure_secs = true;
  absl::InstallFailureSignalHandler(options);

  std::unique_ptr<SDL_Window, sdl_deleter> window_;
  std::unique_ptr<SDL_Renderer, sdl_deleter> renderer_;
  if (SDL_Init(SDL_INIT_EVERYTHING) != 0) {
    return EXIT_FAILURE;
  } else {
    SDL_DisplayMode displayMode;
    SDL_GetCurrentDisplayMode(0, &displayMode);
    int screenWidth = displayMode.w * 0.8;
    int screenHeight = displayMode.h * 0.8;

    window_ = std::unique_ptr<SDL_Window, sdl_deleter>(
        SDL_CreateWindow("Yaze Emulator",          // window title
                         SDL_WINDOWPOS_UNDEFINED,  // initial x position
                         SDL_WINDOWPOS_UNDEFINED,  // initial y position
                         512,                      // width, in pixels
                         480,                      // height, in pixels
                         SDL_WINDOW_RESIZABLE | SDL_WINDOW_ALLOW_HIGHDPI),
        sdl_deleter());
    if (window_ == nullptr) {
      return EXIT_FAILURE;
    }
  }

  renderer_ = std::unique_ptr<SDL_Renderer, sdl_deleter>(
      SDL_CreateRenderer(window_.get(), -1,
                         SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC),
      sdl_deleter());
  if (renderer_ == nullptr) {
    return EXIT_FAILURE;
  } else {
    SDL_SetRenderDrawBlendMode(renderer_.get(), SDL_BLENDMODE_BLEND);
    SDL_SetRenderDrawColor(renderer_.get(), 0x00, 0x00, 0x00, 0x00);
  }

  int audio_frequency_ = 48000;
  SDL_AudioSpec want, have;
  SDL_memset(&want, 0, sizeof(want));
  want.freq = audio_frequency_;
  want.format = AUDIO_S16;
  want.channels = 2;
  want.samples = 2048;
  want.callback = NULL;  // Uses the queue
  auto audio_device_ = SDL_OpenAudioDevice(NULL, 0, &want, &have, 0);
  if (audio_device_ == 0) {
    return EXIT_FAILURE;
  }
  auto audio_buffer_ = new int16_t[audio_frequency_ / 50 * 4];
  SDL_PauseAudioDevice(audio_device_, 0);

#ifdef __APPLE__
  InitializeCocoa();
#endif

  auto ppu_texture_ =
      SDL_CreateTexture(renderer_.get(), SDL_PIXELFORMAT_RGBX8888,
                        SDL_TEXTUREACCESS_STREAMING, 512, 480);
  if (ppu_texture_ == NULL) {
    printf("Failed to create texture: %s\n", SDL_GetError());
    return EXIT_FAILURE;
  }

  Rom rom_;
  emu::SNES snes_;
  Bytes rom_data_;

  bool running = true;
  bool loaded = false;
  auto count_frequency = SDL_GetPerformanceFrequency();
  auto last_count = SDL_GetPerformanceCounter();
  auto time_adder = 0.0;
  int wanted_frames_ = 0;
  int wanted_samples_ = 0;
  SDL_Event event;

  rom_.LoadFromFile("inidisp_hammer_0f00.sfc");
  if (rom_.is_loaded()) {
    rom_data_ = rom_.vector();
    snes_.Init(rom_data_);
    wanted_frames_ = 1.0 / (snes_.Memory().pal_timing() ? 50.0 : 60.0);
    wanted_samples_ = 48000 / (snes_.Memory().pal_timing() ? 50 : 60);
    loaded = true;
  }

  while (running) {
    while (SDL_PollEvent(&event)) {
      switch (event.type) {
        case SDL_DROPFILE:
          rom_.LoadFromFile(event.drop.file);
          if (rom_.is_loaded()) {
            rom_data_ = rom_.vector();
            snes_.Init(rom_data_);
            wanted_frames_ = 1.0 / (snes_.Memory().pal_timing() ? 50.0 : 60.0);
            wanted_samples_ = 48000 / (snes_.Memory().pal_timing() ? 50 : 60);
            loaded = true;
          }
          SDL_free(event.drop.file);
          break;
        case SDL_KEYDOWN:
          break;
        case SDL_KEYUP:
          break;
        case SDL_WINDOWEVENT:
          switch (event.window.event) {
            case SDL_WINDOWEVENT_CLOSE:
              running = false;
              break;
            case SDL_WINDOWEVENT_SIZE_CHANGED:
              break;
            default:
              break;
          }
          break;
        default:
          break;
      }
    }

    uint64_t current_count = SDL_GetPerformanceCounter();
    uint64_t delta = current_count - last_count;
    last_count = current_count;
    float seconds = delta / (float)count_frequency;
    time_adder += seconds;
    // allow 2 ms earlier, to prevent skipping due to being just below wanted
    while (time_adder >= wanted_frames_ - 0.002) {
      time_adder -= wanted_frames_;

      if (loaded) {
        snes_.RunFrame();

        snes_.SetSamples(audio_buffer_, wanted_samples_);
        if (SDL_GetQueuedAudioSize(audio_device_) <= wanted_samples_ * 4 * 6) {
          SDL_QueueAudio(audio_device_, audio_buffer_, wanted_samples_ * 4);
        }

        void *ppu_pixels_;
        int ppu_pitch_;
        if (SDL_LockTexture(ppu_texture_, NULL, &ppu_pixels_, &ppu_pitch_) !=
            0) {
          printf("Failed to lock texture: %s\n", SDL_GetError());
          return EXIT_FAILURE;
        }
        snes_.SetPixels(static_cast<uint8_t *>(ppu_pixels_));
        SDL_UnlockTexture(ppu_texture_);
      }
    }

    SDL_RenderClear(renderer_.get());
    SDL_RenderCopy(renderer_.get(), ppu_texture_, NULL, NULL);
    SDL_RenderPresent(renderer_.get());  // should vsync
  }

  SDL_PauseAudioDevice(audio_device_, 1);
  SDL_CloseAudioDevice(audio_device_);
  delete audio_buffer_;
  // ImGui_ImplSDLRenderer2_Shutdown();
  // ImGui_ImplSDL2_Shutdown();
  // ImGui::DestroyContext();
  SDL_Quit();

  return EXIT_SUCCESS;
}