// Related header
#include "dungeon_object_selector.h"
#include "absl/strings/str_format.h"
#include "util/i18n/tr.h"

// C system headers
#include <cstring>
#include <filesystem>

// C++ standard library headers
#include <algorithm>
#include <array>
#include <cctype>
#include <iterator>

// Third-party library headers
#include "imgui/imgui.h"

// Project headers
#include "app/editor/dungeon/ui/window/object_tile_editor_panel.h"
#include "app/gfx/resource/arena.h"
#include "app/gui/core/agent_theme.h"
#include "app/gui/core/drag_drop.h"
#include "app/gui/core/icons.h"
#include "app/gui/core/style_guard.h"
#include "app/gui/core/ui_helpers.h"
#include "app/gui/widgets/themed_widgets.h"
#include "core/features.h"
#include "rom/rom.h"
#include "zelda3/dungeon/custom_object.h"  // For CustomObjectManager
#include "zelda3/dungeon/dimension_service.h"
#include "zelda3/dungeon/dungeon_object_registry.h"
#include "zelda3/dungeon/object_drawer.h"
#include "zelda3/dungeon/object_layer_semantics.h"
#include "zelda3/dungeon/object_tile_editor.h"
#include "zelda3/dungeon/room.h"
#include "zelda3/dungeon/room_object.h"  // For GetObjectName()

