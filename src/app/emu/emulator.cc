#include "app/emu/emulator.h"

#include <cmath>
#include <cstdint>
#include <cstdlib>
#include <vector>

#include "absl/strings/str_format.h"
#include "app/editor/system/panel_manager.h"
#include "util/log.h"

namespace yaze::core {
extern bool g_window_is_resizing;
}

#include "app/emu/debug/disassembly_viewer.h"
#include "app/emu/ui/debugger_ui.h"
#include "app/emu/ui/emulator_ui.h"
#include "app/emu/ui/input_handler.h"
#include "app/gui/app/editor_layout.h"
#include "app/gui/core/color.h"
#include "app/gui/core/icons.h"
#include "app/gui/core/theme_manager.h"
#include "imgui/imgui.h"

#ifdef __EMSCRIPTEN__
#include "app/emu/platform/wasm/wasm_audio.h"
#endif

namespace yaze {
namespace emu {

namespace {
// SNES audio native sample rate (APU/DSP output rate)
// The actual SNES APU runs at 32040 Hz (not 32000 Hz).
// Using 32040 ensures we generate enough samples to prevent buffer underruns.
constexpr int kNativeSampleRate = 32040;

constexpr int kMusicEditorSampleRate = 22050;

// Accurate SNES frame rates based on master clock calculations
// NTSC: 21477272 Hz / (262 * 341) = ~60.0988 Hz
// PAL:  21281370 Hz / (312 * 341) = ~50.007 Hz
constexpr double kNtscFrameRate = 60.0988;
constexpr double kPalFrameRate = 50.007;

// Speed calibration factor for audio playback timing
// This compensates for any accumulated timing errors in the emulation.
// Value of 1.0 means no calibration. Values < 1.0 slow down playback.
// This can be exposed as a user-adjustable setting if needed.
constexpr double kSpeedCalibration = 1.0;
}  // namespace

Emulator::~Emulator() {
  // Don't call Cleanup() in destructor - renderer is already destroyed
  // Just stop emulation
  running_ = false;
}

void Emulator::Cleanup() {
  // Stop emulation
  running_ = false;

  // Don't try to destroy PPU texture during shutdown
  // The renderer is destroyed before the emulator, so attempting to
  // call renderer_->DestroyTexture() will crash
  // The texture will be cleaned up automatically when SDL quits
  ppu_texture_ = nullptr;

  // Reset state
  snes_initialized_ = false;
  audio_stream_active_ = false;
}

void Emulator::SetInputConfig(const input::InputConfig& config) {
  input_config_ = config;
  input_manager_.SetConfig(input_config_);
}

void Emulator::set_use_sdl_audio_stream(bool enabled) {
  if (use_sdl_audio_stream_ != enabled) {
    use_sdl_audio_stream_ = enabled;
    audio_stream_config_dirty_ = true;
  }
}

void Emulator::ResumeAudio() {
#ifdef __EMSCRIPTEN__
  if (audio_backend_) {
    // Safe cast because we know we created a WasmAudioBackend in WASM builds
    auto* wasm_backend =
        static_cast<audio::WasmAudioBackend*>(audio_backend_.get());
    wasm_backend->HandleUserInteraction();
  }
#endif
}

void Emulator::set_interpolation_type(int type) {
  if (!snes_initialized_)
    return;
  // Clamp to valid range (0-4)
  int safe_type = std::clamp(type, 0, 4);
  snes_.apu().dsp().interpolation_type =
      static_cast<InterpolationType>(safe_type);
}

int Emulator::get_interpolation_type() const {
  if (!snes_initialized_)
    return 0;  // Default to Linear if not initialized
  return static_cast<int>(snes_.apu().dsp().interpolation_type);
}

void Emulator::Initialize(gfx::IRenderer* renderer,
                          const std::vector<uint8_t>& rom_data) {
  // This method is now optional - emulator can be initialized lazily in Run()
  renderer_ = renderer;
  rom_data_ = rom_data;

  if (!audio_stream_env_checked_) {
    const char* env_value = std::getenv("YAZE_USE_SDL_AUDIO_STREAM");
    if (env_value && std::atoi(env_value) != 0) {
      set_use_sdl_audio_stream(true);
    }
    audio_stream_env_checked_ = true;
  }

  // Panels are registered in EditorManager::Initialize() to avoid duplication

  // Reset state for new ROM
  running_ = false;
  snes_initialized_ = false;

  // Initialize audio backend if not already done
  if (!audio_backend_) {
#ifdef __EMSCRIPTEN__
    audio_backend_ = audio::AudioBackendFactory::Create(
        audio::AudioBackendFactory::BackendType::WASM);
#else
    audio_backend_ = audio::AudioBackendFactory::Create(
        audio::AudioBackendFactory::BackendType::SDL2);
#endif

    audio::AudioConfig config;
    config.sample_rate = 48000;
    config.channels = 2;
    // Use moderate buffer size - 1024 samples = ~21ms latency
    // This is a good balance between latency and stability
    config.buffer_frames = 1024;
    config.format = audio::SampleFormat::INT16;

    if (!audio_backend_->Initialize(config)) {
      LOG_ERROR("Emulator", "Failed to initialize audio backend");
    } else {
      LOG_INFO("Emulator", "Audio backend initialized: %s",
               audio_backend_->GetBackendName().c_str());
      audio_stream_config_dirty_ = true;
    }
  }

  // Set up CPU breakpoint callback
  snes_.cpu().on_breakpoint_hit_ = [this](uint32_t pc) -> bool {
    return breakpoint_manager_.ShouldBreakOnExecute(
        pc, BreakpointManager::CpuType::CPU_65816);
  };

  // Set up instruction recording callback for DisassemblyViewer
  snes_.cpu().on_instruction_executed_ =
      [this](uint32_t address, uint8_t opcode,
             const std::vector<uint8_t>& operands, const std::string& mnemonic,
             const std::string& operand_str) {
        disassembly_viewer_.RecordInstruction(address, opcode, operands,
                                              mnemonic, operand_str);
      };

  initialized_ = true;
}

bool Emulator::EnsureInitialized(Rom* rom) {
  if (!rom || !rom->is_loaded()) {
    return false;
  }

  // Initialize audio backend if not already done
  // Skip if using external (shared) audio backend
  if (!audio_backend_ && !external_audio_backend_) {
#ifdef __EMSCRIPTEN__
    audio_backend_ = audio::AudioBackendFactory::Create(
        audio::AudioBackendFactory::BackendType::WASM);
#else
    audio_backend_ = audio::AudioBackendFactory::Create(
        audio::AudioBackendFactory::BackendType::SDL2);
#endif

    audio::AudioConfig config;
    config.sample_rate = 48000;
    config.channels = 2;
    config.buffer_frames = 1024;
    config.format = audio::SampleFormat::INT16;

    if (!audio_backend_->Initialize(config)) {
      LOG_ERROR("Emulator", "Failed to initialize audio backend");
      return false;
    }
    LOG_INFO("Emulator", "Audio backend initialized for headless mode");
  } else if (external_audio_backend_) {
    LOG_INFO("Emulator", "Using external (shared) audio backend");
  }

  // Initialize SNES if not already done
  if (!snes_initialized_) {
    if (rom_data_.empty()) {
      rom_data_ = rom->vector();
    }
    snes_.Init(rom_data_);

    // Use accurate SNES frame rates for proper timing
    const double frame_rate =
        snes_.memory().pal_timing() ? kPalFrameRate : kNtscFrameRate;
    wanted_frames_ = 1.0 / frame_rate;
    // When resampling is enabled (which we just did above), we need to generate
    // samples at the NATIVE rate (32kHz). The backend will resample them to 48kHz.
    // Calculate samples per frame based on actual frame rate for accurate timing.
    wanted_samples_ =
        static_cast<int>(std::lround(kNativeSampleRate / frame_rate));
    snes_initialized_ = true;

    count_frequency = SDL_GetPerformanceFrequency();
    last_count = SDL_GetPerformanceCounter();
    time_adder = 0.0;

    LOG_INFO("Emulator", "SNES initialized for headless mode");
  }

  // Always update timing constants based on current ROM region
  // This ensures MusicPlayer gets correct timing even if ROM changed
  if (snes_initialized_) {
    const double frame_rate =
        snes_.memory().pal_timing() ? kPalFrameRate : kNtscFrameRate;
    wanted_frames_ = 1.0 / frame_rate;
    wanted_samples_ =
        static_cast<int>(std::lround(kNativeSampleRate / frame_rate));
  }

  return true;
}

void Emulator::RunFrameOnly() {
  if (!snes_initialized_ || !running_) {
    return;
  }

  // If audio focus mode is active (Music Editor), skip standard frame processing
  // because MusicPlayer drives the emulator via RunAudioFrame()
  if (audio_focus_mode_) {
    return;
  }

  // Ensure audio stream resampling is configured (32040 Hz -> 48000 Hz)
  // Without this, samples are fed at wrong rate causing 1.5x speedup
  if (audio_backend_ && audio_stream_config_dirty_) {
    if (use_sdl_audio_stream_ && audio_backend_->SupportsAudioStream()) {
      audio_backend_->SetAudioStreamResampling(true, kNativeSampleRate, 2);
      audio_stream_active_ = true;
    } else {
      audio_backend_->SetAudioStreamResampling(false, kNativeSampleRate, 2);
      audio_stream_active_ = false;
    }
    audio_stream_config_dirty_ = false;
  }

  // Calculate timing
  uint64_t current_count = SDL_GetPerformanceCounter();
  uint64_t delta = current_count - last_count;
  last_count = current_count;
  double seconds = delta / (double)count_frequency;

  time_adder += seconds;

  // Cap time accumulation to prevent runaway (max 2 frames worth)
  double max_accumulation = wanted_frames_ * 2.0;
  if (time_adder > max_accumulation) {
    time_adder = max_accumulation;
  }

  // Process frames - limit to 2 frames max per update to prevent fast-forward
  int frames_processed = 0;
  constexpr int kMaxFramesPerUpdate = 2;

  // Local buffer for audio samples (533 stereo samples per frame)
  static int16_t native_audio_buffer[2048];

  while (time_adder >= wanted_frames_ &&
         frames_processed < kMaxFramesPerUpdate) {
    time_adder -= wanted_frames_;
    frames_processed++;

    // Mark frame boundary for DSP sample reading
    // snes_.apu().dsp().NewFrame(); // Removed in favor of readOffset tracking

    // Run SNES frame (generates audio samples)
    snes_.RunFrame();

    // Queue audio samples (always resampled to backend rate)
    if (audio_backend_) {
      auto status = audio_backend_->GetStatus();
      const uint32_t max_buffer = static_cast<uint32_t>(wanted_samples_ * 6);

      if (status.queued_frames < max_buffer) {
        snes_.SetSamples(native_audio_buffer, wanted_samples_);
        // Try native rate resampling first (if audio stream is enabled)
        // Falls back to direct queueing if not available
        if (!audio_backend_->QueueSamplesNative(
                native_audio_buffer, wanted_samples_, 2, kNativeSampleRate)) {
          static int log_counter = 0;
          if (++log_counter % 60 == 0) {
            int backend_rate = audio_backend_->GetConfig().sample_rate;
            LOG_WARN("Emulator",
                     "Resampling failed (Native=%d, Backend=%d) - Dropping "
                     "audio to prevent speedup/pitch shift",
                     kNativeSampleRate, backend_rate);
          }
        }
      }
    }
  }
}

void Emulator::ResetFrameTiming() {
  // Reset timing state to prevent accumulated time from causing fast playback
  count_frequency = SDL_GetPerformanceFrequency();
  last_count = SDL_GetPerformanceCounter();
  time_adder = 0.0;

  // Clear audio buffer to prevent static from stale data
  // Use accessor to get correct backend (external or owned)
  if (auto* backend = audio_backend()) {
    backend->Clear();
  }
}

void Emulator::RunAudioFrame() {
  // Simplified audio-focused frame execution for music editor
  // Runs exactly one SNES frame per call - caller controls timing

  // Use accessor to get correct backend (external or owned)
  auto* backend = audio_backend();

  // DIAGNOSTIC: Always log entry to verify this function is being called
  static int entry_count = 0;
  if (entry_count < 5 || entry_count % 300 == 0) {
    LOG_INFO("Emulator",
             "RunAudioFrame ENTRY #%d: init=%d, running=%d, backend=%p "
             "(external=%p, owned=%p)",
             entry_count, snes_initialized_, running_,
             static_cast<void*>(backend),
             static_cast<void*>(external_audio_backend_),
             static_cast<void*>(audio_backend_.get()));
  }
  entry_count++;

  if (!snes_initialized_ || !running_) {
    static int skip_count = 0;
    if (skip_count < 5) {
      LOG_WARN("Emulator", "RunAudioFrame SKIPPED: init=%d, running=%d",
               snes_initialized_, running_);
    }
    skip_count++;
    return;
  }

  // Ensure audio stream resampling is configured (32040 Hz -> 48000 Hz)
  if (backend && audio_stream_config_dirty_) {
    if (use_sdl_audio_stream_ && backend->SupportsAudioStream()) {
      backend->SetAudioStreamResampling(true, kNativeSampleRate, 2);
      audio_stream_active_ = true;
    }
    audio_stream_config_dirty_ = false;
  }

  // Run exactly one SNES audio frame
  // Note: NewFrame() is called inside Snes::RunCycle() at vblank start
  snes_.RunAudioFrame();

  // Queue audio samples to backend
  if (backend) {
    static int16_t audio_buffer[2048];  // 533 stereo samples max
    snes_.SetSamples(audio_buffer, wanted_samples_);

    bool queued = backend->QueueSamplesNative(audio_buffer, wanted_samples_, 2,
                                              kNativeSampleRate);

    // Diagnostic: Log first few calls and then periodically
    static int frame_log_count = 0;
    if (frame_log_count < 5 || frame_log_count % 300 == 0) {
      LOG_INFO("Emulator", "RunAudioFrame: wanted=%d, queued=%s, stream=%s",
               wanted_samples_, queued ? "YES" : "NO",
               audio_stream_active_ ? "active" : "inactive");
    }
    frame_log_count++;

    if (!queued && backend->SupportsAudioStream()) {
      // Try to re-enable resampling and retry once
      LOG_INFO("Emulator",
               "RunAudioFrame: First queue failed, re-enabling resampling");
      backend->SetAudioStreamResampling(true, kNativeSampleRate, 2);
      audio_stream_active_ = true;
      queued = backend->QueueSamplesNative(audio_buffer, wanted_samples_, 2,
                                           kNativeSampleRate);
      LOG_INFO("Emulator", "RunAudioFrame: Retry queued=%s",
               queued ? "YES" : "NO");
    }

    if (!queued) {
      LOG_WARN("Emulator",
               "RunAudioFrame: AUDIO DROPPED - resampling not working!");
    }
  }
}

void Emulator::Run(Rom* rom) {
  if (!audio_stream_env_checked_) {
    const char* env_value = std::getenv("YAZE_USE_SDL_AUDIO_STREAM");
    if (env_value && std::atoi(env_value) != 0) {
      set_use_sdl_audio_stream(true);
    }
    audio_stream_env_checked_ = true;
  }

  // Lazy initialization: set renderer from Controller if not set yet
  if (!renderer_) {
    ImGui::TextColored(ImVec4(1.0f, 0.3f, 0.3f, 1.0f),
                       "Emulator renderer not initialized");
    return;
  }

  // Initialize audio backend if not already done (lazy initialization)
  if (!audio_backend_) {
#ifdef __EMSCRIPTEN__
    audio_backend_ = audio::AudioBackendFactory::Create(
        audio::AudioBackendFactory::BackendType::WASM);
#else
    audio_backend_ = audio::AudioBackendFactory::Create(
        audio::AudioBackendFactory::BackendType::SDL2);
#endif

    audio::AudioConfig config;
    config.sample_rate = 48000;
    config.channels = 2;
    // Use moderate buffer size - 1024 samples = ~21ms latency
    // This is a good balance between latency and stability
    config.buffer_frames = 1024;
    config.format = audio::SampleFormat::INT16;

    if (!audio_backend_->Initialize(config)) {
      LOG_ERROR("Emulator", "Failed to initialize audio backend");
    } else {
      LOG_INFO("Emulator", "Audio backend initialized (lazy): %s",
               audio_backend_->GetBackendName().c_str());
      audio_stream_config_dirty_ = true;
    }
  }

  // Initialize input manager if not already done
  if (!input_manager_.IsInitialized()) {
    input_manager_.SetConfig(input_config_);
    if (!input_manager_.Initialize(
            input::InputBackendFactory::BackendType::SDL2)) {
      LOG_ERROR("Emulator", "Failed to initialize input manager");
    } else {
      input_config_ = input_manager_.GetConfig();
      LOG_INFO("Emulator", "Input manager initialized: %s",
               input_manager_.backend()->GetBackendName().c_str());
    }
  } else {
    input_config_ = input_manager_.GetConfig();
  }

  // Initialize SNES and create PPU texture on first run
  // This happens lazily when user opens the emulator window
  if (!snes_initialized_ && rom->is_loaded()) {
    // Create PPU texture with correct format for SNES emulator
    // ARGB8888 matches the XBGR format used by the SNES PPU (pixel format 1)
    if (!ppu_texture_) {
      ppu_texture_ = renderer_->CreateTextureWithFormat(
          512, 480, SDL_PIXELFORMAT_ARGB8888, SDL_TEXTUREACCESS_STREAMING);
      if (ppu_texture_ == NULL) {
        printf("Failed to create PPU texture: %s\n", SDL_GetError());
        return;
      }
    }

    // Initialize SNES with ROM data (either from Initialize() or from rom
    // parameter)
    if (rom_data_.empty()) {
      rom_data_ = rom->vector();
    }
    snes_.Init(rom_data_);

    // Note: DisassemblyViewer recording is always enabled via callback
    // No explicit setup needed - callback is set in Initialize()

    // Note: PPU pixel format set to 1 (XBGR) in Init() which matches ARGB8888
    // texture

    // Use accurate SNES frame rates for proper timing
    const double frame_rate =
        snes_.memory().pal_timing() ? kPalFrameRate : kNtscFrameRate;
    wanted_frames_ = 1.0 / frame_rate;
    // Use native SNES sample rate (32kHz), not backend rate (48kHz)
    // The audio backend handles resampling from 32kHz -> 48kHz
    wanted_samples_ =
        static_cast<int>(std::lround(kNativeSampleRate / frame_rate));
    snes_initialized_ = true;

    count_frequency = SDL_GetPerformanceFrequency();
    last_count = SDL_GetPerformanceCounter();
    time_adder = 0.0;
    frame_count_ = 0;
    fps_timer_ = 0.0;
    current_fps_ = 0.0;
    metric_history_head_ = 0;
    metric_history_count_ = 0;

    // Start emulator in running state by default
    // User can press Space to pause if needed
    running_ = true;
  }

  // Auto-pause emulator during window resize to prevent crashes
  // MODERN APPROACH: Only pause on actual window resize, not focus loss
  static bool was_running_before_resize = false;

  // Check if window is being resized (set in HandleEvents)
  if (yaze::core::g_window_is_resizing && running_) {
    was_running_before_resize = true;
    running_ = false;
  } else if (!yaze::core::g_window_is_resizing && !running_ &&
             was_running_before_resize) {
    // Auto-resume after resize completes
    running_ = true;
    was_running_before_resize = false;
  }

  // REMOVED: Aggressive focus-based pausing
  // Modern emulators (RetroArch, bsnes, etc.) continue running in background
  // Users can manually pause with Space if they want to save CPU/battery

  if (running_) {
    // NOTE: Input polling moved inside frame loops below to ensure fresh
    // input state for each SNES frame. This is critical for edge detection
    // (naming screen) when multiple SNES frames run per GUI frame.

    uint64_t current_count = SDL_GetPerformanceCounter();
    uint64_t delta = current_count - last_count;
    last_count = current_count;
    double seconds = delta / (double)count_frequency;
    time_adder += seconds;

    // Cap time accumulation to prevent spiral of death and improve stability
    if (time_adder > wanted_frames_ * 3.0) {
      time_adder = wanted_frames_ * 3.0;
    }

    // Track frames to skip for performance with progressive skip
    int frames_to_process = 0;
    while (time_adder >= wanted_frames_ - 0.002) {
      time_adder -= wanted_frames_;
      frames_to_process++;
    }

    // Progressive frame skip for smoother degradation:
    // - 1 frame behind: process normally
    // - 2-3 frames behind: process but skip some rendering
    // - 4+ frames behind: hard cap to prevent spiral of death
    int max_frames = 4;  // Hard cap
    if (frames_to_process > max_frames) {
      // When severely behind, drop extra accumulated time to catch up smoothly
      // This prevents the "spiral of death" where we never catch up
      time_adder = 0.0;
      frames_to_process = max_frames;
    }

    // Turbo mode: run many frames without timing constraints
    if (turbo_mode_ && snes_initialized_) {
      constexpr int kTurboFrames = 8;  // Run 8 frames per iteration (~480 fps)
      for (int i = 0; i < kTurboFrames; i++) {
        // Poll input BEFORE each frame for proper edge detection
        // Poll player 0 (controller 1) so JOY1* latches correct state
        input_manager_.Poll(&snes_, 0);
        snes_.RunFrame();
        frame_count_++;
      }
      // Reset timing to prevent catch-up spiral after turbo
      time_adder = 0.0;
      frames_to_process = 1;  // Still render one frame
    }

    if (snes_initialized_ && frames_to_process > 0) {
      // Process frames (skip rendering for all but last frame if falling
      // behind)
      for (int i = 0; i < frames_to_process; i++) {
        snes_.ResetFrameMetrics();
        uint64_t frame_start = SDL_GetPerformanceCounter();
        bool should_render = (i == frames_to_process - 1);
        uint32_t queued_frames = 0;
        float audio_rms_left = 0.0f;
        float audio_rms_right = 0.0f;

        // Poll input BEFORE each frame for proper edge detection
        // This ensures the game sees button release between frames
        // Critical for naming screen A button registration
        if (!turbo_mode_) {
          // Poll player 0 (controller 1) for correct JOY1* state
          input_manager_.Poll(&snes_, 0);
          snes_.RunFrame();
        }

        // Queue audio for every emulated frame (not just the rendered one) to
        // avoid starving the SDL queue when we process multiple frames while
        // behind.
        if (audio_backend_) {
          int16_t temp_audio_buffer[2048];
          int16_t* frame_buffer =
              audio_buffer_ ? audio_buffer_ : temp_audio_buffer;

          if (audio_stream_config_dirty_) {
            if (use_sdl_audio_stream_ &&
                audio_backend_->SupportsAudioStream()) {
              LOG_INFO(
                  "Emulator",
                  "Enabling audio stream resampling (32040Hz -> Device Rate)");
              audio_backend_->SetAudioStreamResampling(true, kNativeSampleRate,
                                                       2);
              audio_stream_active_ = true;
            } else {
              LOG_INFO("Emulator", "Disabling audio stream resampling");
              audio_backend_->SetAudioStreamResampling(false, kNativeSampleRate,
                                                       2);
              audio_stream_active_ = false;
            }
            audio_stream_config_dirty_ = false;
          }

          auto audio_status = audio_backend_->GetStatus();
          queued_frames = audio_status.queued_frames;

          const uint32_t samples_per_frame = wanted_samples_;
          const uint32_t max_buffer = samples_per_frame * 6;
          const uint32_t optimal_buffer = 2048;  // ~40ms target

          if (queued_frames < max_buffer) {
            // Generate samples for this emulated frame
            snes_.SetSamples(frame_buffer, wanted_samples_);

            if (should_render) {
              // Compute RMS only once per rendered frame for metrics
              const int num_samples = wanted_samples_ * 2;  // Stereo
              auto compute_rms = [&](int total_samples) {
                if (total_samples <= 0 || frame_buffer == nullptr) {
                  audio_rms_left = 0.0f;
                  audio_rms_right = 0.0f;
                  return;
                }
                double sum_l = 0.0;
                double sum_r = 0.0;
                const int frames = total_samples / 2;
                for (int s = 0; s < frames; ++s) {
                  const float l = static_cast<float>(frame_buffer[2 * s]);
                  const float r = static_cast<float>(frame_buffer[2 * s + 1]);
                  sum_l += l * l;
                  sum_r += r * r;
                }
                audio_rms_left =
                    frames > 0 ? std::sqrt(sum_l / frames) / 32768.0f : 0.0f;
                audio_rms_right =
                    frames > 0 ? std::sqrt(sum_r / frames) / 32768.0f : 0.0f;
              };
              compute_rms(num_samples);
            }

            // Dynamic Rate Control (DRC)
            int effective_rate = kNativeSampleRate;
            if (queued_frames > optimal_buffer + 256) {
              effective_rate += 60;  // subtle speed up
            } else if (queued_frames < optimal_buffer - 256) {
              effective_rate -= 60;  // subtle slow down
            }

            bool queue_ok = audio_backend_->QueueSamplesNative(
                frame_buffer, wanted_samples_, 2, effective_rate);

            if (!queue_ok && audio_backend_->SupportsAudioStream()) {
              // Try to re-enable resampling and retry once
              audio_backend_->SetAudioStreamResampling(true, kNativeSampleRate,
                                                       2);
              audio_stream_active_ = true;
              queue_ok = audio_backend_->QueueSamplesNative(
                  frame_buffer, wanted_samples_, 2, effective_rate);
            }

            if (!queue_ok) {
              // Drop audio rather than playing at wrong speed
              static int error_count = 0;
              if (++error_count % 300 == 0) {
                LOG_WARN(
                    "Emulator",
                    "Resampling failed, dropping audio to prevent 1.5x speed "
                    "(count: %d)",
                    error_count);
              }
            }
          } else {
            // Buffer overflow - skip this frame's audio
            static int overflow_count = 0;
            if (++overflow_count % 60 == 0) {
              LOG_WARN("Emulator",
                       "Audio buffer overflow (count: %d, queued: %u)",
                       overflow_count, queued_frames);
            }
          }
        }

        // Track FPS
        frame_count_++;
        fps_timer_ += wanted_frames_;
        if (fps_timer_ >= 1.0) {
          current_fps_ = frame_count_ / fps_timer_;
          frame_count_ = 0;
          fps_timer_ = 0.0;
        }

        // Only render UI/texture on the last frame
        if (should_render) {
          // Record frame timing and audio queue depth for plots
          {
            const uint64_t frame_end = SDL_GetPerformanceCounter();
            const double elapsed_ms =
                1000.0 * (static_cast<double>(frame_end - frame_start) /
                          static_cast<double>(count_frequency));
            PushFrameMetrics(static_cast<float>(elapsed_ms), queued_frames,
                             snes_.dma_bytes_frame(), snes_.vram_bytes_frame(),
                             audio_rms_left, audio_rms_right);
          }

          // Update PPU texture only on rendered frames
          void* ppu_pixels_;
          int ppu_pitch_;
          if (renderer_->LockTexture(ppu_texture_, NULL, &ppu_pixels_,
                                     &ppu_pitch_)) {
            snes_.SetPixels(static_cast<uint8_t*>(ppu_pixels_));
            renderer_->UnlockTexture(ppu_texture_);

#ifndef __EMSCRIPTEN__
            // WORKAROUND: Tiny delay after texture unlock to prevent macOS
            // Metal crash. macOS CoreAnimation/Metal driver bug in
            // layer_presented() callback. Without this, rapid texture updates
            // corrupt Metal's frame tracking.
            // NOTE: Not needed in WASM builds (WebGL doesn't have this issue)
            SDL_Delay(1);
#endif
          }
        }
      }
    }
  }

  RenderEmulatorInterface();
}

void Emulator::PushFrameMetrics(float frame_ms, uint32_t audio_frames,
                                uint64_t dma_bytes, uint64_t vram_bytes,
                                float audio_rms_left, float audio_rms_right) {
  frame_time_history_[metric_history_head_] = frame_ms;
  fps_history_[metric_history_head_] = static_cast<float>(current_fps_);
  audio_queue_history_[metric_history_head_] = static_cast<float>(audio_frames);
  dma_bytes_history_[metric_history_head_] = static_cast<float>(dma_bytes);
  vram_bytes_history_[metric_history_head_] = static_cast<float>(vram_bytes);
  audio_rms_left_history_[metric_history_head_] = audio_rms_left;
  audio_rms_right_history_[metric_history_head_] = audio_rms_right;
  metric_history_head_ = (metric_history_head_ + 1) % kMetricHistorySize;
  if (metric_history_count_ < kMetricHistorySize) {
    metric_history_count_++;
  }
}

namespace {
std::vector<float> CopyHistoryOrdered(
    const std::array<float, Emulator::kMetricHistorySize>& data, int head,
    int count) {
  std::vector<float> out;
  out.reserve(count);
  int start = (head - count + Emulator::kMetricHistorySize) %
              Emulator::kMetricHistorySize;
  for (int i = 0; i < count; ++i) {
    int idx = (start + i) % Emulator::kMetricHistorySize;
    out.push_back(data[idx]);
  }
  return out;
}
}  // namespace

std::vector<float> Emulator::FrameTimeHistory() const {
  return CopyHistoryOrdered(frame_time_history_, metric_history_head_,
                            metric_history_count_);
}

std::vector<float> Emulator::FpsHistory() const {
  return CopyHistoryOrdered(fps_history_, metric_history_head_,
                            metric_history_count_);
}

std::vector<float> Emulator::AudioQueueHistory() const {
  return CopyHistoryOrdered(audio_queue_history_, metric_history_head_,
                            metric_history_count_);
}

std::vector<float> Emulator::DmaBytesHistory() const {
  return CopyHistoryOrdered(dma_bytes_history_, metric_history_head_,
                            metric_history_count_);
}

std::vector<float> Emulator::VramBytesHistory() const {
  return CopyHistoryOrdered(vram_bytes_history_, metric_history_head_,
                            metric_history_count_);
}

std::vector<float> Emulator::AudioRmsLeftHistory() const {
  return CopyHistoryOrdered(audio_rms_left_history_, metric_history_head_,
                            metric_history_count_);
}

std::vector<float> Emulator::AudioRmsRightHistory() const {
  return CopyHistoryOrdered(audio_rms_right_history_, metric_history_head_,
                            metric_history_count_);
}

std::vector<float> Emulator::RomBankFreeBytes() const {
  constexpr size_t kBankSize = 0x8000;  // LoROM bank size (32KB)
  if (rom_data_.empty()) {
    return {};
  }
  const size_t bank_count = rom_data_.size() / kBankSize;
  std::vector<float> free_bytes;
  free_bytes.reserve(bank_count);
  for (size_t bank = 0; bank < bank_count; ++bank) {
    size_t free_count = 0;
    const size_t base = bank * kBankSize;
    for (size_t i = 0; i < kBankSize && (base + i) < rom_data_.size(); ++i) {
      if (rom_data_[base + i] == 0xFF) {
        free_count++;
      }
    }
    free_bytes.push_back(static_cast<float>(free_count));
  }
  return free_bytes;
}

void Emulator::RenderEmulatorInterface() {
  try {
    if (!panel_manager_)
      return;  // Panel registry must be injected

    static gui::PanelWindow cpu_card("CPU Debugger", ICON_MD_BUG_REPORT);
    static gui::PanelWindow ppu_card("PPU Viewer", ICON_MD_VIDEOGAME_ASSET);
    static gui::PanelWindow memory_card("Memory Viewer", ICON_MD_MEMORY);
    static gui::PanelWindow breakpoints_card("Breakpoints", ICON_MD_STOP);
    static gui::PanelWindow performance_card("Performance", ICON_MD_SPEED);
    static gui::PanelWindow ai_card("AI Agent", ICON_MD_SMART_TOY);
    static gui::PanelWindow save_states_card("Save States", ICON_MD_SAVE);
    static gui::PanelWindow keyboard_card("Keyboard Config", ICON_MD_KEYBOARD);
    static gui::PanelWindow apu_card("APU Debugger", ICON_MD_AUDIOTRACK);
    static gui::PanelWindow audio_card("Audio Mixer", ICON_MD_AUDIO_FILE);

    cpu_card.SetDefaultSize(400, 500);
    ppu_card.SetDefaultSize(550, 520);
    memory_card.SetDefaultSize(800, 600);
    breakpoints_card.SetDefaultSize(400, 350);
    performance_card.SetDefaultSize(350, 300);

    // Get visibility flags from registry and pass them to Begin() for proper X
    // button functionality This ensures each card window can be closed by the
    // user via the window close button
    bool* cpu_visible =
        panel_manager_->GetVisibilityFlag("emulator.cpu_debugger");
    if (cpu_visible && *cpu_visible) {
      if (cpu_card.Begin(cpu_visible)) {
        RenderModernCpuDebugger();
      }
      cpu_card.End();
    }

    bool* ppu_visible =
        panel_manager_->GetVisibilityFlag("emulator.ppu_viewer");
    if (ppu_visible && *ppu_visible) {
      if (ppu_card.Begin(ppu_visible)) {
        RenderNavBar();
        RenderSnesPpu();
      }
      ppu_card.End();
    }

    bool* memory_visible =
        panel_manager_->GetVisibilityFlag("emulator.memory_viewer");
    if (memory_visible && *memory_visible) {
      if (memory_card.Begin(memory_visible)) {
        RenderMemoryViewer();
      }
      memory_card.End();
    }

    bool* breakpoints_visible =
        panel_manager_->GetVisibilityFlag("emulator.breakpoints");
    if (breakpoints_visible && *breakpoints_visible) {
      if (breakpoints_card.Begin(breakpoints_visible)) {
        RenderBreakpointList();
      }
      breakpoints_card.End();
    }

    bool* performance_visible =
        panel_manager_->GetVisibilityFlag("emulator.performance");
    if (performance_visible && *performance_visible) {
      if (performance_card.Begin(performance_visible)) {
        RenderPerformanceMonitor();
      }
      performance_card.End();
    }

    bool* ai_agent_visible =
        panel_manager_->GetVisibilityFlag("emulator.ai_agent");
    if (ai_agent_visible && *ai_agent_visible) {
      if (ai_card.Begin(ai_agent_visible)) {
        RenderAIAgentPanel();
      }
      ai_card.End();
    }

    bool* save_states_visible =
        panel_manager_->GetVisibilityFlag("emulator.save_states");
    if (save_states_visible && *save_states_visible) {
      if (save_states_card.Begin(save_states_visible)) {
        RenderSaveStates();
      }
      save_states_card.End();
    }

    bool* keyboard_config_visible =
        panel_manager_->GetVisibilityFlag("emulator.keyboard_config");
    if (keyboard_config_visible && *keyboard_config_visible) {
      if (keyboard_card.Begin(keyboard_config_visible)) {
        RenderKeyboardConfig();
      }
      keyboard_card.End();
    }

    static gui::PanelWindow controller_card("Virtual Controller",
                                            ICON_MD_SPORTS_ESPORTS);
    controller_card.SetDefaultSize(250, 450);
    bool* virtual_controller_visible =
        panel_manager_->GetVisibilityFlag("emulator.virtual_controller");
    if (virtual_controller_visible && *virtual_controller_visible) {
      if (controller_card.Begin(virtual_controller_visible)) {
        ui::RenderVirtualController(this);
      }
      controller_card.End();
    }

    bool* apu_debugger_visible =
        panel_manager_->GetVisibilityFlag("emulator.apu_debugger");
    if (apu_debugger_visible && *apu_debugger_visible) {
      if (apu_card.Begin(apu_debugger_visible)) {
        RenderApuDebugger();
      }
      apu_card.End();
    }

    bool* audio_mixer_visible =
        panel_manager_->GetVisibilityFlag("emulator.audio_mixer");
    if (audio_mixer_visible && *audio_mixer_visible) {
      if (audio_card.Begin(audio_mixer_visible)) {
        RenderAudioMixer();
      }
      audio_card.End();
    }

  } catch (const std::exception& e) {
    // Fallback to basic UI if theming fails
    ImGui::Text("Error loading emulator UI: %s", e.what());
    if (ImGui::Button("Retry")) {
      // Force theme manager reinitialization
      auto& theme_manager = gui::ThemeManager::Get();
      theme_manager.InitializeBuiltInThemes();
    }
  }
}

void Emulator::RenderSnesPpu() {
  // Delegate to UI layer
  ui::RenderSnesPpu(this);
}

void Emulator::RenderNavBar() {
  // Delegate to UI layer
  ui::RenderNavBar(this);
}

// REMOVED: HandleEvents() - replaced by ui::InputHandler::Poll()
// The old ImGui::IsKeyPressed/Released approach was event-based and didn't work
// properly for continuous game input. Now using SDL_GetKeyboardState() for
// proper polling.

void Emulator::RenderBreakpointList() {
  // Delegate to UI layer
  ui::RenderBreakpointList(this);
}

void Emulator::RenderMemoryViewer() {
  // Delegate to UI layer
  ui::RenderMemoryViewer(this);
}

void Emulator::RenderModernCpuDebugger() {
  try {
    auto& theme_manager = gui::ThemeManager::Get();
    const auto& theme = theme_manager.GetCurrentTheme();

    // Debugger controls toolbar
    if (ImGui::Button(ICON_MD_PLAY_ARROW)) {
      running_ = true;
    }
    ImGui::SameLine();
    if (ImGui::Button(ICON_MD_PAUSE)) {
      running_ = false;
    }
    ImGui::SameLine();
    if (ImGui::Button(ICON_MD_SKIP_NEXT " Step")) {
      if (!running_)
        snes_.cpu().RunOpcode();
    }
    ImGui::SameLine();
    if (ImGui::Button(ICON_MD_REFRESH)) {
      snes_.Reset(true);
    }

    ImGui::Separator();

    // Breakpoint controls
    static char bp_addr[16] = "00FFD9";
    ImGui::Text(ICON_MD_BUG_REPORT " Breakpoints:");
    ImGui::PushItemWidth(100);
    ImGui::InputText("##BPAddr", bp_addr, IM_ARRAYSIZE(bp_addr),
                     ImGuiInputTextFlags_CharsHexadecimal |
                         ImGuiInputTextFlags_CharsUppercase);
    ImGui::PopItemWidth();
    ImGui::SameLine();
    if (ImGui::Button(ICON_MD_ADD " Add")) {
      uint32_t addr = std::strtoul(bp_addr, nullptr, 16);
      breakpoint_manager_.AddBreakpoint(addr, BreakpointManager::Type::EXECUTE,
                                        BreakpointManager::CpuType::CPU_65816,
                                        "",
                                        absl::StrFormat("BP at $%06X", addr));
    }

    // List breakpoints
    ImGui::BeginChild("##BPList", ImVec2(0, 100), true);
    for (const auto& bp : breakpoint_manager_.GetAllBreakpoints()) {
      if (bp.cpu == BreakpointManager::CpuType::CPU_65816) {
        bool enabled = bp.enabled;
        if (ImGui::Checkbox(absl::StrFormat("##en%d", bp.id).c_str(),
                            &enabled)) {
          breakpoint_manager_.SetEnabled(bp.id, enabled);
        }
        ImGui::SameLine();
        ImGui::Text("$%06X", bp.address);
        ImGui::SameLine();
        ImGui::TextDisabled("(hits: %d)", bp.hit_count);
        ImGui::SameLine();
        if (ImGui::SmallButton(
                absl::StrFormat(ICON_MD_DELETE "##%d", bp.id).c_str())) {
          breakpoint_manager_.RemoveBreakpoint(bp.id);
        }
      }
    }
    ImGui::EndChild();

    ImGui::Separator();

    ImGui::TextColored(ConvertColorToImVec4(theme.accent), "CPU Status");
    ImGui::PushStyleColor(ImGuiCol_ChildBg,
                          ConvertColorToImVec4(theme.child_bg));
    ImGui::BeginChild("##CpuStatus", ImVec2(0, 180), true);

    // Compact register display in a table
    if (ImGui::BeginTable(
            "Registers", 4,
            ImGuiTableFlags_Borders | ImGuiTableFlags_SizingFixedFit)) {
      ImGui::TableSetupColumn("Register", ImGuiTableColumnFlags_WidthFixed, 60);
      ImGui::TableSetupColumn("Value", ImGuiTableColumnFlags_WidthFixed, 80);
      ImGui::TableSetupColumn("Register", ImGuiTableColumnFlags_WidthFixed, 60);
      ImGui::TableSetupColumn("Value", ImGuiTableColumnFlags_WidthFixed, 80);
      ImGui::TableHeadersRow();

      ImGui::TableNextRow();
      ImGui::TableNextColumn();
      ImGui::Text("A");
      ImGui::TableNextColumn();
      ImGui::TextColored(ConvertColorToImVec4(theme.accent), "0x%04X",
                         snes_.cpu().A);
      ImGui::TableNextColumn();
      ImGui::Text("D");
      ImGui::TableNextColumn();
      ImGui::TextColored(ConvertColorToImVec4(theme.accent), "0x%04X",
                         snes_.cpu().D);

      ImGui::TableNextRow();
      ImGui::TableNextColumn();
      ImGui::Text("X");
      ImGui::TableNextColumn();
      ImGui::TextColored(ConvertColorToImVec4(theme.accent), "0x%04X",
                         snes_.cpu().X);
      ImGui::TableNextColumn();
      ImGui::Text("DB");
      ImGui::TableNextColumn();
      ImGui::TextColored(ConvertColorToImVec4(theme.accent), "0x%02X",
                         snes_.cpu().DB);

      ImGui::TableNextRow();
      ImGui::TableNextColumn();
      ImGui::Text("Y");
      ImGui::TableNextColumn();
      ImGui::TextColored(ConvertColorToImVec4(theme.accent), "0x%04X",
                         snes_.cpu().Y);
      ImGui::TableNextColumn();
      ImGui::Text("PB");
      ImGui::TableNextColumn();
      ImGui::TextColored(ConvertColorToImVec4(theme.accent), "0x%02X",
                         snes_.cpu().PB);

      ImGui::TableNextRow();
      ImGui::TableNextColumn();
      ImGui::Text("PC");
      ImGui::TableNextColumn();
      ImGui::TextColored(ConvertColorToImVec4(theme.success), "0x%04X",
                         snes_.cpu().PC);
      ImGui::TableNextColumn();
      ImGui::Text("SP");
      ImGui::TableNextColumn();
      ImGui::TextColored(ConvertColorToImVec4(theme.accent), "0x%02X",
                         snes_.memory().mutable_sp());

      ImGui::TableNextRow();
      ImGui::TableNextColumn();
      ImGui::Text("PS");
      ImGui::TableNextColumn();
      ImGui::TextColored(ConvertColorToImVec4(theme.warning), "0x%02X",
                         snes_.cpu().status);
      ImGui::TableNextColumn();
      ImGui::Text("Cycle");
      ImGui::TableNextColumn();
      ImGui::TextColored(ConvertColorToImVec4(theme.info), "%llu",
                         snes_.mutable_cycles());

      ImGui::EndTable();
    }

    ImGui::EndChild();
    ImGui::PopStyleColor();

    // SPC700 Status Panel
    ImGui::TextColored(ConvertColorToImVec4(theme.accent), "SPC700 Status");
    ImGui::PushStyleColor(ImGuiCol_ChildBg,
                          ConvertColorToImVec4(theme.child_bg));
    ImGui::BeginChild("##SpcStatus", ImVec2(0, 150), true);

    if (ImGui::BeginTable(
            "SPCRegisters", 4,
            ImGuiTableFlags_Borders | ImGuiTableFlags_SizingFixedFit)) {
      ImGui::TableSetupColumn("Register", ImGuiTableColumnFlags_WidthFixed, 50);
      ImGui::TableSetupColumn("Value", ImGuiTableColumnFlags_WidthFixed, 60);
      ImGui::TableSetupColumn("Register", ImGuiTableColumnFlags_WidthFixed, 50);
      ImGui::TableSetupColumn("Value", ImGuiTableColumnFlags_WidthFixed, 60);
      ImGui::TableHeadersRow();

      ImGui::TableNextRow();
      ImGui::TableNextColumn();
      ImGui::Text("A");
      ImGui::TableNextColumn();
      ImGui::TextColored(ConvertColorToImVec4(theme.accent), "0x%02X",
                         snes_.apu().spc700().A);
      ImGui::TableNextColumn();
      ImGui::Text("PC");
      ImGui::TableNextColumn();
      ImGui::TextColored(ConvertColorToImVec4(theme.success), "0x%04X",
                         snes_.apu().spc700().PC);

      ImGui::TableNextRow();
      ImGui::TableNextColumn();
      ImGui::Text("X");
      ImGui::TableNextColumn();
      ImGui::TextColored(ConvertColorToImVec4(theme.accent), "0x%02X",
                         snes_.apu().spc700().X);
      ImGui::TableNextColumn();
      ImGui::Text("SP");
      ImGui::TableNextColumn();
      ImGui::TextColored(ConvertColorToImVec4(theme.accent), "0x%02X",
                         snes_.apu().spc700().SP);

      ImGui::TableNextRow();
      ImGui::TableNextColumn();
      ImGui::Text("Y");
      ImGui::TableNextColumn();
      ImGui::TextColored(ConvertColorToImVec4(theme.accent), "0x%02X",
                         snes_.apu().spc700().Y);
      ImGui::TableNextColumn();
      ImGui::Text("PSW");
      ImGui::TableNextColumn();
      ImGui::TextColored(
          ConvertColorToImVec4(theme.warning), "0x%02X",
          snes_.apu().spc700().FlagsToByte(snes_.apu().spc700().PSW));

      ImGui::EndTable();
    }

    ImGui::EndChild();
    ImGui::PopStyleColor();

    // New Disassembly Viewer
    if (ImGui::CollapsingHeader("Disassembly Viewer",
                                ImGuiTreeNodeFlags_DefaultOpen)) {
      uint32_t current_pc =
          (static_cast<uint32_t>(snes_.cpu().PB) << 16) | snes_.cpu().PC;
      auto& disasm = snes_.cpu().disassembly_viewer();
      if (disasm.IsAvailable()) {
        disasm.Render(current_pc, snes_.cpu().breakpoints_);
      } else {
        ImGui::TextColored(ConvertColorToImVec4(theme.error),
                           "Disassembly viewer unavailable.");
      }
    }
  } catch (const std::exception& e) {
    // Ensure any pushed styles are popped on error
    try {
      ImGui::PopStyleColor();
    } catch (...) {
      // Ignore PopStyleColor errors
    }
    ImGui::Text("CPU Debugger Error: %s", e.what());
  }
}

void Emulator::RenderPerformanceMonitor() {
  // Delegate to UI layer
  ui::RenderPerformanceMonitor(this);
}

void Emulator::RenderAIAgentPanel() {
  // Delegate to UI layer
  ui::RenderAIAgentPanel(this);
}

void Emulator::RenderCpuInstructionLog(
    const std::vector<InstructionEntry>& instruction_log) {
  // Delegate to UI layer (legacy log deprecated)
  ui::RenderCpuInstructionLog(this, instruction_log.size());
}

void Emulator::RenderSaveStates() {
  // TODO: Create ui::RenderSaveStates() when save state system is implemented
  auto& theme_manager = gui::ThemeManager::Get();
  const auto& theme = theme_manager.GetCurrentTheme();

  ImGui::TextColored(ConvertColorToImVec4(theme.warning),
                     ICON_MD_SAVE " Save States - Coming Soon");
  ImGui::TextWrapped("Save state functionality will be implemented here.");
}

void Emulator::RenderKeyboardConfig() {
  // Delegate to the input manager UI
  ui::RenderKeyboardConfig(&input_manager_,
                           [this](const input::InputConfig& config) {
                             input_config_ = config;
                             if (input_config_changed_callback_) {
                               input_config_changed_callback_(config);
                             }
                           });
}

void Emulator::RenderApuDebugger() {
  // Delegate to UI layer
  ui::RenderApuDebugger(this);
}

void Emulator::RenderAudioMixer() {
  if (!audio_backend_)
    return;

  // Master Volume
  float volume = audio_backend_->GetVolume();
  if (ImGui::SliderFloat("Master Volume", &volume, 0.0f, 1.0f, "%.2f")) {
    audio_backend_->SetVolume(volume);
  }

  ImGui::Separator();
  ImGui::Text("Channel Mutes (Debug)");

  auto& dsp = snes_.apu().dsp();

  if (ImGui::BeginTable("AudioChannels", 4)) {
    for (int i = 0; i < 8; ++i) {
      ImGui::TableNextColumn();
      bool mute = dsp.GetChannelMute(i);
      std::string label = "Ch " + std::to_string(i + 1);
      if (ImGui::Checkbox(label.c_str(), &mute)) {
        dsp.SetChannelMute(i, mute);
      }
    }
    ImGui::EndTable();
  }

  ImGui::Separator();
  if (ImGui::Button("Mute All")) {
    for (int i = 0; i < 8; ++i)
      dsp.SetChannelMute(i, true);
  }
  ImGui::SameLine();
  if (ImGui::Button("Unmute All")) {
    for (int i = 0; i < 8; ++i)
      dsp.SetChannelMute(i, false);
  }
}

}  // namespace emu
}  // namespace yaze
