#include "app/editor/dungeon/workspace/dungeon_workbench_content.h"
#include "util/i18n/tr.h"

#include <algorithm>
#include <array>
#include <cctype>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <fstream>
#include <sstream>
#include <utility>
#include <vector>

#include "absl/strings/str_format.h"
#include "app/editor/agent/agent_ui_theme.h"
#include "app/editor/dungeon/dungeon_canvas_viewer.h"
#include "app/editor/dungeon/dungeon_project_labels.h"
#include "app/editor/dungeon/dungeon_room_selector.h"
#include "app/editor/dungeon/dungeon_selection_snapshot.h"
#include "app/editor/dungeon/inspectors/door_editor_content.h"
#include "app/editor/dungeon/inspectors/dungeon_entity_inspector.h"
#include "app/editor/dungeon/selectors/object_selector_content.h"
#include "app/editor/dungeon/ui/window/custom_collision_panel.h"
#include "app/editor/dungeon/ui/window/dungeon_entrances_panel.h"
#include "app/editor/dungeon/ui/window/dungeon_map_panel.h"
#include "app/editor/dungeon/ui/window/item_editor_panel.h"
#include "app/editor/dungeon/ui/window/minecart_track_editor_panel.h"
#include "app/editor/dungeon/ui/window/room_tag_editor_panel.h"
#include "app/editor/dungeon/ui/window/sprite_editor_panel.h"
#include "app/editor/dungeon/ui/window/water_fill_panel.h"
#include "app/editor/dungeon/ui/workbench/dungeon_workbench_chrome.h"
#include "app/editor/dungeon/widgets/dungeon_status_bar.h"
#include "app/editor/dungeon/widgets/dungeon_workbench_toolbar.h"
#include "app/editor/dungeon/workspace/dungeon_pit_damage_view_model.h"
#include "app/editor/dungeon/workspace/dungeon_workbench_inspector_helpers.h"
#include "app/editor/dungeon/workspace/dungeon_workbench_layout.h"
#include "app/editor/dungeon/workspace/room_matrix_content.h"
#include "app/gui/automation/widget_auto_register.h"
#include "app/gui/core/icons.h"
#include "app/gui/core/input.h"
#include "app/gui/core/layout_helpers.h"
#include "app/gui/core/platform_keys.h"
#include "app/gui/core/style_guard.h"
#include "app/gui/core/ui_config.h"
#include "app/gui/widgets/centered_icon_button.h"
#include "app/gui/widgets/themed_widgets.h"
#include "core/features.h"
#include "core/project.h"
#include "imgui/imgui.h"
#include "rom/rom.h"
#include "util/file_util.h"
#include "zelda3/dungeon/door_types.h"
#include "zelda3/dungeon/dungeon_rom_addresses.h"
#include "zelda3/dungeon/object_layer_semantics.h"
#include "zelda3/dungeon/pit_damage_table.h"
#include "zelda3/dungeon/room_entrance.h"
#include "zelda3/dungeon/room_layer_manager.h"
#include "zelda3/dungeon/room_object.h"
#include "zelda3/resource_labels.h"
#include "zelda3/sprite/sprite.h"

