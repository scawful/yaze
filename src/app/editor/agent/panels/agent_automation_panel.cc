#include "app/editor/agent/panels/agent_automation_panel.h"

#include <cmath>
#include <string>
#include <vector>

#include "absl/strings/str_format.h"
#include "absl/time/clock.h"
#include "absl/time/time.h"
#include "app/editor/agent/agent_ui_theme.h"
#include "app/gui/core/icons.h"
#include "imgui/imgui.h"

namespace yaze {
namespace editor {

void AgentAutomationPanel::Draw(AgentUIContext* context,
                                const AutomationCallbacks& callbacks) {
  const auto& theme = AgentUI::GetTheme();
  auto& state = context->automation_state();

  ImGui::PushID("AutomationPanel");

  // Auto-poll for status updates
  if (callbacks.poll_status) {
    callbacks.poll_status();
  }

  // Animate pulse and scanlines for retro effect
  state.pulse_animation += ImGui::GetIO().DeltaTime * 2.0f;
  state.scanline_offset += ImGui::GetIO().DeltaTime * 0.5f;
  if (state.scanline_offset > 1.0f) {
    state.scanline_offset -= 1.0f;
  }

  AgentUI::PushPanelStyle();
  if (ImGui::BeginChild("Automation_Panel", ImVec2(0, 240), true)) {
    // === HEADER WITH RETRO GLITCH EFFECT ===
    float pulse = 0.5f + 0.5f * std::sin(state.pulse_animation);
    ImVec4 header_glow = ImVec4(theme.provider_ollama.x + 0.3f * pulse,
                                theme.provider_ollama.y + 0.2f * pulse,
                                theme.provider_ollama.z + 0.4f * pulse, 1.0f);

    ImGui::PushStyleColor(ImGuiCol_Text, header_glow);
    ImGui::TextWrapped("%s %s", ICON_MD_SMART_TOY, "GUI AUTOMATION");
    ImGui::PopStyleColor();

    ImGui::SameLine();
    ImGui::TextDisabled("[v0.5.x]");

    // === CONNECTION STATUS WITH VISUAL EFFECTS ===
    bool connected = state.harness_connected;
    ImVec4 status_color;
    const char* status_text;
    const char* status_icon;

    if (connected) {
      // Pulsing green for connected
      float green_pulse = 0.7f + 0.3f * std::sin(state.pulse_animation * 0.5f);
      status_color = ImVec4(0.1f, green_pulse, 0.3f, 1.0f);
      status_text = "ONLINE";
      status_icon = ICON_MD_CHECK_CIRCLE;
    } else {
      // Pulsing red for disconnected
      float red_pulse = 0.6f + 0.4f * std::sin(state.pulse_animation * 1.5f);
      status_color = ImVec4(red_pulse, 0.2f, 0.2f, 1.0f);
      status_text = "OFFLINE";
      status_icon = ICON_MD_ERROR;
    }

    ImGui::Separator();
    ImGui::TextColored(status_color, "%s %s", status_icon, status_text);
    ImGui::SameLine();
    ImGui::TextDisabled("| %s", state.grpc_server_address.c_str());

    // === CONTROL BAR ===
    ImGui::Spacing();

    // Refresh button with pulse effect when auto-refresh is on
    bool auto_ref_pulse =
        state.auto_refresh_enabled &&
        (static_cast<int>(state.pulse_animation * 2.0f) % 2 == 0);
    if (auto_ref_pulse) {
      ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.2f, 0.5f, 0.7f, 0.8f));
    }

    if (ImGui::SmallButton(ICON_MD_REFRESH " Refresh")) {
      if (callbacks.poll_status) {
        callbacks.poll_status();
      }
      if (callbacks.show_active_tests) {
        callbacks.show_active_tests();
      }
    }

    if (auto_ref_pulse) {
      ImGui::PopStyleColor();
    }

    if (ImGui::IsItemHovered()) {
      ImGui::SetTooltip("Refresh automation status\nAuto-refresh: %s (%.1fs)",
                        state.auto_refresh_enabled ? "ON" : "OFF",
                        state.refresh_interval_seconds);
    }

    // Auto-refresh toggle
    ImGui::SameLine();
    ImGui::Checkbox("##auto_refresh", &state.auto_refresh_enabled);
    if (ImGui::IsItemHovered()) {
      ImGui::SetTooltip("Auto-refresh connection status");
    }

    // Quick action buttons
    ImGui::SameLine();
    if (ImGui::SmallButton(ICON_MD_DASHBOARD " Dashboard")) {
      if (callbacks.open_harness_dashboard) {
        callbacks.open_harness_dashboard();
      }
    }
    if (ImGui::IsItemHovered()) {
      ImGui::SetTooltip("Open automation dashboard");
    }

    ImGui::SameLine();
    if (ImGui::SmallButton(ICON_MD_REPLAY " Replay")) {
      if (callbacks.replay_last_plan) {
        callbacks.replay_last_plan();
      }
    }
    if (ImGui::IsItemHovered()) {
      ImGui::SetTooltip("Replay last automation plan");
    }

