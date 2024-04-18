#include "app/emu/emulator.h"

#include <imgui/imgui.h>
#include <imgui_memory_editor.h>

#include <cstdint>
#include <vector>

#include "app/core/constants.h"
#include "app/emu/snes.h"
#include "app/gui/icons.h"
#include "app/gui/input.h"
#include "app/gui/zeml.h"
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
  if (!snes_.running() && rom()->is_loaded()) {
    snes_.SetupMemory(*rom());
    snes_.Init(*rom());
  }

  RenderNavBar();

  if (running_) {
    HandleEvents();
    if (!step_) {
      snes_.Run();
    }
  }

  RenderEmulator();
}

void Emulator::RenderNavBar() {
  std::string navbar_layout = R"(
    BeginMenuBar {
      BeginMenu title="Options" {
        MenuItem title="Input" {}
        MenuItem title="Audio" {}
        MenuItem title="Video" {}
      }
    }
  )";
  auto navbar_node = gui::zeml::Parse(navbar_layout);
  gui::zeml::Render(navbar_node);

  if (ImGui::Button(ICON_MD_PLAY_ARROW)) {
    loading_ = true;
  }
  if (ImGui::IsItemHovered()) {
    ImGui::SetTooltip("Start Emulation");
  }
  SameLine();

  if (ImGui::Button(ICON_MD_PAUSE)) {
    snes_.SetCpuMode(1);
  }
  if (ImGui::IsItemHovered()) {
    ImGui::SetTooltip("Pause Emulation");
  }
  SameLine();

  if (ImGui::Button(ICON_MD_SKIP_NEXT)) {
    // Step through Code logic
    snes_.StepRun();
  }
  if (ImGui::IsItemHovered()) {
    ImGui::SetTooltip("Step Through Code");
  }
  SameLine();

  if (ImGui::Button(ICON_MD_REFRESH)) {
    // Reset Emulator logic
  }
  if (ImGui::IsItemHovered()) {
    ImGui::SetTooltip("Reset Emulator");
  }
  SameLine();

  if (ImGui::Button(ICON_MD_STOP)) {
    // Stop Emulation logic
    running_ = false;
  }
  if (ImGui::IsItemHovered()) {
    ImGui::SetTooltip("Stop Emulation");
  }
  SameLine();

  if (ImGui::Button(ICON_MD_SAVE)) {
    // Save State logic
  }
  if (ImGui::IsItemHovered()) {
    ImGui::SetTooltip("Save State");
  }
  SameLine();

  if (ImGui::Button(ICON_MD_SYSTEM_UPDATE_ALT)) {
  }
  if (ImGui::IsItemHovered()) {
    ImGui::SetTooltip("Load State");
  }

  // Additional elements
  SameLine();
  if (ImGui::Button(ICON_MD_SETTINGS)) {
    // Settings logic
  }
  if (ImGui::IsItemHovered()) {
    ImGui::SetTooltip("Settings");
  }

  SameLine();
  if (ImGui::Button(ICON_MD_INFO)) {
    if (ImGui::IsItemHovered()) {
      ImGui::SetTooltip("About Debugger");
    }
    // About Debugger logic
  }
  static bool show_memory_viewer = false;

  SameLine();
  if (ImGui::Button(ICON_MD_MEMORY)) {
    show_memory_viewer = !show_memory_viewer;
  }
  if (ImGui::IsItemHovered()) {
    ImGui::SetTooltip("Memory Viewer");
  }

  if (show_memory_viewer) {
    ImGui::Begin("Memory Viewer", &show_memory_viewer);
    RenderMemoryViewer();
    ImGui::End();
  }
}

void Emulator::HandleEvents() {
  // Handle user input events
  // ...
}

void Emulator::RenderEmulator() {
  std::string emulator_layout = R"(
      Table id="Emulator" count="3" flags="Resizable|ScrollY" {
        TableSetupColumn title="CPU",
        TableSetupColumn title="PPU",
        TableHeadersRow,
        TableNextColumn {
          Function id="CpuInstructionLog"
        }
        TableNextColumn {
          Function id="SnesPpu",
          Function id="BreakpointList"
        }
        TableNextColumn {
          BeginChild id="##" size="0,0" flags="NoMove|NoScrollbar" {
            CollapsingHeader id="cpuState" title="Register Values" open=true {
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
            Function id="CpuState"
          }
        }
      }
    )";
  const std::map<std::string, void*> data_bindings = {
      {"cpu.A", &snes_.cpu().A},   {"cpu.D", &snes_.cpu().D},
      {"cpu.X", &snes_.cpu().X},   {"cpu.DB", &snes_.cpu().DB},
      {"cpu.Y", &snes_.cpu().Y},   {"cpu.PB", &snes_.cpu().PB},
      {"cpu.PC", &snes_.cpu().PC}, {"cpu.E", &snes_.cpu().E}};
  auto emulator_node = gui::zeml::Parse(emulator_layout, data_bindings);
  Bind(emulator_node.GetNode("CpuInstructionLog"),
       [&]() { RenderCpuInstructionLog(snes_.cpu().instruction_log_); });
  Bind(emulator_node.GetNode("SnesPpu"), [&]() { RenderSnesPpu(); });
  Bind(emulator_node.GetNode("BreakpointList"),
       [&]() { RenderBreakpointList(); });
  Bind(emulator_node.GetNode("CpuState"),
       [&]() { RenderCpuState(snes_.cpu()); });
  gui::zeml::Render(emulator_node);
}

