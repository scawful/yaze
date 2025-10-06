#include "app/core/service/canvas_automation_service.h"

#ifdef YAZE_WITH_GRPC

#include "src/protos/canvas_automation.pb.h"
#include "app/editor/overworld/overworld_editor.h"
#include "app/gui/canvas/canvas_automation_api.h"

namespace yaze {

void CanvasAutomationServiceImpl::RegisterCanvas(const std::string& canvas_id,
                                                 gui::Canvas* canvas) {
  canvases_[canvas_id] = canvas;
}

void CanvasAutomationServiceImpl::RegisterOverworldEditor(
    const std::string& canvas_id, editor::OverworldEditor* editor) {
  overworld_editors_[canvas_id] = editor;
}

gui::Canvas* CanvasAutomationServiceImpl::GetCanvas(const std::string& canvas_id) {
  auto it = canvases_.find(canvas_id);
  if (it != canvases_.end()) {
    return it->second;
  }
  return nullptr;
}

editor::OverworldEditor* CanvasAutomationServiceImpl::GetOverworldEditor(
    const std::string& canvas_id) {
  auto it = overworld_editors_.find(canvas_id);
  if (it != overworld_editors_.end()) {
    return it->second;
  }
  return nullptr;
}

// ============================================================================
// Tile Operations
// ============================================================================

absl::Status CanvasAutomationServiceImpl::SetTile(
    const proto::SetTileRequest* request, proto::SetTileResponse* response) {
  
  auto* canvas = GetCanvas(request->canvas_id());
  if (!canvas) {
    response->set_success(false);
    response->set_error("Canvas not found: " + request->canvas_id());
    return absl::NotFoundError(response->error());
  }

  auto* api = canvas->GetAutomationAPI();
  bool success = api->SetTileAt(request->x(), request->y(), request->tile_id());
  
  response->set_success(success);
  if (!success) {
    response->set_error("Failed to set tile - out of bounds or callback failed");
  }
  
  return absl::OkStatus();
}

absl::Status CanvasAutomationServiceImpl::GetTile(
    const proto::GetTileRequest* request, proto::GetTileResponse* response) {
  
  auto* canvas = GetCanvas(request->canvas_id());
  if (!canvas) {
    response->set_success(false);
    response->set_error("Canvas not found: " + request->canvas_id());
    return absl::NotFoundError(response->error());
  }

  auto* api = canvas->GetAutomationAPI();
  int tile_id = api->GetTileAt(request->x(), request->y());
  
  if (tile_id >= 0) {
    response->set_tile_id(tile_id);
    response->set_success(true);
  } else {
    response->set_success(false);
    response->set_error("Tile not found - out of bounds or no callback set");
  }
  
  return absl::OkStatus();
}

absl::Status CanvasAutomationServiceImpl::SetTiles(
    const proto::SetTilesRequest* request, proto::SetTilesResponse* response) {
  
  auto* canvas = GetCanvas(request->canvas_id());
  if (!canvas) {
    response->set_success(false);
    response->set_error("Canvas not found: " + request->canvas_id());
    return absl::NotFoundError(response->error());
  }

  auto* api = canvas->GetAutomationAPI();
  
  std::vector<std::tuple<int, int, int>> tiles;
  for (const auto& tile : request->tiles()) {
    tiles.push_back({tile.x(), tile.y(), tile.tile_id()});
  }
  
  bool success = api->SetTiles(tiles);
  response->set_success(success);
  response->set_tiles_painted(tiles.size());
  
  return absl::OkStatus();
}

// ============================================================================
// Selection Operations
// ============================================================================

absl::Status CanvasAutomationServiceImpl::SelectTile(
    const proto::SelectTileRequest* request, proto::SelectTileResponse* response) {
  
  auto* canvas = GetCanvas(request->canvas_id());
  if (!canvas) {
    response->set_success(false);
    response->set_error("Canvas not found: " + request->canvas_id());
    return absl::NotFoundError(response->error());
  }

  auto* api = canvas->GetAutomationAPI();
  api->SelectTile(request->x(), request->y());
  response->set_success(true);
  
  return absl::OkStatus();
}

absl::Status CanvasAutomationServiceImpl::SelectTileRect(
    const proto::SelectTileRectRequest* request,
    proto::SelectTileRectResponse* response) {
  
  auto* canvas = GetCanvas(request->canvas_id());
  if (!canvas) {
    response->set_success(false);
    response->set_error("Canvas not found: " + request->canvas_id());
    return absl::NotFoundError(response->error());
  }

  auto* api = canvas->GetAutomationAPI();
  const auto& rect = request->rect();
  api->SelectTileRect(rect.x1(), rect.y1(), rect.x2(), rect.y2());
  
  auto selection = api->GetSelection();
  response->set_success(true);
  response->set_tiles_selected(selection.selected_tiles.size());
  
  return absl::OkStatus();
}

absl::Status CanvasAutomationServiceImpl::GetSelection(
    const proto::GetSelectionRequest* request,
    proto::GetSelectionResponse* response) {
  
  auto* canvas = GetCanvas(request->canvas_id());
  if (!canvas) {
    return absl::NotFoundError("Canvas not found: " + request->canvas_id());
  }

  auto* api = canvas->GetAutomationAPI();
  auto selection = api->GetSelection();
  
  response->set_has_selection(selection.has_selection);
  
  for (const auto& tile : selection.selected_tiles) {
    auto* coord = response->add_selected_tiles();
    coord->set_x(static_cast<int>(tile.x));
    coord->set_y(static_cast<int>(tile.y));
  }
  
  auto* start = response->mutable_selection_start();
  start->set_x(static_cast<int>(selection.selection_start.x));
  start->set_y(static_cast<int>(selection.selection_start.y));
  
  auto* end = response->mutable_selection_end();
  end->set_x(static_cast<int>(selection.selection_end.x));
  end->set_y(static_cast<int>(selection.selection_end.y));
  
  return absl::OkStatus();
}

absl::Status CanvasAutomationServiceImpl::ClearSelection(
    const proto::ClearSelectionRequest* request,
    proto::ClearSelectionResponse* response) {
  
  auto* canvas = GetCanvas(request->canvas_id());
  if (!canvas) {
    response->set_success(false);
    return absl::NotFoundError("Canvas not found: " + request->canvas_id());
  }

  auto* api = canvas->GetAutomationAPI();
  api->ClearSelection();
  response->set_success(true);
  
  return absl::OkStatus();
}

// ============================================================================
// View Operations
// ============================================================================

absl::Status CanvasAutomationServiceImpl::ScrollToTile(
    const proto::ScrollToTileRequest* request,
    proto::ScrollToTileResponse* response) {
  
  auto* canvas = GetCanvas(request->canvas_id());
  if (!canvas) {
    response->set_success(false);
    response->set_error("Canvas not found: " + request->canvas_id());
    return absl::NotFoundError(response->error());
  }

  auto* api = canvas->GetAutomationAPI();
  api->ScrollToTile(request->x(), request->y(), request->center());
  response->set_success(true);
  
  return absl::OkStatus();
}

absl::Status CanvasAutomationServiceImpl::CenterOn(
    const proto::CenterOnRequest* request, proto::CenterOnResponse* response) {
  
  auto* canvas = GetCanvas(request->canvas_id());
  if (!canvas) {
    response->set_success(false);
    response->set_error("Canvas not found: " + request->canvas_id());
    return absl::NotFoundError(response->error());
  }

  auto* api = canvas->GetAutomationAPI();
  api->CenterOn(request->x(), request->y());
  response->set_success(true);
  
  return absl::OkStatus();
}

absl::Status CanvasAutomationServiceImpl::SetZoom(
    const proto::SetZoomRequest* request, proto::SetZoomResponse* response) {
  
  auto* canvas = GetCanvas(request->canvas_id());
  if (!canvas) {
    response->set_success(false);
    response->set_error("Canvas not found: " + request->canvas_id());
    return absl::NotFoundError(response->error());
  }

  auto* api = canvas->GetAutomationAPI();
  api->SetZoom(request->zoom());
  
  float actual_zoom = api->GetZoom();
  response->set_success(true);
  response->set_actual_zoom(actual_zoom);
  
  return absl::OkStatus();
}

absl::Status CanvasAutomationServiceImpl::GetZoom(
    const proto::GetZoomRequest* request, proto::GetZoomResponse* response) {
  
  auto* canvas = GetCanvas(request->canvas_id());
  if (!canvas) {
    return absl::NotFoundError("Canvas not found: " + request->canvas_id());
  }

  auto* api = canvas->GetAutomationAPI();
  response->set_zoom(api->GetZoom());
  
  return absl::OkStatus();
}

// ============================================================================
// Query Operations
// ============================================================================

absl::Status CanvasAutomationServiceImpl::GetDimensions(
    const proto::GetDimensionsRequest* request,
    proto::GetDimensionsResponse* response) {
  
  auto* canvas = GetCanvas(request->canvas_id());
  if (!canvas) {
    return absl::NotFoundError("Canvas not found: " + request->canvas_id());
  }

  auto* api = canvas->GetAutomationAPI();
  auto dims = api->GetDimensions();
  
  auto* proto_dims = response->mutable_dimensions();
  proto_dims->set_width_tiles(dims.width_tiles);
  proto_dims->set_height_tiles(dims.height_tiles);
  proto_dims->set_tile_size(dims.tile_size);
  
  return absl::OkStatus();
}

absl::Status CanvasAutomationServiceImpl::GetVisibleRegion(
    const proto::GetVisibleRegionRequest* request,
    proto::GetVisibleRegionResponse* response) {
  
  auto* canvas = GetCanvas(request->canvas_id());
  if (!canvas) {
    return absl::NotFoundError("Canvas not found: " + request->canvas_id());
  }

  auto* api = canvas->GetAutomationAPI();
  auto region = api->GetVisibleRegion();
  
  auto* proto_region = response->mutable_region();
  proto_region->set_min_x(region.min_x);
  proto_region->set_min_y(region.min_y);
  proto_region->set_max_x(region.max_x);
  proto_region->set_max_y(region.max_y);
  
  return absl::OkStatus();
}

absl::Status CanvasAutomationServiceImpl::IsTileVisible(
    const proto::IsTileVisibleRequest* request,
    proto::IsTileVisibleResponse* response) {
  
  auto* canvas = GetCanvas(request->canvas_id());
  if (!canvas) {
    return absl::NotFoundError("Canvas not found: " + request->canvas_id());
  }

  auto* api = canvas->GetAutomationAPI();
  response->set_is_visible(api->IsTileVisible(request->x(), request->y()));
  
  return absl::OkStatus();
}

}  // namespace yaze

#endif  // YAZE_WITH_GRPC

