#ifndef YAZE_APP_CORE_EMULATOR_H
#define YAZE_APP_CORE_EMULATOR_H

#include <cstdint>
#include <vector>

#include "app/emu/snes.h"
#include "app/emu/audio/audio_backend.h"
#include "app/emu/debug/breakpoint_manager.h"
#include "app/gui/app/editor_card_manager.h"
#include "app/emu/debug/disassembly_viewer.h"
#include "app/emu/input/input_manager.h"
#include "app/rom.h"

namespace yaze {
namespace gfx {
class IRenderer;
} // namespace gfx

/**
 * @namespace yaze::emu
 * @brief SNES Emulation and debugging tools.
 */
namespace emu {

// REMOVED: EmulatorKeybindings (ImGuiKey-based)
// Now using ui::InputHandler with SDL_GetKeyboardState() for proper continuous polling

/**
 * @class Emulator
 * @brief A class for emulating and debugging SNES games.
 */
class Emulator {
 public:
  Emulator() = default;
  ~Emulator();
  void Initialize(gfx::IRenderer* renderer, const std::vector<uint8_t>& rom_data);
  void Run(Rom* rom);
  void Cleanup();
  
  // Card visibility managed by EditorCardManager

  auto snes() -> Snes& { return snes_; }
  auto running() const -> bool { return running_; }
  void set_running(bool running) { running_ = running; }
  
  // Audio backend access
  audio::IAudioBackend* audio_backend() { return audio_backend_.get(); }
  void set_audio_buffer(int16_t* audio_buffer) { audio_buffer_ = audio_buffer; }
  auto set_audio_device_id(SDL_AudioDeviceID audio_device) {
    audio_device_ = audio_device;
  }
  void set_use_sdl_audio_stream(bool enabled);
  bool use_sdl_audio_stream() const { return use_sdl_audio_stream_; }
  auto wanted_samples() const -> int { return wanted_samples_; }
  void set_renderer(gfx::IRenderer* renderer) { renderer_ = renderer; }
  
  // Render access
  gfx::IRenderer* renderer() { return renderer_; }
  void* ppu_texture() { return ppu_texture_; }
  
  // Turbo mode
  bool is_turbo_mode() const { return turbo_mode_; }
  void set_turbo_mode(bool turbo) { turbo_mode_ = turbo; }
  
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
  std::vector<uint32_t> GetBreakpoints() { return snes_.cpu().GetBreakpoints(); }
  
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
    return {
      .fps = current_fps_,
      .cycles = snes_.mutable_cycles(),
      .audio_frames_queued = SDL_GetQueuedAudioSize(audio_device_) / (wanted_samples_ * 4),
      .is_running = running_,
      .cpu_pc = snes_.cpu().PC,
      .cpu_pb = snes_.cpu().PB
    };
  }

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

  int16_t* audio_buffer_;
  SDL_AudioDeviceID audio_device_;
  
  // Audio backend abstraction
  std::unique_ptr<audio::IAudioBackend> audio_backend_;

  Snes snes_;
  bool initialized_ = false;
  bool snes_initialized_ = false;
  bool debugging_ = false;
  gfx::IRenderer* renderer_ = nullptr;
  void* ppu_texture_ = nullptr;
  bool use_sdl_audio_stream_ = false;
  bool audio_stream_config_dirty_ = false;
  bool audio_stream_active_ = false;
  bool audio_stream_env_checked_ = false;
  
  // Card visibility managed by EditorCardManager - no member variables needed!
  
  // Debugger infrastructure
  BreakpointManager breakpoint_manager_;
  debug::DisassemblyViewer disassembly_viewer_;

  std::vector<uint8_t> rom_data_;

  // Input handling (abstracted for SDL2/SDL3/custom backends)
  input::InputManager input_manager_;
};

}  // namespace emu
}  // namespace yaze

#endif  // YAZE_APP_CORE_EMULATOR_H