namespace yaze::editor {

namespace {

float ClampWorkbenchPaneWidth(float desired_width, float min_width,
                              float max_width) {
  return std::clamp(desired_width, min_width, std::max(min_width, max_width));
}

constexpr float kCompactLeftSidebarMinWidth = 224.0f;
constexpr float kCompactRightSidebarMinWidth = 272.0f;
constexpr float kCompactLeftSidebarScale = 0.72f;
constexpr float kCompactRightSidebarScale = 0.84f;
float GetCompactSidebarWidth(bool right_sidebar, float min_sidebar_width) {
  return std::max(
      right_sidebar ? kCompactRightSidebarMinWidth
                    : kCompactLeftSidebarMinWidth,
      min_sidebar_width * (right_sidebar ? kCompactRightSidebarScale
                                         : kCompactLeftSidebarScale));
}

DungeonWorkbenchTestRect CaptureLastItemRectForTesting() {
  DungeonWorkbenchTestRect rect;
  rect.visible = true;
  const ImVec2 min = ImGui::GetItemRectMin();
  const ImVec2 max = ImGui::GetItemRectMax();
  rect.min_x = min.x;
  rect.min_y = min.y;
  rect.max_x = max.x;
  rect.max_y = max.y;
  return rect;
}

}  // namespace

DungeonWorkbenchToolRequestTarget ResolveDungeonWorkbenchToolRequestTarget(
    bool standalone_window_open) {
  return standalone_window_open
             ? DungeonWorkbenchToolRequestTarget::kStandaloneWindow
             : DungeonWorkbenchToolRequestTarget::kEmbeddedInspector;
}

DungeonWorkbenchResponsiveLayout ResolveDungeonWorkbenchResponsiveLayout(
    float total_width, float min_canvas_width, float min_sidebar_width,
    float splitter_width, bool want_left, bool want_right) {
  DungeonWorkbenchResponsiveLayout result;
  result.show_left = want_left;
  result.show_right = want_right;

  auto required_width = [&](bool left, bool right, bool compact_left,
                            bool compact_right) {
    float required = min_canvas_width;
    required +=
        left ? (compact_left ? GetCompactSidebarWidth(false, min_sidebar_width)
                             : min_sidebar_width)
             : 0.0f;
    required +=
        right ? (compact_right ? GetCompactSidebarWidth(true, min_sidebar_width)
                               : min_sidebar_width)
              : 0.0f;
    if (left) {
      required += splitter_width;
    }
    if (right) {
      required += splitter_width;
    }
    return required;
  };

  if (result.show_left &&
      total_width <
          required_width(result.show_left, result.show_right, false, false)) {
    result.compact_left = true;
  }
  if (result.show_right &&
      total_width < required_width(result.show_left, result.show_right,
                                   result.compact_left, false)) {
    result.compact_right = true;
  }
  // Keep the entrance selector, object browser, and room navigation available
  // when only one pane fits. Toolbar pane toggles still allow either alone.
  if (result.show_right &&
      total_width < required_width(result.show_left, result.show_right,
                                   result.compact_left, result.compact_right)) {
    result.show_right = false;
    result.compact_right = false;
  }
  if (result.show_left &&
      total_width < required_width(result.show_left, result.show_right,
                                   result.compact_left, result.compact_right)) {
    result.show_left = false;
    result.compact_left = false;
  }

  return result;
}

DungeonWorkbenchPaneLayout ResolveDungeonWorkbenchPaneLayout(
    float total_width, float min_canvas_width, float min_sidebar_width,
    float splitter_width, float stored_left_width, float stored_right_width,
    bool want_left, bool want_right) {
  DungeonWorkbenchPaneLayout layout;
  layout.responsive = ResolveDungeonWorkbenchResponsiveLayout(
      total_width, min_canvas_width, min_sidebar_width, splitter_width,
      want_left, want_right);

  const float compact_left_width =
      GetCompactSidebarWidth(false, min_sidebar_width);
  const float compact_right_width =
      GetCompactSidebarWidth(true, min_sidebar_width);
  layout.min_left_width =
      layout.responsive.compact_left ? compact_left_width : min_sidebar_width;
  layout.min_right_width =
      layout.responsive.compact_right ? compact_right_width : min_sidebar_width;

  float left_width = layout.responsive.show_left
                         ? (layout.responsive.compact_left ? compact_left_width
                                                           : stored_left_width)
                         : 0.0f;
  float right_width =
      layout.responsive.show_right
          ? (layout.responsive.compact_right ? compact_right_width
                                             : stored_right_width)
          : 0.0f;

  const float max_left_width =
      total_width - right_width - min_canvas_width -
      (layout.responsive.show_left ? splitter_width : 0.0f) -
      (layout.responsive.show_right ? splitter_width : 0.0f);
  const float max_right_width =
      total_width - left_width - min_canvas_width -
      (layout.responsive.show_left ? splitter_width : 0.0f) -
      (layout.responsive.show_right ? splitter_width : 0.0f);
  if (layout.responsive.show_left) {
    left_width = ClampWorkbenchPaneWidth(
        left_width, layout.min_left_width,
        std::max(layout.min_left_width, max_left_width));
  } else {
    left_width = 0.0f;
  }
  if (layout.responsive.show_right) {
    right_width = ClampWorkbenchPaneWidth(
        right_width, layout.min_right_width,
        std::max(layout.min_right_width, max_right_width));
  } else {
    right_width = 0.0f;
  }

  float center_width = total_width - left_width - right_width;
  if (layout.responsive.show_left) {
    center_width -= splitter_width;
  }
  if (layout.responsive.show_right) {
    center_width -= splitter_width;
  }
  if (center_width < min_canvas_width) {
    float deficit = min_canvas_width - center_width;
    if (layout.responsive.show_left) {
      const float shrink =
          std::min(deficit, left_width - layout.min_left_width);
      left_width -= shrink;
      deficit -= shrink;
    }
    if (deficit > 0.0f && layout.responsive.show_right) {
      const float shrink =
          std::min(deficit, right_width - layout.min_right_width);
      right_width -= shrink;
      deficit -= shrink;
    }
    center_width = std::max(
        1.0f, total_width - left_width - right_width -
                  (layout.responsive.show_left ? splitter_width : 0.0f) -
                  (layout.responsive.show_right ? splitter_width : 0.0f));
  }

  layout.left_width = left_width;
  layout.center_width = center_width;
  layout.right_width = right_width;
  return layout;
}

DungeonWorkbenchContent::DungeonWorkbenchContent(
    DungeonRoomSelector* room_selector, int* current_room_id,
    std::function<void(int)> on_room_selected,
    std::function<void(int, RoomSelectionIntent)> on_room_selected_with_intent,
    std::function<void(int)> on_save_room,
    std::function<void()> on_save_all_rooms,
    std::function<DungeonCanvasViewer*()> get_viewer,
    std::function<DungeonCanvasViewer*(int)> get_compare_viewer,
    std::function<const std::deque<int>&()> get_recent_rooms,
    std::function<void(int)> forget_recent_room,
    std::function<void(bool)> set_workflow_mode, Rom* rom)
    : room_selector_(room_selector),
      current_room_id_(current_room_id),
      on_room_selected_(std::move(on_room_selected)),
      on_room_selected_with_intent_(std::move(on_room_selected_with_intent)),
      on_save_room_(std::move(on_save_room)),
      on_save_all_rooms_(std::move(on_save_all_rooms)),
      get_viewer_(std::move(get_viewer)),
      get_compare_viewer_(std::move(get_compare_viewer)),
      get_recent_rooms_(std::move(get_recent_rooms)),
      forget_recent_room_(std::move(forget_recent_room)),
      set_workflow_mode_(std::move(set_workflow_mode)),
      rom_(rom) {
  proposal_canvas_ = std::make_unique<gui::Canvas>("DungeonProposalCanvas");
  gui::CanvasConfig config;
  config.role = gui::CanvasRole::kPreviewOnly;
  config.enable_context_menu = false;
  config.show_builtin_context_menu = false;
  config.is_draggable = true;
  proposal_canvas_->Init(config);
}

DungeonWorkbenchContent::~DungeonWorkbenchContent() = default;

std::string DungeonWorkbenchContent::GetId() const {
  return "dungeon.workbench";
}
std::string DungeonWorkbenchContent::GetDisplayName() const {
  return "Dungeon Workbench";
}
std::string DungeonWorkbenchContent::GetIcon() const {
  return ICON_MD_WORKSPACES;
}
std::string DungeonWorkbenchContent::GetEditorCategory() const {
  return "Dungeon";
}
int DungeonWorkbenchContent::GetPriority() const {
  return 10;
}

void DungeonWorkbenchContent::SetRom(Rom* rom) {
  rom_ = rom;
  room_dungeon_cache_.clear();
  room_dungeon_cache_built_ = false;
}

void DungeonWorkbenchContent::SetEmbeddedToolPanels(
    RoomTagEditorPanel* room_tags, CustomCollisionPanel* custom_collision,
    WaterFillPanel* water_fill, MinecartTrackEditorPanel* minecart_tracks) {
  room_tag_panel_ = room_tags;
  custom_collision_panel_ = custom_collision;
  water_fill_panel_ = water_fill;
  minecart_track_panel_ = minecart_tracks;
}

void DungeonWorkbenchContent::SetEmbeddedEditorPanels(
    WindowContent* object_selector, WindowContent* door_editor,
    WindowContent* sprite_editor, WindowContent* item_editor,
    WindowContent* room_graphics, WindowContent* palette_editor) {
  object_selector_content_ = object_selector;
  door_editor_content_ = door_editor;
  sprite_editor_content_ = sprite_editor;
  item_editor_content_ = item_editor;
  room_graphics_content_ = room_graphics;
  palette_editor_content_ = palette_editor;
}

void DungeonWorkbenchContent::FocusRoomInspector() {
  layout_state_.show_left_sidebar = true;
  CloseToolInspector();
  sidebar_mode_ = SidebarMode::Room;
}

void DungeonWorkbenchContent::FocusSelectionInspector() {
  layout_state_.show_right_inspector = true;
}

void DungeonWorkbenchContent::FocusRoomMatrix() {
  layout_state_.show_right_inspector = true;
  show_room_matrix_ = true;
}

void DungeonWorkbenchContent::FocusEntranceBrowser() {
  CloseToolInspector();
  sidebar_mode_ = SidebarMode::Entrance;
  layout_state_.show_left_sidebar = true;
}

void DungeonWorkbenchContent::ShowConnectedGraph() {
  layout_state_.show_connected_canvas_view = true;
  split_view_enabled_ = false;
}

void DungeonWorkbenchContent::RequestDungeonMapPopup() {
  open_dungeon_map_popup_ = true;
}

void DungeonWorkbenchContent::RequestProposalPreview() {
  show_proposal_preview_ = true;
  focus_proposal_preview_ = true;
}

bool DungeonWorkbenchContent::LoadProposalOverlay(const std::string& path) {
  std::snprintf(proposal_path_, sizeof(proposal_path_), "%s", path.c_str());
  std::ifstream file(path, std::ios::binary);
  if (!file) {
    proposal_error_ = absl::StrFormat("Cannot open %s", path);
    return false;
  }
  std::stringstream buffer;
  buffer << file.rdbuf();
  auto overlay = ParseDungeonProposalOverlay(buffer.str());
  if (!overlay.ok()) {
    // Keep the previously loaded overlay visible; show why the new one failed.
    proposal_error_ = std::string(overlay.status().message());
    return false;
  }
  proposal_layer_visible_.clear();
  for (const auto& layer : overlay->layers) {
    proposal_layer_visible_.push_back(layer.visible ? 1 : 0);
  }
  proposal_overlay_ = std::move(*overlay);
  loaded_proposal_path_ = path;
  proposal_auto_fit_ = true;
  proposal_error_.clear();
  return true;
}

void DungeonWorkbenchContent::DrawProposalPreviewWindow(
    DungeonCanvasViewer& viewer) {
  if (!show_proposal_preview_) {
    return;
  }
  ImGui::SetNextWindowSize(ImVec2(1120.0f, 700.0f), ImGuiCond_FirstUseEver);
  if (focus_proposal_preview_) {
    ImGui::SetNextWindowFocus();
    focus_proposal_preview_ = false;
  }
  if (!ImGui::Begin(ICON_MD_PREVIEW " Proposal Preview##DungeonProposalPreview",
                    &show_proposal_preview_)) {
    ImGui::End();
    return;
  }

  // File row.
  util::FileDialogOptions json_options;
  json_options.filters.push_back({"Proposal Overlay", "json"});
  json_options.filters.push_back({"All Files", "*"});
  if (ImGui::Button(ICON_MD_FOLDER_OPEN " Open...")) {
    const std::string path =
        util::FileDialogWrapper::ShowOpenFileDialog(json_options);
    if (!path.empty()) {
      (void)LoadProposalOverlay(path);
    }
  }
  ImGui::SameLine();
  ImGui::BeginDisabled(proposal_path_[0] == '\0');
  if (ImGui::Button(ICON_MD_REFRESH " Reload")) {
    (void)LoadProposalOverlay(proposal_path_);
  }
  ImGui::EndDisabled();
  ImGui::SameLine();
  ImGui::SetNextItemWidth(-1.0f);
  if (ImGui::InputTextWithHint("##ProposalPath", tr("overlay .json path"),
                               proposal_path_, sizeof(proposal_path_),
                               ImGuiInputTextFlags_EnterReturnsTrue)) {
    (void)LoadProposalOverlay(proposal_path_);
  }
  if (!proposal_error_.empty()) {
    gui::StyleColorGuard error_color(ImGuiCol_Text,
                                     ImVec4(1.0f, 0.45f, 0.4f, 1.0f));
    ImGui::TextWrapped("%s", proposal_error_.c_str());
    if (!loaded_proposal_path_.empty())
      ImGui::TextWrapped(tr("Still showing: %s"),
                         loaded_proposal_path_.c_str());
  }

  if (!proposal_overlay_.has_value()) {
    ImGui::Spacing();
    ImGui::TextWrapped(
        "%s",
        tr("Open a proposal overlay (.json) to render its rooms from the "
           "loaded ROM with the proposed changes drawn on top. This window "
           "is read-only: it never edits rooms or the ROM."));
    ImGui::End();
    return;
  }
  const DungeonProposalOverlay& overlay = *proposal_overlay_;

  if (!overlay.title.empty()) {
    ImGui::TextUnformatted(overlay.title.c_str());
  }
  if (!overlay.status.empty()) {
    ImGui::SameLine();
    ImGui::TextDisabled("(%s)", overlay.status.c_str());
  }
  ImGui::TextDisabled("%s",
                      tr("Read-only preview. Nothing here edits the ROM."));

  // Comparison changes annotations only; both views share the same room image
  // and dedicated navigation state.
  if (ImGui::RadioButton(tr("Room"), !proposal_show_overlay_))
    proposal_show_overlay_ = false;
  ImGui::SameLine();
  if (ImGui::RadioButton(tr("Room + proposal"), proposal_show_overlay_))
    proposal_show_overlay_ = true;
  ImGui::SameLine();
  if (ImGui::Button(tr("Fit")))
    proposal_auto_fit_ = true;
  float scale = proposal_canvas_->global_scale();
  ImGui::SetNextItemWidth(150.0f);
  if (ImGui::SliderFloat(tr("Zoom"), &scale, 0.25f, 4.0f, "%.2fx")) {
    proposal_canvas_->set_global_scale(scale);
    proposal_auto_fit_ = false;
  }
  ImGui::Checkbox(tr("Grid"), &proposal_show_grid_);
  ImGui::SameLine();
  ImGui::BeginDisabled(!proposal_show_grid_);
  if (ImGui::RadioButton(tr("8 px"), proposal_grid_step_ == 8))
    proposal_grid_step_ = 8;
  ImGui::SameLine();
  if (ImGui::RadioButton(tr("16 px"), proposal_grid_step_ == 16))
    proposal_grid_step_ = 16;
  ImGui::EndDisabled();
  ImGui::TextDisabled("%s", tr("Wheel: zoom  |  Right/middle drag: pan"));

  if (ImGui::CollapsingHeader(tr("Proposal layers"))) {
    ImGui::BeginDisabled(!proposal_show_overlay_);
    for (size_t i = 0; i < overlay.layers.size(); ++i) {
      const auto& layer = overlay.layers[i];
      // Each layer starts a row, avoiding controls beyond a narrow window edge.
      const uint32_t c = layer.color_rgba;
      ImGui::ColorButton(
          absl::StrFormat("##ProposalLayerSwatch%d", i).c_str(),
          ImVec4(((c >> 24) & 0xFF) / 255.0f, ((c >> 16) & 0xFF) / 255.0f,
                 ((c >> 8) & 0xFF) / 255.0f, 1.0f),
          ImGuiColorEditFlags_NoTooltip, ImVec2(12, 12));
      ImGui::SameLine(0, 4);
      bool visible =
          i < proposal_layer_visible_.size() && proposal_layer_visible_[i] != 0;
      if (ImGui::Checkbox(
              absl::StrFormat("%s##ProposalLayer%d", layer.label, i).c_str(),
              &visible) &&
          i < proposal_layer_visible_.size()) {
        proposal_layer_visible_[i] = visible ? 1 : 0;
      }
    }
    ImGui::EndDisabled();
  }

  if (on_room_selected_) {
    for (const auto& room : overlay.rooms) {
      if (ImGui::SmallButton(
              absl::StrFormat("Open $%02X##ProposalOpenRoom", room.room_id)
                  .c_str())) {
        on_room_selected_(room.room_id);
      }
    }
  }

  const bool show_notes =
      !overlay.notes.empty() && ImGui::CollapsingHeader(tr("Proposal notes"));
  // Rooms with the overlay. Expanded notes retain a bounded area below.
  const float notes_height =
      !show_notes ? 0.0f
                  : std::min(140.0f, ImGui::GetTextLineHeightWithSpacing() *
                                         (overlay.notes.size() + 1.5f));
  if (ImGui::BeginChild(
          "##ProposalPreviewBody", ImVec2(0.0f, -notes_height), true,
          ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse)) {
    viewer.DrawProposalPreview(overlay, proposal_layer_visible_,
                               proposal_show_overlay_, *proposal_canvas_,
                               proposal_show_grid_, proposal_grid_step_,
                               proposal_auto_fit_);
  }
  ImGui::EndChild();

  if (show_notes &&
      ImGui::BeginChild("##ProposalPreviewNotes", ImVec2(0.0f, 0.0f), false)) {
    for (const auto& note : overlay.notes) {
      ImGui::BulletText("%s", note.c_str());
    }
  }
  if (show_notes) {
    ImGui::EndChild();
  }
  ImGui::End();
}

void DungeonWorkbenchContent::OpenObjectSelectorTool() {
  OpenTool(WorkbenchTool::ObjectSelector);
}

void DungeonWorkbenchContent::OpenDoorTool() {
  OpenTool(WorkbenchTool::DoorEditor);
}

void DungeonWorkbenchContent::OpenSpriteTool() {
  OpenTool(WorkbenchTool::SpriteEditor);
}

void DungeonWorkbenchContent::OpenItemTool() {
  OpenTool(WorkbenchTool::ItemEditor);
}

void DungeonWorkbenchContent::OpenRoomGraphicsTool() {
  OpenTool(WorkbenchTool::RoomGraphics);
}

void DungeonWorkbenchContent::OpenPaletteTool() {
  OpenTool(WorkbenchTool::Palette);
}

void DungeonWorkbenchContent::OpenRoomTagsTool() {
  OpenTool(WorkbenchTool::RoomTags);
}

void DungeonWorkbenchContent::OpenCustomCollisionTool() {
  OpenTool(WorkbenchTool::CustomCollision);
}

void DungeonWorkbenchContent::OpenWaterFillTool() {
  OpenTool(WorkbenchTool::WaterFill);
}

void DungeonWorkbenchContent::OpenMinecartTool() {
  OpenTool(WorkbenchTool::MinecartTracks);
}

void DungeonWorkbenchContent::OpenObjectCoverageTool() {
  OpenTool(WorkbenchTool::ObjectCoverage);
}

bool DungeonWorkbenchContent::IsToolInspectorActiveForTesting() const {
  return inspector_mode_ == InspectorMode::Tools;
}

const char* DungeonWorkbenchContent::GetInspectorModeIdForTesting() const {
  switch (inspector_mode_) {
    case InspectorMode::Inspect:
      return "inspect";
    case InspectorMode::Tools:
      return "tools";
  }
  return "unknown";
}

const char* DungeonWorkbenchContent::GetActiveToolIdForTesting() const {
  return GetWorkbenchToolId(active_tool_);
}

const char* DungeonWorkbenchContent::GetSidebarModeIdForTesting() const {
  switch (sidebar_mode_) {
    case SidebarMode::Room:
      return "room";
    case SidebarMode::Objects:
      return "objects";
    case SidebarMode::Sprites:
      return "sprites";
    case SidebarMode::Doors:
      return "doors";
    case SidebarMode::Items:
      return "items";
    case SidebarMode::Entrance:
      return "entrance";
    case SidebarMode::Tools:
      return GetWorkbenchToolId(active_tool_);
  }
  return "unknown";
}

void DungeonWorkbenchContent::DrawSidebarPane(float width, float height,
                                              float button_size, bool compact) {
  const bool open = ImGui::BeginChild(
      "##DungeonWorkbenchSidebar", ImVec2(width, height), false,
      ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse);
  if (open) {
    DrawSidebarHeader(button_size, compact);
    const float rail_width = ImGui::GetFrameHeight() + 8.0f;
    if (ImGui::BeginChild("##WorkbenchRail", ImVec2(rail_width, 0), false))
      DrawSidebarModeTabs();
    ImGui::EndChild();
    ImGui::SameLine();
    if (ImGui::BeginChild("##NavigatorTools", ImVec2(0, 0), false))
      DrawSidebarContent();
    ImGui::EndChild();
  }
  ImGui::EndChild();
}

void DungeonWorkbenchContent::DrawSidebarHeader(float button_size,
                                                bool compact) {
  const char* label = "Room";
  switch (sidebar_mode_) {
    case SidebarMode::Room:
      break;
    case SidebarMode::Objects:
      label = "Objects";
      break;
    case SidebarMode::Sprites:
      label = "Sprites";
      break;
    case SidebarMode::Doors:
      label = "Doors";
      break;
    case SidebarMode::Items:
      label = "Items";
      break;
    case SidebarMode::Entrance:
      label = "Entrance";
      break;
    case SidebarMode::Tools:
      label = GetWorkbenchToolShortLabel(active_tool_);
      break;
  }
  const float action_width =
      workbench::CalcIconButtonWidth(ICON_MD_MORE_HORIZ, button_size) +
      workbench::CalcIconButtonWidth(ICON_MD_CHEVRON_LEFT, button_size) +
      ImGui::GetStyle().ItemSpacing.x;
  workbench::DrawPaneHeader(
      "##DungeonWorkbenchSidebarHeader", "", label, label, nullptr, compact,
      action_width, [&]() {
        if (workbench::DrawHeaderIconAction("SidebarQuickActions",
                                            ICON_MD_MORE_HORIZ, button_size,
                                            "Room tools"))
          ImGui::OpenPopup("##WorkbenchSidebarQuickActions");
        if (ImGui::BeginPopup("##WorkbenchSidebarQuickActions")) {
          if (ImGui::MenuItem("Dungeon overview"))
            ShowConnectedGraph();
          if (ImGui::MenuItem("Dungeon map"))
            RequestDungeonMapPopup();
          if (ImGui::MenuItem("Proposal Preview..."))
            RequestProposalPreview();
          ImGui::Separator();
          if (ImGui::MenuItem("Keyboard Shortcuts", nullptr, false,
                              static_cast<bool>(open_keyboard_shortcuts_)))
            open_keyboard_shortcuts_();
          ImGui::EndPopup();
        }
        ImGui::SameLine();
        if (workbench::DrawHeaderIconAction("CollapseRooms",
                                            ICON_MD_CHEVRON_LEFT, button_size,
                                            "Collapse browser"))
          layout_state_.show_left_sidebar = false;
      });
}

void DungeonWorkbenchContent::DrawSidebarModeTabs() {
  gui::AutoWidgetScope scope("Dungeon/WorkbenchRail");
  const float size = ImGui::GetFrameHeight() + 4.0f;
  const auto draw = [&](const char* id, const char* icon, const char* label,
                        bool selected, auto action) {
    gui::StyleColorGuard color(
        ImGuiCol_Button,
        ImGui::GetStyleColorVec4(selected ? ImGuiCol_ButtonActive
                                          : ImGuiCol_Button));
    ImGui::PushID(id);
    ImGui::SetCursorPosX(
        ImGui::GetCursorPosX() +
        std::max(0.0f, (ImGui::GetContentRegionAvail().x - size) * 0.5f));
    if (gui::CenteredIconButton(icon, size))
      action();
    gui::AutoRegisterLastItem("button", id, label);
    if (selected) {
      const ImVec2 p = ImGui::GetItemRectMin();
      ImGui::GetWindowDrawList()->AddLine(
          p, ImVec2(p.x, p.y + size), ImGui::GetColorU32(ImGuiCol_CheckMark),
          2.0f);
    }
    if (ImGui::IsItemHovered())
      ImGui::SetTooltip("%s", label);
    ImGui::PopID();
  };
  draw("room", ICON_MD_TUNE, "Room", sidebar_mode_ == SidebarMode::Room,
       [this]() { FocusRoomInspector(); });
  struct Choice {
    const char* id;
    const char* icon;
    const char* label;
    WorkbenchTool tool;
    SidebarMode mode;
  };
  constexpr Choice choices[] = {
      {"objects", ICON_MD_WIDGETS, "Objects", WorkbenchTool::ObjectSelector,
       SidebarMode::Objects},
      {"sprites", ICON_MD_PEST_CONTROL, "Sprites", WorkbenchTool::SpriteEditor,
       SidebarMode::Sprites},
      {"doors", ICON_MD_DOOR_FRONT, "Doors", WorkbenchTool::DoorEditor,
       SidebarMode::Doors},
      {"items", ICON_MD_INVENTORY_2, "Items", WorkbenchTool::ItemEditor,
       SidebarMode::Items},
  };
  for (const auto& choice : choices)
    draw(choice.id, choice.icon, choice.label, sidebar_mode_ == choice.mode,
         [this, &choice]() { OpenTool(choice.tool); });
  ImGui::Separator();
  draw("entrance", ICON_MD_LOGIN, "Entrance",
       sidebar_mode_ == SidebarMode::Entrance,
       [this]() { FocusEntranceBrowser(); });
  constexpr Choice tools[] = {
      {"palette", ICON_MD_PALETTE, "Palette", WorkbenchTool::Palette,
       SidebarMode::Tools},
      {"minecart", ICON_MD_TRAIN, "Tracks", WorkbenchTool::MinecartTracks,
       SidebarMode::Tools},
      {"custom_collision", ICON_MD_GRID_ON, "Collision",
       WorkbenchTool::CustomCollision, SidebarMode::Tools},
  };
  for (const auto& choice : tools)
    draw(choice.id, choice.icon, choice.label,
         sidebar_mode_ == SidebarMode::Tools && active_tool_ == choice.tool,
         [this, &choice]() { OpenTool(choice.tool); });
}

void DungeonWorkbenchContent::DrawSidebarContent() {
  DungeonCanvasViewer* viewer = get_viewer_ ? get_viewer_() : nullptr;
  if (!viewer) {
    ImGui::TextDisabled("No active room");
    return;
  }
  ImGui::PushID("WorkbenchSidebarMode");
  if (sidebar_mode_ == SidebarMode::Room) {
    DrawInspectorShelfRoom(*viewer);
  } else if (sidebar_mode_ == SidebarMode::Entrance) {
    if (!entrance_navigator_content_) {
      ImGui::TextDisabled("Entrance editor unavailable");
    } else if (is_standalone_tool_open_ &&
               is_standalone_tool_open_(entrance_navigator_content_->GetId())) {
      if (ImGui::Button("Focus Entrance Window") &&
          open_and_focus_standalone_tool_)
        open_and_focus_standalone_tool_(entrance_navigator_content_->GetId());
    } else {
      entrance_navigator_content_->DrawGraphicsContext(
          ImGui::GetContentRegionAvail().x);
      ImGui::Separator();
      ImGui::BeginDisabled(viewer->header_read_only() ||
                           !viewer->IsObjectInteractionEnabled());
      entrance_navigator_content_->DrawPropertiesInWorkbench();
      ImGui::EndDisabled();
    }
  } else if (sidebar_mode_ == SidebarMode::Tools) {
    DrawInspectorToolPanel(*viewer);
  } else {
    WindowContent* content = nullptr;
    switch (sidebar_mode_) {
      case SidebarMode::Objects:
        content = object_selector_content_;
        break;
      case SidebarMode::Sprites:
        content = sprite_editor_content_;
        break;
      case SidebarMode::Doors:
        content = door_editor_content_;
        break;
      case SidebarMode::Items:
        content = item_editor_content_;
        break;
      default:
        break;
    }
    if (!content) {
      ImGui::TextDisabled("Browser unavailable");
    } else if (is_standalone_tool_open_ &&
               is_standalone_tool_open_(content->GetId())) {
      if (ImGui::Button("Focus Window") && open_and_focus_standalone_tool_)
        open_and_focus_standalone_tool_(content->GetId());
    } else if (auto* selector = dynamic_cast<ObjectSelectorContent*>(content)) {
      selector->DrawInWorkbench();
    } else if (auto* sprites = dynamic_cast<SpriteEditorPanel*>(content)) {
      sprites->DrawInWorkbench();
    } else if (auto* doors = dynamic_cast<DoorEditorContent*>(content)) {
      doors->DrawInWorkbench();
    } else if (auto* items = dynamic_cast<ItemEditorPanel*>(content)) {
      items->DrawInWorkbench();
    } else {
      content->Draw(nullptr);
    }
  }
  ImGui::PopID();
}

void DungeonWorkbenchContent::Draw(bool* p_open) {
  (void)p_open;
  const auto& theme = AgentUI::GetTheme();

  if (!rom_ || !rom_->is_loaded()) {
    ImGui::TextDisabled(ICON_MD_INFO " Load a ROM to edit dungeon rooms.");
    return;
  }
  if (!room_selector_ || !current_room_id_ || !get_viewer_) {
    ImGui::TextColored(theme.text_error_red, tr("Dungeon Workbench not wired"));
    return;
  }

  DungeonCanvasViewer* primary_viewer = get_viewer_ ? get_viewer_() : nullptr;
  DungeonCanvasViewer* compare_viewer =
      get_compare_viewer_ ? get_compare_viewer_(compare_room_id_) : nullptr;
  const float splitter_w = gui::UIConfig::kSplitterWidth;
  const float total_w = std::max(ImGui::GetContentRegionAvail().x, 1.0f);
  // Relaxed from 320 px to 260 px so the Inspect pane can breathe at widths
  // where the previous floor forced a too-wide right rail. Room browser on the
  // left also benefits since its matrix cells scale down gracefully.
  const float min_sidebar_w =
      std::max(gui::UIConfig::kContentMinWidthSidebar, 260.0f);
  const float min_canvas_w = std::max(420.0f, min_sidebar_w + 96.0f);
  if (current_room_id_) {
    DungeonWorkbenchToolbarParams params;
    params.layout = &layout_state_;
    params.current_room_id = current_room_id_;
    params.previous_room_id = &previous_room_id_;
    params.split_view_enabled = &split_view_enabled_;
    params.compare_room_id = &compare_room_id_;
    params.primary_viewer = primary_viewer;
    params.compare_viewer = compare_viewer;
    params.on_room_selected = on_room_selected_;
    params.get_recent_rooms = get_recent_rooms_;
    if (on_room_selected_with_intent_) {
      params.on_open_room_panel = [this](int room_id) {
        on_room_selected_with_intent_(room_id,
                                      RoomSelectionIntent::kOpenStandalone);
      };
    }
    params.forget_recent_room = forget_recent_room_;
    params.set_workflow_mode = set_workflow_mode_;
    params.on_save_room = on_save_room_;
    params.draw_tools = [this]() {
      DrawInspectorToolPicker();
    };
    params.draw_save_content = [this]() {
      DrawApplyScopeControls(current_room_id_ ? *current_room_id_ : -1);
    };
    params.draw_layout_options = [this]() {
      DrawLayoutOptions();
    };
    params.minecart_mode = sidebar_mode_ == SidebarMode::Tools &&
                           active_tool_ == WorkbenchTool::MinecartTracks;
    if (minecart_track_panel_) {
      params.on_minecart_mode = [this]() {
        OpenMinecartTool();
      };
    }
    params.on_request_dungeon_map = [this]() {
      RequestDungeonMapPopup();
    };
    params.compare_search_buf = compare_search_buf_;
    params.compare_search_buf_size = sizeof(compare_search_buf_);
    params.draw_entrance_context = [this](float width) {
      if (entrance_navigator_content_)
        entrance_navigator_content_->DrawGraphicsContext(width);
    };
    const bool request_panel_workflow = DungeonWorkbenchToolbar::Draw(params);
    if (request_panel_workflow && set_workflow_mode_) {
      // Defer panel visibility mutation until toolbar child/table scopes closed.
      set_workflow_mode_(false);
      return;
    }
  }

  if (primary_viewer && layout_state_.show_room_context_row) {
    DrawRoomContextRow(*primary_viewer);
  }

  // Display controls can change widths or visibility in this frame.
  const DungeonWorkbenchPaneLayout pane_layout =
      ResolveDungeonWorkbenchPaneLayout(
          total_w, min_canvas_w, min_sidebar_w, splitter_w,
          layout_state_.left_width, layout_state_.right_width,
          layout_state_.show_left_sidebar, layout_state_.show_right_inspector);
  const bool show_left = pane_layout.responsive.show_left;
  const bool show_right = pane_layout.responsive.show_right;

  const float btn = gui::LayoutHelpers::GetTouchSafeWidgetHeight();
  const float total_h = std::max(ImGui::GetContentRegionAvail().y, 1.0f);
  const float left_w = pane_layout.left_width;
  const float right_w = pane_layout.right_width;
  const float center_w = pane_layout.center_width;
  if (show_left && !pane_layout.responsive.compact_left) {
    layout_state_.left_width = left_w;
  }
  if (show_right && !pane_layout.responsive.compact_right) {
    layout_state_.right_width = right_w;
  }

  if (show_left) {
    DrawSidebarPane(left_w, total_h, btn, pane_layout.responsive.compact_left);
    ImGui::SameLine(0, 0);
    if (DrawDungeonWorkbenchVerticalSplitter(
            "##DungeonWorkbenchLeftSplitter", total_h,
            &layout_state_.left_width, pane_layout.min_left_width,
            total_w - right_w - min_canvas_w - (show_right ? splitter_w : 0),
            false))
      layout_state_.show_left_sidebar = false;
    ImGui::SameLine(0, 0);
  }
  DrawCanvasPane(center_w, total_h, primary_viewer);
  if (show_right) {
    ImGui::SameLine(0, 0);
    if (DrawDungeonWorkbenchVerticalSplitter(
            "##DungeonWorkbenchRightSplitter", total_h,
            &layout_state_.right_width, pane_layout.min_right_width,
            total_w - left_w - min_canvas_w - (show_left ? splitter_w : 0),
            true))
      layout_state_.show_right_inspector = false;
    ImGui::SameLine(0, 0);
    DrawInspectorPane(right_w, total_h, btn,
                      pane_layout.responsive.compact_right, primary_viewer);
  }
  if (primary_viewer) {
    DrawDungeonMapPopup(*primary_viewer);
    primary_viewer->DrawChestEditorPopup();
  }
}

void DungeonWorkbenchContent::DrawLayoutOptions() {
  gui::AutoWidgetScope automation_scope("Dungeon/WorkbenchLayout");
  ImGui::TextUnformatted(tr("Workbench layout"));
  ImGui::Checkbox(tr("Room properties above panes"),
                  &layout_state_.show_room_context_row);
  ImGui::Checkbox(tr("Nearby rooms in spare canvas space"),
                  &layout_state_.show_nearby_rooms);
  ImGui::SetNextItemWidth(ImGui::GetFontSize() * 16.0f);
  ImGui::SliderFloat(tr("Browser width"), &layout_state_.left_width, 224.0f,
                     600.0f, "%.0f px");
  gui::AutoRegisterLastItem("slider", "Browser width");
  ImGui::SetNextItemWidth(ImGui::GetFontSize() * 16.0f);
  ImGui::SliderFloat(tr("Inspector width"), &layout_state_.right_width, 272.0f,
                     600.0f, "%.0f px");
  gui::AutoRegisterLastItem("slider", "Inspector width");
  ImGui::SetNextItemWidth(ImGui::GetFontSize() * 16.0f);
  ImGui::SliderFloat(tr("Selection / matrix split"), &navigator_tools_fraction_,
                     0.20f, 0.65f, "%.2f");
  ImGui::SetNextItemWidth(ImGui::GetFontSize() * 16.0f);
  ImGui::SliderFloat(tr("Nearby preview height"),
                     &layout_state_.nearby_rooms_height, 80.0f, 180.0f,
                     "%.0f px");
  if (ImGui::Button(tr("Reset workbench layout"))) {
    layout_state_ = {};
    navigator_tools_fraction_ = 0.30f;
  }
  ImGui::TextDisabled(tr("Layout changes apply immediately for this session."));
}

void DungeonWorkbenchContent::DrawCanvasPane(
    float width, float height, DungeonCanvasViewer* primary_viewer) {
  const bool canvas_open = ImGui::BeginChild(
      "##DungeonWorkbenchCanvas", ImVec2(width, height), false,
      ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse);
  if (canvas_open) {
    if (primary_viewer) {
      // Reserve a fixed strip at the bottom for the status bar so neither
      // single-room, split, nor connected-mode rendering can run past it and
      // clip against the outer window chrome. Inner scrolling (wheel zoom in
      // connected mode, canvas pan in single-room) happens inside this body
      // child — the outer ##DungeonWorkbenchCanvas never needs a scrollbar.
      const float status_bar_reserved =
          std::max(
              ImGui::GetFontSize() + ImGui::GetStyle().FramePadding.y * 2.0f,
              gui::UIConfig::kStatusBarHeight) +
          ImGui::GetStyle().ItemSpacing.y;

      const bool connected_mode = layout_state_.show_connected_canvas_view;
      const ImVec2 body_available = ImGui::GetContentRegionAvail();
      const float nearby_height =
          layout_state_.show_nearby_rooms && !connected_mode &&
                  !split_view_enabled_
              ? ResolveDungeonWorkbenchNearbyHeight(
                    body_available.x - ImGui::GetStyle().WindowPadding.x * 2,
                    body_available.y - status_bar_reserved -
                        ImGui::GetStyle().WindowPadding.y * 2,
                    layout_state_.nearby_rooms_height,
                    ImGui::GetStyle().ItemSpacing.y)
              : 0.0f;
      const float nearby_reserved =
          nearby_height > 0.0f ? nearby_height + ImGui::GetStyle().ItemSpacing.y
                               : 0.0f;
      ImGuiWindowFlags body_flags =
          ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse;
      if (connected_mode) {
        // Body child owns the connected-mode scroll container; matrix no
        // longer nests its own BeginChild for scrolling.
        const ImVec2 content_size =
            primary_viewer->GetConnectedContentSize(*current_room_id_);
        const float scale = primary_viewer->ConnectedCanvasScale();
        ImGui::SetNextWindowContentSize(
            ImVec2(content_size.x * scale, content_size.y * scale));
        body_flags = ImGuiWindowFlags_HorizontalScrollbar |
                     ImGuiWindowFlags_NoScrollWithMouse;
      }
      const bool body_open = ImGui::BeginChild(
          "##DungeonCanvasBody",
          ImVec2(0.0f, -status_bar_reserved - nearby_reserved), false,
          body_flags);
      if (body_open) {
        if (connected_mode) {
          split_view_enabled_ = false;
          if (primary_viewer->DrawConnectedRoomMatrix(*current_room_id_)
                  .has_value()) {
            layout_state_.show_connected_canvas_view = false;
          }
        } else if (split_view_enabled_) {
          DrawSplitView(*primary_viewer);
        } else {
          primary_viewer->DrawDungeonCanvas(*current_room_id_);
        }
        if (on_primary_canvas_drawn_) {
          on_primary_canvas_drawn_(*primary_viewer);
        }
      }
      ImGui::EndChild();

      if (nearby_height > 0.0f) {
        const float preview_y = ImGui::GetCursorPosY();
        primary_viewer->DrawNearbyRoomPreviews(*current_room_id_,
                                               nearby_height);
        if (ImGui::GetCursorPosY() == preview_y) {
          ImGui::Dummy(ImVec2(0.0f, nearby_height));
        }
      }

      const char* tool_mode =
          primary_viewer->object_interaction().mode_manager().GetModeName();
      bool room_dirty = false;
      if (auto* rooms = primary_viewer->rooms();
          rooms && current_room_id_ && *current_room_id_ >= 0) {
        if (const auto* room = rooms->GetIfMaterialized(*current_room_id_)) {
          room_dirty = room->HasUnsavedChanges();
        }
      }
      auto status =
          DungeonStatusBar::BuildState(*primary_viewer, tool_mode, room_dirty);
      status.workflow_mode = layout_state_.show_connected_canvas_view
                                 ? workflow_mode_names::kConnected
                                 : nullptr;
      status.workflow_primary = true;
      if (can_undo_)
        status.can_undo = can_undo_();
      if (can_redo_)
        status.can_redo = can_redo_();
      if (undo_desc_) {
        static std::string s_undo_desc;
        s_undo_desc = undo_desc_();
        status.undo_desc = s_undo_desc.empty() ? nullptr : s_undo_desc.c_str();
      }
      if (redo_desc_) {
        static std::string s_redo_desc;
        s_redo_desc = redo_desc_();
        status.redo_desc = s_redo_desc.empty() ? nullptr : s_redo_desc.c_str();
      }
      if (undo_depth_)
        status.undo_depth = undo_depth_();
      status.on_undo = on_undo_;
      status.on_redo = on_redo_;
      if (!layout_state_.show_connected_canvas_view) {
        status.on_selection = [this]() {
          FocusSelectionInspector();
        };
      }
      DungeonStatusBar::Draw(status);
    } else {
      ImGui::TextDisabled(tr("No active viewer"));
    }
  }
  ImGui::EndChild();
  // Floating, independent window; drawn here so it stays available while the
  // navigation sidebar is collapsed.
  if (primary_viewer) {
    DrawProposalPreviewWindow(*primary_viewer);
  }
}

void DungeonWorkbenchContent::DrawInspectorPane(float width, float height,
                                                float button_size, bool compact,
                                                DungeonCanvasViewer* viewer) {
  const bool open = ImGui::BeginChild(
      "##DungeonWorkbenchInspector", ImVec2(width, height), true,
      ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse);
  if (open) {
    DrawInspectorHeader(button_size, compact);
    const float available = std::max(1.0f, ImGui::GetContentRegionAvail().y);
    const float spacing = ImGui::GetStyle().ItemSpacing.y;
    const float toggle_height = ImGui::GetFrameHeight();
    const float selection_height =
        show_room_matrix_
            ? std::max(1.0f, available * navigator_tools_fraction_)
            : std::max(1.0f, available - toggle_height - 2.0f * spacing);
    if (ImGui::BeginChild("##WorkbenchSelection", ImVec2(0, selection_height),
                          false)) {
      if (viewer)
        DrawInspector(*viewer, compact);
      else
        ImGui::TextDisabled("No active room");
    }
    ImGui::EndChild();
    if (!show_room_matrix_) {
      if (ImGui::Button(ICON_MD_GRID_VIEW " Show Room Matrix",
                        ImVec2(-1, toggle_height)))
        show_room_matrix_ = true;
    } else {
      const ImVec2 divider = ImGui::GetCursorScreenPos();
      ImGui::InvisibleButton("##NavigatorSplit", ImVec2(-1, 6));
      if (ImGui::IsItemHovered() || ImGui::IsItemActive())
        ImGui::SetMouseCursor(ImGuiMouseCursor_ResizeNS);
      if (ImGui::IsItemActive())
        navigator_tools_fraction_ = std::clamp(
            navigator_tools_fraction_ + ImGui::GetIO().MouseDelta.y / available,
            .20f, .65f);
      ImGui::GetWindowDrawList()->AddLine(
          ImVec2(divider.x, divider.y + 3),
          ImVec2(ImGui::GetItemRectMax().x, divider.y + 3),
          ImGui::GetColorU32(ImGuiCol_Separator));
      if (ImGui::SmallButton(ICON_MD_EXPAND_MORE "##CollapseMatrix"))
        show_room_matrix_ = false;
      ImGui::SameLine();
      ImGui::TextDisabled("Room Matrix");
      if (ImGui::BeginChild("##NavigatorMatrix", ImVec2(0, 0), false,
                            ImGuiWindowFlags_NoScrollbar |
                                ImGuiWindowFlags_NoScrollWithMouse)) {
        if (is_standalone_tool_open_ &&
            is_standalone_tool_open_("dungeon.room_matrix")) {
          if (ImGui::Button("Focus Matrix Window") &&
              open_and_focus_standalone_tool_)
            open_and_focus_standalone_tool_("dungeon.room_matrix");
        } else if (room_matrix_content_) {
          room_matrix_content_->DrawInWorkbench(144.0f);
        } else {
          ImGui::TextDisabled("Room Matrix unavailable");
        }
      }
      ImGui::EndChild();
    }
  }
  ImGui::EndChild();
}

void DungeonWorkbenchContent::DrawInspectorHeader(float button_size,
                                                  bool compact) {
  workbench::DrawPaneHeader(
      "##DungeonWorkbenchInspectorHeader", ICON_MD_SELECT_ALL, "Selection",
      "Selection", nullptr, compact,
      workbench::CalcIconButtonWidth(ICON_MD_CHEVRON_RIGHT, button_size),
      [&]() {
        if (workbench::DrawHeaderIconAction(
                "CollapseInspector", ICON_MD_CHEVRON_RIGHT, button_size,
                "Collapse selection and room matrix"))
          layout_state_.show_right_inspector = false;
      });
}

void DungeonWorkbenchContent::DrawSplitView(
    DungeonCanvasViewer& primary_viewer) {
  if (!current_room_id_ || !split_view_enabled_ || compare_room_id_ < 0) {
    if (split_view_enabled_) {
      split_view_enabled_ = false;
    }
    return;
  }

  // Choose a sensible default compare room (most-recent non-current).
  if (compare_room_id_ < 0 || compare_room_id_ == *current_room_id_) {
    if (get_recent_rooms_) {
      for (int rid : get_recent_rooms_()) {
        if (rid != *current_room_id_) {
          compare_room_id_ = rid;
          break;
        }
      }
    }
  }

  if (compare_room_id_ < 0) {
    // Nothing to compare yet.
    split_view_enabled_ = false;
    primary_viewer.DrawDungeonCanvas(*current_room_id_);
    return;
  }

  constexpr ImGuiTableFlags kSplitFlags =
      ImGuiTableFlags_Resizable | ImGuiTableFlags_NoPadOuterX |
      ImGuiTableFlags_NoPadInnerX | ImGuiTableFlags_BordersInnerV;

  if (!ImGui::BeginTable("##DungeonWorkbenchSplit", 2, kSplitFlags)) {
    primary_viewer.DrawDungeonCanvas(*current_room_id_);
    return;
  }

  ImGui::TableSetupColumn("Active", ImGuiTableColumnFlags_WidthStretch);
  ImGui::TableSetupColumn("Compare", ImGuiTableColumnFlags_WidthStretch);
  ImGui::TableNextRow();

  DungeonCanvasViewer* compare_viewer =
      get_compare_viewer_ ? get_compare_viewer_(compare_room_id_) : nullptr;

  // Active pane (minimum height so canvas never collapses)
  ImGui::TableNextColumn();
  ImGui::AlignTextToFramePadding();
  const project::YazeProject* active_project = primary_viewer.project();
  ImGui::TextDisabled(
      ICON_MD_CROP_FREE " Active  [%03X] %s", *current_room_id_,
      dungeon_project_labels::GetRoomLabel(active_project, *current_room_id_)
          .c_str());
  ImGui::Separator();
  const bool split_active_open = gui::LayoutHelpers::BeginContentChild(
      "##SplitActive", ImVec2(1.0f, 1.0f), false,
      ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse);
  if (split_active_open) {
    primary_viewer.DrawDungeonCanvas(*current_room_id_);
  }
  gui::LayoutHelpers::EndContentChild();

  // Compare pane
  ImGui::TableNextColumn();
  ImGui::AlignTextToFramePadding();
  const project::YazeProject* compare_project =
      compare_viewer ? compare_viewer->project() : active_project;
  ImGui::TextDisabled(
      ICON_MD_COMPARE_ARROWS " Compare [%03X] %s", compare_room_id_,
      dungeon_project_labels::GetRoomLabel(compare_project, compare_room_id_)
          .c_str());
  ImGui::Separator();
  const bool split_compare_open = gui::LayoutHelpers::BeginContentChild(
      "##SplitCompare", ImVec2(1.0f, 1.0f), false,
      ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse);
  if (split_compare_open) {
    if (compare_viewer) {
      if (layout_state_.sync_split_view) {
        compare_viewer->canvas().ApplyScaleSnapshot(
            primary_viewer.canvas().GetConfig());
      }
      compare_viewer->DrawDungeonCanvas(compare_room_id_);
    } else {
      ImGui::TextDisabled(tr("No compare viewer"));
    }
  }
  gui::LayoutHelpers::EndContentChild();

  ImGui::EndTable();
}

void DungeonWorkbenchContent::BuildRoomDungeonCache() {
  room_dungeon_cache_.clear();
  room_dungeon_cache_built_ = true;  // Always set, even if ROM missing.
  if (!rom_ || !rom_->is_loaded() ||
      !zelda3::ResolveEntranceLayout(rom_->vector()).ok())
    return;

  // Short dungeon names for display in the inspector badge.
  // Indices 0-13 = vanilla ALTTP dungeons; higher indices = custom/Oracle.
  static const char* const kShortNames[] = {
      "Sewers", "HC",    "Eastern", "Desert", "A-Tower", "Swamp",  "PoD",
      "Misery", "Skull", "Ice",     "Hera",   "Thieves", "Turtle", "GT",
  };
  constexpr int kVanillaCount =
      static_cast<int>(sizeof(kShortNames) / sizeof(kShortNames[0]));

  auto AddRoom = [&](int room_id, int dungeon_id) {
    if (room_id < 0 || room_id >= zelda3::kNumberOfRooms)
      return;
    if (room_dungeon_cache_.contains(room_id))
      return;  // Entrance wins over spawn.
    if (dungeon_id >= 0 && dungeon_id < kVanillaCount) {
      room_dungeon_cache_[room_id] = kShortNames[dungeon_id];
    } else {
      char buf[16];
      snprintf(buf, sizeof(buf), "Dungeon %02X", dungeon_id);
      room_dungeon_cache_[room_id] = buf;
    }
  };

  // Standard entrances — authoritative dungeon assignment.
  for (int i = 0; i < zelda3::kNumRegularDungeonEntrances; ++i) {
    zelda3::RoomEntrance ent(rom_, static_cast<uint8_t>(i), false);
    if (!ent.load_status().ok() || ent.room_ < 0 ||
        ent.room_ >= zelda3::kNumberOfRooms)
      continue;
    int did = ent.dungeon_id_;
    if (did >= 0 && did < kVanillaCount) {
      room_dungeon_cache_[ent.room_] = kShortNames[did];
    } else {
      char buf[16];
      snprintf(buf, sizeof(buf), "Dungeon %02X", did);
      room_dungeon_cache_[ent.room_] = buf;
    }
  }

  // Spawn points — fill in rooms not covered by entrances.
  for (int i = 0; i < zelda3::kNumDungeonSpawnPoints; ++i) {
    zelda3::RoomEntrance ent(rom_, static_cast<uint8_t>(i), true);
    if (ent.load_status().ok())
      AddRoom(static_cast<int>(ent.room_), static_cast<int>(ent.dungeon_id_));
  }
}

void DungeonWorkbenchContent::DrawInspector(DungeonCanvasViewer& viewer,
                                            bool compact) {
  // Gentle compaction only; inspector sections own their own internal
  // spacing (collapsing-header headers, action buttons), so let the theme
  // defaults shape inter-section breathing room here.
  gui::StyleVarGuard item_spacing_guard(
      ImGuiStyleVar_ItemSpacing,
      ImVec2(std::max(ImGui::GetStyle().ItemSpacing.x, 4.0f),
             std::max(4.0f, ImGui::GetStyle().ItemSpacing.y - 1.0f)));
  DrawInspectorShelf(viewer, compact);
}

void DungeonWorkbenchContent::SetAllSaveFlags(bool value) {
  auto& flags = core::FeatureFlags::get().dungeon;
  flags.kSaveObjects = value;
  flags.kSaveSprites = value;
  flags.kSaveRoomHeaders = value;
  flags.kSaveChests = value;
  flags.kSavePotItems = value;
  flags.kSaveEntrances = value;
  flags.kSavePalettes = value;
  flags.kSaveCollision = value;
  flags.kSaveBlocks = value;
  flags.kSaveTorches = value;
  flags.kSavePits = value;
}

void DungeonWorkbenchContent::DrawApplyScopeControls(int room_id) {
  auto& flags = core::FeatureFlags::get().dungeon;
  ImGui::TextUnformatted(tr("Room content to apply"));
  constexpr ImGuiTableFlags kFlags =
      ImGuiTableFlags_SizingStretchSame | ImGuiTableFlags_NoPadOuterX;
  if (ImGui::BeginTable("##WorkbenchSaveContentFlags", 2, kFlags,
                        ImVec2(320.0f, 0.0f))) {
    auto draw_checkbox = [](const char* label, bool* value) {
      ImGui::TableNextColumn();
      ImGui::Checkbox(label, value);
    };
    ImGui::TableNextRow();
    draw_checkbox("Room objects", &flags.kSaveObjects);
    draw_checkbox("Room sprites", &flags.kSaveSprites);
    ImGui::TableNextRow();
    draw_checkbox("Room properties", &flags.kSaveRoomHeaders);
    draw_checkbox("Chest contents", &flags.kSaveChests);
    ImGui::TableNextRow();
    draw_checkbox("Items under pots", &flags.kSavePotItems);
    draw_checkbox("Palettes", &flags.kSavePalettes);
    ImGui::TableNextRow();
    draw_checkbox("Custom collision", &flags.kSaveCollision);
    ImGui::TableNextColumn();
    ImGui::BeginDisabled();
    ImGui::Checkbox("Water Fill", &flags.kSaveWaterFillZones);
    ImGui::EndDisabled();
    if (ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled)) {
      ImGui::SetTooltip(
          "Water Fill saving is fixed when the project opens. Change "
          "save_dungeon_water_fill_zones in the .yaze file, then reopen the "
          "project.");
    }
    ImGui::TableNextRow();
    draw_checkbox("Push blocks", &flags.kSaveBlocks);
    draw_checkbox("Torches", &flags.kSaveTorches);
    ImGui::TableNextRow();
    draw_checkbox("Pit damage", &flags.kSavePits);
    draw_checkbox("Entrances", &flags.kSaveEntrances);
    ImGui::EndTable();
  }

