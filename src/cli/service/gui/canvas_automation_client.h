#ifndef YAZE_CLI_SERVICE_GUI_CANVAS_AUTOMATION_CLIENT_H_
#define YAZE_CLI_SERVICE_GUI_CANVAS_AUTOMATION_CLIENT_H_

#include <memory>
#include <string>
#include <vector>

#include "absl/status/status.h"
#include "absl/status/statusor.h"

#ifdef YAZE_WITH_GRPC
#ifdef _WIN32
#pragma push_macro("DWORD")
#pragma push_macro("ERROR")
#undef DWORD
#undef ERROR
#endif

#include <grpcpp/grpcpp.h>
#include "protos/canvas_automation.grpc.pb.h"

#ifdef _WIN32
#pragma pop_macro("DWORD")
#pragma pop_macro("ERROR")
#endif
#endif

namespace yaze {
namespace cli {

class CanvasAutomationClient {
 public:
  explicit CanvasAutomationClient(const std::string& server_address);

  absl::Status Connect();

  absl::Status SetTile(const std::string& canvas_id, int x, int y, int tile_id);
  absl::StatusOr<int> GetTile(const std::string& canvas_id, int x, int y);
  
  struct TileData {
    int x;
    int y;
    int tile_id;
  };
  absl::Status SetTiles(const std::string& canvas_id, const std::vector<TileData>& tiles);

  absl::Status SelectTile(const std::string& canvas_id, int x, int y);
  absl::Status SelectTileRect(const std::string& canvas_id, int x1, int y1, int x2, int y2);
  absl::Status ClearSelection(const std::string& canvas_id);

  absl::Status ScrollToTile(const std::string& canvas_id, int x, int y, bool center = true);
  absl::Status SetZoom(const std::string& canvas_id, float zoom);

 private:
  std::string server_address_;
#ifdef YAZE_WITH_GRPC
  std::unique_ptr<proto::CanvasAutomation::Stub> stub_;
#endif
};

}  // namespace cli
}  // namespace yaze

#endif  // YAZE_CLI_SERVICE_GUI_CANVAS_AUTOMATION_CLIENT_H_
