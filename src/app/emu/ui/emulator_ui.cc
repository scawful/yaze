#include "app/emu/ui/emulator_ui.h"

#include "absl/strings/str_format.h"
#include "app/emu/emulator.h"
#include "app/gui/color.h"
#include "app/gui/icons.h"
#include "app/gui/theme_manager.h"
#include "imgui/imgui.h"
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
  
  // Navbar with theme colors
  ImGui::PushStyleColor(ImGuiCol_Button, ConvertColorToImVec4(theme.button));
  ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ConvertColorToImVec4(theme.button_hovered));
  ImGui::PushStyleColor(ImGuiCol_ButtonActive, ConvertColorToImVec4(theme.button_active));
  
  // Play/Pause button with icon
  bool is_running = emu->running();
  if (is_running) {
    if (ImGui::Button(ICON_MD_PAUSE " Pause", ImVec2(100, kButtonHeight))) {
      emu->set_running(false);
    }
    if (ImGui::IsItemHovered()) {
      ImGui::SetTooltip("Pause emulation (Space)");
    }
  } else {
    if (ImGui::Button(ICON_MD_PLAY_ARROW " Play", ImVec2(100, kButtonHeight))) {
      emu->set_running(true);
    }
    if (ImGui::IsItemHovered()) {
      ImGui::SetTooltip("Start emulation (Space)");
    }
  }
  
  ImGui::SameLine();
  
  // Step button
  if (ImGui::Button(ICON_MD_SKIP_NEXT " Step", ImVec2(80, kButtonHeight))) {
    if (!is_running) {
      emu->snes().RunFrame();
    }
  }
  if (ImGui::IsItemHovered()) {
    ImGui::SetTooltip("Step one frame (F10)");
  }
  
  ImGui::SameLine();
  
  // Reset button
  if (ImGui::Button(ICON_MD_RESTART_ALT " Reset", ImVec2(80, kButtonHeight))) {
    emu->snes().Reset();
    LOG_INFO("Emulator", "System reset");
  }
  if (ImGui::IsItemHovered()) {
    ImGui::SetTooltip("Reset SNES (Ctrl+R)");
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
  bool turbo = false;  // Need to expose this from Emulator
  if (ImGui::Checkbox(ICON_MD_FAST_FORWARD " Turbo", &turbo)) {
    // emu->set_turbo_mode(turbo);
  }
  if (ImGui::IsItemHovered()) {
    ImGui::SetTooltip("Fast forward (hold Tab)");
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
  } else {
    ImGui::TextColored(ConvertColorToImVec4(theme.error), 
                      ICON_MD_VOLUME_OFF " No Backend");
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
    // Center the SNES display
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
    
    // Render PPU texture
    ImGui::Image((ImTextureID)(intptr_t)emu->ppu_texture(), 
                ImVec2(display_w, display_h), 
                ImVec2(0, 0), ImVec2(1, 1));
  } else {
    // Not initialized - show placeholder
    ImVec2 text_size = ImGui::CalcTextSize("SNES PPU Output\n512x480");
    ImGui::SetCursorPos(ImVec2((canvas_size.x - text_size.x) * 0.5f,
                              (canvas_size.y - text_size.y) * 0.5f));
    ImGui::TextColored(ConvertColorToImVec4(theme.text_disabled),
                      ICON_MD_VIDEOGAME_ASSET "\nSNES PPU Output\n512x480");
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
  
  ImGui::PopStyleColor();
}

}  // namespace ui
}  // namespace emu
}  // namespace yaze

