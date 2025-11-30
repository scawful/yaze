#include "app/editor/graphics/polyhedral_editor_panel.h"

#include <algorithm>
#include <cmath>
#include <string>
#include <vector>

#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "absl/strings/str_format.h"
#include "app/gui/core/icons.h"
#include "app/gui/plots/implot_support.h"
#include "app/snes.h"
#include "imgui/imgui.h"
#include "implot.h"
#include "util/macro.h"

namespace yaze {
namespace editor {

namespace {

constexpr uint32_t kPolyTableSnes = 0x09FF8C;
constexpr uint32_t kPolyEntrySize = 6;
constexpr uint32_t kPolyRegionSize = 0x74;  // 116 bytes, $09:FF8C-$09:FFFF
constexpr uint8_t kPolyBank = 0x09;

constexpr ImVec4 kVertexColor(0.3f, 0.8f, 1.0f, 1.0f);
constexpr ImVec4 kSelectedVertexColor(1.0f, 0.75f, 0.2f, 1.0f);

template <typename T>
T Clamp(T value, T min_v, T max_v) {
  return std::max(min_v, std::min(max_v, value));
}

std::string ShapeNameForIndex(int index) {
  switch (index) {
    case 0:
      return "Crystal";
    case 1:
      return "Triforce";
    default:
      return absl::StrFormat("Shape %d", index);
  }
}

uint32_t ToPc(uint16_t bank_offset) {
  return SnesToPc((kPolyBank << 16) | bank_offset);
}

}  // namespace

uint32_t PolyhedralEditorPanel::TablePc() const {
  return SnesToPc(kPolyTableSnes);
}

absl::Status PolyhedralEditorPanel::Load() {
  RETURN_IF_ERROR(LoadShapes());
  dirty_ = false;
  return absl::OkStatus();
}

absl::Status PolyhedralEditorPanel::LoadShapes() {
  if (!rom_ || !rom_->is_loaded()) {
    return absl::FailedPreconditionError("ROM is not loaded");
  }

  // Read the whole 3D object region to keep parsing bounds explicit.
  ASSIGN_OR_RETURN(auto region,
                   rom_->ReadByteVector(TablePc(), kPolyRegionSize));

  shapes_.clear();

  // Two entries live in the table (crystal, triforce). Stop if we run out of
  // room rather than reading garbage.
  for (int i = 0; i < 2; ++i) {
    size_t base = i * kPolyEntrySize;
    if (base + kPolyEntrySize > region.size()) {
      break;
    }

    PolyShape shape;
    shape.name = ShapeNameForIndex(i);
    shape.vertex_count = region[base];
    shape.face_count = region[base + 1];
    shape.vertex_ptr =
        static_cast<uint16_t>(region[base + 2] | (region[base + 3] << 8));
    shape.face_ptr =
        static_cast<uint16_t>(region[base + 4] | (region[base + 5] << 8));

    // Vertices (signed bytes, XYZ triples)
    const uint32_t vertex_pc = ToPc(shape.vertex_ptr);
    const size_t vertex_bytes = static_cast<size_t>(shape.vertex_count) * 3;
    ASSIGN_OR_RETURN(auto vertex_blob,
                     rom_->ReadByteVector(vertex_pc, vertex_bytes));

    shape.vertices.reserve(shape.vertex_count);
    for (size_t idx = 0; idx + 2 < vertex_blob.size(); idx += 3) {
      PolyVertex v;
      v.x = static_cast<int8_t>(vertex_blob[idx]);
      v.y = static_cast<int8_t>(vertex_blob[idx + 1]);
      v.z = static_cast<int8_t>(vertex_blob[idx + 2]);
      shape.vertices.push_back(v);
    }

    // Faces (count byte, indices[count], shade byte)
    uint32_t face_pc = ToPc(shape.face_ptr);
    shape.faces.reserve(shape.face_count);
    for (int f = 0; f < shape.face_count; ++f) {
      ASSIGN_OR_RETURN(auto count_byte, rom_->ReadByte(face_pc++));
      PolyFace face;
      face.vertex_indices.reserve(count_byte);

      for (int j = 0; j < count_byte; ++j) {
        ASSIGN_OR_RETURN(auto idx_byte, rom_->ReadByte(face_pc++));
        face.vertex_indices.push_back(idx_byte);
      }

      ASSIGN_OR_RETURN(auto shade_byte, rom_->ReadByte(face_pc++));
      face.shade = shade_byte;
      shape.faces.push_back(std::move(face));
    }

    shapes_.push_back(std::move(shape));
  }

  selected_shape_ = 0;
  selected_vertex_ = 0;
  data_loaded_ = true;
  return absl::OkStatus();
}

absl::Status PolyhedralEditorPanel::SaveShapes() {
  for (auto& shape : shapes_) {
    shape.vertex_count = static_cast<uint8_t>(shape.vertices.size());
    shape.face_count = static_cast<uint8_t>(shape.faces.size());
    RETURN_IF_ERROR(WriteShape(shape));
  }
  dirty_ = false;
  return absl::OkStatus();
}

absl::Status PolyhedralEditorPanel::WriteShape(const PolyShape& shape) {
  // Vertices
  std::vector<uint8_t> vertex_blob;
  vertex_blob.reserve(shape.vertices.size() * 3);
  for (const auto& v : shape.vertices) {
    vertex_blob.push_back(static_cast<uint8_t>(static_cast<int8_t>(v.x)));
    vertex_blob.push_back(static_cast<uint8_t>(static_cast<int8_t>(v.y)));
    vertex_blob.push_back(static_cast<uint8_t>(static_cast<int8_t>(v.z)));
  }

  RETURN_IF_ERROR(
      rom_->WriteVector(ToPc(shape.vertex_ptr), std::move(vertex_blob)));

  // Faces
  std::vector<uint8_t> face_blob;
  for (const auto& face : shape.faces) {
    face_blob.push_back(static_cast<uint8_t>(face.vertex_indices.size()));
    for (auto idx : face.vertex_indices) {
      face_blob.push_back(idx);
    }
    face_blob.push_back(face.shade);
  }

  return rom_->WriteVector(ToPc(shape.face_ptr), std::move(face_blob));
}

absl::Status PolyhedralEditorPanel::Update() {
  if (!rom_ || !rom_->is_loaded()) {
    ImGui::TextUnformatted("Load a ROM to edit 3D objects.");
    return absl::OkStatus();
  }

  if (!data_loaded_) {
    RETURN_IF_ERROR(LoadShapes());
  }

  gui::plotting::EnsureImPlotContext();

  ImGui::Text("ALTTP polyhedral data @ $09:%04X (PC $%05X), %u bytes",
              static_cast<uint16_t>(kPolyTableSnes & 0xFFFF), TablePc(),
              kPolyRegionSize);
  ImGui::TextUnformatted(
      "Shapes: 0 = Crystal, 1 = Triforce (IDs used by POLYSHAPE)");

  // Shape selector
  if (!shapes_.empty()) {
    ImGui::SetNextItemWidth(180);
    if (ImGui::BeginCombo("Shape", shapes_[selected_shape_].name.c_str())) {
      for (size_t i = 0; i < shapes_.size(); ++i) {
        bool selected = static_cast<int>(i) == selected_shape_;
        if (ImGui::Selectable(shapes_[i].name.c_str(), selected)) {
          selected_shape_ = static_cast<int>(i);
          selected_vertex_ = 0;
        }
      }
      ImGui::EndCombo();
    }
  }

  if (ImGui::Button(ICON_MD_REFRESH " Reload from ROM")) {
    RETURN_IF_ERROR(LoadShapes());
  }
  ImGui::SameLine();
  ImGui::BeginDisabled(!dirty_);
  if (ImGui::Button(ICON_MD_SAVE " Save 3D objects")) {
    RETURN_IF_ERROR(SaveShapes());
  }
  ImGui::EndDisabled();

  if (shapes_.empty()) {
    ImGui::TextUnformatted("No polyhedral shapes found.");
    return absl::OkStatus();
  }

  ImGui::Separator();
  DrawShapeEditor(shapes_[selected_shape_]);
  return absl::OkStatus();
}

void PolyhedralEditorPanel::DrawShapeEditor(PolyShape& shape) {
  ImGui::Text("Vertices: %u  Faces: %u", shape.vertex_count,
              shape.face_count);
  ImGui::Text("Vertex data @ $09:%04X (PC $%05X)", shape.vertex_ptr,
              ToPc(shape.vertex_ptr));
  ImGui::Text("Face data   @ $09:%04X (PC $%05X)", shape.face_ptr,
              ToPc(shape.face_ptr));

  ImGui::Spacing();

  if (ImGui::BeginTable("##poly_editor", 2,
                        ImGuiTableFlags_Resizable | ImGuiTableFlags_SizingStretchProp)) {
    ImGui::TableSetupColumn("Data", ImGuiTableColumnFlags_WidthStretch, 0.45f);
    ImGui::TableSetupColumn("Plots", ImGuiTableColumnFlags_WidthStretch, 0.55f);

    ImGui::TableNextColumn();
    DrawVertexList(shape);
    ImGui::Spacing();
    DrawFaceList(shape);

    ImGui::TableNextColumn();
    DrawPlot("XY (X vs Y)", PlotPlane::kXY, shape);
    DrawPlot("XZ (X vs Z)", PlotPlane::kXZ, shape);
    ImGui::EndTable();
  }
}

void PolyhedralEditorPanel::DrawVertexList(PolyShape& shape) {
  if (shape.vertices.empty()) {
    ImGui::TextUnformatted("No vertices");
    return;
  }

  for (size_t i = 0; i < shape.vertices.size(); ++i) {
    ImGui::PushID(static_cast<int>(i));
    const bool is_selected = static_cast<int>(i) == selected_vertex_;
    std::string label = absl::StrFormat("Vertex %zu", i);
    if (ImGui::Selectable(label.c_str(), is_selected)) {
      selected_vertex_ = static_cast<int>(i);
    }

    ImGui::SameLine();
    ImGui::SetNextItemWidth(210);
    int coords[3] = {shape.vertices[i].x, shape.vertices[i].y,
                     shape.vertices[i].z};
    if (ImGui::InputInt3("##coords", coords)) {
      shape.vertices[i].x = Clamp(coords[0], -127, 127);
      shape.vertices[i].y = Clamp(coords[1], -127, 127);
      shape.vertices[i].z = Clamp(coords[2], -127, 127);
      dirty_ = true;
    }
    ImGui::PopID();
  }
}

void PolyhedralEditorPanel::DrawFaceList(PolyShape& shape) {
  if (shape.faces.empty()) {
    ImGui::TextUnformatted("No faces");
    return;
  }

  ImGui::TextUnformatted("Faces (vertex indices + shade)");
  for (size_t i = 0; i < shape.faces.size(); ++i) {
    ImGui::PushID(static_cast<int>(i));
    ImGui::Text("Face %zu", i);
    ImGui::SameLine();
    int shade = shape.faces[i].shade;
    ImGui::SetNextItemWidth(70);
    if (ImGui::InputInt("Shade##face", &shade, 0, 0)) {
      shape.faces[i].shade = static_cast<uint8_t>(Clamp(shade, 0, 0xFF));
      dirty_ = true;
    }

    ImGui::SameLine();
    ImGui::TextUnformatted("Vertices:");
    const int max_idx =
        shape.vertices.empty()
            ? 0
            : static_cast<int>(shape.vertices.size() - 1);
    for (size_t v = 0; v < shape.faces[i].vertex_indices.size(); ++v) {
      ImGui::SameLine();
      int idx = shape.faces[i].vertex_indices[v];
      ImGui::SetNextItemWidth(40);
      if (ImGui::InputInt(absl::StrFormat("##v%zu", v).c_str(), &idx, 0, 0)) {
        idx = Clamp(idx, 0, max_idx);
        shape.faces[i].vertex_indices[v] = static_cast<uint8_t>(idx);
        dirty_ = true;
      }
    }
    ImGui::PopID();
  }
}

void PolyhedralEditorPanel::DrawPlot(const char* label, PlotPlane plane,
                                     PolyShape& shape) {
  if (shape.vertices.empty()) {
    return;
  }

  ImVec2 plot_size = ImVec2(-1, 220);
  ImPlotFlags flags = ImPlotFlags_NoLegend | ImPlotFlags_Equal;
  if (ImPlot::BeginPlot(label, plot_size, flags)) {
    const char* x_label = (plane == PlotPlane::kYZ) ? "Y" : "X";
    const char* y_label = (plane == PlotPlane::kXY)
                              ? "Y"
                              : "Z";
    ImPlot::SetupAxes(x_label, y_label, ImPlotAxisFlags_AutoFit,
                      ImPlotAxisFlags_AutoFit);
    ImPlot::SetupAxisLimits(ImAxis_X1, -80, 80, ImGuiCond_Once);
    ImPlot::SetupAxisLimits(ImAxis_Y1, -80, 80, ImGuiCond_Once);

    for (size_t i = 0; i < shape.vertices.size(); ++i) {
      double x = shape.vertices[i].x;
      double y = 0.0;
      switch (plane) {
        case PlotPlane::kXY:
          y = shape.vertices[i].y;
          break;
        case PlotPlane::kXZ:
          y = shape.vertices[i].z;
          break;
        case PlotPlane::kYZ:
          x = shape.vertices[i].y;
          y = shape.vertices[i].z;
          break;
      }

      const bool is_selected = static_cast<int>(i) == selected_vertex_;
      ImVec4 color = is_selected ? kSelectedVertexColor : kVertexColor;
      // ImPlot::DragPoint wants an int ID, so compose one from vertex index and plane.
      int point_id =
          static_cast<int>(i * 10 + static_cast<size_t>(plane));
      if (ImPlot::DragPoint(point_id, &x, &y, color, 6.0f)) {
        // Round so we keep integer coordinates in ROM
        int rounded_x = Clamp(static_cast<int>(std::lround(x)), -127, 127);
        int rounded_y = Clamp(static_cast<int>(std::lround(y)), -127, 127);

        switch (plane) {
          case PlotPlane::kXY:
            shape.vertices[i].x = rounded_x;
            shape.vertices[i].y = rounded_y;
            break;
          case PlotPlane::kXZ:
            shape.vertices[i].x = rounded_x;
            shape.vertices[i].z = rounded_y;
            break;
          case PlotPlane::kYZ:
            shape.vertices[i].y = rounded_x;
            shape.vertices[i].z = rounded_y;
            break;
        }

        dirty_ = true;
        if (!is_selected) {
          selected_vertex_ = static_cast<int>(i);
        }
      }
    }
    ImPlot::EndPlot();
  }
}

}  // namespace editor
}  // namespace yaze
