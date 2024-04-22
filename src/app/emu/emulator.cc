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
bool ShouldDisplay(const InstructionEntry& entry, const char* filter) {
  if (filter[0] == '\0') {
    return true;
  }

  // Supported fields: address, opcode, operands
  if (entry.operands.find(filter) != std::string::npos) {
    return true;
  }

  if (absl::StrFormat("%06X", entry.address).find(filter) !=
      std::string::npos) {
    return true;
  }

  if (opcode_to_mnemonic.at(entry.opcode).find(filter) != std::string::npos) {
    return true;
  }

  return false;
}

}  // namespace

using ImGui::NextColumn;
using ImGui::SameLine;
using ImGui::Separator;
using ImGui::TableNextColumn;
using ImGui::Text;

void Emulator::Run() {
  if (!snes_.running() && rom()->is_loaded()) {
    ppu_texture_ =
        SDL_CreateTexture(rom()->renderer().get(), SDL_PIXELFORMAT_RGBX8888,
                          SDL_TEXTUREACCESS_STREAMING, 512, 480);
    if (ppu_texture_ == NULL) {
      printf("Failed to create texture: %s\n", SDL_GetError());
      return;
    }
    
    snes_.Init(*rom());
    wanted_frames_ = 1.0 / (snes_.Memory()->pal_timing() ? 50.0 : 60.0);
    wanted_samples_ = 48000 / (snes_.Memory()->pal_timing() ? 50 : 60);

    countFreq = SDL_GetPerformanceFrequency();
    lastCount = SDL_GetPerformanceCounter();
    timeAdder = 0.0;
  }

  RenderNavBar();

  if (running_) {
    HandleEvents();

    uint64_t curCount = SDL_GetPerformanceCounter();
    uint64_t delta = curCount - lastCount;
    lastCount = curCount;
    float seconds = delta / (float)countFreq;
    timeAdder += seconds;
    // allow 2 ms earlier, to prevent skipping due to being just below wanted
    while (timeAdder >= wanted_frames_ - 0.002) {
      timeAdder -= wanted_frames_;
      snes_.RunFrame();

      void* ppu_pixels_;
      int ppu_pitch_;
      if (SDL_LockTexture(ppu_texture_, NULL, &ppu_pixels_, &ppu_pitch_) != 0) {
        printf("Failed to lock texture: %s\n", SDL_GetError());
        return;
      }
      snes_.SetPixels(static_cast<uint8_t*>(ppu_pixels_));
      SDL_UnlockTexture(ppu_texture_);
    }
  }

  gui::zeml::Render(emulator_node_);
}

void Emulator::RenderSnesPpu() {
  ImVec2 size = ImVec2(320, 480);
  if (snes_.running()) {
    ImGui::BeginChild("EmulatorOutput", ImVec2(0, 240), true,
                      ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoScrollbar);
    ImGui::SetCursorPosX((ImGui::GetWindowSize().x - size.x) * 0.5f);
    ImGui::SetCursorPosY((ImGui::GetWindowSize().y - size.y) * 0.5f);
    ImGui::Image((void*)ppu_texture_, size, ImVec2(0, 0), ImVec2(1, 1));
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
    running_ = true;
  }
  if (ImGui::IsItemHovered()) {
    ImGui::SetTooltip("Start Emulation");
  }
  SameLine();

  if (ImGui::Button(ICON_MD_PAUSE)) {
    running_ = false;
  }
  if (ImGui::IsItemHovered()) {
    ImGui::SetTooltip("Pause Emulation");
  }
  SameLine();

  if (ImGui::Button(ICON_MD_SKIP_NEXT)) {
    // Step through Code logic
    // snes_.StepRun();
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

  SameLine();
  ImGui::Checkbox("Logging", snes_.cpu().mutable_log_instructions());

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
  gui::InputHexByte("PB", &manual_pb_, 50.f);
  gui::InputHexWord("PC", &manual_pc_, 75.f);
  if (ImGui::Button("Set Current Address")) {
    snes_.cpu().PC = manual_pc_;
    snes_.cpu().PB = manual_pb_;
  }
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
    const std::vector<InstructionEntry>& instruction_log) {
  if (ImGui::CollapsingHeader("Instruction Log",
                              ImGuiTreeNodeFlags_DefaultOpen)) {
    // Filtering options
    static char filter[256];
    ImGui::InputText("Filter", filter, IM_ARRAYSIZE(filter));

    // Instruction list
    ImGui::BeginChild("InstructionList", ImVec2(0, 0), ImGuiChildFlags_None);
    for (const auto& entry : instruction_log) {
      if (ShouldDisplay(entry, filter)) {
        if (ImGui::Selectable(
                absl::StrFormat("%06X:", entry.address).c_str())) {
          // Logic to handle click (e.g., jump to address, set breakpoint)
        }

        ImGui::SameLine();

        ImVec4 color = ImVec4(1.0f, 1.0f, 1.0f, 1.0f);
        ImGui::TextColored(color, "%s",
                           opcode_to_mnemonic.at(entry.opcode).c_str());
        ImVec4 operand_color = ImVec4(0.7f, 0.5f, 0.3f, 1.0f);
        ImGui::SameLine();
        ImGui::TextColored(operand_color, "%s", entry.operands.c_str());
      }
    }
    // Jump to the bottom of the child scrollbar
    if (ImGui::GetScrollY() >= ImGui::GetScrollMaxY()) {
      ImGui::SetScrollHereY(1.0f);
    }

    ImGui::EndChild();
  }
}

}  // namespace emu
}  // namespace app
}  // namespace yaze
