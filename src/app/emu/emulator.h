#ifndef YAZE_APP_CORE_EMULATOR_H
#define YAZE_APP_CORE_EMULATOR_H

#include <array>
#include <cstdint>
#include <functional>
#include <vector>

#include "app/emu/audio/audio_backend.h"
#include "app/emu/debug/breakpoint_manager.h"
#include "app/emu/debug/disassembly_viewer.h"
#include "app/emu/input/input_manager.h"
#include "app/emu/snes.h"
#include "rom/rom.h"

namespace yaze {
namespace gfx {
class IRenderer;
}  // namespace gfx

namespace editor {
class PanelManager;
}  // namespace editor

/**
 * @namespace yaze::emu
 * @brief SNES Emulation and debugging tools.
 */
namespace emu {

// REMOVED: EmulatorKeybindings (ImGuiKey-based)
// Now using ui::InputHandler with SDL_GetKeyboardState() for proper continuous
// polling

/**
 * @class Emulator
 * @brief A class for emulating and debugging SNES games.
 */
class Emulator {
 public:
  Emulator() = default;
  ~Emulator();
  void Initialize(gfx::IRenderer* renderer,
                  const std::vector<uint8_t>& rom_data);
  void Run(Rom* rom);
  void Cleanup();

  // Panel visibility managed by PanelManager (dependency injection)
  void set_panel_manager(editor::PanelManager* manager) {
    panel_manager_ = manager;
  }
  void SetInputConfig(const input::InputConfig& config);
  void set_input_config_changed_callback(
      std::function<void(const input::InputConfig&)> callback) {
    input_config_changed_callback_ = std::move(callback);
  }

  auto snes() -> Snes& { return snes_; }
  auto running() const -> bool { return running_; }
  void set_running(bool running) { running_ = running; }

  // Headless mode for background audio (music editor)
  // Initializes SNES and audio without requiring visible emulator window
  bool EnsureInitialized(Rom* rom);
  // Runs emulator frame without UI rendering (for background audio)
  void RunFrameOnly();
  // Runs audio-focused frame: CPU+APU cycles without PPU rendering
  // Used by MusicEditor for authentic, low-overhead audio playback
  void RunAudioFrame();
  // Reset frame timing (call before starting playback to prevent time buildup)
  void ResetFrameTiming();

  // Audio backend access
  audio::IAudioBackend* audio_backend() {
    return external_audio_backend_ ? external_audio_backend_ : audio_backend_.get();
  }
  void ResumeAudio(); // For WASM/WebAudio context resumption

  // Set an external audio backend (for sharing between emulator instances)
  // When set, this backend is used instead of the internal one
  void SetExternalAudioBackend(audio::IAudioBackend* backend) {
    external_audio_backend_ = backend;
  }
  void set_audio_buffer(int16_t* audio_buffer) { audio_buffer_ = audio_buffer; }
  auto set_audio_device_id(SDL_AudioDeviceID audio_device) {
    audio_device_ = audio_device;
  }
  void set_use_sdl_audio_stream(bool enabled);
  bool use_sdl_audio_stream() const { return use_sdl_audio_stream_; }
  // Mark audio stream as already configured (prevents RunAudioFrame from overriding)
  void mark_audio_stream_configured() {
    audio_stream_config_dirty_ = false;
    audio_stream_active_ = true;
  }
  auto wanted_samples() const -> int { return wanted_samples_; }
  auto wanted_frames() const -> float { return wanted_frames_; }
  void set_renderer(gfx::IRenderer* renderer) { renderer_ = renderer; }

  // Render access
  gfx::IRenderer* renderer() { return renderer_; }
  void* ppu_texture() { return ppu_texture_; }

  // Turbo mode
  bool is_turbo_mode() const { return turbo_mode_; }
  void set_turbo_mode(bool turbo) { turbo_mode_ = turbo; }

  // Audio focus mode - use RunAudioFrame() for lower overhead audio playback
  bool is_audio_focus_mode() const { return audio_focus_mode_; }
  void set_audio_focus_mode(bool focus) { audio_focus_mode_ = focus; }

  // Audio settings
  void set_interpolation_type(int type);
  int get_interpolation_type() const;

  // Debugger access
  BreakpointManager& breakpoint_manager() { return breakpoint_manager_; }
  debug::DisassemblyViewer& disassembly_viewer() { return disassembly_viewer_; }
  input::InputManager& input_manager() { return input_manager_; }
  bool is_debugging() const { return debugging_; }
  void set_debugging(bool debugging) { debugging_ = debugging; }
  bool is_initialized() const { return initialized_; }
  bool is_snes_initialized() const { return snes_initialized_; }

  // AI Agent Integration API
  bool IsEmulatorReady() const { return snes_.running() && !rom_data_.empty(); }
  double GetCurrentFPS() const { return current_fps_; }
  uint64_t GetCurrentCycle() { return snes_.mutable_cycles(); }
  uint16_t GetCPUPC() { return snes_.cpu().PC; }
  uint8_t GetCPUB() { return snes_.cpu().DB; }
  void StepSingleInstruction() { snes_.cpu().RunOpcode(); }
  void SetBreakpoint(uint32_t address) { snes_.cpu().SetBreakpoint(address); }
  void ClearAllBreakpoints() { snes_.cpu().ClearBreakpoints(); }
  std::vector<uint32_t> GetBreakpoints() {
    return snes_.cpu().GetBreakpoints();
  }

