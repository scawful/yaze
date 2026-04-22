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
  if (auto selection_menu = BuildSelectionContextMenu(room_id);
      selection_menu.has_value()) {
    canvas_.AddContextMenuItem(std::move(*selection_menu));
  }
}

void DungeonCanvasViewer::AddLoadedRoomContextMenuItems(int room_id) {
  if (!rooms_ || !rom_ || !rom_->is_loaded()) {
    return;
  }

  const std::string room_label = zelda3::GetRoomLabel(room_id);
  gui::CanvasMenuItem room_header = gui::CanvasMenuItem::Disabled(
      absl::StrFormat("Room 0x%03X: %s", room_id, room_label.c_str()));
  room_header.separator_after = true;
  canvas_.AddContextMenuItem(room_header);
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

std::optional<gui::CanvasMenuItem>
DungeonCanvasViewer::BuildSelectionContextMenu(int room_id) {
  auto& interaction = object_interaction_;
  const auto selected = interaction.GetSelectedObjectIndices();
  const bool has_selection = !selected.empty();
  const bool single_selection = selected.size() == 1;
  const bool has_clipboard = interaction.HasClipboardData();
  const bool placing_object = interaction.IsObjectLoaded();

  gui::CanvasMenuItem selection_menu;
  selection_menu.label = "Selection";
  selection_menu.icon = ICON_MD_NEAR_ME;

  std::string selection_context_info;
  bool can_edit_selected_graphics = false;
  std::function<void()> edit_selected_graphics;
  if (single_selection && rooms_) {
    auto& room = (*rooms_)[room_id];
    const auto& objects = room.GetTileObjects();
    if (selected[0] < objects.size()) {
      const auto object = objects[selected[0]];
      selection_context_info = absl::StrFormat(
          "Object 0x%02X: %s", object.id_, zelda3::GetObjectName(object.id_));
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

  if (has_selection) {
    selection_menu.subitems.emplace_back(
        "Cut", ICON_MD_CONTENT_CUT,
        [&interaction]() {
          interaction.HandleCopySelected();
          interaction.HandleDeleteSelected();
        },
        "Ctrl+X");
    selection_menu.subitems.emplace_back(
        "Copy", ICON_MD_CONTENT_COPY,
        [&interaction]() { interaction.HandleCopySelected(); }, "Ctrl+C");
    selection_menu.subitems.emplace_back(
        "Duplicate", ICON_MD_CONTENT_PASTE,
        [&interaction]() {
          interaction.HandleCopySelected();
          interaction.HandlePasteObjects();
        },
        "Ctrl+D");
    selection_menu.subitems.emplace_back(
        "Delete", ICON_MD_DELETE,
        [&interaction]() { interaction.HandleDeleteSelected(); }, "Del");
  }

  if (has_clipboard) {
    selection_menu.subitems.emplace_back(
        "Paste", ICON_MD_CONTENT_PASTE,
        [&interaction]() { interaction.HandlePasteObjects(); }, "Ctrl+V");
  }

  if (placing_object) {
    selection_menu.subitems.emplace_back(
        "Cancel Placement", ICON_MD_CANCEL,
        [&interaction]() { interaction.CancelPlacement(); }, "Esc");
  }

  if (has_selection) {
    gui::CanvasMenuItem arrange_menu;
    arrange_menu.label = "Arrange";
    arrange_menu.icon = ICON_MD_SWAP_VERT;
    arrange_menu.subitems.emplace_back(
        "Bring to Front", ICON_MD_FLIP_TO_FRONT,
        [&interaction]() { interaction.SendSelectedToFront(); },
        "Ctrl+Shift+]");
    arrange_menu.subitems.emplace_back(
        "Send to Back", ICON_MD_FLIP_TO_BACK,
        [&interaction]() { interaction.SendSelectedToBack(); }, "Ctrl+Shift+[");
    arrange_menu.subitems.emplace_back(
        "Bring Forward", ICON_MD_ARROW_UPWARD,
        [&interaction]() { interaction.BringSelectedForward(); }, "Ctrl+]");
    arrange_menu.subitems.emplace_back(
        "Send Backward", ICON_MD_ARROW_DOWNWARD,
        [&interaction]() { interaction.SendSelectedBackward(); }, "Ctrl+[");
    selection_menu.subitems.push_back(std::move(arrange_menu));

    gui::CanvasMenuItem layer_menu;
    layer_menu.label = "Send to Layer";
    layer_menu.icon = ICON_MD_LAYERS;
    layer_menu.subitems.emplace_back(
        "Primary (main pass)", ICON_MD_LOOKS_ONE,
        [&interaction]() { interaction.SendSelectedToLayer(0); }, "1");
    layer_menu.subitems.emplace_back(
        "BG2 overlay", ICON_MD_LOOKS_TWO,
        [&interaction]() { interaction.SendSelectedToLayer(1); }, "2");
    layer_menu.subitems.emplace_back(
        "BG1 overlay", ICON_MD_LOOKS_3,
        [&interaction]() { interaction.SendSelectedToLayer(2); }, "3");
    selection_menu.subitems.push_back(std::move(layer_menu));
  }

  if (can_edit_selected_graphics) {
    selection_menu.subitems.emplace_back("Edit Graphics...", ICON_MD_IMAGE,
                                         std::move(edit_selected_graphics));
  }

  const auto selected_entity = interaction.GetSelectedEntity();
  const bool has_entity_selection = interaction.HasEntitySelection();
  bool can_delete_selected_entity = false;
  std::function<void()> delete_selected_entity;

  if (has_entity_selection && rooms_) {
    auto& room = (*rooms_)[room_id];
    std::string entity_info;
    switch (selected_entity.type) {
      case EntityType::Door: {
        const auto& doors = room.GetDoors();
        if (selected_entity.index < doors.size()) {
          const auto& door = doors[selected_entity.index];
          entity_info = absl::StrFormat(
              ICON_MD_DOOR_FRONT " Door: %s",
              std::string(zelda3::GetDoorTypeName(door.type)).c_str());
        }
        break;
      }
      case EntityType::Sprite: {
        const auto& sprites = room.GetSprites();
        if (selected_entity.index < sprites.size()) {
          const auto& sprite = sprites[selected_entity.index];
          entity_info = absl::StrFormat(ICON_MD_PERSON " Sprite: %s (0x%02X)",
                                        zelda3::ResolveSpriteName(sprite.id()),
                                        sprite.id());
        }
        break;
      }
      case EntityType::Item: {
        const auto& items = room.GetPotItems();
        if (selected_entity.index < items.size()) {
          const auto& item = items[selected_entity.index];
          entity_info =
              absl::StrFormat(ICON_MD_INVENTORY " Item: 0x%02X", item.item);
        }
        break;
      }
      default:
        break;
    }

    if (!entity_info.empty()) {
      selection_context_info = entity_info;
      can_delete_selected_entity = true;
      delete_selected_entity = [this, room_id, selected_entity]() {
        if (!rooms_ || room_id < 0 || room_id >= zelda3::kNumberOfRooms) {
          return;
        }
        auto& room = (*rooms_)[room_id];
        switch (selected_entity.type) {
          case EntityType::Door: {
            auto& doors = room.GetDoors();
            if (selected_entity.index < doors.size()) {
              doors.erase(doors.begin() +
                          static_cast<long>(selected_entity.index));
              room.MarkObjectStreamDirty();
            }
            break;
          }
          case EntityType::Sprite: {
            auto& sprites = room.GetSprites();
            if (selected_entity.index < sprites.size()) {
              sprites.erase(sprites.begin() +
                            static_cast<long>(selected_entity.index));
              room.MarkSpritesDirty();
            }
            break;
          }
          case EntityType::Item: {
            auto& items = room.GetPotItems();
            if (selected_entity.index < items.size()) {
              items.erase(items.begin() +
                          static_cast<long>(selected_entity.index));
              room.MarkPotItemsDirty();
            }
            break;
          }
          default:
            break;
        }
        object_interaction_.ClearEntitySelection();
      };
    }
  }

  if (!selection_context_info.empty()) {
    gui::CanvasMenuItem selection_header =
        gui::CanvasMenuItem::Disabled(selection_context_info);
    selection_header.separator_after = true;
    selection_menu.subitems.insert(selection_menu.subitems.begin(),
                                   std::move(selection_header));
  }
  if (can_delete_selected_entity) {
    selection_menu.subitems.emplace_back("Delete Entity", ICON_MD_DELETE,
                                         std::move(delete_selected_entity),
                                         "Del");
  }

  if (selection_menu.subitems.empty()) {
    return std::nullopt;
  }
  return selection_menu;
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
      gui::CanvasMenuItem("Primary (main pass)", [this]() {
        object_outline_toggles_.show_layer0_objects =
            !object_outline_toggles_.show_layer0_objects;
      }));
  object_bounds_menu.subitems.back().checked_condition = [this]() {
    return object_outline_toggles_.show_layer0_objects;
  };
  object_bounds_menu.subitems.push_back(
      gui::CanvasMenuItem("BG2 overlay", [this]() {
        object_outline_toggles_.show_layer1_objects =
            !object_outline_toggles_.show_layer1_objects;
      }));
  object_bounds_menu.subitems.back().checked_condition = [this]() {
    return object_outline_toggles_.show_layer1_objects;
  };
  object_bounds_menu.subitems.push_back(
      gui::CanvasMenuItem("BG1 overlay", [this]() {
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
