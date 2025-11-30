#ifndef YAZE_APP_EDITOR_GRAPHICS_POLYHEDRAL_EDITOR_PANEL_H_
#define YAZE_APP_EDITOR_GRAPHICS_POLYHEDRAL_EDITOR_PANEL_H_

#include <string>
#include <vector>

#include "absl/status/status.h"
#include "app/rom.h"

namespace yaze {
namespace editor {

struct PolyVertex {
  int x = 0;
  int y = 0;
  int z = 0;
};

struct PolyFace {
  uint8_t shade = 0;
  std::vector<uint8_t> vertex_indices;
};

struct PolyShape {
  std::string name;
  uint8_t vertex_count = 0;
  uint8_t face_count = 0;
  uint16_t vertex_ptr = 0;
  uint16_t face_ptr = 0;
  std::vector<PolyVertex> vertices;
  std::vector<PolyFace> faces;
};

class PolyhedralEditorPanel {
 public:
  explicit PolyhedralEditorPanel(Rom* rom = nullptr) : rom_(rom) {}

  void SetRom(Rom* rom) {
    rom_ = rom;
    data_loaded_ = false;
  }

  absl::Status Load();
  absl::Status Update();

 private:
  enum class PlotPlane { kXY, kXZ, kYZ };

  absl::Status LoadShapes();
  absl::Status SaveShapes();
  absl::Status WriteShape(const PolyShape& shape);
  void DrawShapeEditor(PolyShape& shape);
  void DrawVertexList(PolyShape& shape);
  void DrawFaceList(PolyShape& shape);
  void DrawPlot(const char* label, PlotPlane plane, PolyShape& shape);

  uint32_t TablePc() const;

  Rom* rom_ = nullptr;
  bool data_loaded_ = false;
  bool dirty_ = false;
  int selected_shape_ = 0;
  int selected_vertex_ = 0;

  std::vector<PolyShape> shapes_;
};

}  // namespace editor
}  // namespace yaze

#endif  // YAZE_APP_EDITOR_GRAPHICS_POLYHEDRAL_EDITOR_PANEL_H_
