#include "app/emu/emulator.h"

#include <cstdint>
#include <vector>

#include "app/core/platform/file_dialog.h"
#include "app/core/platform/renderer.h"
#include "app/emu/cpu/internal/opcodes.h"
#include "app/gui/icons.h"
#include "app/gui/input.h"
#include "app/gui/zeml.h"
#include "imgui/imgui.h"
#include "imgui_memory_editor.h"

namespace yaze {
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
  static bool loaded = false;
  if (!snes_.running() && rom()->is_loaded()) {
    ppu_texture_ = SDL_CreateTexture(core::Renderer::Get().renderer(),
                                     SDL_PIXELFORMAT_ARGB8888,
                                     SDL_TEXTUREACCESS_STREAMING, 512, 480);
    if (ppu_texture_ == NULL) {
      printf("Failed to create texture: %s\n", SDL_GetError());
      return;
    }
    rom_data_ = rom()->vector();
    snes_.Init(rom_data_);
    wanted_frames_ = 1.0 / (snes_.memory().pal_timing() ? 50.0 : 60.0);
    wanted_samples_ = 48000 / (snes_.memory().pal_timing() ? 50 : 60);
    loaded = true;

    count_frequency = SDL_GetPerformanceFrequency();
    last_count = SDL_GetPerformanceCounter();
    time_adder = 0.0;
  }

  RenderNavBar();

  if (running_) {
    HandleEvents();

    uint64_t current_count = SDL_GetPerformanceCounter();
    uint64_t delta = current_count - last_count;
    last_count = current_count;
    float seconds = delta / (float)count_frequency;
    time_adder += seconds;
    // allow 2 ms earlier, to prevent skipping due to being just below wanted
    while (time_adder >= wanted_frames_ - 0.002) {
      time_adder -= wanted_frames_;

      if (loaded) {
        if (turbo_mode_) {
          snes_.RunFrame();
        }
        snes_.RunFrame();

        snes_.SetSamples(audio_buffer_, wanted_samples_);
        if (SDL_GetQueuedAudioSize(audio_device_) <= wanted_samples_ * 4 * 6) {
          SDL_QueueAudio(audio_device_, audio_buffer_, wanted_samples_ * 4);
        }

        void* ppu_pixels_;
        int ppu_pitch_;
        if (SDL_LockTexture(ppu_texture_, NULL, &ppu_pixels_, &ppu_pitch_) !=
            0) {
          printf("Failed to lock texture: %s\n", SDL_GetError());
          return;
        }
        snes_.SetPixels(static_cast<uint8_t*>(ppu_pixels_));
        SDL_UnlockTexture(ppu_texture_);
      }
    }
  }

  gui::zeml::Render(emulator_node_);
}