namespace yaze::editor {

namespace {

constexpr int kPersistedCustomSubtypeSlots = 16;

float GetObjectGridItemSize(int density) {
  switch (density) {
    case 0:
      return 54.0f;
    case 2:
      return 76.0f;
    case 1:
    default:
      return 60.0f;
  }
}

ImU32 ThemeColor(const ImVec4& color) {
  return ImGui::ColorConvertFloat4ToU32(color);
}

ImVec4 WithAlpha(ImVec4 color, float alpha) {
  color.w = alpha;
  return color;
}

void DrawFallbackPreviewTile(ImDrawList* draw_list, ImVec2 top_left,
                             ImVec2 size, const ImVec4& accent_color,
                             const char* label) {
  const auto& theme = AgentUI::GetTheme();
  draw_list->AddRectFilled(
      top_left, ImVec2(top_left.x + size.x, top_left.y + size.y),
      ThemeColor(WithAlpha(theme.panel_bg_darker, 0.72f)), 2.0f);
  draw_list->AddRectFilled(top_left,
                           ImVec2(top_left.x + 2.0f, top_left.y + size.y),
                           ThemeColor(WithAlpha(accent_color, 0.72f)), 2.0f);

  ImVec2 label_size = ImGui::CalcTextSize(label);
  if (label_size.x > size.x - 4.0f || label_size.y > size.y - 4.0f) {
    return;
  }
  ImVec2 label_pos(top_left.x + (size.x - label_size.x) / 2,
                   top_left.y + (size.y - label_size.y) / 2);
  draw_list->AddText(label_pos,
                     ThemeColor(WithAlpha(theme.text_primary, 0.82f)), label);
}

}  // namespace

DungeonObjectSelectorGridLayout ResolveDungeonObjectSelectorGridLayout(
    float available_width, float preferred_item_size, float item_spacing,
    float min_item_size) {
  const float safe_spacing = std::max(item_spacing, 0.0f);
  const float usable_width = std::max(available_width, 1.0f);
  const float safe_min_item_size =
      std::min(std::max(min_item_size, 1.0f), usable_width);
  const float clamped_preferred_item_size =
      std::clamp(preferred_item_size, safe_min_item_size, usable_width);

  DungeonObjectSelectorGridLayout layout;
  layout.columns = std::max(
      1, static_cast<int>((usable_width + safe_spacing) /
                          (clamped_preferred_item_size + safe_spacing)));
  layout.item_size = clamped_preferred_item_size;
  const float total_spacing = safe_spacing * (layout.columns - 1);
  const float occupied_width =
      layout.item_size * layout.columns + total_spacing;
  layout.leading_inset = std::max((usable_width - occupied_width) * 0.5f, 0.0f);
  return layout;
}

DungeonObjectPreviewFit ResolveDungeonObjectPreviewFit(float source_width,
                                                       float source_height,
                                                       float box_width,
                                                       float box_height) {
  DungeonObjectPreviewFit fit;
  if (source_width <= 0.0f || source_height <= 0.0f || box_width <= 0.0f ||
      box_height <= 0.0f) {
    return fit;
  }

  const float scale =
      std::min(box_width / source_width, box_height / source_height);
  fit.valid = true;
  fit.width = source_width * scale;
  fit.height = source_height * scale;
  fit.x = (box_width - fit.width) * 0.5f;
  fit.y = (box_height - fit.height) * 0.5f;
  return fit;
}

bool MatchesDungeonObjectStreamFilter(int object_id, int selected_filter) {
  switch (selected_filter) {
    case 1:
      return object_id >= 0x000 && object_id <= 0x0F7;
    case 2:
      return object_id >= 0x100 && object_id <= 0x13F;
    case 3:
      return object_id >= 0xF80 && object_id <= 0xFFF;
    case 0:
    default:
      return true;
  }
}

bool IsDungeonCustomObjectRuntimeSlot(int object_id, int subtype) {
  return subtype >= 0 &&
         subtype < zelda3::CustomObjectManager::RuntimeSubtypeCountForObject(
                       object_id);
}

bool IsMinecartGraphicsRuntimeSlot(int object_id, int subtype) {
  return object_id == 0x31 &&
         ((subtype >= 0 && subtype <= 12) || subtype == 14);
}

std::string GetDungeonCustomObjectSlotName(int object_id, int subtype) {
  static constexpr std::array<const char*, 16> kObject31Names = {
      "Track horizontal",
      "Track vertical",
      "Track corner top-left",
      "Track corner top-right",
      "Track corner bottom-left",
      "Track corner bottom-right",
      "Floor track vertical",
      "Floor track horizontal",
      "Floor track corner top-left",
      "Floor track corner top-right",
      "Floor track corner bottom-left",
      "Floor track corner bottom-right",
      "Floor track any direction",
      "Sword House wall override",
      "Track any direction",
      "Small statue",
  };
  static constexpr std::array<const char*, 3> kObject32Names = {
      "Ice furnace",
      "Firewood",
      "Ice chair",
  };
  if (object_id == 0x31 && subtype >= 0 &&
      subtype < static_cast<int>(kObject31Names.size())) {
    return kObject31Names[subtype];
  }
  if (object_id == 0x32 && subtype >= 0 &&
      subtype < static_cast<int>(kObject32Names.size())) {
    return kObject32Names[subtype];
  }
  return "Unknown custom runtime slot";
}

DungeonObjectSelector::~DungeonObjectSelector() {
  RetirePreviewCache();
}

bool DungeonObjectSelector::IsRepresentableChestObjectId(int object_id) {
  return object_id == 0xF99 || object_id == 0xF9A || object_id == 0xFB1 ||
         object_id == 0xFB2 || object_id == 0xFF5;
}

ImU32 DungeonObjectSelector::GetObjectTypeColor(int object_id) {
  const auto& theme = AgentUI::GetTheme();

  // Type 3 objects (0xF80-0xFFF) - Special room features
  if (object_id >= 0xF80) {
    if (IsRepresentableChestObjectId(object_id)) {
      return ImGui::GetColorU32(theme.item_color);  // Gold for chests
    } else if (object_id >= 0xF80 && object_id <= 0xF8F) {
      return ImGui::ColorConvertFloat4ToU32(
          theme.selection_secondary);  // Light blue for layer indicators
    } else if (object_id >= 0xF90 && object_id <= 0xF9F) {
      return ImGui::ColorConvertFloat4ToU32(
          theme.transport_color);  // Orange/Purple for door indicators
    } else {
      return ImGui::ColorConvertFloat4ToU32(
          theme.music_zone_color);  // Purple for misc Type 3
    }
  }

  // Type 2 objects (0x100-0x13F) - Torches, blocks, switches
  if (object_id >= 0x100 && object_id < 0x200) {
    if (object_id >= 0x100 && object_id <= 0x10F) {
      return ImGui::GetColorU32(theme.status_warning);  // Torches
    } else if (object_id >= 0x110 && object_id <= 0x11F) {
      return ImGui::GetColorU32(theme.dungeon_object_default);  // Blocks
    } else if (object_id >= 0x120 && object_id <= 0x12F) {
      return ImGui::ColorConvertFloat4ToU32(
          theme.status_success);  // Green for switches
    } else if (object_id >= 0x130 && object_id <= 0x13F) {
      return ImGui::GetColorU32(theme.selection_primary);  // Yellow for stairs
    } else {
      return ImGui::GetColorU32(theme.text_secondary_gray);  // Other Type 2
    }
  }

  // Type 1 objects (0x00-0xF7) - Base room objects
  if (object_id >= 0x10 && object_id <= 0x1F) {
    return ImGui::GetColorU32(theme.dungeon_object_wall);  // Gray for walls
  } else if (object_id >= 0x20 && object_id <= 0x2F) {
    return ImGui::GetColorU32(theme.dungeon_object_floor);  // Brown for floors
  } else if (object_id >= 0x30 && object_id <= 0x3F) {
    return ImGui::GetColorU32(
        theme.dungeon_object_decoration);  // Dim gray for decorations
  } else if (object_id >= 0x00 && object_id <= 0x0F) {
    return ImGui::GetColorU32(theme.dungeon_selection_secondary);  // Corners
  } else {
    return ImGui::GetColorU32(theme.dungeon_object_default);  // Default gray
  }
}

std::string DungeonObjectSelector::GetObjectTypeSymbol(int object_id) {
  // Type 3 objects (0xF80-0xFFF) - Special room features
  if (object_id >= 0xF80) {
    if (IsRepresentableChestObjectId(object_id)) {
      return "C";
    } else if (object_id >= 0xF80 && object_id <= 0xF8F) {
      return "L";  // Layer
    } else if (object_id >= 0xF90 && object_id <= 0xF9F) {
      return "D";  // Door indicator
    } else {
      return "S";  // Special
    }
  }

  // Type 2 objects (0x100-0x13F) - Torches, blocks, switches
  if (object_id >= 0x100 && object_id < 0x200) {
    if (object_id >= 0x100 && object_id <= 0x10F) {
      return "*";  // Torch (flame)
    } else if (object_id >= 0x110 && object_id <= 0x11F) {
      return "#";  // Block
    } else if (object_id >= 0x120 && object_id <= 0x12F) {
      return "o";  // Switch
    } else if (object_id >= 0x130 && object_id <= 0x13F) {
      return "^";  // Stairs
    } else {
      return "2";  // Type 2
    }
  }

  // Type 1 objects (0x00-0xF7) - Base room objects
  if (object_id >= 0x10 && object_id <= 0x1F) {
    return "|";  // Wall
  } else if (object_id >= 0x20 && object_id <= 0x2F) {
    return "_";  // Floor
  } else if (object_id >= 0x30 && object_id <= 0x3F) {
    return "~";  // Decoration
  } else if (object_id >= 0x00 && object_id <= 0x0F) {
    return "/";  // Corner
  } else {
    return "?";  // Unknown
  }
}

void DungeonObjectSelector::SelectObject(int obj_id, int subtype) {
  const int runtime_count =
      zelda3::CustomObjectManager::RuntimeSubtypeCountForObject(obj_id);
  if (runtime_count > 0 && subtype < 0 &&
      core::FeatureFlags::get().kEnableCustomObjects) {
    // In custom-enabled projects these IDs are dispatch families, not one
    // subtype-free object. Selection must come from the Workshop so the exact
    // runtime slot and its decoded source asset are known.
    return;
  }
  if (subtype >= 0) {
    if ((runtime_count > 0 &&
         !IsDungeonCustomObjectRuntimeSlot(obj_id, subtype)) ||
        (runtime_count == 0 && subtype >= kPersistedCustomSubtypeSlots)) {
      return;
    }
    if (runtime_count > 0) {
      if (!core::FeatureFlags::get().kEnableCustomObjects) {
        return;
      }
      SynchronizeCustomObjectGeneration();
      if (!GetCustomObjectAssetStatus(obj_id, subtype).ok()) {
        return;
      }
    }
  }

  selected_object_id_ = obj_id;

  // Create and update preview object
  uint8_t size = zelda3::DefaultRoomObjectSizeForPlacement(obj_id);
  if (subtype >= 0) {
    // Oracle custom objects use the explicitly selected size as a subtype.
    size =
        zelda3::CanonicalRoomObjectSize(obj_id, static_cast<uint8_t>(subtype));
  }
  preview_object_ = zelda3::RoomObject(obj_id, 0, 0, size, 0);
  preview_object_.SetRom(rom_);
  object_loaded_ = true;

  // Notify callback
  if (object_selected_callback_) {
    object_selected_callback_(preview_object_);
  }
}

void DungeonObjectSelector::DrawObjectAssetBrowser() {
  const auto& theme = AgentUI::GetTheme();

  // Object ranges supported by the room-object stream codec.
  struct ObjectRange {
    int start;
    int end;
    const char* label;
  };
  static const ObjectRange ranges[] = {
      {0x00, 0xF7, "Type 1"},
      {0x100, 0x13F, "Type 2"},
      {0xF80, 0xFFF, "Type 3"},
  };

  SynchronizeCustomObjectGeneration();
  auto& obj_manager = zelda3::CustomObjectManager::Get();
  const int custom_count =
      std::min(obj_manager.GetSubtypeCount(0x31),
               kPersistedCustomSubtypeSlots) +
      std::min(obj_manager.GetSubtypeCount(0x32), kPersistedCustomSubtypeSlots);

  const ImGuiStyle& style = ImGui::GetStyle();
  const float control_spacing = std::max(2.0f, style.ItemSpacing.x * 0.5f);
  {
    gui::StyleVarGuard toolbar_spacing_guard(
        ImGuiStyleVar_ItemSpacing,
        ImVec2(control_spacing, std::max(2.0f, style.ItemSpacing.y * 0.5f)));

    const float toolbar_width =
        std::max(ImGui::GetContentRegionAvail().x, 1.0f);
    ImGui::SetNextItemWidth(toolbar_width);
    ImGui::InputTextWithHint("##ObjectSearch", "Search by name or hex ID...",
                             object_search_buffer_,
                             sizeof(object_search_buffer_));

    static const char* kFilterLabels[] = {"All categories", "Walls", "Floors",
                                          "Chests",         "Doors", "Decor",
                                          "Stairs"};
    static const char* kStreamLabels[] = {"All streams", "Type 1", "Type 2",
                                          "Type 3"};
    constexpr const char* kMoreLabel = "More##ObjectSelectorMore";
    const float more_width = ImGui::CalcTextSize(kMoreLabel, nullptr, true).x +
                             style.FramePadding.x * 2.0f;
    const bool stack_filters = toolbar_width < 230.0f;
    const float filter_row_width =
        std::max(1.0f, toolbar_width - more_width - control_spacing);
    const float category_width =
        stack_filters ? toolbar_width : filter_row_width * 0.56f;
    const float stream_width =
        stack_filters ? filter_row_width
                      : std::max(1.0f, filter_row_width - category_width -
                                           control_spacing);

    ImGui::SetNextItemWidth(category_width);
    ImGui::Combo("##ObjectFilterType", &object_type_filter_, kFilterLabels,
                 IM_ARRAYSIZE(kFilterLabels));
    if (!stack_filters) {
      ImGui::SameLine(0.0f, control_spacing);
    }
    ImGui::SetNextItemWidth(stream_width);
    ImGui::Combo("##ObjectStreamFilter", &object_stream_filter_, kStreamLabels,
                 IM_ARRAYSIZE(kStreamLabels));
    ImGui::SameLine(0.0f, control_spacing);
    if (ImGui::Button(kMoreLabel, ImVec2(more_width, 0.0f))) {
      ImGui::OpenPopup("##ObjectSelectorOptionsPopup");
    }
    if (ImGui::BeginPopup("##ObjectSelectorOptionsPopup")) {
      const bool has_filters = object_search_buffer_[0] != '\0' ||
                               object_type_filter_ != 0 ||
                               object_stream_filter_ != 0;
      if (ImGui::MenuItem(tr("Clear filters"), nullptr, false, has_filters)) {
        object_search_buffer_[0] = '\0';
        object_type_filter_ = 0;
        object_stream_filter_ = 0;
      }

      ImGui::SeparatorText(tr("Display"));
      ImGui::Checkbox(tr("Show thumbnails"), &enable_object_previews_);
      if (ImGui::IsItemHovered()) {
        ImGui::SetTooltip(
            tr("Show rendered object thumbnails in the selector.\n"
               "Requires a room to be loaded and may cost some performance."));
      }
      ImGui::SeparatorText(tr("Card size"));
      static constexpr const char* kDensityLabels[] = {"Compact", "Medium",
                                                       "Large"};
      for (int density = 0; density < 3; ++density) {
        if (ImGui::MenuItem(kDensityLabels[density], nullptr,
                            object_grid_density_ == density)) {
          object_grid_density_ = density;
        }
      }

      ImGui::Separator();
      const std::string workshop_label =
          absl::StrFormat("Custom Object Workshop... (%d)", custom_count);
      const bool custom_objects_enabled =
          core::FeatureFlags::get().kEnableCustomObjects;
      if (ImGui::MenuItem(workshop_label.c_str(), nullptr, false,
                          custom_objects_enabled)) {
        open_custom_workshop_popup_ = true;
      }
      if (!custom_objects_enabled &&
          ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled)) {
        ImGui::SetTooltip(
            "%s", tr("Enable Custom Objects in the project before editing or "
                     "placing fixed runtime assets."));
      }
      ImGui::EndPopup();
    }
  }

