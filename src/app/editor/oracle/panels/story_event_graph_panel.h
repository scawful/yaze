#ifndef YAZE_APP_EDITOR_ORACLE_PANELS_STORY_EVENT_GRAPH_PANEL_H
#define YAZE_APP_EDITOR_ORACLE_PANELS_STORY_EVENT_GRAPH_PANEL_H

#include <cmath>
#include <string>

#include "app/editor/core/content_registry.h"
#include "app/editor/system/editor_panel.h"
#include "app/gui/core/icons.h"
#include "core/hack_manifest.h"
#include "core/project.h"
#include "core/story_event_graph.h"
#include "imgui/imgui.h"

namespace yaze::editor {

/**
 * @class StoryEventGraphPanel
 * @brief Interactive node graph of Oracle narrative progression.
 *
 * Renders story events as rounded rectangles connected by Bezier curves,
 * colored by SRAM completion state (green=completed, yellow=available,
 * gray=locked, red=blocked).
 *
 * Supports pan/zoom via mouse drag + scroll.
 * Click a node to show detail sidebar with flags, locations, and text IDs.
 */
class StoryEventGraphPanel : public EditorPanel {
 public:
  StoryEventGraphPanel() = default;

  /**
   * @brief Inject manifest pointer (called by host editor or lazy-resolved).
   */
  void SetManifest(core::HackManifest* manifest) { manifest_ = manifest; }

  std::string GetId() const override { return "oracle.story_event_graph"; }
  std::string GetDisplayName() const override { return "Story Event Graph"; }
  std::string GetIcon() const override { return ICON_MD_ACCOUNT_TREE; }
  std::string GetEditorCategory() const override { return "Oracle"; }
  PanelCategory GetPanelCategory() const override {
    return PanelCategory::CrossEditor;
  }
  float GetPreferredWidth() const override { return 600.0f; }

