#include "app/emu/ui/emulator_ui.h"

#include <fstream>

#include "absl/strings/str_format.h"
#include "app/emu/emulator.h"
#include "app/gui/core/color.h"
#include "app/gui/core/icons.h"
#include "app/gui/core/theme_manager.h"
#include "imgui/imgui.h"
#include "util/file_util.h"
#include "util/log.h"

namespace yaze {
namespace emu {
namespace ui {

using namespace yaze::gui;

namespace {
// UI Constants for consistent spacing
constexpr float kStandardSpacing = 8.0f;
constexpr float kSectionSpacing = 16.0f;
constexpr float kButtonHeight = 30.0f;
constexpr float kIconSize = 24.0f;

// Helper to add consistent spacing
void AddSpacing() {
  ImGui::Spacing();
  ImGui::Spacing();
}

void AddSectionSpacing() {
  ImGui::Spacing();
  ImGui::Separator();
  ImGui::Spacing();
}

}  // namespace

void RenderNavBar(Emulator* emu) {
  if (!emu) return;
  
  auto& theme_manager = ThemeManager::Get();
  const auto& theme = theme_manager.GetCurrentTheme();
  
  // Handle keyboard shortcuts for emulator control
  // IMPORTANT: Use Shortcut() to avoid conflicts with game input
  // Space - toggle play/pause (only when not typing in text fields)
  if (ImGui::Shortcut(ImGuiKey_Space, ImGuiInputFlags_RouteGlobal)) {
    emu->set_running(!emu->running());
  }
  
  // F10 - step one frame
  if (ImGui::Shortcut(ImGuiKey_F10, ImGuiInputFlags_RouteGlobal)) {
    if (!emu->running()) {
      emu->snes().RunFrame();
    }
  }
  
  // Navbar with theme colors
  ImGui::PushStyleColor(ImGuiCol_Button, ConvertColorToImVec4(theme.button));
  ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ConvertColorToImVec4(theme.button_hovered));
  ImGui::PushStyleColor(ImGuiCol_ButtonActive, ConvertColorToImVec4(theme.button_active));
  
  // Play/Pause button with icon
  bool is_running = emu->running();
  if (is_running) {
    if (ImGui::Button(ICON_MD_PAUSE, ImVec2(50, kButtonHeight))) {
      emu->set_running(false);
    }
    if (ImGui::IsItemHovered()) {
      ImGui::SetTooltip("Pause emulation (Space)");
    }
  } else {
    if (ImGui::Button(ICON_MD_PLAY_ARROW, ImVec2(50, kButtonHeight))) {
      emu->set_running(true);
    }
    if (ImGui::IsItemHovered()) {
      ImGui::SetTooltip("Start emulation (Space)");
    }
  }
  
  ImGui::SameLine();
  
  // Step button
  if (ImGui::Button(ICON_MD_SKIP_NEXT, ImVec2(50, kButtonHeight))) {
    if (!is_running) {
      emu->snes().RunFrame();
    }
  }
  if (ImGui::IsItemHovered()) {
    ImGui::SetTooltip("Step one frame (F10)");
  }
  
  ImGui::SameLine();
  
  // Reset button
  if (ImGui::Button(ICON_MD_RESTART_ALT, ImVec2(50, kButtonHeight))) {
    emu->snes().Reset();
    LOG_INFO("Emulator", "System reset");
  }
  if (ImGui::IsItemHovered()) {
    ImGui::SetTooltip("Reset SNES (Ctrl+R)");
  }

  ImGui::SameLine();