    // === SETTINGS ROW ===
    ImGui::Spacing();
    ImGui::SetNextItemWidth(80.0f);
    ImGui::SliderFloat("##refresh_interval", &state.refresh_interval_seconds,
                       0.5f, 10.0f, "%.1fs");
    if (ImGui::IsItemHovered()) {
      ImGui::SetTooltip("Auto-refresh interval");
    }

    ImGui::Spacing();
    ImGui::Separator();
    ImGui::TextDisabled("Automation Hooks");
    ImGui::Checkbox("Auto-run harness plan", &state.auto_run_plan);
    ImGui::Checkbox("Auto-sync ROM context", &state.auto_sync_rom);
    ImGui::Checkbox("Auto-focus proposal drawer", &state.auto_focus_proposals);

    // === RECENT AUTOMATION ACTIONS WITH SCROLLING ===
    ImGui::Spacing();
    ImGui::Separator();

    // Header with retro styling
    ImGui::TextColored(ImVec4(0.4f, 0.8f, 1.0f, 1.0f), "%s RECENT ACTIONS",
                       ICON_MD_LIST);
    ImGui::SameLine();
    ImGui::TextDisabled("[%zu]", state.recent_tests.size());

    if (state.recent_tests.empty()) {
      ImGui::Spacing();
      ImGui::TextDisabled("  > No recent actions");
      ImGui::TextDisabled("  > Waiting for automation tasks...");

      // Add animated dots
      int dots = static_cast<int>(state.pulse_animation) % 4;
      std::string dot_string(dots, '.');
      ImGui::TextDisabled("  > %s", dot_string.c_str());
    } else {
      // Scrollable action list with retro styling
      ImGui::BeginChild("ActionQueue", ImVec2(0, 100), true,
                        ImGuiWindowFlags_AlwaysVerticalScrollbar);

      // Add scanline effect (visual only)
      ImDrawList* draw_list = ImGui::GetWindowDrawList();
      ImVec2 win_pos = ImGui::GetWindowPos();
      ImVec2 win_size = ImGui::GetWindowSize();

      // Draw scanlines
      for (float y = 0; y < win_size.y; y += 4.0f) {
        float offset_y = y + state.scanline_offset * 4.0f;
        if (offset_y < win_size.y) {
          draw_list->AddLine(
              ImVec2(win_pos.x, win_pos.y + offset_y),
              ImVec2(win_pos.x + win_size.x, win_pos.y + offset_y),
              IM_COL32(0, 0, 0, 20));
        }
      }

      for (const auto& test : state.recent_tests) {
        ImGui::PushID(test.test_id.c_str());

        // Status icon with animation for running tests
        ImVec4 action_color;
        const char* status_icon;

        if (test.status == "success" || test.status == "completed" ||
            test.status == "passed") {
          action_color = theme.status_success;
          status_icon = ICON_MD_CHECK_CIRCLE;
        } else if (test.status == "running" || test.status == "in_progress") {
          float running_pulse =
              0.5f + 0.5f * std::sin(state.pulse_animation * 3.0f);
          action_color =
              ImVec4(theme.provider_ollama.x * running_pulse,
                     theme.provider_ollama.y * (0.8f + 0.2f * running_pulse),
                     theme.provider_ollama.z * running_pulse, 1.0f);
          status_icon = ICON_MD_PENDING;
        } else if (test.status == "failed" || test.status == "error") {
          action_color = theme.status_error;
          status_icon = ICON_MD_ERROR;
        } else {
          action_color = theme.text_secondary_color;
          status_icon = ICON_MD_HELP;
        }

        // Icon with pulse
        ImGui::TextColored(action_color, "%s", status_icon);
        ImGui::SameLine();

        // Action name with monospace font
        ImGui::Text("> %s", test.name.c_str());

        // Timestamp
        if (test.updated_at != absl::InfinitePast()) {
          ImGui::SameLine();
          auto elapsed = absl::Now() - test.updated_at;
          if (elapsed < absl::Seconds(60)) {
            ImGui::TextColored(ImVec4(0.5f, 0.5f, 0.5f, 1.0f), "[%ds]",
                               static_cast<int>(absl::ToInt64Seconds(elapsed)));
          } else if (elapsed < absl::Minutes(60)) {
            ImGui::TextColored(ImVec4(0.5f, 0.5f, 0.5f, 1.0f), "[%dm]",
                               static_cast<int>(absl::ToInt64Minutes(elapsed)));
          } else {
            ImGui::TextColored(ImVec4(0.5f, 0.5f, 0.5f, 1.0f), "[%dh]",
                               static_cast<int>(absl::ToInt64Hours(elapsed)));
          }
        }

        // Message (if any) with indentation
        if (!test.message.empty()) {
          ImGui::Indent(20.0f);
          ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.7f, 0.7f, 0.7f, 1.0f));
          ImGui::TextWrapped("  %s %s", ICON_MD_MESSAGE, test.message.c_str());
          ImGui::PopStyleColor();
          ImGui::Unindent(20.0f);
        }

        ImGui::PopID();
      }

      ImGui::EndChild();
    }
  }
  ImGui::EndChild();
  AgentUI::PopPanelStyle();
  ImGui::PopID();
}

}  // namespace editor
}  // namespace yaze
