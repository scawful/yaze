// Related header
#include "dungeon_object_selector.h"
#include "absl/strings/str_format.h"
#include "util/i18n/tr.h"

// C system headers
#include <cstring>
#include <filesystem>

// C++ standard library headers
#include <algorithm>
#include <cctype>
#include <iterator>

// Third-party library headers
#include "imgui/imgui.h"

// Project headers
#include "app/editor/dungeon/ui/window/object_tile_editor_panel.h"
#include "app/gui/core/agent_theme.h"
#include "app/gui/core/drag_drop.h"
#include "app/gui/core/icons.h"
#include "app/gui/core/style_guard.h"
#include "app/gui/core/ui_helpers.h"
#include "app/gui/widgets/themed_widgets.h"
#include "rom/rom.h"
#include "zelda3/dungeon/custom_object.h"  // For CustomObjectManager
#include "zelda3/dungeon/dimension_service.h"
#include "zelda3/dungeon/dungeon_object_registry.h"
#include "zelda3/dungeon/object_drawer.h"
#include "zelda3/dungeon/object_tile_editor.h"
#include "zelda3/dungeon/room.h"
#include "zelda3/dungeon/room_object.h"  // For GetObjectName()

namespace yaze::editor {

namespace {

constexpr int kPersistedCustomSubtypeSlots = 16;

float GetObjectGridItemSize(int density) {
  switch (density) {
    case 0:
      return 56.0f;
    case 2:
      return 88.0f;
    case 1:
    default:
      return 72.0f;
  }
}

ImU32 ThemeColor(const ImVec4& color) {
  return ImGui::ColorConvertFloat4ToU32(color);
}

ImVec4 WithAlpha(ImVec4 color, float alpha) {
  color.w = alpha;
  return color;
}

ImVec4 Dimmed(ImVec4 color, float factor) {
  color.x *= factor;
  color.y *= factor;
  color.z *= factor;
  return color;
}

ImU32 GetObjectRangeHeaderColor(int start_id) {
  const auto& theme = AgentUI::GetTheme();
  if (start_id >= 0xF80) {
    return ThemeColor(theme.music_zone_color);
  }
  if (start_id >= 0x100) {
    return ThemeColor(theme.selection_secondary);
  }
  return ThemeColor(theme.dungeon_object_wall);
}

ImU32 GetObjectRangeHeaderHoverColor(int start_id) {
  const auto& theme = AgentUI::GetTheme();
  if (start_id >= 0xF80) {
    return ThemeColor(WithAlpha(theme.music_zone_color, 1.0f));
  }
  if (start_id >= 0x100) {
    return ThemeColor(WithAlpha(theme.selection_secondary, 1.0f));
  }
  return ThemeColor(WithAlpha(theme.dungeon_object_wall, 1.0f));
}

void DrawFallbackPreviewTile(ImDrawList* draw_list, ImVec2 top_left,
                             float item_size, const ImVec4& accent_color,
                             const char* label) {
  const auto& theme = AgentUI::GetTheme();
  const ImU32 accent = ThemeColor(accent_color);
  const ImU32 dimmed = ThemeColor(Dimmed(accent_color, 0.55f));

  draw_list->AddRectFilledMultiColor(
      top_left, ImVec2(top_left.x + item_size, top_left.y + item_size), dimmed,
      dimmed, accent, accent);

  ImVec2 label_size = ImGui::CalcTextSize(label);
  ImVec2 label_pos(top_left.x + (item_size - label_size.x) / 2,
                   top_left.y + (item_size - label_size.y) / 2 - 10);
  draw_list->AddText(label_pos,
                     ThemeColor(WithAlpha(theme.text_primary, 0.82f)), label);
}

}  // namespace

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
  } else if (object_id >= 0x17 && object_id <= 0x1E) {
    return ImGui::GetColorU32(theme.dungeon_object_floor);  // Brown for doors
  } else if (object_id == 0x2F || object_id == 0x2B) {
    return ImGui::GetColorU32(
        theme.dungeon_object_pot);  // Saddle brown for pots
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
  } else if (object_id >= 0x17 && object_id <= 0x1E) {
    return "+";  // Door
  } else if (object_id == 0x2F || object_id == 0x2B) {
    return "o";  // Pot
  } else if (object_id >= 0x30 && object_id <= 0x3F) {
    return "~";  // Decoration
  } else if (object_id >= 0x00 && object_id <= 0x0F) {
    return "/";  // Corner
  } else {
    return "?";  // Unknown
  }
}

