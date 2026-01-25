#include "app/editor/agent/oracle_ram_panel.h"

#include "app/emu/mesen/mesen_client_registry.h"
#include "app/gui/core/icons.h"
#include "imgui/imgui.h"

namespace yaze {
namespace editor {

namespace {
// Poll interval in seconds (30Hz)
constexpr double kRefreshInterval = 1.0 / 30.0;
}

OracleRamPanel::OracleRamPanel() {
  InitializeVariables();
}

std::string OracleRamPanel::GetIcon() const {
  return ICON_MD_MEMORY;
}

void OracleRamPanel::OnOpen() {
  RefreshVariables();
}

void OracleRamPanel::InitializeVariables() {
  // Key state variables
  variables_ = {
    {0x7E0010, "MODE", "Main game mode", 1},
    {0x7E0011, "SUBMODE", "Sub-mode of current mode", 1},
    {0x7E001B, "INDOORS", "Indoors/Outdoors flag", 1},
    {0x7E00A0, "ROOM", "Current Underworld room ID", 2},
    {0x7E008A, "OWSCR", "Current Overworld screen ID", 1},
    {0x7E002F, "DIR", "Link facing direction", 1},
    {0x7E005D, "LINKDO", "Link state machine ID", 1},
    {0x7E031F, "IFRAMES", "Link invincibility timer", 1},
    
    // Custom OoS Variables
    {0x7E0739, "GoldstarOrHookshot", "0=Hookshot, 1=Goldstar", 1},
    {0x7E0746, "DBG_REINIT_FLAGS", "Debug reinit trigger", 1},
    
    // SRAM Progression (Shadowed in WRAM or read via Mesen)
    {0x7EF3D6, "OOSPROG", "Major quest milestones", 1},
    {0x7EF39C, "JournalState", "Current journal progression", 1},
    {0x7EF410, "Dreams", "Bitfield for Courage/Power/Wisdom", 1},
  };
}

void OracleRamPanel::RefreshVariables() {
  auto client = emu::mesen::MesenClientRegistry::GetOrCreate();
  if (!client || !client->IsConnected()) return;

  // Ideally we'd use a BATCH read, but for now we'll do individual reads 
  // or a large block if they are contiguous.
  // For simplicity in the prototype, we just loop.
  for (auto& var : variables_) {
    auto result = client->ReadBlock(var.address, var.size);
    if (result.ok()) {
      const auto& data = *result;
      if (data.size() < var.size) {
        continue;
      }
      if (var.size == 1) {
        var.last_value = data[0];
      } else {
        var.last_value = data[0] | (data[1] << 8);
      }
    }
  }
}

void OracleRamPanel::Draw(bool* p_open) {
  // Check refresh timer
  double current_time = ImGui::GetTime();
  if (auto_refresh_ && (current_time - last_refresh_time_ >= kRefreshInterval)) {
    RefreshVariables();
    last_refresh_time_ = current_time;
  }

  // Toolbar
  if (ImGui::Button(auto_refresh_ ? ICON_MD_PAUSE : ICON_MD_PLAY_ARROW)) {
    auto_refresh_ = !auto_refresh_;
  }
  ImGui::SameLine();
  if (ImGui::Button(ICON_MD_REFRESH)) {
    RefreshVariables();
  }
  ImGui::SameLine();
  ImGui::TextDisabled("Refreshed: %.1fs ago", current_time - last_refresh_time_);

  ImGui::Separator();

  DrawVariableTable();
}

void OracleRamPanel::DrawVariableTable() {
  static ImGuiTableFlags flags = ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg | 
                                 ImGuiTableFlags_Resizable | ImGuiTableFlags_Reorderable |
                                 ImGuiTableFlags_Hideable;

  if (ImGui::BeginTable("OracleRamVariables", 4, flags)) {
    ImGui::TableSetupColumn("Address", ImGuiTableColumnFlags_WidthFixed, 80.0f);
    ImGui::TableSetupColumn("Label", ImGuiTableColumnFlags_WidthFixed, 120.0f);
    ImGui::TableSetupColumn("Value", ImGuiTableColumnFlags_WidthFixed, 60.0f);
    ImGui::TableSetupColumn("Description", ImGuiTableColumnFlags_WidthStretch);
    ImGui::TableHeadersRow();

    for (const auto& var : variables_) {
      ImGui::TableNextRow();
      
      // Address
      ImGui::TableSetColumnIndex(0);
      ImGui::Text("$%06X", var.address);

      // Label
      ImGui::TableSetColumnIndex(1);
      ImGui::Text("%s", var.label.c_str());

      // Value
      ImGui::TableSetColumnIndex(2);
      if (var.size == 1) {
          ImGui::Text("$%02X", var.last_value);
      } else {
          ImGui::Text("$%04X", var.last_value);
      }

      // Description
      ImGui::TableSetColumnIndex(3);
      ImGui::TextDisabled("%s", var.description.c_str());
    }
    ImGui::EndTable();
  }
}

}  // namespace editor
}  // namespace yaze
