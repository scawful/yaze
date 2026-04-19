// Related header
#include "dungeon_object_selector.h"
#include "absl/strings/str_format.h"

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
#include "app/gui/core/icons.h"
#include "app/gui/core/style_guard.h"
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

ImU32 DungeonObjectSelector::GetObjectTypeColor(int object_id) {
  const auto& theme = AgentUI::GetTheme();

  // Type 3 objects (0xF80-0xFFF) - Special room features
  if (object_id >= 0xF80) {
    if (object_id >= 0xF80 && object_id <= 0xF8F) {
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

  // Type 2 objects (0x100-0x141) - Torches, blocks, switches
  if (object_id >= 0x100 && object_id < 0x200) {
    if (object_id >= 0x100 && object_id <= 0x10F) {
      return IM_COL32(255, 150, 50, 255);  // Orange for torches
    } else if (object_id >= 0x110 && object_id <= 0x11F) {
      return IM_COL32(150, 150, 200, 255);  // Blue-gray for blocks
    } else if (object_id >= 0x120 && object_id <= 0x12F) {
      return ImGui::ColorConvertFloat4ToU32(
          theme.status_success);  // Green for switches
    } else if (object_id >= 0x130 && object_id <= 0x13F) {
      return ImGui::GetColorU32(theme.selection_primary);  // Yellow for stairs
    } else {
      return IM_COL32(180, 180, 180, 255);  // Gray for other Type 2
    }
  }

  // Type 1 objects (0x00-0xFF) - Base room objects
  if (object_id >= 0x10 && object_id <= 0x1F) {
    return ImGui::GetColorU32(theme.dungeon_object_wall);  // Gray for walls
  } else if (object_id >= 0x20 && object_id <= 0x2F) {
    return ImGui::GetColorU32(theme.dungeon_object_floor);  // Brown for floors
  } else if (object_id == 0xF9 || object_id == 0xFA) {
    return ImGui::GetColorU32(theme.item_color);  // Gold for chests
  } else if (object_id >= 0x17 && object_id <= 0x1E) {
    return ImGui::GetColorU32(theme.dungeon_object_floor);  // Brown for doors
  } else if (object_id == 0x2F || object_id == 0x2B) {
    return ImGui::GetColorU32(
        theme.dungeon_object_pot);  // Saddle brown for pots
  } else if (object_id >= 0x30 && object_id <= 0x3F) {
    return ImGui::GetColorU32(
        theme.dungeon_object_decoration);  // Dim gray for decorations
  } else if (object_id >= 0x00 && object_id <= 0x0F) {
    return IM_COL32(120, 120, 180, 255);  // Blue-gray for corners
  } else {
    return ImGui::GetColorU32(theme.dungeon_object_default);  // Default gray
  }
}

std::string DungeonObjectSelector::GetObjectTypeSymbol(int object_id) {
  // Type 3 objects (0xF80-0xFFF) - Special room features
  if (object_id >= 0xF80) {
    if (object_id >= 0xF80 && object_id <= 0xF8F) {
      return "L";  // Layer
    } else if (object_id >= 0xF90 && object_id <= 0xF9F) {
      return "D";  // Door indicator
    } else {
      return "S";  // Special
    }
  }

  // Type 2 objects (0x100-0x141) - Torches, blocks, switches
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

  // Type 1 objects (0x00-0xFF) - Base room objects
  if (object_id >= 0x10 && object_id <= 0x1F) {
    return "|";  // Wall
  } else if (object_id >= 0x20 && object_id <= 0x2F) {
    return "_";  // Floor
  } else if (object_id == 0xF9 || object_id == 0xFA) {
    return "C";  // Chest
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
  selected_object_id_ = obj_id;

  // Create and update preview object
  uint8_t size = 0x12;
  if (subtype >= 0) {
    size = static_cast<uint8_t>(subtype & 0x1F);
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

  // Object ranges: Type 1 (0x00-0xFF), Type 2 (0x100-0x141), Type 3 (0xF80-0xFFF)
  struct ObjectRange {
    int start;
    int end;
    const char* label;
    ImU32 header_color;
  };
  static const ObjectRange ranges[] = {
      {0x00, 0xFF, "Type 1", IM_COL32(80, 120, 180, 255)},
      {0x100, 0x141, "Type 2", IM_COL32(120, 80, 180, 255)},
      {0xF80, 0xFFF, "Type 3", IM_COL32(180, 120, 80, 255)},
  };

  // Total object count
  int total_objects =
      (0xFF - 0x00 + 1) + (0x141 - 0x100 + 1) + (0xFFF - 0xF80 + 1);

  ImGui::TextDisabled("%d objects", total_objects);
  ImGui::SameLine();
  ImGui::Checkbox(ICON_MD_IMAGE " Tile thumbnails", &enable_object_previews_);
  if (ImGui::IsItemHovered()) {
    ImGui::SetTooltip(
        "Show rendered object thumbnails in the selector.\n"
        "Requires a room to be loaded and may cost some performance.");
  }

  if (selected_object_id_ >= 0) {
    ImGui::TextColored(theme.text_info, ICON_MD_LABEL " Current: 0x%03X %s",
                       selected_object_id_,
                       zelda3::GetObjectName(selected_object_id_).c_str());
  } else {
    ImGui::TextColored(theme.text_secondary_gray,
                       "Tip: click once to queue placement, double-click to "
                       "inspect the draw routine.");
  }

  // Search + category filter
  ImGui::SetNextItemWidth(-1.0f);
  ImGui::InputTextWithHint(
      "##ObjectSearch", ICON_MD_SEARCH " Filter by name or hex...",
      object_search_buffer_, sizeof(object_search_buffer_));
  static const char* kFilterLabels[] = {"All",   "Walls", "Floors", "Chests",
                                        "Doors", "Decor", "Stairs"};
  ImGui::SetNextItemWidth(170.0f);
  ImGui::Combo("##ObjectFilterType", &object_type_filter_, kFilterLabels,
               IM_ARRAYSIZE(kFilterLabels));
  ImGui::SameLine();
  if (gui::ThemedButton(ICON_MD_CLEAR " Reset")) {
    object_search_buffer_[0] = '\0';
    object_type_filter_ = 0;
  }
  if (ImGui::IsItemHovered()) {
    gui::ThemedTooltip("Clear search and category filter");
  }

  // Create asset browser-style grid
  const float item_size = 72.0f;
  const float item_spacing = 6.0f;
  const int columns = std::max(
      1, static_cast<int>((ImGui::GetContentRegionAvail().x - item_spacing) /
                          (item_size + item_spacing)));

  // Scrollable child region for grid - use all available space
  float child_height = ImGui::GetContentRegionAvail().y;
  if (ImGui::BeginChild("##ObjectGrid", ImVec2(0, child_height), false,
                        ImGuiWindowFlags_AlwaysVerticalScrollbar)) {

    // Iterate through all object ranges
    for (const auto& range : ranges) {
      // Section header for each type
      gui::StyleColorGuard section_guard(
          {{ImGuiCol_Header,
            ImGui::ColorConvertU32ToFloat4(range.header_color)},
           {ImGuiCol_HeaderHovered,
            ImGui::ColorConvertU32ToFloat4(
                IM_COL32((range.header_color & 0xFF) + 30,
                         ((range.header_color >> 8) & 0xFF) + 30,
                         ((range.header_color >> 16) & 0xFF) + 30, 255))}});
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

        if (ImGui::Selectable("", is_selected,
                              ImGuiSelectableFlags_AllowDoubleClick,
                              button_size)) {
          selected_object_id_ = obj_id;

          // Create and update preview object
          preview_object_ = zelda3::RoomObject(obj_id, 0, 0, 0x12, 0);
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

          // Handle double-click to open static object editor
          if (ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left)) {
            if (object_double_click_callback_) {
              object_double_click_callback_(obj_id);
            }
          }
        }

        // Draw object preview on the button; fall back to styled placeholder
        ImVec2 button_pos = ImGui::GetItemRectMin();
        ImDrawList* draw_list = ImGui::GetWindowDrawList();

        // Only attempt graphical preview if enabled (performance optimization)
        bool rendered = false;
        if (enable_object_previews_) {
          rendered = DrawObjectPreview(MakePreviewObject(obj_id), button_pos,
                                       item_size);
        }

        if (!rendered) {
          // Draw a styled fallback with gradient background
          ImU32 obj_color = GetObjectTypeColor(obj_id);
          ImU32 darker_color = IM_COL32((obj_color & 0xFF) * 0.6f,
                                        ((obj_color >> 8) & 0xFF) * 0.6f,
                                        ((obj_color >> 16) & 0xFF) * 0.6f, 255);

          // Gradient background
          draw_list->AddRectFilledMultiColor(
              button_pos,
              ImVec2(button_pos.x + item_size, button_pos.y + item_size),
              darker_color, darker_color, obj_color, obj_color);

          // Draw object type symbol in center
          std::string symbol = GetObjectTypeSymbol(obj_id);
          ImVec2 symbol_size = ImGui::CalcTextSize(symbol.c_str());
          ImVec2 symbol_pos(
              button_pos.x + (item_size - symbol_size.x) / 2,
              button_pos.y + (item_size - symbol_size.y) / 2 - 10);
          draw_list->AddText(symbol_pos, IM_COL32(255, 255, 255, 180),
                             symbol.c_str());
        }

        // Draw border with special highlight for static editor object
        bool is_static_editor_obj = (obj_id == static_editor_object_id_);
        ImU32 border_color;
        float border_thickness;

        if (is_static_editor_obj) {
          border_color = IM_COL32(0, 200, 255, 255);
          border_thickness = 3.0f;
        } else if (is_selected) {
          border_color = ImGui::GetColorU32(theme.dungeon_selection_primary);
          border_thickness = 3.0f;
        } else {
          border_color = ImGui::GetColorU32(theme.panel_bg_darker);
          border_thickness = 1.0f;
        }

        draw_list->AddRect(
            button_pos,
            ImVec2(button_pos.x + item_size, button_pos.y + item_size),
            border_color, 0.0f, 0, border_thickness);

        // Static editor indicator icon
        if (is_static_editor_obj) {
          ImVec2 icon_pos(button_pos.x + item_size - 14, button_pos.y + 2);
          draw_list->AddCircleFilled(ImVec2(icon_pos.x + 6, icon_pos.y + 6), 6,
                                     IM_COL32(0, 200, 255, 200));
          draw_list->AddText(icon_pos, IM_COL32(255, 255, 255, 255), "i");
        }

        // Get object name for display
        // Truncate name for display
        std::string display_name = full_name;
        const size_t kMaxDisplayChars = 12;
        if (display_name.length() > kMaxDisplayChars) {
          display_name = display_name.substr(0, kMaxDisplayChars - 2) + "..";
        }

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

        // Enhanced tooltip
        if (ImGui::IsItemHovered()) {
          gui::StyleColorGuard tooltip_guard(
              {{ImGuiCol_PopupBg, theme.panel_bg_color},
               {ImGuiCol_Border, theme.panel_border_color}});

          if (ImGui::BeginTooltip()) {
            ImGui::TextColored(theme.selection_primary, "Object 0x%03X",
                               obj_id);
            ImGui::Text("%s", full_name.c_str());
            int subtype = zelda3::GetObjectSubtype(obj_id);
            ImGui::TextColored(theme.text_secondary_gray, "Subtype %d",
                               subtype);
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
              ImGui::TextColored(theme.status_success, "Tiles: %zu",
                                 layout.cells.size());

              if (can_capture_layout) {
                auto& room_ref = (*rooms_)[current_room_id_];
                zelda3::ObjectDrawer drawer(rom_, current_room_id_,
                                            room_ref.get_gfx_buffer().data());
                int rid = drawer.GetDrawRoutineId(obj_id);
                ImGui::TextColored(theme.status_active, "Draw Routine: %d",
                                   rid);
              }

              ImGui::Text("Layout:");
              ImDrawList* tooltip_draw_list = ImGui::GetWindowDrawList();
              ImVec2 grid_start = ImGui::GetCursorScreenPos();
              float cell_size = 4.0f;
              for (const auto& cell : layout.cells) {
                ImVec2 p1(grid_start.x + cell.rel_x * cell_size,
                          grid_start.y + cell.rel_y * cell_size);
                ImVec2 p2(p1.x + cell_size, p1.y + cell_size);
                tooltip_draw_list->AddRectFilled(p1, p2,
                                                 IM_COL32(200, 200, 200, 255));
                tooltip_draw_list->AddRect(p1, p2, IM_COL32(50, 50, 50, 255));
              }
              ImGui::Dummy(ImVec2(layout.bounds_width * cell_size,
                                  layout.bounds_height * cell_size));
            }

            ImGui::Separator();
            ImGui::TextColored(ImVec4(0.7f, 0.7f, 0.7f, 1.0f),
                               "Click to select for placement");
            ImGui::TextColored(ImVec4(0.5f, 0.8f, 1.0f, 1.0f),
                               "Double-click to view details");
            ImGui::EndTooltip();
          }
        }

        ImGui::PopID();

        current_column = (current_column + 1) % columns;
      }  // end object loop
    }  // end range loop

    EnsureCustomObjectsInitialized();

    ImGui::Spacing();
    ImGui::Separator();
    ImGui::Spacing();

    // Custom Objects Section
    gui::StyleColorGuard custom_hdr_guard(
        {{ImGuiCol_Header,
          ImGui::ColorConvertU32ToFloat4(IM_COL32(100, 180, 120, 255))},
         {ImGuiCol_HeaderHovered,
          ImGui::ColorConvertU32ToFloat4(IM_COL32(130, 210, 150, 255))}});
    bool custom_open = ImGui::CollapsingHeader("Custom Objects",
                                               ImGuiTreeNodeFlags_DefaultOpen);

    if (custom_open) {
      ImGui::TextColored(theme.text_secondary_gray,
                         "Create, reload, and browse custom object variants "
                         "separately from the vanilla selector.");
      // "+ New Custom Object" button
      if (tile_editor_panel_) {
        if (ImGui::SmallButton(ICON_MD_ADD " New Custom Object")) {
          show_create_dialog_ = true;
          // Auto-generate a default filename
          snprintf(create_filename_, sizeof(create_filename_),
                   "custom_%02x_%02d.bin", create_object_id_,
                   zelda3::CustomObjectManager::Get().GetSubtypeCount(
                       create_object_id_));
        }
        if (ImGui::IsItemHovered()) {
          ImGui::SetTooltip("Create a new custom object from scratch");
        }
      }

      auto& obj_manager = zelda3::CustomObjectManager::Get();
      const std::string custom_base_path = obj_manager.GetBasePath();
      if (custom_base_path.empty()) {
        ImGui::TextColored(theme.text_secondary_gray,
                           "Custom objects folder: not configured");
      } else {
        ImGui::Text("Custom objects folder: %s", custom_base_path.c_str());
        if (ImGui::IsItemHovered()) {
          ImGui::SetTooltip("%s", custom_base_path.c_str());
        }
        ImGui::SameLine();
        if (ImGui::SmallButton(ICON_MD_REFRESH " Reload")) {
          obj_manager.ReloadAll();
          InvalidatePreviewCache();
        }
        if (ImGui::IsItemHovered()) {
          ImGui::SetTooltip(
              "Reload custom object binaries and refresh previews");
        }
      }
      ImGui::TextColored(theme.text_secondary_gray,
                         "Corner overrides: 0x100/0x101/0x102/0x103 use 0x31 "
                         "subtypes 02/04/03/05");

      DrawNewCustomObjectDialog();

      int custom_col = 0;

      // Initialize if needed (hacky lazy init if drawer hasn't done it yet)
      // Ideally should be initialized by system.
      // We'll skip init here and assume ObjectDrawer did it or will do it.
      // But we need counts. If uninitialized, counts might be wrong?
      // GetSubtypeCount checks static lists, so it's safe even if not fully init with paths.

      for (int obj_id : {0x31, 0x32}) {
        if (!MatchesObjectFilter(obj_id, object_type_filter_)) {
          continue;
        }
        int subtype_count = obj_manager.GetSubtypeCount(obj_id);
        for (int subtype = 0; subtype < subtype_count; ++subtype) {
          std::string base_name = zelda3::GetObjectName(obj_id);
          std::string subtype_name =
              absl::StrFormat("%s %02X", base_name.c_str(), subtype);
          if (!MatchesObjectSearch(obj_id, subtype_name, subtype)) {
            continue;
          }

          if (custom_col > 0)
            ImGui::SameLine();

          ImGui::PushID(obj_id * 1000 + subtype);

          bool is_selected = (selected_object_id_ == obj_id &&
                              (preview_object_.size_ & 0x1F) == subtype);
          ImVec2 button_size(item_size, item_size);

          if (ImGui::Selectable("", is_selected,
                                ImGuiSelectableFlags_AllowDoubleClick,
                                button_size)) {
            SelectObject(obj_id, subtype);

            if (ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left)) {
              if (object_double_click_callback_)
                object_double_click_callback_(obj_id);
            }
          }

          // Draw Preview
          ImVec2 button_pos = ImGui::GetItemRectMin();
          ImDrawList* draw_list = ImGui::GetWindowDrawList();

          bool rendered = false;
          // Native preview requires loaded ROM and correct pathing, might fail if not init.
          // But we can try constructing a temp object with correct subtype.
          if (enable_object_previews_) {
            auto temp_obj = MakePreviewObject(obj_id);
            temp_obj.size_ = subtype;
            rendered = DrawObjectPreview(temp_obj, button_pos, item_size);
          }

          if (!rendered) {
            // Fallback visuals
            ImU32 obj_color = IM_COL32(100, 180, 120, 255);
            ImU32 darker_color = IM_COL32(60, 100, 70, 255);

            draw_list->AddRectFilledMultiColor(
                button_pos,
                ImVec2(button_pos.x + item_size, button_pos.y + item_size),
                darker_color, darker_color, obj_color, obj_color);

            std::string symbol = (obj_id == 0x31) ? "Trk" : "Cus";
            // Subtype
            std::string sub_text = absl::StrFormat("%02X", subtype);
            ImVec2 sub_size = ImGui::CalcTextSize(sub_text.c_str());
            ImVec2 sub_pos(button_pos.x + (item_size - sub_size.x) / 2,
                           button_pos.y + (item_size - sub_size.y) / 2);
            draw_list->AddText(sub_pos, IM_COL32(255, 255, 255, 220),
                               sub_text.c_str());
          }

          // Border
          bool is_static_editor_obj = (obj_id == static_editor_object_id_ &&
                                       static_editor_object_id_ != -1);
          // Static editor doesn't track subtype currently, so highlighting all subtypes of 0x31 is correct
          // if we are editing 0x31 generic. But maybe we only edit specific subtype?
          // Static editor usually edits the code/logic common to ID.
          ImU32 border_color =
              is_selected ? ImGui::GetColorU32(theme.dungeon_selection_primary)
                          : ImGui::GetColorU32(theme.panel_bg_darker);
          float border_thickness = is_selected ? 3.0f : 1.0f;
          draw_list->AddRect(
              button_pos,
              ImVec2(button_pos.x + item_size, button_pos.y + item_size),
              border_color, 0.0f, 0, border_thickness);

          // Name/ID
          std::string id_text = absl::StrFormat("%02X:%02X", obj_id, subtype);
          ImVec2 id_size = ImGui::CalcTextSize(id_text.c_str());
          ImVec2 id_pos = ImVec2(button_pos.x + (item_size - id_size.x) / 2,
                                 button_pos.y + item_size - id_size.y - 2);
          draw_list->AddText(id_pos, ImGui::GetColorU32(theme.text_primary),
                             id_text.c_str());

          if (ImGui::IsItemHovered()) {
            gui::StyleColorGuard tooltip_guard(
                {{ImGuiCol_PopupBg, theme.panel_bg_color},
                 {ImGuiCol_Border, theme.panel_border_color}});
            if (ImGui::BeginTooltip()) {
              const std::string filename =
                  obj_manager.ResolveFilename(obj_id, subtype);
              const bool has_base = !custom_base_path.empty();
              std::filesystem::path full_path =
                  has_base
                      ? (std::filesystem::path(custom_base_path) / filename)
                      : std::filesystem::path();
              const bool file_exists = has_base && !filename.empty() &&
                                       std::filesystem::exists(full_path);

              ImGui::TextColored(theme.selection_primary, "Custom 0x%02X:%02X",
                                 obj_id, subtype);
              ImGui::Text("%s", subtype_name.c_str());
              ImGui::Separator();
              ImGui::Text("File: %s",
                          filename.empty() ? "(unmapped)" : filename.c_str());
              if (!has_base) {
                ImGui::TextColored(theme.text_warning_yellow,
                                   "Folder not configured in project");
              } else if (file_exists) {
                ImGui::TextColored(theme.status_success, "File found");
              } else {
                ImGui::TextColored(theme.status_error, "File missing: %s",
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
                                   "Also used by corner override %s",
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
  }

  ImGui::EndChild();
}

bool DungeonObjectSelector::MatchesObjectFilter(int obj_id, int filter_type) {
  switch (filter_type) {
    case 1:  // Walls
      return obj_id >= 0x10 && obj_id <= 0x1F;
    case 2:  // Floors
      return obj_id >= 0x20 && obj_id <= 0x2F;
    case 3:  // Chests
      return obj_id == 0xF9 || obj_id == 0xFA;
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
  zelda3::RoomObject obj(obj_id, 0, 0, 0x12, 0);
  obj.SetRom(rom_);
  obj.EnsureTilesLoaded();
  return obj;
}

void DungeonObjectSelector::InvalidatePreviewCache() {
  preview_cache_.clear();
  layout_cache_.clear();
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
  if (show_create_dialog_) {
    ImGui::OpenPopup("New Custom Object");
    show_create_dialog_ = false;
  }

  if (ImGui::BeginPopupModal("New Custom Object", nullptr,
                             ImGuiWindowFlags_AlwaysAutoResize)) {
    ImGui::Text("Create a new custom dungeon object");
    ImGui::Separator();

    // Dimensions
    ImGui::SliderInt("Width (tiles)", &create_width_, 1, 32);
    ImGui::SliderInt("Height (tiles)", &create_height_, 1, 32);

    // Object group
    const char* group_labels[] = {"0x31 - Track/Custom", "0x32 - Misc"};
    int group_index = (create_object_id_ == 0x32) ? 1 : 0;
    if (ImGui::Combo("Object Group", &group_index, group_labels,
                     IM_ARRAYSIZE(group_labels))) {
      create_object_id_ = (group_index == 1) ? 0x32 : 0x31;
      // Regenerate filename when group changes
      snprintf(create_filename_, sizeof(create_filename_),
               "custom_%02x_%02d.bin", create_object_id_,
               zelda3::CustomObjectManager::Get().GetSubtypeCount(
                   create_object_id_));
    }

    // Filename
    ImGui::InputText("Filename", create_filename_, sizeof(create_filename_));

    // Validation
    bool valid = true;
    std::string error_msg;

    if (create_filename_[0] == '\0') {
      valid = false;
      error_msg = "Filename cannot be empty";
    } else if (!rooms_ || current_room_id_ < 0) {
      valid = false;
      error_msg = "Load a room first (needed for tile graphics)";
    } else {
      auto& mgr = zelda3::CustomObjectManager::Get();
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
      ImGui::TextColored(ImVec4(1.0f, 0.4f, 0.4f, 1.0f), "%s",
                         error_msg.c_str());
    }

    ImGui::Separator();

    if (!valid)
      ImGui::BeginDisabled();
    if (ImGui::Button("Create", ImVec2(120, 0))) {
      tile_editor_panel_->OpenForNewObject(
          create_width_, create_height_, create_filename_,
          static_cast<int16_t>(create_object_id_), current_room_id_, rooms_);
      ImGui::CloseCurrentPopup();
    }
    if (!valid)
      ImGui::EndDisabled();

    ImGui::SameLine();
    if (ImGui::Button("Cancel", ImVec2(120, 0))) {
      ImGui::CloseCurrentPopup();
    }

    ImGui::EndPopup();
  }
}

}  // namespace yaze::editor
