#ifndef YAZE_APP_CORE_EMULATOR_H
#define YAZE_APP_CORE_EMULATOR_H

#include <imgui/imgui.h>

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
              Text text="E: %d" data="cpu.E"
            }
          }
        }
        Function id="CpuInstructionLog",

        TableNextColumn,
        Function id="SnesPpu",
        Function id="BreakpointList",
      }
    )";
    const std::map<std::string, void*> data_bindings = {
        {"cpu.A", &snes_.cpu().A},   {"cpu.D", &snes_.cpu().D},
        {"cpu.X", &snes_.cpu().X},   {"cpu.DB", &snes_.cpu().DB},
        {"cpu.Y", &snes_.cpu().Y},   {"cpu.PB", &snes_.cpu().PB},
        {"cpu.PC", &snes_.cpu().PC}, {"cpu.E", &snes_.cpu().E}};
    emulator_node_ = gui::zeml::Parse(emulator_layout, data_bindings);
    Bind(emulator_node_.GetNode("CpuInstructionLog"),
         [&]() { RenderCpuInstructionLog(snes_.cpu().instruction_log_); });
    Bind(emulator_node_.GetNode("SnesPpu"), [&]() { RenderSnesPpu(); });
    Bind(emulator_node_.GetNode("BreakpointList"),
         [&]() { RenderBreakpointList(); });
  }
  void Run();

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

  SNES snes_;
  uint16_t manual_pc_ = 0;
  uint8_t manual_pb_ = 0;
  gui::zeml::Node emulator_node_;

  bool power_ = false;
  bool loading_ = false;
  bool running_ = false;
  bool step_ = true;
};

}  // namespace emu
}  // namespace app
}  // namespace yaze

#endif  // YAZE_APP_CORE_EMULATOR_H