  void Draw(bool* /*p_open*/) override {
    // Lazily resolve the manifest from the project context
    if (!manifest_) {
      auto* project = ContentRegistry::Context::current_project();
      if (project && project->hack_manifest.loaded()) {
        manifest_ = &project->hack_manifest;
      }
    }

    if (!manifest_ || !manifest_->HasProjectRegistry()) {
      ImGui::TextDisabled("No Oracle project loaded");
      ImGui::TextDisabled(
          "Open a project with a hack manifest to view story events.");
      return;
    }

    const auto& graph = manifest_->project_registry().story_events;
    if (!graph.loaded()) {
      ImGui::TextDisabled("No story events data available");
      return;
    }

    // Controls row
    if (ImGui::Button("Reset View")) {
      scroll_x_ = 0;
      scroll_y_ = 0;
      zoom_ = 1.0f;
    }
    ImGui::SameLine();
    ImGui::SliderFloat("Zoom", &zoom_, 0.3f, 2.0f, "%.1f");
    ImGui::SameLine();
    ImGui::Text("Nodes: %zu  Edges: %zu", graph.nodes().size(),
                graph.edges().size());
    ImGui::SameLine();
    const auto prog_opt = manifest_->oracle_progression_state();
    if (prog_opt.has_value()) {
      ImGui::TextDisabled("Crystals: %d  State: %s",
                          prog_opt->GetCrystalCount(),
                          prog_opt->GetGameStateName().c_str());
    } else {
      ImGui::TextDisabled("No SRAM loaded");
    }

    ImGui::Separator();

    // Main canvas area
    ImVec2 canvas_pos = ImGui::GetCursorScreenPos();
    ImVec2 canvas_size = ImGui::GetContentRegionAvail();

    // Reserve space for detail sidebar if a node is selected
    float sidebar_width = selected_node_.empty() ? 0.0f : 250.0f;
    canvas_size.x -= sidebar_width;

    if (canvas_size.x < 100 || canvas_size.y < 100) return;

    ImGui::InvisibleButton("story_canvas", canvas_size,
                           ImGuiButtonFlags_MouseButtonLeft |
                               ImGuiButtonFlags_MouseButtonRight);

    bool is_hovered = ImGui::IsItemHovered();
    bool is_active = ImGui::IsItemActive();

    // Pan with right mouse button
    if (is_active && ImGui::IsMouseDragging(ImGuiMouseButton_Right)) {
      ImVec2 delta = ImGui::GetIO().MouseDelta;
      scroll_x_ += delta.x;
      scroll_y_ += delta.y;
    }

    // Zoom with scroll wheel
    if (is_hovered) {
      float wheel = ImGui::GetIO().MouseWheel;
      if (wheel != 0.0f) {
        zoom_ *= (wheel > 0) ? 1.1f : 0.9f;
        if (zoom_ < 0.3f) zoom_ = 0.3f;
        if (zoom_ > 2.0f) zoom_ = 2.0f;
      }
    }

    ImDrawList* draw_list = ImGui::GetWindowDrawList();

    // Clip to canvas
    draw_list->PushClipRect(canvas_pos,
                            ImVec2(canvas_pos.x + canvas_size.x,
                                   canvas_pos.y + canvas_size.y),
                            true);

    // Center offset
    float cx = canvas_pos.x + canvas_size.x * 0.5f + scroll_x_;
    float cy = canvas_pos.y + canvas_size.y * 0.5f + scroll_y_;

    // Draw edges first (behind nodes)
    for (const auto& edge : graph.edges()) {
      const auto* from_node = graph.GetNode(edge.from);
      const auto* to_node = graph.GetNode(edge.to);
      if (!from_node || !to_node) continue;

      ImVec2 p1(cx + from_node->pos_x * zoom_ + kNodeWidth * zoom_ * 0.5f,
                cy + from_node->pos_y * zoom_);
      ImVec2 p2(cx + to_node->pos_x * zoom_ - kNodeWidth * zoom_ * 0.5f,
                cy + to_node->pos_y * zoom_);

      // Bezier control points
      float ctrl_dx = (p2.x - p1.x) * 0.4f;
      ImVec2 cp1(p1.x + ctrl_dx, p1.y);
      ImVec2 cp2(p2.x - ctrl_dx, p2.y);

      draw_list->AddBezierCubic(p1, cp1, cp2, p2, IM_COL32(150, 150, 150, 180),
                                 1.5f * zoom_);

      // Arrow head
      ImVec2 dir(p2.x - cp2.x, p2.y - cp2.y);
      float len = sqrtf(dir.x * dir.x + dir.y * dir.y);
      if (len > 0) {
        dir.x /= len;
        dir.y /= len;
        float arrow_size = 8.0f * zoom_;
        ImVec2 arrow1(p2.x - dir.x * arrow_size + dir.y * arrow_size * 0.4f,
                      p2.y - dir.y * arrow_size - dir.x * arrow_size * 0.4f);
        ImVec2 arrow2(p2.x - dir.x * arrow_size - dir.y * arrow_size * 0.4f,
                      p2.y - dir.y * arrow_size + dir.x * arrow_size * 0.4f);
        draw_list->AddTriangleFilled(p2, arrow1, arrow2,
                                     IM_COL32(150, 150, 150, 200));
      }
    }

    // Draw nodes
    ImVec2 mouse_pos = ImGui::GetIO().MousePos;

    for (const auto& node : graph.nodes()) {
      float nx = cx + node.pos_x * zoom_ - kNodeWidth * zoom_ * 0.5f;
      float ny = cy + node.pos_y * zoom_ - kNodeHeight * zoom_ * 0.5f;
      float nw = kNodeWidth * zoom_;
      float nh = kNodeHeight * zoom_;

      ImVec2 node_min(nx, ny);
      ImVec2 node_max(nx + nw, ny + nh);

      // Color by status
      ImU32 fill_color = GetStatusColor(node.status);
      ImU32 border_color = (node.id == selected_node_)
                               ? IM_COL32(255, 255, 100, 255)
                               : IM_COL32(60, 60, 60, 255);

      draw_list->AddRectFilled(node_min, node_max, fill_color, 8.0f * zoom_);
      draw_list->AddRect(node_min, node_max, border_color, 8.0f * zoom_,
                         0, 2.0f * zoom_);

      // Node text
      float font_size = 11.0f * zoom_;
      if (font_size >= 6.0f) {
        // ID label
        draw_list->AddText(nullptr, font_size,
                           ImVec2(nx + 6 * zoom_, ny + 4 * zoom_),
                           IM_COL32(200, 200, 200, 255),
                           node.id.c_str());
        // Name (truncated)
        std::string display_name = node.name;
        if (display_name.length() > 25) {
          display_name = display_name.substr(0, 22) + "...";
        }
        draw_list->AddText(nullptr, font_size,
                           ImVec2(nx + 6 * zoom_, ny + 18 * zoom_),
                           IM_COL32(255, 255, 255, 255),
                           display_name.c_str());
      }

      // Click detection
      if (is_hovered && ImGui::IsMouseClicked(ImGuiMouseButton_Left)) {
        if (mouse_pos.x >= node_min.x && mouse_pos.x <= node_max.x &&
            mouse_pos.y >= node_min.y && mouse_pos.y <= node_max.y) {
          selected_node_ = (selected_node_ == node.id) ? "" : node.id;
        }
      }
    }

    draw_list->PopClipRect();

    // Detail sidebar
    if (!selected_node_.empty() && sidebar_width > 0) {
      ImGui::SameLine();
      ImGui::BeginGroup();
      DrawNodeDetail(graph);
      ImGui::EndGroup();
    }
  }

