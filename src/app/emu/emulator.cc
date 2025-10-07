#include "app/emu/emulator.h"

#include <cstdint>
#include <fstream>
#include <vector>

#include "app/core/window.h"
#include "app/emu/cpu/internal/opcodes.h"
#include "app/emu/debug/disassembly_viewer.h"
#include "app/gui/color.h"
#include "app/gui/editor_layout.h"
#include "app/gui/icons.h"
#include "app/gui/input.h"
#include "app/gui/theme_manager.h"
#include "imgui/imgui.h"
#include "imgui_memory_editor.h"
#include "util/file_util.h"

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

using ImGui::SameLine;
using ImGui::Separator;
using ImGui::TableNextColumn;
using ImGui::Text;

Emulator::~Emulator() {
  // Don't call Cleanup() in destructor - renderer is already destroyed
  // Just stop emulation
  running_ = false;
}

void Emulator::Cleanup() {
  // Stop emulation
  running_ = false;
  
  // Don't try to destroy PPU texture during shutdown
  // The renderer is destroyed before the emulator, so attempting to
  // call renderer_->DestroyTexture() will crash
  // The texture will be cleaned up automatically when SDL quits
  ppu_texture_ = nullptr;
  
  // Reset state
  snes_initialized_ = false;
}

void Emulator::Initialize(gfx::IRenderer* renderer, const std::vector<uint8_t>& rom_data) {
  // This method is now optional - emulator can be initialized lazily in Run()
  renderer_ = renderer;
  rom_data_ = rom_data;
  
  // Reset state for new ROM
  running_ = false;
  snes_initialized_ = false;
  
  initialized_ = true;
}

void Emulator::Run(Rom* rom) {
  // Lazy initialization: set renderer from Controller if not set yet
  if (!renderer_) {
    ImGui::TextColored(ImVec4(1.0f, 0.3f, 0.3f, 1.0f), 
                       "Emulator renderer not initialized");
    return;
  }
  
  // Initialize SNES and create PPU texture on first run
  // This happens lazily when user opens the emulator window
  if (!snes_initialized_ && rom->is_loaded()) {
    // Create PPU texture with correct format for SNES emulator
    // ARGB8888 matches the XBGR format used by the SNES PPU (pixel format 1)
    if (!ppu_texture_) {
      ppu_texture_ = renderer_->CreateTextureWithFormat(
          512, 480, SDL_PIXELFORMAT_ARGB8888, SDL_TEXTUREACCESS_STREAMING);
      if (ppu_texture_ == NULL) {
        printf("Failed to create PPU texture: %s\n", SDL_GetError());
        return;
      }
    }

    // Initialize SNES with ROM data (either from Initialize() or from rom parameter)
    if (rom_data_.empty()) {
      rom_data_ = rom->vector();
    }
    snes_.Init(rom_data_);

    // Note: PPU pixel format set to 1 (XBGR) in Init() which matches ARGB8888 texture

    wanted_frames_ = 1.0 / (snes_.memory().pal_timing() ? 50.0 : 60.0);
    wanted_samples_ = 48000 / (snes_.memory().pal_timing() ? 50 : 60);
    snes_initialized_ = true;

    count_frequency = SDL_GetPerformanceFrequency();
    last_count = SDL_GetPerformanceCounter();
    time_adder = 0.0;
    frame_count_ = 0;
    fps_timer_ = 0.0;
    current_fps_ = 0.0;
  }

  RenderNavBar();

  // Auto-pause emulator when window loses focus to save CPU/battery
  static bool was_running_before_focus_loss = false;
  bool window_has_focus = ImGui::IsWindowFocused(ImGuiFocusedFlags_RootWindow);
  
  if (!window_has_focus && running_) {
    was_running_before_focus_loss = true;
    running_ = false;
  } else if (window_has_focus && !running_ && was_running_before_focus_loss) {
    // Don't auto-resume - let user manually resume
    was_running_before_focus_loss = false;
  }

  if (running_) {
    HandleEvents();

    uint64_t current_count = SDL_GetPerformanceCounter();
    uint64_t delta = current_count - last_count;
    last_count = current_count;
    double seconds = delta / (double)count_frequency;
    time_adder += seconds;

    // Cap time accumulation to prevent spiral of death and improve stability
    if (time_adder > wanted_frames_ * 3.0) {
      time_adder = wanted_frames_ * 3.0;
    }

    // Track frames to skip for performance
    int frames_to_process = 0;
    while (time_adder >= wanted_frames_ - 0.002) {
      time_adder -= wanted_frames_;
      frames_to_process++;
    }

    // Limit maximum frames to process (prevent spiral of death)
    if (frames_to_process > 4) {
      frames_to_process = 4;
    }

    if (snes_initialized_ && frames_to_process > 0) {
      // Process frames (skip rendering for all but last frame if falling behind)
      for (int i = 0; i < frames_to_process; i++) {
        bool should_render = (i == frames_to_process - 1);
        
        // Run frame
        if (turbo_mode_) {
          snes_.RunFrame();
        }
        snes_.RunFrame();

        // Track FPS
        frame_count_++;
        fps_timer_ += wanted_frames_;
        if (fps_timer_ >= 1.0) {
          current_fps_ = frame_count_ / fps_timer_;
          frame_count_ = 0;
          fps_timer_ = 0.0;
        }

        // Only render and handle audio on the last frame
        if (should_render) {
          // Generate and queue audio samples with improved buffering
          snes_.SetSamples(audio_buffer_, wanted_samples_);
          uint32_t queued = SDL_GetQueuedAudioSize(audio_device_);
          uint32_t target_buffer = wanted_samples_ * 4 * 2;  // Target 2 frames buffered
          uint32_t max_buffer = wanted_samples_ * 4 * 6;     // Max 6 frames

          if (queued < target_buffer) {
            // Buffer is low, queue more audio
            SDL_QueueAudio(audio_device_, audio_buffer_, wanted_samples_ * 4);
          } else if (queued > max_buffer) {
            // Buffer is too full, clear it to prevent lag
            SDL_ClearQueuedAudio(audio_device_);
            SDL_QueueAudio(audio_device_, audio_buffer_, wanted_samples_ * 4);
          }

          // Update PPU texture only on rendered frames
          void* ppu_pixels_;
          int ppu_pitch_;
          if (renderer_->LockTexture(ppu_texture_, NULL, &ppu_pixels_, &ppu_pitch_)) {
            snes_.SetPixels(static_cast<uint8_t*>(ppu_pixels_));
            renderer_->UnlockTexture(ppu_texture_);
          }
        }
      }
    }
  }

  RenderEmulatorInterface();
}