  // Load ROM button
  if (ImGui::Button(ICON_MD_FOLDER_OPEN " Load ROM", ImVec2(110, kButtonHeight))) {
    std::string rom_path = util::FileDialogWrapper::ShowOpenFileDialog();
    if (!rom_path.empty()) {
      // Check if it's a valid ROM file extension
      std::string ext = util::GetFileExtension(rom_path);
      if (ext == ".sfc" || ext == ".smc" || ext == ".SFC" || ext == ".SMC") {
        try {
          // Read ROM file into memory
          std::ifstream rom_file(rom_path, std::ios::binary);
          if (rom_file.good()) {
            std::vector<uint8_t> rom_data(
              (std::istreambuf_iterator<char>(rom_file)),
              std::istreambuf_iterator<char>()
            );
            rom_file.close();

            // Reinitialize emulator with new ROM
            if (!rom_data.empty()) {
              emu->Initialize(emu->renderer(), rom_data);
              LOG_INFO("Emulator", "Loaded ROM: %s (%zu bytes)",
                      util::GetFileName(rom_path).c_str(), rom_data.size());
            } else {
              LOG_ERROR("Emulator", "ROM file is empty: %s", rom_path.c_str());
            }
          } else {
            LOG_ERROR("Emulator", "Failed to open ROM file: %s", rom_path.c_str());
          }
        } catch (const std::exception& e) {
          LOG_ERROR("Emulator", "Error loading ROM: %s", e.what());
        }
      } else {
        LOG_WARN("Emulator", "Invalid ROM file extension: %s (expected .sfc or .smc)",
                ext.c_str());
      }
    }
  }
  if (ImGui::IsItemHovered()) {
    ImGui::SetTooltip("Load a different ROM file\n"
                     "Allows testing hacks with assembly patches applied");
  }

  ImGui::SameLine();
  ImGui::Separator();
  ImGui::SameLine();
  
  // Debugger toggle
  bool is_debugging = emu->is_debugging();
  if (ImGui::Checkbox(ICON_MD_BUG_REPORT " Debug", &is_debugging)) {
    emu->set_debugging(is_debugging);
  }
  if (ImGui::IsItemHovered()) {
    ImGui::SetTooltip("Enable debugger features");
  }
  
  ImGui::SameLine();
  
  // Recording toggle (for DisassemblyViewer)
  // Access through emulator's disassembly viewer
  // bool recording = emu->disassembly_viewer().IsRecording();
  // if (ImGui::Checkbox(ICON_MD_FIBER_MANUAL_RECORD " Rec", &recording)) {
  //   emu->disassembly_viewer().SetRecording(recording);
  // }
  // if (ImGui::IsItemHovered()) {
  //   ImGui::SetTooltip("Record instructions to Disassembly Viewer\n(Lightweight - uses sparse address map)");
  // }
  
  ImGui::SameLine();
  
  // Turbo mode
  bool turbo = emu->is_turbo_mode();
  if (ImGui::Checkbox(ICON_MD_FAST_FORWARD " Turbo", &turbo)) {
    emu->set_turbo_mode(turbo);
  }
  if (ImGui::IsItemHovered()) {
    ImGui::SetTooltip("Fast forward (shortcut: hold Tab)");
  }
  
  ImGui::SameLine();
  ImGui::Separator();
  ImGui::SameLine();
  
  // FPS Counter with color coding
  double fps = emu->GetCurrentFPS();
  ImVec4 fps_color;
  if (fps >= 58.0) {
    fps_color = ConvertColorToImVec4(theme.success);  // Green for good FPS
  } else if (fps >= 45.0) {
    fps_color = ConvertColorToImVec4(theme.warning);  // Yellow for okay FPS
  } else {
    fps_color = ConvertColorToImVec4(theme.error);    // Red for bad FPS
  }
  
  ImGui::TextColored(fps_color, ICON_MD_SPEED " %.1f FPS", fps);
  
  ImGui::SameLine();