  if (ImGui::SmallButton(tr("Select all##WorkbenchSaveContent"))) {
    SetAllSaveFlags(true);
  }
  ImGui::SameLine();
  if (ImGui::SmallButton(tr("Select none##WorkbenchSaveContent"))) {
    SetAllSaveFlags(false);
  }

  ImGui::Separator();
  const auto apply_room_label =
      absl::StrFormat(ICON_MD_SAVE " Apply room 0x%03X", room_id);
  if (on_save_room_ && room_id >= 0 &&
      workbench::DrawActionButton(apply_room_label.c_str(), ImVec2(-1, 0))) {
    on_save_room_(room_id);
  }
  if (on_save_all_rooms_ &&
      workbench::DrawActionButton(ICON_MD_SAVE_ALT " Apply all loaded rooms",
                                  ImVec2(-1, 0))) {
    on_save_all_rooms_();
  }
  ImGui::TextWrapped(
      tr("Apply uses the checked content. File > Save ROM "
         "writes the applied changes to disk."));
}

void DungeonWorkbenchContent::DrawPitDamageControls(int room_id, bool compact) {
  pit_damage_control_rects_ = {};
  if (pit_damage_chooser_room_id_ >= 0 &&
      pit_damage_chooser_room_id_ != room_id) {
    if (ImGui::BeginPopup("##PitDamageSlotChooser")) {
      ImGui::CloseCurrentPopup();
      ImGui::EndPopup();
    }
    pit_damage_chooser_room_id_ = -1;
  }
  const auto& theme = AgentUI::GetTheme();
  zelda3::PitDamageTable* table =
      get_pit_damage_table_ ? get_pit_damage_table_() : nullptr;
  if (!table) {
    bool enabled = false;
    ImGui::BeginDisabled();
    ImGui::Checkbox(
        compact ? "##PitDamageEnabled" : tr("Pit damage##PitDamageEnabled"),
        &enabled);
    ImGui::EndDisabled();
    if (ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled)) {
      ImGui::SetTooltip(tr("Available after room data loads."));
    }
    return;
  }
  const auto membership = BuildPitDamageMembershipState(
      table, room_id, pit_damage_replacement_room_id_,
      pit_damage_victim_room_id_);
  if (!membership.room_valid) {
    ImGui::TextDisabled(tr("No valid room selected."));
    return;
  }

  const uint16_t current_room = membership.room_id;
  bool room_deals_damage = membership.deals_damage;
  if (ImGui::Checkbox(
          compact ? "##PitDamageEnabled" : tr("Pit damage##PitDamageEnabled"),
          &room_deals_damage)) {
    pit_damage_chooser_room_id_ = room_id;
    ImGui::OpenPopup("##PitDamageSlotChooser");
  }
  {
    gui::AutoWidgetScope automation_scope("Dungeon/Workbench");
    gui::AutoRegisterLastItem("checkbox", "PitDamageEnabled");
  }
  if (ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled)) {
    ImGui::BeginTooltip();
    ImGui::TextUnformatted(
        "Choose which room gives up or receives a pit-damage slot.");
    if (membership.dirty) {
      ImGui::TextColored(theme.status_warning, "Pending pit damage change");
    }
    if (!pit_damage_status_message_.empty()) {
      ImGui::TextWrapped("%s", pit_damage_status_message_.c_str());
    }
    ImGui::EndTooltip();
  }

  auto show_status = [&]() {
    if (pit_damage_status_message_.empty()) {
      return;
    }
    ImGui::PushTextWrapPos(0.0f);
    ImGui::TextColored(
        pit_damage_status_error_ ? theme.status_error : theme.status_success,
        "%s %s",
        pit_damage_status_error_ ? ICON_MD_WARNING : ICON_MD_CHECK_CIRCLE,
        pit_damage_status_message_.c_str());
    ImGui::PopTextWrapPos();
  };

  if (!ImGui::BeginPopup("##PitDamageSlotChooser")) {
    return;
  }

  // Toggling requests a membership change. Only the explicit confirmation
  // transfers a fixed-capacity slot; closing the chooser leaves it untouched.
  if (membership.deals_damage) {
    if (!membership.suggested_replacement_room.has_value()) {
      ImGui::PushTextWrapPos(0.0f);
      ImGui::TextDisabled(tr("No available non-damaging room slot."));
      ImGui::PopTextWrapPos();
      show_status();
      ImGui::EndPopup();
      return;
    }
    pit_damage_replacement_room_id_ = *membership.suggested_replacement_room;

    ImGui::TextUnformatted(tr("Move pit damage to room:"));
    ImGui::SetNextItemWidth(88.0f);
    auto edit_value = pit_damage_replacement_room_id_;
    if (auto res = gui::InputHexWordEx("##PitDamageReplacementRoom",
                                       &edit_value, 88.0f, true);
        res.ShouldApply()) {
      pit_damage_replacement_room_id_ =
          std::min<uint16_t>(edit_value, zelda3::kNumberOfRooms - 1);
    }
    if (ImGui::IsItemHovered()) {
      ImGui::SetTooltip(
          tr("Replacement room must not already be in the table."));
    }
    const bool replace_clicked =
        ImGui::Button(tr("Disable pit damage##PitDamageReplaceCurrent"));
    pit_damage_control_rects_.replace_current = CaptureLastItemRectForTesting();
    if (replace_clicked) {
      auto status = RemoveCurrentRoomFromPitDamage(
          table, current_room, pit_damage_replacement_room_id_);
      pit_damage_status_error_ = !status.ok();
      pit_damage_status_message_ =
          status.ok()
              ? absl::StrFormat("Room 0x%03X removed; slot now 0x%03X",
                                current_room, pit_damage_replacement_room_id_)
              : std::string(status.message());
      if (status.ok()) {
        if (on_pit_damage_changed_) {
          on_pit_damage_changed_();
        }
        pit_damage_chooser_room_id_ = -1;
        ImGui::CloseCurrentPopup();
      }
    }
  } else {
    if (!membership.suggested_victim_room.has_value()) {
      ImGui::PushTextWrapPos(0.0f);
      ImGui::TextDisabled(tr("No existing pit-damage slot is available."));
      ImGui::PopTextWrapPos();
      show_status();
      ImGui::EndPopup();
      return;
    }
    pit_damage_victim_room_id_ = *membership.suggested_victim_room;

    ImGui::TextUnformatted(tr("Take pit damage from room:"));
    ImGui::SetNextItemWidth(88.0f);
    auto edit_value = pit_damage_victim_room_id_;
    if (auto res = gui::InputHexWordEx("##PitDamageVictimRoom", &edit_value,
                                       88.0f, true);
        res.ShouldApply()) {
      pit_damage_victim_room_id_ =
          std::min<uint16_t>(edit_value, zelda3::kNumberOfRooms - 1);
    }
    if (ImGui::IsItemHovered()) {
      ImGui::SetTooltip(tr("This room will stop dealing pit damage."));
    }
    const bool add_clicked =
        ImGui::Button(tr("Enable pit damage##PitDamageAddCurrent"));
    pit_damage_control_rects_.add_current = CaptureLastItemRectForTesting();
    if (add_clicked) {
      auto status = AddCurrentRoomToPitDamage(table, current_room,
                                              pit_damage_victim_room_id_);
      pit_damage_status_error_ = !status.ok();
      pit_damage_status_message_ =
          status.ok()
              ? absl::StrFormat("Room 0x%03X added; replaced 0x%03X",
                                current_room, pit_damage_victim_room_id_)
              : std::string(status.message());
      if (status.ok()) {
        if (on_pit_damage_changed_) {
          on_pit_damage_changed_();
        }
        pit_damage_chooser_room_id_ = -1;
        ImGui::CloseCurrentPopup();
      }
    }
  }

  ImGui::SameLine();
  if (ImGui::Button(tr("Cancel##PitDamageCancel"))) {
    pit_damage_chooser_room_id_ = -1;
    ImGui::CloseCurrentPopup();
  }
  {
    gui::AutoWidgetScope automation_scope("Dungeon/Workbench");
    gui::AutoRegisterLastItem("button", "PitDamageCancel");
  }
  show_status();
  ImGui::EndPopup();
}