  // The grid is the selector's sole scroll owner. Calculate its geometry only
  // after entering the child so themed padding and the scrollbar are included.
  const float child_height = std::max(ImGui::GetContentRegionAvail().y, 1.0f);
  if (ImGui::BeginChild("##ObjectGrid", ImVec2(0, child_height), false)) {
    const float item_spacing = control_spacing;
    // GetContentRegionAvail() already excludes the child window's scrollbar
    // and padding. Centering the fixed-density grid balances any remainder so
    // a resize cannot leave a second, artificial gutter on the right.
    const auto grid_layout = ResolveDungeonObjectSelectorGridLayout(
        ImGui::GetContentRegionAvail().x,
        GetObjectGridItemSize(object_grid_density_), item_spacing);
    const float item_size = grid_layout.item_size;
    const int columns = grid_layout.columns;
    gui::StyleVarGuard grid_spacing_guard(
        ImGuiStyleVar_ItemSpacing,
        ImVec2(item_spacing, std::max(item_spacing, style.ItemSpacing.y)));

    // Iterate through all object ranges
    for (const auto& range : ranges) {
      if (!MatchesDungeonObjectStreamFilter(range.start,
                                            object_stream_filter_)) {
        continue;
      }

      if (object_stream_filter_ == 0) {
        ImGui::TextDisabled("%s  0x%03X-0x%03X", range.label, range.start,
                            range.end);
      }

      int current_column = 0;

      for (int obj_id = range.start; obj_id <= range.end; ++obj_id) {
        if (core::FeatureFlags::get().kEnableCustomObjects &&
            zelda3::CustomObjectManager::RuntimeSubtypeCountForObject(obj_id) >
                0) {
          // Custom-enabled 0x31/0x32 are shown only in the Workshop, where an
          // exact runtime subtype and a validated source asset are required.
          continue;
        }
        if (!MatchesObjectFilter(obj_id, object_type_filter_)) {
          continue;
        }
        if (!MatchesDungeonObjectStreamFilter(obj_id, object_stream_filter_)) {
          continue;
        }

        std::string full_name = zelda3::GetObjectName(obj_id);
        if (!MatchesObjectSearch(obj_id, full_name)) {
          continue;
        }

        if (current_column > 0) {
          ImGui::SameLine(0.0f, item_spacing);
        } else {
          ImGui::SetCursorPosX(ImGui::GetCursorPosX() +
                               grid_layout.leading_inset);
        }

        ImGui::PushID(obj_id);

        // Create selectable button for object
        bool is_selected = (selected_object_id_ == obj_id);
        ImVec2 button_size(item_size, item_size);

        if (ImGui::Selectable("", is_selected, 0, button_size)) {
          SelectObject(obj_id);
        }
        const bool item_visible = ImGui::IsItemVisible();
        ImVec2 button_pos = ImGui::GetItemRectMin();
        gui::BeginRoomObjectDragSource(
            static_cast<uint16_t>(obj_id), current_room_id_, 0, 0,
            zelda3::DefaultRoomObjectSizeForPlacement(obj_id));
        ImDrawList* draw_list = ImGui::GetWindowDrawList();
        const bool show_id = item_size >= 32.0f;
        const float footer_height =
            show_id ? std::min(item_size * 0.32f, ImGui::GetFontSize() + 3.0f)
                    : 0.0f;
        const float card_padding = std::min(3.0f, item_size * 0.08f);
        const ImVec2 preview_pos(button_pos.x + card_padding,
                                 button_pos.y + card_padding);
        const ImVec2 preview_box(
            std::max(1.0f, item_size - card_padding * 2.0f),
            std::max(1.0f, item_size - footer_height - card_padding * 2.0f));

        // Only attempt graphical preview if enabled (performance optimization)
        bool rendered = false;
        if (item_visible && enable_object_previews_) {
          rendered = DrawObjectPreview(MakePreviewObject(obj_id), preview_pos,
                                       preview_box);
        }

        if (item_visible && !rendered) {
          std::string symbol = GetObjectTypeSymbol(obj_id);
          DrawFallbackPreviewTile(
              draw_list, preview_pos, preview_box,
              ImGui::ColorConvertU32ToFloat4(GetObjectTypeColor(obj_id)),
              symbol.c_str());
        }

        const bool item_hovered = ImGui::IsItemHovered();
        if (item_visible && (is_selected || item_hovered)) {
          const ImU32 border_color =
              ImGui::GetColorU32(is_selected ? theme.dungeon_selection_primary
                                             : theme.panel_border_color);
          draw_list->AddRect(
              button_pos,
              ImVec2(button_pos.x + item_size, button_pos.y + item_size),
              border_color, 2.0f, 0, is_selected ? 2.0f : 1.0f);
        }

        if (item_visible && show_id) {
          const ImVec2 card_bottom(button_pos.x + item_size,
                                   button_pos.y + item_size);
          const float footer_y = card_bottom.y - footer_height;
          draw_list->AddRectFilled(
              ImVec2(button_pos.x, footer_y), card_bottom,
              ImGui::GetColorU32(WithAlpha(theme.panel_bg_darker, 0.88f)),
              2.0f);
          std::string id_text = absl::StrFormat("%03X", obj_id);
          ImVec2 id_size = ImGui::CalcTextSize(id_text.c_str());
          ImVec2 id_pos = ImVec2(button_pos.x + (item_size - id_size.x) / 2,
                                 footer_y + (footer_height - id_size.y) * 0.5f);
          draw_list->AddText(id_pos, ImGui::GetColorU32(theme.text_primary),
                             id_text.c_str());
        }

        // Enhanced tooltip
        if (item_hovered) {
          gui::StyleColorGuard tooltip_guard(
              {{ImGuiCol_PopupBg, theme.panel_bg_color},
               {ImGuiCol_Border, theme.panel_border_color}});

          if (ImGui::BeginTooltip()) {
            ImGui::TextColored(theme.selection_primary, tr("Object 0x%03X"),
                               obj_id);
            ImGui::Text("%s", full_name.c_str());
            int subtype = zelda3::GetObjectSubtype(obj_id);
            ImGui::TextColored(theme.text_secondary_gray, tr("Subtype %d"),
                               subtype);
            ImGui::TextColored(
                rendered ? theme.status_success : theme.status_warning,
                tr("Preview: %s"),
                rendered ? "rendered tile layout"
                         : (enable_object_previews_ ? "fallback symbol"
                                                    : "thumbnails off"));
            ImGui::Separator();

            const uint8_t preview_size =
                zelda3::DefaultRoomObjectSizeForPlacement(obj_id);
            const bool can_capture_layout =
                rom_ && rooms_ && current_room_id_ >= 0 &&
                current_room_id_ < zelda3::kNumberOfRooms;
            const zelda3::Room* layout_room =
                can_capture_layout ? &(*rooms_)[current_room_id_] : nullptr;
            const uint32_t layout_key =
                MakeLayoutCacheKey(obj_id, preview_size, layout_room);
            if (can_capture_layout &&
                layout_cache_.find(layout_key) == layout_cache_.end()) {
              zelda3::ObjectTileEditor editor(rom_);
              auto& room_ref = *layout_room;
              auto layout_or = editor.CaptureObjectLayout(
                  obj_id, room_ref, current_palette_group_, preview_size);
              if (layout_or.ok()) {
                layout_cache_[layout_key] = layout_or.value();
              }
            }

            if (layout_cache_.count(layout_key)) {
              const auto& layout = layout_cache_[layout_key];
              ImGui::TextColored(theme.status_success, tr("Tiles: %zu"),
                                 layout.cells.size());

              if (can_capture_layout) {
                auto& room_ref = (*rooms_)[current_room_id_];
                zelda3::ObjectDrawer drawer(rom_, current_room_id_,
                                            room_ref.get_gfx_buffer().data());
                int rid = drawer.GetDrawRoutineId(obj_id);
                ImGui::TextColored(theme.status_active, tr("Draw Routine: %d"),
                                   rid);
              }

              ImGui::Text(tr("Layout:"));
              ImDrawList* tooltip_draw_list = ImGui::GetWindowDrawList();
              ImVec2 grid_start = ImGui::GetCursorScreenPos();
              float cell_size = 4.0f;
              for (const auto& cell : layout.cells) {
                ImVec2 p1(grid_start.x + cell.rel_x * cell_size,
                          grid_start.y + cell.rel_y * cell_size);
                ImVec2 p2(p1.x + cell_size, p1.y + cell_size);
                tooltip_draw_list->AddRectFilled(
                    p1, p2, ThemeColor(theme.dungeon_grid_cell_highlight));
                tooltip_draw_list->AddRect(
                    p1, p2, ThemeColor(theme.panel_border_color));
              }
              ImGui::Dummy(ImVec2(layout.bounds_width * cell_size,
                                  layout.bounds_height * cell_size));
            }

            ImGui::Separator();
            ImGui::TextColored(theme.text_secondary_gray,
                               tr("Click to select for placement"));
            ImGui::EndTooltip();
          }
        }

        ImGui::PopID();

        current_column = (current_column + 1) % columns;
      }  // end object loop
    }  // end range loop
  }

