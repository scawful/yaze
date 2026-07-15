#include "dungeon_canvas_viewer.h"
#include "util/i18n/tr.h"

#include <algorithm>
#include <cstdio>
#include <string>

#include "absl/strings/str_format.h"
#include "app/editor/dungeon/dungeon_canvas_transform.h"
#include "app/editor/dungeon/ui/window/minecart_track_editor_panel.h"
#include "app/gfx/resource/arena.h"
#include "app/gui/core/agent_theme.h"
#include "app/gui/core/color.h"
#include "app/gui/core/ui_helpers.h"
#include "app/gui/widgets/themed_widgets.h"
#include "dungeon_coordinates.h"
#include "imgui/imgui.h"
#include "zelda3/dungeon/editor_dungeon_state.h"

namespace yaze::editor {

void DungeonCanvasViewer::DrawPersistentDebugWindows(int room_id) {
  if (!rooms_ || !rom_ || !rom_->is_loaded()) {
    return;
  }

  auto& room = (*rooms_)[room_id];
  if (show_room_debug_info_) {
    DrawRoomDebugWindow(room, room_id);
  }
  if (show_texture_debug_) {
    DrawTextureDebugWindow(room, room_id);
  }
  if (show_layer_info_) {
    DrawLayerInfoWindow(room_id);
  }
}

void DungeonCanvasViewer::DrawRoomDebugWindow(zelda3::Room& room, int room_id) {
  ImGui::SetNextWindowPos(
      ImVec2(canvas_.zero_point().x + 10, canvas_.zero_point().y + 10),
      ImGuiCond_FirstUseEver);
  ImGui::SetNextWindowSize(ImVec2(300, 0), ImGuiCond_FirstUseEver);
  if (ImGui::Begin("Room Debug Info", &show_room_debug_info_,
                   ImGuiWindowFlags_NoCollapse)) {
    ImGui::Text(tr("Room: 0x%03X (%d)"), room_id, room_id);
    ImGui::Separator();
    ImGui::Text(tr("Graphics"));
    ImGui::Text(tr("  Blockset: 0x%02X"), room.blockset());
    ImGui::Text(tr("  Palette: 0x%02X"), room.palette());
    ImGui::Text(tr("  Layout: 0x%02X"), room.layout_id());
    ImGui::Text(tr("  Spriteset: 0x%02X"), room.spriteset());
    ImGui::Separator();
    ImGui::Text(tr("Content"));
    ImGui::Text(tr("  Objects: %zu"), room.GetTileObjects().size());
    ImGui::Text(tr("  Sprites: %zu"), room.GetSprites().size());
    ImGui::Separator();
    ImGui::Text(tr("Buffers"));
    auto& bg1 = room.bg1_buffer().bitmap();
    auto& bg2 = room.bg2_buffer().bitmap();
    ImGui::Text(tr("  BG1: %dx%d %s"), bg1.width(), bg1.height(),
                bg1.texture() ? "(has texture)" : "(NO TEXTURE)");
    ImGui::Text(tr("  BG2: %dx%d %s"), bg2.width(), bg2.height(),
                bg2.texture() ? "(has texture)" : "(NO TEXTURE)");
    ImGui::Separator();
    ImGui::Text(tr("Layers (4-way)"));
    auto& layer_mgr = GetRoomLayerManager(room_id);
    bool bg1l = layer_mgr.IsLayerVisible(zelda3::LayerType::BG1_Layout);
    bool bg1o = layer_mgr.IsLayerVisible(zelda3::LayerType::BG1_Objects);
    bool bg2l = layer_mgr.IsLayerVisible(zelda3::LayerType::BG2_Layout);
    bool bg2o = layer_mgr.IsLayerVisible(zelda3::LayerType::BG2_Objects);
    if (ImGui::Checkbox(tr("BG1 Layout"), &bg1l)) {
      layer_mgr.SetLayerVisible(zelda3::LayerType::BG1_Layout, bg1l);
    }
    if (ImGui::Checkbox(tr("BG1 Objects"), &bg1o)) {
      layer_mgr.SetLayerVisible(zelda3::LayerType::BG1_Objects, bg1o);
    }
    if (ImGui::Checkbox(tr("BG2 Layout"), &bg2l)) {
      layer_mgr.SetLayerVisible(zelda3::LayerType::BG2_Layout, bg2l);
    }
    if (ImGui::Checkbox(tr("BG2 Objects"), &bg2o)) {
      layer_mgr.SetLayerVisible(zelda3::LayerType::BG2_Objects, bg2o);
    }
    int blend = static_cast<int>(
        layer_mgr.GetLayerBlendMode(zelda3::LayerType::BG2_Layout));
    if (ImGui::SliderInt(tr("BG2 Blend"), &blend, 0, 4)) {
      layer_mgr.SetLayerBlendMode(zelda3::LayerType::BG2_Layout,
                                  static_cast<zelda3::LayerBlendMode>(blend));
      layer_mgr.SetLayerBlendMode(zelda3::LayerType::BG2_Objects,
                                  static_cast<zelda3::LayerBlendMode>(blend));
    }

    ImGui::Separator();
    ImGui::Text(tr("Layout Override"));
    static bool enable_override = false;
    ImGui::Checkbox(tr("Enable Override"), &enable_override);
    if (enable_override) {
      ImGui::SliderInt(tr("Layout ID"), &layout_override_, 0, 7);
    } else {
      layout_override_ = -1;
    }

    ImGui::Separator();
    ImGui::Text(tr("Preview State"));
    if (auto* editor_state =
            dynamic_cast<zelda3::EditorDungeonState*>(room.GetDungeonState());
        editor_state != nullptr) {
      bool preview_state_changed = false;

      bool water_face_active = editor_state->IsWaterFaceActive(room_id);
      if (ImGui::Checkbox(tr("Water Face Active"), &water_face_active)) {
        editor_state->SetWaterFaceActive(room_id, water_face_active);
        preview_state_changed = true;
      }

      bool dam_floodgate_open = editor_state->IsDamFloodgateOpen(room_id);
      if (ImGui::Checkbox(tr("Dam Floodgate Open"), &dam_floodgate_open)) {
        editor_state->SetDamFloodgateOpen(room_id, dam_floodgate_open);
        preview_state_changed = true;
      }

      bool wall_moved = editor_state->IsWallMoved(room_id);
      if (ImGui::Checkbox(tr("Moving Wall Shifted"), &wall_moved)) {
        editor_state->SetWallMoved(room_id, wall_moved);
        preview_state_changed = true;
      }

      bool floor_bombed = editor_state->IsFloorBombable(room_id);
      if (ImGui::Checkbox(tr("Bombed Floor Open"), &floor_bombed)) {
        editor_state->SetFloorBombable(room_id, floor_bombed);
        preview_state_changed = true;
      }

      bool rupee_floor_active = editor_state->IsRupeeFloorActive(room_id);
      if (ImGui::Checkbox(tr("Rupee Floor Active"), &rupee_floor_active)) {
        editor_state->SetRupeeFloorActive(room_id, rupee_floor_active);
        preview_state_changed = true;
      }

      if (ImGui::Button(tr("Reset Preview State"))) {
        editor_state->Reset();
        preview_state_changed = true;
      }

      if (preview_state_changed) {
        room.ReloadGraphics(current_entrance_blockset_);
      }
    } else {
      ImGui::TextDisabled(
          tr("Preview state controls require EditorDungeonState."));
    }

    if (show_object_bounds_) {
      ImGui::Separator();
      ImGui::Text(tr("Object Outline Filters"));
      ImGui::Text(tr("By Type:"));
      ImGui::Checkbox(tr("Type 1"),
                      &object_outline_toggles_.show_type1_objects);
      ImGui::Checkbox(tr("Type 2"),
                      &object_outline_toggles_.show_type2_objects);
      ImGui::Checkbox(tr("Type 3"),
                      &object_outline_toggles_.show_type3_objects);
      ImGui::Text(tr("By Layer:"));
      ImGui::Checkbox(tr("Layer 1 (Primary)"),
                      &object_outline_toggles_.show_layer0_objects);
      ImGui::Checkbox(tr("Layer 2 (BG2 overlay)"),
                      &object_outline_toggles_.show_layer1_objects);
      ImGui::Checkbox(tr("Layer 3 (BG1 overlay)"),
                      &object_outline_toggles_.show_layer2_objects);
    }
  }
  ImGui::End();
}

void DungeonCanvasViewer::DrawTextureDebugWindow(zelda3::Room& room,
                                                 int room_id) {
  ImGui::SetNextWindowPos(
      ImVec2(canvas_.zero_point().x + 320, canvas_.zero_point().y + 10),
      ImGuiCond_FirstUseEver);
  ImGui::SetNextWindowSize(ImVec2(250, 0), ImGuiCond_FirstUseEver);
  if (ImGui::Begin("Texture Debug", &show_texture_debug_,
                   ImGuiWindowFlags_NoCollapse)) {
    auto& bg1 = room.bg1_buffer().bitmap();
    auto& bg2 = room.bg2_buffer().bitmap();

    auto ensure_bitmap_texture = [this](gfx::Bitmap& bitmap) {
      if (!renderer_ || !bitmap.is_active() || bitmap.width() <= 0) {
        return;
      }
      if (!bitmap.texture()) {
        gfx::Arena::Get().QueueTextureCommand(
            gfx::Arena::TextureCommandType::CREATE, &bitmap);
      } else if (bitmap.modified()) {
        gfx::Arena::Get().QueueTextureCommand(
            gfx::Arena::TextureCommandType::UPDATE, &bitmap);
      }
    };

    ensure_bitmap_texture(bg1);
    ensure_bitmap_texture(bg2);
    if (renderer_) {
      gfx::Arena::Get().ProcessTextureQueue(renderer_);
    }

    ImGui::Text(tr("BG1 Bitmap"));
    ImGui::Text(tr("  Size: %dx%d"), bg1.width(), bg1.height());
    ImGui::Text(tr("  Active: %s"), bg1.is_active() ? "YES" : "NO");
    ImGui::Text(tr("  Texture: 0x%p"), bg1.texture());
    ImGui::Text(tr("  Modified: %s"), bg1.modified() ? "YES" : "NO");

    if (bg1.texture()) {
      ImGui::Text(tr("  Preview:"));
      ImGui::Image((ImTextureID)(intptr_t)bg1.texture(), ImVec2(128, 128));
    }

    ImGui::Separator();
    ImGui::Text(tr("BG2 Bitmap"));
    ImGui::Text(tr("  Size: %dx%d"), bg2.width(), bg2.height());
    ImGui::Text(tr("  Active: %s"), bg2.is_active() ? "YES" : "NO");
    ImGui::Text(tr("  Texture: 0x%p"), bg2.texture());
    ImGui::Text(tr("  Modified: %s"), bg2.modified() ? "YES" : "NO");

    if (bg2.texture()) {
      ImGui::Text(tr("  Preview:"));
      ImGui::Image((ImTextureID)(intptr_t)bg2.texture(), ImVec2(128, 128));
    }
  }
  ImGui::End();
}

void DungeonCanvasViewer::DrawLayerInfoWindow(int room_id) {
  ImGui::SetNextWindowPos(
      ImVec2(canvas_.zero_point().x + 580, canvas_.zero_point().y + 10),
      ImGuiCond_FirstUseEver);
  ImGui::SetNextWindowSize(ImVec2(220, 0), ImGuiCond_FirstUseEver);
  if (ImGui::Begin("Layer Info", &show_layer_info_,
                   ImGuiWindowFlags_NoCollapse)) {
    ImGui::Text(tr("Canvas Scale: %.2f"), canvas_.global_scale());
    ImGui::Text(tr("Canvas Size: %.0fx%.0f"), canvas_.width(),
                canvas_.height());
    auto& layer_mgr = GetRoomLayerManager(room_id);
    ImGui::Separator();
    ImGui::Text(tr("Layer Visibility (4-way):"));

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
    ImGui::Text(tr("Draw Order:"));
    auto draw_order = layer_mgr.GetDrawOrder();
    for (int i = 0; i < 4; ++i) {
      ImGui::Text("  %d: %s", i + 1,
                  zelda3::RoomLayerManager::GetLayerName(draw_order[i]));
    }
    ImGui::Text(tr("BG2 On Top: %s"), layer_mgr.IsBG2OnTop() ? "YES" : "NO");
  }
  ImGui::End();
}

void DungeonCanvasViewer::DrawRoomCanvasOverlays(const gui::CanvasRuntime& rt,
                                                 zelda3::Room& room,
                                                 int room_id) {
  const DungeonCanvasTransform transform(rt.canvas_p0, rt.scrolling, rt.scale);
  const ImVec2 room_origin = transform.room_origin_screen();
  const float scale = transform.scale();

  if (show_object_bounds_) {
    DrawObjectPositionOutlines(rt, room);
  }

  if (show_track_collision_overlay_) {
    DungeonRenderingHelpers::DrawTrackCollisionOverlay(
        ImGui::GetWindowDrawList(), room_origin, scale,
        GetCollisionOverlayCache(room.id()), track_collision_config_,
        track_direction_map_enabled_, track_tile_order_, switch_tile_order_,
        show_track_collision_legend_);
  }

  if (show_custom_collision_overlay_) {
    DungeonRenderingHelpers::DrawCustomCollisionOverlay(
        ImGui::GetWindowDrawList(), room_origin, scale, room);
  }

  if (show_water_fill_overlay_) {
    DungeonRenderingHelpers::DrawWaterFillOverlay(ImGui::GetWindowDrawList(),
                                                  room_origin, scale, room);
  }

  if (show_camera_quadrant_overlay_) {
    DungeonRenderingHelpers::DrawCameraQuadrantOverlay(
        ImGui::GetWindowDrawList(), room_origin, scale, room);
  }

  if (show_minecart_sprite_overlay_) {
    DungeonRenderingHelpers::DrawMinecartSpriteOverlay(
        ImGui::GetWindowDrawList(), room_origin, scale, room,
        minecart_sprite_ids_, track_collision_config_);
  }

  if (show_track_gap_overlay_) {
    DungeonRenderingHelpers::DrawTrackGapOverlay(
        ImGui::GetWindowDrawList(), room_origin, scale, room,
        GetCollisionOverlayCache(room.id()));
  }

  if (show_track_route_overlay_) {
    DungeonRenderingHelpers::DrawTrackRouteOverlay(
        ImGui::GetWindowDrawList(), room_origin, scale,
        GetCollisionOverlayCache(room.id()));
  }

  if (show_custom_objects_overlay_) {
    ImDrawList* draw_list = ImGui::GetWindowDrawList();
    const ImVec2 canvas_pos = room_origin;
    const ImVec4 info = gui::GetInfoColor();
    const ImU32 fill_color =
        ImGui::GetColorU32(ImVec4(info.x, info.y, info.z, 0.25f));
    const ImU32 border_color =
        ImGui::GetColorU32(ImVec4(info.x, info.y, info.z, 0.8f));
    const ImU32 text_bg_color = ImGui::GetColorU32(ImVec4(0, 0, 0, 0.6f));

    auto is_custom = [](int id) {
      return id == 0x31 || id == 0x32;
    };

    for (const auto& obj : room.GetTileObjects()) {
      if (!is_custom(static_cast<int>(obj.id_))) {
        continue;
      }

      const float px = static_cast<float>(obj.x()) * 8.0f * scale;
      const float py = static_cast<float>(obj.y()) * 8.0f * scale;
      const float box_w = 16.0f * scale;
      const float box_h = 16.0f * scale;
      const ImVec2 p0(canvas_pos.x + px, canvas_pos.y + py);
      const ImVec2 p1(p0.x + box_w, p0.y + box_h);

      draw_list->AddRectFilled(p0, p1, fill_color, 2.0f);
      draw_list->AddRect(p0, p1, border_color, 2.0f, 0, 1.5f);

      char label[32];
      std::snprintf(label, sizeof(label), "0x%02X s%d",
                    static_cast<int>(obj.id_),
                    static_cast<int>(obj.size_ & 0x1F));
      const ImVec2 text_sz = ImGui::CalcTextSize(label);
      const ImVec2 tp(p0.x + 1.0f, p0.y - text_sz.y - 1.0f);
      draw_list->AddRectFilled(
          tp, ImVec2(tp.x + text_sz.x + 2.0f, tp.y + text_sz.y), text_bg_color,
          2.0f);
      draw_list->AddText(tp, border_color, label);
    }
  }

  if (minecart_track_panel_) {
    const bool show_tracks =
        show_minecart_tracks_ || minecart_track_panel_->IsPickingCoordinates();
    const auto& tracks = minecart_track_panel_->GetTracks();
    if (show_tracks && !tracks.empty()) {
      ImDrawList* draw_list = ImGui::GetWindowDrawList();
      const ImVec2 canvas_pos = room_origin;
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

        ImVec4 marker_color = theme.selection_primary;
        if (track.id == active_track) {
          marker_color = theme.status_warning;
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

void DungeonCanvasViewer::DrawCoordinateOverlayHud(int room_id) {
  if (!show_coordinate_overlay_ || !canvas_.IsMouseHovering()) {
    return;
  }

  // Plain locals (not structured bindings): the DrawCanvasHUD lambda below
  // captures these, which is ill-formed on clang<16 / gcc<8 (C++17).
  const DungeonCanvasTransform transform(
      canvas_.zero_point(), canvas_.scrolling(), canvas_.global_scale());
  const std::pair<int, int> tile_coords = transform.ScreenToRoomTiles(
      ImGui::GetMousePos(), dungeon_coords::kTileSize);
  const int tile_x = tile_coords.first;
  const int tile_y = tile_coords.second;
  if (tile_x < 0 || tile_x >= 64 || tile_y < 0 || tile_y >= 64) {
    return;
  }

  const int canvas_x = tile_x * 8;
  const int canvas_y = tile_y * 8;
  const std::pair<uint16_t, uint16_t> camera_coords =
      dungeon_coords::TileToCameraCoords(room_id, tile_x, tile_y);
  const uint16_t camera_x = camera_coords.first;
  const uint16_t camera_y = camera_coords.second;
  const int sprite_x = canvas_x / dungeon_coords::kSpriteTileSize;
  const int sprite_y = canvas_y / dungeon_coords::kSpriteTileSize;

  const ImVec2 mouse_pos = ImGui::GetMousePos();
  const ImVec2 overlay_pos(mouse_pos.x + 15, mouse_pos.y + 15);

  gui::DrawCanvasHUD("##CoordHUD", overlay_pos, ImVec2(0, 0), [&]() {
    ImGui::Text(tr("Tile: (%d, %d)"), tile_x, tile_y);
    ImGui::Text(tr("Pixel: (%d, %d)"), canvas_x, canvas_y);
    ImGui::Text(tr("Camera: ($%04X, $%04X)"), camera_x, camera_y);
    ImGui::Text(tr("Sprite: (%d, %d)"), sprite_x, sprite_y);
  });
}

}  // namespace yaze::editor
