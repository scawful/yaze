#ifndef YAZE_APP_CORE_EMULATOR_H
#define YAZE_APP_CORE_EMULATOR_H

#include "imgui/imgui.h"

#include <cstdint>
#include <vector>

#include "app/emu/snes.h"
#include "app/gui/zeml.h"
#include "app/rom.h"

namespace yaze {
namespace app {

/**
 * @namespace yaze::app::emu
 * @brief SNES Emulation and debugging tools.
 */
namespace emu {

/**
 * @class Emulator
 * @brief A class for emulating and debugging SNES games.
 */
class Emulator : public SharedRom {
 public:
  Emulator() {
    std::string emulator_layout = R"(
      Table id="Emulator" count="2" flags="Resizable|ScrollY" {
        TableSetupColumn title="CPU",
        TableSetupColumn title="PPU",

        TableHeadersRow,
        TableNextColumn,

        CollapsingHeader id="cpuState" title="Register Values" flags="DefaultOpen" {
          BeginChild id="##CpuState" size="0,100" flags="NoMove|NoScrollbar" {
            Columns id="registersColumns" count="2" {
              Text text="A: 0x%04X" data="cpu.A",
              Text text="D: 0x%04X" data="cpu.D",
              Text text="X: 0x%04X" data="cpu.X",
              Text text="DB: 0x%02X" data="cpu.DB",
              Text text="Y: 0x%04X" data="cpu.Y",
              Text text="PB: 0x%02X" data="cpu.PB",
              Text text="PC: 0x%04X" data="cpu.PC",
              Text text="PS: 0x%02X" data="cpu.status",
              Text text="SP: 0x%02X" data="cpu.SP",
              Text text="Cycle: %d" data="snes.cycle_count",
            }
          }
        }
        CollapsingHeader id="spcState" title="SPC Registers" flags="DefaultOpen" {
          BeginChild id="##SpcState" size="0,100" flags="NoMove|NoScrollbar" {
            Columns id="spcRegistersColumns" count="2" {
              Text text="A: 0x%02X" data="spc.A",
              Text text="PC: 0x%04X" data="spc.PC",
              Text text="X: 0x%02X" data="spc.X",
              Text text="SP: 0x%02X" data="spc.SP",
              Text text="Y: 0x%02X" data="spc.Y",
              Text text="PSW: 0x%02X" data="spc.PSW",
            }
          }
        }
        Function id="CpuInstructionLog",

        TableNextColumn,
        Function id="SnesPpu",
        Function id="BreakpointList"
      }
    )";
    const std::map<std::string, void*> data_bindings = {
        {"cpu.A", &snes_.cpu().A},
        {"cpu.D", &snes_.cpu().D},
        {"cpu.X", &snes_.cpu().X},
        {"cpu.DB", &snes_.cpu().DB},
        {"cpu.Y", &snes_.cpu().Y},
        {"cpu.PB", &snes_.cpu().PB},
        {"cpu.PC", &snes_.cpu().PC},
        {"cpu.status", &snes_.cpu().status},
        {"snes.cycle_count", &snes_.mutable_cycles()},
        {"cpu.SP", &snes_.Memory().mutable_sp()},
        {"spc.A", &snes_.apu().spc700().A},
        {"spc.X", &snes_.apu().spc700().X},
        {"spc.Y", &snes_.apu().spc700().Y},
        {"spc.PC", &snes_.apu().spc700().PC},
        {"spc.SP", &snes_.apu().spc700().SP},
        {"spc.PSW", &snes_.apu().spc700().PSW}};
    emulator_node_ = gui::zeml::Parse(emulator_layout, data_bindings);
    Bind(emulator_node_.GetNode("CpuInstructionLog"),
         [&]() { RenderCpuInstructionLog(snes_.cpu().instruction_log_); });
    Bind(emulator_node_.GetNode("SnesPpu"), [&]() { RenderSnesPpu(); });
    Bind(emulator_node_.GetNode("BreakpointList"),
         [&]() { RenderBreakpointList(); });
  }
  void Run();

  auto snes() -> SNES& { return snes_; }
  auto running() const -> bool { return running_; }
  void set_audio_buffer(int16_t* audio_buffer) { audio_buffer_ = audio_buffer; }
  auto set_audio_device_id(SDL_AudioDeviceID audio_device) {
    audio_device_ = audio_device;
  }
  auto wanted_samples() const -> int { return wanted_samples_; }

 private:
  void RenderNavBar();
  void HandleEvents();

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

  SNES snes_;
  SDL_Texture* ppu_texture_;

  std::vector<uint8_t> rom_data_;

  gui::zeml::Node emulator_node_;
};

}  // namespace emu
}  // namespace app
}  // namespace yaze

#endif  // YAZE_APP_CORE_EMULATOR_H