DungeonMapPanel* DungeonWorkbenchContent::GetEmbeddedDungeonMap(
    DungeonCanvasViewer& viewer) {
  if (!room_selector_ || !current_room_id_) {
    return nullptr;
  }
  if (!embedded_dungeon_map_) {
    embedded_dungeon_map_ = std::make_unique<DungeonMapPanel>(
        current_room_id_, &room_selector_->mutable_active_rooms(),
        on_room_selected_, viewer.rooms());
    embedded_dungeon_map_->SetRoomIntentCallback(on_room_selected_with_intent_);
    if (*current_room_id_ >= 0) {
      embedded_dungeon_map_->AddRoom(*current_room_id_);
    }
  }
  embedded_dungeon_map_->SetRooms(viewer.rooms());
  embedded_dungeon_map_->SetRenderContextCallback(prepare_room_render_context_);
  if (const auto* project = viewer.project()) {
    embedded_dungeon_map_->SetHackManifest(&project->hack_manifest);
  } else {
    embedded_dungeon_map_->SetHackManifest(nullptr);
  }
  return embedded_dungeon_map_.get();
}

void DungeonWorkbenchContent::DrawDungeonMapPopup(DungeonCanvasViewer& viewer) {
  constexpr const char* kPopupId = "Dungeon Map##WorkbenchDungeonMapPopup";
  if (open_dungeon_map_popup_) {
    const ImVec2 viewport = ImGui::GetMainViewport()->WorkSize;
    ImGui::SetNextWindowSize(ImVec2(std::min(1040.0f, viewport.x * 0.85f),
                                    std::min(780.0f, viewport.y * 0.85f)),
                             ImGuiCond_Appearing);
    ImGui::OpenPopup(kPopupId);
    open_dungeon_map_popup_ = false;
  }

  bool popup_open = true;
  if (ImGui::BeginPopupModal(kPopupId, &popup_open,
                             ImGuiWindowFlags_NoSavedSettings)) {
    if (!popup_open) {
      ImGui::CloseCurrentPopup();
      ImGui::EndPopup();
      return;
    }
    if (auto* map = GetEmbeddedDungeonMap(viewer)) {
      const ImVec2 map_size =
          ImVec2(std::max(1.0f, ImGui::GetContentRegionAvail().x),
                 std::max(1.0f, ImGui::GetContentRegionAvail().y -
                                    ImGui::GetFrameHeightWithSpacing()));
      if (ImGui::BeginChild("##WorkbenchDungeonMapBody", map_size, false)) {
        map->Draw(nullptr);
      }
      ImGui::EndChild();
    } else {
      ImGui::TextDisabled(tr("Dungeon map unavailable"));
    }

    if (workbench::DrawActionButton(ICON_MD_CLOSE " Close", ImVec2(-1, 0))) {
      ImGui::CloseCurrentPopup();
    }
    ImGui::EndPopup();
  }
}