  ImGui::EndChild();
  if (open_custom_workshop_popup_) {
    ImGui::OpenPopup("Custom Object Workshop");
    open_custom_workshop_popup_ = false;
  }
  DrawCustomObjectWorkshopPopup();
}

bool DungeonObjectSelector::MatchesObjectFilter(int obj_id, int filter_type) {
  switch (filter_type) {
    case 1:  // Walls
      return obj_id >= 0x10 && obj_id <= 0x1F;
    case 2:  // Floors
      return obj_id >= 0x20 && obj_id <= 0x2F;
    case 3:  // Chests
      return IsRepresentableChestObjectId(obj_id);
    case 4:  // Doors
      return obj_id >= 0x17 && obj_id <= 0x1E;
    case 5:  // Decorations
      return obj_id >= 0x30 && obj_id <= 0x3F;
    case 6:  // Stairs
      return obj_id >= 0x138 && obj_id <= 0x13B;
    default:  // All
      return true;
  }
}

bool DungeonObjectSelector::MatchesObjectSearch(int obj_id,
                                                const std::string& name,
                                                int subtype) const {
  if (object_search_buffer_[0] == '\0') {
    return true;
  }

  auto to_lower = [](std::string value) {
    std::transform(value.begin(), value.end(), value.begin(),
                   [](unsigned char c) { return std::tolower(c); });
    return value;
  };

  std::string needle = to_lower(object_search_buffer_);
  std::string name_lower = to_lower(name);

  std::string id_hex = absl::StrFormat("%03X", obj_id);
  std::string id_lower = to_lower(id_hex);
  std::string id_pref = "0x" + id_lower;

  if (name_lower.find(needle) != std::string::npos) {
    return true;
  }
  if (id_lower.find(needle) != std::string::npos ||
      id_pref.find(needle) != std::string::npos) {
    return true;
  }

  if (subtype >= 0) {
    std::string sub_hex = absl::StrFormat("%02X", subtype);
    std::string sub_lower = to_lower(sub_hex);
    std::string combined = id_lower + ":" + sub_lower;
    std::string combined_pref = "0x" + combined;
    if (combined.find(needle) != std::string::npos ||
        combined_pref.find(needle) != std::string::npos) {
      return true;
    }
  }

  return false;
}

