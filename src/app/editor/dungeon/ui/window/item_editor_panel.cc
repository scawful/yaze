#include "app/editor/dungeon/ui/window/item_editor_panel.h"

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <string>

#include "absl/strings/str_format.h"
#include "app/editor/dungeon/inspectors/dungeon_entity_inspector.h"
#include "app/gui/automation/widget_auto_register.h"
#include "app/gui/core/input.h"
#include "app/gui/core/style_guard.h"
#include "util/i18n/tr.h"

namespace yaze {
namespace editor {

void ItemEditorPanel::Draw(bool* /*p_open*/) {
  DrawContent(true);
}

void ItemEditorPanel::DrawInWorkbench() {
  DrawContent(false);
}

void ItemEditorPanel::DrawContent(bool show_room_items) {
  gui::AutoWidgetScope automation_scope("Dungeon/ItemEditor");
  if (!current_room_id_ || !rooms_) {
    ImGui::TextDisabled(tr("No room data available"));
    return;
  }

  if (*current_room_id_ < 0 ||
      *current_room_id_ >= static_cast<int>(rooms_->size())) {
    ImGui::TextDisabled(tr("No room selected"));
    return;
  }

  DrawDungeonPotItemSaveNotice();
  DrawPlacementControls();
  ImGui::Separator();
  DrawItemSelector(show_room_items);
  if (show_room_items) {
    ImGui::Separator();
    DrawRoomItems();
  }
}

bool ItemEditorPanel::HasMatchingViewer() const {
  if (!current_room_id_ || !rooms_ || !canvas_viewer_ ||
      canvas_viewer_->rooms() != rooms_ || *current_room_id_ < 0 ||
      *current_room_id_ >= static_cast<int>(rooms_->size()) ||
      canvas_viewer_->current_room_id() != *current_room_id_) {
    return false;
  }
  const auto* context = canvas_viewer_->object_interaction()
                            .entity_coordinator()
                            .tile_handler()
                            .context();
  return context && context->current_room_id == *current_room_id_;
}

bool ItemEditorPanel::CanEditRoom() const {
  return HasMatchingViewer() && canvas_viewer_->IsObjectInteractionEnabled() &&
         !canvas_viewer_->header_read_only();
}

void ItemEditorPanel::DrawPlacementControls() {
  const auto& theme = AgentUI::GetTheme();
  // Follow canvas cancellation and selection instead of a stale panel mode.
  placement_mode_ =
      HasMatchingViewer() &&
      canvas_viewer_->object_interaction().IsItemPlacementActive();
  if (placement_mode_) {
    selected_item_id_ = canvas_viewer_->object_interaction()
                            .entity_coordinator()
                            .item_handler()
                            .GetItemId();
    const char* item_name =
        GetDungeonPotItemName(static_cast<uint8_t>(selected_item_id_));
    ImGui::TextColored(theme.status_warning,
                       ICON_MD_PLACE " Placing: %s (0x%02X)", item_name,
                       selected_item_id_);
    if (ImGui::SmallButton(ICON_MD_CANCEL " Cancel")) {
      placement_mode_ = false;
      if (canvas_viewer_) {
        canvas_viewer_->object_interaction().SetItemPlacementMode(false, 0);
      }
    }
  } else {
    ImGui::TextColored(theme.text_secondary_gray,
                       ICON_MD_INFO " Select an item to place");
  }
}

void ItemEditorPanel::DrawItemSelector(bool show_room_items) {
  const bool can_edit = CanEditRoom();
  const auto& theme = AgentUI::GetTheme();
  ImGui::Text(ICON_MD_INVENTORY " Select Item:");

  // Item grid with responsive sizing
  float available_height = ImGui::GetContentRegionAvail().y;
  float reserved_height = 120.0f;
  const float grid_height =
      show_room_items
          ? std::max(150.0f,
                     std::min(400.0f, available_height - reserved_height))
          : std::max(1.0f, available_height);

  // Responsive item size based on panel width
  float panel_width = ImGui::GetContentRegionAvail().x;
  float item_size =
      std::max(36.0f, std::min(48.0f, (panel_width - 40.0f) / 6.0f));
  int items_per_row =
      std::max(1, static_cast<int>(panel_width / (item_size + 8)));

  ImGui::BeginChild("##ItemGrid", ImVec2(0, grid_height), true,
                    ImGuiWindowFlags_HorizontalScrollbar);
  ImGui::BeginDisabled(!can_edit);

  int col = 0;
  for (size_t i = 0; i < kDungeonPotItemCount; ++i) {
    bool is_selected = (selected_item_id_ == static_cast<int>(i));

    ImGui::PushID(static_cast<int>(i));

    ImVec4 button_color = GetItemTypeColor(static_cast<int>(i), theme);
    if (is_selected) {
      button_color.x = std::min(1.0f, button_color.x + 0.2f);
      button_color.y = std::min(1.0f, button_color.y + 0.2f);
      button_color.z = std::min(1.0f, button_color.z + 0.2f);
    }

    {
      gui::StyleColorGuard btn_colors({
          {ImGuiCol_Button, button_color},
          {ImGuiCol_ButtonHovered,
           ImVec4(std::min(1.0f, button_color.x + 0.1f),
                  std::min(1.0f, button_color.y + 0.1f),
                  std::min(1.0f, button_color.z + 0.1f), 1.0f)},
          {ImGuiCol_ButtonActive,
           ImVec4(std::min(1.0f, button_color.x + 0.2f),
                  std::min(1.0f, button_color.y + 0.2f),
                  std::min(1.0f, button_color.z + 0.2f), 1.0f)},
      });

      const char* icon = GetItemTypeIcon(static_cast<int>(i));
      std::string label =
          absl::StrFormat("%s\n%02X", icon, static_cast<int>(i));
      const bool picked =
          ImGui::Button(label.c_str(), ImVec2(item_size, item_size));
      gui::AutoRegisterLastItem("button",
                                absl::StrFormat("place_item_%02X", i));
      if (picked && can_edit) {
        selected_item_id_ = static_cast<int>(i);
        placement_mode_ = true;
        if (canvas_viewer_) {
          canvas_viewer_->object_interaction().SetItemPlacementMode(
              true, static_cast<uint8_t>(i));
        }
      }
    }

    if (ImGui::IsItemHovered()) {
      ImGui::SetTooltip(tr("%s (0x%02X)\nClick to select for placement"),
                        GetDungeonPotItemName(static_cast<uint8_t>(i)),
                        static_cast<int>(i));
    }

    if (is_selected) {
      ImVec2 min = ImGui::GetItemRectMin();
      ImVec2 max = ImGui::GetItemRectMax();
      ImU32 sel_color =
          ImGui::ColorConvertFloat4ToU32(theme.dungeon_selection_primary);
      ImGui::GetWindowDrawList()->AddRect(min, max, sel_color, 0.0f, 0, 2.0f);
    }

    ImGui::PopID();

    col++;
    if (col < items_per_row) {
      ImGui::SameLine();
    } else {
      col = 0;
    }
  }

  ImGui::EndDisabled();
  ImGui::EndChild();
}

void ItemEditorPanel::DrawRoomItems() {
  const auto& theme = AgentUI::GetTheme();
  auto& room = (*rooms_)[*current_room_id_];
  const auto& items = room.GetPotItems();
  int selected_item_index = -1;
  if (HasMatchingViewer() &&
      canvas_viewer_->object_interaction().HasEntitySelection()) {
    const auto selected_entity =
        canvas_viewer_->object_interaction().GetSelectedEntity();
    if (selected_entity.type == EntityType::Item) {
      selected_item_index = static_cast<int>(selected_entity.index);
    }
  }

  ImGui::Text(ICON_MD_LIST " Room Items (%zu):", items.size());
  if (selected_item_index >= 0 && open_selection_inspector_callback_) {
    ImGui::SameLine();
    if (ImGui::SmallButton(ICON_MD_OPEN_IN_NEW " Inspect Selected")) {
      open_selection_inspector_callback_();
    }
  }

  if (items.empty()) {
    ImGui::TextColored(theme.text_secondary_gray,
                       ICON_MD_INFO " No items in this room");
    return;
  }

  if (selected_item_index >= 0 &&
      selected_item_index < static_cast<int>(items.size()) && canvas_viewer_) {
    const bool can_edit = CanEditRoom();
    ImGui::BeginDisabled(!can_edit);
    uint8_t raw_item_type = items[selected_item_index].item;
    ImGui::AlignTextToFramePadding();
    ImGui::TextUnformatted(tr("Selected raw type"));
    ImGui::SameLine();
    ImGui::SetNextItemWidth(72.0f);
    const bool type_changed = gui::InputScalarDeferred(
        "##SelectedItemRawType", ImGuiDataType_U8, &raw_item_type, "%02X",
        ImGuiInputTextFlags_CharsHexadecimal,
        {reinterpret_cast<uintptr_t>(rooms_),
         static_cast<uint64_t>(*current_room_id_),
         static_cast<uint64_t>(selected_item_index)});
    gui::AutoRegisterLastItem("input_scalar", "selected_item_raw_type",
                              "Selected pot item raw type byte");
    ImGui::EndDisabled();
    if (type_changed && can_edit) {
      canvas_viewer_->object_interaction()
          .entity_coordinator()
          .item_handler()
          .MutateItemType(selected_item_index, raw_item_type);
    }
  }

  float list_height = std::max(100.0f, ImGui::GetContentRegionAvail().y);
  ImGui::BeginChild("##ItemList", ImVec2(0, list_height), true);
  for (size_t i = 0; i < items.size(); ++i) {
    const auto& item = items[i];

    ImGui::PushID(static_cast<int>(i));

    const char* item_name = GetDungeonPotItemName(item.item);

    const std::string row_label =
        absl::StrFormat("[%zu] %s (0x%02X)", i, item_name, item.item);
    const bool row_selected = ImGui::Selectable(
        row_label.c_str(), selected_item_index == static_cast<int>(i));
    gui::AutoRegisterLastItem("selectable", absl::StrFormat("item_row_%zu", i),
                              "Select a pot item in the current dungeon room");
    ImGui::SameLine();
    ImGui::TextColored(theme.text_secondary_gray, "@ (%d,%d)", item.GetTileX(),
                       item.GetTileY());

    if (row_selected && HasMatchingViewer()) {
      canvas_viewer_->object_interaction().SelectEntity(EntityType::Item, i);
    }

    ImGui::PopID();
  }
  ImGui::EndChild();
}

ImVec4 ItemEditorPanel::GetItemTypeColor(int item_id,
                                         const AgentUITheme& theme) const {
  if (item_id == 0 || item_id == 22) {
    return theme.dungeon_object_default;  // Gray for "Nothing"
  } else if (item_id >= 1 && item_id <= 7) {
    return theme.dungeon_sprite_layer0;  // Green for rupees/items
  } else if (item_id == 8) {
    return theme.dungeon_object_chest;  // Gold for key
  } else if (item_id >= 15 && item_id <= 17) {
    return theme.status_error;  // Red for enemies
  } else if (item_id >= 23 && item_id <= 27) {
    return theme.dungeon_object_stairs;  // Yellow for special
  }
  return theme.dungeon_object_pot;  // Pot color for default
}

const char* ItemEditorPanel::GetItemTypeIcon(int item_id) const {
  if (item_id == 0 || item_id == 22) {
    return ICON_MD_BLOCK;  // Nothing
  } else if (item_id == 1 || item_id == 7) {
    return ICON_MD_MONETIZATION_ON;  // Rupees (green/blue)
  } else if (item_id == 4 || item_id == 6 || item_id == 11 || item_id == 19 ||
             item_id == 21) {
    return ICON_MD_FAVORITE;  // Hearts
  } else if (item_id == 8) {
    return ICON_MD_KEY;  // Key
  } else if (item_id == 5 || item_id == 10) {
    return ICON_MD_CIRCLE;  // Bombs
  } else if (item_id == 9) {
    return ICON_MD_ARROW_UPWARD;  // Arrows
  } else if (item_id == 12 || item_id == 13) {
    return ICON_MD_AUTO_AWESOME;  // Magic
  } else if (item_id == 14) {
    return ICON_MD_EGG;  // Cucco
  } else if (item_id >= 15 && item_id <= 17) {
    return ICON_MD_PERSON;  // Soldiers
  } else if (item_id == 18) {
    return ICON_MD_WARNING;  // Landmine
  } else if (item_id == 20) {
    return ICON_MD_FLUTTER_DASH;  // Fairy
  } else if (item_id == 23) {
    return ICON_MD_TERRAIN;  // Hole
  } else if (item_id == 24) {
    return ICON_MD_SWAP_HORIZ;  // Warp
  } else if (item_id == 25) {
    return ICON_MD_STAIRS;  // Staircase
  } else if (item_id == 26) {
    return ICON_MD_BROKEN_IMAGE;  // Bombable
  } else if (item_id == 27) {
    return ICON_MD_TOGGLE_ON;  // Switch
  } else if (item_id == 2) {
    return ICON_MD_LANDSCAPE;  // Rock
  } else if (item_id == 3) {
    return ICON_MD_BUG_REPORT;  // Bee
  }
  return ICON_MD_HELP;  // Unknown
}

}  // namespace editor
}  // namespace yaze
