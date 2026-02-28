#include "dungeon_status_bar.h"

#include <algorithm>
#include <cstdio>

#include "app/editor/dungeon/dungeon_canvas_viewer.h"
#include "app/gui/core/icons.h"
#include "app/gui/core/layout_helpers.h"
#include "app/gui/core/style_guard.h"
#include "app/gui/core/theme_manager.h"
#include "imgui/imgui.h"

namespace yaze::editor {

void DungeonStatusBar::Draw(const DungeonStatusBarState& state) {
  const auto& theme = gui::ThemeManager::Get().GetCurrentTheme();

  // Reserve a fixed-height bar at the bottom
  const float bar_height =
      ImGui::GetFontSize() + ImGui::GetStyle().FramePadding.y * 2.0f;

  gui::StyleColorGuard bar_colors({
      {ImGuiCol_ChildBg, gui::ConvertColorToImVec4(theme.frame_bg)},
  });

  ImGui::BeginChild(
      "##DungeonStatusBar", ImVec2(-1, bar_height), false,
      ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse);

  const float spacing = ImGui::GetStyle().ItemSpacing.x;

  // Tool mode indicator
  ImGui::AlignTextToFramePadding();
  ImGui::TextDisabled(ICON_MD_BUILD);
  ImGui::SameLine(0, 2);
  ImGui::Text("%s", state.tool_mode);
  ImGui::SameLine(0, spacing * 2);

  // Separator
  ImGui::TextDisabled("|");
  ImGui::SameLine(0, spacing * 2);

  // Undo/Redo buttons with depth indicator
  {
    if (!state.can_undo)
      ImGui::BeginDisabled();
    if (ImGui::SmallButton(ICON_MD_UNDO)) {
      if (state.on_undo)
        state.on_undo();
    }
    if (ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled)) {
      ImGui::SetTooltip("Undo%s%s", state.undo_desc ? ": " : "",
                        state.undo_desc ? state.undo_desc : "");
    }
    if (!state.can_undo)
      ImGui::EndDisabled();
    ImGui::SameLine(0, 2);

    if (!state.can_redo)
      ImGui::BeginDisabled();
    if (ImGui::SmallButton(ICON_MD_REDO)) {
      if (state.on_redo)
        state.on_redo();
    }
    if (ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled)) {
      ImGui::SetTooltip("Redo%s%s", state.redo_desc ? ": " : "",
                        state.redo_desc ? state.redo_desc : "");
    }
    if (!state.can_redo)
      ImGui::EndDisabled();

    if (state.undo_depth > 0) {
      ImGui::SameLine(0, 4);
      ImGui::TextDisabled("(%d)", state.undo_depth);
    }
  }
  ImGui::SameLine(0, spacing * 2);

  // Separator
  ImGui::TextDisabled("|");
  ImGui::SameLine(0, spacing * 2);

  // Selection summary
  if (state.selection_count > 0) {
    ImGui::TextDisabled(ICON_MD_SELECT_ALL);
    ImGui::SameLine(0, 2);
    if (state.selection_layer >= 0) {
      ImGui::Text("%d obj, L%d", state.selection_count,
                  state.selection_layer + 1);
    } else {
      ImGui::Text("%d obj", state.selection_count);
    }
  } else {
    ImGui::TextDisabled("No selection");
  }
  ImGui::SameLine(0, spacing * 2);

  // Separator
  ImGui::TextDisabled("|");
  ImGui::SameLine(0, spacing * 2);

  // Zoom level
  ImGui::TextDisabled(ICON_MD_ZOOM_IN);
  ImGui::SameLine(0, 2);
  ImGui::Text("%d%%", state.zoom_percent);
  ImGui::SameLine(0, spacing * 2);

  // Separator
  ImGui::TextDisabled("|");
  ImGui::SameLine(0, spacing * 2);

  // Cursor tile coordinates
  if (state.cursor_tile_x >= 0 && state.cursor_tile_y >= 0) {
    ImGui::TextDisabled(ICON_MD_MY_LOCATION);
    ImGui::SameLine(0, 2);
    ImGui::Text("(%d, %d)", state.cursor_tile_x, state.cursor_tile_y);
  } else {
    ImGui::TextDisabled(ICON_MD_MY_LOCATION " --");
  }

  // Right-aligned section: dirty indicator + room ID
  {
    char right_text[64];
    if (state.room_id >= 0) {
      if (state.room_dirty) {
        snprintf(right_text, sizeof(right_text), ICON_MD_CIRCLE " Room 0x%03X",
                 state.room_id);
      } else {
        snprintf(right_text, sizeof(right_text), "Room 0x%03X", state.room_id);
      }
    } else {
      snprintf(right_text, sizeof(right_text), "No room");
    }

    const float text_width = ImGui::CalcTextSize(right_text).x;
    const float right_x = ImGui::GetWindowWidth() - text_width -
                          ImGui::GetStyle().WindowPadding.x;

    ImGui::SameLine(std::max(ImGui::GetCursorPosX(), right_x));

    if (state.room_dirty) {
      ImGui::TextColored(gui::ConvertColorToImVec4(theme.warning), "%s",
                         right_text);
      if (ImGui::IsItemHovered()) {
        ImGui::SetTooltip("Room has unsaved changes");
      }
    } else {
      ImGui::TextDisabled("%s", right_text);
    }
  }

  ImGui::EndChild();
}

DungeonStatusBarState DungeonStatusBar::BuildState(
    const DungeonCanvasViewer& viewer, const char* tool_mode, bool room_dirty) {
  DungeonStatusBarState state;
  state.tool_mode = tool_mode;
  state.room_dirty = room_dirty;
  state.room_id = viewer.current_room_id();

  // Zoom from canvas
  float scale = viewer.canvas().GetGlobalScale();
  state.zoom_percent = static_cast<int>(scale * 100.0f + 0.5f);

  // Selection from object interaction
  const auto& interaction =
      const_cast<DungeonCanvasViewer&>(viewer).object_interaction();
  auto selected = interaction.GetSelectedObjectIndices();
  state.selection_count = static_cast<int>(selected.size());

  // Determine layer from first selected object (if any)
  if (!selected.empty() && viewer.rooms() && viewer.current_room_id() >= 0 &&
      viewer.current_room_id() < static_cast<int>(viewer.rooms()->size())) {
    const auto& objects =
        (*viewer.rooms())[viewer.current_room_id()].GetTileObjects();
    if (selected[0] < objects.size()) {
      state.selection_layer = objects[selected[0]].layer_;
    }
  }

  // Cursor coordinates from ImGui hover state on canvas
  // Note: the actual tile coordinates are derived from the canvas hover
  // position. Since we don't have direct access to the canvas mouse pos
  // from outside the draw call, we leave these at -1 (the canvas viewer
  // itself could populate this if we add a public accessor later).
  state.cursor_tile_x = -1;
  state.cursor_tile_y = -1;

  return state;
}

}  // namespace yaze::editor