void DungeonObjectSelector::CalculateObjectDimensions(
    const zelda3::RoomObject& object, int& width, int& height) {
  auto [w, h] = zelda3::DimensionService::Get().GetPixelDimensions(object);
  width = std::min(w, 256);
  height = std::min(h, 256);
}

void DungeonObjectSelector::EnsureRegistryInitialized() {
  if (registry_initialized_)
    return;
  object_registry_.RegisterVanillaRange(0x000, 0x1FF);
  registry_initialized_ = true;
}

void DungeonObjectSelector::SynchronizeCustomObjectGeneration() {
  const bool custom_objects_enabled =
      core::FeatureFlags::get().kEnableCustomObjects;
  const uint64_t generation =
      zelda3::CustomObjectManager::Get().asset_generation();
  const bool custom_feature_changed =
      custom_objects_enabled != observed_custom_objects_enabled_;
  if (generation == observed_custom_object_generation_ &&
      !custom_feature_changed) {
    return;
  }

  if (rooms_ != nullptr) {
    rooms_->ForEachMaterialized(
        [](int, zelda3::Room& room) { room.MarkObjectsDirty(); });
  }
  InvalidatePreviewCache();
  observed_custom_object_generation_ = generation;
  observed_custom_objects_enabled_ = custom_objects_enabled;

  // Corner aliases remain valid Type 2 placements on both sides of the
  // custom-object feature flag. Re-emit the queued object so the placement
  // handler rebuilds its ghost using the current room's alias eligibility and
  // the latest custom asset bytes.
  if (object_loaded_ &&
      zelda3::IsTrackCornerAliasObjectId(preview_object_.id_)) {
    if (object_selected_callback_) {
      object_selected_callback_(preview_object_);
    }
    return;
  }

  const int subtype = preview_object_.size_ & 0x1F;
  if (!object_loaded_ ||
      !IsDungeonCustomObjectRuntimeSlot(preview_object_.id_, subtype)) {
    return;
  }

  // A subtype-free vanilla 0x31/0x32 selection stores an ordinary size in the
  // same bits used by Oracle's custom dispatch. Never reinterpret that queued
  // placement across either feature-state edge; require a fresh selection.
  if (custom_feature_changed) {
    object_loaded_ = false;
    selected_object_id_ = -1;
    if (placement_invalidated_callback_) {
      placement_invalidated_callback_();
    }
    return;
  }

  // With the feature stably disabled, 0x31/0x32 retain their vanilla size
  // semantics. A custom-asset cache or mapping change is irrelevant to that
  // queued placement.
  if (!custom_objects_enabled) {
    return;
  }

  if (!GetCustomObjectAssetStatus(preview_object_.id_, subtype).ok()) {
    object_loaded_ = false;
    selected_object_id_ = -1;
    if (placement_invalidated_callback_) {
      placement_invalidated_callback_();
    }
    return;
  }

  if (object_selected_callback_) {
    object_selected_callback_(preview_object_);
  }
}

void DungeonObjectSelector::DetachRuntimeContext() {
  rom_ = nullptr;
  game_data_ = nullptr;
  rooms_ = nullptr;
  tile_editor_panel_ = nullptr;
  open_tile_editor_window_callback_ = {};
  open_minecart_editor_window_callback_ = {};
  custom_object_action_error_.clear();
  object_loaded_ = false;
  selected_object_id_ = -1;
  placement_invalidated_callback_ = {};
  InvalidatePreviewCache();
  observed_custom_object_generation_ =
      zelda3::CustomObjectManager::Get().asset_generation();
  observed_custom_objects_enabled_ =
      core::FeatureFlags::get().kEnableCustomObjects;
}

absl::Status DungeonObjectSelector::GetCustomObjectAssetStatus(int object_id,
                                                               int subtype) {
  if (!IsDungeonCustomObjectRuntimeSlot(object_id, subtype)) {
    return absl::OutOfRangeError(
        "Custom object subtype is outside the runtime dispatch table");
  }

  const uint32_t key =
      (static_cast<uint32_t>(object_id) << 8) | static_cast<uint32_t>(subtype);
  if (const auto cached = custom_asset_status_cache_.find(key);
      cached != custom_asset_status_cache_.end()) {
    return cached->second;
  }

  auto object_or =
      zelda3::CustomObjectManager::Get().GetObjectInternal(object_id, subtype);
  absl::Status status = object_or.ok() ? absl::OkStatus() : object_or.status();
  custom_asset_status_cache_.emplace(key, status);
  return status;
}

zelda3::RoomObject DungeonObjectSelector::MakePreviewObject(int obj_id) const {
  zelda3::RoomObject obj(obj_id, 0, 0,
                         zelda3::DefaultRoomObjectSizeForPlacement(obj_id), 0);
  obj.SetRom(rom_);
  obj.EnsureTilesLoaded();
  return obj;
}

void DungeonObjectSelector::InvalidatePreviewCache() {
  RetirePreviewCache();
  layout_cache_.clear();
  custom_asset_status_cache_.clear();
  ++preview_cache_invalidations_;
}

void DungeonObjectSelector::RetirePreviewCache() {
  auto& arena = gfx::Arena::Get();
  for (auto& [key, preview] : preview_cache_) {
    (void)key;
    if (preview != nullptr) {
      arena.RetireBitmap(preview->bitmap());
    }
  }
  preview_cache_.clear();
}

void DungeonObjectSelector::SynchronizePreviewCacheRoomContext(
    const zelda3::Room& room) {
  if (current_room_id_ == cached_preview_room_id_ &&
      room.blockset() == cached_preview_blockset_ &&
      room.render_entrance_blockset() == cached_preview_entrance_blockset_ &&
      room.palette() == cached_preview_palette_ &&
      room.floor1() == cached_preview_floor1_ &&
      room.floor2() == cached_preview_floor2_) {
    return;
  }

  InvalidatePreviewCache();
  cached_preview_room_id_ = current_room_id_;
  cached_preview_blockset_ = room.blockset();
  cached_preview_entrance_blockset_ = room.render_entrance_blockset();
  cached_preview_palette_ = room.palette();
  cached_preview_floor1_ = room.floor1();
  cached_preview_floor2_ = room.floor2();
}

