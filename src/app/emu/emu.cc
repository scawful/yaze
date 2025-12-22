#if __APPLE__
// #include "app/platform/app_delegate.h"
#endif

#include "app/platform/sdl_compat.h"

#include <memory>
#include <string>
#include <vector>

#include "absl/debugging/failure_signal_handler.h"
#include "absl/debugging/symbolize.h"
#include "absl/flags/flag.h"
#include "absl/flags/parse.h"
#include "app/emu/snes.h"
#include "app/emu/audio/audio_backend.h"
#include "app/emu/input/input_manager.h"
#include "app/gfx/backend/irenderer.h"
#include "app/gfx/backend/renderer_factory.h"
#include "app/platform/iwindow.h"
#include "rom/rom.h"
#include "util/sdl_deleter.h"
#include "imgui/imgui.h"

ABSL_FLAG(std::string, emu_rom, "", "Path to the ROM file to load.");
ABSL_FLAG(bool, emu_no_gui, false, "Disable GUI and run in headless mode.");
ABSL_FLAG(std::string, emu_load_state, "", "Load emulator state from a file.");
ABSL_FLAG(std::string, emu_dump_state, "", "Dump emulator state to a file.");
ABSL_FLAG(int, emu_frames, 0, "Number of frames to run the emulator for.");
ABSL_FLAG(int, emu_max_frames, 180,
          "Maximum frames to run before auto-exit (0=infinite, default=180/3 "
          "seconds).");
ABSL_FLAG(bool, emu_debug_apu, false, "Enable detailed APU/SPC700 logging.");
ABSL_FLAG(bool, emu_debug_cpu, false, "Enable detailed CPU execution logging.");
ABSL_FLAG(bool, emu_fix_red_tint, true, "Fix red/blue channel swap (BGR->RGB).");

using yaze::util::SDL_Deleter;