void Emulator::RenderSnesPpu() {
  ImVec2 size = ImVec2(320, 240);
  if (snes_.running()) {
    ImGui::BeginChild("EmulatorOutput", ImVec2(0, 240), true,
                      ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoScrollbar);
    ImGui::SetCursorPosX((ImGui::GetWindowSize().x - size.x) * 0.5f);
    ImGui::SetCursorPosY((ImGui::GetWindowSize().y - size.y) * 0.5f);
    ImGui::Image((void*)snes_.ppu().GetScreen()->texture(), size, ImVec2(0, 0),
                 ImVec2(1, 1));
    ImGui::EndChild();

  } else {
    ImGui::Text("Emulator output not available.");
    ImGui::BeginChild("EmulatorOutput", ImVec2(0, 240), true,
                      ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoScrollbar);
    ImGui::SetCursorPosX(((ImGui::GetWindowSize().x * 0.5f) - size.x) * 0.5f);
    ImGui::SetCursorPosY(((ImGui::GetWindowSize().y * 0.5f) - size.y) * 0.5f);
    ImGui::Dummy(size);
    ImGui::EndChild();
  }
  ImGui::Separator();
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
    snes_.cpu().SetBreakpoint(breakpoint);
    memset(breakpoint_input, 0, sizeof(breakpoint_input));
  }
  SameLine();
  if (ImGui::Button("Add")) {
    int breakpoint = std::stoi(breakpoint_input, nullptr, 16);
    snes_.cpu().SetBreakpoint(breakpoint);
    memset(breakpoint_input, 0, sizeof(breakpoint_input));
  }
  SameLine();
  if (ImGui::Button("Clear")) {
    snes_.cpu().ClearBreakpoints();
  }
  Separator();
  auto breakpoints = snes_.cpu().GetBreakpoints();
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
  Separator();
  gui::InputHexByte("PB", &manual_pb_, 25.f);
  gui::InputHexWord("PC", &manual_pc_, 25.f);
  if (ImGui::Button("Set Current Address")) {
    snes_.cpu().PC = manual_pc_;
    snes_.cpu().PB = manual_pb_;
  }
}

void Emulator::RenderCpuState(Cpu& cpu) {
  // Call Stack
  if (ImGui::CollapsingHeader("Call Stack", ImGuiTreeNodeFlags_DefaultOpen)) {
    // For each return address in the call stack:
    Text("Return Address: 0x%08X", 0xFFFFFF);  // Placeholder
  }

  snes_.SetCpuMode(0);
}

void Emulator::RenderMemoryViewer() {
  static MemoryEditor mem_edit;
  if (ImGui::Button("RAM")) {
    mem_edit.GotoAddrAndHighlight(0x7E0000, 0x7E0001);
  }

  if (ImGui::BeginTable("MemoryViewerTable", 2,
                        ImGuiTableFlags_Resizable | ImGuiTableFlags_ScrollY)) {
    ImGui::TableSetupColumn("Bookmarks");
    ImGui::TableSetupColumn("Memory");
    ImGui::TableHeadersRow();

    TableNextColumn();
    if (ImGui::CollapsingHeader("Bookmarks", ImGuiTreeNodeFlags_DefaultOpen)) {
      // Input for adding a new bookmark
      static char nameBuf[256];
      static uint64_t uint64StringBuf;
      ImGui::InputText("Name", nameBuf, IM_ARRAYSIZE(nameBuf));
      gui::InputHex("Address", &uint64StringBuf);
      if (ImGui::Button("Add Bookmark")) {
        bookmarks.push_back({nameBuf, uint64StringBuf});
        memset(nameBuf, 0, sizeof(nameBuf));
        uint64StringBuf = 0;
      }

      // Tree view of bookmarks
      for (const auto& bookmark : bookmarks) {
        if (ImGui::TreeNode(bookmark.name.c_str(), ICON_MD_STAR)) {
          auto bookmark_string = absl::StrFormat(
              "%s: 0x%08X", bookmark.name.c_str(), bookmark.value);
          if (ImGui::Selectable(bookmark_string.c_str())) {
            mem_edit.GotoAddrAndHighlight(static_cast<ImU64>(bookmark.value),
                                          1);
          }
          SameLine();
          if (ImGui::Button("Delete")) {
            // Logic to delete the bookmark
            bookmarks.erase(std::remove_if(bookmarks.begin(), bookmarks.end(),
                                           [&](const Bookmark& b) {
                                             return b.name == bookmark.name &&
                                                    b.value == bookmark.value;
                                           }),
                            bookmarks.end());
          }
          ImGui::TreePop();
        }
      }
    }

    TableNextColumn();
    mem_edit.DrawContents((void*)snes_.Memory()->data(),
                          snes_.Memory()->size());

    ImGui::EndTable();
  }
}

void Emulator::RenderCpuInstructionLog(
    const std::vector<InstructionEntry>& instructionLog) {
  if (ImGui::CollapsingHeader("CPU Instruction Log")) {
    // Filtering options
    static char filterBuf[256];
    ImGui::InputText("Filter", filterBuf, IM_ARRAYSIZE(filterBuf));
    SameLine();
    if (ImGui::Button("Clear")) { /* Clear filter logic */
    }

    // Toggle for showing all opcodes
    static bool showAllOpcodes = true;
    ImGui::Checkbox("Show All Opcodes", &showAllOpcodes);

    // Instruction list
    ImGui::BeginChild("InstructionList", ImVec2(0, 0), ImGuiChildFlags_None);
    for (const auto& entry : instructionLog) {
      if (ShouldDisplay(entry, filterBuf, showAllOpcodes)) {
        if (ImGui::Selectable(
                absl::StrFormat("%06X: %s %s", entry.address,
                                opcode_to_mnemonic.at(entry.opcode),
                                entry.operands)
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