void DungeonWorkbenchContent::OpenTool(WorkbenchTool tool) {
  if (tool == WorkbenchTool::None) {
    return;
  }
  const bool keep_minecart_tool = tool == WorkbenchTool::MinecartTracks &&
                                  active_tool_ == tool &&
                                  sidebar_mode_ == SidebarMode::Tools;
  active_tool_ = tool;
  if (minecart_track_panel_ && !keep_minecart_tool) {
    minecart_track_panel_->CancelNodePlacement();
    minecart_track_panel_->set_collision_stamping(false);
    minecart_track_panel_->set_canvas_editing(tool ==
                                              WorkbenchTool::MinecartTracks);
  }

  if (ResolveDungeonWorkbenchToolRequestTarget(IsStandaloneToolOpen(tool)) ==
      DungeonWorkbenchToolRequestTarget::kStandaloneWindow) {
    // A pinned or explicitly popped-out tool remains the presentation owner.
    // Re-open/focus it, and never draw the same WindowContent twice.
    CloseToolInspector();
    if (open_and_focus_standalone_tool_) {
      if (WindowContent* content = GetWorkbenchToolContent(tool)) {
        (void)open_and_focus_standalone_tool_(content->GetId());
      }
    }
    return;
  }

  if (tool == WorkbenchTool::ObjectSelector ||
      tool == WorkbenchTool::SpriteEditor ||
      tool == WorkbenchTool::DoorEditor || tool == WorkbenchTool::ItemEditor) {
    CloseToolInspector();
    sidebar_mode_ = tool == WorkbenchTool::SpriteEditor ? SidebarMode::Sprites
                    : tool == WorkbenchTool::DoorEditor ? SidebarMode::Doors
                    : tool == WorkbenchTool::ItemEditor ? SidebarMode::Items
                                                        : SidebarMode::Objects;
    layout_state_.show_left_sidebar = true;
    FocusSelectionInspector();
    return;
  }

  inspector_mode_ = InspectorMode::Tools;
  sidebar_mode_ = SidebarMode::Tools;
  layout_state_.show_left_sidebar = true;
}

