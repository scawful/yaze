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

  // Initialize SDL subsystems
  SDL_SetMainReady();
  if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO | SDL_INIT_EVENTS) != 0) {
    printf("SDL_Init failed: %s\n", SDL_GetError());
    return EXIT_FAILURE;
  }

  // Create window and renderer with RAII smart pointers
  std::unique_ptr<SDL_Window, SDL_Deleter> window_(
      SDL_CreateWindow("Yaze Emulator",
                       SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
                       512, 480,
                       SDL_WINDOW_RESIZABLE | SDL_WINDOW_ALLOW_HIGHDPI),
      SDL_Deleter());
  if (!window_) {
    printf("SDL_CreateWindow failed: %s\n", SDL_GetError());
    SDL_Quit();
    return EXIT_FAILURE;
  }

  std::unique_ptr<SDL_Renderer, SDL_Deleter> renderer_(
      SDL_CreateRenderer(window_.get(), -1,
                         SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC),
      SDL_Deleter());
  if (!renderer_) {
    printf("SDL_CreateRenderer failed: %s\n", SDL_GetError());
    SDL_Quit();
    return EXIT_FAILURE;
  }
  SDL_SetRenderDrawBlendMode(renderer_.get(), SDL_BLENDMODE_BLEND);
  SDL_SetRenderDrawColor(renderer_.get(), 0x00, 0x00, 0x00, 0xFF);

  // Initialize audio system
  constexpr int kAudioFrequency = 48000;
  SDL_AudioSpec want = {};
  want.freq = kAudioFrequency;
  want.format = AUDIO_S16;
  want.channels = 2;
  want.samples = 2048;
  want.callback = nullptr;  // Use audio queue

  SDL_AudioSpec have;
  SDL_AudioDeviceID audio_device = SDL_OpenAudioDevice(nullptr, 0, &want, &have, 0);
  if (audio_device == 0) {
    printf("SDL_OpenAudioDevice failed: %s\n", SDL_GetError());
    SDL_Quit();
    return EXIT_FAILURE;
  }
  
  // Allocate audio buffer using unique_ptr for automatic cleanup
  std::unique_ptr<int16_t[]> audio_buffer(new int16_t[kAudioFrequency / 50 * 4]);
  SDL_PauseAudioDevice(audio_device, 0);

  // Create PPU texture for rendering
  SDL_Texture* ppu_texture = SDL_CreateTexture(renderer_.get(), 
                                                SDL_PIXELFORMAT_RGBX8888,
                                                SDL_TEXTUREACCESS_STREAMING, 
                                                512, 480);
  if (!ppu_texture) {
    printf("SDL_CreateTexture failed: %s\n", SDL_GetError());
    SDL_CloseAudioDevice(audio_device);
    SDL_Quit();
    return EXIT_FAILURE;
  }

  yaze::Rom rom_;
  yaze::emu::Snes snes_;
  std::vector<uint8_t> rom_data_;

  // Emulator state
  bool running = true;
  bool loaded = false;
  int frame_count = 0;
  const int max_frames = absl::GetFlag(FLAGS_emu_max_frames);
  
  // Timing management
  const uint64_t count_frequency = SDL_GetPerformanceFrequency();
  uint64_t last_count = SDL_GetPerformanceCounter();
  double time_adder = 0.0;
  double wanted_frame_time = 0.0;
  int wanted_samples = 0;
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
    
    // Calculate timing based on PAL/NTSC
    const bool is_pal = snes_.memory().pal_timing();
    const double refresh_rate = is_pal ? 50.0 : 60.0;
    wanted_frame_time = 1.0 / refresh_rate;
    wanted_samples = kAudioFrequency / static_cast<int>(refresh_rate);
    
    printf("Emulator initialized: %s mode (%.1f Hz)\n", is_pal ? "PAL" : "NTSC", refresh_rate);
    loaded = true;
  }

  while (running) {
    while (SDL_PollEvent(&event)) {
      switch (event.type) {
        case SDL_DROPFILE:
          if (rom_.LoadFromFile(event.drop.file).ok() && rom_.is_loaded()) {
            rom_data_ = rom_.vector();
            snes_.Init(rom_data_);
            
            const bool is_pal = snes_.memory().pal_timing();
            const double refresh_rate = is_pal ? 50.0 : 60.0;
            wanted_frame_time = 1.0 / refresh_rate;
            wanted_samples = kAudioFrequency / static_cast<int>(refresh_rate);
            
            printf("Loaded new ROM via drag-and-drop: %s\n", event.drop.file);
            frame_count = 0;  // Reset frame counter
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

    const uint64_t current_count = SDL_GetPerformanceCounter();
    const uint64_t delta = current_count - last_count;
    last_count = current_count;
    const double seconds = static_cast<double>(delta) / static_cast<double>(count_frequency);
    time_adder += seconds;
    
    // Run frame if enough time has elapsed (allow 2ms grace period)
    while (time_adder >= wanted_frame_time - 0.002) {
      time_adder -= wanted_frame_time;

      if (loaded) {
        snes_.RunFrame();
        frame_count++;

        // Detect deadlock - CPU stuck in same location
        static uint16_t last_cpu_pc = 0;
        static int stuck_count = 0;
        uint16_t current_cpu_pc = snes_.cpu().PC;
        
        if (current_cpu_pc == last_cpu_pc && current_cpu_pc >= 0x88B0 && current_cpu_pc <= 0x88C0) {
          stuck_count++;
          if (stuck_count > 180 && frame_count % 60 == 0) {
            printf("[WARNING] CPU stuck at $%02X:%04X for %d frames (APU deadlock?)\n",
                   snes_.cpu().PB, current_cpu_pc, stuck_count);
          }
        } else {
          stuck_count = 0;
        }
        last_cpu_pc = current_cpu_pc;

        // Print status every 60 frames (1 second)
        if (frame_count % 60 == 0) {
          printf("[Frame %d] CPU=$%02X:%04X SPC=$%04X APU_cycles=%llu\n", 
                 frame_count, snes_.cpu().PB, snes_.cpu().PC, 
                 snes_.apu().spc700().PC, snes_.apu().GetCycles());
        }

        // Auto-exit after max_frames (if set)
        if (max_frames > 0 && frame_count >= max_frames) {
          printf("\n[EMULATOR] Reached max frames (%d), shutting down...\n", max_frames);
          printf("[EMULATOR] Final state: CPU=$%02X:%04X SPC=$%04X\n",
                 snes_.cpu().PB, snes_.cpu().PC, snes_.apu().spc700().PC);
          running = false;
          break;  // Exit inner loop immediately
        }

        // Generate audio samples and queue them
        snes_.SetSamples(audio_buffer.get(), wanted_samples);
        const uint32_t queued_size = SDL_GetQueuedAudioSize(audio_device);
        const uint32_t max_queued = wanted_samples * 4 * 6;  // Keep up to 6 frames queued
        if (queued_size <= max_queued) {
          SDL_QueueAudio(audio_device, audio_buffer.get(), wanted_samples * 4);
        }

        // Render PPU output to texture
        void *ppu_pixels = nullptr;
        int ppu_pitch = 0;
        if (SDL_LockTexture(ppu_texture, nullptr, &ppu_pixels, &ppu_pitch) != 0) {
          printf("Failed to lock texture: %s\n", SDL_GetError());
          running = false;
          break;
        }
        snes_.SetPixels(static_cast<uint8_t*>(ppu_pixels));
        SDL_UnlockTexture(ppu_texture);
      }
    }

    // Present rendered frame
    SDL_RenderClear(renderer_.get());
    SDL_RenderCopy(renderer_.get(), ppu_texture, nullptr, nullptr);
    SDL_RenderPresent(renderer_.get());
  }

  // === Cleanup SDL resources (in reverse order of initialization) ===
  printf("\n[EMULATOR] Shutting down...\n");
  
  // Clean up texture
  if (ppu_texture) {
    SDL_DestroyTexture(ppu_texture);
    ppu_texture = nullptr;
  }
  
  // Clean up audio (audio_buffer cleaned up automatically by unique_ptr)
  SDL_PauseAudioDevice(audio_device, 1);
  SDL_ClearQueuedAudio(audio_device);
  SDL_CloseAudioDevice(audio_device);
  
  // Clean up renderer and window (done automatically by unique_ptr destructors)
  renderer_.reset();
  window_.reset();
  
  // Quit SDL subsystems
  SDL_Quit();
  
  printf("[EMULATOR] Shutdown complete.\n");
  return EXIT_SUCCESS;
}
