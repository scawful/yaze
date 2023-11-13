#include "app/emu/emulator.h"

#include <cstdint>
#include <vector>

#include "app/core/constants.h"
#include "app/emu/snes.h"
#include "app/rom.h"

namespace yaze {
namespace app {
namespace emu {

void Emulator::Run() {
  // Initialize the emulator if a ROM is loaded
  if (!snes_.running() && loading_) {
    if (rom()->isLoaded()) {
      snes_.Init(*rom());
      running_ = true;
    }
  }

  // Render the emulator output
  RenderNavBar();

  if (running_) {
    // Handle user input events
    HandleEvents();
    // Update the emulator state
    UpdateEmulator();

    RenderEmulator();
  }

  if (debugger_) {
    RenderDebugger();
  }
}

void Emulator::RenderEmulator() {
  // Get the emulator output and render it to the child window
  // You can use the ImGui::Image function to display the emulator output as a
  // texture
  // ...
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
    ImGui::MenuItem("PPU Register Viewer", nullptr, &show_ppu_reg_viewer_);
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
    MENU_ITEM("Audio") {}
    MENU_ITEM("Input") {}
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
            "DebugTable", 2,
            ImGuiTableFlags_Resizable | ImGuiTableFlags_ScrollY)) {
      ImGui::TableNextColumn();
      RenderCpuState(snes_.Cpu());

      ImGui::TableNextColumn();
      RenderCPUInstructionLog(snes_.Cpu().instruction_log_);

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

void Emulator::RenderCpuState(CPU& cpu) {
  ImGui::Columns(2, "RegistersColumns");
  ImGui::Separator();
  ImGui::Text("A: 0x%04X", cpu.A);
  ImGui::NextColumn();
  ImGui::Text("D: 0x%04X", cpu.D);
  ImGui::NextColumn();
  ImGui::Text("X: 0x%04X", cpu.X);
  ImGui::NextColumn();
  ImGui::Text("DB: 0x%02X", cpu.DB);
  ImGui::NextColumn();
  ImGui::Text("Y: 0x%04X", cpu.Y);
  ImGui::NextColumn();
  ImGui::Text("PB: 0x%02X", cpu.PB);
  ImGui::NextColumn();
  ImGui::Text("PC: 0x%04X", cpu.PC);
  ImGui::NextColumn();
  ImGui::Text("E: %d", cpu.E);
  ImGui::NextColumn();
  ImGui::Columns(1);
  ImGui::Separator();
  // Call Stack
  if (ImGui::CollapsingHeader("Call Stack")) {
    // For each return address in the call stack:
    ImGui::Text("Return Address: 0x%08X", 0xFFFFFF);  // Placeholder
  }
}

void Emulator::RenderMemoryViewer() {
  // Render memory viewer
}

namespace {
bool ShouldDisplay(const InstructionEntry& entry, const char* filter,
                   bool showAll) {
  // Implement logic to determine if the entry should be displayed based on the
  // filter and showAll flag
  return true;
}
}  // namespace

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
    ImGui::BeginChild("InstructionList");
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
