#include "app/editor/dungeon/dungeon_canvas_viewer.h"

#include <algorithm>
#include <cstring>
#include <optional>
#include <type_traits>

#include "app/editor/dungeon/dungeon_canvas_transform.h"
#include "app/editor/dungeon/ui/window/minecart_track_editor_panel.h"
#include "app/gfx/resource/arena.h"
#include "app/gui/core/agent_theme.h"
#include "app/gui/core/drag_drop.h"
#include "app/platform/sdl_compat.h"
#include "imgui/imgui_internal.h"
#include "zelda3/dungeon/custom_object.h"
#include "zelda3/dungeon/object_drawer.h"

namespace yaze::editor {

static_assert(std::is_trivially_copyable_v<MinecartToolPayload>);

bool DungeonCanvasViewer::DrawMinecartNodePlacement(
    const gui::CanvasRuntime& rt, zelda3::Room& room, int room_id) {
  auto* panel = minecart_track_panel_;
  if (!panel || !object_interaction_enabled_ || header_read_only()) {
    return false;
  }
  if (panel->assembly_tool_active()) {
    if (panel->active_room() != room_id)
      return true;
    if (room_canvas_shortcut_focus_ && ImGui::IsKeyPressed(ImGuiKey_Escape)) {
      panel->CancelAssemblyPreview();
      return true;
    }
    const DungeonCanvasTransform transform(rt.canvas_p0, rt.scrolling,
                                           rt.scale);
    const auto [mx, my] = transform.ScreenToRoomTiles(ImGui::GetMousePos(), 8);
    if (rt.hovered && mx >= 0 && my >= 0 && mx < 64 && my < 64 &&
        !ImGui::IsMouseDown(ImGuiMouseButton_Right) &&
        !ImGui::IsMouseDown(ImGuiMouseButton_Middle)) {
      panel->UpdateAssemblyPreview(
          room_id, mx & ~1, my & ~1,
          ImGui::IsMouseClicked(ImGuiMouseButton_Left));
    }
    const auto& preview = panel->assembly_preview();
    if (preview && preview->selection.room_id == room_id && rt.draw_list) {
      if (rom_ && rom_->is_loaded() && !room.get_gfx_buffer().empty()) {
        if (!minecart_assembly_preview_buffer_)
          minecart_assembly_preview_buffer_ =
              std::make_unique<gfx::BackgroundBuffer>(512, 512);
        auto& buffer = *minecart_assembly_preview_buffer_;
        if (minecart_assembly_preview_revision_ !=
                panel->assembly_preview_revision() ||
            minecart_assembly_preview_room_ != room_id ||
            minecart_assembly_graphics_revision_ != room.graphics_revision()) {
          buffer.EnsureBitmapInitialized();
          buffer.ClearBuffer();
          auto& bitmap = buffer.bitmap();
          std::fill(bitmap.mutable_data().begin(), bitmap.mutable_data().end(),
                    255);
          if (auto* palette = platform::GetSurfacePalette(
                  room.bg1_buffer().bitmap().surface());
              palette && palette->ncolors > 0) {
            std::vector<SDL_Color> colors(256, {0, 0, 0, 0});
            std::copy_n(palette->colors, std::min(palette->ncolors, 256),
                        colors.begin());
            colors[255] = {0, 0, 0, 0};
            bitmap.SetPalette(colors);
            SDL_SetColorKey(bitmap.surface(), SDL_TRUE, 255);
            SDL_SetSurfaceBlendMode(bitmap.surface(), SDL_BLENDMODE_BLEND);
          }
          zelda3::ObjectDrawer drawer(rom_, room_id,
                                      room.get_gfx_buffer().data());
          drawer.InitializeDrawRoutines();
          drawer.SetRoomFloorGraphics(room.floor1(), room.floor2());
          for (size_t i = preview->selection.before.objects.size();
               i < preview->selection.after.objects.size(); ++i)
            (void)drawer.DrawObject(preview->selection.after.objects[i], buffer,
                                    buffer, current_palette_group_);
          bitmap.set_modified(true);
          bitmap.UpdateSurfacePixels();
          if (bitmap.texture())
            gfx::Arena::Get().QueueTextureCommand(
                gfx::Arena::TextureCommandType::UPDATE, &bitmap);
          else if (!minecart_assembly_texture_queued_) {
            gfx::Arena::Get().QueueTextureCommand(
                gfx::Arena::TextureCommandType::CREATE, &bitmap);
            minecart_assembly_texture_queued_ = true;
          }
          gfx::Arena::Get().ProcessTextureQueue(nullptr);
          if (bitmap.texture())
            minecart_assembly_texture_queued_ = false;
          minecart_assembly_preview_revision_ =
              panel->assembly_preview_revision();
          minecart_assembly_preview_room_ = room_id;
          minecart_assembly_graphics_revision_ = room.graphics_revision();
        }
        rt.draw_list->PushClipRect(
            rt.canvas_p0,
            ImVec2(rt.canvas_p0.x + rt.canvas_sz.x * rt.scale,
                   rt.canvas_p0.y + rt.canvas_sz.y * rt.scale),
            true);
        if (buffer.bitmap().texture())
          rt.draw_list->AddImage(
              (ImTextureID)(intptr_t)buffer.bitmap().texture(),
              transform.room_origin_screen(),
              transform.RoomPixelsToScreen(ImVec2(512, 512)), ImVec2(0, 0),
              ImVec2(1, 1), IM_COL32(255, 255, 255, 190));
      }
      for (size_t i = 0; i < preview->collision_after.tiles.size(); ++i) {
        if (preview->collision_after.HasOverride(i) ==
                preview->collision_before.HasOverride(i) &&
            preview->collision_after.tiles[i] ==
                preview->collision_before.tiles[i])
          continue;
        const auto p =
            transform.RoomPixelsToScreen(ImVec2((i % 64) * 8, (i / 64) * 8));
        const auto q = transform.RoomPixelsToScreen(
            ImVec2((i % 64) * 8 + 8, (i / 64) * 8 + 8));
        rt.draw_list->AddRect(p, q, IM_COL32(90, 230, 170, 210));
        if (rt.scale >= 1.5f) {
          const auto text = std::to_string(preview->collision_after.tiles[i]);
          rt.draw_list->AddText(p, IM_COL32(255, 255, 255, 230), text.c_str());
        }
      }
      for (size_t i = preview->selection.before.sprites.size();
           i < preview->selection.after.sprites.size(); ++i) {
        const auto& actor = preview->selection.after.sprites[i];
        const auto p =
            transform.RoomPixelsToScreen(ImVec2(actor.x * 16, actor.y * 16));
        const auto q = transform.RoomPixelsToScreen(
            ImVec2(actor.x * 16 + 16, actor.y * 16 + 16));
        rt.draw_list->AddRect(p, q, IM_COL32(255, 210, 80, 255), 0, 2);
        const auto label =
            std::string(actor.id == 0xA3 ? "Cart " : "Channel ") +
            std::to_string(actor.subtype);
        rt.draw_list->AddText(p, IM_COL32(255, 220, 110, 255), label.c_str());
      }
      rt.draw_list->PopClipRect();
    }
    for (const auto& [x, y] : panel->path_waypoints()) {
      if (rt.draw_list)
        rt.draw_list->AddCircle(
            transform.RoomPixelsToScreen(ImVec2(x * 8 + 8, y * 8 + 8)), 4,
            IM_COL32(255, 210, 80, 255));
    }
    if (rt.hovered && !panel->assembly_error().empty())
      ImGui::SetTooltip("%s", panel->assembly_error().c_str());
    return true;
  }
  const auto* active = ImGui::GetDragDropPayload();
  bool node_drag = active && active->IsDataType(kMinecartNodeDragPayload) &&
                   active->DataSize == sizeof(MinecartToolPayload);
  const auto rail_profile =
      zelda3::CustomObjectManager::Get().GetMinecartRailProfile();
  const bool rail_drag =
      active && active->IsDataType(gui::kDragPayloadRoomObject) &&
      active->DataSize == sizeof(gui::RoomObjectDragPayload) &&
      zelda3::FindMinecartRailDescriptor(
          static_cast<const gui::RoomObjectDragPayload*>(active->Data)
              ->object_id,
          static_cast<const gui::RoomObjectDragPayload*>(active->Data)->size,
          rail_profile) != nullptr;
  if (!node_drag && !rail_drag && !panel->placement_brush()) {
    return false;
  }
  if (!node_drag && !rail_drag && panel->placement_brush()->collision_only) {
    return false;  // Existing node gestures use the collision tool's CAS drag.
  }
  if (room_canvas_shortcut_focus_ && ImGui::IsKeyPressed(ImGuiKey_Escape)) {
    panel->CancelNodePlacement();
    return true;
  }
  if (node_drag &&
      static_cast<const MinecartToolPayload*>(active->Data)->owner == panel &&
      (ImGui::IsKeyPressed(ImGuiKey_Escape) ||
       ImGui::IsMouseDown(ImGuiMouseButton_Right) ||
       ImGui::IsMouseDown(ImGuiMouseButton_Middle))) {
    panel->CancelNodePlacement();
    return true;
  }
  std::optional<MinecartToolPayload> brush = panel->placement_brush();
  const DungeonCanvasTransform transform(rt.canvas_p0, rt.scrolling, rt.scale);
  const auto origin = transform.room_origin_screen();
  ImRect target(origin, transform.RoomPixelsToScreen(ImVec2(512, 512)));
  target.ClipWith(
      ImRect(rt.canvas_p0,
             ImVec2(rt.canvas_p0.x + rt.canvas_sz.x * transform.scale(),
                    rt.canvas_p0.y + rt.canvas_sz.y * transform.scale())));
  if (rt.draw_list) {
    target.ClipWith(
        ImRect(rt.draw_list->GetClipRectMin(), rt.draw_list->GetClipRectMax()));
  }
  auto actor_at = [&](int x, int y) -> const zelda3::Sprite* {
    if (!brush || brush->collision_only ||
        (brush->node.kind != MinecartNodeKind::kSwitch &&
         brush->node.kind != MinecartNodeKind::kSwitchTrack)) {
      return nullptr;
    }
    const int wanted =
        brush->node.kind == MinecartNodeKind::kSwitch ? 0xAF : 0xB0;
    const zelda3::Sprite* found = nullptr;
    for (const auto& sprite : room.GetSprites()) {
      if (!sprite.deleted() && sprite.layer() == 0 && sprite.id() == wanted &&
          sprite.x() * 2 == x && sprite.y() * 2 == y) {
        if (found)
          return nullptr;  // Ambiguous source must not choose an actor.
        found = &sprite;
      }
    }
    return found;
  };
  if (!rail_drag && brush && !brush->collision_only &&
      ImGui::IsMouseDragging(ImGuiMouseButton_Left) &&
      !ImGui::IsMouseDown(ImGuiMouseButton_Right) &&
      !ImGui::IsMouseDown(ImGuiMouseButton_Middle)) {
    const auto click = ImGui::GetIO().MouseClickedPos[ImGuiMouseButton_Left];
    auto [source_x, source_y] = ImGui::IsMousePosValid(&click)
                                    ? transform.ScreenToRoomTiles(click, 8)
                                    : std::pair{-1, -1};
    source_x &= ~1;
    source_y &= ~1;
    if (source_x >= 0 && source_y >= 0 && source_x <= 62 && source_y <= 62) {
      if (const auto* sprite = actor_at(source_x, source_y);
          sprite && ImGui::BeginDragDropSource()) {
        auto moving = *brush;
        moving.node.id = sprite->subtype();
        moving.node.source_x = source_x;
        moving.node.source_y = source_y;
        moving.node.source_key_drop = static_cast<uint8_t>(sprite->key_drop());
        moving.node.expected_collision = room.custom_collision();
        if (sprite->id() == 0xB0) {
          moving.node.collision =
              room.custom_collision().tiles[source_y * 64 + source_x];
        }
        moving.owner = panel;
        moving.source_viewer = this;
        moving.revision = panel->canvas_revision();
        moving.room_id = room_id;
        ImGui::SetDragDropPayload(kMinecartNodeDragPayload, &moving,
                                  sizeof(moving), ImGuiCond_Once);
        ImGui::TextUnformatted(sprite->id() == 0xAF ? "Move switch"
                                                    : "Move switch track");
        ImGui::EndDragDropSource();
        // The canvas itself owns the source. Its first drag frame must also
        // participate in target preview instead of falling through to
        // placement.
        active = ImGui::GetDragDropPayload();
        node_drag = active && active->IsDataType(kMinecartNodeDragPayload) &&
                    active->DataSize == sizeof(MinecartToolPayload);
      }
    }
  }
  if (node_drag) {
    MinecartToolPayload incoming;
    std::memcpy(&incoming, active->Data, sizeof(incoming));
    brush = incoming;
  }
  auto [x, y] = ImGui::IsMousePosValid()
                    ? transform.ScreenToRoomTiles(ImGui::GetMousePos(), 8)
                    : std::pair{-1, -1};
  if (rail_drag || !brush || !brush->collision_only || brush->size == 2) {
    x &= ~1;
    y &= ~1;
  }
  // Ordinary item hover is suppressed while another item owns a drag source.
  const bool hovered =
      (node_drag || rail_drag)
          ? ImGui::IsWindowHovered(
                ImGuiHoveredFlags_AllowWhenBlockedByActiveItem) &&
                ImGui::IsMouseHoveringRect(target.Min, target.Max)
          : rt.hovered;
  const bool inside = hovered && x >= 0 && y >= 0 && x < 64 && y < 64;
  absl::Status validity =
      absl::InvalidArgumentError("Keep the node inside this room");
  std::optional<zelda3::RoomObject> rail;
  if (rail_drag) {
    const auto& payload =
        *static_cast<const gui::RoomObjectDragPayload*>(active->Data);
    rail.emplace(payload.object_id, x, y, payload.size, 0);
    auto objects = room.GetTileObjects();
    objects.push_back(*rail);
    validity = panel->automatic_rail_collision()
                   ? zelda3::SynchronizeMinecartTrackCollision(
                         room.GetTileObjects(), objects,
                         room.custom_collision(), {}, rail_profile)
                         .status()
                   : absl::OkStatus();
    if (x < 0 || y < 0 || x > 62 || y > 62 || payload.size > 15) {
      validity =
          absl::InvalidArgumentError("Place a complete rail inside this room");
    }
  } else if (brush && inside) {
    if (brush->owner != panel || brush->revision != panel->canvas_revision() ||
        brush->room_id != room_id) {
      validity = absl::FailedPreconditionError(
          "Track tool belongs to an earlier room or project; select it again");
    } else {
      validity = brush->collision_only
                     ? PreviewMinecartCollisionStamp(
                           room.custom_collision(),
                           {x, y, brush->size, brush->collision})
                           .status()
                     : PreviewMinecartNodePlacement(room, x, y, brush->node,
                                                    panel->GetTracks())
                           .status();
    }
  }
  bool delivered = false;
  if (node_drag || rail_drag) {
    if (ImGui::BeginDragDropTargetCustom(target,
                                         ImGui::GetID("##MinecartNodeDrop"))) {
      if (const auto* payload = ImGui::AcceptDragDropPayload(
              node_drag ? kMinecartNodeDragPayload
                        : gui::kDragPayloadRoomObject,
              ImGuiDragDropFlags_AcceptBeforeDelivery |
                  ImGuiDragDropFlags_AcceptNoDrawDefaultRect)) {
        delivered = payload->IsDelivery();
      }
      ImGui::EndDragDropTarget();
    }
  }
  const bool clicked = !node_drag && !rail_drag && inside && !actor_at(x, y) &&
                       ImGui::IsMouseClicked(ImGuiMouseButton_Left) &&
                       !ImGui::IsMouseDown(ImGuiMouseButton_Right) &&
                       !ImGui::IsMouseDown(ImGuiMouseButton_Middle);
  if (inside && rt.draw_list) {
    const auto& theme = AgentUI::GetTheme();
    const auto color = ImGui::GetColorU32(validity.ok() ? theme.status_success
                                                        : theme.status_error);
    const int pixels =
        rail_drag || !brush || !brush->collision_only ? 16 : brush->size * 8;
    const auto p = transform.RoomPixelsToScreen(ImVec2(x * 8, y * 8));
    const auto q =
        transform.RoomPixelsToScreen(ImVec2(x * 8 + pixels, y * 8 + pixels));
    rt.draw_list->PushClipRect(
        rt.canvas_p0,
        ImVec2(rt.canvas_p0.x + rt.canvas_sz.x * transform.scale(),
               rt.canvas_p0.y + rt.canvas_sz.y * transform.scale()),
        true);
    rt.draw_list->AddRectFilled(p, q, (color & 0x00FFFFFF) | 0x44000000);
    rt.draw_list->AddRect(p, q, color, 0.0f, 2.0f);
    rt.draw_list->PopClipRect();
    if (!validity.ok()) {
      ImGui::SetTooltip("%s", std::string(validity.message()).c_str());
    }
  }
  if ((delivered || clicked) && inside) {
    auto status = validity;
    if (status.ok() && rail) {
      auto& handler = object_interaction_.entity_coordinator().tile_handler();
      if (!handler.PlaceObjectAt(room_id, *rail, x, y)) {
        status = handler.mutation_status();
      }
    } else if (status.ok() && brush) {
      if (brush->collision_only && panel->collision_stamp_callback()) {
        status = panel->collision_stamp_callback()(
            room_id, room.custom_collision(),
            {x, y, brush->size, brush->collision});
      } else if (!brush->collision_only && panel->node_placement_callback()) {
        status = panel->node_placement_callback()(room_id, x, y, brush->node);
      } else {
        status =
            absl::FailedPreconditionError("Track placement is unavailable");
      }
    }
    panel->ReportToolStatus(status);
    if (status.ok() && brush && !brush->collision_only &&
        brush->node.kind == MinecartNodeKind::kCartStart) {
      panel->CancelNodePlacement();
    }
  }
  return true;
}

}  // namespace yaze::editor
