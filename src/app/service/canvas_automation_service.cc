#include "app/service/canvas_automation_service.h"

#ifdef YAZE_WITH_GRPC

#include <grpcpp/grpcpp.h>
#include "app/editor/overworld/overworld_editor.h"
#include "app/gui/canvas/canvas_automation_api.h"
#include "protos/canvas_automation.grpc.pb.h"
#include "protos/canvas_automation.pb.h"

namespace yaze {

namespace {

// Helper to convert absl::Status to grpc::Status
grpc::Status ConvertStatus(const absl::Status& status) {
  if (status.ok()) {
    return grpc::Status::OK;
  }

  grpc::StatusCode code;
  switch (status.code()) {
    case absl::StatusCode::kNotFound:
      code = grpc::StatusCode::NOT_FOUND;
      break;
    case absl::StatusCode::kInvalidArgument:
      code = grpc::StatusCode::INVALID_ARGUMENT;
      break;
    case absl::StatusCode::kFailedPrecondition:
      code = grpc::StatusCode::FAILED_PRECONDITION;
      break;
    case absl::StatusCode::kOutOfRange:
      code = grpc::StatusCode::OUT_OF_RANGE;
      break;
    case absl::StatusCode::kUnimplemented:
      code = grpc::StatusCode::UNIMPLEMENTED;
      break;
    case absl::StatusCode::kInternal:
      code = grpc::StatusCode::INTERNAL;
      break;
    case absl::StatusCode::kUnavailable:
      code = grpc::StatusCode::UNAVAILABLE;
      break;
    default:
      code = grpc::StatusCode::UNKNOWN;
      break;
  }

  return grpc::Status(code, std::string(status.message()));
}

}  // namespace

void CanvasAutomationServiceImpl::RegisterCanvas(const std::string& canvas_id,
                                                 gui::Canvas* canvas) {
  canvases_[canvas_id] = canvas;
}

void CanvasAutomationServiceImpl::RegisterOverworldEditor(
    const std::string& canvas_id, editor::OverworldEditor* editor) {
  overworld_editors_[canvas_id] = editor;
}

gui::Canvas* CanvasAutomationServiceImpl::GetCanvas(
    const std::string& canvas_id) {
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
    response->set_error(
        "Failed to set tile - out of bounds or callback failed");
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
    const proto::SelectTileRequest* request,
    proto::SelectTileResponse* response) {

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

// ============================================================================
// gRPC Service Wrapper
// ============================================================================

/**
 * @brief gRPC service wrapper that forwards to CanvasAutomationServiceImpl
 * 
 * This adapter implements the proto-generated Service interface and
 * forwards all calls to our implementation, converting between gRPC
 * and absl::Status types.
 */
class CanvasAutomationServiceGrpc final
    : public proto::CanvasAutomation::Service {
 public:
  explicit CanvasAutomationServiceGrpc(CanvasAutomationServiceImpl* impl)
      : impl_(impl) {}

  // Tile Operations
  grpc::Status SetTile(grpc::ServerContext* context,
                       const proto::SetTileRequest* request,
                       proto::SetTileResponse* response) override {
    return ConvertStatus(impl_->SetTile(request, response));
  }

  grpc::Status GetTile(grpc::ServerContext* context,
                       const proto::GetTileRequest* request,
                       proto::GetTileResponse* response) override {
    return ConvertStatus(impl_->GetTile(request, response));
  }

  grpc::Status SetTiles(grpc::ServerContext* context,
                        const proto::SetTilesRequest* request,
                        proto::SetTilesResponse* response) override {
    return ConvertStatus(impl_->SetTiles(request, response));
  }

  // Selection Operations
  grpc::Status SelectTile(grpc::ServerContext* context,
                          const proto::SelectTileRequest* request,
                          proto::SelectTileResponse* response) override {
    return ConvertStatus(impl_->SelectTile(request, response));
  }

  grpc::Status SelectTileRect(
      grpc::ServerContext* context, const proto::SelectTileRectRequest* request,
      proto::SelectTileRectResponse* response) override {
    return ConvertStatus(impl_->SelectTileRect(request, response));
  }

  grpc::Status GetSelection(grpc::ServerContext* context,
                            const proto::GetSelectionRequest* request,
                            proto::GetSelectionResponse* response) override {
    return ConvertStatus(impl_->GetSelection(request, response));
  }

  grpc::Status ClearSelection(
      grpc::ServerContext* context, const proto::ClearSelectionRequest* request,
      proto::ClearSelectionResponse* response) override {
    return ConvertStatus(impl_->ClearSelection(request, response));
  }

  // View Operations
  grpc::Status ScrollToTile(grpc::ServerContext* context,
                            const proto::ScrollToTileRequest* request,
                            proto::ScrollToTileResponse* response) override {
    return ConvertStatus(impl_->ScrollToTile(request, response));
  }

  grpc::Status CenterOn(grpc::ServerContext* context,
                        const proto::CenterOnRequest* request,
                        proto::CenterOnResponse* response) override {
    return ConvertStatus(impl_->CenterOn(request, response));
  }

  grpc::Status SetZoom(grpc::ServerContext* context,
                       const proto::SetZoomRequest* request,
                       proto::SetZoomResponse* response) override {
    return ConvertStatus(impl_->SetZoom(request, response));
  }

  grpc::Status GetZoom(grpc::ServerContext* context,
                       const proto::GetZoomRequest* request,
                       proto::GetZoomResponse* response) override {
    return ConvertStatus(impl_->GetZoom(request, response));
  }

  // Query Operations
  grpc::Status GetDimensions(grpc::ServerContext* context,
                             const proto::GetDimensionsRequest* request,
                             proto::GetDimensionsResponse* response) override {
    return ConvertStatus(impl_->GetDimensions(request, response));
  }

  grpc::Status GetVisibleRegion(
      grpc::ServerContext* context,
      const proto::GetVisibleRegionRequest* request,
      proto::GetVisibleRegionResponse* response) override {
    return ConvertStatus(impl_->GetVisibleRegion(request, response));
  }

  grpc::Status IsTileVisible(grpc::ServerContext* context,
                             const proto::IsTileVisibleRequest* request,
                             proto::IsTileVisibleResponse* response) override {
    return ConvertStatus(impl_->IsTileVisible(request, response));
  }

 private:
  CanvasAutomationServiceImpl* impl_;
};

// Factory function to create the gRPC wrapper
// Returns as base grpc::Service* to avoid incomplete type issues in headers
std::unique_ptr<grpc::Service> CreateCanvasAutomationServiceGrpc(
    CanvasAutomationServiceImpl* impl) {
  return std::make_unique<CanvasAutomationServiceGrpc>(impl);
}

}  // namespace yaze

#endif  // YAZE_WITH_GRPC
