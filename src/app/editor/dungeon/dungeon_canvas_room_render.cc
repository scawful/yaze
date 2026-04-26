#include "dungeon_canvas_viewer.h"

#include <cstdio>

#include "app/gfx/resource/arena.h"
#include "app/gui/core/drag_drop.h"
#include "app/gui/widgets/themed_widgets.h"
#include "imgui/imgui.h"

namespace yaze::editor {

void DungeonCanvasViewer::SyncCanvasCaptureRegion(
    const gui::CanvasRuntime& canvas_rt) {
  has_canvas_capture_region_ =
      canvas_rt.canvas_sz.x > 1.0f && canvas_rt.canvas_sz.y > 1.0f;
  if (!has_canvas_capture_region_) {
    return;
  }

  canvas_capture_x_ = static_cast<int>(canvas_rt.canvas_p0.x);
  canvas_capture_y_ = static_cast<int>(canvas_rt.canvas_p0.y);
  canvas_capture_width_ = static_cast<int>(canvas_rt.canvas_sz.x);
  canvas_capture_height_ = static_cast<int>(canvas_rt.canvas_sz.y);
}

void DungeonCanvasViewer::ConsumePendingCanvasScroll(
    const gui::CanvasRuntime& canvas_rt) {
  if (!pending_scroll_target_.has_value()) {
    return;
  }

  constexpr int kRoomPixelWidth = 512;
  constexpr int kRoomPixelHeight = 512;
  constexpr int kDungeonTileSize = 8;

  const auto [target_x, target_y] = pending_scroll_target_.value();
  float scale = canvas_.global_scale();
  if (scale <= 0.0f) {
    scale = 1.0f;
  }

  const float pixel_x = static_cast<float>(target_x * kDungeonTileSize) * scale;
  const float pixel_y = static_cast<float>(target_y * kDungeonTileSize) * scale;
  const ImVec2 view_size = canvas_rt.canvas_sz;
  const ImVec2 content_size(static_cast<float>(kRoomPixelWidth) * scale,
                            static_cast<float>(kRoomPixelHeight) * scale);

  const ImVec2 desired_scroll((view_size.x * 0.5f) - pixel_x,
                              (view_size.y * 0.5f) - pixel_y);
  canvas_.set_scrolling(
      gui::ClampScroll(desired_scroll, content_size, view_size));
  pending_scroll_target_.reset();
}

void DungeonCanvasViewer::DrawHeaderHiddenMetadataHud(int room_id) {
  if (header_visible_ || !show_header_hidden_metadata_hud_) {
    return;
  }

  const auto& label = zelda3::GetRoomLabel(room_id);
  char text1[160];
  snprintf(text1, sizeof(text1), "[%03X] %s", room_id, label.c_str());

  char text2[96] = {};
  bool show_meta = false;
  if (rooms_ && room_id >= 0 && room_id < static_cast<int>(rooms_->size())) {
    const auto& room = (*rooms_)[room_id];
    if (!object_interaction_enabled_) {
      snprintf(text2, sizeof(text2), "B:%02X P:%02X L:%02X S:%02X  RO",
               room.blockset(), room.palette(), room.layout_id(),
               room.spriteset());
    } else {
      snprintf(text2, sizeof(text2), "B:%02X P:%02X L:%02X S:%02X",
               room.blockset(), room.palette(), room.layout_id(),
               room.spriteset());
    }
    show_meta = true;
  } else if (!object_interaction_enabled_) {
    snprintf(text2, sizeof(text2), "Read-only");
    show_meta = true;
  }

  const float pad = 10.0f;
  const ImVec2 hud_pos(canvas_.zero_point().x + pad,
                       canvas_.zero_point().y + pad);
  const ImVec2 hud_size(0, 0);

  gui::DrawCanvasHUD("##MetadataHUD", hud_pos, hud_size, [&]() {
    ImGui::TextUnformatted(text1);
    if (show_meta) {
      ImGui::TextDisabled("%s", text2);
    }
  });
}

void DungeonCanvasViewer::PrepareRoomStateForCanvas(zelda3::Room& room,
                                                    int room_id) {
  object_interaction_.SetCurrentRoom(rooms_, room_id);

  if (!room.AreObjectsLoaded()) {
    room.LoadObjects();
  }

  if (!room.AreSpritesLoaded()) {
    room.LoadSprites();
  }

  if (!room.ArePotItemsLoaded()) {
    room.LoadPotItems();
  }

  auto& bg1_bitmap = room.bg1_buffer().bitmap();
  const bool needs_render = !bg1_bitmap.is_active() || bg1_bitmap.width() == 0;

  static int last_rendered_room = -1;
  static bool has_rendered = false;
  if (needs_render && (last_rendered_room != room_id || !has_rendered)) {
    (void)LoadAndRenderRoomGraphics(room_id);
    last_rendered_room = room_id;
    has_rendered = true;
  }

  if (rom_ && rom_->is_loaded()) {
    gfx::Arena::Get().ProcessTextureQueue(renderer_);
  }
}

void DungeonCanvasViewer::HandleRoomCanvasDropTargets(zelda3::Room& room,
                                                      int room_id) {
  gui::RoomObjectDragPayload obj_drop;
  if (gui::AcceptRoomObjectDrop(&obj_drop)) {
    auto [tile_x, tile_y] = DungeonRenderingHelpers::ScreenToRoomCoordinates(
        ImGui::GetMousePos(), canvas_.zero_point(), canvas_.global_scale());
    if (tile_x >= 0 && tile_x < 64 && tile_y >= 0 && tile_y < 64) {
      const uint8_t object_size = obj_drop.size;
      zelda3::RoomObject new_obj(static_cast<int16_t>(obj_drop.object_id),
                                 static_cast<uint8_t>(tile_x),
                                 static_cast<uint8_t>(tile_y), object_size, 0);
      const size_t before = room.GetTileObjects().size();
      object_interaction_.entity_coordinator().tile_handler().PlaceObjectAt(
          room_id, new_obj, tile_x, tile_y);
      if (room.GetTileObjects().size() > before) {
        object_interaction_.SetSelectedObjects({before});
      }
    }
  }

  gui::SpriteDragPayload sprite_drop;
  if (gui::AcceptSpriteDrop(&sprite_drop)) {
    auto [tile_x, tile_y] = DungeonRenderingHelpers::ScreenToRoomCoordinates(
        ImGui::GetMousePos(), canvas_.zero_point(), canvas_.global_scale());
    int sprite_x = (tile_x * 8) / 16;
    int sprite_y = (tile_y * 8) / 16;
    if (sprite_x >= 0 && sprite_x < 32 && sprite_y >= 0 && sprite_y < 32) {
      zelda3::Sprite new_sprite(static_cast<uint8_t>(sprite_drop.sprite_id),
                                static_cast<uint8_t>(sprite_x),
                                static_cast<uint8_t>(sprite_y), 0, 0);
      if (auto* ctx = object_interaction_.entity_coordinator()
                          .sprite_handler()
                          .context()) {
        ctx->NotifyMutation(MutationDomain::kSprites);
      }
      room.GetSprites().push_back(new_sprite);
      room.MarkSpritesDirty();
      if (auto* ctx = object_interaction_.entity_coordinator()
                          .sprite_handler()
                          .context()) {
        ctx->NotifyInvalidateCache(MutationDomain::kSprites);
      }
    }
  }
}

void DungeonCanvasViewer::DrawRoomCanvasContent(
    const gui::CanvasRuntime& canvas_rt, zelda3::Room& room, int room_id) {
  PrepareRoomStateForCanvas(room, room_id);

  DrawRoomBackgroundLayers(room_id);

  if (object_interaction_.IsMaskModeActive()) {
    DrawMaskHighlights(canvas_rt, room);
  }

  RenderEntityOverlay(canvas_rt, room);

  if (object_interaction_enabled_) {
    object_interaction_.HandleCanvasMouseInput();
    object_interaction_.CheckForObjectSelection();
    object_interaction_.DrawSelectionHighlights();
    object_interaction_.DrawEntitySelectionHighlights();
    object_interaction_.DrawGhostPreview();

    const auto selected = object_interaction_.GetSelectedObjectIndices();
    if (selected.size() == 1) {
      const auto& objects = room.GetTileObjects();
      const size_t idx = selected.front();
      if (idx < objects.size()) {
        const auto& obj = objects[idx];
        gui::BeginRoomObjectDragSource(static_cast<uint16_t>(obj.id_), room_id,
                                       obj.x_, obj.y_, obj.size_);
      }
    }

    if (object_interaction_.HasEntitySelection()) {
      const auto sel = object_interaction_.GetSelectedEntity();
      if (sel.type == EntityType::Sprite) {
        const auto& sprites = room.GetSprites();
        if (sel.index < sprites.size()) {
          const auto& sprite = sprites[sel.index];
          gui::BeginSpriteDragSource(sprite.id(), room_id);
        }
      }
    }

    HandleTouchLongPressContextMenu(canvas_rt, room);
  }

  HandleRoomCanvasDropTargets(room, room_id);
}

}  // namespace yaze::editor
