#include <algorithm>
#include <cfloat>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <optional>
#include <string>
#include <utility>

#include "absl/status/status.h"
#include "absl/strings/str_format.h"
#include "app/editor/agent/agent_ui_theme.h"
#include "app/editor/dungeon/panels/minecart_track_editor_panel.h"
#include "app/gfx/resource/arena.h"
#include "app/gfx/types/snes_palette.h"
#include "app/gui/canvas/canvas_menu.h"
#include "app/gui/core/icons.h"
#include "app/gui/core/input.h"
#include "dungeon_canvas_viewer.h"
#include "dungeon_coordinates.h"
#include "editor/dungeon/object_selection.h"
#include "imgui/imgui.h"
#include "rom/rom.h"
#include "util/log.h"
#include "util/macro.h"
#include "zelda3/dungeon/custom_collision.h"
#include "zelda3/dungeon/dimension_service.h"
#include "zelda3/dungeon/object_dimensions.h"
#include "zelda3/dungeon/object_drawer.h"
#include "zelda3/dungeon/room.h"
#include "zelda3/dungeon/room_layer_manager.h"
#include "zelda3/dungeon/room_object.h"
#include "zelda3/resource_labels.h"
#include "zelda3/sprite/sprite.h"
#include "zelda3/zelda3_labels.h"