int main(int argc, char** argv) {
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
      auto status = snes.loadState(absl::GetFlag(FLAGS_emu_load_state));
      if (!status.ok()) {
        printf("Failed to load state: %s\n", std::string(status.message()).c_str());
        return EXIT_FAILURE;
      }
    }

    for (int i = 0; i < absl::GetFlag(FLAGS_emu_frames); ++i) {
      snes.RunFrame();
    }

    if (!absl::GetFlag(FLAGS_emu_dump_state).empty()) {
      auto status = snes.saveState(absl::GetFlag(FLAGS_emu_dump_state));
      if (!status.ok()) {
        printf("Failed to save state: %s\n", std::string(status.message()).c_str());
        return EXIT_FAILURE;
      }
    }

    return EXIT_SUCCESS;
  }

  // Initialize window backend (SDL2 or SDL3)
  auto window_backend = yaze::platform::WindowBackendFactory::Create(
      yaze::platform::WindowBackendFactory::GetDefaultType());

  yaze::platform::WindowConfig config;
  config.title = "Yaze Emulator";
  config.width = 512;
  config.height = 480;
  config.resizable = true;
  config.high_dpi = false;  // Disabled - causes issues on macOS Retina with SDL_Renderer

  if (!window_backend->Initialize(config).ok()) {
    printf("Failed to initialize window backend\n");
    return EXIT_FAILURE;
  }

  // Create and initialize the renderer (uses factory for SDL2/SDL3 selection)
  auto renderer = yaze::gfx::RendererFactory::Create();
  if (!window_backend->InitializeRenderer(renderer.get())) {
    printf("Failed to initialize renderer\n");
    window_backend->Shutdown();
    return EXIT_FAILURE;
  }

  // Initialize ImGui (with viewports if supported)
  if (!window_backend->InitializeImGui(renderer.get()).ok()) {
    printf("Failed to initialize ImGui\n");
    window_backend->Shutdown();
    return EXIT_FAILURE;
  }

  // Initialize audio system using AudioBackend
  auto audio_backend = yaze::emu::audio::AudioBackendFactory::Create(
      yaze::emu::audio::AudioBackendFactory::BackendType::SDL2);

  yaze::emu::audio::AudioConfig audio_config;
  audio_config.sample_rate = 48000;
  audio_config.channels = 2;
  audio_config.buffer_frames = 1024;
  audio_config.format = yaze::emu::audio::SampleFormat::INT16;

  // Native SNES audio sample rate (SPC700)
  constexpr int kNativeSampleRate = 32040;

  if (!audio_backend->Initialize(audio_config)) {
    printf("Failed to initialize audio backend\n");
    // Continue without audio
  } else {
    printf("Audio initialized: %s\n", audio_backend->GetBackendName().c_str());
    // Enable audio stream resampling (32040 Hz -> 48000 Hz)
    // CRITICAL: Without this, audio plays at 1.5x speed (48000/32040 = 1.498)
    if (audio_backend->SupportsAudioStream()) {
      audio_backend->SetAudioStreamResampling(true, kNativeSampleRate, 2);
      printf("Audio resampling enabled: %dHz -> %dHz\n",
             kNativeSampleRate, audio_config.sample_rate);
    }
  }

  // Allocate audio buffer using unique_ptr for automatic cleanup
  std::unique_ptr<int16_t[]> audio_buffer(
      new int16_t[audio_config.sample_rate / 50 * 4]);

  // Create PPU texture for rendering
  void* ppu_texture = renderer->CreateTexture(512, 480);
  if (!ppu_texture) {
    printf("SDL_CreateTexture failed: %s\n", SDL_GetError());
    window_backend->Shutdown();
    return EXIT_FAILURE;
  }

  yaze::Rom rom_;
  yaze::emu::Snes snes_;
  std::vector<uint8_t> rom_data_;
  yaze::emu::input::InputManager input_manager_;

  // Initialize input manager
  // TODO: Use factory or detect backend
  input_manager_.Initialize(yaze::emu::input::InputBackendFactory::BackendType::SDL2);

  // Emulator state
  bool running = true;
  bool loaded = false;
  int frame_count = 0;
  const int max_frames = absl::GetFlag(FLAGS_emu_max_frames);
  bool fix_red_tint = absl::GetFlag(FLAGS_emu_fix_red_tint);

  // Timing management
  const uint64_t count_frequency = SDL_GetPerformanceFrequency();
  uint64_t last_count = SDL_GetPerformanceCounter();
  double time_adder = 0.0;
  double wanted_frame_time = 0.0;
  int wanted_samples = 0;
  yaze::platform::WindowEvent event;

  // Load ROM from command-line argument or default
  std::string rom_path = absl::GetFlag(FLAGS_emu_rom);
  if (rom_path.empty()) {
    rom_path = "assets/zelda3.sfc";  // Default to zelda3 in assets
  }

  if (!rom_.LoadFromFile(rom_path).ok()) {
    printf("Failed to load ROM: %s\n", rom_path.c_str());
    // Continue running without ROM to show UI
  }

  if (rom_.is_loaded()) {
    printf("Loaded ROM: %s (%zu bytes)\n", rom_path.c_str(), rom_.size());
    rom_data_ = rom_.vector();
    snes_.Init(rom_data_);

    // Calculate timing based on PAL/NTSC
    const bool is_pal = snes_.memory().pal_timing();
    const double refresh_rate = is_pal ? 50.0 : 60.0;
    wanted_frame_time = 1.0 / refresh_rate;
    // Use NATIVE sample rate (32040 Hz), not device rate
    // Audio stream resampling handles conversion to device rate
    wanted_samples = kNativeSampleRate / static_cast<int>(refresh_rate);

    printf("Emulator initialized: %s mode (%.1f Hz)\n", is_pal ? "PAL" : "NTSC",
           refresh_rate);
    loaded = true;
  }

  while (running) {
    while (window_backend->PollEvent(event)) {
      switch (event.type) {
        case yaze::platform::WindowEventType::DropFile:
          if (rom_.LoadFromFile(event.dropped_file).ok() && rom_.is_loaded()) {
            rom_data_ = rom_.vector();
            snes_.Init(rom_data_);

            const bool is_pal = snes_.memory().pal_timing();
            const double refresh_rate = is_pal ? 50.0 : 60.0;
            wanted_frame_time = 1.0 / refresh_rate;
            // Use NATIVE sample rate (32040 Hz), not device rate (48000 Hz)
            // Audio stream resampling handles conversion to device rate
            wanted_samples = kNativeSampleRate / static_cast<int>(refresh_rate);

            printf("Loaded new ROM via drag-and-drop: %s\n", event.dropped_file.c_str());
            frame_count = 0;  // Reset frame counter
            loaded = true;
          }
          break;
        case yaze::platform::WindowEventType::Close:
        case yaze::platform::WindowEventType::Quit:
          running = false;
          break;
        default:
          break;
      }
    }

    const uint64_t current_count = SDL_GetPerformanceCounter();
    const uint64_t delta = current_count - last_count;
    last_count = current_count;
    const double seconds =
        static_cast<double>(delta) / static_cast<double>(count_frequency);
    time_adder += seconds;

    // Run frame if enough time has elapsed (allow 2ms grace period)
    while (time_adder >= wanted_frame_time - 0.002) {
      time_adder -= wanted_frame_time;

      if (loaded) {
        // Poll input before each frame for proper edge detection
        input_manager_.Poll(&snes_, 1);
        snes_.RunFrame();
        frame_count++;

        // Detect deadlock - CPU stuck in same location
        static uint16_t last_cpu_pc = 0;
        static int stuck_count = 0;
        uint16_t current_cpu_pc = snes_.cpu().PC;

        if (current_cpu_pc == last_cpu_pc && current_cpu_pc >= 0x88B0 &&
            current_cpu_pc <= 0x88C0) {
          stuck_count++;
          if (stuck_count > 180 && frame_count % 60 == 0) {
            printf(
                "[WARNING] CPU stuck at $%02X:%04X for %d frames (APU "
                "deadlock?)\n",
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
          printf("\n[EMULATOR] Reached max frames (%d), shutting down...\n",
                 max_frames);
          printf("[EMULATOR] Final state: CPU=$%02X:%04X SPC=$%04X\n",
                 snes_.cpu().PB, snes_.cpu().PC, snes_.apu().spc700().PC);
          running = false;
          break;  // Exit inner loop immediately
        }

        // Generate audio samples at native rate (32040 Hz)
        snes_.SetSamples(audio_buffer.get(), wanted_samples);

        if (audio_backend && audio_backend->IsInitialized()) {
          auto status = audio_backend->GetStatus();
          // Keep up to 6 frames queued
          if (status.queued_frames <= static_cast<uint32_t>(wanted_samples * 6)) {
            // Use QueueSamplesNative for proper resampling (32040 Hz -> device rate)
            // DO NOT use QueueSamples directly - that causes 1.5x speed bug!
            if (!audio_backend->QueueSamplesNative(audio_buffer.get(), wanted_samples,
                                                    2, kNativeSampleRate)) {
              // If resampling failed, try to re-enable and retry once
              if (audio_backend->SupportsAudioStream()) {
                audio_backend->SetAudioStreamResampling(true, kNativeSampleRate, 2);
                audio_backend->QueueSamplesNative(audio_buffer.get(), wanted_samples,
                                                   2, kNativeSampleRate);
              }
            }
          }
        }

        // Render PPU output to texture
        void* ppu_pixels = nullptr;
        int ppu_pitch = 0;
        if (renderer->LockTexture(ppu_texture, nullptr, &ppu_pixels,
                                  &ppu_pitch)) {
          uint8_t* pixels = static_cast<uint8_t*>(ppu_pixels);
          snes_.SetPixels(pixels);
          
          // Fix red tint if enabled (BGR -> RGB swap)
          // This assumes 32-bit BGRA/RGBA buffer. PPU outputs XRGB.
          // SDL textures are often ARGB/BGRA.
          // If we see red tint, blue and red are swapped.
          if (fix_red_tint) {
            for (int i = 0; i < 512 * 480; ++i) {
              uint8_t b = pixels[i * 4 + 0];
              uint8_t r = pixels[i * 4 + 2];
              pixels[i * 4 + 0] = r;
              pixels[i * 4 + 2] = b;
            }
          }
          
          renderer->UnlockTexture(ppu_texture);
        }
      }
    }

    // Present rendered frame
    window_backend->NewImGuiFrame();
    
    // Simple debug overlay
    ImGui::Begin("Emulator Stats");
    ImGui::Text("Frame: %d", frame_count);
    ImGui::Text("FPS: %.1f", ImGui::GetIO().Framerate);
    ImGui::Checkbox("Fix Red Tint", &fix_red_tint);
    if (loaded) {
        ImGui::Separator();
        ImGui::Text("CPU PC: $%02X:%04X", snes_.cpu().PB, snes_.cpu().PC);
        ImGui::Text("SPC PC: $%04X", snes_.apu().spc700().PC);
    }
    ImGui::End();
    
    renderer->Clear();
    
    // Render texture (scaled to window)
    // TODO: Use proper aspect ratio handling
    renderer->RenderCopy(ppu_texture, nullptr, nullptr);
    
    // Render ImGui draw data and handle viewports
    window_backend->RenderImGui(renderer.get()); 
    
    renderer->Present();
  }

  // === Cleanup SDL resources (in reverse order of initialization) ===
  printf("\n[EMULATOR] Shutting down...\n");

  // Clean up texture
  if (ppu_texture) {
    renderer->DestroyTexture(ppu_texture);
    ppu_texture = nullptr;
  }

  // Clean up audio
  if (audio_backend) {
    audio_backend->Shutdown();
  }

  // Clean up renderer and window (via backend)
  window_backend->ShutdownImGui();
  renderer->Shutdown();
  window_backend->Shutdown();

  printf("[EMULATOR] Shutdown complete.\n");
  return EXIT_SUCCESS;
}
