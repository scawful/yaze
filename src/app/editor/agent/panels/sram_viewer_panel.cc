#include "app/editor/agent/panels/sram_viewer_panel.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstring>

#include "absl/strings/str_format.h"
#include "app/editor/agent/agent_ui_theme.h"
#include "app/emu/mesen/mesen_client_registry.h"
#include "app/gui/core/icons.h"
#include "core/project.h"
#include "imgui/imgui.h"

namespace yaze {
namespace editor {

namespace {

// Address range boundaries for grouping
constexpr uint32_t kStoryRangeStart = 0x7EF3C5;
constexpr uint32_t kStoryRangeEnd = 0x7EF3D8;
constexpr uint32_t kDungeonCrystals = 0x7EF37A;
constexpr uint32_t kDungeonPendants = 0x7EF374;
constexpr uint32_t kItemsRangeStart = 0x7EF340;
constexpr uint32_t kItemsRangeEnd = 0x7EF380;

// Crystal bitfield labels (bit index -> dungeon name)
struct CrystalBit {
  uint8_t mask;
  const char* label;
};
constexpr CrystalBit kCrystalBits[] = {
    {0x01, "D1 Mushroom Grotto"},
    {0x02, "D6 Goron Mines"},
    {0x04, "D5 Glacia Estate"},
    {0x08, "D7 Dragon Ship"},
    {0x10, "D2 Tail Palace"},
    {0x20, "D4 Zora Temple"},
    {0x40, "D3 Kalyxo Castle"},
};

// GameState enum labels
const char* kGameStateLabels[] = {
    "0: Start",
    "1: LoomBeach",
    "2: KydrogComplete",
    "3: FaroreRescued",
};
constexpr int kGameStateLabelCount = 4;

// How long the yellow highlight lasts after a value changes (seconds)
constexpr float kChangeHighlightDuration = 2.0f;

bool IsInStoryRange(uint32_t addr) {
  return addr >= kStoryRangeStart && addr < kStoryRangeEnd;
}

bool IsDungeonAddress(uint32_t addr) {
  return addr == kDungeonCrystals || addr == kDungeonPendants;
}

bool IsInItemsRange(uint32_t addr) {
  return addr >= kItemsRangeStart && addr < kItemsRangeEnd;
}

}  // namespace

SramViewerPanel::SramViewerPanel() {
  client_ = emu::mesen::MesenClientRegistry::GetOrCreate();
  RefreshSocketList();
  if (!socket_paths_.empty()) {
    selected_socket_index_ = 0;
    std::snprintf(socket_path_buffer_, sizeof(socket_path_buffer_), "%s",
                  socket_paths_[0].c_str());
  }
}

SramViewerPanel::~SramViewerPanel() = default;

bool SramViewerPanel::IsConnected() const {
  return client_ && client_->IsConnected();
}

void SramViewerPanel::Connect() {
  if (!client_) {
    client_ = emu::mesen::MesenClientRegistry::GetOrCreate();
  }
  auto status = client_->Connect();
  if (!status.ok()) {
    connection_error_ = std::string(status.message());
  } else {
    connection_error_.clear();
    emu::mesen::MesenClientRegistry::SetClient(client_);
    RefreshValues();
  }
}

void SramViewerPanel::ConnectToPath(const std::string& socket_path) {
  if (!client_) {
    client_ = emu::mesen::MesenClientRegistry::GetOrCreate();
  }
  auto status = client_->Connect(socket_path);
  if (!status.ok()) {
    connection_error_ = std::string(status.message());
  } else {
    connection_error_.clear();
    emu::mesen::MesenClientRegistry::SetClient(client_);
    RefreshValues();
  }
}

void SramViewerPanel::Disconnect() {
  if (client_) {
    client_->Disconnect();
  }
}

void SramViewerPanel::RefreshSocketList() {
  socket_paths_ = emu::mesen::MesenSocketClient::ListAvailableSockets();
  if (!socket_paths_.empty()) {
    if (selected_socket_index_ < 0 ||
        selected_socket_index_ >= static_cast<int>(socket_paths_.size())) {
      selected_socket_index_ = 0;
    }
    if (socket_path_buffer_[0] == '\0') {
      std::snprintf(socket_path_buffer_, sizeof(socket_path_buffer_), "%s",
                    socket_paths_[selected_socket_index_].c_str());
    }
  } else {
    selected_socket_index_ = -1;
  }
}

void SramViewerPanel::LoadVariablesFromManifest() {
  variables_.clear();
  variables_loaded_ = false;

  if (!project_) return;
  if (!project_->hack_manifest.loaded()) return;

  variables_ = project_->hack_manifest.sram_variables();
  variables_loaded_ = true;

  // Sort by address for consistent display
  std::sort(variables_.begin(), variables_.end(),
            [](const core::SramVariable& a, const core::SramVariable& b) {
              return a.address < b.address;
            });
}

void SramViewerPanel::RefreshValues() {
  if (!IsConnected()) return;
  if (variables_.empty()) return;

  // Save previous values for change detection
  previous_values_ = current_values_;

  // Read each variable individually (they may be scattered across WRAM)
  float current_time = static_cast<float>(ImGui::GetTime());
  for (const auto& var : variables_) {
    auto result = client_->ReadByte(var.address);
    if (result.ok()) {
      uint8_t new_value = *result;

      // Detect changes
      auto prev_it = previous_values_.find(var.address);
      if (prev_it != previous_values_.end() && prev_it->second != new_value) {
        change_timestamps_[var.address] = current_time;
      }

      current_values_[var.address] = new_value;
    }
  }
}

void SramViewerPanel::PokeValue(uint32_t address, uint8_t value) {
  if (!IsConnected()) return;

  auto status = client_->WriteByte(address, value);
  if (!status.ok()) {
    status_message_ =
        absl::StrFormat("Write failed: %s", status.message());
  } else {
    status_message_ =
        absl::StrFormat("Wrote $%02X to $%06X", value, address);
    // Update cache immediately
    current_values_[address] = value;
  }
}

void SramViewerPanel::Draw() {
  ImGui::PushID("SramViewerPanel");

  // Load variables from manifest on first draw (or when project changes)
  if (!variables_loaded_ && project_) {
    LoadVariablesFromManifest();
  }

  // Auto-refresh logic
  if (IsConnected() && auto_refresh_) {
    time_since_refresh_ += ImGui::GetIO().DeltaTime;
    if (time_since_refresh_ >= refresh_interval_) {
      RefreshValues();
      time_since_refresh_ = 0.0f;
    }
  }

  AgentUI::PushPanelStyle();
  if (ImGui::BeginChild("SramViewer_Panel", ImVec2(0, 0), true)) {
    if (ImGui::IsWindowAppearing()) {
      RefreshSocketList();
      if (project_) {
        LoadVariablesFromManifest();
      }
    }
    DrawConnectionHeader();

    if (IsConnected()) {
      ImGui::Spacing();

      if (variables_.empty()) {
        ImGui::TextDisabled(
            "No SRAM variables loaded. Ensure hack_manifest.json is present "
            "in the project.");
      } else {
        // Filter
        ImGui::TextDisabled("Filter");
        ImGui::SameLine();
        ImGui::SetNextItemWidth(-1);
        ImGui::InputTextWithHint("##sram_filter", "Search by name or address",
                                 filter_text_, sizeof(filter_text_));
        ImGui::Spacing();

        DrawVariableTable();
      }
    } else if (!variables_.empty()) {
      // Show variables even when disconnected (no values)
      ImGui::Spacing();
      ImGui::TextDisabled("Connect to Mesen2 to see live values.");
    }
  }
  ImGui::EndChild();
  AgentUI::PopPanelStyle();

  ImGui::PopID();
}

void SramViewerPanel::DrawConnectionHeader() {
  const auto& theme = AgentUI::GetTheme();

  ImGui::TextColored(theme.accent_color, "%s SRAM Viewer",
                     ICON_MD_MEMORY);

  // Connection status indicator
  ImGui::SameLine(ImGui::GetWindowWidth() - 100);
  if (IsConnected()) {
    float pulse = 0.7f + 0.3f * std::sin(ImGui::GetTime() * 2.0f);
    ImVec4 connected_color = ImVec4(0.1f, pulse, 0.3f, 1.0f);
    ImGui::TextColored(connected_color, "%s Connected", ICON_MD_CHECK_CIRCLE);
  } else {
    ImGui::TextColored(theme.status_error, "%s Disconnected", ICON_MD_ERROR);
  }

  ImGui::Separator();

  if (!IsConnected()) {
    ImGui::TextDisabled("Socket");
    const char* preview =
        (selected_socket_index_ >= 0 &&
         selected_socket_index_ < static_cast<int>(socket_paths_.size()))
            ? socket_paths_[selected_socket_index_].c_str()
            : "No sockets found";
    ImGui::SetNextItemWidth(-40);
    if (ImGui::BeginCombo("##sram_socket_combo", preview)) {
      for (int i = 0; i < static_cast<int>(socket_paths_.size()); ++i) {
        bool selected = (i == selected_socket_index_);
        if (ImGui::Selectable(socket_paths_[i].c_str(), selected)) {
          selected_socket_index_ = i;
          std::snprintf(socket_path_buffer_, sizeof(socket_path_buffer_), "%s",
                        socket_paths_[i].c_str());
        }
        if (selected) {
          ImGui::SetItemDefaultFocus();
        }
      }
      ImGui::EndCombo();
    }
    ImGui::SameLine();
    if (ImGui::SmallButton(ICON_MD_REFRESH "##sram_refresh")) {
      RefreshSocketList();
    }

    ImGui::TextDisabled("Path");
    ImGui::SetNextItemWidth(-1);
    ImGui::InputTextWithHint("##sram_socket_path", "/tmp/mesen2-12345.sock",
                             socket_path_buffer_, sizeof(socket_path_buffer_));

    if (ImGui::Button(ICON_MD_LINK " Connect")) {
      std::string path = socket_path_buffer_;
      if (path.empty() && selected_socket_index_ >= 0 &&
          selected_socket_index_ < static_cast<int>(socket_paths_.size())) {
        path = socket_paths_[selected_socket_index_];
      }
      if (path.empty()) {
        Connect();
      } else {
        ConnectToPath(path);
      }
    }
    ImGui::SameLine();
    if (ImGui::SmallButton(ICON_MD_AUTO_MODE " Auto")) {
      Connect();
    }
    if (!connection_error_.empty()) {
      ImGui::Spacing();
      ImGui::TextColored(theme.status_error, "%s", connection_error_.c_str());
    }
  } else {
    if (ImGui::Button(ICON_MD_LINK_OFF " Disconnect")) {
      Disconnect();
    }
    ImGui::SameLine();
    ImGui::Checkbox("Auto-refresh", &auto_refresh_);
    if (auto_refresh_) {
      ImGui::SameLine();
      ImGui::SetNextItemWidth(60);
      ImGui::SliderFloat("##SramRefreshRate", &refresh_interval_, 0.05f, 1.0f,
                         "%.2fs");
    } else {
      ImGui::SameLine();
      if (ImGui::SmallButton(ICON_MD_REFRESH " Refresh")) {
        RefreshValues();
      }
    }
  }

  if (!status_message_.empty()) {
    ImGui::Spacing();
    ImGui::TextColored(theme.text_secondary_color, "%s",
                       status_message_.c_str());
  }
}

void SramViewerPanel::DrawVariableTable() {
  // Partition variables into groups
  std::vector<const core::SramVariable*> story_vars;
  std::vector<const core::SramVariable*> dungeon_vars;
  std::vector<const core::SramVariable*> item_vars;
  std::vector<const core::SramVariable*> other_vars;

  std::string filter_lower;
  if (filter_text_[0] != '\0') {
    filter_lower = filter_text_;
    std::transform(filter_lower.begin(), filter_lower.end(),
                   filter_lower.begin(), ::tolower);
  }

  for (const auto& var : variables_) {
    // Apply filter
    if (!filter_lower.empty()) {
      std::string name_lower = var.name;
      std::transform(name_lower.begin(), name_lower.end(), name_lower.begin(),
                     ::tolower);
      std::string purpose_lower = var.purpose;
      std::transform(purpose_lower.begin(), purpose_lower.end(),
                     purpose_lower.begin(), ::tolower);
      std::string addr_str = absl::StrFormat("$%06X", var.address);
      std::string addr_lower = addr_str;
      std::transform(addr_lower.begin(), addr_lower.end(), addr_lower.begin(),
                     ::tolower);

      if (name_lower.find(filter_lower) == std::string::npos &&
          purpose_lower.find(filter_lower) == std::string::npos &&
          addr_lower.find(filter_lower) == std::string::npos) {
        continue;
      }
    }

    if (IsInStoryRange(var.address)) {
      story_vars.push_back(&var);
    } else if (IsDungeonAddress(var.address)) {
      dungeon_vars.push_back(&var);
    } else if (IsInItemsRange(var.address)) {
      item_vars.push_back(&var);
    } else {
      other_vars.push_back(&var);
    }
  }

  // Story section
  if (!story_vars.empty()) {
    std::string story_label =
        absl::StrFormat("%s Story (%zu)", ICON_MD_AUTO_STORIES,
                        story_vars.size());
    if (ImGui::CollapsingHeader(
            story_label.c_str(),
            story_expanded_ ? ImGuiTreeNodeFlags_DefaultOpen : 0)) {
      story_expanded_ = true;
      for (const auto* var : story_vars) {
        DrawVariableRow(*var);
      }
    } else {
      story_expanded_ = false;
    }
  }

  // Dungeon section
  if (!dungeon_vars.empty()) {
    std::string dungeon_label =
        absl::StrFormat("%s Dungeon (%zu)", ICON_MD_CASTLE,
                        dungeon_vars.size());
    if (ImGui::CollapsingHeader(
            dungeon_label.c_str(),
            dungeon_expanded_ ? ImGuiTreeNodeFlags_DefaultOpen : 0)) {
      dungeon_expanded_ = true;
      for (const auto* var : dungeon_vars) {
        DrawVariableRow(*var);
      }
    } else {
      dungeon_expanded_ = false;
    }
  }

  // Items section
  if (!item_vars.empty()) {
    std::string items_label =
        absl::StrFormat("%s Items (%zu)", ICON_MD_INVENTORY_2,
                        item_vars.size());
    if (ImGui::CollapsingHeader(
            items_label.c_str(),
            items_expanded_ ? ImGuiTreeNodeFlags_DefaultOpen : 0)) {
      items_expanded_ = true;
      for (const auto* var : item_vars) {
        DrawVariableRow(*var);
      }
    } else {
      items_expanded_ = false;
    }
  }

  // Other/uncategorized
  if (!other_vars.empty()) {
    std::string other_label =
        absl::StrFormat("%s Other (%zu)", ICON_MD_MORE_HORIZ,
                        other_vars.size());
    if (ImGui::CollapsingHeader(other_label.c_str())) {
      for (const auto* var : other_vars) {
        DrawVariableRow(*var);
      }
    }
  }
}

void SramViewerPanel::DrawVariableRow(const core::SramVariable& var) {
  const auto& theme = AgentUI::GetTheme();
  float current_time = static_cast<float>(ImGui::GetTime());

  ImGui::PushID(static_cast<int>(var.address));

  // Check if this value recently changed
  bool recently_changed = false;
  auto ts_it = change_timestamps_.find(var.address);
  if (ts_it != change_timestamps_.end()) {
    float elapsed = current_time - ts_it->second;
    if (elapsed < kChangeHighlightDuration) {
      recently_changed = true;
      // Flash yellow background that fades out
      float alpha = 1.0f - (elapsed / kChangeHighlightDuration);
      ImVec4 highlight_color = ImVec4(0.9f, 0.8f, 0.1f, alpha * 0.3f);
      ImVec2 cursor_pos = ImGui::GetCursorScreenPos();
      ImVec2 row_size = ImVec2(ImGui::GetContentRegionAvail().x,
                               ImGui::GetTextLineHeightWithSpacing());
      ImGui::GetWindowDrawList()->AddRectFilled(
          cursor_pos, ImVec2(cursor_pos.x + row_size.x,
                             cursor_pos.y + row_size.y),
          ImGui::ColorConvertFloat4ToU32(highlight_color));
    }
  }

  // Address column
  ImGui::TextColored(theme.text_secondary_color, "$%06X", var.address);

  // Name column
  ImGui::SameLine(80);
  if (recently_changed) {
    ImGui::TextColored(ImVec4(0.9f, 0.9f, 0.2f, 1.0f), "%s",
                       var.name.c_str());
  } else {
    ImGui::Text("%s", var.name.c_str());
  }

  // Purpose (tooltip)
  if (!var.purpose.empty() && ImGui::IsItemHovered()) {
    ImGui::SetTooltip("%s", var.purpose.c_str());
  }

  // Value columns (only when connected and we have data)
  auto val_it = current_values_.find(var.address);
  if (val_it != current_values_.end()) {
    uint8_t value = val_it->second;

    // Decimal value
    ImGui::SameLine(240);
    ImGui::Text("%d", value);

    // Hex value
    ImGui::SameLine(290);
    ImGui::TextColored(theme.text_secondary_color, "$%02X", value);

    // Edit button
    ImGui::SameLine(340);
    std::string edit_label = absl::StrFormat(
        "%s##edit_%06X", ICON_MD_EDIT, var.address);
    if (ImGui::SmallButton(edit_label.c_str())) {
      editing_active_ = true;
      editing_address_ = var.address;
      editing_value_ = value;
      ImGui::OpenPopup("SramEditPopup");
    }
  }

  // Special expansions for key addresses
  if (val_it != current_values_.end()) {
    if (var.address == kDungeonCrystals) {
      DrawCrystalBitfield(val_it->second, var.address);
    } else if (var.address == kStoryRangeStart) {
      // GameState is the first story address ($7EF3C5)
      DrawGameStateDropdown(val_it->second, var.address);
    }
  }

  ImGui::PopID();
}

void SramViewerPanel::DrawCrystalBitfield(uint8_t value, uint32_t address) {
  ImGui::Indent(20);
  bool any_changed = false;
  uint8_t new_value = value;

  for (const auto& bit : kCrystalBits) {
    bool set = (value & bit.mask) != 0;
    std::string cb_label = absl::StrFormat("%s##crystal_%02X", bit.label,
                                          bit.mask);
    if (ImGui::Checkbox(cb_label.c_str(), &set)) {
      if (set) {
        new_value |= bit.mask;
      } else {
        new_value &= ~bit.mask;
      }
      any_changed = true;
    }
  }

  if (any_changed) {
    PokeValue(address, new_value);
  }

  ImGui::Unindent(20);
}

void SramViewerPanel::DrawGameStateDropdown(uint8_t value, uint32_t address) {
  ImGui::Indent(20);

  int current_index = value;
  if (current_index >= kGameStateLabelCount) {
    current_index = -1;  // Unknown state
  }

  const char* preview = (current_index >= 0 && current_index < kGameStateLabelCount)
                            ? kGameStateLabels[current_index]
                            : "Unknown";

  ImGui::SetNextItemWidth(200);
  if (ImGui::BeginCombo("##gamestate_combo", preview)) {
    for (int i = 0; i < kGameStateLabelCount; ++i) {
      bool selected = (i == current_index);
      if (ImGui::Selectable(kGameStateLabels[i], selected)) {
        PokeValue(address, static_cast<uint8_t>(i));
      }
      if (selected) {
        ImGui::SetItemDefaultFocus();
      }
    }
    ImGui::EndCombo();
  }

  ImGui::Unindent(20);
}

}  // namespace editor
}  // namespace yaze
