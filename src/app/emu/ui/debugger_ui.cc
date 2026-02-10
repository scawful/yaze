#include "app/emu/ui/debugger_ui.h"

#include "absl/strings/str_format.h"
#include "app/emu/cpu/cpu.h"
#include "app/emu/emulator.h"
#include "app/gui/core/color.h"
#include "app/gui/core/icons.h"
#include "app/gui/core/input.h"
#include "app/gui/core/style_guard.h"
#include "app/gui/core/theme_manager.h"
#include "imgui/imgui.h"
#include "app/gui/imgui_memory_editor.h"
#include "util/log.h"

namespace yaze {
namespace emu {
namespace ui {

using namespace yaze::gui;

namespace {
// UI Constants
constexpr float kStandardSpacing = 8.0f;
constexpr float kButtonHeight = 30.0f;
constexpr float kLargeButtonHeight = 35.0f;

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

void RenderModernCpuDebugger(Emulator* emu) {
  if (!emu)
    return;

  auto& theme_manager = ThemeManager::Get();
  const auto& theme = theme_manager.GetCurrentTheme();

  gui::StyledChild cpu_child("##CPUDebugger", ImVec2(0, 0),
                             {.bg = ConvertColorToImVec4(theme.child_bg)},
                             true);

  // Title with icon
  ImGui::TextColored(ConvertColorToImVec4(theme.accent),
                     ICON_MD_DEVELOPER_BOARD " 65816 CPU Debugger");
  AddSectionSpacing();

  auto& cpu = emu->snes().cpu();

  // Debugger Controls
  if (ImGui::CollapsingHeader(ICON_MD_SETTINGS " Controls",
                              ImGuiTreeNodeFlags_DefaultOpen)) {
    gui::StyleVarGuard frame_padding(ImGuiStyleVar_FramePadding, ImVec2(8, 6));

    if (ImGui::Button(ICON_MD_SKIP_NEXT " Step", ImVec2(100, kButtonHeight))) {
      cpu.RunOpcode();
    }
    if (ImGui::IsItemHovered()) {
      ImGui::SetTooltip("Execute single instruction (F10)");
    }

    ImGui::SameLine();
    if (ImGui::Button(ICON_MD_FAST_FORWARD " Run to BP",
                      ImVec2(120, kButtonHeight))) {
      // Run until breakpoint
      emu->set_running(true);
    }
    if (ImGui::IsItemHovered()) {
      ImGui::SetTooltip("Run until next breakpoint (F5)");
    }
  }

  AddSpacing();

  // CPU Registers
  if (ImGui::CollapsingHeader(ICON_MD_MEMORY " Registers",
                              ImGuiTreeNodeFlags_DefaultOpen)) {
    if (ImGui::BeginTable("CPU_Registers", 4,
                          ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg)) {
      ImGui::TableSetupColumn("Reg", ImGuiTableColumnFlags_WidthFixed, 40.0f);
      ImGui::TableSetupColumn("Value", ImGuiTableColumnFlags_WidthFixed, 70.0f);
      ImGui::TableSetupColumn("Reg", ImGuiTableColumnFlags_WidthFixed, 40.0f);
      ImGui::TableSetupColumn("Value", ImGuiTableColumnFlags_WidthFixed, 70.0f);
      ImGui::TableHeadersRow();

      // Row 1: A, X
      ImGui::TableNextRow();
      ImGui::TableNextColumn();
      ImGui::TextColored(ConvertColorToImVec4(theme.text_secondary), "A:");
      ImGui::TableNextColumn();
      ImGui::TextColored(ConvertColorToImVec4(theme.accent), "$%04X", cpu.A);
      ImGui::TableNextColumn();
      ImGui::TextColored(ConvertColorToImVec4(theme.text_secondary), "X:");
      ImGui::TableNextColumn();
      ImGui::TextColored(ConvertColorToImVec4(theme.accent), "$%04X", cpu.X);

      // Row 2: Y, D
      ImGui::TableNextRow();
      ImGui::TableNextColumn();
      ImGui::TextColored(ConvertColorToImVec4(theme.text_secondary), "Y:");
      ImGui::TableNextColumn();
      ImGui::TextColored(ConvertColorToImVec4(theme.accent), "$%04X", cpu.Y);
      ImGui::TableNextColumn();
      ImGui::TextColored(ConvertColorToImVec4(theme.text_secondary), "D:");
      ImGui::TableNextColumn();
      ImGui::TextColored(ConvertColorToImVec4(theme.accent), "$%04X", cpu.D);

      // Row 3: DB, PB
      ImGui::TableNextRow();
      ImGui::TableNextColumn();
      ImGui::TextColored(ConvertColorToImVec4(theme.text_secondary), "DB:");
      ImGui::TableNextColumn();
      ImGui::TextColored(ConvertColorToImVec4(theme.accent), "$%02X", cpu.DB);
      ImGui::TableNextColumn();
      ImGui::TextColored(ConvertColorToImVec4(theme.text_secondary), "PB:");
      ImGui::TableNextColumn();
      ImGui::TextColored(ConvertColorToImVec4(theme.accent), "$%02X", cpu.PB);

      // Row 4: PC, SP
      ImGui::TableNextRow();
      ImGui::TableNextColumn();
      ImGui::TextColored(ConvertColorToImVec4(theme.text_secondary), "PC:");
      ImGui::TableNextColumn();
      ImGui::TextColored(ConvertColorToImVec4(theme.warning), "$%04X", cpu.PC);
      ImGui::TableNextColumn();
      ImGui::TextColored(ConvertColorToImVec4(theme.text_secondary), "SP:");
      ImGui::TableNextColumn();
      ImGui::TextColored(ConvertColorToImVec4(theme.accent), "$%04X", cpu.SP());

      ImGui::EndTable();
    }

    AddSpacing();

    // Status Flags (visual checkboxes)
    ImGui::TextColored(ConvertColorToImVec4(theme.text_secondary),
                       ICON_MD_FLAG " Flags:");
    ImGui::Indent();

    auto RenderFlag = [&](const char* name, bool value) {
      ImVec4 color = value ? ConvertColorToImVec4(theme.success)
                           : ConvertColorToImVec4(theme.text_disabled);
      ImGui::TextColored(
          color, "%s %s",
          value ? ICON_MD_CHECK_BOX : ICON_MD_CHECK_BOX_OUTLINE_BLANK, name);
      if (ImGui::IsItemHovered()) {
        ImGui::SetTooltip("%s: %s", name, value ? "Set" : "Clear");
      }
      ImGui::SameLine();
    };

    RenderFlag("N", cpu.GetNegativeFlag());
    RenderFlag("V", cpu.GetOverflowFlag());
    RenderFlag("D", cpu.GetDecimalFlag());
    RenderFlag("I", cpu.GetInterruptFlag());
    RenderFlag("Z", cpu.GetZeroFlag());
    RenderFlag("C", cpu.GetCarryFlag());
    ImGui::NewLine();

    ImGui::Unindent();
  }

  AddSpacing();

  // Breakpoint Management
  if (ImGui::CollapsingHeader(ICON_MD_STOP_CIRCLE " Breakpoints")) {
    static char bp_input[10] = "";

    ImGui::SetNextItemWidth(150);
    if (ImGui::InputTextWithHint("##BP", "Address (hex)", bp_input,
                                 sizeof(bp_input),
                                 ImGuiInputTextFlags_CharsHexadecimal |
                                     ImGuiInputTextFlags_EnterReturnsTrue)) {
      if (strlen(bp_input) > 0) {
        uint32_t addr = std::stoi(bp_input, nullptr, 16);
        emu->SetBreakpoint(addr);
        memset(bp_input, 0, sizeof(bp_input));
      }
    }

    ImGui::SameLine();
    if (ImGui::Button(ICON_MD_ADD " Add", ImVec2(80, 0))) {
      if (strlen(bp_input) > 0) {
        uint32_t addr = std::stoi(bp_input, nullptr, 16);
        emu->SetBreakpoint(addr);
        memset(bp_input, 0, sizeof(bp_input));
      }
    }

    ImGui::SameLine();
    if (ImGui::Button(ICON_MD_CLEAR_ALL " Clear All", ImVec2(100, 0))) {
      emu->ClearAllBreakpoints();
    }

    AddSpacing();

    // List breakpoints
    auto breakpoints = emu->GetBreakpoints();
    if (!breakpoints.empty()) {
      ImGui::BeginChild("##BPList", ImVec2(0, 150), true);
      for (size_t i = 0; i < breakpoints.size(); ++i) {
        uint32_t bp = breakpoints[i];
        ImGui::PushID(i);

        ImGui::TextColored(ConvertColorToImVec4(theme.accent),
                           ICON_MD_STOP " $%06X", bp);

        ImGui::SameLine(200);
        if (ImGui::SmallButton(ICON_MD_DELETE " Remove")) {
          cpu.ClearBreakpoint(bp);
        }

        ImGui::PopID();
      }
      ImGui::EndChild();
    } else {
      ImGui::TextColored(ConvertColorToImVec4(theme.text_disabled),
                         "No breakpoints set");
    }
  }

}

void RenderBreakpointList(Emulator* emu) {
  if (!emu)
    return;

  auto& theme_manager = ThemeManager::Get();
  const auto& theme = theme_manager.GetCurrentTheme();

  gui::StyledChild bp_child("##BreakpointList", ImVec2(0, 0),
                            {.bg = ConvertColorToImVec4(theme.child_bg)}, true);

  ImGui::TextColored(ConvertColorToImVec4(theme.accent),
                     ICON_MD_STOP_CIRCLE " Breakpoint Manager");
  AddSectionSpacing();

  // Same content as in RenderModernCpuDebugger but with more detail
  auto breakpoints = emu->GetBreakpoints();

  ImGui::Text("Active Breakpoints: %zu", breakpoints.size());
  AddSpacing();

  if (!breakpoints.empty()) {
    if (ImGui::BeginTable("BreakpointTable", 3,
                          ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg)) {
      ImGui::TableSetupColumn(ICON_MD_TAG, ImGuiTableColumnFlags_WidthFixed,
                              40);
      ImGui::TableSetupColumn("Address", ImGuiTableColumnFlags_WidthFixed, 100);
      ImGui::TableSetupColumn("Actions", ImGuiTableColumnFlags_WidthStretch);
      ImGui::TableHeadersRow();

      for (size_t i = 0; i < breakpoints.size(); ++i) {
        ImGui::TableNextRow();
        ImGui::TableNextColumn();
        ImGui::TextColored(ConvertColorToImVec4(theme.error), ICON_MD_STOP);

        ImGui::TableNextColumn();
        ImGui::TextColored(ConvertColorToImVec4(theme.accent), "$%06X",
                           breakpoints[i]);

        ImGui::TableNextColumn();
        ImGui::PushID(i);
        if (ImGui::SmallButton(ICON_MD_DELETE " Remove")) {
          emu->snes().cpu().ClearBreakpoint(breakpoints[i]);
        }
        ImGui::PopID();
      }

      ImGui::EndTable();
    }
  } else {
    ImGui::TextColored(ConvertColorToImVec4(theme.text_disabled),
                       ICON_MD_INFO " No breakpoints set");
  }

}

void RenderMemoryViewer(Emulator* emu) {
  if (!emu)
    return;

  auto& theme_manager = ThemeManager::Get();
  const auto& theme = theme_manager.GetCurrentTheme();

  static yaze::gui::MemoryEditorWidget mem_edit;

  gui::StyledChild mem_child("##MemoryViewer", ImVec2(0, 0),
                             {.bg = ConvertColorToImVec4(theme.child_bg)},
                             true);

  ImGui::TextColored(ConvertColorToImVec4(theme.accent),
                     ICON_MD_STORAGE " Memory Viewer");
  AddSectionSpacing();

  // Memory region selector
  static int region = 0;
  const char* regions[] = {"RAM ($0000-$1FFF)", "ROM Bank 0",
                           "WRAM ($7E0000-$7FFFFF)", "SRAM"};

  ImGui::SetNextItemWidth(250);
  if (ImGui::Combo(ICON_MD_MAP " Region", &region, regions,
                   IM_ARRAYSIZE(regions))) {
    // Region changed
  }

  AddSpacing();

  // Render memory editor
  uint8_t* memory_base = emu->snes().get_ram();
  size_t memory_size = 0x20000;

  mem_edit.DrawContents(memory_base, memory_size, 0x0000);

}

void RenderCpuInstructionLog(Emulator* emu, uint32_t log_size) {
  if (!emu)
    return;

  auto& theme_manager = ThemeManager::Get();
  const auto& theme = theme_manager.GetCurrentTheme();

  ImGui::PushStyleColor(ImGuiCol_ChildBg, ConvertColorToImVec4(theme.child_bg));
  ImGui::BeginChild("##InstructionLog", ImVec2(0, 0), true);

  ImGui::TextColored(ConvertColorToImVec4(theme.warning),
                     ICON_MD_WARNING " Legacy Instruction Log");
  ImGui::TextColored(ConvertColorToImVec4(theme.text_disabled),
                     "Deprecated - Use Disassembly Viewer instead");
  AddSectionSpacing();

  // Show DisassemblyViewer stats instead
  ImGui::Text(ICON_MD_INFO " DisassemblyViewer Active:");
  ImGui::BulletText("Unique addresses: %zu",
                    emu->disassembly_viewer().GetInstructionCount());
  ImGui::BulletText("Recording: %s",
                    emu->disassembly_viewer().IsRecording() ? "ON" : "OFF");
  ImGui::BulletText("Auto-scroll: Available in viewer");

  AddSpacing();

  if (ImGui::Button(ICON_MD_OPEN_IN_NEW " Open Disassembly Viewer",
                    ImVec2(-1, kLargeButtonHeight))) {
    // TODO: Open disassembly viewer window
  }

  ImGui::EndChild();
  ImGui::PopStyleColor();
}

void RenderApuDebugger(Emulator* emu) {
  if (!emu)
    return;

  auto& theme_manager = ThemeManager::Get();
  const auto& theme = theme_manager.GetCurrentTheme();

  ImGui::PushStyleColor(ImGuiCol_ChildBg, ConvertColorToImVec4(theme.child_bg));
  ImGui::BeginChild("##ApuDebugger", ImVec2(0, 0), true);

  // Title
  ImGui::TextColored(ConvertColorToImVec4(theme.accent),
                     ICON_MD_MUSIC_NOTE " APU / SPC700 Debugger");
  AddSectionSpacing();

  auto& tracker = emu->snes().apu_handshake_tracker();

  // Handshake Status with enhanced visuals
  if (ImGui::CollapsingHeader(ICON_MD_HANDSHAKE " Handshake Status",
                              ImGuiTreeNodeFlags_DefaultOpen)) {
    // Phase with icon and color
    auto phase_str = tracker.GetPhaseString();
    ImVec4 phase_color;
    const char* phase_icon;

    if (phase_str == "RUNNING") {
      phase_color = ConvertColorToImVec4(theme.success);
      phase_icon = ICON_MD_CHECK_CIRCLE;
    } else if (phase_str == "TRANSFER_ACTIVE") {
      phase_color = ConvertColorToImVec4(theme.info);
      phase_icon = ICON_MD_SYNC;
    } else if (phase_str == "WAITING_BBAA" || phase_str == "IPL_BOOT") {
      phase_color = ConvertColorToImVec4(theme.warning);
      phase_icon = ICON_MD_PENDING;
    } else {
      phase_color = ConvertColorToImVec4(theme.error);
      phase_icon = ICON_MD_ERROR;
    }

    ImGui::Text(ICON_MD_SETTINGS " Phase:");
    ImGui::SameLine();
    ImGui::TextColored(phase_color, "%s %s", phase_icon, phase_str.c_str());

    // Handshake complete indicator
    ImGui::Text(ICON_MD_LINK " Handshake:");
    ImGui::SameLine();
    if (tracker.IsHandshakeComplete()) {
      ImGui::TextColored(ConvertColorToImVec4(theme.success),
                         ICON_MD_CHECK_CIRCLE " Complete");
    } else {
      ImGui::TextColored(ConvertColorToImVec4(theme.warning),
                         ICON_MD_HOURGLASS_EMPTY " Waiting");
    }

    // Transfer progress
    if (tracker.IsTransferActive() || tracker.GetBytesTransferred() > 0) {
      AddSpacing();
      ImGui::Text(ICON_MD_CLOUD_UPLOAD " Transfer Progress:");
      ImGui::Indent();
      ImGui::BulletText("Bytes: %d", tracker.GetBytesTransferred());
      ImGui::BulletText("Blocks: %d", tracker.GetBlockCount());

      auto progress = tracker.GetTransferProgress();
      if (!progress.empty()) {
        ImGui::TextColored(ConvertColorToImVec4(theme.info), "%s",
                           progress.c_str());
      }
      ImGui::Unindent();
    }

    // Status summary
    AddSectionSpacing();
    ImGui::TextWrapped("%s", tracker.GetStatusSummary().c_str());
  }

  // Port Activity Log
  if (ImGui::CollapsingHeader(ICON_MD_LIST " Port Activity Log",
                              ImGuiTreeNodeFlags_DefaultOpen)) {
    ImGui::BeginChild("##PortLog", ImVec2(0, 200), true);

    const auto& history = tracker.GetPortHistory();

    if (history.empty()) {
      ImGui::TextColored(ConvertColorToImVec4(theme.text_disabled),
                         ICON_MD_INFO " No port activity yet");
    } else {
      // Show last 50 entries
      int start_idx = std::max(0, static_cast<int>(history.size()) - 50);
      for (size_t i = start_idx; i < history.size(); ++i) {
        const auto& entry = history[i];

        ImVec4 color = entry.is_cpu ? ConvertColorToImVec4(theme.accent)
                                    : ConvertColorToImVec4(theme.info);
        const char* icon =
            entry.is_cpu ? ICON_MD_ARROW_FORWARD : ICON_MD_ARROW_BACK;

        ImGui::TextColored(color, "[%04llu] %s %s F%d = $%02X @ PC=$%04X %s",
                           entry.timestamp, entry.is_cpu ? "CPU" : "SPC", icon,
                           entry.port + 4, entry.value, entry.pc,
                           entry.description.c_str());
      }

      // Auto-scroll
      if (ImGui::GetScrollY() >= ImGui::GetScrollMaxY()) {
        ImGui::SetScrollHereY(1.0f);
      }
    }

    ImGui::EndChild();
  }

  // Current Port Values
  if (ImGui::CollapsingHeader(ICON_MD_SETTINGS_INPUT_COMPONENT
                              " Current Port Values",
                              ImGuiTreeNodeFlags_DefaultOpen)) {
    if (ImGui::BeginTable("APU_Ports", 4,
                          ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg)) {
      ImGui::TableSetupColumn("Port", ImGuiTableColumnFlags_WidthFixed, 50);
      ImGui::TableSetupColumn("CPU → SPC", ImGuiTableColumnFlags_WidthFixed,
                              80);
      ImGui::TableSetupColumn("SPC → CPU", ImGuiTableColumnFlags_WidthFixed,
                              80);
      ImGui::TableSetupColumn("Address", ImGuiTableColumnFlags_WidthStretch);
      ImGui::TableHeadersRow();

      for (int i = 0; i < 4; ++i) {
        ImGui::TableNextRow();
        ImGui::TableNextColumn();
        ImGui::Text(ICON_MD_SETTINGS " F%d", i + 4);

        ImGui::TableNextColumn();
        ImGui::TextColored(ConvertColorToImVec4(theme.accent), "$%02X",
                           emu->snes().apu().in_ports_[i]);

        ImGui::TableNextColumn();
        ImGui::TextColored(ConvertColorToImVec4(theme.info), "$%02X",
                           emu->snes().apu().out_ports_[i]);

        ImGui::TableNextColumn();
        ImGui::TextDisabled("$214%d / $F%d", i, i + 4);
      }

      ImGui::EndTable();
    }
  }

  // Quick Actions
  if (ImGui::CollapsingHeader(ICON_MD_BUILD " Quick Actions",
                              ImGuiTreeNodeFlags_DefaultOpen)) {
    ImGui::TextColored(ConvertColorToImVec4(theme.warning),
                       ICON_MD_WARNING " Manual Testing Tools");
    AddSpacing();

    // Full handshake test
    if (ImGui::Button(ICON_MD_PLAY_CIRCLE " Full Handshake Test",
                      ImVec2(-1, kLargeButtonHeight))) {
      LOG_INFO("APU_DEBUG", "=== MANUAL HANDSHAKE TEST ===");
      emu->snes().Write(0x002140, 0xCC);
      emu->snes().Write(0x002141, 0x01);
      emu->snes().Write(0x002142, 0x00);
      emu->snes().Write(0x002143, 0x02);
      LOG_INFO("APU_DEBUG", "Handshake sequence executed");
    }
    if (ImGui::IsItemHovered()) {
      ImGui::SetTooltip(
          "Execute full handshake sequence:\n"
          "$CC → F4, $01 → F5, $00 → F6, $02 → F7");
    }

    AddSpacing();

    // Manual port writes
    if (ImGui::TreeNode(ICON_MD_EDIT " Manual Port Writes")) {
      static uint8_t port_values[4] = {0xCC, 0x01, 0x00, 0x02};

      for (int i = 0; i < 4; ++i) {
        ImGui::PushID(i);
        ImGui::Text("F%d ($214%d):", i + 4, i);
        ImGui::SameLine();
        ImGui::SetNextItemWidth(80);
        ImGui::InputScalar("##val", ImGuiDataType_U8, &port_values[i], NULL,
                           NULL, "%02X", ImGuiInputTextFlags_CharsHexadecimal);
        ImGui::SameLine();
        if (ImGui::Button(ICON_MD_SEND " Write", ImVec2(100, 0))) {
          emu->snes().Write(0x002140 + i, port_values[i]);
          LOG_INFO("APU_DEBUG", "Wrote $%02X to F%d", port_values[i], i + 4);
        }
        ImGui::PopID();
      }

      ImGui::TreePop();
    }

    AddSectionSpacing();

    // System controls
    if (ImGui::Button(ICON_MD_RESTART_ALT " Reset APU",
                      ImVec2(-1, kButtonHeight))) {
      emu->snes().apu().Reset();
      LOG_INFO("APU_DEBUG", "APU reset");
    }

    if (ImGui::Button(ICON_MD_CLEAR_ALL " Clear Port History",
                      ImVec2(-1, kButtonHeight))) {
      tracker.Reset();
      LOG_INFO("APU_DEBUG", "Port history cleared");
    }
  }

  ImGui::Separator();
  ImGui::Text("Audio Resampling");

  // Combo box for interpolation type
  const char* items[] = {"Linear", "Hermite", "Cosine", "Cubic"};
  int current_item =
      static_cast<int>(emu->snes().apu().dsp().interpolation_type);
  if (ImGui::Combo("Interpolation", &current_item, items,
                   IM_ARRAYSIZE(items))) {
    emu->snes().apu().dsp().interpolation_type =
        static_cast<InterpolationType>(current_item);
  }

  ImGui::EndChild();
  ImGui::PopStyleColor();
}

void RenderAIAgentPanel(Emulator* emu) {
  if (!emu)
    return;

  auto& theme_manager = ThemeManager::Get();
  const auto& theme = theme_manager.GetCurrentTheme();

  ImGui::PushStyleColor(ImGuiCol_ChildBg, ConvertColorToImVec4(theme.child_bg));
  ImGui::BeginChild("##AIAgent", ImVec2(0, 0), true);

  ImGui::TextColored(ConvertColorToImVec4(theme.accent),
                     ICON_MD_SMART_TOY " AI Agent Integration");
  AddSectionSpacing();

  // Agent status
  bool agent_ready = emu->IsEmulatorReady();
  ImVec4 status_color = agent_ready ? ConvertColorToImVec4(theme.success)
                                    : ConvertColorToImVec4(theme.error);

  ImGui::Text("Status:");
  ImGui::SameLine();
  ImGui::TextColored(status_color, "%s %s",
                     agent_ready ? ICON_MD_CHECK_CIRCLE : ICON_MD_ERROR,
                     agent_ready ? "Ready" : "Not Ready");

  AddSpacing();

  // Emulator metrics for agents
  if (ImGui::CollapsingHeader(ICON_MD_DATA_OBJECT " Metrics",
                              ImGuiTreeNodeFlags_DefaultOpen)) {
    auto metrics = emu->GetMetrics();

    ImGui::BulletText("FPS: %.2f", metrics.fps);
    ImGui::BulletText("Cycles: %llu", metrics.cycles);
    ImGui::BulletText("CPU PC: $%02X:%04X", metrics.cpu_pb, metrics.cpu_pc);
    ImGui::BulletText("Audio Queued: %u frames", metrics.audio_frames_queued);
    ImGui::BulletText("Running: %s", metrics.is_running ? "YES" : "NO");
  }

  // Agent controls
  if (ImGui::CollapsingHeader(ICON_MD_PLAY_CIRCLE " Agent Controls")) {
    if (ImGui::Button(ICON_MD_PLAY_ARROW " Start Agent Session",
                      ImVec2(-1, kLargeButtonHeight))) {
      // TODO: Start agent
    }

    if (ImGui::Button(ICON_MD_STOP " Stop Agent", ImVec2(-1, kButtonHeight))) {
      // TODO: Stop agent
    }
  }

  ImGui::EndChild();
  ImGui::PopStyleColor();
}

}  // namespace ui
}  // namespace emu
}  // namespace yaze