uint32_t DungeonObjectSelector::MakeLayoutCacheKey(int object_id,
                                                   uint8_t preview_size,
                                                   const zelda3::Room* room) {
  uint8_t room_floor = 0;
  if (room != nullptr) {
    if (object_id == 0xC4) {
      room_floor = room->floor1() & 0x0F;
    } else if (object_id == 0xDB) {
      room_floor = room->floor2() & 0x0F;
    }
  }

  return (static_cast<uint32_t>(object_id) << 16) |
         (static_cast<uint32_t>(preview_size) << 8) | room_floor;
}

bool DungeonObjectSelector::GetOrCreatePreview(const zelda3::RoomObject& object,
                                               gfx::BackgroundBuffer** out) {
  if (out == nullptr) {
    return false;
  }
  *out = nullptr;
  if (!rom_ || !rom_->is_loaded()) {
    return false;
  }
  SynchronizeCustomObjectGeneration();

  // Check if room context changed - invalidate cache if so
  const zelda3::Room* room =
      rooms_ != nullptr ? rooms_->GetIfLoaded(current_room_id_) : nullptr;
  if (room == nullptr) {
    return false;
  }
  SynchronizePreviewCacheRoomContext(*room);

  // Check if already in cache
  // Key: object, subtype, blockset, palette, and both room floor nibbles.
  // Room/entrance changes clear the complete cache before this lookup.
  const uint8_t preview_size =
      zelda3::CanonicalRoomObjectSize(object.id_, object.size());
  int subtype = preview_size & 0x1F;
  uint64_t cache_key =
      (static_cast<uint64_t>(object.id_) << 32) |
      (static_cast<uint64_t>(subtype) << 24) |
      (static_cast<uint64_t>(cached_preview_blockset_) << 16) |
      (static_cast<uint64_t>(cached_preview_palette_) << 8) |
      (static_cast<uint64_t>(cached_preview_floor1_ & 0x0F) << 4) |
      static_cast<uint64_t>(cached_preview_floor2_ & 0x0F);

  auto it = preview_cache_.find(cache_key);
  if (it != preview_cache_.end()) {
    *out = it->second.get();
    return (*out)->bitmap().texture() != nullptr;
  }

  // Create new preview using ObjectTileEditor
  const uint8_t* gfx_data = room->get_gfx_buffer().data();

  zelda3::ObjectTileEditor editor(rom_);
  auto layout_or = editor.CaptureObjectLayout(
      object.id_, *room, current_palette_group_, preview_size);
  if (!layout_or.ok()) {
    return false;
  }
  const auto& layout = layout_or.value();

  // Create preview buffer large enough for object
  int bmp_w = std::max(8, layout.bounds_width * 8);
  int bmp_h = std::max(8, layout.bounds_height * 8);
  auto preview = std::make_unique<gfx::BackgroundBuffer>(bmp_w, bmp_h);
  preview->EnsureBitmapInitialized();

  // Render layout to bitmap
  auto render_status = editor.RenderLayoutToBitmap(
      layout, preview->bitmap(), gfx_data, current_palette_group_);
  if (!render_status.ok()) {
    gfx::Arena::Get().RetireBitmap(preview->bitmap());
    return false;
  }

  auto& bitmap = preview->bitmap();
  // Texture creation and SDL sync
  if (!bitmap.surface()) {
    gfx::Arena::Get().RetireBitmap(bitmap);
    return false;
  }
  SDL_LockSurface(bitmap.surface());
  memcpy(bitmap.surface()->pixels, bitmap.mutable_data().data(),
         bitmap.mutable_data().size());
  SDL_UnlockSurface(bitmap.surface());

  // Install the owner before queuing CREATE. The renderer may defer this
  // command until DoRender, so the Bitmap address must remain valid even when
  // this frame falls back to the symbolic preview.
  auto [cache_it, inserted] =
      preview_cache_.try_emplace(cache_key, std::move(preview));
  if (!inserted) {
    if (preview != nullptr) {
      gfx::Arena::Get().RetireBitmap(preview->bitmap());
    }
    *out = cache_it->second.get();
    return (*out)->bitmap().texture() != nullptr;
  }

  *out = cache_it->second.get();
  auto& cached_bitmap = (*out)->bitmap();
  gfx::Arena::Get().QueueTextureCommand(gfx::Arena::TextureCommandType::CREATE,
                                        &cached_bitmap);
  gfx::Arena::Get().ProcessTextureQueue(nullptr);

  // A null texture is an expected deferred state when the Arena has no active
  // renderer yet. Keep the cache entry and its queued owner alive; the next
  // frame will draw it after DoRender processes CREATE.
  return cached_bitmap.texture() != nullptr;
}

bool DungeonObjectSelector::DrawObjectPreview(const zelda3::RoomObject& object,
                                              ImVec2 top_left,
                                              ImVec2 box_size) {
  gfx::BackgroundBuffer* preview = nullptr;
  if (!GetOrCreatePreview(object, &preview)) {
    return false;
  }

  // Draw the cached preview image
  auto& bitmap = preview->bitmap();
  if (!bitmap.texture()) {
    return false;
  }

  const DungeonObjectPreviewFit fit = ResolveDungeonObjectPreviewFit(
      static_cast<float>(bitmap.width()), static_cast<float>(bitmap.height()),
      box_size.x, box_size.y);
  if (!fit.valid) {
    return false;
  }

  const ImVec2 image_top_left(top_left.x + fit.x, top_left.y + fit.y);
  const ImVec2 image_bottom_right(image_top_left.x + fit.width,
                                  image_top_left.y + fit.height);
  ImGui::GetWindowDrawList()->AddImage((ImTextureID)(intptr_t)bitmap.texture(),
                                       image_top_left, image_bottom_right);
  return true;
}

absl::Status DungeonObjectSelector::OpenExistingCustomObjectEditor(
    int16_t object_id, int subtype, int room_id) {
  if (tile_editor_panel_ == nullptr) {
    return absl::FailedPreconditionError(
        "Object Tile Editor panel is unavailable");
  }

  const absl::Status open_status = tile_editor_panel_->OpenForCustomObject(
      object_id, subtype, room_id, rooms_, current_palette_group_);
  if (!open_status.ok()) {
    if (tile_editor_panel_->IsOpen() && open_tile_editor_window_callback_) {
      (void)open_tile_editor_window_callback_();
    }
    return open_status;
  }
  if (!open_tile_editor_window_callback_ ||
      !open_tile_editor_window_callback_()) {
    tile_editor_panel_->Close();
    return absl::NotFoundError(
        "Object Tile Editor window is not registered in this session");
  }
  return absl::OkStatus();
}

