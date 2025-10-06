#if __APPLE__
#include "app/core/platform/app_delegate.h"
#endif

#include <SDL.h>

#include <memory>
#include <string>
#include <vector>

#include "absl/debugging/failure_signal_handler.h"
#include "absl/debugging/symbolize.h"
#include "absl/flags/flag.h"
#include "absl/flags/parse.h"
#include "app/emu/snes.h"
#include "app/rom.h"
#include "util/sdl_deleter.h"

ABSL_FLAG(std::string, emu_rom, "", "Path to the ROM file to load.");
ABSL_FLAG(bool, emu_no_gui, false, "Disable GUI and run in headless mode.");
ABSL_FLAG(std::string, emu_load_state, "", "Load emulator state from a file.");
ABSL_FLAG(std::string, emu_dump_state, "", "Dump emulator state to a file.");
ABSL_FLAG(int, emu_frames, 0, "Number of frames to run the emulator for.");
ABSL_FLAG(int, emu_max_frames, 180, "Maximum frames to run before auto-exit (0=infinite, default=180/3 seconds).");
ABSL_FLAG(bool, emu_debug_apu, false, "Enable detailed APU/SPC700 logging.");
ABSL_FLAG(bool, emu_debug_cpu, false, "Enable detailed CPU execution logging.");

using yaze::util::SDL_Deleter;

int main(int argc, char **argv) {
  absl::InitializeSymbolizer(argv[0]);

  absl::FailureSignalHandlerOptions options;
  options.symbolize_stacktrace = true;
  options.use_alternate_stack =
      false;  // Disable alternate stack to avoid shutdown conflicts
  options.alarm_on_failure_secs =
      false;  // Disable alarm to avoid false positives during SDL cleanup
  options.call_previous_handler = true;
  absl::InstallFailureSignalHandler(options);

  absl::ParseCommandLine(argc, argv);

  if (absl::GetFlag(FLAGS_emu_no_gui)) {
    yaze::Rom rom;
    if (!rom.LoadFromFile(absl::GetFlag(FLAGS_emu_rom)).ok()) {
      return EXIT_FAILURE;
    }

    yaze::emu::Snes snes;
    std::vector<uint8_t> rom_data = rom.vector();
    snes.Init(rom_data);

    if (!absl::GetFlag(FLAGS_emu_load_state).empty()) {
      snes.loadState(absl::GetFlag(FLAGS_emu_load_state));
    }

    for (int i = 0; i < absl::GetFlag(FLAGS_emu_frames); ++i) {
      snes.RunFrame();
    }

    if (!absl::GetFlag(FLAGS_emu_dump_state).empty()) {
      snes.saveState(absl::GetFlag(FLAGS_emu_dump_state));
    }

    return EXIT_SUCCESS;
  }

  SDL_SetMainReady();

  std::unique_ptr<SDL_Window, SDL_Deleter> window_;
  std::unique_ptr<SDL_Renderer, SDL_Deleter> renderer_;
  if (SDL_Init(SDL_INIT_EVERYTHING) != 0) {
    return EXIT_FAILURE;
  } else {
    SDL_DisplayMode displayMode;
    SDL_GetCurrentDisplayMode(0, &displayMode);
    window_ = std::unique_ptr<SDL_Window, SDL_Deleter>(
        SDL_CreateWindow("Yaze Emulator",          // window title
                         SDL_WINDOWPOS_UNDEFINED,  // initial x position
                         SDL_WINDOWPOS_UNDEFINED,  // initial y position
                         512,                      // width, in pixels
                         480,                      // height, in pixels
                         SDL_WINDOW_RESIZABLE | SDL_WINDOW_ALLOW_HIGHDPI),
        SDL_Deleter());
    if (window_ == nullptr) {
      return EXIT_FAILURE;
    }
  }

  renderer_ = std::unique_ptr<SDL_Renderer, SDL_Deleter>(
      SDL_CreateRenderer(window_.get(), -1,
                         SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC),
      SDL_Deleter());
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

// Cocoa initialization not needed for standalone SDL emulator
// (Handled by SDL_SetMainReady)

  auto ppu_texture_ =
      SDL_CreateTexture(renderer_.get(), SDL_PIXELFORMAT_RGBX8888,
                        SDL_TEXTUREACCESS_STREAMING, 512, 480);
  if (ppu_texture_ == NULL) {
    printf("Failed to create texture: %s\n", SDL_GetError());
    return EXIT_FAILURE;
  }

  yaze::Rom rom_;
  yaze::emu::Snes snes_;
  std::vector<uint8_t> rom_data_;

  bool running = true;
  bool loaded = false;
  auto count_frequency = SDL_GetPerformanceFrequency();
  auto last_count = SDL_GetPerformanceCounter();
  auto time_adder = 0.0;
  int wanted_frames_ = 0;
  int wanted_samples_ = 0;
  int frame_count = 0;
  int max_frames = absl::GetFlag(FLAGS_emu_max_frames);
  SDL_Event event;

  // Load ROM from command-line argument or default
  std::string rom_path = absl::GetFlag(FLAGS_emu_rom);
  if (rom_path.empty()) {
    rom_path = "assets/zelda3.sfc";  // Default to zelda3 in assets
  }
  
  if (!rom_.LoadFromFile(rom_path).ok()) {
    printf("Failed to load ROM: %s\n", rom_path.c_str());
    return EXIT_FAILURE;
  }

  if (rom_.is_loaded()) {
    printf("Loaded ROM: %s (%zu bytes)\n", rom_path.c_str(), rom_.size());
    rom_data_ = rom_.vector();
    snes_.Init(rom_data_);
    wanted_frames_ = 1.0 / (snes_.memory().pal_timing() ? 50.0 : 60.0);
    wanted_samples_ = 48000 / (snes_.memory().pal_timing() ? 50 : 60);
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
            wanted_frames_ = 1.0 / (snes_.memory().pal_timing() ? 50.0 : 60.0);
            wanted_samples_ = 48000 / (snes_.memory().pal_timing() ? 50 : 60);
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
        frame_count++;

        // Print status every 60 frames (1 second)
        if (frame_count % 60 == 0) {
          printf("[Frame %d] CPU=$%02X:%04X SPC=$%04X APU_cycles=%llu\n", 
                 frame_count, snes_.cpu().PB, snes_.cpu().PC, 
                 snes_.apu().spc700().PC, snes_.apu().GetCycles());
        }

        // Auto-exit after max_frames (if set)
        if (max_frames > 0 && frame_count >= max_frames) {
          printf("\nReached max frames (%d), exiting...\n", max_frames);
          printf("Final state: CPU=$%02X:%04X SPC=$%04X\n",
                 snes_.cpu().PB, snes_.cpu().PC, snes_.apu().spc700().PC);
          running = false;
        }

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
  delete[] audio_buffer_;
  // ImGui_ImplSDLRenderer2_Shutdown();
  // ImGui_ImplSDL2_Shutdown();
  // ImGui::DestroyContext();
  SDL_Quit();

  return EXIT_SUCCESS;
}