namespace yaze::editor {

namespace {

constexpr int kRoomMatrixCols = 16;
constexpr int kRoomMatrixRows = 19;
constexpr int kRoomPropertyColumns = 2;

enum class TrackDir : uint8_t { North, East, South, West };



using TrackDirectionMasks = yaze::editor::TrackDirectionMasks;

}  // namespace

// Use shared GetObjectName() from zelda3/dungeon/room_object.h
using zelda3::GetObjectName;
using zelda3::GetObjectSubtype;

void DungeonCanvasViewer::SetProject(const project::YazeProject* project) {
  project_ = project;
  ApplyTrackCollisionConfig();
}

void DungeonCanvasViewer::ApplyTrackCollisionConfig() {
  auto apply_list = [](std::array<bool, 256>& dest,
                       const std::vector<uint16_t>& values) {
    dest.fill(false);
    for (uint16_t value : values) {
      if (value < 256) {
        dest[value] = true;
      }
    }
  };
  auto ids_valid = [](const std::vector<uint16_t>& values) {
    std::array<bool, 256> seen{};
    for (uint16_t value : values) {
      if (value >= seen.size() || seen[value]) {
        return false;
      }
      seen[value] = true;
    }
    return true;
  };

  std::vector<uint16_t> default_track_tile_list;
  for (uint16_t tile = 0xB0; tile <= 0xBE; ++tile) {
    default_track_tile_list.push_back(tile);
  }
  const std::vector<uint16_t> default_stop_tile_list = {0xB7, 0xB8, 0xB9, 0xBA};
  const std::vector<uint16_t> default_switch_tile_list = {0xD0, 0xD1, 0xD2,
                                                          0xD3};

  const auto& track_tiles =
      (project_ && !project_->dungeon_overlay.track_tiles.empty())
          ? project_->dungeon_overlay.track_tiles
          : default_track_tile_list;
  apply_list(track_collision_config_.track_tiles, track_tiles);
  track_tile_order_ = track_tiles;

  const auto& stop_tiles =
      (project_ && !project_->dungeon_overlay.track_stop_tiles.empty())
          ? project_->dungeon_overlay.track_stop_tiles
          : default_stop_tile_list;
  apply_list(track_collision_config_.stop_tiles, stop_tiles);

  const auto& switch_tiles =
      (project_ && !project_->dungeon_overlay.track_switch_tiles.empty())
          ? project_->dungeon_overlay.track_switch_tiles
          : default_switch_tile_list;
  apply_list(track_collision_config_.switch_tiles, switch_tiles);
  switch_tile_order_ = switch_tiles;

  track_direction_map_enabled_ =
      (track_tile_order_.size() == default_track_tile_list.size()) &&
      (switch_tile_order_.size() == default_switch_tile_list.size()) &&
      ids_valid(track_tile_order_) && ids_valid(switch_tile_order_);

  minecart_sprite_ids_.reset();
  std::vector<uint16_t> minecart_ids = {0xA3};
  if (project_ && !project_->dungeon_overlay.minecart_sprite_ids.empty()) {
    minecart_ids = project_->dungeon_overlay.minecart_sprite_ids;
  }
  for (uint16_t id : minecart_ids) {
    if (id < minecart_sprite_ids_.size()) {
      minecart_sprite_ids_[id] = true;
    }
  }

  collision_overlay_cache_.clear();
}

void DungeonCanvasViewer::Draw(int room_id) {
  DrawDungeonCanvas(room_id);
}

void DungeonCanvasViewer::DrawDungeonCanvas(int room_id) {
  current_room_id_ = room_id;
  // Validate room_id and ROM
  if (room_id < 0 || room_id >= 0x128) {
    ImGui::Text("Invalid room ID: %d", room_id);
    return;
  }

  if (!rom_ || !rom_->is_loaded()) {
    ImGui::Text("ROM not loaded");
    return;
  }

  // Handle pending scroll request
  if (pending_scroll_target_.has_value()) {
    auto [target_x, target_y] = pending_scroll_target_.value();

    // Convert tile coordinates to pixels
    float scale = canvas_.global_scale();
    if (scale <= 0.0f)
      scale = 1.0f;

    float pixel_x = target_x * 8 * scale;
    float pixel_y = target_y * 8 * scale;

    // Center in view
    ImVec2 view_size = ImGui::GetWindowSize();
    float scroll_x = pixel_x - (view_size.x * 0.5f);
    float scroll_y = pixel_y - (view_size.y * 0.5f);

    // Account for canvas position offset if possible, but roughly centering is
    // usually enough Ideally we'd add the cursor position y-offset to scroll_y
    // to account for the UI above canvas but GetCursorPosY() might not be
    // accurate before content is laid out. For X, canvas usually starts at
    // left, so it's fine.

    ImGui::SetScrollX(scroll_x);
    ImGui::SetScrollY(scroll_y);

    pending_scroll_target_.reset();
  }

  ImGui::BeginGroup();

  // CRITICAL: Canvas coordinate system for dungeons
  // The canvas system uses a two-stage scaling model:
  // 1. Canvas size: UNSCALED content dimensions (512x512 for dungeon rooms)
  // 2. Viewport size: canvas_size * global_scale (handles zoom)
  // 3. Grid lines: grid_step * global_scale (auto-scales with zoom)
  // 4. Bitmaps: drawn with scale = global_scale (matches viewport)
  constexpr int kRoomPixelWidth = 512;  // 64 tiles * 8 pixels (UNSCALED)
  constexpr int kRoomPixelHeight = 512;
  constexpr int kDungeonTileSize = 8;  // Dungeon tiles are 8x8 pixels

  // Configure canvas frame options for the new BeginCanvas/EndCanvas pattern
  gui::CanvasFrameOptions frame_opts;
  frame_opts.canvas_size = ImVec2(kRoomPixelWidth, kRoomPixelHeight);
  frame_opts.draw_grid = show_grid_;
  frame_opts.grid_step = static_cast<float>(custom_grid_size_);
  frame_opts.draw_context_menu = true;
  frame_opts.draw_overlay = true;
  frame_opts.render_popups = true;

  // Legacy configuration for context menu and interaction systems
  canvas_.SetShowBuiltinContextMenu(false);  // Hide default canvas debug items

  if (rooms_) {
    auto& room = (*rooms_)[room_id];

    // Check if critical properties changed and trigger reload
    if (prev_blockset_ != room.blockset || prev_palette_ != room.palette ||
        prev_layout_ != room.layout || prev_spriteset_ != room.spriteset) {
      // Only reload if ROM is properly loaded
      if (room.rom() && room.rom()->is_loaded()) {
        // Force reload of room graphics
        // Room buffers are now self-contained - no need for separate palette
        // operations
        room.LoadRoomGraphics(room.blockset);
        room.RenderRoomGraphics();  // Applies palettes internally
      }

      prev_blockset_ = room.blockset;
      prev_palette_ = room.palette;
      prev_layout_ = room.layout;
      prev_spriteset_ = room.spriteset;
    }
    if (header_visible_) {
        DrawRoomHeader(room, room_id);
    }
  }

  ImGui::EndGroup();

  // Set up context menu items BEFORE DrawBackground so DrawContextMenu can be
  // called immediately after (OpenPopupOnItemClick requires this ordering)
  canvas_.ClearContextMenuItems();

  // Add object interaction menu items to canvas context menu
  if (object_interaction_enabled_) {
    auto& interaction = object_interaction_;
    auto selected = interaction.GetSelectedObjectIndices();
    const bool has_selection = !selected.empty();
    const bool single_selection = selected.size() == 1;
    const bool group_selection = selected.size() > 1;
    const bool has_clipboard = interaction.HasClipboardData();
    const bool placing_object = interaction.IsObjectLoaded();
    const bool door_mode = interaction.IsDoorPlacementActive();
    bool has_objects = false;
    if (rooms_ && room_id >= 0 && room_id < 296) {
      has_objects = !(*rooms_)[room_id].GetTileObjects().empty();
    }

    if (single_selection && rooms_) {
      auto& room = (*rooms_)[room_id];
      const auto& objects = room.GetTileObjects();
      if (selected[0] < objects.size()) {
        const auto& obj = objects[selected[0]];
        std::string name = GetObjectName(obj.id_);
        canvas_.AddContextMenuItem(gui::CanvasMenuItem::Disabled(
            absl::StrFormat("Object 0x%02X: %s", obj.id_, name.c_str())));
      }
    }

    auto enabled_if = [](bool enabled) {
      return [enabled]() {
        return enabled;
      };
    };

    // Insert menu (parity with ZScream "Insert new <mode>")
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
    insert_menu.subitems.push_back(insert_object_item);

    gui::CanvasMenuItem insert_sprite_item("Sprite...", ICON_MD_PERSON,
                                           [this]() {
                                             if (show_sprite_panel_callback_) {
                                               show_sprite_panel_callback_();
                                             }
                                           });
    insert_sprite_item.enabled_condition =
        enabled_if(show_sprite_panel_callback_ != nullptr);
    insert_menu.subitems.push_back(insert_sprite_item);

    gui::CanvasMenuItem insert_item_item("Item...", ICON_MD_INVENTORY,
                                         [this]() {
                                           if (show_item_panel_callback_) {
                                             show_item_panel_callback_();
                                           }
                                         });
    insert_item_item.enabled_condition =
        enabled_if(show_item_panel_callback_ != nullptr);
    insert_menu.subitems.push_back(insert_item_item);

    gui::CanvasMenuItem insert_door_item(
        door_mode ? "Cancel Door Placement" : "Door (Normal)",
        ICON_MD_DOOR_FRONT, [&interaction, door_mode]() {
          interaction.SetDoorPlacementMode(!door_mode,
                                           zelda3::DoorType::NormalDoor);
        });
    insert_menu.subitems.push_back(insert_door_item);

    canvas_.AddContextMenuItem(insert_menu);

    gui::CanvasMenuItem cut_item(
        "Cut", ICON_MD_CONTENT_CUT,
        [&interaction]() {
          interaction.HandleCopySelected();
          interaction.HandleDeleteSelected();
        },
        "Ctrl+X");
    cut_item.enabled_condition = enabled_if(has_selection);
    canvas_.AddContextMenuItem(cut_item);

    gui::CanvasMenuItem copy_item(
        "Copy", ICON_MD_CONTENT_COPY,
        [&interaction]() { interaction.HandleCopySelected(); }, "Ctrl+C");
    copy_item.enabled_condition = enabled_if(has_selection);
    canvas_.AddContextMenuItem(copy_item);

    gui::CanvasMenuItem paste_item(
        "Paste", ICON_MD_CONTENT_PASTE,
        [&interaction]() { interaction.HandlePasteObjects(); }, "Ctrl+V");
    paste_item.enabled_condition = enabled_if(has_clipboard);
    canvas_.AddContextMenuItem(paste_item);

    gui::CanvasMenuItem duplicate_item(
        "Duplicate", ICON_MD_CONTENT_PASTE,
        [&interaction]() {
          interaction.HandleCopySelected();
          interaction.HandlePasteObjects();
        },
        "Ctrl+D");
    duplicate_item.enabled_condition = enabled_if(has_selection);
    canvas_.AddContextMenuItem(duplicate_item);

    gui::CanvasMenuItem delete_item(
        "Delete", ICON_MD_DELETE,
        [&interaction]() { interaction.HandleDeleteSelected(); }, "Del");
    delete_item.enabled_condition = enabled_if(has_selection);
    canvas_.AddContextMenuItem(delete_item);

    gui::CanvasMenuItem delete_all_item(
        "Delete All Objects", ICON_MD_DELETE_FOREVER,
        [&interaction]() { interaction.HandleDeleteAllObjects(); });
    delete_all_item.enabled_condition = enabled_if(has_objects);
    canvas_.AddContextMenuItem(delete_all_item);

    gui::CanvasMenuItem cancel_item(
        "Cancel Placement", ICON_MD_CANCEL,
        [&interaction]() { interaction.CancelPlacement(); }, "Esc");
    cancel_item.enabled_condition = enabled_if(placing_object);
    canvas_.AddContextMenuItem(cancel_item);

    // Arrange submenu (object draw order)
    gui::CanvasMenuItem arrange_menu;
    arrange_menu.label = "Arrange";
    arrange_menu.icon = ICON_MD_SWAP_VERT;
    arrange_menu.enabled_condition = enabled_if(has_selection);

    gui::CanvasMenuItem bring_front_item(
        "Bring to Front", ICON_MD_FLIP_TO_FRONT,
        [&interaction]() { interaction.SendSelectedToFront(); },
        "Ctrl+Shift+]");
    bring_front_item.enabled_condition = enabled_if(has_selection);
    arrange_menu.subitems.push_back(bring_front_item);

    gui::CanvasMenuItem send_back_item(
        "Send to Back", ICON_MD_FLIP_TO_BACK,
        [&interaction]() { interaction.SendSelectedToBack(); }, "Ctrl+Shift+[");
    send_back_item.enabled_condition = enabled_if(has_selection);
    arrange_menu.subitems.push_back(send_back_item);

    gui::CanvasMenuItem bring_forward_item(
        "Bring Forward", ICON_MD_ARROW_UPWARD,
        [&interaction]() { interaction.BringSelectedForward(); }, "Ctrl+]");
    bring_forward_item.enabled_condition = enabled_if(has_selection);
    arrange_menu.subitems.push_back(bring_forward_item);

    gui::CanvasMenuItem send_backward_item(
        "Send Backward", ICON_MD_ARROW_DOWNWARD,
        [&interaction]() { interaction.SendSelectedBackward(); }, "Ctrl+[");
    send_backward_item.enabled_condition = enabled_if(has_selection);
    arrange_menu.subitems.push_back(send_backward_item);

    canvas_.AddContextMenuItem(arrange_menu);

    // Send to Layer submenu
    gui::CanvasMenuItem layer_menu;
    layer_menu.label = "Send to Layer";
    layer_menu.icon = ICON_MD_LAYERS;
    layer_menu.enabled_condition = enabled_if(has_selection);

    gui::CanvasMenuItem layer1_item(
        "Layer 1 (BG1)", ICON_MD_LOOKS_ONE,
        [&interaction]() { interaction.SendSelectedToLayer(0); }, "1");
    layer1_item.enabled_condition = enabled_if(has_selection);
    layer_menu.subitems.push_back(layer1_item);

    gui::CanvasMenuItem layer2_item(
        "Layer 2 (BG2)", ICON_MD_LOOKS_TWO,
        [&interaction]() { interaction.SendSelectedToLayer(1); }, "2");
    layer2_item.enabled_condition = enabled_if(has_selection);
    layer_menu.subitems.push_back(layer2_item);

    gui::CanvasMenuItem layer3_item(
        "Layer 3 (BG3)", ICON_MD_LOOKS_3,
        [&interaction]() { interaction.SendSelectedToLayer(2); }, "3");
    layer3_item.enabled_condition = enabled_if(has_selection);
    layer_menu.subitems.push_back(layer3_item);

    canvas_.AddContextMenuItem(layer_menu);

    if (group_selection) {
      canvas_.AddContextMenuItem(
          gui::CanvasMenuItem::Disabled("Save As New Layout..."));
    }

    if (single_selection && rooms_) {
      auto& room = (*rooms_)[room_id];
      const auto& objects = room.GetTileObjects();
      if (selected[0] < objects.size()) {
        const auto object = objects[selected[0]];
        gui::CanvasMenuItem edit_graphics_item(
            "Edit Graphics...", ICON_MD_IMAGE, [this, room_id, object]() {
              if (edit_graphics_callback_) {
                edit_graphics_callback_(room_id, object);
              } else if (show_room_graphics_callback_) {
                show_room_graphics_callback_();
              }
            });
        edit_graphics_item.enabled_condition =
            enabled_if(edit_graphics_callback_ != nullptr ||
                       show_room_graphics_callback_ != nullptr);
        canvas_.AddContextMenuItem(edit_graphics_item);
      }
    }

    // === Entity Selection Actions (Doors, Sprites, Items) ===
    const auto& selected_entity = interaction.GetSelectedEntity();
    const bool has_entity_selection = interaction.HasEntitySelection();

    if (has_entity_selection && rooms_) {
      auto& room = (*rooms_)[room_id];

      // Show selected entity info header
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
            entity_info = absl::StrFormat(
                ICON_MD_PERSON " Sprite: %s (0x%02X)",
                zelda3::ResolveSpriteName(sprite.id()), sprite.id());
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
        canvas_.AddContextMenuItem(gui::CanvasMenuItem::Disabled(entity_info));

        // Delete entity action
        gui::CanvasMenuItem delete_entity_item(
            "Delete Entity", ICON_MD_DELETE,
            [this, &room, selected_entity]() {
              switch (selected_entity.type) {
                case EntityType::Door: {
                  auto& doors = room.GetDoors();
                  if (selected_entity.index < doors.size()) {
                    doors.erase(doors.begin() +
                                static_cast<long>(selected_entity.index));
                  }
                  break;
                }
                case EntityType::Sprite: {
                  auto& sprites = room.GetSprites();
                  if (selected_entity.index < sprites.size()) {
                    sprites.erase(sprites.begin() +
                                  static_cast<long>(selected_entity.index));
                  }
                  break;
                }
                case EntityType::Item: {
                  auto& items = room.GetPotItems();
                  if (selected_entity.index < items.size()) {
                    items.erase(items.begin() +
                                static_cast<long>(selected_entity.index));
                  }
                  break;
                }
                default:
                  break;
              }
              object_interaction_.ClearEntitySelection();
            },
            "Del");
        canvas_.AddContextMenuItem(delete_entity_item);
      }
    }
  }
  if (rooms_ && rom_->is_loaded()) {
    auto& room = (*rooms_)[room_id];

    // === Room Menu ===
    gui::CanvasMenuItem room_menu;
    room_menu.label = "Room";
    room_menu.icon = ICON_MD_HOME;

    const std::string room_label = zelda3::GetRoomLabel(room_id);
    room_menu.subitems.push_back(gui::CanvasMenuItem::Disabled(
        absl::StrFormat("Room 0x%03X: %s", room_id, room_label.c_str())));

    if (save_room_callback_) {
      room_menu.subitems.push_back(gui::CanvasMenuItem(
          "Save Room", ICON_MD_SAVE,
          [this, room_id]() { save_room_callback_(room_id); }, "Ctrl+Shift+S"));
    }
    room_menu.subitems.push_back(
        gui::CanvasMenuItem("Copy Room ID", ICON_MD_CONTENT_COPY, [room_id]() {
          ImGui::SetClipboardText(absl::StrFormat("0x%03X", room_id).c_str());
        }));
    room_menu.subitems.push_back(gui::CanvasMenuItem(
        "Copy Room Name", ICON_MD_CONTENT_COPY,
        [room_label]() { ImGui::SetClipboardText(room_label.c_str()); }));

    room_menu.subitems.push_back(
        gui::CanvasMenuItem("Open Room List", ICON_MD_LIST, [this]() {
          if (show_room_list_callback_) {
            show_room_list_callback_();
          }
        }));
    room_menu.subitems.push_back(
        gui::CanvasMenuItem("Open Room Matrix", ICON_MD_GRID_VIEW, [this]() {
          if (show_room_matrix_callback_) {
            show_room_matrix_callback_();
          }
        }));
    room_menu.subitems.push_back(
        gui::CanvasMenuItem("Open Entrance List", ICON_MD_DOOR_FRONT, [this]() {
          if (show_entrance_list_callback_) {
            show_entrance_list_callback_();
          }
        }));
    room_menu.subitems.push_back(
        gui::CanvasMenuItem("Open Room Graphics", ICON_MD_IMAGE, [this]() {
          if (show_room_graphics_callback_) {
            show_room_graphics_callback_();
          }
        }));

    canvas_.AddContextMenuItem(room_menu);

    // === Entity Placement Menu ===
    gui::CanvasMenuItem place_menu;
    place_menu.label = "Place Entity";
    place_menu.icon = ICON_MD_ADD;

    // Place Object option
    place_menu.subitems.push_back(
        gui::CanvasMenuItem("Object", ICON_MD_WIDGETS, [this]() {
          if (show_object_panel_callback_) {
            show_object_panel_callback_();
          }
        }));

    // Place Sprite option
    place_menu.subitems.push_back(
        gui::CanvasMenuItem("Sprite", ICON_MD_PERSON, [this]() {
          bool active = object_interaction_.IsSpritePlacementActive();
          object_interaction_.SetSpritePlacementMode(!active, 0x09);
        }));

    // Place Item option
    place_menu.subitems.push_back(
        gui::CanvasMenuItem("Item", ICON_MD_INVENTORY, [this]() {
          bool active = object_interaction_.IsItemPlacementActive();
          object_interaction_.SetItemPlacementMode(!active, 1);
        }));

    // Place Door option
    place_menu.subitems.push_back(
        gui::CanvasMenuItem("Door", ICON_MD_DOOR_FRONT, [this]() {
          bool active = object_interaction_.IsDoorPlacementActive();
          object_interaction_.SetDoorPlacementMode(
              !active, zelda3::DoorType::NormalDoor);
        }));

    canvas_.AddContextMenuItem(place_menu);

    // Add room property quick toggles (4-way layer visibility)
    gui::CanvasMenuItem layer_menu;
    layer_menu.label = "Layer Visibility";
    layer_menu.icon = ICON_MD_LAYERS;

    layer_menu.subitems.push_back(
        gui::CanvasMenuItem("BG1 Layout", [this, room_id]() {
          auto& mgr = GetRoomLayerManager(room_id);
          mgr.SetLayerVisible(
              zelda3::LayerType::BG1_Layout,
              !mgr.IsLayerVisible(zelda3::LayerType::BG1_Layout));
        }));
    layer_menu.subitems.push_back(
        gui::CanvasMenuItem("BG1 Objects", [this, room_id]() {
          auto& mgr = GetRoomLayerManager(room_id);
          mgr.SetLayerVisible(
              zelda3::LayerType::BG1_Objects,
              !mgr.IsLayerVisible(zelda3::LayerType::BG1_Objects));
        }));
    layer_menu.subitems.push_back(
        gui::CanvasMenuItem("BG2 Layout", [this, room_id]() {
          auto& mgr = GetRoomLayerManager(room_id);
          mgr.SetLayerVisible(
              zelda3::LayerType::BG2_Layout,
              !mgr.IsLayerVisible(zelda3::LayerType::BG2_Layout));
        }));
    layer_menu.subitems.push_back(
        gui::CanvasMenuItem("BG2 Objects", [this, room_id]() {
          auto& mgr = GetRoomLayerManager(room_id);
          mgr.SetLayerVisible(
              zelda3::LayerType::BG2_Objects,
              !mgr.IsLayerVisible(zelda3::LayerType::BG2_Objects));
        }));

    canvas_.AddContextMenuItem(layer_menu);

    // Entity Visibility menu
    gui::CanvasMenuItem entity_menu;
    entity_menu.label = "Entity Visibility";
    entity_menu.icon = ICON_MD_PERSON;

    entity_menu.subitems.push_back(
        gui::CanvasMenuItem("Show Sprites", [this]() {
          entity_visibility_.show_sprites = !entity_visibility_.show_sprites;
        }));
    entity_menu.subitems.push_back(
        gui::CanvasMenuItem("Show Pot Items", [this]() {
          entity_visibility_.show_pot_items =
              !entity_visibility_.show_pot_items;
        }));

    canvas_.AddContextMenuItem(entity_menu);

    // Custom overlays (minecart tracks, etc.)
    gui::CanvasMenuItem custom_menu;
    custom_menu.label = "Custom Overlays";
    custom_menu.icon = ICON_MD_TRAIN;

    gui::CanvasMenuItem minecart_toggle(
        show_minecart_tracks_ ? "Hide Minecart Tracks" : "Show Minecart Tracks",
        ICON_MD_TRAIN,
        [this]() { show_minecart_tracks_ = !show_minecart_tracks_; });
    minecart_toggle.enabled_condition = [this]() {
      return minecart_track_panel_ != nullptr;
    };
    custom_menu.subitems.push_back(minecart_toggle);

    custom_menu.subitems.push_back(gui::CanvasMenuItem(
        show_custom_collision_overlay_ ? "Hide Custom Collision"
                                       : "Show Custom Collision",
        ICON_MD_GRID_ON,
        [this]() {
          show_custom_collision_overlay_ = !show_custom_collision_overlay_;
        }));

    gui::CanvasMenuItem collision_toggle(
        show_track_collision_overlay_ ? "Hide Track Collision"
                                      : "Show Track Collision",
        ICON_MD_LAYERS, [this]() {
          show_track_collision_overlay_ = !show_track_collision_overlay_;
        });
    custom_menu.subitems.push_back(collision_toggle);

    gui::CanvasMenuItem quadrant_toggle(
        show_camera_quadrant_overlay_ ? "Hide Camera Quadrants"
                                      : "Show Camera Quadrants",
        ICON_MD_GRID_VIEW, [this]() {
          show_camera_quadrant_overlay_ = !show_camera_quadrant_overlay_;
        });
    custom_menu.subitems.push_back(quadrant_toggle);

    gui::CanvasMenuItem cart_sprite_toggle(
        show_minecart_sprite_overlay_ ? "Hide Minecart Sprites"
                                      : "Show Minecart Sprites",
        ICON_MD_TRAIN, [this]() {
          show_minecart_sprite_overlay_ = !show_minecart_sprite_overlay_;
        });
    custom_menu.subitems.push_back(cart_sprite_toggle);

    if (show_track_collision_overlay_) {
      gui::CanvasMenuItem legend_toggle(
          show_track_collision_legend_ ? "Hide Collision Legend"
                                       : "Show Collision Legend",
          ICON_MD_INFO, [this]() {
            show_track_collision_legend_ = !show_track_collision_legend_;
          });
      custom_menu.subitems.push_back(legend_toggle);
    }

    canvas_.AddContextMenuItem(custom_menu);

    // Dungeon Settings panel access
    canvas_.AddContextMenuItem(gui::CanvasMenuItem(
        "Dungeon Settings", ICON_MD_SETTINGS, [this]() {
          if (show_dungeon_settings_callback_) {
            show_dungeon_settings_callback_();
          }
        }));

    // Add re-render option
    canvas_.AddContextMenuItem(gui::CanvasMenuItem(
        "Re-render Room", ICON_MD_REFRESH,
        [&room]() { room.RenderRoomGraphics(); }, "Ctrl+R"));

    // Grid Options
    gui::CanvasMenuItem grid_menu;
    grid_menu.label = "Grid Options";
    grid_menu.icon = ICON_MD_GRID_ON;

    // Toggle grid visibility
    gui::CanvasMenuItem toggle_grid_item(
        show_grid_ ? "Hide Grid" : "Show Grid",
        show_grid_ ? ICON_MD_GRID_OFF : ICON_MD_GRID_ON,
        [this]() { show_grid_ = !show_grid_; }, "G");
    grid_menu.subitems.push_back(toggle_grid_item);

    // Grid size options (only show if grid is visible)
    grid_menu.subitems.push_back(gui::CanvasMenuItem("8x8", [this]() {
      custom_grid_size_ = 8;
      show_grid_ = true;
    }));
    grid_menu.subitems.push_back(gui::CanvasMenuItem("16x16", [this]() {
      custom_grid_size_ = 16;
      show_grid_ = true;
    }));
    grid_menu.subitems.push_back(gui::CanvasMenuItem("32x32", [this]() {
      custom_grid_size_ = 32;
      show_grid_ = true;
    }));

    canvas_.AddContextMenuItem(grid_menu);

    // === DEBUG MENU ===
    gui::CanvasMenuItem debug_menu;
    debug_menu.label = "Debug";
    debug_menu.icon = ICON_MD_BUG_REPORT;

    // Show room info
    gui::CanvasMenuItem room_info_item(
        "Show Room Info", ICON_MD_INFO,
        [this]() { show_room_debug_info_ = !show_room_debug_info_; });
    debug_menu.subitems.push_back(room_info_item);

    // Show texture info
    gui::CanvasMenuItem texture_info_item(
        "Show Texture Debug", ICON_MD_IMAGE,
        [this]() { show_texture_debug_ = !show_texture_debug_; });
    debug_menu.subitems.push_back(texture_info_item);

    // Toggle coordinate overlay
    gui::CanvasMenuItem coord_overlay_item(
        show_coordinate_overlay_ ? "Hide Coordinates" : "Show Coordinates",
        ICON_MD_MY_LOCATION,
        [this]() { show_coordinate_overlay_ = !show_coordinate_overlay_; });
    debug_menu.subitems.push_back(coord_overlay_item);

    // Show object bounds with sub-menu for categories
    gui::CanvasMenuItem object_bounds_menu;
    object_bounds_menu.label = "Show Object Bounds";
    object_bounds_menu.icon = ICON_MD_CROP_SQUARE;
    object_bounds_menu.callback = [this]() {
      show_object_bounds_ = !show_object_bounds_;
    };

    // Sub-menu for filtering by type
    object_bounds_menu.subitems.push_back(
        gui::CanvasMenuItem("Type 1 (0x00-0xFF)", [this]() {
          object_outline_toggles_.show_type1_objects =
              !object_outline_toggles_.show_type1_objects;
        }));
    object_bounds_menu.subitems.push_back(
        gui::CanvasMenuItem("Type 2 (0x100-0x1FF)", [this]() {
          object_outline_toggles_.show_type2_objects =
              !object_outline_toggles_.show_type2_objects;
        }));
    object_bounds_menu.subitems.push_back(
        gui::CanvasMenuItem("Type 3 (0xF00-0xFFF)", [this]() {
          object_outline_toggles_.show_type3_objects =
              !object_outline_toggles_.show_type3_objects;
        }));

    // Separator
    gui::CanvasMenuItem sep;
    sep.label = "---";
    sep.enabled_condition = []() {
      return false;
    };
    object_bounds_menu.subitems.push_back(sep);

    // Sub-menu for filtering by layer
    object_bounds_menu.subitems.push_back(
        gui::CanvasMenuItem("Layer 0 (BG1)", [this]() {
          object_outline_toggles_.show_layer0_objects =
              !object_outline_toggles_.show_layer0_objects;
        }));
    object_bounds_menu.subitems.push_back(
        gui::CanvasMenuItem("Layer 1 (BG2)", [this]() {
          object_outline_toggles_.show_layer1_objects =
              !object_outline_toggles_.show_layer1_objects;
        }));
    object_bounds_menu.subitems.push_back(
        gui::CanvasMenuItem("Layer 2 (BG3)", [this]() {
          object_outline_toggles_.show_layer2_objects =
              !object_outline_toggles_.show_layer2_objects;
        }));

    debug_menu.subitems.push_back(object_bounds_menu);

    // Show layer info
    gui::CanvasMenuItem layer_info_item(
        "Show Layer Info", ICON_MD_LAYERS,
        [this]() { show_layer_info_ = !show_layer_info_; });
    debug_menu.subitems.push_back(layer_info_item);

    // Force reload room
    gui::CanvasMenuItem force_reload_item(
        "Force Reload", ICON_MD_REFRESH, [&room]() {
          room.LoadObjects();
          room.LoadRoomGraphics(room.blockset);
          room.RenderRoomGraphics();
        });
    debug_menu.subitems.push_back(force_reload_item);

    // Log room state
    gui::CanvasMenuItem log_item(
        "Log Room State", ICON_MD_PRINT, [&room, room_id]() {
          LOG_DEBUG("DungeonDebug", "=== Room %03X Debug ===", room_id);
          LOG_DEBUG("DungeonDebug", "Blockset: %d, Palette: %d, Layout: %d",
                    room.blockset, room.palette, room.layout);
          LOG_DEBUG("DungeonDebug", "Objects: %zu, Sprites: %zu",
                    room.GetTileObjects().size(), room.GetSprites().size());
          LOG_DEBUG("DungeonDebug", "BG1: %dx%d, BG2: %dx%d",
                    room.bg1_buffer().bitmap().width(),
                    room.bg1_buffer().bitmap().height(),
                    room.bg2_buffer().bitmap().width(),
                    room.bg2_buffer().bitmap().height());
        });
    debug_menu.subitems.push_back(log_item);

    canvas_.AddContextMenuItem(debug_menu);
  }

