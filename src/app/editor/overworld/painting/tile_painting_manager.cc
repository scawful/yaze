#include "app/editor/overworld/painting/tile_painting_manager.h"

#include <algorithm>
#include <cmath>
#include <string>
#include <vector>

#include "absl/strings/str_format.h"
#include "app/editor/overworld/painting/tile_brush_preview.h"
#include "app/editor/overworld/tile16_editor.h"
#include "app/gfx/resource/arena.h"
#include "app/gui/canvas/canvas_usage_tracker.h"
#include "util/log.h"

namespace yaze::editor {
namespace {

constexpr int kTilesPerMap = kOverworldMapSize / kTile16Size;

int LegacyAllocatedRowsForWorld(int world) {
  const int world_start = std::clamp(world, 0, 2) * 0x40;
  return (std::clamp(zelda3::kNumOverworldMaps - world_start, 0, 0x40) + 7) / 8;
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

int TilePaintingManager::AllocatedRowsForWorld(int world) const {
  return callbacks_.allocated_rows
             ? std::clamp(callbacks_.allocated_rows(world), 0, 8)
             : LegacyAllocatedRowsForWorld(world);
}

bool TilePaintingManager::IsValidMapGridPosition(int world, int map_x,
                                                 int map_y) const {
  return world >= 0 && world <= 2 && map_x >= 0 && map_x < 8 && map_y >= 0 &&
         map_y < AllocatedRowsForWorld(world);
}

bool TilePaintingManager::IsProjectTile(TilePosition position) const {
  return callbacks_.is_project_tile &&
         callbacks_.is_project_tile(*deps_.current_world, position.x,
                                    position.y);
}

absl::StatusOr<uint16_t> TilePaintingManager::ReadTile(
    TilePosition position) const {
  if (position.x < 0 || position.y < 0 ||
      !IsValidMapGridPosition(*deps_.current_world, position.x / kTilesPerMap,
                              position.y / kTilesPerMap)) {
    return absl::OutOfRangeError("Tile is outside the displayed world");
  }
  if (IsProjectTile(position)) {
    if (!callbacks_.read_project_tile)
      return absl::FailedPreconditionError("Project tile reader unavailable");
    return callbacks_.read_project_tile(*deps_.current_world, position.x,
                                        position.y);
  }
  if (MapIndexForGridPosition(*deps_.current_world, position.x / kTilesPerMap,
                              position.y / kTilesPerMap) >=
      zelda3::kNumOverworldMaps) {
    return absl::OutOfRangeError("Tile is outside the ROM map model");
  }
  const auto& tiles = WorldTiles();
  if (position.x >= static_cast<int>(tiles.size()) ||
      position.y >= static_cast<int>(tiles[position.x].size())) {
    return absl::OutOfRangeError("Tile is outside the loaded world");
  }
  return tiles[position.x][position.y];
}

bool TilePaintingManager::IsValidTile(TilePosition position) const {
  return ReadTile(position).ok();
}

const TileBrush* TilePaintingManager::selection_brush() const {
  if (external_brush_active_ && brush_.valid()) {
    return &brush_;
  }
  return deps_.ow_map_canvas->select_rect_active() && brush_.valid() ? &brush_
                                                                     : nullptr;
}

void TilePaintingManager::SetSelectionBrush(TileBrush brush) {
  brush_ = std::move(brush);
  if (deps_.selected_tile16_ids) {
    *deps_.selected_tile16_ids = brush_.tile_ids;
  }
  external_brush_active_ = brush_.valid();
}

void TilePaintingManager::ClearSelectionBrush() {
  brush_ = {};
  if (deps_.selected_tile16_ids) {
    deps_.selected_tile16_ids->clear();
  }
  external_brush_active_ = false;
  if (deps_.ow_map_canvas) {
    deps_.ow_map_canvas->ClearSelection();
  }
}

void TilePaintingManager::FinalizePendingPaint() {
  if (single_paint_pending_ && callbacks_.finalize_paint_operation) {
    callbacks_.finalize_paint_operation();
  }
  single_paint_pending_ = false;
  if (project_paint_pending_ && callbacks_.finalize_project_paint) {
    callbacks_.finalize_project_paint();
  }
  project_paint_pending_ = false;
}

void TilePaintingManager::SuspendCanvasGestures() {
  FinalizePendingPaint();
  paint_gesture_owned_ = false;
  selection_gesture_owned_ = false;
}

void TilePaintingManager::CheckForOverworldEdits() {
  CheckForSelectRectangle();
  auto& canvas = *deps_.ow_map_canvas;
  const auto anchor = HoveredTile();
  if (!ImGui::IsMouseDown(ImGuiMouseButton_Left)) {
    FinalizePendingPaint();
  }
  if (ImGui::IsMouseClicked(ImGuiMouseButton_Left)) {
    paint_gesture_owned_ = canvas.IsMouseHovering() && ImGui::IsItemActive();
    gesture_project_ = IsProjectTile(anchor);
    gesture_world_ = *deps_.current_world;
    gesture_mode_ = *deps_.current_mode;
  } else if (!ImGui::IsMouseDown(ImGuiMouseButton_Left)) {
    paint_gesture_owned_ = false;
  }
  if (paint_gesture_owned_ &&
      (gesture_world_ != *deps_.current_world ||
       gesture_mode_ != *deps_.current_mode ||
       (gesture_project_ &&
        (!canvas.IsMouseHovering() || !IsValidTile(anchor))) ||
       (canvas.IsMouseHovering() && IsValidTile(anchor) &&
        gesture_project_ != IsProjectTile(anchor)))) {
    // A held gesture cannot acquire another storage backend or interpolate a
    // project stroke across an unobserved gap outside its canvas/document.
    FinalizePendingPaint();
    paint_gesture_owned_ = false;
  }
  const bool can_paint =
      paint_gesture_owned_ && canvas.IsMouseHovering() && ImGui::IsItemActive();
  const auto* selected = selection_brush();
  const TileBrush single{1, 1, {*deps_.current_tile16}};

  if (*deps_.current_mode == EditingMode::FILL_TILE) {
    if (can_paint && ImGui::IsMouseClicked(ImGuiMouseButton_Left) &&
        IsValidTile(anchor)) {
      const TilePosition screen{anchor.x / kTilesPerMap * kTilesPerMap,
                                anchor.y / kTilesPerMap * kTilesPerMap};
      (void)PaintPattern(selected ? *selected : single, screen, kTilesPerMap,
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
      (void)PaintPattern(*selected, anchor, selected->width, selected->height,
                         false);
    }
  } else if (single.valid()) {
    // The hovered tile shows the selected Tile16 as the map under the cursor
    // draws it (its own graphics and palette), not as the current map does.
    auto* points = canvas.mutable_points();
    points->clear();
    if (canvas.IsMouseHovering() && IsValidTile(anchor)) {
      const float scale =
          canvas.global_scale() > 0 ? canvas.global_scale() : 1.0f;
      const float size = kTile16Size * scale;
      points->push_back(ImVec2(anchor.x * size, anchor.y * size));
      points->push_back(ImVec2((anchor.x + 1) * size, (anchor.y + 1) * size));
      DrawBrushPreview(single, anchor, /*mark_selection=*/false, 255);
    }
    if (can_paint && (ImGui::IsMouseClicked(ImGuiMouseButton_Left) ||
                      ImGui::IsMouseDragging(ImGuiMouseButton_Left))) {
      (void)PaintPattern(single, anchor, 1, 1, false);
    }
  }
}

absl::Status TilePaintingManager::PasteBrush(const TileBrush& brush) {
  if (!brush.valid()) {
    return absl::InvalidArgumentError("Clipboard tile pattern is invalid");
  }
  if (!deps_.ow_map_canvas->IsMouseHovering()) {
    return absl::FailedPreconditionError("Hover the overworld canvas to paste");
  }
  FinalizePendingPaint();
  return PaintPattern(brush, HoveredTile(), brush.width, brush.height);
}

absl::Status TilePaintingManager::PaintPattern(const TileBrush& brush,
                                               TilePosition anchor, int width,
                                               int height, bool finalize) {
  if (!brush.valid()) {
    return absl::InvalidArgumentError("Tile pattern is invalid");
  }
  if (IsProjectTile(anchor)) {
    if (!callbacks_.paint_project_pattern)
      return absl::FailedPreconditionError("Project tile writer unavailable");
    auto status = callbacks_.paint_project_pattern(brush, anchor.x, anchor.y,
                                                   width, height, finalize);
    if (!status.ok()) {
      FinalizePendingPaint();
      paint_gesture_owned_ = false;
      return status;
    }
    project_paint_pending_ = !finalize;
    if (IsValidTile(anchor) && callbacks_.select_map_for_editing) {
      callbacks_.select_map_for_editing(
          MapIndexForGridPosition(*deps_.current_world, anchor.x / kTilesPerMap,
                                  anchor.y / kTilesPerMap));
    }
    return absl::OkStatus();
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
    return absl::OkStatus();
  }
  deps_.rom->set_dirty(true);
  if (finalize) {
    callbacks_.finalize_paint_operation();
    single_paint_pending_ = false;
  } else {
    single_paint_pending_ = true;
  }
  if (IsValidTile(anchor)) {
    const int anchor_map = MapIndexForGridPosition(
        *deps_.current_world, anchor.x / kTilesPerMap, anchor.y / kTilesPerMap);
    // A direct write changes the selected map without refreshing its Tile16
    // palette and ignores the pin. The editor callback does both.
    if (callbacks_.select_map_for_editing) {
      callbacks_.select_map_for_editing(anchor_map);
    } else {
      *deps_.current_map = anchor_map;
    }
  }
  RefreshChangedMaps(changed_maps);
  return absl::OkStatus();
}

absl::StatusOr<int> TilePaintingManager::ApplyGuardedCells(
    int map_id, const std::vector<GuardedTileCell>& cells) {
  if (map_id < 0 || map_id >= zelda3::kNumOverworldMaps) {
    return absl::OutOfRangeError("Stencil map is outside the ROM map model");
  }
  if (map_id / 0x40 != *deps_.current_world) {
    return absl::FailedPreconditionError(
        "Stencil map is not in the displayed world");
  }
  if (cells.empty()) {
    return absl::InvalidArgumentError("Stencil has no cells");
  }
  const int origin_x = (map_id % 0x40) % 8 * kTilesPerMap;
  const int origin_y = (map_id % 0x40) / 8 * kTilesPerMap;
  std::vector<bool> seen(kTilesPerMap * kTilesPerMap, false);
  int mismatches = 0;
  std::string first_mismatch;
  for (const auto& cell : cells) {
    if (cell.x < 0 || cell.x >= kTilesPerMap || cell.y < 0 ||
        cell.y >= kTilesPerMap) {
      return absl::InvalidArgumentError(absl::StrFormat(
          "Stencil cell (%d,%d) is outside the map", cell.x, cell.y));
    }
    if (cell.proposed >= zelda3::kNumTile16Individual) {
      return absl::InvalidArgumentError(
          absl::StrFormat("Stencil cell (%d,%d) proposes invalid Tile16 0x%X",
                          cell.x, cell.y, cell.proposed));
    }
    const int index = cell.y * kTilesPerMap + cell.x;
    if (seen[index]) {
      return absl::InvalidArgumentError(
          absl::StrFormat("Stencil repeats cell (%d,%d)", cell.x, cell.y));
    }
    seen[index] = true;
    const TilePosition position{origin_x + cell.x, origin_y + cell.y};
    if (IsProjectTile(position)) {
      return absl::FailedPreconditionError(
          "Stencil cells must be ROM map tiles, not project terrain");
    }
    const auto current = ReadTile(position);
    if (!current.ok()) {
      return current.status();
    }
    if (*current != cell.expected && mismatches++ == 0) {
      first_mismatch = absl::StrFormat("(%d,%d) expected 0x%03X, found 0x%03X",
                                       cell.x, cell.y, cell.expected, *current);
    }
  }
  if (mismatches > 0) {
    return absl::FailedPreconditionError(
        absl::StrFormat("Stencil guard failed at %d cell(s); first %s",
                        mismatches, first_mismatch));
  }

  FinalizePendingPaint();
  ChangedMaps changed_maps{};
  int changed = 0;
  for (const auto& cell : cells) {
    if (PaintTile({origin_x + cell.x, origin_y + cell.y}, cell.proposed,
                  changed_maps)) {
      ++changed;
    }
  }
  if (changed == 0) {
    return 0;
  }
  deps_.rom->set_dirty(true);
  callbacks_.finalize_paint_operation();
  RefreshChangedMaps(changed_maps);
  return changed;
}

void TilePaintingManager::RefreshChangedMaps(const ChangedMaps& changed_maps) {
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
  if (IsProjectTile(position) || !IsValidTile(position)) {
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
  // Draw with the destination map's graphics: a stroke can cross into an
  // area whose sheets differ from the current map's blockset.
  const auto pixels = Tile16PixelsForMap(map, tile_id);
  if (!pixels.empty()) {
    RenderMapTile(map, position, pixels);
  }
  return true;
}

std::vector<uint8_t> TilePaintingManager::Tile16PixelsForMap(
    int map_id, int tile_id) const {
  const auto* map =
      deps_.overworld ? deps_.overworld->overworld_map(map_id) : nullptr;
  if (map && map->is_built() && !map->current_tile16_blockset().empty()) {
    auto pixels =
        Tile16PixelsFromBlockset(map->current_tile16_blockset(), tile_id);
    if (!pixels.empty()) {
      return pixels;
    }
  }
  if (deps_.tile16_blockset && deps_.tile16_blockset->atlas.is_active()) {
    return gfx::GetTilemapData(*deps_.tile16_blockset, tile_id);
  }
  return {};
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
      const auto tile = ReadTile({x, y});
      if (!tile.ok()) {
        canvas.ClearSelection();
        return;
      }
      captured.tile_ids.push_back(*tile);
    }
  }
  brush_ = std::move(captured);
  *deps_.selected_tile16_ids = brush_.tile_ids;
}

void TilePaintingManager::CheckForSelectRectangle() {
  auto& canvas = *deps_.ow_map_canvas;
  const bool was_active = canvas.select_rect_active();
  if (ImGui::IsMouseClicked(ImGuiMouseButton_Right)) {
    // Shift+right-click opens the map context menu instead of sampling.
    selection_gesture_owned_ = canvas.IsMouseHovering() &&
                               ImGui::IsItemActive() &&
                               !ImGui::GetIO().KeyShift;
  }
  const float scale = canvas.global_scale() > 0 ? canvas.global_scale() : 1.0f;
  canvas.DrawSelectRect(*deps_.current_map, kTile16Size, scale);
  if (canvas.selected_tile_pos().x != -1) {
    const auto source = HoveredTile();
    const auto tile = ReadTile(source);
    if (selection_gesture_owned_ && tile.ok()) {
      // Same path as the I key: keeps staged Tile16 edits guarded.
      const int tile_id = *tile;
      if (callbacks_.request_tile16_selection) {
        callbacks_.request_tile16_selection(tile_id);
      } else {
        *deps_.current_tile16 = tile_id;
      }
      callbacks_.scroll_blockset_to_current_tile();
    }
    canvas.set_selected_tile_pos(ImVec2(-1, -1));
  }
  if (!was_active && canvas.select_rect_active()) {
    external_brush_active_ = false;
    if (selection_gesture_owned_ && canvas.IsMouseHovering()) {
      CaptureSelection();
    } else {
      canvas.ClearSelection();
    }
  }
  if (!ImGui::IsMouseDown(ImGuiMouseButton_Right)) {
    selection_gesture_owned_ = false;
  }
  if (!canvas.select_rect_active() && !external_brush_active_) {
    brush_ = {};
    deps_.selected_tile16_ids->clear();
  }
  if (selection_brush() && canvas.IsMouseHovering()) {
    DrawBrushPreview(brush_, HoveredTile(), /*mark_selection=*/true, 180);
  }
}

void TilePaintingManager::DrawBrushPreview(const TileBrush& brush,
                                           TilePosition anchor,
                                           bool mark_selection, int alpha) {
  auto& canvas = *deps_.ow_map_canvas;
  if (mark_selection) {
    auto* points = canvas.mutable_selected_points();
    points->clear();
    points->push_back(ImVec2(anchor.x * kTile16Size, anchor.y * kTile16Size));
    points->push_back(ImVec2((anchor.x + brush.width - 1) * kTile16Size,
                             (anchor.y + brush.height - 1) * kTile16Size));
  }

  // Build each destination piece once. The selected/pinned map's atlas is
  // only a fallback for synthetic or unavailable map data.
  std::array<MapBrushPreview, zelda3::kNumOverworldMaps> pieces;
  std::array<bool, zelda3::kNumOverworldMaps> has_piece{};
  const int world = *deps_.current_world;
  const int first_x = std::max(0, anchor.x) / kTilesPerMap;
  const int first_y = std::max(0, anchor.y) / kTilesPerMap;
  const int last_x = std::min(255, anchor.x + brush.width - 1) / kTilesPerMap;
  const int last_y = std::min(255, anchor.y + brush.height - 1) / kTilesPerMap;
  for (int my = first_y; my <= last_y; ++my) {
    for (int mx = first_x; mx <= last_x; ++mx) {
      if (!IsValidMapGridPosition(world, mx, my))
        continue;
      const int map_id = MapIndexForGridPosition(world, mx, my);
      if (IsProjectTile({mx * kTilesPerMap, my * kTilesPerMap}) ||
          map_id >= zelda3::kNumOverworldMaps)
        continue;
      const auto* map = deps_.overworld->overworld_map(map_id);
      if (!map)
        continue;
      if (!map->is_built() && !deps_.overworld->EnsureMapBuilt(map_id).ok())
        continue;
      const auto& source = map->current_tile16_blockset();
      if (source.empty())
        continue;
      auto& piece = pieces[map_id];
      piece = BuildMapBrushPreview(brush, anchor.x, anchor.y, mx, my, source);
      if (piece.pixels.empty())
        continue;
      auto& preview = map_brush_previews_[map_id];
      SyncMapBrushPreview(piece, map->current_palette(), preview);
      has_piece[map_id] = true;
    }
  }
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
  for (int y = 0; y < brush.height; ++y) {
    for (int x = 0; x < brush.width; ++x) {
      const int id = brush.at(x, y);
      const TilePosition position{anchor.x + x, anchor.y + y};
      if (!IsValidTile(position))
        continue;
      const int map_id = MapIndexForGridPosition(
          world, position.x / kTilesPerMap, position.y / kTilesPerMap);
      const bool project_tile = IsProjectTile(position);
      const bool use_piece = !project_tile && has_piece[map_id];
      const gfx::Bitmap* source_atlas =
          project_tile ? (callbacks_.project_tile_atlas
                              ? callbacks_.project_tile_atlas(map_id)
                              : nullptr)
                       : (use_piece ? &map_brush_previews_[map_id]
                                    : &deps_.tile16_blockset->atlas);
      if (!source_atlas)
        continue;
      const auto& atlas = *source_atlas;
      if (!atlas.is_active() || !atlas.texture() || atlas.width() < 16 ||
          atlas.height() < 16)
        continue;
      const int columns = atlas.width() / 16;
      const int source_x = use_piece ? (position.x - pieces[map_id].tile_x) * 16
                                     : id % columns * 16;
      const int source_y = use_piece ? (position.y - pieces[map_id].tile_y) * 16
                                     : id / columns * 16;
      if (source_y + 16 > atlas.height())
        continue;
      const ImVec2 start(origin.x + position.x * size,
                         origin.y + position.y * size);
      const ImVec2 uv0(static_cast<float>(source_x) / atlas.width(),
                       static_cast<float>(source_y) / atlas.height());
      const ImVec2 uv1(uv0.x + 16.0f / atlas.width(),
                       uv0.y + 16.0f / atlas.height());
      draw->AddImage((ImTextureID)(intptr_t)atlas.texture(), start,
                     ImVec2(start.x + size, start.y + size), uv0, uv1,
                     ImGui::GetColorU32(IM_COL32(255, 255, 255, alpha)));
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

  const auto tile = ReadTile(HoveredTile());
  if (!tile.ok()) {
    return false;
  }
  const int tile_id = *tile;

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