void DungeonObjectSelector::SelectObject(int obj_id, int subtype) {
  if (subtype >= kPersistedCustomSubtypeSlots) {
    return;
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
  if (game_data_) {
    auto palette =
        game_data_->palette_groups.dungeon_main[current_palette_group_id_];
    preview_palette_ = palette;
  }
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

  // Total object count
  int total_objects =
      (0xF7 - 0x00 + 1) + (0x13F - 0x100 + 1) + (0xFFF - 0xF80 + 1);

  EnsureCustomObjectsInitialized();
  auto& obj_manager = zelda3::CustomObjectManager::Get();
  const int custom_count =
      std::min(obj_manager.GetSubtypeCount(0x31),
               kPersistedCustomSubtypeSlots) +
      std::min(obj_manager.GetSubtypeCount(0x32), kPersistedCustomSubtypeSlots);

  // Row 1: search input, full-width since this is the most-used control.
  ImGui::SetNextItemWidth(-1.0f);
  ImGui::InputTextWithHint(
      "##ObjectSearch", ICON_MD_SEARCH " Filter objects by name or hex...",
      object_search_buffer_, sizeof(object_search_buffer_));

  // Row 2: category filter + clear + display popover. Display options
  // (thumbnails toggle, grid density) are rare-use preferences hidden behind
  // an ICON_MD_TUNE popover so the chrome stays one row instead of three.
  static const char* kFilterLabels[] = {"All",   "Walls", "Floors", "Chests",
                                        "Doors", "Decor", "Stairs"};
  const float controls_width = ImGui::GetContentRegionAvail().x;
  const float filter_width = std::min(170.0f, controls_width);
  ImGui::SetNextItemWidth(filter_width);
  ImGui::Combo("##ObjectFilterType", &object_type_filter_, kFilterLabels,
               IM_ARRAYSIZE(kFilterLabels));
  ImGui::SameLine();
  if (gui::ThemedIconButton(ICON_MD_CLEAR, "Clear search and category filter",
                            gui::IconSize::Small())) {
    object_search_buffer_[0] = '\0';
    object_type_filter_ = 0;
  }
  ImGui::SameLine();
  if (gui::ThemedIconButton(ICON_MD_TUNE, "Display options",
                            gui::IconSize::Small())) {
    ImGui::OpenPopup("##ObjectSelectorDisplayPopup");
  }
  if (ImGui::BeginPopup("##ObjectSelectorDisplayPopup")) {
    ImGui::TextDisabled(tr("Display"));
    ImGui::Checkbox(ICON_MD_IMAGE " Thumbnails", &enable_object_previews_);
    if (ImGui::IsItemHovered()) {
      ImGui::SetTooltip(
          tr("Show rendered object thumbnails in the selector.\n"
             "Requires a room to be loaded and may cost some performance."));
    }
    ImGui::Spacing();
    ImGui::TextDisabled(tr("Grid density"));
    static constexpr const char* kDensityLabels[] = {"Small", "Medium",
                                                     "Large"};
    constexpr float kDensitySegmentWidth = 84.0f;
    const float density_spacing = ImGui::GetStyle().ItemSpacing.x;
    for (int density = 0; density < 3; ++density) {
      if (density > 0) {
        ImGui::SameLine(0.0f, density_spacing);
      }
      if (gui::ToggleButton(kDensityLabels[density],
                            object_grid_density_ == density,
                            ImVec2(kDensitySegmentWidth, 0.0f))) {
        object_grid_density_ = density;
      }
    }
    ImGui::EndPopup();
  }

  // Row 3: status row, count + selection chip + Custom Workshop entry.
  // Inlines on wider drawers, wraps on narrow.
  ImGui::Spacing();
  ImGui::TextColored(theme.text_secondary_gray, tr("%d vanilla objects"),
                     total_objects);
  if (selected_object_id_ >= 0) {
    if (ImGui::GetContentRegionAvail().x > 220.0f) {
      ImGui::SameLine();
    }
    ImGui::TextColored(theme.text_info, ICON_MD_LABEL " Queued 0x%03X %s",
                       selected_object_id_,
                       zelda3::GetObjectName(selected_object_id_).c_str());
  }
  if (ImGui::GetContentRegionAvail().x > 180.0f) {
    ImGui::SameLine();
  }
  DrawCustomObjectWorkshopButton(custom_count);

  // Create asset browser-style grid
  const float item_spacing = 6.0f;
  // Defensive clamp: when the inspector drawer narrows below the user's chosen
  // density, scale the cell down so at least one item is fully visible per row
  // before the horizontal scrollbar engages. The 16px margin accounts for the
  // child window's vertical scrollbar plus a small buffer for the cell border.
  constexpr float kRightMargin = 16.0f;
  constexpr float kMinItemSize = 32.0f;
  const float available_width = ImGui::GetContentRegionAvail().x;
  const float requested_item_size = GetObjectGridItemSize(object_grid_density_);
  const float item_size =
      std::min(requested_item_size,
               std::max(kMinItemSize, available_width - kRightMargin));
  const int columns =
      std::max(1, static_cast<int>((available_width - item_spacing) /
                                   (item_size + item_spacing)));

  // Scrollable child region for grid - use all available space.
  // Horizontal scrollbar guards against silent right-edge clipping when a row
  // overruns due to per-frame column recalculation; the Item/Sprite selectors
  // use the same flag combination.
  float child_height = ImGui::GetContentRegionAvail().y;
  if (ImGui::BeginChild("##ObjectGrid", ImVec2(0, child_height), false,
                        ImGuiWindowFlags_AlwaysVerticalScrollbar |
                            ImGuiWindowFlags_HorizontalScrollbar)) {

    // Iterate through all object ranges
    for (const auto& range : ranges) {
      // Section header for each type
      const ImU32 header_color = GetObjectRangeHeaderColor(range.start);
      const ImU32 header_hover_color =
          GetObjectRangeHeaderHoverColor(range.start);
      gui::StyleColorGuard section_guard(
          {{ImGuiCol_Header, ImGui::ColorConvertU32ToFloat4(header_color)},
           {ImGuiCol_HeaderHovered,
            ImGui::ColorConvertU32ToFloat4(header_hover_color)}});
      bool section_open = ImGui::CollapsingHeader(
          absl::StrFormat("%s (0x%03X-0x%03X)", range.label, range.start,
                          range.end)
              .c_str(),
          ImGuiTreeNodeFlags_DefaultOpen);

      if (!section_open)
        continue;

      int current_column = 0;

      for (int obj_id = range.start; obj_id <= range.end; ++obj_id) {
        if (!MatchesObjectFilter(obj_id, object_type_filter_)) {
          continue;
        }

        std::string full_name = zelda3::GetObjectName(obj_id);
        if (!MatchesObjectSearch(obj_id, full_name)) {
          continue;
        }

        if (current_column > 0) {
          ImGui::SameLine();
        }

        ImGui::PushID(obj_id);

        // Create selectable button for object
        bool is_selected = (selected_object_id_ == obj_id);
        ImVec2 button_size(item_size, item_size);

        if (ImGui::Selectable("", is_selected, 0, button_size)) {
          selected_object_id_ = obj_id;

          // Create and update preview object
          preview_object_ = zelda3::RoomObject(
              obj_id, 0, 0, zelda3::DefaultRoomObjectSizeForPlacement(obj_id),
              0);
          preview_object_.SetRom(rom_);
          if (game_data_ &&
              current_palette_group_id_ <
                  game_data_->palette_groups.dungeon_main.size()) {
            auto palette = game_data_->palette_groups
                               .dungeon_main[current_palette_group_id_];
            preview_palette_ = palette;
          }
          object_loaded_ = true;

          // Notify callbacks
          if (object_selected_callback_) {
            object_selected_callback_(preview_object_);
          }
        }
        const bool item_visible = ImGui::IsItemVisible();
        ImVec2 button_pos = ImGui::GetItemRectMin();
        gui::BeginRoomObjectDragSource(
            static_cast<uint16_t>(obj_id), current_room_id_, 0, 0,
            zelda3::DefaultRoomObjectSizeForPlacement(obj_id));
        // Draw object preview on the button; fall back to styled placeholder
        ImDrawList* draw_list = ImGui::GetWindowDrawList();

        // Only attempt graphical preview if enabled (performance optimization)
        bool rendered = false;
        if (item_visible && enable_object_previews_) {
          rendered = DrawObjectPreview(MakePreviewObject(obj_id), button_pos,
                                       item_size);
        }

        if (item_visible && !rendered) {
          // Draw a styled fallback with gradient background
          std::string symbol = GetObjectTypeSymbol(obj_id);
          DrawFallbackPreviewTile(
              draw_list, button_pos, item_size,
              ImGui::ColorConvertU32ToFloat4(GetObjectTypeColor(obj_id)),
              symbol.c_str());
        }

        // Draw selection border
        ImU32 border_color;
        float border_thickness;

        if (is_selected) {
          border_color = ImGui::GetColorU32(theme.dungeon_selection_primary);
          border_thickness = 3.0f;
        } else {
          border_color = ImGui::GetColorU32(theme.panel_bg_darker);
          border_thickness = 1.0f;
        }

        if (item_visible) {
          draw_list->AddRect(
              button_pos,
              ImVec2(button_pos.x + item_size, button_pos.y + item_size),
              border_color, 0.0f, 0, border_thickness);
        }

        // Get object name for display
        // Truncate name for display
        std::string display_name = full_name;
        const size_t max_display_chars =
            std::max<size_t>(7, static_cast<size_t>(item_size / 6.0f));
        if (display_name.length() > max_display_chars) {
          display_name = display_name.substr(0, max_display_chars - 2) + "..";
        }

        if (item_visible) {
          // Draw object name (smaller, above ID)
          ImVec2 name_size = ImGui::CalcTextSize(display_name.c_str());
          ImVec2 name_pos = ImVec2(button_pos.x + (item_size - name_size.x) / 2,
                                   button_pos.y + item_size - 26);
          draw_list->AddText(name_pos,
                             ImGui::GetColorU32(theme.text_secondary_gray),
                             display_name.c_str());

          // Draw object ID at bottom (hex format)
          std::string id_text = absl::StrFormat("%03X", obj_id);
          ImVec2 id_size = ImGui::CalcTextSize(id_text.c_str());
          ImVec2 id_pos = ImVec2(button_pos.x + (item_size - id_size.x) / 2,
                                 button_pos.y + item_size - id_size.y - 2);
          draw_list->AddText(id_pos, ImGui::GetColorU32(theme.text_primary),
                             id_text.c_str());
        }

        // Enhanced tooltip
        if (ImGui::IsItemHovered()) {
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

            uint32_t layout_key = (static_cast<uint32_t>(obj_id) << 16) |
                                  static_cast<uint32_t>(subtype);
            const bool can_capture_layout =
                rom_ && rooms_ && current_room_id_ >= 0 &&
                current_room_id_ < zelda3::kNumberOfRooms;
            if (can_capture_layout &&
                layout_cache_.find(layout_key) == layout_cache_.end()) {
              zelda3::ObjectTileEditor editor(rom_);
              auto& room_ref = (*rooms_)[current_room_id_];
              auto layout_or = editor.CaptureObjectLayout(
                  obj_id, room_ref, current_palette_group_);
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
  DrawCustomObjectWorkshopPopup(item_size);
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

void DungeonObjectSelector::SetCustomObjectsFolder(const std::string& folder) {
  if (custom_objects_folder_ != folder) {
    custom_objects_folder_ = folder;
    custom_objects_initialized_ = false;
    InvalidatePreviewCache();
  }
  EnsureCustomObjectsInitialized();
}

void DungeonObjectSelector::EnsureCustomObjectsInitialized() {
  if (custom_objects_initialized_) {
    return;
  }
  if (!custom_objects_folder_.empty()) {
    zelda3::CustomObjectManager::Get().Initialize(custom_objects_folder_);
    custom_objects_initialized_ = true;
  }
}

zelda3::RoomObject DungeonObjectSelector::MakePreviewObject(int obj_id) const {
  zelda3::RoomObject obj(obj_id, 0, 0,
                         zelda3::DefaultRoomObjectSizeForPlacement(obj_id), 0);
  obj.SetRom(rom_);
  obj.EnsureTilesLoaded();
  return obj;
}

void DungeonObjectSelector::InvalidatePreviewCache() {
  preview_cache_.clear();
  layout_cache_.clear();
  ++preview_cache_invalidations_;
}

bool DungeonObjectSelector::GetOrCreatePreview(const zelda3::RoomObject& object,
                                               float size,
                                               gfx::BackgroundBuffer** out) {
  if (!rom_ || !rom_->is_loaded()) {
    return false;
  }
  EnsureCustomObjectsInitialized();

  // Check if room context changed - invalidate cache if so
  if (rooms_ && current_room_id_ < static_cast<int>(rooms_->size())) {
    const auto& room = (*rooms_)[current_room_id_];
    if (!room.IsLoaded()) {
      return false;  // Can't render without loaded room
    }

    // Invalidate cache if room/palette/blockset changed
    if (current_room_id_ != cached_preview_room_id_ ||
        room.blockset() != cached_preview_blockset_ ||
        room.palette() != cached_preview_palette_) {
      InvalidatePreviewCache();
      cached_preview_room_id_ = current_room_id_;
      cached_preview_blockset_ = room.blockset();
      cached_preview_palette_ = room.palette();
    }
  } else {
    return false;
  }

  // Check if already in cache
  // Key: (object_id << 32) | (subtype << 16) | (blockset << 8) | palette
  int subtype = object.size_ & 0x1F;
  uint64_t cache_key = (static_cast<uint64_t>(object.id_) << 32) |
                       (static_cast<uint64_t>(subtype) << 16) |
                       (static_cast<uint64_t>(cached_preview_blockset_) << 8) |
                       static_cast<uint64_t>(cached_preview_palette_);

  auto it = preview_cache_.find(cache_key);
  if (it != preview_cache_.end()) {
    *out = it->second.get();
    return (*out)->bitmap().texture() != nullptr;
  }

  // Create new preview using ObjectTileEditor
  auto& room = (*rooms_)[current_room_id_];
  const uint8_t* gfx_data = room.get_gfx_buffer().data();

  zelda3::ObjectTileEditor editor(rom_);
  auto layout_or =
      editor.CaptureObjectLayout(object.id_, room, current_palette_group_);
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
    return false;
  }

  auto& bitmap = preview->bitmap();
  // Texture creation and SDL sync
  if (bitmap.surface()) {
    // Sync to surface
    SDL_LockSurface(bitmap.surface());
    memcpy(bitmap.surface()->pixels, bitmap.mutable_data().data(),
           bitmap.mutable_data().size());
    SDL_UnlockSurface(bitmap.surface());

    // Create texture
    gfx::Arena::Get().QueueTextureCommand(
        gfx::Arena::TextureCommandType::CREATE, &bitmap);
    gfx::Arena::Get().ProcessTextureQueue(nullptr);
  }

  if (!bitmap.texture()) {
    return false;
  }

  // Store in cache and return
  *out = preview.get();
  preview_cache_[cache_key] = std::move(preview);
  return true;
}

bool DungeonObjectSelector::DrawObjectPreview(const zelda3::RoomObject& object,
                                              ImVec2 top_left, float size) {
  gfx::BackgroundBuffer* preview = nullptr;
  if (!GetOrCreatePreview(object, size, &preview)) {
    return false;
  }

  // Draw the cached preview image
  auto& bitmap = preview->bitmap();
  if (!bitmap.texture()) {
    return false;
  }

  ImDrawList* draw_list = ImGui::GetWindowDrawList();
  ImVec2 bottom_right(top_left.x + size, top_left.y + size);
  draw_list->AddImage((ImTextureID)(intptr_t)bitmap.texture(), top_left,
                      bottom_right);
  return true;
}

void DungeonObjectSelector::DrawNewCustomObjectDialog() {
  const auto& theme = AgentUI::GetTheme();
  if (show_create_dialog_) {
    ImGui::OpenPopup("New Custom Object");
    show_create_dialog_ = false;
  }

  if (ImGui::BeginPopupModal("New Custom Object", nullptr,
                             ImGuiWindowFlags_AlwaysAutoResize)) {
    ImGui::Text(tr("Create a new custom dungeon object"));
    ImGui::Separator();

    // Dimensions
    ImGui::SliderInt(tr("Width (tiles)"), &create_width_, 1, 32);
    ImGui::SliderInt(tr("Height (tiles)"), &create_height_, 1, 32);

    // Object group
    const char* group_labels[] = {"0x31 - Track/Custom", "0x32 - Misc"};
    int group_index = (create_object_id_ == 0x32) ? 1 : 0;
    if (ImGui::Combo(tr("Object Group"), &group_index, group_labels,
                     IM_ARRAYSIZE(group_labels))) {
      create_object_id_ = (group_index == 1) ? 0x32 : 0x31;
      // Regenerate filename when group changes
      snprintf(create_filename_, sizeof(create_filename_),
               "custom_%02x_%02d.bin", create_object_id_,
               zelda3::CustomObjectManager::Get().GetSubtypeCount(
                   create_object_id_));
    }

    // Filename
    ImGui::InputText(tr("Filename"), create_filename_,
                     sizeof(create_filename_));

    // Validation
    bool valid = true;
    std::string error_msg;

    auto& mgr = zelda3::CustomObjectManager::Get();
    const int subtype_count = mgr.GetSubtypeCount(create_object_id_);
    if (subtype_count >= kPersistedCustomSubtypeSlots) {
      valid = false;
      error_msg = "Object group already uses all 16 persisted subtype slots";
    } else if (create_filename_[0] == '\0') {
      valid = false;
      error_msg = "Filename cannot be empty";
    } else if (!rooms_ || current_room_id_ < 0) {
      valid = false;
      error_msg = "Load a room first (needed for tile graphics)";
    } else {
      if (mgr.GetBasePath().empty()) {
        valid = false;
        error_msg = "Custom objects folder not configured in project";
      } else {
        // Check if file already exists
        auto path = std::filesystem::path(mgr.GetBasePath()) / create_filename_;
        if (std::filesystem::exists(path)) {
          valid = false;
          error_msg = "File already exists: " + std::string(create_filename_);
        }
      }
    }

    if (!error_msg.empty()) {
      ImGui::TextColored(theme.status_error, "%s", error_msg.c_str());
    }

    ImGui::Separator();

    if (!valid)
      ImGui::BeginDisabled();
    if (ImGui::Button(tr("Create"), ImVec2(120, 0))) {
      tile_editor_panel_->OpenForNewObject(
          create_width_, create_height_, create_filename_,
          static_cast<int16_t>(create_object_id_), current_room_id_, rooms_);
      ImGui::CloseCurrentPopup();
    }
    if (!valid)
      ImGui::EndDisabled();

    ImGui::SameLine();
    if (ImGui::Button(tr("Cancel"), ImVec2(120, 0))) {
      ImGui::CloseCurrentPopup();
    }

    ImGui::EndPopup();
  }
}

void DungeonObjectSelector::DrawCustomObjectWorkshopButton(int custom_count) {
  if (open_custom_workshop_popup_) {
    ImGui::OpenPopup("Custom Object Workshop");
    open_custom_workshop_popup_ = false;
  }

  if (ImGui::SmallButton(absl::StrFormat(ICON_MD_PRECISION_MANUFACTURING
                                         " Workshop (%d)",
                                         custom_count)
                             .c_str())) {
    ImGui::OpenPopup("Custom Object Workshop");
  }
  if (ImGui::IsItemHovered()) {
    ImGui::SetTooltip(tr(
        "Browse and create custom dungeon objects without cluttering the main "
        "selector."));
  }
}

void DungeonObjectSelector::DrawCustomObjectWorkshopPopup(float item_size) {
  const auto& theme = AgentUI::GetTheme();
  auto& obj_manager = zelda3::CustomObjectManager::Get();
  const std::string custom_base_path = obj_manager.GetBasePath();

  DrawNewCustomObjectDialog();

  ImGui::SetNextWindowSize(ImVec2(860.0f, 620.0f), ImGuiCond_FirstUseEver);
  if (!ImGui::BeginPopupModal("Custom Object Workshop", nullptr,
                              ImGuiWindowFlags_None)) {
    return;
  }

  ImGui::TextColored(theme.text_info,
                     ICON_MD_PRECISION_MANUFACTURING " Custom Object Workshop");
  ImGui::TextColored(
      theme.text_secondary_gray,
      tr("Create and place custom dungeon objects without mixing "
         "them into the main selector grid."));
  ImGui::Separator();

  if (ImGui::BeginTable(
          "##CustomObjectToolbar", 2,
          ImGuiTableFlags_SizingStretchProp | ImGuiTableFlags_NoPadOuterX)) {
    ImGui::TableSetupColumn("Status", ImGuiTableColumnFlags_WidthStretch, 1.5f);
    ImGui::TableSetupColumn("Actions", ImGuiTableColumnFlags_WidthStretch,
                            1.0f);
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
    if (tile_editor_panel_) {
      if (ImGui::Button(ICON_MD_ADD " New Custom Object", ImVec2(-1, 0))) {
        show_create_dialog_ = true;
        std::snprintf(create_filename_, sizeof(create_filename_),
                      "custom_%02x_%02d.bin", create_object_id_,
                      zelda3::CustomObjectManager::Get().GetSubtypeCount(
                          create_object_id_));
      }
      if (ImGui::IsItemHovered()) {
        ImGui::SetTooltip(tr("Create a new custom object from scratch"));
      }
    }
    if (ImGui::Button(ICON_MD_REFRESH " Reload Workshop", ImVec2(-1, 0))) {
      obj_manager.ReloadAll();
      InvalidatePreviewCache();
    }
    if (ImGui::IsItemHovered()) {
      ImGui::SetTooltip(
          tr("Reload custom object binaries and refresh their previews"));
    }
    ImGui::EndTable();
  }

  ImGui::TextColored(
      theme.text_secondary_gray, ICON_MD_INFO
      " Corner overrides map to 0x31 subtypes 02, 04, 03, and 05.");
  ImGui::Spacing();

  if (ImGui::BeginChild("##CustomObjectGrid",
                        ImVec2(0, -ImGui::GetFrameHeightWithSpacing() - 4.0f),
                        false,
                        ImGuiWindowFlags_AlwaysVerticalScrollbar |
                            ImGuiWindowFlags_HorizontalScrollbar)) {
    const float item_spacing = 6.0f;
    const int columns = std::max(
        1, static_cast<int>((ImGui::GetContentRegionAvail().x - item_spacing) /
                            (item_size + item_spacing)));
    int custom_col = 0;
    for (int obj_id : {0x31, 0x32}) {
      if (!MatchesObjectFilter(obj_id, object_type_filter_)) {
        continue;
      }
      const int subtype_count = std::min(obj_manager.GetSubtypeCount(obj_id),
                                         kPersistedCustomSubtypeSlots);
      for (int subtype = 0; subtype < subtype_count; ++subtype) {
        std::string base_name = zelda3::GetObjectName(obj_id);
        std::string subtype_name =
            absl::StrFormat("%s %02X", base_name.c_str(), subtype);
        if (!MatchesObjectSearch(obj_id, subtype_name, subtype)) {
          continue;
        }

        if (custom_col > 0) {
          ImGui::SameLine();
        }

        ImGui::PushID(obj_id * 1000 + subtype);

        bool is_selected =
            (selected_object_id_ == obj_id && preview_object_.size_ == subtype);
        ImVec2 button_size(item_size, item_size);

        if (ImGui::Selectable("", is_selected, 0, button_size)) {
          SelectObject(obj_id, subtype);
          ImGui::CloseCurrentPopup();
        }
        const bool item_visible = ImGui::IsItemVisible();
        ImVec2 button_pos = ImGui::GetItemRectMin();
        gui::BeginRoomObjectDragSource(
            static_cast<uint16_t>(obj_id), current_room_id_, 0, 0,
            zelda3::CanonicalRoomObjectSize(obj_id,
                                            static_cast<uint8_t>(subtype)));
        ImDrawList* draw_list = ImGui::GetWindowDrawList();

        bool rendered = false;
        if (item_visible && enable_object_previews_) {
          auto temp_obj = MakePreviewObject(obj_id);
          temp_obj.size_ = zelda3::CanonicalRoomObjectSize(
              obj_id, static_cast<uint8_t>(subtype));
          rendered = DrawObjectPreview(temp_obj, button_pos, item_size);
        }

        if (item_visible && !rendered) {
          std::string sub_text = absl::StrFormat("%02X", subtype);
          DrawFallbackPreviewTile(draw_list, button_pos, item_size,
                                  theme.status_success, sub_text.c_str());
        }

        ImU32 border_color =
            is_selected ? ImGui::GetColorU32(theme.dungeon_selection_primary)
                        : ImGui::GetColorU32(theme.panel_bg_darker);
        float border_thickness = is_selected ? 3.0f : 1.0f;
        if (item_visible) {
          draw_list->AddRect(
              button_pos,
              ImVec2(button_pos.x + item_size, button_pos.y + item_size),
              border_color, 0.0f, 0, border_thickness);

          std::string id_text = absl::StrFormat("%02X:%02X", obj_id, subtype);
          ImVec2 id_size = ImGui::CalcTextSize(id_text.c_str());
          ImVec2 id_pos = ImVec2(button_pos.x + (item_size - id_size.x) / 2,
                                 button_pos.y + item_size - id_size.y - 2);
          draw_list->AddText(id_pos, ImGui::GetColorU32(theme.text_primary),
                             id_text.c_str());
        }

        if (ImGui::IsItemHovered()) {
          gui::StyleColorGuard tooltip_guard(
              {{ImGuiCol_PopupBg, theme.panel_bg_color},
               {ImGuiCol_Border, theme.panel_border_color}});
          if (ImGui::BeginTooltip()) {
            const std::string filename =
                obj_manager.ResolveFilename(obj_id, subtype);
            const bool has_base = !custom_base_path.empty();
            std::filesystem::path full_path =
                has_base ? (std::filesystem::path(custom_base_path) / filename)
                         : std::filesystem::path();
            const bool file_exists = has_base && !filename.empty() &&
                                     std::filesystem::exists(full_path);

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
            if (!has_base) {
              ImGui::TextColored(theme.text_warning_yellow,
                                 tr("Folder not configured in project"));
            } else if (file_exists) {
              ImGui::TextColored(theme.status_success, tr("File found"));
            } else {
              ImGui::TextColored(theme.status_error, tr("File missing: %s"),
                                 full_path.string().c_str());
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
                                 tr("Also used by corner override %s"),
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
  if (ImGui::Button(ICON_MD_CLOSE " Close")) {
    ImGui::CloseCurrentPopup();
  }

  ImGui::EndPopup();
}

}  // namespace yaze::editor