  // CRITICAL: Begin canvas frame using modern BeginCanvas/EndCanvas pattern
  // This replaces DrawBackground + DrawContextMenu with a unified frame
  auto canvas_rt = gui::BeginCanvas(canvas_, frame_opts);

  // When the header is hidden (e.g. split/compare stitched views), draw a small
  // in-canvas label so the user always knows what they're looking at.
  if (!header_visible_) {
    const auto& label = zelda3::GetRoomLabel(room_id);
    char text1[160];
    snprintf(text1, sizeof(text1), "[%03X] %s", room_id, label.c_str());

    char text2[96] = {};
    bool show_meta = false;
    if (rooms_ && room_id >= 0 && room_id < static_cast<int>(rooms_->size())) {
      const auto& room = (*rooms_)[room_id];
      if (!object_interaction_enabled_) {
        snprintf(text2, sizeof(text2), "B:%02X P:%02X L:%02X S:%02X  RO",
                 room.blockset, room.palette, room.layout, room.spriteset);
      } else {
        snprintf(text2, sizeof(text2), "B:%02X P:%02X L:%02X S:%02X",
                 room.blockset, room.palette, room.layout, room.spriteset);
      }
      show_meta = true;
    } else if (!object_interaction_enabled_) {
      snprintf(text2, sizeof(text2), "Read-only");
      show_meta = true;
    }

    ImDrawList* dl = ImGui::GetWindowDrawList();
    const ImVec2 pad(8.0f, 6.0f);
    const ImVec2 zp = canvas_.zero_point();
    const ImVec2 p0(zp.x + 10.0f, zp.y + 10.0f);
    const ImVec2 ts1 = ImGui::CalcTextSize(text1);
    const ImVec2 ts2 = show_meta ? ImGui::CalcTextSize(text2) : ImVec2(0, 0);
    const float gap = show_meta ? 2.0f : 0.0f;
    const float w = std::max(ts1.x, ts2.x);
    const float h = ts1.y + (show_meta ? (gap + ts2.y) : 0.0f);
    const ImVec2 p1(p0.x + w + pad.x * 2.0f, p0.y + h + pad.y * 2.0f);

    ImVec4 bg = ImGui::GetStyleColorVec4(ImGuiCol_WindowBg);
    bg.w = std::min(0.90f, bg.w + 0.25f);
    dl->AddRectFilled(p0, p1, ImGui::ColorConvertFloat4ToU32(bg), 6.0f);
    dl->AddRect(p0, p1, ImGui::GetColorU32(ImGuiCol_Border), 6.0f);

    const ImVec2 t0(p0.x + pad.x, p0.y + pad.y);
    dl->AddText(t0, ImGui::GetColorU32(ImGuiCol_Text), text1);
    if (show_meta) {
      dl->AddText(ImVec2(t0.x, t0.y + ts1.y + gap),
                  ImGui::GetColorU32(ImGuiCol_Text), text2);
    }
  }

