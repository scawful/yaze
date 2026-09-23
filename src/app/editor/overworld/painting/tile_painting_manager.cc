#include "app/editor/overworld/painting/tile_painting_manager.h"

#include <algorithm>
#include <cmath>
#include <vector>

#include "app/editor/overworld/tile16_editor.h"
#include "app/gfx/resource/arena.h"
#include "app/gui/canvas/canvas_usage_tracker.h"
#include "util/log.h"

namespace yaze::editor {
namespace {

constexpr int kTilesPerMap = kOverworldMapSize / kTile16Size;

int AllocatedRowsForWorld(int world) {
  const int world_start = std::clamp(world, 0, 2) * 0x40;
  return (std::clamp(zelda3::kNumOverworldMaps - world_start, 0, 0x40) + 7) / 8;
}

bool IsValidMapGridPosition(int world, int map_x, int map_y) {
  return world >= 0 && world <= 2 && map_x >= 0 && map_x < 8 && map_y >= 0 &&
         map_y < AllocatedRowsForWorld(world);
}

int MapIndexForGridPosition(int world, int map_x, int map_y) {
  return world * 0x40 + map_x + map_y * 8;
}

}  // namespace

TilePaintingManager::TilePaintingManager(const TilePaintingDependencies& deps,
                                         const TilePaintingCallbacks& callbacks)
    : deps_(deps), callbacks_(callbacks) {}

std::vector<std::vector<uint16_t>>& TilePaintingManager::WorldTiles() const {
  return deps_.overworld->GetMapTiles(*deps_.current_world);
}

TilePaintingManager::TilePosition TilePaintingManager::HoveredTile() const {
  const auto& canvas = *deps_.ow_map_canvas;
  const float scale = canvas.global_scale() > 0 ? canvas.global_scale() : 1.0f;
  const ImVec2 origin(canvas.zero_point().x + canvas.scrolling().x,
                      canvas.zero_point().y + canvas.scrolling().y);
  const ImVec2 mouse = ImGui::GetIO().MousePos;
  const float x = (mouse.x - origin.x) / scale / kTile16Size;
  const float y = (mouse.y - origin.y) / scale / kTile16Size;
  if (!ImGui::IsMousePosValid(&mouse) || !std::isfinite(x) ||
      !std::isfinite(y)) {
    return {-256, -256};  // Outside even the largest valid brush footprint.
  }
  // Bound before converting to int; focus loss or extreme scrolling can supply
  // coordinates outside the integer range. Neither bound reaches a valid tile.
  return {static_cast<int>(std::floor(std::clamp(x, -256.0f, 512.0f))),
          static_cast<int>(std::floor(std::clamp(y, -256.0f, 512.0f)))};
}

bool TilePaintingManager::IsValidTile(TilePosition position) const {
  if (position.x < 0 || position.y < 0 ||
      !IsValidMapGridPosition(*deps_.current_world, position.x / kTilesPerMap,
                              position.y / kTilesPerMap)) {
    return false;
  }
  const auto& tiles = WorldTiles();
  return position.x < static_cast<int>(tiles.size()) &&
         position.y < static_cast<int>(tiles[position.x].size());
}

const TileBrush* TilePaintingManager::selection_brush() const {
  return deps_.ow_map_canvas->select_rect_active() && brush_.valid() ? &brush_
                                                                     : nullptr;
}

void TilePaintingManager::CheckForOverworldEdits() {
  CheckForSelectRectangle();
  auto& canvas = *deps_.ow_map_canvas;
  if (single_paint_pending_ && !ImGui::IsMouseDown(ImGuiMouseButton_Left)) {
    callbacks_.finalize_paint_operation();
    single_paint_pending_ = false;
  }
  if (ImGui::IsMouseClicked(ImGuiMouseButton_Left)) {
    paint_gesture_owned_ = canvas.IsMouseHovering() && ImGui::IsItemActive();
  } else if (!ImGui::IsMouseDown(ImGuiMouseButton_Left)) {
    paint_gesture_owned_ = false;
  }
  const bool can_paint =
      paint_gesture_owned_ && canvas.IsMouseHovering() && ImGui::IsItemActive();
  const auto anchor = HoveredTile();
  const auto* selected = selection_brush();
  const TileBrush single{1, 1, {*deps_.current_tile16}};

  if (*deps_.current_mode == EditingMode::FILL_TILE) {
    if (can_paint && ImGui::IsMouseClicked(ImGuiMouseButton_Left) &&
        IsValidTile(anchor)) {
      const TilePosition screen{anchor.x / kTilesPerMap * kTilesPerMap,
                                anchor.y / kTilesPerMap * kTilesPerMap};
      PaintPattern(selected ? *selected : single, screen, kTilesPerMap,
                   kTilesPerMap);
    }
    return;
  }
  if (*deps_.current_mode != EditingMode::DRAW_TILE) {
    return;
  }
  if (selected) {
    if (can_paint && (ImGui::IsMouseClicked(ImGuiMouseButton_Left) ||
                      ImGui::IsMouseDragging(ImGuiMouseButton_Left))) {
      PaintPattern(*selected, anchor, selected->width, selected->height);
    }
  } else if (single.valid() &&
             canvas.DrawTilemapPainter(*deps_.tile16_blockset,
                                       *deps_.current_tile16) &&
             can_paint) {
    PaintPattern(single, anchor, 1, 1, false);
  }
}

absl::Status TilePaintingManager::PasteBrush(const TileBrush& brush) {
  if (!brush.valid()) {
    return absl::InvalidArgumentError("Clipboard tile pattern is invalid");
  }
  if (!deps_.ow_map_canvas->IsMouseHovering()) {
    return absl::FailedPreconditionError("Hover the overworld canvas to paste");
  }
  PaintPattern(brush, HoveredTile(), brush.width, brush.height);
  return absl::OkStatus();
}

void TilePaintingManager::PaintPattern(const TileBrush& brush,
                                       TilePosition anchor, int width,
                                       int height, bool finalize) {
  if (!brush.valid()) {
    return;
  }
  ChangedMaps changed_maps{};
  for (int y = 0; y < height; ++y) {
    for (int x = 0; x < width; ++x) {
      PaintTile({anchor.x + x, anchor.y + y},
                brush.at(x % brush.width, y % brush.height), changed_maps);
    }
  }
  if (std::none_of(changed_maps.begin(), changed_maps.end(),
                   [](bool changed) { return changed; })) {
    return;
  }
  deps_.rom->set_dirty(true);
  if (finalize) {
    callbacks_.finalize_paint_operation();
    single_paint_pending_ = false;
  } else {
    single_paint_pending_ = true;
  }
  if (IsValidTile(anchor)) {
    *deps_.current_map = MapIndexForGridPosition(
        *deps_.current_world, anchor.x / kTilesPerMap, anchor.y / kTilesPerMap);
  }
  for (int map = 0; map < zelda3::kNumOverworldMaps; ++map) {
    if (changed_maps[map]) {
      if ((*deps_.maps_bmp)[map].is_active()) {
        gfx::Arena::Get().QueueTextureCommand(
            gfx::Arena::TextureCommandType::UPDATE, &(*deps_.maps_bmp)[map]);
      }
      callbacks_.refresh_overworld_map_on_demand(map);
    }
  }
}

bool TilePaintingManager::PaintTile(TilePosition position, int tile_id,
                                    ChangedMaps& changed_maps) {
  if (!IsValidTile(position)) {
    return false;
  }
  auto& old_id = WorldTiles()[position.x][position.y];
  if (old_id == tile_id) {
    return false;
  }
  const int map =
      MapIndexForGridPosition(*deps_.current_world, position.x / kTilesPerMap,
                              position.y / kTilesPerMap);
  callbacks_.create_undo_point(map, *deps_.current_world, position.x,
                               position.y, old_id);
  old_id = tile_id;
  changed_maps[map] = true;
  (*deps_.maps_bmp)[map].set_modified(true);
  if (deps_.tile16_blockset->atlas.is_active()) {
    RenderMapTile(map, position,
                  gfx::GetTilemapData(*deps_.tile16_blockset, tile_id));
  }
  return true;
}

void TilePaintingManager::RenderMapTile(int map_id, TilePosition position,
                                        const std::vector<uint8_t>& tile_data) {
  if (map_id < 0 || map_id >= static_cast<int>(deps_.maps_bmp->size())) {
    return;
  }
  auto& bitmap = (*deps_.maps_bmp)[map_id];
  if (!bitmap.is_active() || tile_data.size() < kTile16Size * kTile16Size) {
    return;
  }
  const int pixel_x = position.x % kTilesPerMap * kTile16Size;
  const int pixel_y = position.y % kTilesPerMap * kTile16Size;
  for (int y = 0; y < kTile16Size; ++y) {
    for (int x = 0; x < kTile16Size; ++x) {
      const int offset = (pixel_y + y) * kOverworldMapSize + pixel_x + x;
      if (offset >= 0 && offset < static_cast<int>(bitmap.size())) {
        bitmap.WriteToPixel(offset, tile_data[y * kTile16Size + x]);
      }
    }
  }
  bitmap.set_modified(true);
}

void TilePaintingManager::RenderUpdatedMapBitmap(
    const ImVec2& position, const std::vector<uint8_t>& tile_data) {
  RenderMapTile(*deps_.current_map,
                {static_cast<int>(std::floor(position.x / kTile16Size)),
                 static_cast<int>(std::floor(position.y / kTile16Size))},
                tile_data);
  if (*deps_.current_map >= 0 &&
      *deps_.current_map < zelda3::kNumOverworldMaps &&
      (*deps_.maps_bmp)[*deps_.current_map].is_active()) {
    gfx::Arena::Get().QueueTextureCommand(
        gfx::Arena::TextureCommandType::UPDATE,
        &(*deps_.maps_bmp)[*deps_.current_map]);
  }
}

void TilePaintingManager::CaptureSelection() {
  auto& canvas = *deps_.ow_map_canvas;
  const auto points = canvas.selected_points();
  if (points.size() != 2) {
    return;
  }
  const int left = std::max(
      0, static_cast<int>(
             std::floor(std::min(points[0].x, points[1].x) / kTile16Size)));
  const int top = std::max(
      0, static_cast<int>(
             std::floor(std::min(points[0].y, points[1].y) / kTile16Size)));
  const int right =
      std::min(8 * kTilesPerMap - 1,
               static_cast<int>(std::floor(std::max(points[0].x, points[1].x) /
                                           kTile16Size)));
  const int bottom =
      std::min(AllocatedRowsForWorld(*deps_.current_world) * kTilesPerMap - 1,
               static_cast<int>(std::floor(std::max(points[0].y, points[1].y) /
                                           kTile16Size)));
  TileBrush captured{right - left + 1, bottom - top + 1, {}};
  if (captured.width <= 0 || captured.height <= 0) {
    canvas.ClearSelection();
    return;
  }
  for (int y = top; y <= bottom; ++y) {
    for (int x = left; x <= right; ++x) {
      if (!IsValidTile({x, y})) {
        canvas.ClearSelection();
        return;
      }
      captured.tile_ids.push_back(WorldTiles()[x][y]);
    }
  }
  brush_ = std::move(captured);
  *deps_.selected_tile16_ids = brush_.tile_ids;
}

void TilePaintingManager::CheckForSelectRectangle() {
  auto& canvas = *deps_.ow_map_canvas;
  const bool was_active = canvas.select_rect_active();
  if (ImGui::IsMouseClicked(ImGuiMouseButton_Right)) {
    selection_gesture_owned_ =
        canvas.IsMouseHovering() && ImGui::IsItemActive();
  }
  const float scale = canvas.global_scale() > 0 ? canvas.global_scale() : 1.0f;
  canvas.DrawSelectRect(*deps_.current_map, kTile16Size, scale);
  if (canvas.selected_tile_pos().x != -1) {
    const auto source = HoveredTile();
    if (selection_gesture_owned_ && IsValidTile(source)) {
      *deps_.current_tile16 = WorldTiles()[source.x][source.y];
      callbacks_.scroll_blockset_to_current_tile();
    }
    canvas.set_selected_tile_pos(ImVec2(-1, -1));
  }
  if (!was_active && canvas.select_rect_active()) {
    if (selection_gesture_owned_ && canvas.IsMouseHovering()) {
      CaptureSelection();
    } else {
      canvas.ClearSelection();
    }
  }
  if (!ImGui::IsMouseDown(ImGuiMouseButton_Right)) {
    selection_gesture_owned_ = false;
  }
  if (!canvas.select_rect_active()) {
    brush_ = {};
    deps_.selected_tile16_ids->clear();
  }
  if (selection_brush() && canvas.IsMouseHovering()) {
    DrawBrushPreview(HoveredTile());
  }
}

void TilePaintingManager::DrawBrushPreview(TilePosition anchor) {
  auto& canvas = *deps_.ow_map_canvas;
  auto* points = canvas.mutable_selected_points();
  points->clear();
  points->push_back(ImVec2(anchor.x * kTile16Size, anchor.y * kTile16Size));
  points->push_back(ImVec2((anchor.x + brush_.width - 1) * kTile16Size,
                           (anchor.y + brush_.height - 1) * kTile16Size));

  const auto& atlas = deps_.tile16_blockset->atlas;
  if (!atlas.is_active() || !atlas.texture() || atlas.width() < kTile16Size ||
      atlas.height() < kTile16Size) {
    return;
  }
  const int columns = atlas.width() / kTile16Size;
  const int tile_count = columns * (atlas.height() / kTile16Size);
  const float scale = canvas.global_scale() > 0 ? canvas.global_scale() : 1.0f;
  const float size = kTile16Size * scale;
  const ImVec2 origin(canvas.zero_point().x + canvas.scrolling().x,
                      canvas.zero_point().y + canvas.scrolling().y);
  auto* draw = canvas.draw_list();
  draw->PushClipRect(
      canvas.zero_point(),
      ImVec2(canvas.zero_point().x + canvas.canvas_size().x * scale,
             canvas.zero_point().y + canvas.canvas_size().y * scale),
      true);
  for (int y = 0; y < brush_.height; ++y) {
    for (int x = 0; x < brush_.width; ++x) {
      const int id = brush_.at(x, y);
      if (!IsValidTile({anchor.x + x, anchor.y + y}) || id >= tile_count) {
        continue;
      }
      const ImVec2 start(origin.x + (anchor.x + x) * size,
                         origin.y + (anchor.y + y) * size);
      const ImVec2 uv0(
          static_cast<float>(id % columns * kTile16Size) / atlas.width(),
          static_cast<float>(id / columns * kTile16Size) / atlas.height());
      const ImVec2 uv1(
          uv0.x + static_cast<float>(kTile16Size) / atlas.width(),
          uv0.y + static_cast<float>(kTile16Size) / atlas.height());
      draw->AddImage((ImTextureID)(intptr_t)atlas.texture(), start,
                     ImVec2(start.x + size, start.y + size), uv0, uv1,
                     IM_COL32(255, 255, 255, 180));
    }
  }
  draw->PopClipRect();
}

// ---------------------------------------------------------------------------
// PickTile16FromHoveredCanvas - eyedropper tool
// ---------------------------------------------------------------------------
bool TilePaintingManager::PickTile16FromHoveredCanvas() {
  if (!deps_.ow_map_canvas->IsMouseHovering()) {
    return false;
  }

  const ImVec2 scaled_position = deps_.ow_map_canvas->hover_mouse_pos();
  float scale = deps_.ow_map_canvas->global_scale();
  if (scale <= 0.0f) {
    scale = 1.0f;
  }
  if (scaled_position.x < 0.0f || scaled_position.y < 0.0f) {
    return false;
  }

  const int map_x =
      static_cast<int>(scaled_position.x / scale) / kOverworldMapSize;
  const int map_y =
      static_cast<int>(scaled_position.y / scale) / kOverworldMapSize;
  if (!IsValidMapGridPosition(*deps_.current_world, map_x, map_y)) {
    return false;
  }

  const int local_tile_x =
      (static_cast<int>(scaled_position.x / scale) % kOverworldMapSize) /
      kTile16Size;
  const int local_tile_y =
      (static_cast<int>(scaled_position.y / scale) % kOverworldMapSize) /
      kTile16Size;
  if (local_tile_x < 0 || local_tile_x >= 32 || local_tile_y < 0 ||
      local_tile_y >= 32) {
    return false;
  }

  const int world_tile_x = map_x * 32 + local_tile_x;
  const int world_tile_y = map_y * 32 + local_tile_y;
  if (world_tile_x < 0 || world_tile_x >= 256 || world_tile_y < 0 ||
      world_tile_y >= 256) {
    return false;
  }

  const auto* map_tiles = deps_.overworld->mutable_map_tiles();
  if (!map_tiles) {
    return false;
  }
  const auto& world_tiles = (*deps_.current_world == 0) ? map_tiles->light_world
                            : (*deps_.current_world == 1)
                                ? map_tiles->dark_world
                                : map_tiles->special_world;
  if (world_tile_x >= static_cast<int>(world_tiles.size()) ||
      world_tile_y >= static_cast<int>(world_tiles[world_tile_x].size())) {
    return false;
  }
  const int tile_id = world_tiles[world_tile_x][world_tile_y];
  if (tile_id < 0) {
    return false;
  }

  if (callbacks_.request_tile16_selection) {
    callbacks_.request_tile16_selection(tile_id);
  } else {
    *deps_.current_tile16 = tile_id;
    auto set_tile_status =
        deps_.tile16_editor->SetCurrentTile(*deps_.current_tile16);
    if (!set_tile_status.ok()) {
      util::logf("Failed to sync Tile16 editor after eyedropper: %s",
                 set_tile_status.message().data());
    }
  }

  callbacks_.scroll_blockset_to_current_tile();
  return true;
}

// ---------------------------------------------------------------------------
// ToggleBrushTool - switch between DRAW_TILE and MOUSE modes
// ---------------------------------------------------------------------------
void TilePaintingManager::ToggleBrushTool() {
  if (*deps_.current_mode == EditingMode::DRAW_TILE) {
    *deps_.current_mode = EditingMode::MOUSE;
  } else {
    *deps_.current_mode = EditingMode::DRAW_TILE;
  }

  if (*deps_.current_mode == EditingMode::MOUSE) {
    deps_.ow_map_canvas->SetUsageMode(gui::CanvasUsage::kEntityManipulation);
  } else {
    deps_.ow_map_canvas->SetUsageMode(gui::CanvasUsage::kTilePainting);
  }
}

// ---------------------------------------------------------------------------
// ActivateFillTool - toggle FILL_TILE mode on/off
// ---------------------------------------------------------------------------
void TilePaintingManager::ActivateFillTool() {
  if (*deps_.current_mode == EditingMode::FILL_TILE) {
    *deps_.current_mode = EditingMode::DRAW_TILE;
  } else {
    *deps_.current_mode = EditingMode::FILL_TILE;
  }

  // Fill tool is still a tile painting interaction.
  deps_.ow_map_canvas->SetUsageMode(gui::CanvasUsage::kTilePainting);
}

}  // namespace yaze::editor
