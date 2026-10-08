#include "app/editor/dungeon/ui/crumble/crumble_floor_view.h"

#include <algorithm>
#include <string_view>

#include "app/editor/dungeon/dungeon_canvas_transform.h"
#include "app/gui/automation/widget_auto_register.h"
#include "app/gui/canvas/canvas_types.h"
#include "app/gui/core/agent_theme.h"
#include "core/project.h"
#include "imgui/imgui.h"
#include "rom/rom.h"
#include "zelda3/dungeon/room.h"

namespace yaze::editor {

bool HasOracleCrumbleTag(const Rom& rom, const project::YazeProject* project,
                         const zelda3::Room& room) {
  if (!project || !project->hack_manifest.loaded()) {
    return false;
  }
  const auto& tags = project->hack_manifest.room_tags();
  const bool has_source =
      std::any_of(tags.begin(), tags.end(), [](const auto& tag) {
        return tag.enabled && tag.address == 0x01CC08 &&
               std::string_view(tag.source)
                   .starts_with("Dungeons/crumblefloor_tag.asm:");
      });
  if (!has_source) {
    return false;
  }
  // USDASM Underworld_HandleRoomTags.vectors. Do not use the manifest tag_id:
  // older Oracle generators infer a linear dispatch order that is incorrect.
  for (const auto tag : {room.tag1(), room.tag2()}) {
    const int id = static_cast<int>(tag);
    if (id < 0 || id > 0x3F) {
      continue;
    }
    const auto vector = rom.ReadWord(0xC27D + id * 2);
    if (vector.ok() && *vector == 0xCC08) {
      return true;
    }
  }
  return false;
}

CrumbleFloorModel::Words ReadCrumbleFloorWords(const zelda3::Room& room) {
  CrumbleFloorModel::Words words{};
  const auto& layout = room.bg1_buffer();
  const auto& objects = room.object_bg1_buffer();
  const auto& coverage = objects.coverage_data();
  for (int y = 0; y < 32; ++y) {
    for (int x = 0; x < 32; ++x) {
      const size_t pixel = y * 16 * 512 + x * 16;
      const bool object_written =
          pixel < coverage.size() && coverage[pixel] != 0;
      words[y * 32 + x] =
          (object_written ? objects : layout).GetTileAt(x * 2, y * 2);
    }
  }
  return words;
}

void CrumbleFloorView::Sync(const Rom* rom, const project::YazeProject* project,
                            const zelda3::Room& room) {
  const bool supported = rom && HasOracleCrumbleTag(*rom, project, room);
  const auto words = ReadCrumbleFloorWords(room);
  if (rom_ != rom || project_ != project || room_id_ != room.id() ||
      words_ != words || supported_ != supported) {
    const bool had_route = !model_.route().empty();
    model_.Reset(words);
    rehearsing_ = false;
    message_ = had_route ? "Preview reset: room or floor data changed." : "";
  }
  rom_ = rom;
  project_ = project;
  room_id_ = room.id();
  supported_ = supported;
  words_ = words;
}

void CrumbleFloorView::DrawControls(bool interactive) {
  gui::AutoWidgetScope scope("Dungeon/Crumble");
  if (!supported_) {
    rehearsing_ = false;
    ImGui::TextWrapped(
        "Crumble preview needs an active Oracle crumble tag and its source "
        "manifest.");
    return;
  }
  const bool has_floor =
      std::any_of(words_.begin(), words_.end(), [](auto word) {
        const auto cell = ClassifyCrumbleFloorWord(word);
        return cell == CrumbleFloorCell::kIntact ||
               cell == CrumbleFloorCell::kCracked;
      });
  if (!has_floor) {
    rehearsing_ = false;
    ImGui::TextWrapped(
        "Crumble tag is active, but BG1 has no matching floor cells. "
        "Inspect or place a floor pattern before rehearsing.");
    return;
  }
  ImGui::BeginDisabled(!interactive);
  ImGui::Checkbox("Rehearse floor##crumble", &rehearsing_);
  gui::AutoRegisterLastItem("checkbox", "Rehearse",
                            "Read-only floor rehearsal");
  ImGui::SameLine();
  if (ImGui::SmallButton("Back##crumble")) {
    model_.Back();
    message_.clear();
  }
  gui::AutoRegisterLastItem("button", "Back");
  ImGui::SameLine();
  if (ImGui::SmallButton("Reset##crumble")) {
    model_.Reset(words_);
    message_.clear();
  }
  gui::AutoRegisterLastItem("button", "Reset");
  ImGui::EndDisabled();
  ImGui::TextWrapped(
      "Green: intact. Amber: cracked. Red: pit. Leaving a cell wears it; "
      "standing still does not.");
  if (rehearsing_) {
    ImGui::TextWrapped(
        "Click a highlighted cell, then adjacent cells. Escape exits. "
        "Floor-only model: collision, enemies, jumps, Minish and room entry "
        "are excluded. No ROM edits.");
  }
  if (!message_.empty()) {
    ImGui::TextWrapped("%s", message_.c_str());
  }
}

bool CrumbleFloorView::DrawCanvas(const gui::CanvasRuntime& rt,
                                  bool interactive, bool shortcut_focus) {
  if (!supported_ || !rt.draw_list) {
    return false;
  }
  const bool owns_input = rehearsing_ && interactive;
  if (owns_input && shortcut_focus && !ImGui::GetIO().WantTextInput &&
      ImGui::IsKeyPressed(ImGuiKey_Escape)) {
    rehearsing_ = false;
  }
  const DungeonCanvasTransform transform(rt.canvas_p0, rt.scrolling, rt.scale);
  const auto [x, y] = transform.ScreenToRoomTiles(ImGui::GetMousePos(), 16);
  if (owns_input && rehearsing_ && rt.hovered && ImGui::IsWindowHovered() &&
      ImGui::IsMouseClicked(ImGuiMouseButton_Left) &&
      !ImGui::IsMouseDown(ImGuiMouseButton_Middle) &&
      !ImGui::IsMouseDown(ImGuiMouseButton_Right)) {
    const auto status = model_.Step(x, y);
    message_ = status.ok() ? "" : std::string(status.message());
  }

  const auto& theme = AgentUI::GetTheme();
  rt.draw_list->PushClipRect(
      rt.canvas_p0,
      ImVec2(rt.canvas_p0.x + rt.canvas_sz.x, rt.canvas_p0.y + rt.canvas_sz.y),
      true);
  for (int i = 0; i < CrumbleFloorModel::kCellCount; ++i) {
    const auto cell = model_.cell(i);
    if (cell == CrumbleFloorCell::kOther) {
      continue;
    }
    const auto color = ImGui::GetColorU32(
        cell == CrumbleFloorCell::kIntact    ? theme.status_success
        : cell == CrumbleFloorCell::kCracked ? theme.status_warning
                                             : theme.status_error);
    const auto p =
        transform.RoomPixelsToScreen(ImVec2((i % 32) * 16, (i / 32) * 16));
    const auto q = transform.RoomPixelsToScreen(
        ImVec2((i % 32 + 1) * 16, (i / 32 + 1) * 16));
    rt.draw_list->AddRectFilled(p, q, (color & 0x00FFFFFF) | 0x33000000);
    rt.draw_list->AddRect(p, q, color);
  }
  const auto center = [&](int cell) {
    return transform.RoomPixelsToScreen(
        ImVec2((cell % 32) * 16 + 8, (cell / 32) * 16 + 8));
  };
  for (size_t i = 1; i < model_.route().size(); ++i) {
    rt.draw_list->AddLine(center(model_.route()[i - 1]),
                          center(model_.route()[i]),
                          ImGui::GetColorU32(ImGuiCol_Text), 2.0f);
  }
  if (model_.current()) {
    rt.draw_list->AddCircleFilled(center(*model_.current()),
                                  4.0f * transform.scale(),
                                  ImGui::GetColorU32(ImGuiCol_Text));
  }
  rt.draw_list->PopClipRect();
  return owns_input;
}

}  // namespace yaze::editor