void DungeonObjectSelector::DrawCustomObjectWorkshopPopup() {
  const auto& theme = AgentUI::GetTheme();
  auto& obj_manager = zelda3::CustomObjectManager::Get();
  const std::string custom_base_path = obj_manager.GetBasePath();

  ImGui::SetNextWindowSize(ImVec2(860.0f, 620.0f), ImGuiCond_FirstUseEver);
  if (!ImGui::BeginPopupModal("Custom Object Workshop", nullptr,
                              ImGuiWindowFlags_None)) {
    return;
  }

  ImGui::TextColored(theme.text_info,
                     ICON_MD_PRECISION_MANUFACTURING " Custom Object Workshop");
  ImGui::TextColored(
      theme.text_secondary_gray,
      tr("Manage the 19 fixed runtime assets used by Oracle custom objects."));
  ImGui::TextDisabled(
      "%s", tr("New subtypes require an ASM dispatch-table change; this "
               "workshop edits or places existing slots only."));
  ImGui::Separator();

  if (!core::FeatureFlags::get().kEnableCustomObjects) {
    ImGui::TextColored(
        theme.text_warning_yellow, ICON_MD_WARNING
        " Custom Objects is disabled for this project. Enable the feature "
        "before editing or placing runtime assets.");
    if (ImGui::Button(ICON_MD_CLOSE " Close")) {
      ImGui::CloseCurrentPopup();
    }
    ImGui::EndPopup();
    return;
  }

  if (ImGui::BeginTable(
          "##CustomObjectToolbar", 2,
          ImGuiTableFlags_SizingStretchProp | ImGuiTableFlags_NoPadOuterX)) {
    ImGui::TableSetupColumn("Status", ImGuiTableColumnFlags_WidthStretch, 2.0f);
    ImGui::TableSetupColumn("Actions", ImGuiTableColumnFlags_WidthFixed,
                            180.0f);
    ImGui::TableNextRow();

    ImGui::TableNextColumn();
    if (custom_base_path.empty()) {
      ImGui::TextColored(theme.text_warning_yellow, ICON_MD_WARNING
                         " Custom object folder is not configured.");
    } else {
      ImGui::TextColored(theme.text_secondary_gray,
                         ICON_MD_FOLDER " Workshop folder");
      if (ImGui::IsItemHovered()) {
        ImGui::SetTooltip("%s", custom_base_path.c_str());
      }
    }

    ImGui::TableNextColumn();
    if (ImGui::Button(ICON_MD_REFRESH " Reload Assets", ImVec2(-1, 0))) {
      obj_manager.ReloadAll();
      SynchronizeCustomObjectGeneration();
      custom_object_action_error_.clear();
    }
    if (ImGui::IsItemHovered()) {
      ImGui::SetTooltip(
          tr("Reload custom object binaries and refresh their previews"));
    }
    ImGui::EndTable();
  }

  ImGui::TextColored(
      theme.text_secondary_gray, ICON_MD_INFO
      " Track graphics are assets here; track paths and collision are managed "
      "in Minecart Tracks.");
  ImGui::Spacing();

  if (ImGui::BeginChild("##CustomObjectGrid",
                        ImVec2(0, -ImGui::GetFrameHeightWithSpacing() * 5.5f),
                        false)) {
    const ImGuiStyle& style = ImGui::GetStyle();
    const float item_spacing = std::max(2.0f, style.ItemSpacing.x * 0.5f);
    const auto grid_layout = ResolveDungeonObjectSelectorGridLayout(
        ImGui::GetContentRegionAvail().x,
        GetObjectGridItemSize(object_grid_density_), item_spacing);
    const int columns = grid_layout.columns;
    const float item_size = grid_layout.item_size;
    gui::StyleVarGuard grid_spacing_guard(
        ImGuiStyleVar_ItemSpacing,
        ImVec2(item_spacing, std::max(item_spacing, style.ItemSpacing.y)));
    int custom_col = 0;
    for (int obj_id : {0x31, 0x32}) {
      if (!MatchesObjectFilter(obj_id, object_type_filter_)) {
        continue;
      }
      const int subtype_count = std::min(obj_manager.GetSubtypeCount(obj_id),
                                         kPersistedCustomSubtypeSlots);
      for (int subtype = 0; subtype < subtype_count; ++subtype) {
        const std::string subtype_name =
            GetDungeonCustomObjectSlotName(obj_id, subtype);
        if (!MatchesObjectSearch(obj_id, subtype_name, subtype)) {
          continue;
        }

        if (custom_col > 0) {
          ImGui::SameLine(0.0f, item_spacing);
        } else {
          ImGui::SetCursorPosX(ImGui::GetCursorPosX() +
                               grid_layout.leading_inset);
        }

        ImGui::PushID(obj_id * 1000 + subtype);

        const absl::Status asset_status =
            GetCustomObjectAssetStatus(obj_id, subtype);
        const bool asset_ready = asset_status.ok();

        const bool is_selected =
            workshop_object_id_ == obj_id && workshop_subtype_ == subtype;
        ImVec2 button_size(item_size, item_size);

        if (ImGui::Selectable("", is_selected, 0, button_size)) {
          workshop_object_id_ = obj_id;
          workshop_subtype_ = subtype;
          custom_object_action_error_.clear();
        }
        const bool item_visible = ImGui::IsItemVisible();
        ImVec2 button_pos = ImGui::GetItemRectMin();
        if (asset_ready && rooms_ != nullptr &&
            rooms_->GetIfLoaded(current_room_id_) != nullptr) {
          gui::BeginRoomObjectDragSource(
              static_cast<uint16_t>(obj_id), current_room_id_, 0, 0,
              zelda3::CanonicalRoomObjectSize(obj_id,
                                              static_cast<uint8_t>(subtype)));
        }
        ImDrawList* draw_list = ImGui::GetWindowDrawList();
        const bool show_id = item_size >= 44.0f;
        const float footer_height =
            show_id ? std::min(item_size * 0.32f, ImGui::GetFontSize() + 3.0f)
                    : 0.0f;
        const float card_padding = std::min(3.0f, item_size * 0.08f);
        const ImVec2 preview_pos(button_pos.x + card_padding,
                                 button_pos.y + card_padding);
        const ImVec2 preview_box(
            std::max(1.0f, item_size - card_padding * 2.0f),
            std::max(1.0f, item_size - footer_height - card_padding * 2.0f));

        bool rendered = false;
        if (item_visible && enable_object_previews_) {
          auto temp_obj = MakePreviewObject(obj_id);
          temp_obj.size_ = zelda3::CanonicalRoomObjectSize(
              obj_id, static_cast<uint8_t>(subtype));
          rendered = DrawObjectPreview(temp_obj, preview_pos, preview_box);
        }

        if (item_visible && !rendered) {
          std::string sub_text = absl::StrFormat("%02X", subtype);
          DrawFallbackPreviewTile(
              draw_list, preview_pos, preview_box,
              asset_ready ? theme.status_success : theme.status_error,
              sub_text.c_str());
        }

        const bool item_hovered = ImGui::IsItemHovered();
        if (item_visible && (is_selected || item_hovered)) {
          const ImU32 border_color =
              ImGui::GetColorU32(is_selected ? theme.dungeon_selection_primary
                                             : theme.panel_border_color);
          draw_list->AddRect(
              button_pos,
              ImVec2(button_pos.x + item_size, button_pos.y + item_size),
              border_color, 2.0f, 0, is_selected ? 2.0f : 1.0f);
        }

        if (item_visible && show_id) {
          const ImVec2 card_bottom(button_pos.x + item_size,
                                   button_pos.y + item_size);
          const float footer_y = card_bottom.y - footer_height;
          draw_list->AddRectFilled(
              ImVec2(button_pos.x, footer_y), card_bottom,
              ImGui::GetColorU32(WithAlpha(theme.panel_bg_darker, 0.88f)),
              2.0f);
          std::string id_text = absl::StrFormat("%02X:%02X", obj_id, subtype);
          ImVec2 id_size = ImGui::CalcTextSize(id_text.c_str());
          ImVec2 id_pos = ImVec2(button_pos.x + (item_size - id_size.x) / 2,
                                 footer_y + (footer_height - id_size.y) * 0.5f);
          draw_list->AddText(id_pos, ImGui::GetColorU32(theme.text_primary),
                             id_text.c_str());
        }

        if (item_hovered) {
          gui::StyleColorGuard tooltip_guard(
              {{ImGuiCol_PopupBg, theme.panel_bg_color},
               {ImGuiCol_Border, theme.panel_border_color}});
          if (ImGui::BeginTooltip()) {
            const std::string filename =
                obj_manager.ResolveFilename(obj_id, subtype);

            ImGui::TextColored(theme.selection_primary,
                               tr("Custom 0x%02X:%02X"), obj_id, subtype);
            ImGui::Text("%s", subtype_name.c_str());
            ImGui::TextColored(
                rendered ? theme.status_success : theme.status_warning,
                tr("Preview: %s"),
                rendered ? "rendered custom layout"
                         : (enable_object_previews_ ? "fallback subtype"
                                                    : "thumbnails off"));
            ImGui::Separator();
            ImGui::Text(tr("File: %s"),
                        filename.empty() ? "(unmapped)" : filename.c_str());
            if (asset_ready) {
              ImGui::TextColored(theme.status_success,
                                 tr("Asset decoded and ready"));
            } else {
              ImGui::TextColored(theme.status_error, "%s",
                                 asset_status.message().data());
            }

            if (obj_id == 0x31 && subtype >= 2 && subtype <= 5) {
              const char* corner_id = "";
              if (subtype == 2) {
                corner_id = "0x100 (TL)";
              } else if (subtype == 3) {
                corner_id = "0x102 (TR)";
              } else if (subtype == 4) {
                corner_id = "0x101 (BL)";
              } else {
                corner_id = "0x103 (BR)";
              }
              ImGui::Separator();
              ImGui::TextColored(theme.status_active,
                                 tr("Can also drive corner override %s when "
                                    "explicitly mapped"),
                                 corner_id);
            }
            ImGui::EndTooltip();
          }
        }

        ImGui::PopID();
        custom_col = (custom_col + 1) % columns;
      }
    }
  }
  ImGui::EndChild();

  ImGui::Separator();
  const std::string selected_name =
      GetDungeonCustomObjectSlotName(workshop_object_id_, workshop_subtype_);
  const std::string selected_filename =
      obj_manager.ResolveFilename(workshop_object_id_, workshop_subtype_);
  const absl::Status selected_asset_status =
      GetCustomObjectAssetStatus(workshop_object_id_, workshop_subtype_);
  const auto selected_path_or =
      zelda3::ResolveCustomObjectAssetPath(custom_base_path, selected_filename);
  const bool selected_asset_ready = selected_asset_status.ok();
  ImGui::TextColored(theme.selection_primary, "0x%02X:%02X  %s",
                     workshop_object_id_, workshop_subtype_,
                     selected_name.c_str());
  ImGui::SameLine();
  ImGui::TextDisabled("%s", selected_filename.empty()
                                ? "(unmapped)"
                                : selected_filename.c_str());
  if (selected_asset_ready) {
    ImGui::TextColored(theme.status_success,
                       ICON_MD_CHECK_CIRCLE " Asset ready");
    if (selected_path_or.ok() && ImGui::IsItemHovered()) {
      ImGui::SetTooltip("%s", selected_path_or->string().c_str());
    }
  } else {
    ImGui::TextColored(theme.status_error, ICON_MD_ERROR " %s",
                       selected_asset_status.message().data());
  }
  if (workshop_object_id_ == 0x31 && workshop_subtype_ == 13) {
    ImGui::SameLine();
    ImGui::TextDisabled("%s", tr("ASM-backed wall override"));
  } else if (IsMinecartGraphicsRuntimeSlot(workshop_object_id_,
                                           workshop_subtype_)) {
    ImGui::SameLine();
    ImGui::TextDisabled("%s", tr("Graphics only; behavior is separate"));
  }
  if (!custom_object_action_error_.empty()) {
    ImGui::TextColored(theme.status_error, ICON_MD_ERROR " %s",
                       custom_object_action_error_.c_str());
  }

  const bool has_room_context =
      rooms_ != nullptr && rooms_->GetIfLoaded(current_room_id_) != nullptr;
#if defined(__EMSCRIPTEN__)
  constexpr bool kCanPublishCustomAssets = false;
#else
  constexpr bool kCanPublishCustomAssets = true;
#endif
  const bool can_edit = selected_asset_ready && has_room_context &&
                        tile_editor_panel_ != nullptr &&
                        kCanPublishCustomAssets;
  if (!can_edit) {
    ImGui::BeginDisabled();
  }
  if (ImGui::Button(ICON_MD_EDIT " Edit Graphics")) {
    const absl::Status status = OpenExistingCustomObjectEditor(
        static_cast<int16_t>(workshop_object_id_), workshop_subtype_,
        current_room_id_);
    if (status.ok()) {
      custom_object_action_error_.clear();
      ImGui::CloseCurrentPopup();
    } else {
      custom_object_action_error_ = std::string(status.message());
    }
  }
  if (!can_edit) {
    ImGui::EndDisabled();
  }
  if (!kCanPublishCustomAssets &&
      ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled)) {
    ImGui::SetTooltip(
        "%s", tr("Custom asset publishing is desktop-only until browser "
                 "persistence can be verified."));
  }

  ImGui::SameLine();
  const bool can_use_in_room = selected_asset_ready && has_room_context;
  if (!can_use_in_room) {
    ImGui::BeginDisabled();
  }
  if (ImGui::Button(ICON_MD_ADD " Use in Room")) {
    SelectObject(workshop_object_id_, workshop_subtype_);
    custom_object_action_error_.clear();
    ImGui::CloseCurrentPopup();
  }
  if (!can_use_in_room) {
    ImGui::EndDisabled();
  }

  if (IsMinecartGraphicsRuntimeSlot(workshop_object_id_, workshop_subtype_)) {
    ImGui::SameLine();
    const bool can_open_minecart =
        static_cast<bool>(open_minecart_editor_window_callback_);
    if (!can_open_minecart) {
      ImGui::BeginDisabled();
    }
    if (ImGui::Button(ICON_MD_TRAIN " Minecart Paths & Collision")) {
      if (open_minecart_editor_window_callback_()) {
        custom_object_action_error_.clear();
        ImGui::CloseCurrentPopup();
      } else {
        custom_object_action_error_ =
            "Minecart Tracks window is unavailable in this session";
      }
    }
    if (!can_open_minecart) {
      ImGui::EndDisabled();
    }
  }

  ImGui::SameLine();
  if (ImGui::Button(ICON_MD_CLOSE " Close")) {
    ImGui::CloseCurrentPopup();
  }

  ImGui::EndPopup();
}

}  // namespace yaze::editor