  // Audio backend status
  if (emu->audio_backend()) {
    auto audio_status = emu->audio_backend()->GetStatus();
    ImVec4 audio_color = audio_status.is_playing ?
      ConvertColorToImVec4(theme.success) : ConvertColorToImVec4(theme.text_disabled);

    ImGui::TextColored(audio_color, ICON_MD_VOLUME_UP " %s | %u frames",
                      emu->audio_backend()->GetBackendName().c_str(),
                      audio_status.queued_frames);

    if (ImGui::IsItemHovered()) {
      ImGui::SetTooltip("Audio Backend: %s\nQueued: %u frames\nPlaying: %s",
                       emu->audio_backend()->GetBackendName().c_str(),
                       audio_status.queued_frames,
                       audio_status.is_playing ? "YES" : "NO");
    }


    ImGui::SameLine();
    static bool use_sdl_audio_stream = emu->use_sdl_audio_stream();
    if (ImGui::Checkbox(ICON_MD_SETTINGS " SDL Audio Stream", &use_sdl_audio_stream)) {
      emu->set_use_sdl_audio_stream(use_sdl_audio_stream);
    }
    if (ImGui::IsItemHovered()) {
      ImGui::SetTooltip("Use SDL audio stream for audio");
    }
  } else {
    ImGui::TextColored(ConvertColorToImVec4(theme.error),
                      ICON_MD_VOLUME_OFF " No Backend");
  }

  ImGui::SameLine();
  ImGui::Separator();
  ImGui::SameLine();

  // Input capture status indicator (like modern emulators)
  ImGuiIO& io = ImGui::GetIO();
  if (io.WantCaptureKeyboard) {
    // ImGui is capturing keyboard (typing in UI)
    ImGui::TextColored(ConvertColorToImVec4(theme.warning),
                      ICON_MD_KEYBOARD " UI");
    if (ImGui::IsItemHovered()) {
      ImGui::SetTooltip("Keyboard captured by UI\nGame input disabled");
    }
  } else {
    // Emulator can receive input
    ImGui::TextColored(ConvertColorToImVec4(theme.success),
                      ICON_MD_SPORTS_ESPORTS " Game");
    if (ImGui::IsItemHovered()) {
      ImGui::SetTooltip("Game input active\nPress F1 for controls");
    }
  }