void Emulator::RenderSnesPpu() {
  ImVec2 size = ImVec2(512, 480);
  if (snes_.running()) {
    ImGui::BeginChild("EmulatorOutput", ImVec2(0, 480), true,
                      ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoScrollbar);
    ImGui::SetCursorPosX((ImGui::GetWindowSize().x - size.x) * 0.5f);
    ImGui::SetCursorPosY((ImGui::GetWindowSize().y - size.y) * 0.5f);
    ImGui::Image((ImTextureID)(intptr_t)ppu_texture_, size, ImVec2(0, 0),
                 ImVec2(1, 1));
    ImGui::EndChild();

  } else {
    ImGui::Text("Emulator output not available.");
    ImGui::BeginChild("EmulatorOutput", ImVec2(0, 480), true,
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

  static auto navbar_node = gui::zeml::Parse(navbar_layout);
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
    snes_.cpu().RunOpcode();
  }
  if (ImGui::IsItemHovered()) {
    ImGui::SetTooltip("Step Through Code");
  }
  SameLine();

  if (ImGui::Button(ICON_MD_REFRESH)) {
    // Reset Emulator logic
    snes_.Reset(true);
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

  static bool open_file = false;
  SameLine();
  if (ImGui::Button(ICON_MD_INFO)) {
    open_file = true;

    // About Debugger logic
  }
  if (ImGui::IsItemHovered()) {
    ImGui::SetTooltip("About Debugger");
  }
  SameLine();
  ImGui::Checkbox("Logging", snes_.cpu().mutable_log_instructions());

  SameLine();
  ImGui::Checkbox("Turbo", &turbo_mode_);

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

  if (open_file) {
    auto file_name = core::FileDialogWrapper::ShowOpenFileDialog();
    if (!file_name.empty()) {
      std::ifstream file(file_name, std::ios::binary);
      // Load the data directly into rom_data
      rom_data_.assign(std::istreambuf_iterator<char>(file),
                       std::istreambuf_iterator<char>());
      snes_.Init(rom_data_);
      open_file = false;
    }
  }
}

void Emulator::HandleEvents() {
  // Handle user input events
  if (ImGui::IsKeyPressed(keybindings_.a_button)) {
    snes_.SetButtonState(1, 0, true);
  }

  if (ImGui::IsKeyPressed(keybindings_.b_button)) {
    snes_.SetButtonState(1, 1, true);
  }

  if (ImGui::IsKeyPressed(keybindings_.select_button)) {
    snes_.SetButtonState(1, 2, true);
  }

  if (ImGui::IsKeyPressed(keybindings_.start_button)) {
    snes_.SetButtonState(1, 3, true);
  }

  if (ImGui::IsKeyPressed(keybindings_.up_button)) {
    snes_.SetButtonState(1, 4, true);
  }

  if (ImGui::IsKeyPressed(keybindings_.down_button)) {
    snes_.SetButtonState(1, 5, true);
  }

  if (ImGui::IsKeyPressed(keybindings_.left_button)) {
    snes_.SetButtonState(1, 6, true);
  }

  if (ImGui::IsKeyPressed(keybindings_.right_button)) {
    snes_.SetButtonState(1, 7, true);
  }

  if (ImGui::IsKeyPressed(keybindings_.x_button)) {
    snes_.SetButtonState(1, 8, true);
  }

  if (ImGui::IsKeyPressed(keybindings_.y_button)) {
    snes_.SetButtonState(1, 9, true);
  }

  if (ImGui::IsKeyPressed(keybindings_.l_button)) {
    snes_.SetButtonState(1, 10, true);
  }

  if (ImGui::IsKeyPressed(keybindings_.r_button)) {
    snes_.SetButtonState(1, 11, true);
  }

  if (ImGui::IsKeyReleased(keybindings_.a_button)) {
    snes_.SetButtonState(1, 0, false);
  }

  if (ImGui::IsKeyReleased(keybindings_.b_button)) {
    snes_.SetButtonState(1, 1, false);
  }

  if (ImGui::IsKeyReleased(keybindings_.select_button)) {
    snes_.SetButtonState(1, 2, false);
  }

  if (ImGui::IsKeyReleased(keybindings_.start_button)) {
    snes_.SetButtonState(1, 3, false);
  }

  if (ImGui::IsKeyReleased(keybindings_.up_button)) {
    snes_.SetButtonState(1, 4, false);
  }

  if (ImGui::IsKeyReleased(keybindings_.down_button)) {
    snes_.SetButtonState(1, 5, false);
  }

  if (ImGui::IsKeyReleased(keybindings_.left_button)) {
    snes_.SetButtonState(1, 6, false);
  }

  if (ImGui::IsKeyReleased(keybindings_.right_button)) {
    snes_.SetButtonState(1, 7, false);
  }

  if (ImGui::IsKeyReleased(keybindings_.x_button)) {
    snes_.SetButtonState(1, 8, false);
  }

  if (ImGui::IsKeyReleased(keybindings_.y_button)) {
    snes_.SetButtonState(1, 9, false);
  }

  if (ImGui::IsKeyReleased(keybindings_.l_button)) {
    snes_.SetButtonState(1, 10, false);
  }

  if (ImGui::IsKeyReleased(keybindings_.r_button)) {
    snes_.SetButtonState(1, 11, false);
  }
}

void Emulator::RenderBreakpointList() {
  if (ImGui::Button("Set SPC PC")) {
    snes_.apu().spc700().PC = 0xFFEF;
  }
  Separator();
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
        // snes_.cpu().JumpToBreakpoint(breakpoint);
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
  static MemoryEditor ram_edit;
  static MemoryEditor aram_edit;
  static MemoryEditor mem_edit;

  if (ImGui::BeginTable("MemoryViewerTable", 4,
                        ImGuiTableFlags_Resizable | ImGuiTableFlags_ScrollY)) {
    ImGui::TableSetupColumn("Bookmarks");
    ImGui::TableSetupColumn("RAM");
    ImGui::TableSetupColumn("ARAM");
    ImGui::TableSetupColumn("ROM");
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
    if (ImGui::BeginChild("RAM", ImVec2(0, 0), true,
                          ImGuiWindowFlags_NoMove |
                              ImGuiWindowFlags_NoScrollbar |
                              ImGuiWindowFlags_NoScrollWithMouse)) {
      ram_edit.DrawContents((void*)snes_.get_ram(), 0x20000);
      ImGui::EndChild();
    }

    TableNextColumn();
    if (ImGui::BeginChild("ARAM", ImVec2(0, 0), true,
                          ImGuiWindowFlags_NoMove |
                              ImGuiWindowFlags_NoScrollbar |
                              ImGuiWindowFlags_NoScrollWithMouse)) {
      aram_edit.DrawContents((void*)snes_.apu().ram.data(),
                             snes_.apu().ram.size());
      ImGui::EndChild();
    }

    TableNextColumn();
    if (ImGui::BeginChild("ROM", ImVec2(0, 0), true,
                          ImGuiWindowFlags_NoMove |
                              ImGuiWindowFlags_NoScrollbar |
                              ImGuiWindowFlags_NoScrollWithMouse)) {
      mem_edit.DrawContents((void*)snes_.memory().rom_.data(),
                            snes_.memory().rom_.size());
      ImGui::EndChild();
    }

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
}  // namespace yaze
