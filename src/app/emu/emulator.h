#ifndef YAZE_APP_CORE_EMULATOR_H
#define YAZE_APP_CORE_EMULATOR_H

#include <cstdint>
#include <vector>

#include "app/emu/snes.h"
#include "app/rom.h"
#include "imgui/imgui.h"

namespace yaze {

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
  void Run(Rom* rom);

  auto snes() -> Snes& { return snes_; }
  auto running() const -> bool { return running_; }
  void set_audio_buffer(int16_t* audio_buffer) { audio_buffer_ = audio_buffer; }
  auto set_audio_device_id(SDL_AudioDeviceID audio_device) {
    audio_device_ = audio_device;
  }
  auto wanted_samples() const -> int { return wanted_samples_; }

 private:
  void RenderNavBar();
  void HandleEvents();
  void RenderEmulatorInterface();

  void RenderSnesPpu();
  void RenderBreakpointList();
  void RenderMemoryViewer();

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
  float time_adder = 0.0;

  int16_t* audio_buffer_;
  SDL_AudioDeviceID audio_device_;

  Snes snes_;
  SDL_Texture* ppu_texture_;

  std::vector<uint8_t> rom_data_;

  EmulatorKeybindings keybindings_;
};

}  // namespace emu
}  // namespace yaze

#endif  // YAZE_APP_CORE_EMULATOR_H
