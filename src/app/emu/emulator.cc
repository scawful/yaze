#include "app/emu/emulator.h"

#include <imgui/imgui.h>

#include <cstdint>
#include <vector>

#include "app/core/constants.h"
#include "app/emu/snes.h"
#include "app/rom.h"

namespace yaze {
namespace app {
namespace emu {

namespace {
bool ShouldDisplay(const InstructionEntry& entry, const char* filter,
                   bool showAll) {
  // Implement logic to determine if the entry should be displayed based on the
  // filter and showAll flag
  return true;
}
}  // namespace

using ImGui::NextColumn;
using ImGui::SameLine;
using ImGui::Separator;
using ImGui::TableNextColumn;
using ImGui::Text;

void Emulator::Run() {
  if (!snes_.running() && loading_) {
    if (rom()->isLoaded()) {
      snes_.Init(*rom());
      running_ = true;
    }
  }

  RenderNavBar();

  if (running_) {
    HandleEvents();
    UpdateEmulator();
    RenderEmulator();
  }

  if (debugger_) {
    RenderDebugger();
  }
}

void Emulator::RenderEmulator() {
  ImVec2 size = ImVec2(320, 240);
  if (snes_.running()) {
    ImGui::Image((void*)snes_.Ppu().GetScreen()->texture(), size, ImVec2(0, 0),
                 ImVec2(1, 1));
    ImGui::Separator();
  } else {
    ImGui::Dummy(size);
    ImGui::Separator();
    ImGui::Text("Emulator output not available.");
  }
}

void Emulator::RenderNavBar() {
  MENU_BAR()

  if (ImGui::BeginMenu("Game")) {
    MENU_ITEM("Load ROM") { loading_ = true; }
    MENU_ITEM("Power Off") {
      running_ = false;
      loading_ = false;
      debugger_ = false;
    }
    MENU_ITEM("Pause") {
      running_ = false;
      debugger_ = false;
    }
    MENU_ITEM("Reset") {}

    MENU_ITEM("Save State") {}
    MENU_ITEM("Load State") {}

    ImGui::EndMenu();
  }

  if (ImGui::BeginMenu("Debug")) {
    MENU_ITEM("Debugger") { debugger_ = !debugger_; }
    if (ImGui::MenuItem("Integrated Debugger", nullptr,
                        &integrated_debugger_mode_)) {
      separate_debugger_mode_ = !integrated_debugger_mode_;
    }
    if (ImGui::MenuItem("Separate Debugger Windows", nullptr,
                        &separate_debugger_mode_)) {
      integrated_debugger_mode_ = !separate_debugger_mode_;
    }
    MENU_ITEM("Memory Viewer") {}
    MENU_ITEM("Tile Viewer") {}
    ImGui::EndMenu();
  }

  if (ImGui::BeginMenu("Options")) {
    MENU_ITEM("Input") {}
    MENU_ITEM("Audio") {}
    MENU_ITEM("Video") {}
    ImGui::EndMenu();
  }
  END_MENU_BAR()
}

void Emulator::HandleEvents() {
  // Handle user input events
  // ...
}

void Emulator::UpdateEmulator() {
  // Update the emulator state (CPU, PPU, APU, etc.)
  // ...
  snes_.Run();
}

void Emulator::RenderDebugger() {
  // Define a lambda with the actual debugger
  auto debugger = [&]() {
    if (ImGui::BeginTable(
            "DebugTable", 3,
            ImGuiTableFlags_Resizable | ImGuiTableFlags_ScrollY)) {
      TableNextColumn();
      RenderCpuState(snes_.Cpu());

      TableNextColumn();
      RenderCPUInstructionLog(snes_.Cpu().instruction_log_);

      TableNextColumn();
      RenderBreakpointList();
      ImGui::EndTable();
    }
  };

  if (integrated_debugger_mode_) {
    debugger();
  } else if (separate_debugger_mode_) {
    ImGui::Begin("Debugger");
    debugger();
    ImGui::End();
  }
}

void Emulator::RenderBreakpointList() {
  Text("Breakpoints");
  Separator();
  static char breakpoint_input[10] = "";
  static int current_memory_mode = 0;

  static bool read_mode = false;
  static bool write_mode = false;
  static bool execute_mode = false;

  if (ImGui::Combo("##TypeOfMemory", &current_memory_mode, "PRG\0RAM\0")) {
  }

  ImGui::Checkbox("Read", &read_mode);
  SameLine();
  ImGui::Checkbox("Write", &write_mode);
  SameLine();
  ImGui::Checkbox("Execute", &execute_mode);

  // Breakpoint input fields and buttons
  if (ImGui::InputText("##BreakpointInput", breakpoint_input, 10,
                       ImGuiInputTextFlags_EnterReturnsTrue)) {
    int breakpoint = std::stoi(breakpoint_input, nullptr, 16);
    snes_.Cpu().SetBreakpoint(breakpoint);
    memset(breakpoint_input, 0, sizeof(breakpoint_input));
  }
  SameLine();
  if (ImGui::Button("Add")) {
    int breakpoint = std::stoi(breakpoint_input, nullptr, 16);
    snes_.Cpu().SetBreakpoint(breakpoint);
    memset(breakpoint_input, 0, sizeof(breakpoint_input));
  }
  SameLine();
  if (ImGui::Button("Clear")) {
    snes_.Cpu().ClearBreakpoints();
  }
  Separator();
  auto breakpoints = snes_.Cpu().GetBreakpoints();
  if (!breakpoints.empty()) {
    Text("Breakpoints:");
    ImGui::BeginChild("BreakpointsList", ImVec2(0, 100), true);
    for (auto breakpoint : breakpoints) {
      if (ImGui::Selectable(absl::StrFormat("0x%04X", breakpoint).c_str())) {
        // Jump to breakpoint
        // snes_.Cpu().JumpToBreakpoint(breakpoint);
      }
    }
    ImGui::EndChild();
  }
}

void Emulator::RenderCpuState(CPU& cpu) {
  if (ImGui::CollapsingHeader("Register Values",
                              ImGuiTreeNodeFlags_DefaultOpen)) {
    ImGui::Columns(2, "RegistersColumns");
    Separator();
    Text("A: 0x%04X", cpu.A);
    NextColumn();
    Text("D: 0x%04X", cpu.D);
    NextColumn();
    Text("X: 0x%04X", cpu.X);
    NextColumn();
    Text("DB: 0x%02X", cpu.DB);
    NextColumn();
    Text("Y: 0x%04X", cpu.Y);
    NextColumn();
    Text("PB: 0x%02X", cpu.PB);
    NextColumn();
    Text("PC: 0x%04X", cpu.PC);
    NextColumn();
    Text("E: %d", cpu.E);
    NextColumn();
    ImGui::Columns(1);
    Separator();
  }
  // Call Stack
  if (ImGui::CollapsingHeader("Call Stack", ImGuiTreeNodeFlags_DefaultOpen)) {
    // For each return address in the call stack:
    Text("Return Address: 0x%08X", 0xFFFFFF);  // Placeholder
  }

  static int debugger_mode_ = 0;
  const char* debugger_modes_[] = {"Run", "Step", "Pause"};
  Text("Mode");
  ImGui::ListBox("##DebuggerMode", &debugger_mode_, debugger_modes_,
                 IM_ARRAYSIZE(debugger_modes_));

  snes_.SetCpuMode(debugger_mode_);
}

void Emulator::RenderMemoryViewer() {
  // Render memory viewer
}

void Emulator::RenderCPUInstructionLog(
    const std::vector<InstructionEntry>& instructionLog) {
  if (ImGui::CollapsingHeader("CPU Instruction Log")) {
    // Filtering options
    static char filterBuf[256];
    ImGui::InputText("Filter", filterBuf, IM_ARRAYSIZE(filterBuf));
    ImGui::SameLine();
    if (ImGui::Button("Clear")) { /* Clear filter logic */
    }

    // Toggle for showing all opcodes
    static bool showAllOpcodes = false;
    ImGui::Checkbox("Show All Opcodes", &showAllOpcodes);

    // Instruction list
    ImGui::BeginChild("InstructionList", ImVec2(0, 0),
                      ImGuiChildFlags_AlwaysAutoResize);
    for (const auto& entry : instructionLog) {
      if (ShouldDisplay(entry, filterBuf, showAllOpcodes)) {
        if (ImGui::Selectable(absl::StrFormat("%04X: %02X %s %s", entry.address,
                                              entry.opcode, entry.operands,
                                              entry.instruction)
                                  .c_str())) {
          // Logic to handle click (e.g., jump to address, set breakpoint)
        }
      }
    }
    ImGui::EndChild();
  }
}

}  // namespace emu
}  // namespace app
}  // namespace yaze