  ImGui::PopStyleColor(3);
}

void RenderSnesPpu(Emulator* emu) {
  if (!emu) return;

  auto& theme_manager = ThemeManager::Get();
  const auto& theme = theme_manager.GetCurrentTheme();

  ImGui::PushStyleColor(ImGuiCol_ChildBg, ConvertColorToImVec4(theme.editor_background));
  ImGui::BeginChild("##SNES_PPU", ImVec2(0, 0), true,
                    ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse);

  ImVec2 canvas_size = ImGui::GetContentRegionAvail();
  ImVec2 snes_size = ImVec2(512, 480);

  if (emu->is_snes_initialized() && emu->ppu_texture()) {
    // Center the SNES display with aspect ratio preservation
    float aspect = snes_size.x / snes_size.y;
    float display_w = canvas_size.x;
    float display_h = display_w / aspect;

    if (display_h > canvas_size.y) {
      display_h = canvas_size.y;
      display_w = display_h * aspect;
    }

    float pos_x = (canvas_size.x - display_w) * 0.5f;
    float pos_y = (canvas_size.y - display_h) * 0.5f;

    ImGui::SetCursorPos(ImVec2(pos_x, pos_y));

    // Render PPU texture with click detection for focus
    ImGui::Image((ImTextureID)(intptr_t)emu->ppu_texture(),
                ImVec2(display_w, display_h),
                ImVec2(0, 0), ImVec2(1, 1));

    // Allow clicking on the display to ensure focus
    // Modern emulators make the game area "sticky" for input
    if (ImGui::IsItemHovered()) {
      // ImGui::SetTooltip("Click to ensure game input focus");

      // Visual feedback when hovered (subtle border)
      ImDrawList* draw_list = ImGui::GetWindowDrawList();
      ImVec2 screen_pos = ImGui::GetItemRectMin();
      ImVec2 screen_size = ImGui::GetItemRectMax();
      draw_list->AddRect(screen_pos, screen_size,
                        ImGui::ColorConvertFloat4ToU32(ConvertColorToImVec4(theme.accent)),
                        0.0f, 0, 2.0f);
    }
  } else {
    // Not initialized - show helpful placeholder
    ImVec2 text_size = ImGui::CalcTextSize("Load a ROM to start emulation");
    ImGui::SetCursorPos(ImVec2((canvas_size.x - text_size.x) * 0.5f,
                              (canvas_size.y - text_size.y) * 0.5f - 20));
    ImGui::TextColored(ConvertColorToImVec4(theme.text_disabled),
                      ICON_MD_VIDEOGAME_ASSET);
    ImGui::SetCursorPosX((canvas_size.x - text_size.x) * 0.5f);
    ImGui::TextColored(ConvertColorToImVec4(theme.text_primary),
                      "Load a ROM to start emulation");
    ImGui::SetCursorPosX((canvas_size.x - ImGui::CalcTextSize("512x480 SNES output").x) * 0.5f);
    ImGui::TextColored(ConvertColorToImVec4(theme.text_disabled),
                      "512x480 SNES output");
  }

  ImGui::EndChild();
  ImGui::PopStyleColor();
}

void RenderPerformanceMonitor(Emulator* emu) {
  if (!emu) return;
  
  auto& theme_manager = ThemeManager::Get();
  const auto& theme = theme_manager.GetCurrentTheme();
  
  ImGui::PushStyleColor(ImGuiCol_ChildBg, ConvertColorToImVec4(theme.child_bg));
  ImGui::BeginChild("##Performance", ImVec2(0, 0), true);
  
  ImGui::TextColored(ConvertColorToImVec4(theme.accent), 
                    ICON_MD_SPEED " Performance Monitor");
  AddSectionSpacing();
  
  auto metrics = emu->GetMetrics();
  
  // FPS Graph
  if (ImGui::CollapsingHeader(ICON_MD_SHOW_CHART " Frame Rate", ImGuiTreeNodeFlags_DefaultOpen)) {
    ImGui::Text("Current: %.2f FPS", metrics.fps);
    ImGui::Text("Target: %.2f FPS", emu->snes().memory().pal_timing() ? 50.0 : 60.0);
    
    // TODO: Add FPS graph with ImPlot
  }
  
  // CPU Stats
  if (ImGui::CollapsingHeader(ICON_MD_MEMORY " CPU Status", ImGuiTreeNodeFlags_DefaultOpen)) {
    ImGui::Text("PC: $%02X:%04X", metrics.cpu_pb, metrics.cpu_pc);
    ImGui::Text("Cycles: %llu", metrics.cycles);
  }
  
  // Audio Stats
  if (ImGui::CollapsingHeader(ICON_MD_AUDIOTRACK " Audio Status", ImGuiTreeNodeFlags_DefaultOpen)) {
    if (emu->audio_backend()) {
      auto audio_status = emu->audio_backend()->GetStatus();
      ImGui::Text("Backend: %s", emu->audio_backend()->GetBackendName().c_str());
      ImGui::Text("Queued: %u frames", audio_status.queued_frames);
      ImGui::Text("Playing: %s", audio_status.is_playing ? "YES" : "NO");
    } else {
      ImGui::TextColored(ConvertColorToImVec4(theme.error), "No audio backend");
    }
  }
  
  ImGui::EndChild();
  ImGui::PopStyleColor();
}

void RenderKeyboardShortcuts(bool* show) {
  if (!show || !*show) return;

  auto& theme_manager = ThemeManager::Get();
  const auto& theme = theme_manager.GetCurrentTheme();

  // Center the window
  ImVec2 center = ImGui::GetMainViewport()->GetCenter();
  ImGui::SetNextWindowPos(center, ImGuiCond_Appearing, ImVec2(0.5f, 0.5f));
  ImGui::SetNextWindowSize(ImVec2(550, 600), ImGuiCond_Appearing);

  ImGui::PushStyleColor(ImGuiCol_TitleBg, ConvertColorToImVec4(theme.accent));
  ImGui::PushStyleColor(ImGuiCol_TitleBgActive, ConvertColorToImVec4(theme.accent));

  if (ImGui::Begin(ICON_MD_KEYBOARD " Keyboard Shortcuts", show,
                   ImGuiWindowFlags_NoCollapse)) {
    // Emulator controls section
    ImGui::TextColored(ConvertColorToImVec4(theme.accent),
                      ICON_MD_VIDEOGAME_ASSET " Emulator Controls");
    ImGui::Separator();
    ImGui::Spacing();

    if (ImGui::BeginTable("EmulatorControls", 2, ImGuiTableFlags_Borders)) {
      ImGui::TableSetupColumn("Key", ImGuiTableColumnFlags_WidthFixed, 120);
      ImGui::TableSetupColumn("Action", ImGuiTableColumnFlags_WidthStretch);
      ImGui::TableHeadersRow();

      auto AddRow = [](const char* key, const char* action) {
        ImGui::TableNextRow();
        ImGui::TableNextColumn();
        ImGui::Text("%s", key);
        ImGui::TableNextColumn();
        ImGui::Text("%s", action);
      };

      AddRow("Space", "Play/Pause emulation");
      AddRow("F10", "Step one frame");
      AddRow("Ctrl+R", "Reset SNES");
      AddRow("Tab (hold)", "Turbo mode (fast forward)");
      AddRow("F1", "Show/hide this help");

      ImGui::EndTable();
    }

    ImGui::Spacing();
    ImGui::Spacing();

    // Game controls section
    ImGui::TextColored(ConvertColorToImVec4(theme.accent),
                      ICON_MD_SPORTS_ESPORTS " SNES Controller");
    ImGui::Separator();
    ImGui::Spacing();

    if (ImGui::BeginTable("GameControls", 2, ImGuiTableFlags_Borders)) {
      ImGui::TableSetupColumn("Key", ImGuiTableColumnFlags_WidthFixed, 120);
      ImGui::TableSetupColumn("Button", ImGuiTableColumnFlags_WidthStretch);
      ImGui::TableHeadersRow();

      auto AddRow = [](const char* key, const char* button) {
        ImGui::TableNextRow();
        ImGui::TableNextColumn();
        ImGui::Text("%s", key);
        ImGui::TableNextColumn();
        ImGui::Text("%s", button);
      };

      AddRow("Arrow Keys", "D-Pad (Up/Down/Left/Right)");
      AddRow("X", "A Button");
      AddRow("Z", "B Button");
      AddRow("S", "X Button");
      AddRow("A", "Y Button");
      AddRow("D", "L Shoulder");
      AddRow("C", "R Shoulder");
      AddRow("Enter", "Start");
      AddRow("RShift", "Select");

      ImGui::EndTable();
    }

    ImGui::Spacing();
    ImGui::Spacing();

    // Tips section
    ImGui::TextColored(ConvertColorToImVec4(theme.info),
                      ICON_MD_INFO " Tips");
    ImGui::Separator();
    ImGui::Spacing();

    ImGui::BulletText("Input is disabled when typing in UI fields");
    ImGui::BulletText("Check the status bar for input capture state");
    ImGui::BulletText("Click the game screen to ensure focus");
    ImGui::BulletText("The emulator continues running in background");

    ImGui::Spacing();
    ImGui::Spacing();

    if (ImGui::Button("Close", ImVec2(-1, 30))) {
      *show = false;
    }
  }
  ImGui::End();

  ImGui::PopStyleColor(2);
}

void RenderEmulatorInterface(Emulator* emu) {
  if (!emu) return;

  auto& theme_manager = ThemeManager::Get();
  const auto& theme = theme_manager.GetCurrentTheme();

  // Main layout
  ImGui::PushStyleColor(ImGuiCol_ChildBg, ConvertColorToImVec4(theme.window_bg));

  RenderNavBar(emu);

  ImGui::Separator();

  // Main content area
  RenderSnesPpu(emu);

  // Keyboard shortcuts overlay (F1 to toggle)
  static bool show_shortcuts = false;
  if (ImGui::IsKeyPressed(ImGuiKey_F1)) {
    show_shortcuts = !show_shortcuts;
  }
  RenderKeyboardShortcuts(&show_shortcuts);

  ImGui::PopStyleColor();
}

}  // namespace ui
}  // namespace emu
}  // namespace yaze

