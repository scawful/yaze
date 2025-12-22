#include "app/emu/ui/emulator_ui.h"

#include <algorithm>
#include <fstream>

#include "absl/strings/str_format.h"
#include "app/emu/emulator.h"
#include "app/emu/input/input_backend.h"
#include "app/gui/core/color.h"
#include "app/gui/core/icons.h"
#include "app/gui/core/theme_manager.h"
#include "app/gui/plots/implot_support.h"
#include "app/platform/sdl_compat.h"
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
  if (!emu)
    return;

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
  ImGui::PushStyleColor(ImGuiCol_ButtonHovered,
                        ConvertColorToImVec4(theme.button_hovered));
  ImGui::PushStyleColor(ImGuiCol_ButtonActive,
                        ConvertColorToImVec4(theme.button_active));

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
  if (ImGui::Button(ICON_MD_FOLDER_OPEN " Load ROM",
                    ImVec2(110, kButtonHeight))) {
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
                std::istreambuf_iterator<char>());
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
            LOG_ERROR("Emulator", "Failed to open ROM file: %s",
                      rom_path.c_str());
          }
        } catch (const std::exception& e) {
          LOG_ERROR("Emulator", "Error loading ROM: %s", e.what());
        }
      } else {
        LOG_WARN("Emulator",
                 "Invalid ROM file extension: %s (expected .sfc or .smc)",
                 ext.c_str());
      }
    }
  }
  if (ImGui::IsItemHovered()) {
    ImGui::SetTooltip(
        "Load a different ROM file\n"
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
  //   ImGui::SetTooltip("Record instructions to Disassembly
  //   Viewer\n(Lightweight - uses sparse address map)");
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
    fps_color = ConvertColorToImVec4(theme.error);  // Red for bad FPS
  }

  ImGui::TextColored(fps_color, ICON_MD_SPEED " %.1f FPS", fps);

  ImGui::SameLine();

  // Audio backend status
  if (emu->audio_backend()) {
    auto audio_status = emu->audio_backend()->GetStatus();
    ImVec4 audio_color = audio_status.is_playing
                             ? ConvertColorToImVec4(theme.success)
                             : ConvertColorToImVec4(theme.text_disabled);

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
    if (ImGui::Checkbox(ICON_MD_SETTINGS " SDL Audio Stream",
                        &use_sdl_audio_stream)) {
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
  const auto input_config = emu->input_manager().GetConfig();
  if (io.WantCaptureKeyboard && !input_config.ignore_imgui_text_input) {
    // ImGui is capturing keyboard (typing in UI)
    ImGui::TextColored(ConvertColorToImVec4(theme.warning),
                       ICON_MD_KEYBOARD " UI");
    if (ImGui::IsItemHovered()) {
      ImGui::SetTooltip("Keyboard captured by UI\nGame input disabled");
    }
  } else {
    // Emulator can receive input
    ImVec4 state_color = input_config.ignore_imgui_text_input
                             ? ConvertColorToImVec4(theme.accent)
                             : ConvertColorToImVec4(theme.success);
    ImGui::TextColored(
        state_color,
        input_config.ignore_imgui_text_input
            ? ICON_MD_SPORTS_ESPORTS " Game (Forced)"
            : ICON_MD_SPORTS_ESPORTS " Game");
    if (ImGui::IsItemHovered()) {
      ImGui::SetTooltip(
          input_config.ignore_imgui_text_input
              ? "Game input forced on (ignores ImGui text capture)\nPress F1 "
                "for controls"
              : "Game input active\nPress F1 for controls");
    }
  }

  ImGui::SameLine();
  bool force_game_input = input_config.ignore_imgui_text_input;
  if (ImGui::Checkbox("Force Game Input", &force_game_input)) {
    auto cfg = input_config;
    cfg.ignore_imgui_text_input = force_game_input;
    emu->SetInputConfig(cfg);
  }
  if (ImGui::IsItemHovered()) {
    ImGui::SetTooltip(
        "When enabled, emulator input is not blocked by ImGui text widgets.\n"
        "Use if the game controls stop working while typing in other panels.");
  }

  ImGui::SameLine();

  // Option to disable ImGui keyboard navigation (prevents Tab from cycling UI)
  static bool disable_nav = false;
  if (ImGui::Checkbox("Disable Nav", &disable_nav)) {
    ImGuiIO& imgui_io = ImGui::GetIO();
    if (disable_nav) {
      imgui_io.ConfigFlags &= ~ImGuiConfigFlags_NavEnableKeyboard;
    } else {
      imgui_io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
    }
  }
  if (ImGui::IsItemHovered()) {
    ImGui::SetTooltip(
        "Disable ImGui keyboard navigation.\n"
        "Prevents Tab from cycling through UI elements.\n"
        "Enable this if Tab isn't working for turbo mode.");
  }

  ImGui::PopStyleColor(3);
}

void RenderSnesPpu(Emulator* emu) {
  if (!emu)
    return;

  auto& theme_manager = ThemeManager::Get();
  const auto& theme = theme_manager.GetCurrentTheme();

  ImGui::PushStyleColor(ImGuiCol_ChildBg,
                        ConvertColorToImVec4(theme.editor_background));
  ImGui::BeginChild(
      "##SNES_PPU", ImVec2(0, 0), true,
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
                 ImVec2(display_w, display_h), ImVec2(0, 0), ImVec2(1, 1));

    // Allow clicking on the display to ensure focus
    // Modern emulators make the game area "sticky" for input
    if (ImGui::IsItemHovered()) {
      // ImGui::SetTooltip("Click to ensure game input focus");

      // Visual feedback when hovered (subtle border)
      ImDrawList* draw_list = ImGui::GetWindowDrawList();
      ImVec2 screen_pos = ImGui::GetItemRectMin();
      ImVec2 screen_size = ImGui::GetItemRectMax();
      draw_list->AddRect(
          screen_pos, screen_size,
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
    ImGui::SetCursorPosX(
        (canvas_size.x - ImGui::CalcTextSize("512x480 SNES output").x) * 0.5f);
    ImGui::TextColored(ConvertColorToImVec4(theme.text_disabled),
                       "512x480 SNES output");
  }

  ImGui::EndChild();
  ImGui::PopStyleColor();
}

void RenderPerformanceMonitor(Emulator* emu) {
  if (!emu)
    return;

  auto& theme_manager = ThemeManager::Get();
  const auto& theme = theme_manager.GetCurrentTheme();

  ImGui::PushStyleColor(ImGuiCol_ChildBg, ConvertColorToImVec4(theme.child_bg));
  ImGui::BeginChild("##Performance", ImVec2(0, 0), true);

  ImGui::TextColored(ConvertColorToImVec4(theme.accent),
                     ICON_MD_SPEED " Performance Monitor");
  AddSectionSpacing();

  auto metrics = emu->GetMetrics();

  // FPS Graph
  if (ImGui::CollapsingHeader(ICON_MD_SHOW_CHART " Frame Rate",
                              ImGuiTreeNodeFlags_DefaultOpen)) {
    ImGui::Text("Current: %.2f FPS", metrics.fps);
    ImGui::Text("Target: %.2f FPS",
                emu->snes().memory().pal_timing() ? 50.0 : 60.0);

    const float target_ms =
        emu->snes().memory().pal_timing() ? 1000.0f / 50.0f : 1000.0f / 60.0f;
    auto frame_ms = emu->FrameTimeHistory();
    auto fps_history = emu->FpsHistory();
    if (!frame_ms.empty()) {
      plotting::PlotStyleScope plot_style(theme);
      plotting::PlotConfig config{
          .id = "Frame Times",
          .y_label = "ms",
          .flags = ImPlotFlags_NoLegend,
          .x_axis_flags = ImPlotAxisFlags_NoTickLabels |
                          ImPlotAxisFlags_NoGridLines |
                          ImPlotAxisFlags_NoTickMarks,
          .y_axis_flags = ImPlotAxisFlags_AutoFit};
      plotting::PlotGuard plot(config);
      if (plot) {
        ImPlot::SetupAxisLimits(ImAxis_Y1, 0.0f, target_ms * 2.5f,
                                ImGuiCond_Always);
        ImPlot::PlotLine("Frame ms", frame_ms.data(),
                         static_cast<int>(frame_ms.size()));
        ImPlot::PlotInfLines("Target", &target_ms, 1);
      }
    }

    if (!fps_history.empty()) {
      plotting::PlotStyleScope plot_style(theme);
      plotting::PlotConfig fps_config{
          .id = "FPS History",
          .y_label = "fps",
          .flags = ImPlotFlags_NoLegend,
          .x_axis_flags = ImPlotAxisFlags_NoTickLabels |
                          ImPlotAxisFlags_NoGridLines |
                          ImPlotAxisFlags_NoTickMarks,
          .y_axis_flags = ImPlotAxisFlags_AutoFit};
      plotting::PlotGuard plot(fps_config);
      if (plot) {
        ImPlot::SetupAxisLimits(ImAxis_Y1, 0.0f, 75.0f, ImGuiCond_Always);
        ImPlot::PlotLine("FPS", fps_history.data(),
                         static_cast<int>(fps_history.size()));
        const float target_fps = emu->snes().memory().pal_timing() ? 50.0f
                                                                   : 60.0f;
        ImPlot::PlotInfLines("Target", &target_fps, 1);
      }
    }
  }

  if (ImGui::CollapsingHeader(ICON_MD_DATA_USAGE " DMA / VRAM Activity",
                              ImGuiTreeNodeFlags_DefaultOpen)) {
    auto dma_hist = emu->DmaBytesHistory();
    auto vram_hist = emu->VramBytesHistory();
    if (!dma_hist.empty() || !vram_hist.empty()) {
      plotting::PlotStyleScope plot_style(theme);
      plotting::PlotConfig dma_config{
          .id = "DMA/VRAM Bytes",
          .y_label = "bytes/frame",
          .flags = ImPlotFlags_NoLegend,
          .x_axis_flags = ImPlotAxisFlags_NoTickLabels |
                          ImPlotAxisFlags_NoGridLines |
                          ImPlotAxisFlags_NoTickMarks,
          .y_axis_flags = ImPlotAxisFlags_AutoFit};
      plotting::PlotGuard plot(dma_config);
      if (plot) {
        // Calculate max_val before any plotting to avoid locking setup
        float max_val = 512.0f;
        if (!dma_hist.empty()) {
          max_val = std::max(max_val,
                             *std::max_element(dma_hist.begin(), dma_hist.end()));
        }
        if (!vram_hist.empty()) {
          max_val = std::max(max_val, *std::max_element(vram_hist.begin(),
                                                        vram_hist.end()));
        }
        // Setup must be called before any PlotX functions
        ImPlot::SetupAxisLimits(ImAxis_Y1, 0.0f, max_val * 1.2f,
                                ImGuiCond_Always);
        // Now do the plotting
        if (!dma_hist.empty()) {
          ImPlot::PlotLine("DMA", dma_hist.data(),
                           static_cast<int>(dma_hist.size()));
        }
        if (!vram_hist.empty()) {
          ImPlot::PlotLine("VRAM", vram_hist.data(),
                           static_cast<int>(vram_hist.size()));
        }
      }
    } else {
      ImGui::TextDisabled("No DMA activity recorded yet");
    }
  }

  if (ImGui::CollapsingHeader(ICON_MD_STORAGE " ROM Free Space",
                              ImGuiTreeNodeFlags_DefaultOpen)) {
    auto free_bytes = emu->RomBankFreeBytes();
    if (!free_bytes.empty()) {
      plotting::PlotStyleScope plot_style(theme);
      plotting::PlotConfig free_config{
          .id = "ROM Free Bytes",
          .y_label = "bytes (0xFF)",
          .flags = ImPlotFlags_NoLegend | ImPlotFlags_NoBoxSelect,
          .x_axis_flags = ImPlotAxisFlags_AutoFit,
          .y_axis_flags = ImPlotAxisFlags_AutoFit};
      plotting::PlotGuard plot(free_config);
      if (plot) {
        std::vector<double> x(free_bytes.size());
        std::vector<double> y(free_bytes.size());
        for (size_t i = 0; i < free_bytes.size(); ++i) {
          x[i] = static_cast<double>(i);
          y[i] = static_cast<double>(free_bytes[i]);
        }
        ImPlot::PlotBars("Free", x.data(), y.data(),
                         static_cast<int>(free_bytes.size()), 0.67, 0.0,
                         ImPlotBarsFlags_None);
      }
    } else {
      ImGui::TextDisabled("Load a ROM to analyze free space.");
    }
  }

  // CPU Stats
  if (ImGui::CollapsingHeader(ICON_MD_MEMORY " CPU Status",
                              ImGuiTreeNodeFlags_DefaultOpen)) {
    ImGui::Text("PC: $%02X:%04X", metrics.cpu_pb, metrics.cpu_pc);
    ImGui::Text("Cycles: %llu", metrics.cycles);
  }

  // Audio Stats
  if (ImGui::CollapsingHeader(ICON_MD_AUDIOTRACK " Audio Status",
                              ImGuiTreeNodeFlags_DefaultOpen)) {
    if (emu->audio_backend()) {
      auto audio_status = emu->audio_backend()->GetStatus();
      ImGui::Text("Backend: %s",
                  emu->audio_backend()->GetBackendName().c_str());
      ImGui::Text("Queued: %u frames", audio_status.queued_frames);
      ImGui::Text("Playing: %s", audio_status.is_playing ? "YES" : "NO");

      auto audio_history = emu->AudioQueueHistory();
      if (!audio_history.empty()) {
        plotting::PlotStyleScope plot_style(theme);
        plotting::PlotConfig audio_config{
            .id = "Audio Queue Depth",
            .y_label = "frames",
            .flags = ImPlotFlags_NoLegend,
            .x_axis_flags = ImPlotAxisFlags_NoTickLabels |
                            ImPlotAxisFlags_NoGridLines |
                            ImPlotAxisFlags_NoTickMarks,
            .y_axis_flags = ImPlotAxisFlags_AutoFit};
        plotting::PlotGuard plot(audio_config);
        if (plot) {
          ImPlot::SetupAxisLimits(ImAxis_Y1, 0.0f,
                                  std::max(512.0f,
                                           *std::max_element(audio_history.begin(),
                                                             audio_history.end()) *
                                               1.2f),
                                  ImGuiCond_Always);
          ImPlot::PlotLine("Queued", audio_history.data(),
                           static_cast<int>(audio_history.size()));
        }
      }

      auto audio_l = emu->AudioRmsLeftHistory();
      auto audio_r = emu->AudioRmsRightHistory();
      if (!audio_l.empty() || !audio_r.empty()) {
        plotting::PlotStyleScope plot_style(theme);
        plotting::PlotConfig audio_level_config{
            .id = "Audio Levels (RMS)",
            .y_label = "normalized",
            .flags = ImPlotFlags_NoLegend,
            .x_axis_flags = ImPlotAxisFlags_NoTickLabels |
                            ImPlotAxisFlags_NoGridLines |
                            ImPlotAxisFlags_NoTickMarks,
            .y_axis_flags = ImPlotAxisFlags_AutoFit};
        plotting::PlotGuard plot(audio_level_config);
        if (plot) {
          ImPlot::SetupAxisLimits(ImAxis_Y1, 0.0f, 1.0f, ImGuiCond_Always);
          if (!audio_l.empty()) {
            ImPlot::PlotLine("L", audio_l.data(),
                             static_cast<int>(audio_l.size()));
          }
          if (!audio_r.empty()) {
            ImPlot::PlotLine("R", audio_r.data(),
                             static_cast<int>(audio_r.size()));
          }
        }
      }
    } else {
      ImGui::TextColored(ConvertColorToImVec4(theme.error), "No audio backend");
    }
  }

  ImGui::EndChild();
  ImGui::PopStyleColor();
}

void RenderKeyboardShortcuts(bool* show) {
  if (!show || !*show)
    return;

  auto& theme_manager = ThemeManager::Get();
  const auto& theme = theme_manager.GetCurrentTheme();

  // Center the window
  ImVec2 center = ImGui::GetMainViewport()->GetCenter();
  ImGui::SetNextWindowPos(center, ImGuiCond_Appearing, ImVec2(0.5f, 0.5f));
  ImGui::SetNextWindowSize(ImVec2(550, 600), ImGuiCond_Appearing);

  ImGui::PushStyleColor(ImGuiCol_TitleBg, ConvertColorToImVec4(theme.accent));
  ImGui::PushStyleColor(ImGuiCol_TitleBgActive,
                        ConvertColorToImVec4(theme.accent));

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
    ImGui::TextColored(ConvertColorToImVec4(theme.info), ICON_MD_INFO " Tips");
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
  if (!emu)
    return;

  auto& theme_manager = ThemeManager::Get();
  const auto& theme = theme_manager.GetCurrentTheme();

  // Main layout
  ImGui::PushStyleColor(ImGuiCol_ChildBg,
                        ConvertColorToImVec4(theme.window_bg));

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

  // Tab key: Hold for turbo mode
  // Use SDL directly to bypass ImGui's keyboard navigation capture
  // Use SDL directly to bypass ImGui's keyboard navigation capture
  platform::KeyboardState keyboard_state = SDL_GetKeyboardState(nullptr);
  bool tab_pressed = platform::IsKeyPressed(keyboard_state, SDL_SCANCODE_TAB);
  emu->set_turbo_mode(tab_pressed);

  ImGui::PopStyleColor();
}

void RenderVirtualController(Emulator* emu) {
  if (!emu)
    return;

  auto& theme_manager = ThemeManager::Get();
  const auto& theme = theme_manager.GetCurrentTheme();

  ImGui::PushStyleColor(ImGuiCol_ChildBg, ConvertColorToImVec4(theme.child_bg));
  ImGui::BeginChild("##VirtualController", ImVec2(0, 0), true);

  ImGui::TextColored(ConvertColorToImVec4(theme.accent),
                     ICON_MD_SPORTS_ESPORTS " Virtual Controller");
  ImGui::SameLine();
  ImGui::TextColored(ConvertColorToImVec4(theme.text_disabled),
                     "(Click to test input)");
  ImGui::Separator();
  ImGui::Spacing();

  auto& input_mgr = emu->input_manager();

  // Track which buttons are currently pressed via virtual controller
  // Use a static to persist state across frames
  static uint16_t virtual_buttons_pressed = 0;

  // Helper lambda for controller buttons - press on mouse down, release on up
  auto ControllerButton = [&](const char* label, input::SnesButton button,
                               ImVec2 size = ImVec2(50, 40)) {
    ImGui::PushID(static_cast<int>(button));

    uint16_t button_mask = 1 << static_cast<uint8_t>(button);
    bool was_pressed = (virtual_buttons_pressed & button_mask) != 0;

    // Style the button if it's currently pressed
    if (was_pressed) {
      ImGui::PushStyleColor(ImGuiCol_Button,
                            ConvertColorToImVec4(theme.accent));
    }

    // Render the button
    ImGui::Button(label, size);

    // Check if mouse is held down on THIS button (after rendering)
    bool is_active = ImGui::IsItemActive();

    // Update virtual button state
    if (is_active) {
      virtual_buttons_pressed |= button_mask;
      input_mgr.PressButton(button);
    } else if (was_pressed) {
      // Only release if we were the ones who pressed it
      virtual_buttons_pressed &= ~button_mask;
      input_mgr.ReleaseButton(button);
    }

    if (was_pressed) {
      ImGui::PopStyleColor();
    }

    ImGui::PopID();
  };

  // D-Pad layout
  ImGui::Text("D-Pad:");
  ImGui::Indent();

  // Up
  ImGui::SetCursorPosX(ImGui::GetCursorPosX() + 55);
  ControllerButton(ICON_MD_ARROW_UPWARD, input::SnesButton::UP);

  // Left, (space), Right
  ControllerButton(ICON_MD_ARROW_BACK, input::SnesButton::LEFT);
  ImGui::SameLine();
  ImGui::Dummy(ImVec2(50, 40));
  ImGui::SameLine();
  ControllerButton(ICON_MD_ARROW_FORWARD, input::SnesButton::RIGHT);

  // Down
  ImGui::SetCursorPosX(ImGui::GetCursorPosX() + 55);
  ControllerButton(ICON_MD_ARROW_DOWNWARD, input::SnesButton::DOWN);

  ImGui::Unindent();
  ImGui::Spacing();

  // Face buttons (SNES layout: Y B on left, X A on right)
  ImGui::Text("Face Buttons:");
  ImGui::Indent();

  // Top row: X
  ImGui::SetCursorPosX(ImGui::GetCursorPosX() + 55);
  ControllerButton("X", input::SnesButton::X);

  // Middle row: Y, A
  ControllerButton("Y", input::SnesButton::Y);
  ImGui::SameLine();
  ImGui::Dummy(ImVec2(50, 40));
  ImGui::SameLine();
  ControllerButton("A", input::SnesButton::A);

  // Bottom row: B
  ImGui::SetCursorPosX(ImGui::GetCursorPosX() + 55);
  ControllerButton("B", input::SnesButton::B);

  ImGui::Unindent();
  ImGui::Spacing();

  // Shoulder buttons
  ImGui::Text("Shoulder:");
  ControllerButton("L", input::SnesButton::L, ImVec2(70, 30));
  ImGui::SameLine();
  ControllerButton("R", input::SnesButton::R, ImVec2(70, 30));

  ImGui::Spacing();

  // Start/Select
  ImGui::Text("Start/Select:");
  ControllerButton("Select", input::SnesButton::SELECT, ImVec2(70, 30));
  ImGui::SameLine();
  ControllerButton("Start", input::SnesButton::START, ImVec2(70, 30));

  ImGui::Spacing();
  ImGui::Separator();

  // Debug info - show current button state
  ImGui::TextColored(ConvertColorToImVec4(theme.text_disabled), "Debug:");
  uint16_t input_state = emu->snes().GetInput1State();
  ImGui::Text("current_state: 0x%04X", input_state);
  ImGui::Text("virtual_pressed: 0x%04X", virtual_buttons_pressed);
  ImGui::Text("Input registered: %s", input_state != 0 ? "YES" : "NO");

  // Show which buttons are detected
  if (input_state != 0) {
    ImGui::Text("Buttons:");
    if (input_state & 0x0001) ImGui::SameLine(), ImGui::Text("B");
    if (input_state & 0x0002) ImGui::SameLine(), ImGui::Text("Y");
    if (input_state & 0x0004) ImGui::SameLine(), ImGui::Text("Sel");
    if (input_state & 0x0008) ImGui::SameLine(), ImGui::Text("Sta");
    if (input_state & 0x0010) ImGui::SameLine(), ImGui::Text("Up");
    if (input_state & 0x0020) ImGui::SameLine(), ImGui::Text("Dn");
    if (input_state & 0x0040) ImGui::SameLine(), ImGui::Text("Lt");
    if (input_state & 0x0080) ImGui::SameLine(), ImGui::Text("Rt");
    if (input_state & 0x0100) ImGui::SameLine(), ImGui::Text("A");
    if (input_state & 0x0200) ImGui::SameLine(), ImGui::Text("X");
    if (input_state & 0x0400) ImGui::SameLine(), ImGui::Text("L");
    if (input_state & 0x0800) ImGui::SameLine(), ImGui::Text("R");
  }

  ImGui::Spacing();

  // Show what the game actually reads ($4218/$4219)
  auto& snes = emu->snes();
  uint16_t port_read = snes.GetPortAutoRead(0);
  ImGui::Text("port_auto_read[0]: 0x%04X", port_read);
  ImGui::Text("auto_joy_read: %s", snes.IsAutoJoyReadEnabled() ? "ON" : "OFF");

  // Show $4218/$4219 values (what game reads)
  uint8_t reg_4218 = port_read & 0xFF;        // Low byte
  uint8_t reg_4219 = (port_read >> 8) & 0xFF; // High byte
  ImGui::Text("$4218: 0x%02X  $4219: 0x%02X", reg_4218, reg_4219);

  // Decode $4218 (A, X, L, R in bits 7-4, unused 3-0)
  ImGui::Text("$4218 bits: A=%d X=%d L=%d R=%d",
              (reg_4218 >> 7) & 1, (reg_4218 >> 6) & 1,
              (reg_4218 >> 5) & 1, (reg_4218 >> 4) & 1);

  // Edge detection debug - track state changes
  static uint16_t last_port_read = 0;
  static int frames_a_pressed = 0;
  static int frames_since_release = 0;
  static bool detected_edge = false;

  bool a_now = (port_read & 0x0080) != 0;
  bool a_before = (last_port_read & 0x0080) != 0;

  if (a_now && !a_before) {
    detected_edge = true;
    frames_a_pressed = 1;
    frames_since_release = 0;
  } else if (a_now) {
    frames_a_pressed++;
    detected_edge = false;
  } else {
    frames_a_pressed = 0;
    frames_since_release++;
    detected_edge = false;
  }

  last_port_read = port_read;

  ImGui::Spacing();
  ImGui::TextColored(ImVec4(1.0f, 1.0f, 0.0f, 1.0f), "Edge Detection:");
  ImGui::Text("A held for: %d frames", frames_a_pressed);
  ImGui::Text("Released for: %d frames", frames_since_release);
  if (detected_edge) {
    ImGui::TextColored(ImVec4(0.0f, 1.0f, 0.0f, 1.0f), ">>> EDGE DETECTED <<<");
  }

  ImGui::EndChild();
  ImGui::PopStyleColor();
}

}  // namespace ui
}  // namespace emu
}  // namespace yaze
