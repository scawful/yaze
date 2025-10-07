#ifndef YAZE_APP_CORE_EMULATOR_H
#define YAZE_APP_CORE_EMULATOR_H

#include <cstdint>
#include <vector>

#include "app/emu/snes.h"
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

struct EmulatorKeybindings {
  ImGuiKey a_button = ImGuiKey_Z;
  ImGuiKey b_button = ImGuiKey_A;
  ImGuiKey x_button = ImGuiKey_S;
  ImGuiKey y_button = ImGuiKey_X;
  ImGuiKey l_button = ImGuiKey_Q;
  ImGuiKey r_button = ImGuiKey_W;
  ImGuiKey start_button = ImGuiKey_Enter;
  ImGuiKey select_button = ImGuiKey_Backspace;
  ImGuiKey up_button = ImGuiKey_UpArrow;
  ImGuiKey down_button = ImGuiKey_DownArrow;
  ImGuiKey left_button = ImGuiKey_LeftArrow;
  ImGuiKey right_button = ImGuiKey_RightArrow;
};

/**
 * @class Emulator
 * @brief A class for emulating and debugging SNES games.
 */
class Emulator {
 public:
  Emulator() = default;
  ~Emulator() = default;
  void Initialize(gfx::IRenderer* renderer, const std::vector<uint8_t>& rom_data);
  void Run(Rom* rom);

  auto snes() -> Snes& { return snes_; }
  auto running() const -> bool { return running_; }
  void set_audio_buffer(int16_t* audio_buffer) { audio_buffer_ = audio_buffer; }
  auto set_audio_device_id(SDL_AudioDeviceID audio_device) {
    audio_device_ = audio_device;
  }
  auto wanted_samples() const -> int { return wanted_samples_; }
  void set_renderer(gfx::IRenderer* renderer) { renderer_ = renderer; }
  
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
  void HandleEvents();
  void RenderEmulatorInterface();

  void RenderSnesPpu();
  void RenderBreakpointList();
  void RenderMemoryViewer();
  void RenderModernCpuDebugger();
  void RenderPerformanceMonitor();
  void RenderAIAgentPanel();
  void RenderSaveStates();
  void RenderKeyboardConfig();

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

  Snes snes_;
  bool initialized_ = false;
  bool snes_initialized_ = false;
  gfx::IRenderer* renderer_ = nullptr;
  void* ppu_texture_ = nullptr;

  std::vector<uint8_t> rom_data_;

  EmulatorKeybindings keybindings_;
};

}  // namespace emu
}  // namespace yaze

#endif  // YAZE_APP_CORE_EMULATOR_H
