#include "cli/service/gui/canvas_automation_client.h"

#include "absl/strings/str_cat.h"

namespace yaze {
namespace cli {

CanvasAutomationClient::CanvasAutomationClient(const std::string& server_address)
    : server_address_(server_address) {}

absl::Status CanvasAutomationClient::Connect() {
#ifdef YAZE_WITH_GRPC
  auto channel = grpc::CreateChannel(server_address_,
                                     grpc::InsecureChannelCredentials());
  stub_ = proto::CanvasAutomation::NewStub(channel);
  return absl::OkStatus();
#else
  return absl::UnimplementedError("gRPC support not enabled");
#endif
}

absl::Status CanvasAutomationClient::SetTile(const std::string& canvas_id, int x, int y, int tile_id) {
#ifdef YAZE_WITH_GRPC
  if (!stub_) return absl::FailedPreconditionError("Not connected");

  grpc::ClientContext context;
  proto::SetTileRequest request;
  request.set_canvas_id(canvas_id);
  request.set_x(x);
  request.set_y(y);
  request.set_tile_id(tile_id);
  
  proto::SetTileResponse response;
  grpc::Status status = stub_->SetTile(&context, request, &response);

  if (!status.ok()) {
    return absl::InternalError(status.error_message());
  }
  if (!response.success()) {
    return absl::InternalError(response.error());
  }
  return absl::OkStatus();
#else
  return absl::UnimplementedError("gRPC support not enabled");
#endif
}

absl::StatusOr<int> CanvasAutomationClient::GetTile(const std::string& canvas_id, int x, int y) {
#ifdef YAZE_WITH_GRPC
  if (!stub_) return absl::FailedPreconditionError("Not connected");

  grpc::ClientContext context;
  proto::GetTileRequest request;
  request.set_canvas_id(canvas_id);
  request.set_x(x);
  request.set_y(y);
  
  proto::GetTileResponse response;
  grpc::Status status = stub_->GetTile(&context, request, &response);

  if (!status.ok()) {
    return absl::InternalError(status.error_message());
  }
  if (!response.success()) {
    return absl::InternalError(response.error());
  }
  return response.tile_id();
#else
  return absl::UnimplementedError("gRPC support not enabled");
#endif
}

absl::Status CanvasAutomationClient::SetTiles(const std::string& canvas_id, const std::vector<TileData>& tiles) {
#ifdef YAZE_WITH_GRPC
  if (!stub_) return absl::FailedPreconditionError("Not connected");

  grpc::ClientContext context;
  proto::SetTilesRequest request;
  request.set_canvas_id(canvas_id);
  
  for (const auto& t : tiles) {
    auto* tile = request.add_tiles();
    tile->set_x(t.x);
    tile->set_y(t.y);
    tile->set_tile_id(t.tile_id);
  }
  
  proto::SetTilesResponse response;
  grpc::Status status = stub_->SetTiles(&context, request, &response);

  if (!status.ok()) {
    return absl::InternalError(status.error_message());
  }
  if (!response.success()) {
    return absl::InternalError(response.error());
  }
  return absl::OkStatus();
#else
  return absl::UnimplementedError("gRPC support not enabled");
#endif
}

absl::Status CanvasAutomationClient::SelectTile(const std::string& canvas_id, int x, int y) {
#ifdef YAZE_WITH_GRPC
  if (!stub_) return absl::FailedPreconditionError("Not connected");

  grpc::ClientContext context;
  proto::SelectTileRequest request;
  request.set_canvas_id(canvas_id);
  request.set_x(x);
  request.set_y(y);
  
  proto::SelectTileResponse response;
  grpc::Status status = stub_->SelectTile(&context, request, &response);

  if (!status.ok()) {
    return absl::InternalError(status.error_message());
  }
  if (!response.success()) {
    return absl::InternalError(response.error());
  }
  return absl::OkStatus();
#else
  return absl::UnimplementedError("gRPC support not enabled");
#endif
}

absl::Status CanvasAutomationClient::SelectTileRect(const std::string& canvas_id, int x1, int y1, int x2, int y2) {
#ifdef YAZE_WITH_GRPC
  if (!stub_) return absl::FailedPreconditionError("Not connected");

  grpc::ClientContext context;
  proto::SelectTileRectRequest request;
  request.set_canvas_id(canvas_id);
  auto* rect = request.mutable_rect();
  rect->set_x1(x1);
  rect->set_y1(y1);
  rect->set_x2(x2);
  rect->set_y2(y2);
  
  proto::SelectTileRectResponse response;
  grpc::Status status = stub_->SelectTileRect(&context, request, &response);

  if (!status.ok()) {
    return absl::InternalError(status.error_message());
  }
  if (!response.success()) {
    return absl::InternalError(response.error());
  }
  return absl::OkStatus();
#else
  return absl::UnimplementedError("gRPC support not enabled");
#endif
}

absl::Status CanvasAutomationClient::ClearSelection(const std::string& canvas_id) {
#ifdef YAZE_WITH_GRPC
  if (!stub_) return absl::FailedPreconditionError("Not connected");

  grpc::ClientContext context;
  proto::ClearSelectionRequest request;
  request.set_canvas_id(canvas_id);
  
  proto::ClearSelectionResponse response;
  grpc::Status status = stub_->ClearSelection(&context, request, &response);

  if (!status.ok()) {
    return absl::InternalError(status.error_message());
  }
  if (!response.success()) {
    // ClearSelection usually succeeds even if nothing selected, but respect error
    // return absl::InternalError(response.error()); 
    // Note: response message doesn't have error field in proto definition for ClearSelectionResponse?
    // Checking proto... yes it does: bool success = 1;
    // Wait, proto def:
    // message ClearSelectionResponse { bool success = 1; }
    // It doesn't have error string. My bad.
    if (!response.success()) return absl::InternalError("ClearSelection failed");
  }
  return absl::OkStatus();
#else
  return absl::UnimplementedError("gRPC support not enabled");
#endif
}

absl::Status CanvasAutomationClient::ScrollToTile(const std::string& canvas_id, int x, int y, bool center) {
#ifdef YAZE_WITH_GRPC
  if (!stub_) return absl::FailedPreconditionError("Not connected");

  grpc::ClientContext context;
  proto::ScrollToTileRequest request;
  request.set_canvas_id(canvas_id);
  request.set_x(x);
  request.set_y(y);
  request.set_center(center);
  
  proto::ScrollToTileResponse response;
  grpc::Status status = stub_->ScrollToTile(&context, request, &response);

  if (!status.ok()) {
    return absl::InternalError(status.error_message());
  }
  if (!response.success()) {
    return absl::InternalError(response.error());
  }
  return absl::OkStatus();
#else
  return absl::UnimplementedError("gRPC support not enabled");
#endif
}

absl::Status CanvasAutomationClient::SetZoom(const std::string& canvas_id, float zoom) {
#ifdef YAZE_WITH_GRPC
  if (!stub_) return absl::FailedPreconditionError("Not connected");

  grpc::ClientContext context;
  proto::SetZoomRequest request;
  request.set_canvas_id(canvas_id);
  request.set_zoom(zoom);
  
  proto::SetZoomResponse response;
  grpc::Status status = stub_->SetZoom(&context, request, &response);

  if (!status.ok()) {
    return absl::InternalError(status.error_message());
  }
  if (!response.success()) {
    return absl::InternalError(response.error());
  }
  return absl::OkStatus();
#else
  return absl::UnimplementedError("gRPC support not enabled");
#endif
}

}  // namespace cli
}  // namespace yaze
