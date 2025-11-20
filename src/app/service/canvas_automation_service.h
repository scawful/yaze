#ifndef YAZE_APP_CORE_SERVICE_CANVAS_AUTOMATION_SERVICE_H_
#define YAZE_APP_CORE_SERVICE_CANVAS_AUTOMATION_SERVICE_H_

#include "grpcpp/impl/service_type.h"
#ifdef YAZE_WITH_GRPC

#include <memory>
#include <string>
#include <unordered_map>

#include "absl/status/status.h"
#include "app/gui/canvas/canvas.h"

// Forward declarations
namespace grpc {
class ServerContext;
}

namespace yaze {
namespace editor {
class OverworldEditor;
}

namespace proto {
// Forward declare proto types
class SetTileRequest;
class SetTileResponse;
class GetTileRequest;
class GetTileResponse;
class SetTilesRequest;
class SetTilesResponse;
class SelectTileRequest;
class SelectTileResponse;
class SelectTileRectRequest;
class SelectTileRectResponse;
class GetSelectionRequest;
class GetSelectionResponse;
class ClearSelectionRequest;
class ClearSelectionResponse;
class ScrollToTileRequest;
class ScrollToTileResponse;
class CenterOnRequest;
class CenterOnResponse;
class SetZoomRequest;
class SetZoomResponse;
class GetZoomRequest;
class GetZoomResponse;
class GetDimensionsRequest;
class GetDimensionsResponse;
class GetVisibleRegionRequest;
class GetVisibleRegionResponse;
class IsTileVisibleRequest;
class IsTileVisibleResponse;
}  // namespace proto

/**
 * @brief Implementation of CanvasAutomation gRPC service
 *
 * Provides remote access to canvas automation API for:
 * - AI agent tool calls
 * - Remote GUI testing
 * - Collaborative editing workflows
 * - CLI automation scripts
 */
class CanvasAutomationServiceImpl {
 public:
  CanvasAutomationServiceImpl() = default;

  // Register a canvas for automation
  void RegisterCanvas(const std::string& canvas_id, gui::Canvas* canvas);

  // Register an overworld editor (for tile get/set callbacks)
  void RegisterOverworldEditor(const std::string& canvas_id,
                               editor::OverworldEditor* editor);

  // RPC method implementations
  absl::Status SetTile(const proto::SetTileRequest* request,
                       proto::SetTileResponse* response);

  absl::Status GetTile(const proto::GetTileRequest* request,
                       proto::GetTileResponse* response);

  absl::Status SetTiles(const proto::SetTilesRequest* request,
                        proto::SetTilesResponse* response);

  absl::Status SelectTile(const proto::SelectTileRequest* request,
                          proto::SelectTileResponse* response);

  absl::Status SelectTileRect(const proto::SelectTileRectRequest* request,
                              proto::SelectTileRectResponse* response);

  absl::Status GetSelection(const proto::GetSelectionRequest* request,
                            proto::GetSelectionResponse* response);

  absl::Status ClearSelection(const proto::ClearSelectionRequest* request,
                              proto::ClearSelectionResponse* response);

  absl::Status ScrollToTile(const proto::ScrollToTileRequest* request,
                            proto::ScrollToTileResponse* response);

  absl::Status CenterOn(const proto::CenterOnRequest* request,
                        proto::CenterOnResponse* response);

  absl::Status SetZoom(const proto::SetZoomRequest* request,
                       proto::SetZoomResponse* response);

  absl::Status GetZoom(const proto::GetZoomRequest* request,
                       proto::GetZoomResponse* response);

  absl::Status GetDimensions(const proto::GetDimensionsRequest* request,
                             proto::GetDimensionsResponse* response);

  absl::Status GetVisibleRegion(const proto::GetVisibleRegionRequest* request,
                                proto::GetVisibleRegionResponse* response);

  absl::Status IsTileVisible(const proto::IsTileVisibleRequest* request,
                             proto::IsTileVisibleResponse* response);

 private:
  gui::Canvas* GetCanvas(const std::string& canvas_id);
  editor::OverworldEditor* GetOverworldEditor(const std::string& canvas_id);

  // Canvas registry
  std::unordered_map<std::string, gui::Canvas*> canvases_;

  // Editor registry (for tile callbacks)
  std::unordered_map<std::string, editor::OverworldEditor*> overworld_editors_;
};

/**
 * @brief Factory function to create gRPC service wrapper
 * 
 * Creates the gRPC service wrapper for CanvasAutomationServiceImpl.
 * The wrapper handles the conversion between gRPC and absl::Status.
 * Returns as base grpc::Service to avoid incomplete type issues.
 * 
 * @param impl Pointer to implementation (not owned)
 * @return Unique pointer to gRPC service
 */
std::unique_ptr<grpc::Service> CreateCanvasAutomationServiceGrpc(
    CanvasAutomationServiceImpl* impl);

}  // namespace yaze

#endif  // YAZE_WITH_GRPC
#endif  // YAZE_APP_CORE_SERVICE_CANVAS_AUTOMATION_SERVICE_H_