WindowContent* DungeonWorkbenchContent::GetWorkbenchToolContent(
    WorkbenchTool tool) const {
  switch (tool) {
    case WorkbenchTool::RoomTags:
      return room_tag_panel_;
    case WorkbenchTool::CustomCollision:
      return custom_collision_panel_;
    case WorkbenchTool::WaterFill:
      return water_fill_panel_;
    case WorkbenchTool::MinecartTracks:
      return minecart_track_panel_;
    case WorkbenchTool::ObjectCoverage:
      return object_coverage_content_;
    case WorkbenchTool::ObjectSelector:
      return object_selector_content_;
    case WorkbenchTool::DoorEditor:
      return door_editor_content_;
    case WorkbenchTool::SpriteEditor:
      return sprite_editor_content_;
    case WorkbenchTool::ItemEditor:
      return item_editor_content_;
    case WorkbenchTool::RoomGraphics:
      return room_graphics_content_;
    case WorkbenchTool::Palette:
      return palette_editor_content_;
    case WorkbenchTool::None:
      return nullptr;
  }
  return nullptr;
}

bool DungeonWorkbenchContent::IsWorkbenchToolAvailable(
    WorkbenchTool tool) const {
  return GetWorkbenchToolContent(tool) != nullptr;
}

bool DungeonWorkbenchContent::IsStandaloneToolOpen(WorkbenchTool tool) const {
  WindowContent* content = GetWorkbenchToolContent(tool);
  if (!content || !is_standalone_tool_open_) {
    return false;
  }
  const std::string standalone_id = content->GetId();
  return !standalone_id.empty() && is_standalone_tool_open_(standalone_id);
}

