#include "app/emu/emulator.h"

#include <cmath>
#include <cstdint>
#include <cstdlib>
#include <fstream>
#include <vector>

#include "app/editor/system/editor_card_registry.h"
#include "app/platform/window.h"
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
constexpr int kNativeSampleRate = 32000;
}

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
    auto* wasm_backend = static_cast<audio::WasmAudioBackend*>(audio_backend_.get());
    wasm_backend->HandleUserInteraction();
  }
#endif
}

void Emulator::set_interpolation_type(int type) {
  if (!snes_initialized_) return;
  // Clamp to valid range (0-3)
  int safe_type = std::clamp(type, 0, 3);
  snes_.apu().dsp().interpolation_type = static_cast<InterpolationType>(safe_type);
}

int Emulator::get_interpolation_type() const {
  if (!snes_initialized_) return 0; // Default to Linear if not initialized
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

  // Cards are registered in EditorManager::Initialize() to avoid duplication

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
    config.buffer_frames = 1024;
    config.format = audio::SampleFormat::INT16;

    if (!audio_backend_->Initialize(config)) {
      LOG_ERROR("Emulator", "Failed to initialize audio backend");
      return false;
    }

    // Enable audio stream resampling for proper 32kHz -> 48kHz conversion
    if (audio_backend_->SupportsAudioStream()) {
      audio_backend_->SetAudioStreamResampling(true, kNativeSampleRate, 2);
    }
    LOG_INFO("Emulator", "Audio backend initialized for headless mode");
  }

  // Initialize SNES if not already done
  if (!snes_initialized_) {
    if (rom_data_.empty()) {
      rom_data_ = rom->vector();
    }
    snes_.Init(rom_data_);

    wanted_frames_ = 1.0 / (snes_.memory().pal_timing() ? 50.0 : 60.0);
    wanted_samples_ = audio_backend_->GetConfig().sample_rate / (snes_.memory().pal_timing() ? 50 : 60);
    snes_initialized_ = true;

    count_frequency = SDL_GetPerformanceFrequency();
    last_count = SDL_GetPerformanceCounter();
    time_adder = 0.0;

    LOG_INFO("Emulator", "SNES initialized for headless mode");
  }

  return true;
}