  // Draw persistent debug overlays
  if (show_room_debug_info_ && rooms_ && rom_->is_loaded()) {
    auto& room = (*rooms_)[room_id];
    ImGui::SetNextWindowPos(
        ImVec2(canvas_.zero_point().x + 10, canvas_.zero_point().y + 10),
        ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowSize(ImVec2(300, 0), ImGuiCond_FirstUseEver);
    if (ImGui::Begin("Room Debug Info", &show_room_debug_info_,
                     ImGuiWindowFlags_NoCollapse)) {
      ImGui::Text("Room: 0x%03X (%d)", room_id, room_id);
      ImGui::Separator();
      ImGui::Text("Graphics");
      ImGui::Text("  Blockset: 0x%02X", room.blockset);
      ImGui::Text("  Palette: 0x%02X", room.palette);
      ImGui::Text("  Layout: 0x%02X", room.layout);
      ImGui::Text("  Spriteset: 0x%02X", room.spriteset);
      ImGui::Separator();
      ImGui::Text("Content");
      ImGui::Text("  Objects: %zu", room.GetTileObjects().size());
      ImGui::Text("  Sprites: %zu", room.GetSprites().size());
      ImGui::Separator();
      ImGui::Text("Buffers");
      auto& bg1 = room.bg1_buffer().bitmap();
      auto& bg2 = room.bg2_buffer().bitmap();
      ImGui::Text("  BG1: %dx%d %s", bg1.width(), bg1.height(),
                  bg1.texture() ? "(has texture)" : "(NO TEXTURE)");
      ImGui::Text("  BG2: %dx%d %s", bg2.width(), bg2.height(),
                  bg2.texture() ? "(has texture)" : "(NO TEXTURE)");
      ImGui::Separator();
      ImGui::Text("Layers (4-way)");
      auto& layer_mgr = GetRoomLayerManager(room_id);
      bool bg1l = layer_mgr.IsLayerVisible(zelda3::LayerType::BG1_Layout);
      bool bg1o = layer_mgr.IsLayerVisible(zelda3::LayerType::BG1_Objects);
      bool bg2l = layer_mgr.IsLayerVisible(zelda3::LayerType::BG2_Layout);
      bool bg2o = layer_mgr.IsLayerVisible(zelda3::LayerType::BG2_Objects);
      if (ImGui::Checkbox("BG1 Layout", &bg1l))
        layer_mgr.SetLayerVisible(zelda3::LayerType::BG1_Layout, bg1l);
      if (ImGui::Checkbox("BG1 Objects", &bg1o))
        layer_mgr.SetLayerVisible(zelda3::LayerType::BG1_Objects, bg1o);
      if (ImGui::Checkbox("BG2 Layout", &bg2l))
        layer_mgr.SetLayerVisible(zelda3::LayerType::BG2_Layout, bg2l);
      if (ImGui::Checkbox("BG2 Objects", &bg2o))
        layer_mgr.SetLayerVisible(zelda3::LayerType::BG2_Objects, bg2o);
      int blend = static_cast<int>(
          layer_mgr.GetLayerBlendMode(zelda3::LayerType::BG2_Layout));
      if (ImGui::SliderInt("BG2 Blend", &blend, 0, 4)) {
        layer_mgr.SetLayerBlendMode(zelda3::LayerType::BG2_Layout,
                                    static_cast<zelda3::LayerBlendMode>(blend));
        layer_mgr.SetLayerBlendMode(zelda3::LayerType::BG2_Objects,
                                    static_cast<zelda3::LayerBlendMode>(blend));
      }

      ImGui::Separator();
      ImGui::Text("Layout Override");
      static bool enable_override = false;
      ImGui::Checkbox("Enable Override", &enable_override);
      if (enable_override) {
        ImGui::SliderInt("Layout ID", &layout_override_, 0, 7);
      } else {
        layout_override_ = -1;  // Disable override
      }

      if (show_object_bounds_) {
        ImGui::Separator();
        ImGui::Text("Object Outline Filters");
        ImGui::Text("By Type:");
        ImGui::Checkbox("Type 1", &object_outline_toggles_.show_type1_objects);
        ImGui::Checkbox("Type 2", &object_outline_toggles_.show_type2_objects);
        ImGui::Checkbox("Type 3", &object_outline_toggles_.show_type3_objects);
        ImGui::Text("By Layer:");
        ImGui::Checkbox("Layer 0",
                        &object_outline_toggles_.show_layer0_objects);
        ImGui::Checkbox("Layer 1",
                        &object_outline_toggles_.show_layer1_objects);
        ImGui::Checkbox("Layer 2",
                        &object_outline_toggles_.show_layer2_objects);
      }
    }
    ImGui::End();
  }

  if (show_texture_debug_ && rooms_ && rom_->is_loaded()) {
    ImGui::SetNextWindowPos(
        ImVec2(canvas_.zero_point().x + 320, canvas_.zero_point().y + 10),
        ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowSize(ImVec2(250, 0), ImGuiCond_FirstUseEver);
    if (ImGui::Begin("Texture Debug", &show_texture_debug_,
                     ImGuiWindowFlags_NoCollapse)) {
      auto& room = (*rooms_)[room_id];
      auto& bg1 = room.bg1_buffer().bitmap();
      auto& bg2 = room.bg2_buffer().bitmap();

      ImGui::Text("BG1 Bitmap");
      ImGui::Text("  Size: %dx%d", bg1.width(), bg1.height());
      ImGui::Text("  Active: %s", bg1.is_active() ? "YES" : "NO");
      ImGui::Text("  Texture: 0x%p", bg1.texture());
      ImGui::Text("  Modified: %s", bg1.modified() ? "YES" : "NO");

      if (bg1.texture()) {
        ImGui::Text("  Preview:");
        ImGui::Image((ImTextureID)(intptr_t)bg1.texture(), ImVec2(128, 128));
      }

      ImGui::Separator();
      ImGui::Text("BG2 Bitmap");
      ImGui::Text("  Size: %dx%d", bg2.width(), bg2.height());
      ImGui::Text("  Active: %s", bg2.is_active() ? "YES" : "NO");
      ImGui::Text("  Texture: 0x%p", bg2.texture());
      ImGui::Text("  Modified: %s", bg2.modified() ? "YES" : "NO");

      if (bg2.texture()) {
        ImGui::Text("  Preview:");
        ImGui::Image((ImTextureID)(intptr_t)bg2.texture(), ImVec2(128, 128));
      }
    }
    ImGui::End();
  }

  if (show_layer_info_) {
    ImGui::SetNextWindowPos(
        ImVec2(canvas_.zero_point().x + 580, canvas_.zero_point().y + 10),
        ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowSize(ImVec2(220, 0), ImGuiCond_FirstUseEver);
    if (ImGui::Begin("Layer Info", &show_layer_info_,
                     ImGuiWindowFlags_NoCollapse)) {
      ImGui::Text("Canvas Scale: %.2f", canvas_.global_scale());
      ImGui::Text("Canvas Size: %.0fx%.0f", canvas_.width(), canvas_.height());
      auto& layer_mgr = GetRoomLayerManager(room_id);
      ImGui::Separator();
      ImGui::Text("Layer Visibility (4-way):");

      // Display each layer with visibility and blend mode
      for (int i = 0; i < 4; ++i) {
        auto layer = static_cast<zelda3::LayerType>(i);
        bool visible = layer_mgr.IsLayerVisible(layer);
        auto blend = layer_mgr.GetLayerBlendMode(layer);
        ImGui::Text("  %s: %s (%s)",
                    zelda3::RoomLayerManager::GetLayerName(layer),
                    visible ? "VISIBLE" : "hidden",
                    zelda3::RoomLayerManager::GetBlendModeName(blend));
      }

      ImGui::Separator();
      ImGui::Text("Draw Order:");
      auto draw_order = layer_mgr.GetDrawOrder();
      for (int i = 0; i < 4; ++i) {
        ImGui::Text("  %d: %s", i + 1,
                    zelda3::RoomLayerManager::GetLayerName(draw_order[i]));
      }
      ImGui::Text("BG2 On Top: %s", layer_mgr.IsBG2OnTop() ? "YES" : "NO");
    }
    ImGui::End();
  }

  if (rooms_ && rom_->is_loaded()) {
    auto& room = (*rooms_)[room_id];

    // Update object interaction context
    object_interaction_.SetCurrentRoom(rooms_, room_id);

    // Check if THIS ROOM's buffers need rendering (not global arena!)
    auto& bg1_bitmap = room.bg1_buffer().bitmap();
    bool needs_render = !bg1_bitmap.is_active() || bg1_bitmap.width() == 0;

    // Render immediately if needed (but only once per room change)
    static int last_rendered_room = -1;
    static bool has_rendered = false;
    if (needs_render && (last_rendered_room != room_id || !has_rendered)) {
      printf(
          "[DungeonCanvasViewer] Loading and rendering graphics for room %d\n",
          room_id);
      (void)LoadAndRenderRoomGraphics(room_id);
      last_rendered_room = room_id;
      has_rendered = true;
    }

    // Load room objects if not already loaded
    if (room.GetTileObjects().empty()) {
      room.LoadObjects();
    }

    // Load sprites if not already loaded
    if (room.GetSprites().empty()) {
      room.LoadSprites();
    }

    // Load pot items if not already loaded
    if (room.GetPotItems().empty()) {
      room.LoadPotItems();
    }

    // CRITICAL: Process texture queue BEFORE drawing to ensure textures are
    // ready This must happen before DrawRoomBackgroundLayers() attempts to draw
    // bitmaps
    if (rom_ && rom_->is_loaded()) {
      gfx::Arena::Get().ProcessTextureQueue(renderer_);
    }

    // Draw the room's background layers to canvas
    // This already includes objects rendered by ObjectDrawer in
    // Room::RenderObjectsToBackground()
    DrawRoomBackgroundLayers(room_id);

    // Draw mask highlights when mask selection mode is active
    // This helps visualize which objects are BG2 overlays
    if (object_interaction_.IsMaskModeActive()) {
      DrawMaskHighlights(canvas_rt, room);
    }

    // Render entity overlays (sprites, pot items) as colored squares with labels
    // (Entities are not part of the background buffers)
    RenderEntityOverlay(canvas_rt, room);

    // Handle object interaction if enabled
    if (object_interaction_enabled_) {
      object_interaction_.HandleCanvasMouseInput();
      object_interaction_.CheckForObjectSelection();
      object_interaction_
          .DrawSelectionHighlights();  // Draw object selection highlights
      object_interaction_
          .DrawEntitySelectionHighlights();  // Draw door/sprite/item selection
      object_interaction_.DrawGhostPreview();  // Draw placement preview
      // Context menu is handled by BeginCanvas via frame_opts.draw_context_menu
    }
  }

  // Draw optional overlays on top of background bitmap
  if (rooms_ && rom_->is_loaded()) {
    auto& room = (*rooms_)[room_id];

    // Draw the room layout first as the base layer

    // VISUALIZATION: Draw object position rectangles (for debugging)
    // This shows where objects are placed regardless of whether graphics render
    if (show_object_bounds_) {
      DrawObjectPositionOutlines(canvas_rt, room);
    }

    // Track collision overlay (custom collision tiles)
    if (show_track_collision_overlay_) {
      DungeonRenderingHelpers::DrawTrackCollisionOverlay(
          ImGui::GetWindowDrawList(), canvas_.zero_point(),
          canvas_.global_scale(), GetCollisionOverlayCache(room.id()),
          track_collision_config_, track_direction_map_enabled_,
          track_tile_order_, switch_tile_order_, show_track_collision_legend_);
    }

    if (show_custom_collision_overlay_) {
      DungeonRenderingHelpers::DrawCustomCollisionOverlay(
          ImGui::GetWindowDrawList(), canvas_.zero_point(),
          canvas_.global_scale(), room);
    }

    if (show_water_fill_overlay_) {
      DungeonRenderingHelpers::DrawWaterFillOverlay(
          ImGui::GetWindowDrawList(), canvas_.zero_point(),
          canvas_.global_scale(), room);
    }

    if (show_camera_quadrant_overlay_) {
      DungeonRenderingHelpers::DrawCameraQuadrantOverlay(
          ImGui::GetWindowDrawList(), canvas_.zero_point(),
          canvas_.global_scale(), room);
    }

    if (show_minecart_sprite_overlay_) {
      DungeonRenderingHelpers::DrawMinecartSpriteOverlay(
          ImGui::GetWindowDrawList(), canvas_.zero_point(),
          canvas_.global_scale(), room, minecart_sprite_ids_,
          track_collision_config_);
    }

    if (show_track_gap_overlay_) {
      DungeonRenderingHelpers::DrawTrackGapOverlay(
          ImGui::GetWindowDrawList(), canvas_.zero_point(),
          canvas_.global_scale(), room, GetCollisionOverlayCache(room.id()));
    }

    if (show_track_route_overlay_) {
      DungeonRenderingHelpers::DrawTrackRouteOverlay(
          ImGui::GetWindowDrawList(), canvas_.zero_point(),
          canvas_.global_scale(), GetCollisionOverlayCache(room.id()));
    }

    if (minecart_track_panel_) {
      const bool show_tracks = show_minecart_tracks_ ||
                               minecart_track_panel_->IsPickingCoordinates();
      const auto& tracks = minecart_track_panel_->GetTracks();
      if (show_tracks && !tracks.empty()) {
        ImDrawList* draw_list = ImGui::GetWindowDrawList();
        ImVec2 canvas_pos = canvas_.zero_point();
        float scale = canvas_.global_scale();
        const auto& theme = AgentUI::GetTheme();
        const int active_track =
            minecart_track_panel_->IsPickingCoordinates()
                ? minecart_track_panel_->GetPickingTrackIndex()
                : -1;

        for (const auto& track : tracks) {
          auto local = dungeon_coords::CameraToLocalCoords(
              static_cast<uint16_t>(track.start_x),
              static_cast<uint16_t>(track.start_y));
          if (local.room_id != room_id) {
            continue;
          }

          ImVec4 marker_color = theme.dungeon_selection_primary;
          if (track.id == active_track) {
            marker_color = theme.text_warning_yellow;
          }

          const float px = static_cast<float>(local.local_pixel_x) * scale;
          const float py = static_cast<float>(local.local_pixel_y) * scale;
          ImVec2 center(canvas_pos.x + px, canvas_pos.y + py);
          const float radius = 6.0f * scale;

          draw_list->AddCircleFilled(center, radius,
                                     ImGui::GetColorU32(marker_color));
          draw_list->AddCircle(center, radius + 2.0f,
                               ImGui::GetColorU32(ImVec4(0, 0, 0, 0.6f)), 0,
                               2.0f);

          std::string label = absl::StrFormat("T%d", track.id);
          draw_list->AddText(
              ImVec2(center.x + 8.0f * scale, center.y - 6.0f * scale),
              ImGui::GetColorU32(theme.text_primary), label.c_str());
        }
      }
    }
  }

  // Draw coordinate overlay when hovering over canvas
  if (show_coordinate_overlay_ && canvas_.IsMouseHovering()) {
    auto [tile_x, tile_y] = DungeonRenderingHelpers::ScreenToRoomCoordinates(
        ImGui::GetMousePos(), canvas_.zero_point(), canvas_.global_scale());

    // Only show if within bounds
    if (tile_x >= 0 && tile_x < 64 && tile_y >= 0 && tile_y < 64) {

      // Calculate logical pixel coordinates
      int canvas_x = tile_x * 8;
      int canvas_y = tile_y * 8;

      // Calculate camera/world coordinates (for minecart tracks, sprites, etc.)
      auto [camera_x, camera_y] =
          dungeon_coords::TileToCameraCoords(room_id, tile_x, tile_y);

      // Calculate sprite coordinates (16-pixel units)
      int sprite_x = canvas_x / dungeon_coords::kSpriteTileSize;
      int sprite_y = canvas_y / dungeon_coords::kSpriteTileSize;

      // Draw coordinate info box at mouse position
      ImVec2 mouse_pos = ImGui::GetMousePos();
      ImVec2 overlay_pos = ImVec2(mouse_pos.x + 15, mouse_pos.y + 15);

      // Build coordinate text
      std::string coord_text = absl::StrFormat(
          "Tile: (%d, %d)\n"
          "Pixel: (%d, %d)\n"
          "Camera: ($%04X, $%04X)\n"
          "Sprite: (%d, %d)",
          tile_x, tile_y, canvas_x, canvas_y, camera_x, camera_y, sprite_x,
          sprite_y);

      // Draw background box
      ImVec2 text_size = ImGui::CalcTextSize(coord_text.c_str());
      ImVec2 box_min = ImVec2(overlay_pos.x - 4, overlay_pos.y - 2);
      ImVec2 box_max = ImVec2(overlay_pos.x + text_size.x + 8,
                              overlay_pos.y + text_size.y + 4);

      ImDrawList* draw_list = ImGui::GetWindowDrawList();
      draw_list->AddRectFilled(box_min, box_max, IM_COL32(0, 0, 0, 200), 4.0f);
      draw_list->AddRect(box_min, box_max, IM_COL32(100, 100, 100, 255), 4.0f);
      draw_list->AddText(overlay_pos, IM_COL32(255, 255, 255, 255),
                         coord_text.c_str());
    }
  }

  // End canvas frame - this draws grid/overlay based on frame_opts
  gui::EndCanvas(canvas_, canvas_rt, frame_opts);
}

void DungeonCanvasViewer::DisplayObjectInfo(const gui::CanvasRuntime& rt,
                                            const zelda3::RoomObject& object,
                                            int canvas_x, int canvas_y) {
  // Display object information as text overlay with hex ID and name
  std::string name = GetObjectName(object.id_);
  std::string info_text;
  if (object.id_ >= 0x100) {
    info_text =
        absl::StrFormat("0x%03X %s (X:%d Y:%d S:0x%02X)", object.id_,
                        name.c_str(), object.x_, object.y_, object.size_);
  } else {
    info_text =
        absl::StrFormat("0x%02X %s (X:%d Y:%d S:0x%02X)", object.id_,
                        name.c_str(), object.x_, object.y_, object.size_);
  }

  // Draw text at the object position using runtime-based helper
  gui::DrawText(rt, info_text, canvas_x, canvas_y - 12);
}

void DungeonCanvasViewer::RenderSprites(const gui::CanvasRuntime& rt,
                                        const zelda3::Room& room) {
  // Skip if sprites are not visible
  if (!entity_visibility_.show_sprites) {
    return;
  }

  const auto& theme = AgentUI::GetTheme();

  // Render sprites as 16x16 colored squares with sprite name/ID
  // NOTE: Sprite coordinates are in 16-pixel units (0-31 range = 512 pixels)
  // unlike object coordinates which are in 8-pixel tile units
  for (const auto& sprite : room.GetSprites()) {
    // Sprites use 16-pixel coordinate system
    int canvas_x = sprite.x() * 16;
    int canvas_y = sprite.y() * 16;

    if (canvas_x >= -16 && canvas_y >= -16 && canvas_x < 512 + 16 &&
        canvas_y < 512 + 16) {
      // Draw 16x16 square for sprite (like overworld entities)
      ImVec4 sprite_color;

      // Color-code sprites based on layer
      if (sprite.layer() == 0) {
        sprite_color = theme.dungeon_sprite_layer0;  // Green for layer 0
      } else {
        sprite_color = theme.dungeon_sprite_layer1;  // Blue for layer 1
      }

      // Draw filled square using runtime-based helper
      gui::DrawRect(rt, canvas_x, canvas_y, 16, 16, sprite_color);

      // Draw sprite ID and name using unified ResourceLabelProvider
      std::string full_name = zelda3::GetSpriteLabel(sprite.id());
      std::string sprite_text;
      // Truncate long names for display
      if (full_name.length() > 12) {
        sprite_text = absl::StrFormat("%02X %s..", sprite.id(),
                                      full_name.substr(0, 8).c_str());
      } else {
        sprite_text =
            absl::StrFormat("%02X %s", sprite.id(), full_name.c_str());
      }

      gui::DrawText(rt, sprite_text, canvas_x, canvas_y);
    }
  }
}

void DungeonCanvasViewer::RenderPotItems(const gui::CanvasRuntime& rt,
                                         const zelda3::Room& room) {
  // Skip if pot items are not visible
  if (!entity_visibility_.show_pot_items) {
    return;
  }

  const auto& pot_items = room.GetPotItems();

  // If no pot items in this room, nothing to render
  if (pot_items.empty()) {
    return;
  }

  // Pot item names
  static const char* kPotItemNames[] = {
      "Nothing",        // 0
      "Green Rupee",    // 1
      "Rock",           // 2
      "Bee",            // 3
      "Health",         // 4
      "Bomb",           // 5
      "Heart",          // 6
      "Blue Rupee",     // 7
      "Key",            // 8
      "Arrow",          // 9
      "Bomb",           // 10
      "Heart",          // 11
      "Magic",          // 12
      "Full Magic",     // 13
      "Cucco",          // 14
      "Green Soldier",  // 15
      "Bush Stal",      // 16
      "Blue Soldier",   // 17
      "Landmine",       // 18
      "Heart",          // 19
      "Fairy",          // 20
      "Heart",          // 21
      "Nothing",        // 22
      "Hole",           // 23
      "Warp",           // 24
      "Staircase",      // 25
      "Bombable",       // 26
      "Switch"          // 27
  };
  constexpr size_t kPotItemNameCount =
      sizeof(kPotItemNames) / sizeof(kPotItemNames[0]);

  // Pot items now have their own position data from ROM
  // No need to match to objects - each item has exact pixel coordinates
  for (const auto& pot_item : pot_items) {
    // Get pixel coordinates from PotItem structure
    int pixel_x = pot_item.GetPixelX();
    int pixel_y = pot_item.GetPixelY();

    // Convert to canvas coordinates (already in pixels, just need offset)
    // Note: pot item coords are already in full room pixel space
    auto [canvas_x, canvas_y] =
        DungeonRenderingHelpers::RoomToCanvasCoordinates(pixel_x / 8, pixel_y / 8);

    if (canvas_x >= -16 && canvas_y >= -16 && canvas_x < 512 + 16 &&
        canvas_y < 512 + 16) {
      // Draw colored square
      ImVec4 pot_item_color;
      if (pot_item.item == 0) {
        pot_item_color = ImVec4(0.5f, 0.5f, 0.5f, 0.5f);  // Gray for Nothing
      } else {
        pot_item_color = ImVec4(1.0f, 0.85f, 0.2f, 0.75f);  // Yellow for items
      }

      gui::DrawRect(rt, canvas_x, canvas_y, 16, 16, pot_item_color);

      // Get item name
      std::string item_name;
      if (pot_item.item < kPotItemNameCount) {
        item_name = kPotItemNames[pot_item.item];
      } else {
        item_name = absl::StrFormat("Unk%02X", pot_item.item);
      }

      // Draw label above the box
      std::string item_text =
          absl::StrFormat("%02X %s", pot_item.item, item_name.c_str());
      gui::DrawText(rt, item_text, canvas_x, canvas_y - 10);
    }
  }
}

void DungeonCanvasViewer::RenderEntityOverlay(const gui::CanvasRuntime& rt,
                                              const zelda3::Room& room) {
  // Render all entity overlays using runtime-based helpers
  RenderSprites(rt, room);
  RenderPotItems(rt, room);
}


// Room layout visualization
// Room layout visualization

// Object visualization methods
void DungeonCanvasViewer::DrawObjectPositionOutlines(
    const gui::CanvasRuntime& rt, const zelda3::Room& room) {
  // Draw colored rectangles showing object positions
  // This helps visualize object placement even if graphics don't render
  // correctly

  const auto& theme = AgentUI::GetTheme();
  const auto& objects = room.GetTileObjects();

  // Create ObjectDrawer for accurate dimension calculation
  // ObjectDrawer uses game-accurate draw routine mapping to determine sizes
  // Note: const_cast needed because rom() accessor is non-const, but we don't
  // modify ROM
  zelda3::ObjectDrawer drawer(const_cast<zelda3::Room&>(room).rom(), room.id(),
                              nullptr);

  for (const auto& obj : objects) {
    // Filter by object type (default to true if unknown type)
    bool show_this_type = true;  // Default to showing
    if (obj.id_ < 0x100) {
      show_this_type = object_outline_toggles_.show_type1_objects;
    } else if (obj.id_ >= 0x100 && obj.id_ < 0x200) {
      show_this_type = object_outline_toggles_.show_type2_objects;
    } else if (obj.id_ >= 0xF00) {
      show_this_type = object_outline_toggles_.show_type3_objects;
    }
    // else: unknown type, use default (true)

    // Filter by layer (default to true if unknown layer)
    bool show_this_layer = true;  // Default to showing
    if (obj.GetLayerValue() == 0) {
      show_this_layer = object_outline_toggles_.show_layer0_objects;
    } else if (obj.GetLayerValue() == 1) {
      show_this_layer = object_outline_toggles_.show_layer1_objects;
    } else if (obj.GetLayerValue() == 2) {
      show_this_layer = object_outline_toggles_.show_layer2_objects;
    }
    // else: unknown layer, use default (true)

    // Skip if filtered out
    if (!show_this_type || !show_this_layer) {
      continue;
    }

    // Convert object position (tile coordinates) to canvas pixel coordinates
    // (UNSCALED)
    auto [canvas_x, canvas_y] = DungeonRenderingHelpers::RoomToCanvasCoordinates(
        obj.x(), obj.y());

    // Calculate object dimensions via DimensionService
    auto [width, height] =
        zelda3::DimensionService::Get().GetPixelDimensions(obj);

    // IMPORTANT: Do NOT apply canvas scale here - DrawRect handles it
    // Clamp to reasonable sizes (in logical space)
    width = std::min(width, 512);
    height = std::min(height, 512);

    // Color-code by layer
    ImVec4 outline_color;
    if (obj.GetLayerValue() == 0) {
      outline_color = theme.dungeon_outline_layer0;  // Red for layer 0
    } else if (obj.GetLayerValue() == 1) {
      outline_color = theme.dungeon_outline_layer1;  // Green for layer 1
    } else {
      outline_color = theme.dungeon_outline_layer2;  // Blue for layer 2
    }

    // Draw outline rectangle using runtime-based helper
    gui::DrawRect(rt, canvas_x, canvas_y, width, height, outline_color);

    // Draw object ID label with hex ID and abbreviated name
    // Format: "0xNN Name" where name is truncated if needed
    std::string name = GetObjectName(obj.id_);
    // Truncate name to fit (approx 12 chars for small objects)
    if (name.length() > 12) {
      name = name.substr(0, 10) + "..";
    }
    std::string label;
    if (obj.id_ >= 0x100) {
      label = absl::StrFormat("0x%03X\n%s\n[%dx%d]", obj.id_, name.c_str(),
                              width, height);
    } else {
      label = absl::StrFormat("0x%02X\n%s\n[%dx%d]", obj.id_, name.c_str(),
                              width, height);
    }
    gui::DrawText(rt, label, canvas_x + 1, canvas_y + 1);
  }
}

const DungeonRenderingHelpers::CollisionOverlayCache&
DungeonCanvasViewer::GetCollisionOverlayCache(int room_id) {
  auto it = collision_overlay_cache_.find(room_id);
  if (it != collision_overlay_cache_.end()) {
    return it->second;
  }

  DungeonRenderingHelpers::CollisionOverlayCache cache;
  cache.entries.clear();

  if (!rom_ || !rom_->is_loaded()) {
    collision_overlay_cache_.emplace(room_id, cache);
    return collision_overlay_cache_.at(room_id);
  }

  auto map_or = zelda3::LoadCustomCollisionMap(rom_, room_id);
  if (!map_or.ok()) {
    collision_overlay_cache_.emplace(room_id, cache);
    return collision_overlay_cache_.at(room_id);
  }

  const auto& map = map_or.value();
  cache.has_data = map.has_data;
  if (cache.has_data && !track_collision_config_.IsEmpty()) {
    for (int y = 0; y < 64; ++y) {
      for (int x = 0; x < 64; ++x) {
        const uint8_t tile = map.tiles[static_cast<size_t>(y * 64 + x)];
        if (tile < 256 && (track_collision_config_.track_tiles[tile] ||
                           track_collision_config_.stop_tiles[tile] ||
                           track_collision_config_.switch_tiles[tile])) {
          cache.entries.push_back(DungeonRenderingHelpers::CollisionOverlayEntry{
              static_cast<uint8_t>(x), static_cast<uint8_t>(y), tile});
        }
      }
    }
  }

  collision_overlay_cache_.emplace(room_id, std::move(cache));
  return collision_overlay_cache_.at(room_id);
}

// Room graphics management methods
absl::Status DungeonCanvasViewer::LoadAndRenderRoomGraphics(int room_id) {
  LOG_DEBUG("[LoadAndRender]", "START room_id=%d", room_id);

  if (room_id < 0 || room_id >= 128) {
    LOG_DEBUG("[LoadAndRender]", "ERROR: Invalid room ID");
    return absl::InvalidArgumentError("Invalid room ID");
  }

  if (!rom_ || !rom_->is_loaded()) {
    LOG_DEBUG("[LoadAndRender]", "ERROR: ROM not loaded");
    return absl::FailedPreconditionError("ROM not loaded");
  }

  if (!rooms_) {
    LOG_DEBUG("[LoadAndRender]", "ERROR: Room data not available");
    return absl::FailedPreconditionError("Room data not available");
  }

  auto& room = (*rooms_)[room_id];
  LOG_DEBUG("[LoadAndRender]", "Got room reference");

  // Load room graphics with proper blockset
  LOG_DEBUG("[LoadAndRender]", "Loading graphics for blockset %d",
            room.blockset);
  room.LoadRoomGraphics(room.blockset);
  LOG_DEBUG("[LoadAndRender]", "Graphics loaded");

  // Load the room's palette with bounds checking
  if (!game_data_) {
    LOG_ERROR("[LoadAndRender]", "GameData not available");
    return absl::FailedPreconditionError("GameData not available");
  }
  const auto& dungeon_main = game_data_->palette_groups.dungeon_main;
  if (!dungeon_main.empty()) {
    int palette_id = room.palette;
    if (room.palette < game_data_->paletteset_ids.size()) {
      palette_id = game_data_->paletteset_ids[room.palette][0];
    }
    current_palette_group_id_ = std::min<uint64_t>(
        std::max(0, palette_id), static_cast<int>(dungeon_main.size() - 1));

    auto full_palette = dungeon_main[current_palette_group_id_];
    ASSIGN_OR_RETURN(current_palette_group_,
                     gfx::CreatePaletteGroupFromLargePalette(full_palette, 16));
    LOG_DEBUG("[LoadAndRender]", "Palette loaded: group_id=%zu",
              current_palette_group_id_);
  }

  // Render the room graphics (self-contained - handles all palette application)
  LOG_DEBUG("[LoadAndRender]", "Calling room.RenderRoomGraphics()...");
  room.RenderRoomGraphics();
  LOG_DEBUG("[LoadAndRender]",
            "RenderRoomGraphics() complete - room buffers self-contained");

  LOG_DEBUG("[LoadAndRender]", "SUCCESS");
  return absl::OkStatus();
}

void DungeonCanvasViewer::DrawRoomBackgroundLayers(int room_id) {
  if (room_id < 0 || room_id >= zelda3::NumberOfRooms || !rooms_)
    return;

  auto& room = (*rooms_)[room_id];
  auto& layer_mgr = GetRoomLayerManager(room_id);

  // Apply room's layer merging settings to the manager
  layer_mgr.ApplyLayerMerging(room.layer_merging());

  float scale = canvas_.global_scale();

  // Always use composite mode: single merged bitmap with back-to-front layer order
  // This matches SNES hardware behavior where BG2 is drawn first, then BG1 on top
  auto& composite = room.GetCompositeBitmap(layer_mgr);
  if (composite.is_active() && composite.width() > 0) {
    // Ensure texture exists or is updated when bitmap data changes
    if (!composite.texture()) {
      gfx::Arena::Get().QueueTextureCommand(
          gfx::Arena::TextureCommandType::CREATE, &composite);
      composite.set_modified(false);
    } else if (composite.modified()) {
      // Update texture when bitmap was regenerated
      gfx::Arena::Get().QueueTextureCommand(
          gfx::Arena::TextureCommandType::UPDATE, &composite);
      composite.set_modified(false);
    }
    if (composite.texture()) {
      canvas_.DrawBitmap(composite, 0, 0, scale, 255);
    }
  }
}

void DungeonCanvasViewer::DrawMaskHighlights(const gui::CanvasRuntime& rt,
                                             const zelda3::Room& room) {
  // Draw semi-transparent blue overlay on BG2/Layer 1 objects when mask mode
  // is active. This helps identify which objects are the "overlay" content
  // (platforms, statues, stairs) that create transparency holes in BG1.
  const auto& objects = room.GetTileObjects();

  // Create ObjectDrawer for dimension calculation
  zelda3::ObjectDrawer drawer(const_cast<zelda3::Room&>(room).rom(), room.id(),
                              nullptr);

  // Mask highlight color: semi-transparent cyan/blue
  // DrawRect draws a filled rectangle with a black outline
  ImVec4 mask_color(0.2f, 0.6f, 1.0f, 0.4f);  // Light blue, 40% opacity

  for (const auto& obj : objects) {
    // Only highlight Layer 1 (BG2) objects - these are the mask/overlay objects
    if (obj.GetLayerValue() != 1) {
      continue;
    }

    // Convert object position to canvas coordinates
    auto [canvas_x, canvas_y] = DungeonRenderingHelpers::RoomToCanvasCoordinates(
        obj.x(), obj.y());

    // Calculate object dimensions via DimensionService
    auto [width, height] =
        zelda3::DimensionService::Get().GetPixelDimensions(obj);

    // Clamp to reasonable sizes
    width = std::min(width, 512);
    height = std::min(height, 512);

    // Draw filled rectangle with semi-transparent overlay (includes black outline)
    gui::DrawRect(rt, canvas_x, canvas_y, width, height, mask_color);
  }
}

void DungeonCanvasViewer::DrawRoomHeader(zelda3::Room& room, int room_id) {
  ImGui::Separator();
  if (header_read_only_) ImGui::BeginDisabled();

  constexpr ImGuiTableFlags kPropsTableFlags =
      ImGuiTableFlags_NoPadOuterX | ImGuiTableFlags_NoBordersInBody;

  if (ImGui::BeginTable("##RoomPropsTable", 2, kPropsTableFlags)) {
    const float nav_col_width = (ImGui::GetFrameHeight() * 4.0f) +
                                (ImGui::GetStyle().ItemSpacing.x * 3.0f) +
                                (ImGui::GetStyle().FramePadding.x * 2.0f);
    ImGui::TableSetupColumn("NavCol", ImGuiTableColumnFlags_WidthFixed, nav_col_width);
    ImGui::TableSetupColumn("PropsCol", ImGuiTableColumnFlags_WidthStretch);

    ImGui::TableNextRow();
    ImGui::TableNextColumn();
    DrawRoomNavigation(room_id);
    ImGui::TableNextColumn();
    DrawRoomPropertyTable(room, room_id);

    if (!compact_header_mode_ || show_room_details_) {
      ImGui::TableNextRow();
      ImGui::TableNextColumn();
      ImGui::TextDisabled(ICON_MD_SELECT_ALL " Select");
      ImGui::TableNextColumn();
      DrawLayerControls(room, room_id);
    }

    ImGui::EndTable();
  }

  if (header_read_only_) ImGui::EndDisabled();
}

void DungeonCanvasViewer::DrawRoomNavigation(int room_id) {
  if (!room_swap_callback_ && !room_navigation_callback_) return;

  const int col = room_id % kRoomMatrixCols;
  const int row = room_id / kRoomMatrixCols;

  auto room_if_valid = [](int candidate) -> std::optional<int> {
    if (candidate < 0 || candidate >= zelda3::NumberOfRooms) return std::nullopt;
    return candidate;
  };

  const auto north = room_if_valid(row > 0 ? room_id - kRoomMatrixCols : -1);
  const auto south = room_if_valid(row < kRoomMatrixRows - 1 ? room_id + kRoomMatrixCols : -1);
  const auto west = room_if_valid(col > 0 ? room_id - 1 : -1);
  const auto east = room_if_valid(col < kRoomMatrixCols - 1 ? room_id + 1 : -1);

  auto make_tooltip = [](const std::optional<int>& target, const char* direction) -> std::string {
    if (!target.has_value()) return "";
    return absl::StrFormat("%s: [%03X] %s", direction, *target, zelda3::GetRoomLabel(*target));
  };

  auto nav_button = [&](const char* id, ImGuiDir dir, const std::optional<int>& target, const std::string& tooltip) {
    const bool enabled = target.has_value();
    if (!enabled) ImGui::BeginDisabled();
    if (ImGui::ArrowButton(id, dir) && enabled) {
      if (room_swap_callback_) room_swap_callback_(room_id, *target);
      else if (room_navigation_callback_) room_navigation_callback_(*target);
    }
    if (!enabled) ImGui::EndDisabled();
    if (enabled && ImGui::IsItemHovered()) ImGui::SetTooltip("%s", tooltip.c_str());
  };

  ImGui::BeginGroup();
  nav_button("RoomNavWest", ImGuiDir_Left, west, make_tooltip(west, "West"));
  ImGui::SameLine();
  nav_button("RoomNavNorth", ImGuiDir_Up, north, make_tooltip(north, "North"));
  ImGui::SameLine();
  nav_button("RoomNavSouth", ImGuiDir_Down, south, make_tooltip(south, "South"));
  ImGui::SameLine();
  nav_button("RoomNavEast", ImGuiDir_Right, east, make_tooltip(east, "East"));
  ImGui::EndGroup();
}

void DungeonCanvasViewer::DrawRoomPropertyTable(zelda3::Room& room, int room_id) {
  ImGui::AlignTextToFramePadding();
  ImGui::Text(ICON_MD_TUNE " %03X", room_id);
  ImGui::SameLine();

  if (pin_callback_) {
    if (ImGui::SmallButton(is_pinned_ ? ICON_MD_PUSH_PIN "##RoomPin" : ICON_MD_PIN "##RoomPin")) {
      pin_callback_(!is_pinned_);
    }
    if (ImGui::IsItemHovered()) ImGui::SetTooltip(is_pinned_ ? "Unpin Room" : "Pin Room");
    ImGui::SameLine();
  }

  if (ImGui::SmallButton(show_room_details_ ? ICON_MD_EXPAND_LESS : ICON_MD_EXPAND_MORE)) {
    show_room_details_ = !show_room_details_;
  }
  ImGui::SameLine();

  // Core properties (Blockset, Palette, Layout, Spriteset)
  auto hex_input = [&](const char* label, const char* icon, uint8_t* val, uint8_t max, const char* tooltip) {
    ImGui::TextDisabled("%s", icon);
    ImGui::SameLine(0, 2);
    if (gui::InputHexByteEx(label, val, max, 32.f, true).ShouldApply()) {
      return true;
    }
    if (ImGui::IsItemHovered()) ImGui::SetTooltip("%s", tooltip);
    return false;
  };

  uint8_t bs = room.blockset;
  if (hex_input("##BS", ICON_MD_VIEW_MODULE, &bs, 81, "Blockset")) {
    room.SetBlockset(bs);
    if (room.rom() && room.rom()->is_loaded()) room.RenderRoomGraphics();
  }
  ImGui::SameLine();
  
  uint8_t pal = room.palette;
  if (hex_input("##Pal", ICON_MD_PALETTE, &pal, 71, "Palette")) {
    room.SetPalette(pal);
    // ... palette update logic ...
    if (room.rom() && room.rom()->is_loaded()) room.RenderRoomGraphics();
  }
  ImGui::SameLine();

  uint8_t lyr = room.layout;
  if (hex_input("##Lyr", ICON_MD_GRID_VIEW, &lyr, 7, "Layout")) {
    room.layout = lyr;
    room.MarkLayoutDirty();
    if (room.rom() && room.rom()->is_loaded()) room.RenderRoomGraphics();
  }
  ImGui::SameLine();

  uint8_t ss = room.spriteset;
  if (hex_input("##SS", ICON_MD_PEST_CONTROL, &ss, 143, "Spriteset")) {
    room.SetSpriteset(ss);
    if (room.rom() && room.rom()->is_loaded()) room.RenderRoomGraphics();
  }

  if (show_room_details_) {
    // Floor and Effects (simplified for plan)
    ImGui::TableNextRow(); ImGui::TableNextColumn(); ImGui::TableNextColumn();
    ImGui::TextDisabled("Ext Props: Floor/Effect/Tags...");
  }
}

void DungeonCanvasViewer::DrawLayerControls(zelda3::Room& room, int room_id) {
  const auto& theme = AgentUI::GetTheme();
  auto& interaction = object_interaction_;
  
  interaction.SetLayersMerged(GetRoomLayerManager(room_id).AreLayersMerged());
  int current_filter = interaction.GetLayerFilter();
  
  auto radio = [&](const char* label, int filter) {
    if (ImGui::RadioButton(label, current_filter == filter)) {
      interaction.SetLayerFilter(filter);
    }
    ImGui::SameLine();
  };

  radio("All", ObjectSelection::kLayerAll);
  radio("L1", ObjectSelection::kLayer1);
  radio("L2", ObjectSelection::kLayer2);
  radio("L3", ObjectSelection::kLayer3);
}

}  // namespace yaze::editor
