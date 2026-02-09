#ifndef YAZE_APP_EDITOR_ORACLE_PANELS_STORY_EVENT_GRAPH_PANEL_H
#define YAZE_APP_EDITOR_ORACLE_PANELS_STORY_EVENT_GRAPH_PANEL_H

#include <cmath>
#include <cstdint>
#include <filesystem>
#include <optional>
#include <string>
#include <unordered_map>
#include <vector>

#include "app/editor/core/content_registry.h"
#include "app/editor/events/core_events.h"
#include "app/editor/system/editor_panel.h"
#include "app/gui/core/icons.h"
#include "core/hack_manifest.h"
#include "core/oracle_progression_loader.h"
#include "core/project.h"
#include "core/story_event_graph.h"
#include "core/story_event_graph_query.h"
#include "imgui/imgui.h"
#include "imgui/misc/cpp/imgui_stdlib.h"
#include "util/file_util.h"

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

    ImGui::SameLine();
    if (ImGui::SmallButton("Import .srm...")) {
      ImportOracleSramFromFileDialog();
    }
    ImGui::SameLine();
    if (ImGui::SmallButton("Clear SRAM")) {
      ClearOracleSramState();
    }
    if (!loaded_srm_path_.empty()) {
      const std::filesystem::path p(loaded_srm_path_);
      ImGui::SameLine();
      ImGui::TextDisabled("SRM: %s", p.filename().string().c_str());
      if (ImGui::IsItemHovered()) {
        ImGui::SetTooltip("%s", loaded_srm_path_.c_str());
      }
    }

    if (!last_srm_error_.empty()) {
      ImGui::TextColored(ImVec4(1.0f, 0.35f, 0.35f, 1.0f), "SRM error: %s",
                         last_srm_error_.c_str());
    }

    ImGui::Separator();

    DrawFilterControls(graph);
    UpdateFilterCache(graph);

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
      if (hide_non_matching_) {
        if (!IsNodeVisible(edge.from) || !IsNodeVisible(edge.to)) continue;
      }

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
      if (hide_non_matching_ && !IsNodeVisible(node.id)) {
        continue;
      }

      float nx = cx + node.pos_x * zoom_ - kNodeWidth * zoom_ * 0.5f;
      float ny = cy + node.pos_y * zoom_ - kNodeHeight * zoom_ * 0.5f;
      float nw = kNodeWidth * zoom_;
      float nh = kNodeHeight * zoom_;

      ImVec2 node_min(nx, ny);
      ImVec2 node_max(nx + nw, ny + nh);

      // Color by status
      ImU32 fill_color = GetStatusColor(node.status);
      const bool selected = (node.id == selected_node_);
      const bool query_match = (HasNonEmptyQuery() && IsNodeQueryMatch(node.id));
      ImU32 border_color = selected ? IM_COL32(255, 255, 100, 255)
                                    : (query_match ? IM_COL32(220, 220, 220, 255)
                                                   : IM_COL32(60, 60, 60, 255));

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
      for (size_t i = 0; i < node->locations.size(); ++i) {
        const auto& loc = node->locations[i];
        ImGui::PushID(static_cast<int>(i));

        ImGui::BulletText("%s", loc.name.c_str());

        if (auto room_id = ParseIntLoose(loc.room_id)) {
          ImGui::SameLine();
          if (ImGui::SmallButton("Room")) {
            PublishJumpToRoom(*room_id);
          }
        }
        if (auto map_id = ParseIntLoose(loc.overworld_id)) {
          ImGui::SameLine();
          if (ImGui::SmallButton("Map")) {
            PublishJumpToMap(*map_id);
          }
        }

        if (!loc.room_id.empty() || !loc.overworld_id.empty() ||
            !loc.entrance_id.empty()) {
          ImGui::TextDisabled("room=%s  map=%s  entrance=%s",
                              loc.room_id.empty() ? "-" : loc.room_id.c_str(),
                              loc.overworld_id.empty() ? "-" : loc.overworld_id.c_str(),
                              loc.entrance_id.empty() ? "-" : loc.entrance_id.c_str());
        }

        ImGui::PopID();
      }
    }

    if (!node->text_ids.empty()) {
      ImGui::Spacing();
      ImGui::Text("Text IDs:");
      for (size_t i = 0; i < node->text_ids.size(); ++i) {
        const auto& tid = node->text_ids[i];
        ImGui::PushID(static_cast<int>(i));

        ImGui::BulletText("%s", tid.c_str());
        ImGui::SameLine();
        if (ImGui::SmallButton("Open")) {
          if (auto msg_id = ParseIntLoose(tid)) {
            PublishJumpToMessage(*msg_id);
          }
        }
        ImGui::SameLine();
        if (ImGui::SmallButton("Copy")) {
          ImGui::SetClipboardText(tid.c_str());
        }

        ImGui::PopID();
      }
    }

    if (!node->scripts.empty()) {
      ImGui::Spacing();
      ImGui::Text("Scripts:");
      for (size_t i = 0; i < node->scripts.size(); ++i) {
        const auto& script = node->scripts[i];
        ImGui::PushID(static_cast<int>(i));
        ImGui::BulletText("%s", script.c_str());
        ImGui::SameLine();
        if (ImGui::SmallButton("Copy")) {
          ImGui::SetClipboardText(script.c_str());
        }
        ImGui::PopID();
      }
    }

    if (!node->notes.empty()) {
      ImGui::Spacing();
      ImGui::TextWrapped("Notes: %s", node->notes.c_str());
    }

    ImGui::EndChild();
  }

  static std::optional<int> ParseIntLoose(const std::string& input) {
    // Trim whitespace.
    size_t start = input.find_first_not_of(" \t\r\n");
    if (start == std::string::npos) return std::nullopt;
    size_t end = input.find_last_not_of(" \t\r\n");
    std::string trimmed = input.substr(start, end - start + 1);

    try {
      size_t idx = 0;
      int value = std::stoi(trimmed, &idx, /*base=*/0);
      if (idx != trimmed.size()) return std::nullopt;
      return value;
    } catch (...) {
      return std::nullopt;
    }
  }

  void PublishJumpToRoom(int room_id) const {
    if (auto* bus = ContentRegistry::Context::event_bus()) {
      bus->Publish(JumpToRoomRequestEvent::Create(room_id));
    }
  }

  void PublishJumpToMap(int map_id) const {
    if (auto* bus = ContentRegistry::Context::event_bus()) {
      bus->Publish(JumpToMapRequestEvent::Create(map_id));
    }
  }

  void PublishJumpToMessage(int message_id) const {
    if (auto* bus = ContentRegistry::Context::event_bus()) {
      bus->Publish(JumpToMessageRequestEvent::Create(message_id));
    }
  }

  uint64_t ComputeProgressionFingerprint() const {
    if (!manifest_) return 0;
    const auto prog_opt = manifest_->oracle_progression_state();
    if (!prog_opt.has_value()) return 0;

    const auto& s = *prog_opt;
    return static_cast<uint64_t>(s.crystal_bitfield) |
           (static_cast<uint64_t>(s.game_state) << 8) |
           (static_cast<uint64_t>(s.oosprog) << 16) |
           (static_cast<uint64_t>(s.oosprog2) << 24) |
           (static_cast<uint64_t>(s.side_quest) << 32) |
           (static_cast<uint64_t>(s.pendants) << 40);
  }

  void DrawFilterControls(const core::StoryEventGraph& graph) {
    (void)graph;

    ImGui::Text("Filter");
    ImGui::SameLine();
    ImGui::SetNextItemWidth(260.0f);
    if (ImGui::InputTextWithHint("##story_graph_filter",
                                 "Search id/name/text/script/flag/room...",
                                 &filter_query_)) {
      filter_dirty_ = true;
    }
    ImGui::SameLine();
    if (ImGui::SmallButton("Clear")) {
      if (!filter_query_.empty()) {
        filter_query_.clear();
        filter_dirty_ = true;
      }
    }

    ImGui::SameLine();
    if (ImGui::Checkbox("Hide non-matching", &hide_non_matching_)) {
      // Hiding doesn't change matches, but it can invalidate selection.
      filter_dirty_ = true;
    }

    ImGui::SameLine();
    bool toggles_changed = false;
    toggles_changed |= ImGui::Checkbox("Completed", &show_completed_);
    ImGui::SameLine();
    toggles_changed |= ImGui::Checkbox("Available", &show_available_);
    ImGui::SameLine();
    toggles_changed |= ImGui::Checkbox("Locked", &show_locked_);
    ImGui::SameLine();
    toggles_changed |= ImGui::Checkbox("Blocked", &show_blocked_);
    if (toggles_changed) {
      filter_dirty_ = true;
    }
  }

  static uint8_t StatusMask(bool completed, bool available, bool locked,
                            bool blocked) {
    uint8_t mask = 0;
    if (completed) mask |= 1u << 0;
    if (available) mask |= 1u << 1;
    if (locked) mask |= 1u << 2;
    if (blocked) mask |= 1u << 3;
    return mask;
  }

  bool HasNonEmptyQuery() const { return !filter_query_.empty(); }

  bool IsNodeVisible(const std::string& id) const {
    auto it = node_visible_by_id_.find(id);
    return it != node_visible_by_id_.end() ? it->second : true;
  }

  bool IsNodeQueryMatch(const std::string& id) const {
    auto it = node_query_match_by_id_.find(id);
    return it != node_query_match_by_id_.end() ? it->second : false;
  }

  void UpdateFilterCache(const core::StoryEventGraph& graph) {
    const uint8_t status_mask =
        StatusMask(show_completed_, show_available_, show_locked_, show_blocked_);
    const uint64_t progress_fp = ComputeProgressionFingerprint();

    const size_t node_count = graph.nodes().size();
    if (!filter_dirty_ && node_count == last_node_count_ &&
        filter_query_ == last_filter_query_ && status_mask == last_status_mask_ &&
        progress_fp == last_progress_fp_) {
      return;
    }

    last_node_count_ = node_count;
    last_filter_query_ = filter_query_;
    last_status_mask_ = status_mask;
    last_progress_fp_ = progress_fp;
    filter_dirty_ = false;

    core::StoryEventNodeFilter filter;
    filter.query = filter_query_;
    filter.include_completed = show_completed_;
    filter.include_available = show_available_;
    filter.include_locked = show_locked_;
    filter.include_blocked = show_blocked_;

    node_query_match_by_id_.clear();
    node_visible_by_id_.clear();
    node_query_match_by_id_.reserve(node_count);
    node_visible_by_id_.reserve(node_count);

    for (const auto& node : graph.nodes()) {
      const bool query_match = core::StoryEventNodeMatchesQuery(node, filter.query);
      const bool visible =
          query_match && core::StoryNodeStatusAllowed(node.status, filter);
      node_query_match_by_id_[node.id] = query_match;
      node_visible_by_id_[node.id] = visible;
    }

    // If we're hiding nodes and the selection becomes invisible, clear it to
    // avoid a "ghost sidebar" pointing at a filtered-out node.
    if (hide_non_matching_ && !selected_node_.empty() &&
        !IsNodeVisible(selected_node_)) {
      selected_node_.clear();
    }
  }

  void ImportOracleSramFromFileDialog() {
    if (!manifest_) return;

    util::FileDialogOptions options;
    options.filters = {
        {"SRAM (.srm)", "srm"},
        {"All Files", "*"},
    };

    std::string file_path =
        util::FileDialogWrapper::ShowOpenFileDialog(options);
    if (file_path.empty()) {
      return;
    }

    auto state_or = core::LoadOracleProgressionFromSrmFile(file_path);
    if (!state_or.ok()) {
      last_srm_error_ = std::string(state_or.status().message());
      return;
    }

    manifest_->SetOracleProgressionState(*state_or);
    loaded_srm_path_ = file_path;
    last_srm_error_.clear();

    // Status coloring changed; refresh filter cache visibility.
    filter_dirty_ = true;
  }

  void ClearOracleSramState() {
    if (!manifest_) return;
    manifest_->ClearOracleProgressionState();
    loaded_srm_path_.clear();
    last_srm_error_.clear();
    filter_dirty_ = true;
  }

  core::HackManifest* manifest_ = nullptr;
  std::string selected_node_;
  float scroll_x_ = 0;
  float scroll_y_ = 0;
  float zoom_ = 1.0f;

  // Filter state
  std::string filter_query_;
  bool hide_non_matching_ = false;
  bool show_completed_ = true;
  bool show_available_ = true;
  bool show_locked_ = true;
  bool show_blocked_ = true;

  // Filter cache (recomputed only when query/toggles change)
  bool filter_dirty_ = true;
  size_t last_node_count_ = 0;
  std::string last_filter_query_;
  uint8_t last_status_mask_ = 0;
  std::unordered_map<std::string, bool> node_query_match_by_id_;
  std::unordered_map<std::string, bool> node_visible_by_id_;

  // SRAM import state (purely UI; the actual progression state lives in HackManifest).
  std::string loaded_srm_path_;
  std::string last_srm_error_;

  uint64_t last_progress_fp_ = 0;
};

}  // namespace yaze::editor

#endif  // YAZE_APP_EDITOR_ORACLE_PANELS_STORY_EVENT_GRAPH_PANEL_H