void Emulator::RunFrameOnly() {
  if (!snes_initialized_ || !running_) {
    return;
  }

  // Calculate timing
  uint64_t current_count = SDL_GetPerformanceCounter();
  uint64_t delta = current_count - last_count;
  last_count = current_count;
  double seconds = delta / (double)count_frequency;

  // Note: playback_speed_ is NOT applied to frame timing here.
  // Variable speed requires proper audio time-stretching or tempo control.
  // For now, run at real-time to ensure consistent audio sample generation.
  time_adder += seconds;

  // Cap time accumulation to prevent runaway (max 2 frames worth)
  double max_accumulation = wanted_frames_ * 2.0;
  if (time_adder > max_accumulation) {
    time_adder = max_accumulation;
  }

  // Process frames - limit to 2 frames max per update to prevent fast-forward
  int frames_processed = 0;
  constexpr int kMaxFramesPerUpdate = 2;

  // Local buffer for native sample path (641 * 2 channels = 1282 samples max)
  static int16_t native_audio_buffer[1284];

  while (time_adder >= wanted_frames_ && frames_processed < kMaxFramesPerUpdate) {
    time_adder -= wanted_frames_;
    frames_processed++;

    // Note: NewFrame() is called internally by Snes::RunCycle() at vblank start
    // Do NOT call it here - duplicate calls reset the frame boundary incorrectly

    // Run SNES frame (generates audio samples)
    snes_.RunFrame();

    // Queue audio samples
    if (audio_backend_) {
      auto status = audio_backend_->GetStatus();
      const int native_per_frame = snes_.memory().pal_timing() ? 641 : 534;

      if (status.queued_frames < static_cast<uint32_t>(native_per_frame * 4)) {
        // Try native sample rate path first (proper 32kHz -> 48kHz resampling)
        const int frames_native = snes_.apu().dsp().CopyNativeFrame(
            native_audio_buffer, snes_.memory().pal_timing());
        bool queue_ok = audio_backend_->QueueSamplesNative(
            native_audio_buffer, frames_native, 2, kNativeSampleRate);

        // Fallback to resampled path if native fails
        if (!queue_ok) {
          snes_.SetSamples(native_audio_buffer, wanted_samples_);
          audio_backend_->QueueSamples(native_audio_buffer, wanted_samples_ * 2);
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
  if (audio_backend_) {
    audio_backend_->Clear();
  }
}

void Emulator::RunAudioFrame() {
  // Audio-focused frame execution for music editor
  // Runs CPU+APU without PPU rendering for lower overhead and authentic sound

  if (!snes_initialized_ || !running_) {
    return;
  }

  // Calculate timing
  uint64_t current_count = SDL_GetPerformanceCounter();
  uint64_t delta = current_count - last_count;
  last_count = current_count;
  double seconds = delta / (double)count_frequency;

  // Note: playback_speed_ is NOT applied to frame timing here.
  // Variable speed requires proper audio time-stretching or tempo control,
  // which would need N-SPC driver modification. For now, run at real-time
  // to ensure consistent audio sample generation.
  time_adder += seconds;

  // Cap time accumulation to prevent runaway (max 2 frames worth)
  double max_accumulation = wanted_frames_ * 2.0;
  if (time_adder > max_accumulation) {
    time_adder = max_accumulation;
  }

  // Process frames - limit to 2 frames max per update
  int frames_processed = 0;
  constexpr int kMaxFramesPerUpdate = 2;

  // Local buffer for native sample path
  static int16_t native_audio_buffer[1284];

  while (time_adder >= wanted_frames_ && frames_processed < kMaxFramesPerUpdate) {
    time_adder -= wanted_frames_;
    frames_processed++;

    // Note: NewFrame() is called internally by Snes::RunCycle() at vblank start
    // No need to call it here - duplicate calls can cause timing issues

    // Run SNES audio-focused frame (skips PPU rendering)
    snes_.RunAudioFrame();

    // Queue audio samples
    if (audio_backend_) {
      auto status = audio_backend_->GetStatus();
      const int native_per_frame = snes_.memory().pal_timing() ? 641 : 534;

      if (status.queued_frames < static_cast<uint32_t>(native_per_frame * 4)) {
        // Use native 32kHz sample rate path for authentic sound
        const int frames_native = snes_.apu().dsp().CopyNativeFrame(
            native_audio_buffer, snes_.memory().pal_timing());
        bool queue_ok = audio_backend_->QueueSamplesNative(
            native_audio_buffer, frames_native, 2, kNativeSampleRate);

        // Fallback to resampled path if native fails
        if (!queue_ok) {
          snes_.SetSamples(native_audio_buffer, wanted_samples_);
          audio_backend_->QueueSamples(native_audio_buffer, wanted_samples_ * 2);
        }
      }
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

    wanted_frames_ = 1.0 / (snes_.memory().pal_timing() ? 50.0 : 60.0);
    wanted_samples_ = audio_backend_->GetConfig().sample_rate / (snes_.memory().pal_timing() ? 50 : 60);
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

        // Track FPS
        frame_count_++;
        fps_timer_ += wanted_frames_;
        if (fps_timer_ >= 1.0) {
          current_fps_ = frame_count_ / fps_timer_;
          frame_count_ = 0;
          fps_timer_ = 0.0;
        }

        // Only render and handle audio on the last frame
        if (should_render) {
          // SMOOTH AUDIO BUFFERING
          // Strategy: Always queue samples, never drop. Use dynamic rate
          // control to keep buffer at target level. This prevents pops and
          // glitches.

          if (audio_backend_) {
            if (audio_stream_config_dirty_) {
              if (use_sdl_audio_stream_ &&
                  audio_backend_->SupportsAudioStream()) {
                audio_backend_->SetAudioStreamResampling(true,
                                                         kNativeSampleRate, 2);
                audio_stream_active_ = true;
              } else {
                audio_backend_->SetAudioStreamResampling(false,
                                                         kNativeSampleRate, 2);
                audio_stream_active_ = false;
              }
              audio_stream_config_dirty_ = false;
            }

            const bool use_native_stream =
                use_sdl_audio_stream_ && audio_stream_active_ &&
                audio_backend_->SupportsAudioStream();

            auto audio_status = audio_backend_->GetStatus();
            queued_frames = audio_status.queued_frames;

            // Synchronize DSP frame boundary for resampling
            snes_.apu().dsp().NewFrame();

            // Target buffer: 2.0 frames for low latency with safety margin
            const uint32_t max_buffer = wanted_samples_ * 4;

            if (queued_frames < max_buffer) {
              bool queue_ok = true;
              auto compute_rms = [&](int total_samples) {
                if (total_samples <= 0 || audio_buffer_ == nullptr) {
                  audio_rms_left = 0.0f;
                  audio_rms_right = 0.0f;
                  return;
                }
                double sum_l = 0.0;
                double sum_r = 0.0;
                const int frames = total_samples / 2;
                for (int s = 0; s < frames; ++s) {
                  const float l = static_cast<float>(audio_buffer_[2 * s]);
                  const float r = static_cast<float>(audio_buffer_[2 * s + 1]);
                  sum_l += l * l;
                  sum_r += r * r;
                }
                audio_rms_left =
                    frames > 0 ? std::sqrt(sum_l / frames) / 32768.0f : 0.0f;
                audio_rms_right =
                    frames > 0 ? std::sqrt(sum_r / frames) / 32768.0f : 0.0f;
              };

              if (use_native_stream) {
                const int frames_native = snes_.apu().dsp().CopyNativeFrame(
                    audio_buffer_, snes_.memory().pal_timing());
                compute_rms(frames_native * 2);
                queue_ok = audio_backend_->QueueSamplesNative(
                    audio_buffer_, frames_native, 2, kNativeSampleRate);
              } else {
                snes_.SetSamples(audio_buffer_, wanted_samples_);
                const int num_samples = wanted_samples_ * 2;  // Stereo
                compute_rms(num_samples);
                queue_ok =
                    audio_backend_->QueueSamples(audio_buffer_, num_samples);
              }

              if (!queue_ok && use_native_stream) {
                snes_.SetSamples(audio_buffer_, wanted_samples_);
                const int num_samples = wanted_samples_ * 2;
                queue_ok =
                    audio_backend_->QueueSamples(audio_buffer_, num_samples);
              }

              if (!queue_ok) {
                static int error_count = 0;
                if (++error_count % 300 == 0) {
                  LOG_WARN("Emulator",
                           "Failed to queue audio (count: %d, stream=%s)",
                           error_count, use_native_stream ? "SDL" : "manual");
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

          // Record frame timing and audio queue depth for plots
          {
            const uint64_t frame_end = SDL_GetPerformanceCounter();
            const double elapsed_ms =
                1000.0 *
                (static_cast<double>(frame_end - frame_start) /
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
  audio_queue_history_[metric_history_head_] =
      static_cast<float>(audio_frames);
  dma_bytes_history_[metric_history_head_] =
      static_cast<float>(dma_bytes);
  vram_bytes_history_[metric_history_head_] =
      static_cast<float>(vram_bytes);
  audio_rms_left_history_[metric_history_head_] = audio_rms_left;
  audio_rms_right_history_[metric_history_head_] = audio_rms_right;
  metric_history_head_ =
      (metric_history_head_ + 1) % kMetricHistorySize;
  if (metric_history_count_ < kMetricHistorySize) {
    metric_history_count_++;
  }
}

namespace {
std::vector<float> CopyHistoryOrdered(const std::array<float, Emulator::kMetricHistorySize>& data,
                                      int head, int count) {
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
    if (!card_registry_)
      return;  // Card registry must be injected

    static gui::EditorCard cpu_card("CPU Debugger", ICON_MD_MEMORY);
    static gui::EditorCard ppu_card("PPU Viewer", ICON_MD_VIDEOGAME_ASSET);
    static gui::EditorCard memory_card("Memory Viewer", ICON_MD_MEMORY);
    static gui::EditorCard breakpoints_card("Breakpoints", ICON_MD_STOP);
    static gui::EditorCard performance_card("Performance", ICON_MD_SPEED);
    static gui::EditorCard ai_card("AI Agent", ICON_MD_SMART_TOY);
    static gui::EditorCard save_states_card("Save States", ICON_MD_SAVE);
    static gui::EditorCard keyboard_card("Keyboard Config", ICON_MD_KEYBOARD);
    static gui::EditorCard apu_card("APU Debugger", ICON_MD_MUSIC_NOTE);
    static gui::EditorCard audio_card("Audio Mixer", ICON_MD_AUDIO_FILE);

    cpu_card.SetDefaultSize(400, 500);
    ppu_card.SetDefaultSize(550, 520);
    memory_card.SetDefaultSize(800, 600);
    breakpoints_card.SetDefaultSize(400, 350);
    performance_card.SetDefaultSize(350, 300);

    // Get visibility flags from registry and pass them to Begin() for proper X
    // button functionality This ensures each card window can be closed by the
    // user via the window close button
    bool* cpu_visible =
        card_registry_->GetVisibilityFlag("emulator.cpu_debugger");
    if (cpu_visible && *cpu_visible) {
      if (cpu_card.Begin(cpu_visible)) {
        RenderModernCpuDebugger();
      }
      cpu_card.End();
    }

    bool* ppu_visible =
        card_registry_->GetVisibilityFlag("emulator.ppu_viewer");
    if (ppu_visible && *ppu_visible) {
      if (ppu_card.Begin(ppu_visible)) {
        RenderNavBar();
        RenderSnesPpu();
      }
      ppu_card.End();
    }

    bool* memory_visible =
        card_registry_->GetVisibilityFlag("emulator.memory_viewer");
    if (memory_visible && *memory_visible) {
      if (memory_card.Begin(memory_visible)) {
        RenderMemoryViewer();
      }
      memory_card.End();
    }

    bool* breakpoints_visible =
        card_registry_->GetVisibilityFlag("emulator.breakpoints");
    if (breakpoints_visible && *breakpoints_visible) {
      if (breakpoints_card.Begin(breakpoints_visible)) {
        RenderBreakpointList();
      }
      breakpoints_card.End();
    }

    bool* performance_visible =
        card_registry_->GetVisibilityFlag("emulator.performance");
    if (performance_visible && *performance_visible) {
      if (performance_card.Begin(performance_visible)) {
        RenderPerformanceMonitor();
      }
      performance_card.End();
    }

    bool* ai_agent_visible =
        card_registry_->GetVisibilityFlag("emulator.ai_agent");
    if (ai_agent_visible && *ai_agent_visible) {
      if (ai_card.Begin(ai_agent_visible)) {
        RenderAIAgentPanel();
      }
      ai_card.End();
    }

    bool* save_states_visible =
        card_registry_->GetVisibilityFlag("emulator.save_states");
    if (save_states_visible && *save_states_visible) {
      if (save_states_card.Begin(save_states_visible)) {
        RenderSaveStates();
      }
      save_states_card.End();
    }

    bool* keyboard_config_visible =
        card_registry_->GetVisibilityFlag("emulator.keyboard_config");
    if (keyboard_config_visible && *keyboard_config_visible) {
      if (keyboard_card.Begin(keyboard_config_visible)) {
        RenderKeyboardConfig();
      }
      keyboard_card.End();
    }

    static gui::EditorCard controller_card("Virtual Controller",
                                           ICON_MD_SPORTS_ESPORTS);
    controller_card.SetDefaultSize(250, 450);
    bool* virtual_controller_visible =
        card_registry_->GetVisibilityFlag("emulator.virtual_controller");
    if (virtual_controller_visible && *virtual_controller_visible) {
      if (controller_card.Begin(virtual_controller_visible)) {
        ui::RenderVirtualController(this);
      }
      controller_card.End();
    }

    bool* apu_debugger_visible =
        card_registry_->GetVisibilityFlag("emulator.apu_debugger");
    if (apu_debugger_visible && *apu_debugger_visible) {
      if (apu_card.Begin(apu_debugger_visible)) {
        RenderApuDebugger();
      }
      apu_card.End();
    }

    bool* audio_mixer_visible =
        card_registry_->GetVisibilityFlag("emulator.audio_mixer");
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
  ui::RenderKeyboardConfig(
      &input_manager_,
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
  if (!audio_backend_) return;

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
    for (int i = 0; i < 8; ++i) dsp.SetChannelMute(i, true);
  }
  ImGui::SameLine();
  if (ImGui::Button("Unmute All")) {
    for (int i = 0; i < 8; ++i) dsp.SetChannelMute(i, false);
  }
}

}  // namespace emu
}  // namespace yaze