 private:
  static constexpr float kNodeWidth = 160.0f;
  static constexpr float kNodeHeight = 40.0f;

  static ImU32 GetStatusColor(core::StoryNodeStatus status) {
    switch (status) {
      case core::StoryNodeStatus::kCompleted:
        return IM_COL32(40, 120, 40, 220);
      case core::StoryNodeStatus::kAvailable:
        return IM_COL32(180, 160, 40, 220);
      case core::StoryNodeStatus::kBlocked:
        return IM_COL32(160, 40, 40, 220);
      case core::StoryNodeStatus::kLocked:
      default:
        return IM_COL32(60, 60, 60, 220);
    }
  }

  void DrawNodeDetail(const core::StoryEventGraph& graph) {
    const auto* node = graph.GetNode(selected_node_);
    if (!node) return;

    ImGui::BeginChild("node_detail", ImVec2(240, 0), ImGuiChildFlags_Borders);

    ImGui::TextWrapped("%s", node->name.c_str());
    ImGui::TextDisabled("%s", node->id.c_str());
    ImGui::Separator();

    if (!node->flags.empty()) {
      ImGui::Text("Flags:");
      for (const auto& flag : node->flags) {
        if (!flag.value.empty()) {
          ImGui::BulletText("%s = %s", flag.name.c_str(), flag.value.c_str());
        } else {
          ImGui::BulletText("%s", flag.name.c_str());
        }
      }
    }

    if (!node->locations.empty()) {
      ImGui::Spacing();
      ImGui::Text("Locations:");
      for (const auto& loc : node->locations) {
        ImGui::BulletText("%s", loc.name.c_str());
      }
    }

    if (!node->text_ids.empty()) {
      ImGui::Spacing();
      ImGui::Text("Text IDs:");
      for (const auto& tid : node->text_ids) {
        ImGui::BulletText("%s", tid.c_str());
      }
    }

    if (!node->scripts.empty()) {
      ImGui::Spacing();
      ImGui::Text("Scripts:");
      for (const auto& script : node->scripts) {
        ImGui::BulletText("%s", script.c_str());
      }
    }

    if (!node->notes.empty()) {
      ImGui::Spacing();
      ImGui::TextWrapped("Notes: %s", node->notes.c_str());
    }

    ImGui::EndChild();
  }

  core::HackManifest* manifest_ = nullptr;
  std::string selected_node_;
  float scroll_x_ = 0;
  float scroll_y_ = 0;
  float zoom_ = 1.0f;
};

}  // namespace yaze::editor

#endif  // YAZE_APP_EDITOR_ORACLE_PANELS_STORY_EVENT_GRAPH_PANEL_H
