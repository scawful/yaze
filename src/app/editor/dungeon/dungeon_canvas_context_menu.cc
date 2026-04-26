#include "app/editor/dungeon/dungeon_canvas_viewer.h"

#include <optional>
#include <string>
#include <utility>

#include "absl/strings/str_format.h"
#include "app/gui/core/icons.h"
#include "imgui/imgui.h"
#include "util/log.h"
#include "zelda3/resource_labels.h"
#include "zelda3/sprite/sprite.h"

namespace yaze::editor {

void DungeonCanvasViewer::PopulateCanvasContextMenu(int room_id) {
  canvas_.ClearContextMenuItems();
  AddInteractionContextMenuItems(room_id);
  AddLoadedRoomContextMenuItems(room_id);
}

void DungeonCanvasViewer::AddInteractionContextMenuItems(int room_id) {
  if (!object_interaction_enabled_) {
    return;
  }

  canvas_.AddContextMenuItem(BuildInsertContextMenu());
  for (auto& item : BuildSelectionContextMenuItems(room_id)) {
    canvas_.AddContextMenuItem(std::move(item));
  }
}

void DungeonCanvasViewer::AddLoadedRoomContextMenuItems(int room_id) {
  if (!rooms_ || !rom_ || !rom_->is_loaded()) {
    return;
  }

  canvas_.AddContextMenuItem(BuildRoomContextMenu(room_id));
  canvas_.AddContextMenuItem(BuildReportContextMenu(room_id));
  if (auto open_menu = BuildOpenContextMenu(); open_menu.has_value()) {
    canvas_.AddContextMenuItem(std::move(*open_menu));
  }
  canvas_.AddContextMenuItem(BuildCopyExportContextMenu(room_id));
  canvas_.AddContextMenuItem(BuildLayerVisibilityContextMenu(room_id));
  canvas_.AddContextMenuItem(BuildOverlayContextMenu());
  canvas_.AddContextMenuItem(BuildDebugContextMenu(room_id));
}

gui::CanvasMenuItem DungeonCanvasViewer::BuildInsertContextMenu() {
  auto enabled_if = [](bool enabled) {
    return [enabled]() {
      return enabled;
    };
  };

  gui::CanvasMenuItem insert_menu;
  insert_menu.label = "Insert";
  insert_menu.icon = ICON_MD_ADD_CIRCLE;

  gui::CanvasMenuItem insert_object_item("Object...", ICON_MD_WIDGETS,
                                         [this]() {
                                           if (show_object_panel_callback_) {
                                             show_object_panel_callback_();
                                           }
                                         });
  insert_object_item.enabled_condition =
      enabled_if(show_object_panel_callback_ != nullptr);
  insert_menu.subitems.push_back(std::move(insert_object_item));

  gui::CanvasMenuItem insert_sprite_item("Sprite...", ICON_MD_PERSON, [this]() {
    if (show_sprite_panel_callback_) {
      show_sprite_panel_callback_();
    }
  });
  insert_sprite_item.enabled_condition =
      enabled_if(show_sprite_panel_callback_ != nullptr);
  insert_menu.subitems.push_back(std::move(insert_sprite_item));

  gui::CanvasMenuItem insert_item_item("Item...", ICON_MD_INVENTORY, [this]() {
    if (show_item_panel_callback_) {
      show_item_panel_callback_();
    }
  });
  insert_item_item.enabled_condition =
      enabled_if(show_item_panel_callback_ != nullptr);
  insert_menu.subitems.push_back(std::move(insert_item_item));

  const bool door_mode = object_interaction_.IsDoorPlacementActive();
  insert_menu.subitems.emplace_back(
      door_mode ? "Cancel Door Placement" : "Door (Normal)", ICON_MD_DOOR_FRONT,
      [this, door_mode]() {
        object_interaction_.SetDoorPlacementMode(!door_mode,
                                                 zelda3::DoorType::NormalDoor);
      });

  return insert_menu;
}

std::optional<zelda3::RoomObject>
DungeonCanvasViewer::GetObjectUnderContextCursor(int room_id) {
  if (!rooms_ || room_id < 0 || room_id >= zelda3::kNumberOfRooms) {
    return std::nullopt;
  }

  const ImGuiIO& io = ImGui::GetIO();
  const ImVec2 canvas_pos = canvas_.zero_point();
  const int canvas_x = static_cast<int>(io.MousePos.x - canvas_pos.x);
  const int canvas_y = static_cast<int>(io.MousePos.y - canvas_pos.y);
  const auto hovered_index = object_interaction_.entity_coordinator()
                                 .tile_handler()
                                 .GetEntityAtPosition(canvas_x, canvas_y);
  if (!hovered_index.has_value()) {
    return std::nullopt;
  }

  const auto& objects = (*rooms_)[room_id].GetTileObjects();
  if (*hovered_index >= objects.size()) {
    return std::nullopt;
  }
  return objects[*hovered_index];
}

std::vector<gui::CanvasMenuItem>
DungeonCanvasViewer::BuildSelectionContextMenuItems(int room_id) {
  auto& interaction = object_interaction_;
  const auto selected = interaction.GetSelectedObjectIndices();
  const bool has_selection = !selected.empty();
  const bool single_selection = selected.size() == 1;
  const bool group_selection = selected.size() > 1;
  const bool has_clipboard = interaction.HasClipboardData();
  const bool placing_object = interaction.IsObjectLoaded();
  const bool valid_room =
      rooms_ && room_id >= 0 && room_id < zelda3::kNumberOfRooms;
  const bool room_has_objects =
      valid_room && !(*rooms_)[room_id].GetTileObjects().empty();

  std::vector<gui::CanvasMenuItem> items;

  bool can_edit_selected_graphics = false;
  std::function<void()> edit_selected_graphics;
  if (single_selection && rooms_) {
    auto& room = (*rooms_)[room_id];
    const auto& objects = room.GetTileObjects();
    if (selected[0] < objects.size()) {
      const auto object = objects[selected[0]];
      can_edit_selected_graphics = edit_graphics_callback_ != nullptr ||
                                   show_room_graphics_callback_ != nullptr;
      edit_selected_graphics = [this, room_id, object]() {
        if (edit_graphics_callback_) {
          edit_graphics_callback_(room_id, object);
        } else if (show_room_graphics_callback_) {
          show_room_graphics_callback_();
        }
      };
    }
  }

  auto enabled_if = [](bool enabled) {
    return [enabled]() {
      return enabled;
    };
  };
  auto paste_item = [&]() {
    gui::CanvasMenuItem item(
        "Paste", ICON_MD_CONTENT_PASTE,
        [&interaction]() { interaction.HandlePasteObjects(); }, "Ctrl+V");
    item.enabled_condition = enabled_if(has_clipboard);
    return item;
  };
  auto disabled_item = [](const char* label, const char* icon,
                          const char* shortcut = nullptr) {
    gui::CanvasMenuItem item;
    item.label = label;
    item.icon = icon;
    if (shortcut != nullptr) {
      item.shortcut = shortcut;
    }
    item.enabled_condition = []() {
      return false;
    };
    return item;
  };
  auto layer_item = [&interaction](const char* label, int layer,
                                   const char* shortcut) {
    return gui::CanvasMenuItem(
        label, ICON_MD_LAYERS,
        [&interaction, layer]() { interaction.SendSelectedToLayer(layer); },
        shortcut);
  };

  const auto context_sample_object = GetObjectUnderContextCursor(room_id);
  std::optional<size_t> sample_item_index;
  if (context_sample_object.has_value()) {
    sample_item_index = items.size();
    items.emplace_back("Sample Object", ICON_MD_COLORIZE,
                       [this, object = *context_sample_object]() {
                         SetPreviewObject(object);
                       });
  }

  if (has_selection) {
    items.emplace_back(
        "Cut", ICON_MD_CONTENT_CUT,
        [&interaction]() {
          interaction.HandleCopySelected();
          interaction.HandleDeleteSelected();
        },
        "Ctrl+X");
    items.emplace_back(
        "Copy", ICON_MD_CONTENT_COPY,
        [&interaction]() { interaction.HandleCopySelected(); }, "Ctrl+C");
    items.push_back(paste_item());
    items.emplace_back(
        "Delete", ICON_MD_DELETE,
        [&interaction]() { interaction.HandleDeleteSelected(); }, "Del");

    if (single_selection) {
      gui::CanvasMenuItem increase_z;
      increase_z.label = "Increase Z";
      increase_z.icon = ICON_MD_ARROW_UPWARD;
      increase_z.subitems.emplace_back(
          "Send to Front", ICON_MD_FLIP_TO_FRONT,
          [&interaction]() { interaction.SendSelectedToFront(); },
          "Ctrl+Shift+]");
      increase_z.subitems.emplace_back(
          "Increase Z by 1", ICON_MD_ARROW_UPWARD,
          [&interaction]() { interaction.BringSelectedForward(); }, "Ctrl+]");
      items.push_back(std::move(increase_z));

      gui::CanvasMenuItem decrease_z;
      decrease_z.label = "Decrease Z";
      decrease_z.icon = ICON_MD_ARROW_DOWNWARD;
      decrease_z.subitems.emplace_back(
          "Send to Back", ICON_MD_FLIP_TO_BACK,
          [&interaction]() { interaction.SendSelectedToBack(); },
          "Ctrl+Shift+[");
      decrease_z.subitems.emplace_back(
          "Decrease Z by 1", ICON_MD_ARROW_DOWNWARD,
          [&interaction]() { interaction.SendSelectedBackward(); }, "Ctrl+[");
      items.push_back(std::move(decrease_z));
    } else if (group_selection) {
      items.emplace_back(
          "Send to Front", ICON_MD_FLIP_TO_FRONT,
          [&interaction]() { interaction.SendSelectedToFront(); },
          "Ctrl+Shift+]");
      items.emplace_back(
          "Send to Back", ICON_MD_FLIP_TO_BACK,
          [&interaction]() { interaction.SendSelectedToBack(); },
          "Ctrl+Shift+[");
      items.push_back(
          disabled_item("Save As New Layout...", ICON_MD_SAVE_AS, nullptr));
    }

    items.push_back(layer_item("Send to Layer 1", 0, "1"));
    items.push_back(layer_item("Send to Layer 2", 1, "2"));
    items.push_back(layer_item("Send to Layer 3", 2, "3"));

    gui::CanvasMenuItem yaze_more;
    yaze_more.label = "More";
    yaze_more.icon = ICON_MD_MORE_HORIZ;
    yaze_more.subitems.emplace_back(
        "Duplicate", ICON_MD_CONTENT_PASTE,
        [&interaction]() {
          interaction.HandleCopySelected();
          interaction.HandlePasteObjects();
        },
        "Ctrl+D");
    items.push_back(std::move(yaze_more));
  }

  if (can_edit_selected_graphics) {
    items.emplace_back("Edit Graphics...", ICON_MD_IMAGE,
                       std::move(edit_selected_graphics));
  }

  const bool has_entity_selection = interaction.HasEntitySelection();
  if (has_entity_selection && !has_selection) {
    items.emplace_back(
        "Delete", ICON_MD_DELETE,
        [&interaction]() { interaction.HandleDeleteSelected(); }, "Del");
  }

  if (!has_selection && !has_entity_selection) {
    items.push_back(paste_item());
    items.push_back(disabled_item("Delete", ICON_MD_DELETE, "Del"));
    gui::CanvasMenuItem delete_all_item(
        "Delete All", ICON_MD_DELETE_FOREVER,
        [&interaction]() { interaction.HandleDeleteAllObjects(); });
    delete_all_item.enabled_condition = enabled_if(room_has_objects);
    items.push_back(std::move(delete_all_item));
  }

  if (placing_object) {
    gui::CanvasMenuItem cancel_item(
        "Cancel Placement", ICON_MD_CANCEL,
        [&interaction]() { interaction.CancelPlacement(); }, "Esc");
    if (!items.empty()) {
      items.back().separator_after = true;
    }
    items.push_back(std::move(cancel_item));
  }

  if (sample_item_index.has_value() && items.size() > *sample_item_index + 1) {
    items[*sample_item_index].separator_after = true;
  }

  if (!items.empty()) {
    items.back().separator_after = true;
  }
  return items;
}

gui::CanvasMenuItem DungeonCanvasViewer::BuildRoomContextMenu(int room_id) {
  gui::CanvasMenuItem room_menu;
  room_menu.label = "Room";
  room_menu.icon = ICON_MD_HOME;
  if (save_room_callback_) {
    room_menu.subitems.emplace_back(
        "Apply Room to ROM", ICON_MD_SAVE,
        [this, room_id]() { save_room_callback_(room_id); }, "Ctrl+Shift+S");
  }
  room_menu.subitems.emplace_back(
      "Re-render Room", ICON_MD_REFRESH,
      [this, room_id]() {
        if (rooms_ && room_id >= 0 && room_id < zelda3::kNumberOfRooms) {
          (*rooms_)[room_id].RenderRoomGraphics();
        }
      },
      "Ctrl+R");
  if (rooms_ && !(*rooms_)[room_id].GetTileObjects().empty()) {
    room_menu.subitems.emplace_back(
        "Delete All Objects", ICON_MD_DELETE_FOREVER,
        [this]() { object_interaction_.HandleDeleteAllObjects(); });
  }
  return room_menu;
}

gui::CanvasMenuItem DungeonCanvasViewer::BuildReportContextMenu(int room_id) {
  gui::CanvasMenuItem report_menu;
  report_menu.label = "Report";
  report_menu.icon = ICON_MD_BUG_REPORT;
  report_menu.subitems.emplace_back(
      "Render Issue...", ICON_MD_BUG_REPORT, [this, room_id]() {
        if (!rooms_ || room_id < 0 || room_id >= zelda3::kNumberOfRooms) {
          return;
        }
        OpenIssueReportPopup(
            absl::StrFormat("Report Room 0x%03X Render Issue", room_id),
            absl::StrFormat("Room 0x%03X render mismatch", room_id),
            "Dungeon Render Issue Report",
            BuildDrawIssueReport((*rooms_)[room_id], room_id), room_id,
            ToIssueCategoryIndex(
                DungeonIssueCategory::GeneralRoomRenderMismatch));
      });
  report_menu.subitems.emplace_back(
      "Palette Issue...", ICON_MD_PALETTE, [this, room_id]() {
        if (!rooms_ || room_id < 0 || room_id >= zelda3::kNumberOfRooms) {
          return;
        }
        OpenIssueReportPopup(
            absl::StrFormat("Report Room 0x%03X Palette Issue", room_id),
            absl::StrFormat("Room 0x%03X palette mismatch", room_id),
            "Dungeon Palette Issue Report",
            BuildDrawIssueReport((*rooms_)[room_id], room_id), room_id,
            ToIssueCategoryIndex(DungeonIssueCategory::PaletteMismatch));
      });
  if (object_interaction_.GetSelectionCount() > 0 ||
      object_interaction_.HasEntitySelection()) {
    report_menu.subitems.emplace_back(
        "Selection Issue...", ICON_MD_FACT_CHECK, [this, room_id]() {
          if (!rooms_ || room_id < 0 || room_id >= zelda3::kNumberOfRooms) {
            return;
          }
          OpenIssueReportPopup(
              absl::StrFormat("Report Selection Issue for Room 0x%03X",
                              room_id),
              absl::StrFormat("Room 0x%03X selection or entity mismatch",
                              room_id),
              "Dungeon Selection Issue Report",
              BuildSelectionIssueReport((*rooms_)[room_id], room_id), room_id,
              ToIssueCategoryIndex(
                  GetDefaultSelectionIssueCategory(object_interaction_)));
        });
  }
  return report_menu;
}

std::optional<gui::CanvasMenuItem> DungeonCanvasViewer::BuildOpenContextMenu() {
  gui::CanvasMenuItem open_menu;
  open_menu.label = "Open";
  open_menu.icon = ICON_MD_OPEN_IN_NEW;
  if (show_room_matrix_callback_) {
    open_menu.subitems.emplace_back("Room Matrix", ICON_MD_GRID_VIEW,
                                    [this]() { show_room_matrix_callback_(); });
  }
  if (show_room_graphics_callback_) {
    open_menu.subitems.emplace_back("Room Graphics", ICON_MD_IMAGE, [this]() {
      show_room_graphics_callback_();
    });
  }
  if (show_door_editor_callback_) {
    open_menu.subitems.emplace_back("Door Editor", ICON_MD_DOOR_FRONT,
                                    [this]() { show_door_editor_callback_(); });
  }
  if (show_entrance_list_callback_) {
    open_menu.subitems.emplace_back("Entrance List", ICON_MD_LOGIN, [this]() {
      show_entrance_list_callback_();
    });
  }
  if (open_menu.subitems.empty()) {
    return std::nullopt;
  }
  return open_menu;
}

gui::CanvasMenuItem DungeonCanvasViewer::BuildCopyExportContextMenu(
    int room_id) {
  gui::CanvasMenuItem copy_export_menu;
  copy_export_menu.label = "Copy / Export";
  copy_export_menu.icon = ICON_MD_CONTENT_COPY;
  copy_export_menu.subitems.emplace_back(
      "Copy Room Summary", ICON_MD_ASSIGNMENT, [this, room_id]() {
        if (!rooms_ || room_id < 0 || room_id >= zelda3::kNumberOfRooms) {
          return;
        }
        ImGui::SetClipboardText(
            BuildRoomMetadataSummary((*rooms_)[room_id], room_id).c_str());
      });
  copy_export_menu.subitems.emplace_back(
      "Copy Room ID", ICON_MD_CONTENT_COPY, [room_id]() {
        ImGui::SetClipboardText(absl::StrFormat("0x%03X", room_id).c_str());
      });
  if (rooms_ && room_id >= 0 && room_id < zelda3::kNumberOfRooms) {
    const std::string room_label = zelda3::GetRoomLabel(room_id);
    copy_export_menu.subitems.emplace_back(
        "Copy Room Name", ICON_MD_CONTENT_COPY,
        [room_label]() { ImGui::SetClipboardText(room_label.c_str()); });
  }
  copy_export_menu.subitems.emplace_back(
      "Export Layout Template...", ICON_MD_FILE_DOWNLOAD, [this, room_id]() {
        if (!rooms_ || room_id < 0 || room_id >= zelda3::kNumberOfRooms) {
          return;
        }
        auto& room = (*rooms_)[room_id];
        auto result = zelda3::ExportRoomLayoutTemplate(room);
        if (result.ok()) {
          ImGui::SetClipboardText(result.value().c_str());
        }
      });
  return copy_export_menu;
}

gui::CanvasMenuItem DungeonCanvasViewer::BuildLayerVisibilityContextMenu(
    int room_id) {
  gui::CanvasMenuItem layers_menu;
  layers_menu.label = "Layers";
  layers_menu.icon = ICON_MD_VISIBILITY;

  gui::CanvasMenuItem bg1_layout_item("BG1 Layout", [this, room_id]() {
    auto& mgr = GetRoomLayerManager(room_id);
    mgr.SetLayerVisible(zelda3::LayerType::BG1_Layout,
                        !mgr.IsLayerVisible(zelda3::LayerType::BG1_Layout));
  });
  bg1_layout_item.checked_condition = [this, room_id]() {
    return GetRoomLayerManager(room_id).IsLayerVisible(
        zelda3::LayerType::BG1_Layout);
  };
  layers_menu.subitems.push_back(std::move(bg1_layout_item));

  gui::CanvasMenuItem bg1_objects_item("BG1 Objects", [this, room_id]() {
    auto& mgr = GetRoomLayerManager(room_id);
    mgr.SetLayerVisible(zelda3::LayerType::BG1_Objects,
                        !mgr.IsLayerVisible(zelda3::LayerType::BG1_Objects));
  });
  bg1_objects_item.checked_condition = [this, room_id]() {
    return GetRoomLayerManager(room_id).IsLayerVisible(
        zelda3::LayerType::BG1_Objects);
  };
  layers_menu.subitems.push_back(std::move(bg1_objects_item));

  gui::CanvasMenuItem bg2_layout_item("BG2 Layout", [this, room_id]() {
    auto& mgr = GetRoomLayerManager(room_id);
    mgr.SetLayerVisible(zelda3::LayerType::BG2_Layout,
                        !mgr.IsLayerVisible(zelda3::LayerType::BG2_Layout));
  });
  bg2_layout_item.checked_condition = [this, room_id]() {
    return GetRoomLayerManager(room_id).IsLayerVisible(
        zelda3::LayerType::BG2_Layout);
  };
  layers_menu.subitems.push_back(std::move(bg2_layout_item));

  gui::CanvasMenuItem bg2_objects_item("BG2 Objects", [this, room_id]() {
    auto& mgr = GetRoomLayerManager(room_id);
    mgr.SetLayerVisible(zelda3::LayerType::BG2_Objects,
                        !mgr.IsLayerVisible(zelda3::LayerType::BG2_Objects));
  });
  bg2_objects_item.checked_condition = [this, room_id]() {
    return GetRoomLayerManager(room_id).IsLayerVisible(
        zelda3::LayerType::BG2_Objects);
  };
  layers_menu.subitems.push_back(std::move(bg2_objects_item));

  gui::CanvasMenuItem sprites_item("Sprites", ICON_MD_PERSON, [this]() {
    entity_visibility_.show_sprites = !entity_visibility_.show_sprites;
  });
  sprites_item.checked_condition = [this]() {
    return entity_visibility_.show_sprites;
  };
  layers_menu.subitems.push_back(std::move(sprites_item));

  gui::CanvasMenuItem pot_items_item("Pot Items", ICON_MD_INVENTORY, [this]() {
    entity_visibility_.show_pot_items = !entity_visibility_.show_pot_items;
  });
  pot_items_item.checked_condition = [this]() {
    return entity_visibility_.show_pot_items;
  };
  layers_menu.subitems.push_back(std::move(pot_items_item));

  return layers_menu;
}

gui::CanvasMenuItem DungeonCanvasViewer::BuildOverlayContextMenu() {
  gui::CanvasMenuItem overlays_menu;
  overlays_menu.label = "Overlays";
  overlays_menu.icon = ICON_MD_LAYERS;

  gui::CanvasMenuItem coordinates_item(
      "Coordinates", ICON_MD_MY_LOCATION,
      [this]() { show_coordinate_overlay_ = !show_coordinate_overlay_; });
  coordinates_item.checked_condition = [this]() {
    return show_coordinate_overlay_;
  };
  overlays_menu.subitems.push_back(std::move(coordinates_item));

  gui::CanvasMenuItem minecart_toggle(
      "Minecart Tracks", ICON_MD_TRAIN,
      [this]() { show_minecart_tracks_ = !show_minecart_tracks_; });
  minecart_toggle.enabled_condition = [this]() {
    return minecart_track_panel_ != nullptr;
  };
  minecart_toggle.checked_condition = [this]() {
    return show_minecart_tracks_;
  };
  overlays_menu.subitems.push_back(std::move(minecart_toggle));

  gui::CanvasMenuItem custom_collision_item(
      "Custom Collision", ICON_MD_GRID_ON, [this]() {
        show_custom_collision_overlay_ = !show_custom_collision_overlay_;
      });
  custom_collision_item.checked_condition = [this]() {
    return show_custom_collision_overlay_;
  };
  overlays_menu.subitems.push_back(std::move(custom_collision_item));

  gui::CanvasMenuItem track_collision_item(
      "Track Collision", ICON_MD_LAYERS, [this]() {
        show_track_collision_overlay_ = !show_track_collision_overlay_;
      });
  track_collision_item.checked_condition = [this]() {
    return show_track_collision_overlay_;
  };
  overlays_menu.subitems.push_back(std::move(track_collision_item));

  gui::CanvasMenuItem camera_quadrants_item(
      "Camera Quadrants", ICON_MD_GRID_VIEW, [this]() {
        show_camera_quadrant_overlay_ = !show_camera_quadrant_overlay_;
      });
  camera_quadrants_item.checked_condition = [this]() {
    return show_camera_quadrant_overlay_;
  };
  overlays_menu.subitems.push_back(std::move(camera_quadrants_item));

  gui::CanvasMenuItem minecart_sprites_item(
      "Minecart Sprites", ICON_MD_TRAIN, [this]() {
        show_minecart_sprite_overlay_ = !show_minecart_sprite_overlay_;
      });
  minecart_sprites_item.checked_condition = [this]() {
    return show_minecart_sprite_overlay_;
  };
  overlays_menu.subitems.push_back(std::move(minecart_sprites_item));

  if (show_track_collision_overlay_) {
    gui::CanvasMenuItem collision_legend_item(
        "Collision Legend", ICON_MD_INFO, [this]() {
          show_track_collision_legend_ = !show_track_collision_legend_;
        });
    collision_legend_item.checked_condition = [this]() {
      return show_track_collision_legend_;
    };
    overlays_menu.subitems.push_back(std::move(collision_legend_item));
  }

  gui::CanvasMenuItem object_bounds_item(
      "Object Bounds", ICON_MD_CROP_SQUARE,
      [this]() { show_object_bounds_ = !show_object_bounds_; });
  object_bounds_item.checked_condition = [this]() {
    return show_object_bounds_;
  };
  overlays_menu.subitems.push_back(std::move(object_bounds_item));

  return overlays_menu;
}

gui::CanvasMenuItem DungeonCanvasViewer::BuildDebugContextMenu(int room_id) {
  gui::CanvasMenuItem debug_menu;
  debug_menu.label = "Debug";
  debug_menu.icon = ICON_MD_BUG_REPORT;

  debug_menu.subitems.push_back(gui::CanvasMenuItem(
      "Room Info", ICON_MD_INFO,
      [this]() { show_room_debug_info_ = !show_room_debug_info_; }));
  debug_menu.subitems.back().checked_condition = [this]() {
    return show_room_debug_info_;
  };

  debug_menu.subitems.push_back(gui::CanvasMenuItem(
      "Texture Debug", ICON_MD_IMAGE,
      [this]() { show_texture_debug_ = !show_texture_debug_; }));
  debug_menu.subitems.back().checked_condition = [this]() {
    return show_texture_debug_;
  };

  gui::CanvasMenuItem object_bounds_menu;
  object_bounds_menu.label = "Object Bounds Filter";
  object_bounds_menu.icon = ICON_MD_CROP_SQUARE;

  object_bounds_menu.subitems.push_back(
      gui::CanvasMenuItem("Type 1 (0x00-0xFF)", [this]() {
        object_outline_toggles_.show_type1_objects =
            !object_outline_toggles_.show_type1_objects;
      }));
  object_bounds_menu.subitems.back().checked_condition = [this]() {
    return object_outline_toggles_.show_type1_objects;
  };
  object_bounds_menu.subitems.push_back(
      gui::CanvasMenuItem("Type 2 (0x100-0x1FF)", [this]() {
        object_outline_toggles_.show_type2_objects =
            !object_outline_toggles_.show_type2_objects;
      }));
  object_bounds_menu.subitems.back().checked_condition = [this]() {
    return object_outline_toggles_.show_type2_objects;
  };
  object_bounds_menu.subitems.push_back(
      gui::CanvasMenuItem("Type 3 (0xF00-0xFFF)", [this]() {
        object_outline_toggles_.show_type3_objects =
            !object_outline_toggles_.show_type3_objects;
      }));
  object_bounds_menu.subitems.back().checked_condition = [this]() {
    return object_outline_toggles_.show_type3_objects;
  };

  gui::CanvasMenuItem sep;
  sep.label = "---";
  sep.enabled_condition = []() {
    return false;
  };
  object_bounds_menu.subitems.push_back(std::move(sep));

  object_bounds_menu.subitems.push_back(
      gui::CanvasMenuItem("Layer 1", [this]() {
        object_outline_toggles_.show_layer0_objects =
            !object_outline_toggles_.show_layer0_objects;
      }));
  object_bounds_menu.subitems.back().checked_condition = [this]() {
    return object_outline_toggles_.show_layer0_objects;
  };
  object_bounds_menu.subitems.push_back(
      gui::CanvasMenuItem("Layer 2", [this]() {
        object_outline_toggles_.show_layer1_objects =
            !object_outline_toggles_.show_layer1_objects;
      }));
  object_bounds_menu.subitems.back().checked_condition = [this]() {
    return object_outline_toggles_.show_layer1_objects;
  };
  object_bounds_menu.subitems.push_back(
      gui::CanvasMenuItem("Layer 3", [this]() {
        object_outline_toggles_.show_layer2_objects =
            !object_outline_toggles_.show_layer2_objects;
      }));
  object_bounds_menu.subitems.back().checked_condition = [this]() {
    return object_outline_toggles_.show_layer2_objects;
  };
  debug_menu.subitems.push_back(std::move(object_bounds_menu));

  debug_menu.subitems.push_back(
      gui::CanvasMenuItem("Layer Info", ICON_MD_LAYERS,
                          [this]() { show_layer_info_ = !show_layer_info_; }));
  debug_menu.subitems.back().checked_condition = [this]() {
    return show_layer_info_;
  };

  debug_menu.subitems.push_back(
      gui::CanvasMenuItem("Force Reload", ICON_MD_REFRESH, [this, room_id]() {
        if (rooms_ && room_id >= 0 && room_id < zelda3::kNumberOfRooms) {
          (*rooms_)[room_id].ReloadGraphics(current_entrance_blockset_);
        }
      }));

  debug_menu.subitems.push_back(
      gui::CanvasMenuItem("Log Room State", ICON_MD_PRINT, [this, room_id]() {
        if (!rooms_ || room_id < 0 || room_id >= zelda3::kNumberOfRooms) {
          return;
        }
        const auto& room = (*rooms_)[room_id];
        LOG_DEBUG("DungeonDebug", "=== Room %03X Debug ===", room_id);
        LOG_DEBUG("DungeonDebug", "Blockset: %d, Palette: %d, Layout: %d",
                  room.blockset(), room.palette(), room.layout_id());
        LOG_DEBUG("DungeonDebug", "Objects: %zu, Sprites: %zu",
                  room.GetTileObjects().size(), room.GetSprites().size());
        LOG_DEBUG("DungeonDebug", "BG1: %dx%d, BG2: %dx%d",
                  room.bg1_buffer().bitmap().width(),
                  room.bg1_buffer().bitmap().height(),
                  room.bg2_buffer().bitmap().width(),
                  room.bg2_buffer().bitmap().height());
      }));

  return debug_menu;
}

}  // namespace yaze::editor