void DungeonWorkbenchContent::CloseToolInspector() {
  if (inspector_mode_ == InspectorMode::Tools) {
    if (minecart_track_panel_) {
      minecart_track_panel_->set_collision_stamping(false);
      minecart_track_panel_->CancelNodePlacement();
      minecart_track_panel_->set_canvas_editing(false);
    }
    if (get_viewer_) {
      if (auto* viewer = get_viewer_()) {
        viewer->object_interaction().HandleMouseRelease();
        viewer->object_interaction().CancelPlacement();
        viewer->object_interaction().mode_manager().CancelCurrentMode();
      }
    }
  }
  inspector_mode_ = InspectorMode::Inspect;
  if (sidebar_mode_ == SidebarMode::Tools)
    sidebar_mode_ = SidebarMode::Room;
}

bool DungeonWorkbenchContent::PopOutActiveTool() {
  WindowContent* content = GetWorkbenchToolContent(active_tool_);
  if (!content || !open_and_focus_standalone_tool_) {
    return false;
  }

  const std::string standalone_id = content->GetId();
  if (standalone_id.empty() ||
      !open_and_focus_standalone_tool_(standalone_id)) {
    return false;
  }

  CloseToolInspector();
  return true;
}

const char* DungeonWorkbenchContent::GetWorkbenchToolId(
    WorkbenchTool tool) const {
  switch (tool) {
    case WorkbenchTool::RoomTags:
      return "room_tags";
    case WorkbenchTool::CustomCollision:
      return "custom_collision";
    case WorkbenchTool::WaterFill:
      return "water_fill";
    case WorkbenchTool::MinecartTracks:
      return "minecart";
    case WorkbenchTool::ObjectCoverage:
      return "object_coverage";
    case WorkbenchTool::ObjectSelector:
      return "object_selector";
    case WorkbenchTool::DoorEditor:
      return "door";
    case WorkbenchTool::SpriteEditor:
      return "sprite";
    case WorkbenchTool::ItemEditor:
      return "item";
    case WorkbenchTool::RoomGraphics:
      return "room_graphics";
    case WorkbenchTool::Palette:
      return "palette";
    case WorkbenchTool::None:
      return "none";
  }
  return "unknown";
}

