#include "app/editor/overworld/panels/overworld_item_list_panel.h"

#include <algorithm>
#include <cctype>
#include <string>
#include <vector>

#include "absl/strings/str_format.h"
#include "app/editor/core/panel_registration.h"
#include "app/editor/overworld/panels/overworld_panel_access.h"
#include "imgui/imgui.h"
#include "zelda3/overworld/overworld_item.h"

namespace yaze::editor {
namespace {

bool ItemIdentityMatches(const zelda3::OverworldItem& lhs,
                         const zelda3::OverworldItem& rhs) {
  return lhs.id_ == rhs.id_ && lhs.room_map_id_ == rhs.room_map_id_ &&
         lhs.x_ == rhs.x_ && lhs.y_ == rhs.y_ && lhs.game_x_ == rhs.game_x_ &&
         lhs.game_y_ == rhs.game_y_ && lhs.bg2_ == rhs.bg2_;
}

std::string ToLower(std::string value) {
  std::transform(
      value.begin(), value.end(), value.begin(),
      [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
  return value;
}

std::string ItemName(const zelda3::OverworldItem& item) {
  if (item.id_ < zelda3::kSecretItemNames.size()) {
    return zelda3::kSecretItemNames[item.id_];
  }
  return absl::StrFormat("Item 0x%02X", static_cast<int>(item.id_));
}

bool MatchesItemFilter(const zelda3::OverworldItem& item,
                       const std::string& lowered_filter) {
  if (lowered_filter.empty()) {
    return true;
  }

  const std::string name = ToLower(ItemName(item));
  const std::string id_hex =
      absl::StrFormat("%02x", static_cast<int>(item.id_));
  const std::string map_hex =
      absl::StrFormat("%02x", static_cast<int>(item.room_map_id_));
  const std::string pos = absl::StrFormat("%d,%d", item.x_, item.y_);

  return name.find(lowered_filter) != std::string::npos ||
         id_hex.find(lowered_filter) != std::string::npos ||
         map_hex.find(lowered_filter) != std::string::npos ||
         pos.find(lowered_filter) != std::string::npos;
}

}  // namespace

void OverworldItemListPanel::Draw(bool* p_open) {
  (void)p_open;
  auto* ow_editor = CurrentOverworldEditor();
  if (!ow_editor) {
    return;
  }

  auto* items = ow_editor->overworld().mutable_all_items();
  if (!items) {
    ImGui::TextDisabled("No item list available.");
    return;
  }

  static char filter_buffer[64] = "";
  static bool current_world_only = true;
  static bool current_map_only = false;
  static int sort_mode = 0;  // 0=Map+Pos, 1=ID, 2=Name

  ImGui::InputTextWithHint("##ItemFilter", ICON_MD_FILTER_ALT " Filter items",
                           filter_buffer, sizeof(filter_buffer));
  ImGui::SameLine();
  ImGui::Checkbox("World", &current_world_only);
  ImGui::SameLine();
  ImGui::Checkbox("Map", &current_map_only);

  const char* sort_modes[] = {"Map + Position", "Item ID", "Name"};
  ImGui::SetNextItemWidth(180.0f);
  ImGui::Combo("Sort", &sort_mode, sort_modes, IM_ARRAYSIZE(sort_modes));

  const std::string lowered_filter = ToLower(std::string(filter_buffer));
  std::vector<size_t> filtered_indices;
  filtered_indices.reserve(items->size());

  const int current_map = ow_editor->current_map_id();
  const int current_world = ow_editor->current_world_id();

  for (size_t i = 0; i < items->size(); ++i) {
    const auto& item = items->at(i);
    if (item.deleted) {
      continue;
    }
    if (current_world_only && (item.room_map_id_ / 0x40) != current_world) {
      continue;
    }
    if (current_map_only && item.room_map_id_ != current_map) {
      continue;
    }
    if (!MatchesItemFilter(item, lowered_filter)) {
      continue;
    }
    filtered_indices.push_back(i);
  }

  auto sort_by_map_pos = [&](size_t lhs, size_t rhs) {
    const auto& a = items->at(lhs);
    const auto& b = items->at(rhs);
    if (a.room_map_id_ != b.room_map_id_) {
      return a.room_map_id_ < b.room_map_id_;
    }
    if (a.y_ != b.y_) {
      return a.y_ < b.y_;
    }
    if (a.x_ != b.x_) {
      return a.x_ < b.x_;
    }
    return a.id_ < b.id_;
  };
  auto sort_by_id = [&](size_t lhs, size_t rhs) {
    const auto& a = items->at(lhs);
    const auto& b = items->at(rhs);
    if (a.id_ != b.id_) {
      return a.id_ < b.id_;
    }
    return sort_by_map_pos(lhs, rhs);
  };
  auto sort_by_name = [&](size_t lhs, size_t rhs) {
    const auto& a = items->at(lhs);
    const auto& b = items->at(rhs);
    const std::string name_a = ItemName(a);
    const std::string name_b = ItemName(b);
    if (name_a != name_b) {
      return name_a < name_b;
    }
    return sort_by_map_pos(lhs, rhs);
  };

  switch (sort_mode) {
    case 1:
      std::sort(filtered_indices.begin(), filtered_indices.end(), sort_by_id);
      break;
    case 2:
      std::sort(filtered_indices.begin(), filtered_indices.end(), sort_by_name);
      break;
    case 0:
    default:
      std::sort(filtered_indices.begin(), filtered_indices.end(),
                sort_by_map_pos);
      break;
  }

  const zelda3::OverworldItem* selected_item = ow_editor->GetSelectedItem();
  const bool has_selection = selected_item != nullptr;

  if (!has_selection) {
    ImGui::BeginDisabled();
  }
  if (ImGui::Button(ICON_MD_CONTENT_COPY " Duplicate", ImVec2(120.0f, 0.0f))) {
    ow_editor->DuplicateSelectedItem();
    selected_item = ow_editor->GetSelectedItem();
  }
  ImGui::SameLine();
  if (ImGui::Button(ICON_MD_DELETE " Delete", ImVec2(96.0f, 0.0f))) {
    ow_editor->DeleteSelectedItem();
    selected_item = ow_editor->GetSelectedItem();
  }
  if (!has_selection) {
    ImGui::EndDisabled();
  }

  ImGui::SameLine();
  if (selected_item) {
    ImGui::TextDisabled("Selected: 0x%02X @ (%d,%d)",
                        static_cast<int>(selected_item->id_), selected_item->x_,
                        selected_item->y_);
  } else {
    ImGui::TextDisabled("No item selected");
  }

  ImGui::TextDisabled(
      "Shortcuts: Ctrl+D duplicate, arrows nudge, Shift+arrows nudge by 16px");
  ImGui::TextDisabled("Total: %zu | Visible: %zu", items->size(),
                      filtered_indices.size());

  if (ImGui::BeginTable("##OverworldItemListTable", 6,
                        ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg |
                            ImGuiTableFlags_Resizable |
                            ImGuiTableFlags_SizingStretchProp |
                            ImGuiTableFlags_ScrollY,
                        ImVec2(0.0f, 0.0f))) {
    ImGui::TableSetupColumn("", ImGuiTableColumnFlags_WidthFixed, 30.0f);
    ImGui::TableSetupColumn("ID", ImGuiTableColumnFlags_WidthFixed, 42.0f);
    ImGui::TableSetupColumn("Name", ImGuiTableColumnFlags_WidthStretch, 1.0f);
    ImGui::TableSetupColumn("Map", ImGuiTableColumnFlags_WidthFixed, 54.0f);
    ImGui::TableSetupColumn("World XY", ImGuiTableColumnFlags_WidthFixed,
                            118.0f);
    ImGui::TableSetupColumn("Game XY", ImGuiTableColumnFlags_WidthFixed, 86.0f);
    ImGui::TableHeadersRow();

    for (size_t visible_row = 0; visible_row < filtered_indices.size();
         ++visible_row) {
      const size_t index = filtered_indices[visible_row];
      const auto& item = items->at(index);
      const bool is_selected =
          selected_item && ItemIdentityMatches(item, *selected_item);

      ImGui::TableNextRow();

      ImGui::TableNextColumn();
      if (ImGui::SmallButton(
              absl::StrFormat("%s##select_item_%zu",
                              is_selected ? ICON_MD_RADIO_BUTTON_CHECKED
                                          : ICON_MD_RADIO_BUTTON_UNCHECKED,
                              index)
                  .c_str())) {
        ow_editor->SelectItemByIdentity(item);
        selected_item = ow_editor->GetSelectedItem();
      }

      ImGui::TableNextColumn();
      ImGui::Text("0x%02X", static_cast<int>(item.id_));

      ImGui::TableNextColumn();
      ImGui::TextUnformatted(ItemName(item).c_str());

      ImGui::TableNextColumn();
      if (item.room_map_id_ == current_map) {
        ImGui::TextColored(ImVec4(0.45f, 0.85f, 0.50f, 1.0f), "%02X",
                           static_cast<int>(item.room_map_id_));
      } else {
        ImGui::Text("%02X", static_cast<int>(item.room_map_id_));
      }

      ImGui::TableNextColumn();
      ImGui::Text("%4d, %4d", item.x_, item.y_);

      ImGui::TableNextColumn();
      ImGui::Text("%2d, %2d", item.game_x_, item.game_y_);
    }

    ImGui::EndTable();
  }
}

REGISTER_PANEL(OverworldItemListPanel);

}  // namespace yaze::editor