  // Performance monitoring for AI agents
  struct EmulatorMetrics {
    double fps;
    uint64_t cycles;
    uint32_t audio_frames_queued;
    bool is_running;
    uint16_t cpu_pc;
    uint8_t cpu_pb;
  };
  EmulatorMetrics GetMetrics() {
    return {.fps = current_fps_,
            .cycles = snes_.mutable_cycles(),
            .audio_frames_queued =
                audio_backend_ ? audio_backend_->GetStatus().queued_frames : 0,
            .is_running = running_,
            .cpu_pc = snes_.cpu().PC,
            .cpu_pb = snes_.cpu().PB};
  }

  // Performance history (for ImPlot)
  std::vector<float> FrameTimeHistory() const;
  std::vector<float> FpsHistory() const;
  std::vector<float> AudioQueueHistory() const;
  std::vector<float> DmaBytesHistory() const;
  std::vector<float> VramBytesHistory() const;
  std::vector<float> AudioRmsLeftHistory() const;
  std::vector<float> AudioRmsRightHistory() const;
  std::vector<float> RomBankFreeBytes() const;

 private:
  void RenderNavBar();
  void RenderEmulatorInterface();

  void RenderSnesPpu();
  void RenderBreakpointList();
  void RenderMemoryViewer();
  void RenderModernCpuDebugger();
  void RenderPerformanceMonitor();
  void RenderAIAgentPanel();
  void RenderSaveStates();
  void RenderKeyboardConfig();
  void RenderApuDebugger();
  void RenderAudioMixer();

  struct Bookmark {
    std::string name;
    uint64_t value;
  };
  std::vector<Bookmark> bookmarks;

  void RenderCpuInstructionLog(
      const std::vector<InstructionEntry>& instructionLog);

  bool step_ = true;
  bool power_ = false;
  bool loading_ = false;
  bool running_ = false;
  bool turbo_mode_ = false;
  bool audio_focus_mode_ = false;  // Skip PPU rendering for audio playback

  float wanted_frames_;
  int wanted_samples_;

  uint8_t manual_pb_ = 0;
  uint16_t manual_pc_ = 0;

  // timing
  uint64_t count_frequency;
  uint64_t last_count;
  double time_adder = 0.0;

  // FPS tracking
  int frame_count_ = 0;
  double fps_timer_ = 0.0;
  double current_fps_ = 0.0;

  // Recent history for plotting (public for helper functions)
 public:
  static constexpr int kMetricHistorySize = 240;
 private:
  std::array<float, kMetricHistorySize> frame_time_history_{};
  std::array<float, kMetricHistorySize> fps_history_{};
  std::array<float, kMetricHistorySize> audio_queue_history_{};
  std::array<float, kMetricHistorySize> dma_bytes_history_{};
  std::array<float, kMetricHistorySize> vram_bytes_history_{};
  std::array<float, kMetricHistorySize> audio_rms_left_history_{};
  std::array<float, kMetricHistorySize> audio_rms_right_history_{};
  int metric_history_head_ = 0;
  int metric_history_count_ = 0;
  void PushFrameMetrics(float frame_ms, uint32_t audio_frames,
                        uint64_t dma_bytes, uint64_t vram_bytes,
                        float audio_rms_left, float audio_rms_right);

  int16_t* audio_buffer_;
  SDL_AudioDeviceID audio_device_;

  // Audio backend abstraction
  std::unique_ptr<audio::IAudioBackend> audio_backend_;
  audio::IAudioBackend* external_audio_backend_ = nullptr;  // Shared backend (not owned)

  Snes snes_;
  bool initialized_ = false;
  bool snes_initialized_ = false;
  bool debugging_ = false;
  gfx::IRenderer* renderer_ = nullptr;
  void* ppu_texture_ = nullptr;
  bool use_sdl_audio_stream_ = true;  // Enable resampling by default (32kHz -> 48kHz)
  bool audio_stream_config_dirty_ = true;  // Start dirty to ensure setup on first use
  bool audio_stream_active_ = false;
  bool audio_stream_env_checked_ = false;

  // Panel visibility managed by EditorPanelManager - no member variables needed!

  // Debugger infrastructure
  BreakpointManager breakpoint_manager_;
  debug::DisassemblyViewer disassembly_viewer_;

  std::vector<uint8_t> rom_data_;

  // Input handling (abstracted for SDL2/SDL3/custom backends)
  input::InputManager input_manager_;
  input::InputConfig input_config_;
  std::function<void(const input::InputConfig&)> input_config_changed_callback_;

  // Panel manager for card visibility (injected)
  editor::PanelManager* panel_manager_ = nullptr;
};

}  // namespace emu
}  // namespace yaze

#endif  // YAZE_APP_CORE_EMULATOR_H