const char* DungeonWorkbenchContent::GetWorkbenchToolShortLabel(
    WorkbenchTool tool) const {
  switch (tool) {
    case WorkbenchTool::RoomTags:
      return "Room Tags";
    case WorkbenchTool::CustomCollision:
      return "Custom Collision";
    case WorkbenchTool::WaterFill:
      return "Water Fill";
    case WorkbenchTool::MinecartTracks:
      return "Minecart Tracks";
    case WorkbenchTool::ObjectCoverage:
      return "Object Coverage";
    case WorkbenchTool::ObjectSelector:
      return "Object Selector";
    case WorkbenchTool::DoorEditor:
      return "Door Tools";
    case WorkbenchTool::SpriteEditor:
      return "Sprite Tools";
    case WorkbenchTool::ItemEditor:
      return "Item Tools";
    case WorkbenchTool::RoomGraphics:
      return "Room Graphics";
    case WorkbenchTool::Palette:
      return "Palette";
    case WorkbenchTool::None:
      return "Tool";
  }
  return "Tool";
}

const char* DungeonWorkbenchContent::GetWorkbenchToolUnavailableMessage(
    WorkbenchTool tool) const {
  switch (tool) {
    case WorkbenchTool::RoomTags:
      return "Room tag tools are not available.";
    case WorkbenchTool::CustomCollision:
      return "Custom collision tools are not available.";
    case WorkbenchTool::WaterFill:
      return "Water fill tools are not available.";
    case WorkbenchTool::MinecartTracks:
      return "Minecart track tools are not available.";
    case WorkbenchTool::ObjectCoverage:
      return "Object coverage is not available.";
    case WorkbenchTool::ObjectSelector:
      return "Object selector is not available.";
    case WorkbenchTool::DoorEditor:
      return "Door tools are not available.";
    case WorkbenchTool::SpriteEditor:
      return "Sprite tools are not available.";
    case WorkbenchTool::ItemEditor:
      return "Item tools are not available.";
    case WorkbenchTool::RoomGraphics:
      return "Room graphics tools are not available.";
    case WorkbenchTool::Palette:
      return "Palette tools are not available.";
    case WorkbenchTool::None:
      return "No Workbench tool selected.";
  }
  return "Tool is not available.";
}

void DungeonWorkbenchContent::DrawWorkbenchTool(DungeonCanvasViewer& viewer,
                                                WorkbenchTool tool) {
  const int room_id = viewer.current_room_id();
  auto draw_window_content = [](WindowContent* content,
                                const char* unavailable_message) {
    if (!content) {
      ImGui::TextDisabled("%s", unavailable_message);
      return;
    }
    content->Draw(nullptr);
  };

  ImGui::PushID(GetWorkbenchToolId(tool));
  switch (tool) {
    case WorkbenchTool::RoomTags:
      if (!room_tag_panel_) {
        ImGui::TextDisabled("%s", GetWorkbenchToolUnavailableMessage(tool));
        break;
      }
      room_tag_panel_->SetCurrentRoomId(room_id);
      room_tag_panel_->Draw(nullptr);
      break;
    case WorkbenchTool::CustomCollision:
      if (!custom_collision_panel_) {
        ImGui::TextDisabled("%s", GetWorkbenchToolUnavailableMessage(tool));
        break;
      }
      custom_collision_panel_->SetCanvasViewer(&viewer);
      custom_collision_panel_->SetInteraction(&viewer.object_interaction());
      custom_collision_panel_->Draw(nullptr);
      break;
    case WorkbenchTool::WaterFill:
      if (!water_fill_panel_) {
        ImGui::TextDisabled("%s", GetWorkbenchToolUnavailableMessage(tool));
        break;
      }
      water_fill_panel_->SetCanvasViewer(&viewer);
      water_fill_panel_->SetInteraction(&viewer.object_interaction());
      water_fill_panel_->Draw(nullptr);
      break;
    case WorkbenchTool::MinecartTracks:
      if (!minecart_track_panel_) {
        ImGui::TextDisabled("%s", GetWorkbenchToolUnavailableMessage(tool));
        break;
      }
      minecart_track_panel_->Draw(nullptr);
      break;
    case WorkbenchTool::ObjectCoverage:
      draw_window_content(object_coverage_content_,
                          GetWorkbenchToolUnavailableMessage(tool));
      break;
    case WorkbenchTool::ObjectSelector:
      if (auto* selector =
              dynamic_cast<ObjectSelectorContent*>(object_selector_content_)) {
        selector->DrawInWorkbench();
      } else {
        draw_window_content(object_selector_content_,
                            GetWorkbenchToolUnavailableMessage(tool));
      }
      break;
    case WorkbenchTool::DoorEditor:
      draw_window_content(door_editor_content_,
                          GetWorkbenchToolUnavailableMessage(tool));
      break;
    case WorkbenchTool::SpriteEditor:
      draw_window_content(sprite_editor_content_,
                          GetWorkbenchToolUnavailableMessage(tool));
      break;
    case WorkbenchTool::ItemEditor:
      draw_window_content(item_editor_content_,
                          GetWorkbenchToolUnavailableMessage(tool));
      break;
    case WorkbenchTool::RoomGraphics:
      draw_window_content(room_graphics_content_,
                          GetWorkbenchToolUnavailableMessage(tool));
      break;
    case WorkbenchTool::Palette:
      draw_window_content(palette_editor_content_,
                          GetWorkbenchToolUnavailableMessage(tool));
      break;
    case WorkbenchTool::None:
      ImGui::TextDisabled("%s", GetWorkbenchToolUnavailableMessage(tool));
      break;
  }
  ImGui::PopID();
}

void DungeonWorkbenchContent::DrawInspectorToolPicker() {
  constexpr WorkbenchTool tools[] = {
      WorkbenchTool::RoomGraphics,  WorkbenchTool::Palette,
      WorkbenchTool::RoomTags,      WorkbenchTool::CustomCollision,
      WorkbenchTool::WaterFill,     WorkbenchTool::MinecartTracks,
      WorkbenchTool::ObjectCoverage};
  const bool pressed = ImGui::Button("Room tools");
  {
    gui::AutoWidgetScope scope("Dungeon/Workbench");
    gui::AutoRegisterLastItem("button", "tool_picker",
                              "Specialized room tools");
  }
  if (pressed)
    ImGui::OpenPopup("##WorkbenchToolPickerPopup");
  if (ImGui::BeginPopup("##WorkbenchToolPickerPopup")) {
    for (const auto tool : tools) {
      const bool available = IsWorkbenchToolAvailable(tool);
      const bool selected = ImGui::MenuItem(
          GetWorkbenchToolShortLabel(tool), nullptr,
          inspector_mode_ == InspectorMode::Tools && active_tool_ == tool,
          available);
      {
        gui::AutoWidgetScope scope("Dungeon/Workbench");
        gui::AutoRegisterLastItem(
            "menu_item", absl::StrFormat("tool_%s", GetWorkbenchToolId(tool)));
      }
      if (selected)
        OpenTool(tool);
    }
    ImGui::EndPopup();
  }
}

void DungeonWorkbenchContent::DrawInspectorToolPanel(
    DungeonCanvasViewer& viewer) {
  const bool back = ImGui::Button("Room");
  {
    gui::AutoWidgetScope scope("Dungeon/Workbench");
    gui::AutoRegisterLastItem("button", "back_to_inspect");
  }
  if (back) {
    FocusRoomInspector();
    return;
  }
  ImGui::SameLine();
  const bool can_pop_out =
      IsWorkbenchToolAvailable(active_tool_) && open_and_focus_standalone_tool_;
  ImGui::BeginDisabled(!can_pop_out);
  const bool pop_out = ImGui::Button("Pop out");
  {
    gui::AutoWidgetScope scope("Dungeon/Workbench");
    gui::AutoRegisterLastItem("button", "pop_out_tool");
  }
  ImGui::EndDisabled();
  if (pop_out && can_pop_out) {
    (void)PopOutActiveTool();
    return;
  }
  if (IsStandaloneToolOpen(active_tool_)) {
    ImGui::TextDisabled("%s is open in a separate window.",
                        GetWorkbenchToolShortLabel(active_tool_));
    return;
  }
  if (!IsWorkbenchToolAvailable(active_tool_)) {
    ImGui::TextDisabled("%s", GetWorkbenchToolUnavailableMessage(active_tool_));
    return;
  }
  ImGui::SeparatorText(GetWorkbenchToolShortLabel(active_tool_));
  const bool body_open = ImGui::BeginChild("##WorkbenchToolInspectorBody",
                                           ImVec2(0.0f, 0.0f), false);
  if (body_open)
    DrawWorkbenchTool(viewer, active_tool_);
  ImGui::EndChild();
}

}  // namespace yaze::editor