void Emulator::RenderEmulatorInterface() {
  // Apply modern theming with safety checks
  try {
    auto& theme_manager = gui::ThemeManager::Get();
    const auto& theme = theme_manager.GetCurrentTheme();

    // Modern EditorCard-based layout - modular and flexible
    static bool show_cpu_debugger_ = true;
    static bool show_ppu_display_ = true;
    static bool show_memory_viewer_ = false;
    static bool show_breakpoints_ = false;
    static bool show_performance_ = true;
    static bool show_ai_agent_ = false;
    static bool show_save_states_ = false;
    static bool show_keyboard_config_ = false;

    // Create session-aware cards
    gui::EditorCard cpu_card(ICON_MD_MEMORY " CPU Debugger", ICON_MD_MEMORY);
    gui::EditorCard ppu_card(ICON_MD_VIDEOGAME_ASSET " PPU Display",
                             ICON_MD_VIDEOGAME_ASSET);
    gui::EditorCard memory_card(ICON_MD_DATA_ARRAY " Memory Viewer",
                                ICON_MD_DATA_ARRAY);
    gui::EditorCard breakpoints_card(ICON_MD_BUG_REPORT " Breakpoints",
                                     ICON_MD_BUG_REPORT);
    gui::EditorCard performance_card(ICON_MD_SPEED " Performance",
                                     ICON_MD_SPEED);
    gui::EditorCard ai_card(ICON_MD_SMART_TOY " AI Agent", ICON_MD_SMART_TOY);
    gui::EditorCard save_states_card(ICON_MD_SAVE " Save States", ICON_MD_SAVE);
    gui::EditorCard keyboard_card(ICON_MD_KEYBOARD " Keyboard Config",
                                  ICON_MD_KEYBOARD);

    // Configure default positions
    static bool cards_configured = false;
    if (!cards_configured) {
      cpu_card.SetDefaultSize(400, 500);
      cpu_card.SetPosition(gui::EditorCard::Position::Right);

      ppu_card.SetDefaultSize(550, 520);
      ppu_card.SetPosition(gui::EditorCard::Position::Floating);

      memory_card.SetDefaultSize(800, 600);
      memory_card.SetPosition(gui::EditorCard::Position::Floating);

      breakpoints_card.SetDefaultSize(400, 350);
      breakpoints_card.SetPosition(gui::EditorCard::Position::Right);

      performance_card.SetDefaultSize(350, 300);
      performance_card.SetPosition(gui::EditorCard::Position::Bottom);

      ai_card.SetDefaultSize(500, 450);
      ai_card.SetPosition(gui::EditorCard::Position::Floating);

      save_states_card.SetDefaultSize(400, 300);
      save_states_card.SetPosition(gui::EditorCard::Position::Floating);

      keyboard_card.SetDefaultSize(450, 400);
      keyboard_card.SetPosition(gui::EditorCard::Position::Floating);

      cards_configured = true;
    }

    // CPU Debugger Card
    if (show_cpu_debugger_) {
      if (cpu_card.Begin(&show_cpu_debugger_)) {
        RenderModernCpuDebugger();
      }
      cpu_card.End();
    }

    // PPU Display Card
    if (show_ppu_display_) {
      if (ppu_card.Begin(&show_ppu_display_)) {
        RenderSnesPpu();
      }
      ppu_card.End();
    }

    // Memory Viewer Card
    if (show_memory_viewer_) {
      if (memory_card.Begin(&show_memory_viewer_)) {
        RenderMemoryViewer();
      }
      memory_card.End();
    }

    // Breakpoints Card
    if (show_breakpoints_) {
      if (breakpoints_card.Begin(&show_breakpoints_)) {
        RenderBreakpointList();
      }
      breakpoints_card.End();
    }

    // Performance Monitor Card
    if (show_performance_) {
      if (performance_card.Begin(&show_performance_)) {
        RenderPerformanceMonitor();
      }
      performance_card.End();
    }

    // AI Agent Card
    if (show_ai_agent_) {
      if (ai_card.Begin(&show_ai_agent_)) {
        RenderAIAgentPanel();
      }
      ai_card.End();
    }

    // Save States Card
    if (show_save_states_) {
      if (save_states_card.Begin(&show_save_states_)) {
        RenderSaveStates();
      }
      save_states_card.End();
    }

    // Keyboard Configuration Card
    if (show_keyboard_config_) {
      if (keyboard_card.Begin(&show_keyboard_config_)) {
        RenderKeyboardConfig();
      }
      keyboard_card.End();
    }

  } catch (const std::exception& e) {
    // Fallback to basic UI if theming fails
    ImGui::Text("Error loading emulator UI: %s", e.what());
    if (ImGui::Button("Retry")) {
      // Force theme manager reinitialization
      auto& theme_manager = gui::ThemeManager::Get();
      theme_manager.InitializeBuiltInThemes();
    }
  }
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

  if (ImGui::Button(ICON_MD_SYSTEM_UPDATE_ALT)) {}
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

  // Display FPS and Audio Status
  SameLine();
  ImGui::Text("|");
  SameLine();
  if (current_fps_ > 0) {
    ImGui::Text("FPS: %.1f", current_fps_);
  } else {
    ImGui::Text("FPS: --");
  }

  SameLine();
  uint32_t audio_queued = SDL_GetQueuedAudioSize(audio_device_);
  uint32_t audio_frames = audio_queued / (wanted_samples_ * 4);
  ImGui::Text("| Audio: %u frames", audio_frames);

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
    auto file_name = util::FileDialogWrapper::ShowOpenFileDialog();
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

  if (ImGui::Combo("##TypeOfMemory", &current_memory_mode, "PRG\0RAM\0")) {}

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

void Emulator::RenderModernCpuDebugger() {
  try {
    auto& theme_manager = gui::ThemeManager::Get();
    const auto& theme = theme_manager.GetCurrentTheme();

    ImGui::TextColored(ConvertColorToImVec4(theme.accent), "CPU Status");
    ImGui::PushStyleColor(ImGuiCol_ChildBg,
                          ConvertColorToImVec4(theme.child_bg));
    ImGui::BeginChild("##CpuStatus", ImVec2(0, 120), true);

    // Compact register display in a table
    if (ImGui::BeginTable(
            "Registers", 4,
            ImGuiTableFlags_Borders | ImGuiTableFlags_SizingFixedFit)) {
      ImGui::TableSetupColumn("Register", ImGuiTableColumnFlags_WidthFixed, 60);
      ImGui::TableSetupColumn("Value", ImGuiTableColumnFlags_WidthFixed, 80);
      ImGui::TableSetupColumn("Register", ImGuiTableColumnFlags_WidthFixed, 60);
      ImGui::TableSetupColumn("Value", ImGuiTableColumnFlags_WidthFixed, 80);
      ImGui::TableHeadersRow();

      ImGui::TableNextRow();
      ImGui::TableNextColumn();
      ImGui::Text("A");
      ImGui::TableNextColumn();
      ImGui::TextColored(ConvertColorToImVec4(theme.accent), "0x%04X",
                         snes_.cpu().A);
      ImGui::TableNextColumn();
      ImGui::Text("D");
      ImGui::TableNextColumn();
      ImGui::TextColored(ConvertColorToImVec4(theme.accent), "0x%04X",
                         snes_.cpu().D);

      ImGui::TableNextRow();
      ImGui::TableNextColumn();
      ImGui::Text("X");
      ImGui::TableNextColumn();
      ImGui::TextColored(ConvertColorToImVec4(theme.accent), "0x%04X",
                         snes_.cpu().X);
      ImGui::TableNextColumn();
      ImGui::Text("DB");
      ImGui::TableNextColumn();
      ImGui::TextColored(ConvertColorToImVec4(theme.accent), "0x%02X",
                         snes_.cpu().DB);

      ImGui::TableNextRow();
      ImGui::TableNextColumn();
      ImGui::Text("Y");
      ImGui::TableNextColumn();
      ImGui::TextColored(ConvertColorToImVec4(theme.accent), "0x%04X",
                         snes_.cpu().Y);
      ImGui::TableNextColumn();
      ImGui::Text("PB");
      ImGui::TableNextColumn();
      ImGui::TextColored(ConvertColorToImVec4(theme.accent), "0x%02X",
                         snes_.cpu().PB);

      ImGui::TableNextRow();
      ImGui::TableNextColumn();
      ImGui::Text("PC");
      ImGui::TableNextColumn();
      ImGui::TextColored(ConvertColorToImVec4(theme.success), "0x%04X",
                         snes_.cpu().PC);
      ImGui::TableNextColumn();
      ImGui::Text("SP");
      ImGui::TableNextColumn();
      ImGui::TextColored(ConvertColorToImVec4(theme.accent), "0x%02X",
                         snes_.memory().mutable_sp());

      ImGui::TableNextRow();
      ImGui::TableNextColumn();
      ImGui::Text("PS");
      ImGui::TableNextColumn();
      ImGui::TextColored(ConvertColorToImVec4(theme.warning), "0x%02X",
                         snes_.cpu().status);
      ImGui::TableNextColumn();
      ImGui::Text("Cycle");
      ImGui::TableNextColumn();
      ImGui::TextColored(ConvertColorToImVec4(theme.info), "%llu",
                         snes_.mutable_cycles());

      ImGui::EndTable();
    }

    ImGui::EndChild();
    ImGui::PopStyleColor();

    // SPC700 Status Panel
    ImGui::TextColored(ConvertColorToImVec4(theme.accent), "SPC700 Status");
    ImGui::PushStyleColor(ImGuiCol_ChildBg,
                          ConvertColorToImVec4(theme.child_bg));
    ImGui::BeginChild("##SpcStatus", ImVec2(0, 80), true);

    if (ImGui::BeginTable(
            "SPCRegisters", 4,
            ImGuiTableFlags_Borders | ImGuiTableFlags_SizingFixedFit)) {
      ImGui::TableSetupColumn("Register", ImGuiTableColumnFlags_WidthFixed, 50);
      ImGui::TableSetupColumn("Value", ImGuiTableColumnFlags_WidthFixed, 60);
      ImGui::TableSetupColumn("Register", ImGuiTableColumnFlags_WidthFixed, 50);
      ImGui::TableSetupColumn("Value", ImGuiTableColumnFlags_WidthFixed, 60);
      ImGui::TableHeadersRow();

      ImGui::TableNextRow();
      ImGui::TableNextColumn();
      ImGui::Text("A");
      ImGui::TableNextColumn();
      ImGui::TextColored(ConvertColorToImVec4(theme.accent), "0x%02X",
                         snes_.apu().spc700().A);
      ImGui::TableNextColumn();
      ImGui::Text("PC");
      ImGui::TableNextColumn();
      ImGui::TextColored(ConvertColorToImVec4(theme.success), "0x%04X",
                         snes_.apu().spc700().PC);

      ImGui::TableNextRow();
      ImGui::TableNextColumn();
      ImGui::Text("X");
      ImGui::TableNextColumn();
      ImGui::TextColored(ConvertColorToImVec4(theme.accent), "0x%02X",
                         snes_.apu().spc700().X);
      ImGui::TableNextColumn();
      ImGui::Text("SP");
      ImGui::TableNextColumn();
      ImGui::TextColored(ConvertColorToImVec4(theme.accent), "0x%02X",
                         snes_.apu().spc700().SP);

      ImGui::TableNextRow();
      ImGui::TableNextColumn();
      ImGui::Text("Y");
      ImGui::TableNextColumn();
      ImGui::TextColored(ConvertColorToImVec4(theme.accent), "0x%02X",
                         snes_.apu().spc700().Y);
      ImGui::TableNextColumn();
      ImGui::Text("PSW");
      ImGui::TableNextColumn();
      ImGui::TextColored(
          ConvertColorToImVec4(theme.warning), "0x%02X",
          snes_.apu().spc700().FlagsToByte(snes_.apu().spc700().PSW));

      ImGui::EndTable();
    }

    ImGui::EndChild();
    ImGui::PopStyleColor();

    // New Disassembly Viewer
    if (ImGui::CollapsingHeader("Disassembly Viewer", 
                                ImGuiTreeNodeFlags_DefaultOpen)) {
      uint32_t current_pc = (static_cast<uint32_t>(snes_.cpu().PB) << 16) | snes_.cpu().PC;
      auto& disasm = snes_.cpu().disassembly_viewer();
      if (disasm.IsAvailable()) {
        disasm.Render(current_pc, snes_.cpu().breakpoints_);
      } else {
        ImGui::TextColored(ConvertColorToImVec4(theme.error), "Disassembly viewer unavailable.");
      }
    }
  } catch (const std::exception& e) {
    // Ensure any pushed styles are popped on error
    try {
      ImGui::PopStyleColor();
    } catch (...) {
      // Ignore PopStyleColor errors
    }
    ImGui::Text("CPU Debugger Error: %s", e.what());
  }
}

void Emulator::RenderPerformanceMonitor() {
  try {
    auto& theme_manager = gui::ThemeManager::Get();
    const auto& theme = theme_manager.GetCurrentTheme();

    ImGui::PushStyleColor(ImGuiCol_ChildBg,
                          ConvertColorToImVec4(theme.child_bg));
    ImGui::BeginChild("##PerformanceMonitor", ImVec2(0, 0), true);

    // Performance Metrics
    if (ImGui::CollapsingHeader("Real-time Metrics",
                                ImGuiTreeNodeFlags_DefaultOpen)) {
      ImGui::Columns(2, "PerfColumns");

      // Frame Rate
      ImGui::Text("Frame Rate:");
      ImGui::SameLine();
      if (current_fps_ > 0) {
        ImVec4 fps_color = (current_fps_ >= 59.0 && current_fps_ <= 61.0)
                               ? ConvertColorToImVec4(theme.success)
                               : ConvertColorToImVec4(theme.error);
        ImGui::TextColored(fps_color, "%.1f FPS", current_fps_);
      } else {
        ImGui::TextColored(ConvertColorToImVec4(theme.warning), "-- FPS");
      }

      // Audio Status
      uint32_t audio_queued = SDL_GetQueuedAudioSize(audio_device_);
      uint32_t audio_frames = audio_queued / (wanted_samples_ * 4);
      ImGui::Text("Audio Queue:");
      ImGui::SameLine();
      ImVec4 audio_color = (audio_frames >= 2 && audio_frames <= 6)
                               ? ConvertColorToImVec4(theme.success)
                               : ConvertColorToImVec4(theme.warning);
      ImGui::TextColored(audio_color, "%u frames", audio_frames);

      ImGui::NextColumn();

      // Timing
      double frame_time = (current_fps_ > 0) ? (1000.0 / current_fps_) : 0.0;
      ImGui::Text("Frame Time:");
      ImGui::SameLine();
      ImGui::TextColored(ConvertColorToImVec4(theme.info), "%.2f ms",
                         frame_time);

      // Emulation State
      ImGui::Text("State:");
      ImGui::SameLine();
      ImVec4 state_color = running_ ? ConvertColorToImVec4(theme.success)
                                    : ConvertColorToImVec4(theme.warning);
      ImGui::TextColored(state_color, "%s", running_ ? "Running" : "Paused");

      ImGui::Columns(1);
    }

    // Memory Usage
    if (ImGui::CollapsingHeader("Memory Usage")) {
      ImGui::Text("ROM Size: %zu bytes", rom_data_.size());
      ImGui::Text("RAM Usage: %d KB", 128);  // SNES RAM is 128KB
      ImGui::Text("VRAM Usage: %d KB", 64);  // SNES VRAM is 64KB
    }

    ImGui::EndChild();
    ImGui::PopStyleColor();
  } catch (const std::exception& e) {
    // Ensure any pushed styles are popped on error
    try {
      ImGui::PopStyleColor();
    } catch (...) {
      // Ignore PopStyleColor errors
    }
    ImGui::Text("Performance Monitor Error: %s", e.what());
  }
}

void Emulator::RenderAIAgentPanel() {
  try {
    auto& theme_manager = gui::ThemeManager::Get();
    const auto& theme = theme_manager.GetCurrentTheme();

    ImGui::PushStyleColor(ImGuiCol_ChildBg,
                          ConvertColorToImVec4(theme.child_bg));
    ImGui::BeginChild("##AIAgentPanel", ImVec2(0, 0), true);

    // AI Agent Status
    if (ImGui::CollapsingHeader("Agent Status",
                                ImGuiTreeNodeFlags_DefaultOpen)) {
      auto metrics = GetMetrics();

      ImGui::Columns(2, "AgentColumns");

      // Emulator Readiness
      ImGui::Text("Emulator Ready:");
      ImGui::SameLine();
      ImVec4 ready_color = IsEmulatorReady()
                               ? ConvertColorToImVec4(theme.success)
                               : ConvertColorToImVec4(theme.error);
      ImGui::TextColored(ready_color, "%s", IsEmulatorReady() ? "Yes" : "No");

      // Current State
      ImGui::Text("Current PC:");
      ImGui::SameLine();
      ImGui::TextColored(ConvertColorToImVec4(theme.accent), "0x%04X:%02X",
                         metrics.cpu_pc, metrics.cpu_pb);

      ImGui::NextColumn();

      // Performance
      ImGui::Text("FPS:");
      ImGui::SameLine();
      ImVec4 fps_color = (metrics.fps >= 59.0)
                             ? ConvertColorToImVec4(theme.success)
                             : ConvertColorToImVec4(theme.warning);
      ImGui::TextColored(fps_color, "%.1f", metrics.fps);

      // Cycles
      ImGui::Text("Cycles:");
      ImGui::SameLine();
      ImGui::TextColored(ConvertColorToImVec4(theme.info), "%llu",
                         metrics.cycles);

      ImGui::Columns(1);
    }

    // AI Agent Controls
    if (ImGui::CollapsingHeader("Agent Controls",
                                ImGuiTreeNodeFlags_DefaultOpen)) {
      ImGui::Columns(2, "ControlColumns");

      // Single Step Control
      if (ImGui::Button("Step Instruction", ImVec2(-1, 30))) {
        StepSingleInstruction();
      }
      if (ImGui::IsItemHovered()) {
        ImGui::SetTooltip("Execute a single CPU instruction");
      }

      // Breakpoint Controls
      static char bp_input[10] = "";
      ImGui::InputText("Breakpoint Address", bp_input, IM_ARRAYSIZE(bp_input));
      if (ImGui::Button("Add Breakpoint", ImVec2(-1, 25))) {
        if (strlen(bp_input) > 0) {
          uint32_t addr = std::stoi(bp_input, nullptr, 16);
          SetBreakpoint(addr);
          memset(bp_input, 0, sizeof(bp_input));
        }
      }

      ImGui::NextColumn();

      // Clear All Breakpoints
      if (ImGui::Button("Clear All Breakpoints", ImVec2(-1, 30))) {
        ClearAllBreakpoints();
      }
      if (ImGui::IsItemHovered()) {
        ImGui::SetTooltip("Remove all active breakpoints");
      }

      // Toggle Emulation
      if (ImGui::Button(running_ ? "Pause Emulation" : "Resume Emulation",
                        ImVec2(-1, 30))) {
        running_ = !running_;
      }

      ImGui::Columns(1);
    }

    // Current Breakpoints
    if (ImGui::CollapsingHeader("Active Breakpoints")) {
      auto breakpoints = GetBreakpoints();
      if (breakpoints.empty()) {
        ImGui::TextColored(ConvertColorToImVec4(theme.text_disabled),
                           "No breakpoints set");
      } else {
        ImGui::BeginChild("BreakpointsList", ImVec2(0, 150), true);
        for (auto bp : breakpoints) {
          if (ImGui::Selectable(absl::StrFormat("0x%04X", bp).c_str())) {
            // Jump to breakpoint or remove it
          }
          ImGui::SameLine();
          if (ImGui::SmallButton(absl::StrFormat("Remove##%04X", bp).c_str())) {
            // TODO: Implement individual breakpoint removal
          }
        }
        ImGui::EndChild();
      }
    }

    // AI Agent API Information
    if (ImGui::CollapsingHeader("API Reference")) {
      ImGui::TextWrapped("Available API functions for AI agents:");
      ImGui::BulletText("IsEmulatorReady() - Check if emulator is ready");
      ImGui::BulletText("GetMetrics() - Get current performance metrics");
      ImGui::BulletText(
          "StepSingleInstruction() - Execute one CPU instruction");
      ImGui::BulletText("SetBreakpoint(address) - Set breakpoint at address");
      ImGui::BulletText("ClearAllBreakpoints() - Remove all breakpoints");
      ImGui::BulletText("GetBreakpoints() - Get list of active breakpoints");
    }

    ImGui::EndChild();
    ImGui::PopStyleColor();
  } catch (const std::exception& e) {
    // Ensure any pushed styles are popped on error
    try {
      ImGui::PopStyleColor();
    } catch (...) {
      // Ignore PopStyleColor errors
    }
    ImGui::Text("AI Agent Panel Error: %s", e.what());
  }
}

void Emulator::RenderCpuInstructionLog(
    const std::vector<InstructionEntry>& instruction_log) {
  // Filtering options
  static char filter[256];
  ImGui::InputText("Filter", filter, IM_ARRAYSIZE(filter));

  // Instruction list
  ImGui::BeginChild("InstructionList", ImVec2(0, 0), ImGuiChildFlags_None);
  for (const auto& entry : instruction_log) {
    if (ShouldDisplay(entry, filter)) {
      if (ImGui::Selectable(absl::StrFormat("%06X:", entry.address).c_str())) {
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

void Emulator::RenderSaveStates() {
  try {
    auto& theme_manager = gui::ThemeManager::Get();
    const auto& theme = theme_manager.GetCurrentTheme();

    ImGui::PushStyleColor(ImGuiCol_ChildBg,
                          ConvertColorToImVec4(theme.child_bg));
    ImGui::BeginChild("##SaveStates", ImVec2(0, 0), true);

    // Save State Management
    if (ImGui::CollapsingHeader("Quick Save/Load",
                                ImGuiTreeNodeFlags_DefaultOpen)) {
      ImGui::Columns(2, "SaveStateColumns");

      // Save slots
      for (int i = 1; i <= 4; ++i) {
        if (ImGui::Button(absl::StrFormat("Save Slot %d", i).c_str(),
                          ImVec2(-1, 30))) {
          // TODO: Implement save state to slot
        }
        if (ImGui::IsItemHovered()) {
          ImGui::SetTooltip("Save current state to slot %d (F%d)", i, i);
        }
      }

      ImGui::NextColumn();

      // Load slots
      for (int i = 1; i <= 4; ++i) {
        if (ImGui::Button(absl::StrFormat("Load Slot %d", i).c_str(),
                          ImVec2(-1, 30))) {
          // TODO: Implement load state from slot
        }
        if (ImGui::IsItemHovered()) {
          ImGui::SetTooltip("Load state from slot %d (Shift+F%d)", i, i);
        }
      }

      ImGui::Columns(1);
    }

    // File-based save states
    if (ImGui::CollapsingHeader("File-based Saves")) {
      static char save_name[256] = "";
      ImGui::InputText("Save Name", save_name, IM_ARRAYSIZE(save_name));

      if (ImGui::Button("Save to File", ImVec2(-1, 30))) {
        // TODO: Implement save to file
      }

      if (ImGui::Button("Load from File", ImVec2(-1, 30))) {
        // TODO: Implement load from file
      }
    }

    // Rewind functionality
    if (ImGui::CollapsingHeader("Rewind")) {
      ImGui::TextWrapped(
          "Rewind functionality allows you to step back through recent "
          "gameplay.");

      static bool rewind_enabled = false;
      ImGui::Checkbox("Enable Rewind (uses more memory)", &rewind_enabled);

      if (rewind_enabled) {
        if (ImGui::Button("Rewind 1 Second", ImVec2(-1, 30))) {
          // TODO: Implement rewind
        }
        if (ImGui::IsItemHovered()) {
          ImGui::SetTooltip("Rewind gameplay by 1 second (Backquote key)");
        }
      } else {
        ImGui::TextColored(ConvertColorToImVec4(theme.text_disabled),
                           "Enable rewind to use this feature");
      }
    }

    ImGui::EndChild();
    ImGui::PopStyleColor();
  } catch (const std::exception& e) {
    try {
      ImGui::PopStyleColor();
    } catch (...) {}
    ImGui::Text("Save States Error: %s", e.what());
  }
}

void Emulator::RenderKeyboardConfig() {
  try {
    auto& theme_manager = gui::ThemeManager::Get();
    const auto& theme = theme_manager.GetCurrentTheme();

    ImGui::PushStyleColor(ImGuiCol_ChildBg,
                          ConvertColorToImVec4(theme.child_bg));
    ImGui::BeginChild("##KeyboardConfig", ImVec2(0, 0), true);

    // Keyboard Configuration
    if (ImGui::CollapsingHeader("SNES Controller Mapping",
                                ImGuiTreeNodeFlags_DefaultOpen)) {
      ImGui::TextWrapped("Click on a button and press a key to remap it.");
      ImGui::Separator();

      if (ImGui::BeginTable("KeyboardTable", 2,
                            ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg)) {
        ImGui::TableSetupColumn("Button", ImGuiTableColumnFlags_WidthFixed,
                                120);
        ImGui::TableSetupColumn("Key", ImGuiTableColumnFlags_WidthStretch);
        ImGui::TableHeadersRow();

        auto DrawKeyBinding = [&](const char* label, ImGuiKey& key) {
          ImGui::TableNextRow();
          ImGui::TableNextColumn();
          ImGui::TextColored(ConvertColorToImVec4(theme.accent), "%s", label);
          ImGui::TableNextColumn();

          std::string button_label =
              absl::StrFormat("%s##%s", ImGui::GetKeyName(key), label);
          if (ImGui::Button(button_label.c_str(), ImVec2(-1, 0))) {
            // TODO: Implement key remapping
            ImGui::OpenPopup(absl::StrFormat("Remap%s", label).c_str());
          }

          if (ImGui::BeginPopup(absl::StrFormat("Remap%s", label).c_str())) {
            ImGui::Text("Press any key...");
            // TODO: Detect key press and update binding
            ImGui::EndPopup();
          }
        };

        DrawKeyBinding("A Button", keybindings_.a_button);
        DrawKeyBinding("B Button", keybindings_.b_button);
        DrawKeyBinding("X Button", keybindings_.x_button);
        DrawKeyBinding("Y Button", keybindings_.y_button);
        DrawKeyBinding("L Button", keybindings_.l_button);
        DrawKeyBinding("R Button", keybindings_.r_button);
        DrawKeyBinding("Start", keybindings_.start_button);
        DrawKeyBinding("Select", keybindings_.select_button);
        DrawKeyBinding("Up", keybindings_.up_button);
        DrawKeyBinding("Down", keybindings_.down_button);
        DrawKeyBinding("Left", keybindings_.left_button);
        DrawKeyBinding("Right", keybindings_.right_button);

        ImGui::EndTable();
      }
    }

    // Emulator Hotkeys
    if (ImGui::CollapsingHeader("Emulator Hotkeys")) {
      ImGui::TextWrapped("System-level keyboard shortcuts:");
      ImGui::Separator();

      ImGui::BulletText("F1-F4: Quick save to slot 1-4");
      ImGui::BulletText("Shift+F1-F4: Quick load from slot 1-4");
      ImGui::BulletText("Backquote (`): Rewind gameplay");
      ImGui::BulletText("Tab: Fast forward (turbo mode)");
      ImGui::BulletText("Pause/Break: Pause/Resume emulation");
      ImGui::BulletText("F12: Take screenshot");
    }

    // Reset to defaults
    if (ImGui::Button("Reset to Defaults", ImVec2(-1, 35))) {
      keybindings_ = EmulatorKeybindings();
    }

    ImGui::EndChild();
    ImGui::PopStyleColor();
  } catch (const std::exception& e) {
    try {
      ImGui::PopStyleColor();
    } catch (...) {}
    ImGui::Text("Keyboard Config Error: %s", e.what());
  }
}

}  // namespace emu
}  // namespace yaze
