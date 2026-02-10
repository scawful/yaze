#include "app/editor/agent/panels/agent_rom_sync_panel.h"

#include <string>

#include "absl/strings/str_format.h"
#include "absl/time/clock.h"
#include "absl/time/time.h"
#include "app/editor/ui/toast_manager.h"
#include "app/gui/core/icons.h"
#include "app/gui/core/style_guard.h"
#include "imgui/imgui.h"

namespace yaze {
namespace editor {

void AgentRomSyncPanel::Draw(AgentUIContext* context,
                             const RomSyncCallbacks& callbacks,
                             ToastManager* toast_manager) {
  auto& state = context->rom_sync_state();
  auto& collab_state = context->collaboration_state();

  gui::StyleColorGuard rom_bg(ImGuiCol_ChildBg,
                              ImVec4(0.18f, 0.14f, 0.12f, 1.0f));
  ImGui::BeginChild("RomSync", ImVec2(0, 130), true);

  ImGui::Text(ICON_MD_STORAGE " ROM State");
  ImGui::Separator();

  // Display current ROM hash
  if (!state.current_rom_hash.empty()) {
    ImGui::Text("Hash: %s", state.current_rom_hash.substr(0, 16).c_str());
    ImGui::SameLine();
    if (ImGui::SmallButton(ICON_MD_CONTENT_COPY)) {
      ImGui::SetClipboardText(state.current_rom_hash.c_str());
      if (toast_manager) {
        toast_manager->Show("ROM hash copied", ToastType::kInfo, 2.0f);
      }
    }
  } else {
    ImGui::TextDisabled("No ROM loaded");
  }

  if (state.last_sync_time != absl::InfinitePast()) {
    ImGui::Text("Last Sync: %s",
                absl::FormatTime("%H:%M:%S", state.last_sync_time,
                                 absl::LocalTimeZone())
                    .c_str());
  }

  ImGui::Spacing();
  ImGui::Checkbox("Auto-sync ROM changes", &state.auto_sync_enabled);

  if (state.auto_sync_enabled) {
    ImGui::SliderInt("Sync Interval (seconds)", &state.sync_interval_seconds,
                     10, 120);
  }

  ImGui::Spacing();
  ImGui::Separator();

  bool can_sync = static_cast<bool>(callbacks.generate_rom_diff) &&
                  collab_state.active &&
                  collab_state.mode == CollaborationMode::kNetwork;

  if (!can_sync)
    ImGui::BeginDisabled();

  if (ImGui::Button(ICON_MD_CLOUD_UPLOAD " Send ROM Sync", ImVec2(-1, 0))) {
    if (callbacks.generate_rom_diff) {
      auto diff_result = callbacks.generate_rom_diff();
      if (diff_result.ok()) {
        std::string hash =
            callbacks.get_rom_hash ? callbacks.get_rom_hash() : "";

        state.current_rom_hash = hash;
        state.last_sync_time = absl::Now();

        // TODO: Send via network coordinator (handled by caller usually)
        if (toast_manager) {
          toast_manager->Show(ICON_MD_CLOUD_DONE " ROM synced to collaborators",
                              ToastType::kSuccess, 3.0f);
        }
      } else if (toast_manager) {
        toast_manager->Show(absl::StrFormat(ICON_MD_ERROR " Sync failed: %s",
                                            diff_result.status().message()),
                            ToastType::kError, 5.0f);
      }
    }
  }

  if (!can_sync) {
    ImGui::EndDisabled();
    if (ImGui::IsItemHovered()) {
      ImGui::SetTooltip("Connect to a network session to sync ROM");
    }
  }

  // Show pending syncs
  if (!state.pending_syncs.empty()) {
    ImGui::Spacing();
    ImGui::Text(ICON_MD_PENDING " Pending Syncs (%zu)",
                state.pending_syncs.size());
    ImGui::Separator();

    ImGui::BeginChild("PendingSyncs", ImVec2(0, 80), true);
    for (const auto& sync : state.pending_syncs) {
      ImGui::BulletText("%s", sync.substr(0, 40).c_str());
    }
    ImGui::EndChild();
  }

  ImGui::EndChild();
}

}  // namespace editor
}  // namespace yaze
