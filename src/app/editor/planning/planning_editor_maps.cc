// Plan Maps: the overworld layer of a yaze.plan/1 file (markers, notes,
// sketch lines and regions) drawn over a schematic grid of each world's areas.
// Works without a ROM; "Open map" hands off to the Overworld editor.

#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <string>
#include <vector>

#include "absl/strings/match.h"
#include "absl/strings/str_cat.h"
#include "absl/strings/str_format.h"
#include "absl/time/clock.h"
#include "absl/time/time.h"
#include "app/editor/planning/planning_editor.h"
#include "app/editor/planning/planning_widgets.h"
#include "app/gui/core/icons.h"
#include "app/gui/core/ui_helpers.h"
#include "imgui/imgui.h"
#include "imgui/misc/cpp/imgui_stdlib.h"

namespace yaze::editor {
namespace {

using planning::StatusCombo;

constexpr const char* kMarkerKinds[] = {"dungeon", "shrine", "place", "item",
                                        "planned"};
constexpr const char* kMarkerStatuses[] = {"implemented", "decided", "draft",
                                           "proposed", "idea"};
constexpr float kPickRadius = 8.0f;  // screen pixels
constexpr float kMinZoom = 0.05f;
constexpr int kNewRegionRadius = 256;

ImU32 ToU32(ImVec4 color, float alpha = 1.0f) {
  color.w *= alpha;
  return ImGui::ColorConvertFloat4ToU32(color);
}

ImVec4 MarkerColor(const std::string& status) {
  if (status == "implemented") {
    return gui::GetSuccessColor();
  }
  if (status == "decided") {
    return gui::GetInfoColor();
  }
  if (status == "draft" || status == "proposed") {
    return gui::GetWarningColor();
  }
  return gui::GetDisabledColor();
}

ImVec4 LineColor(const std::string& color) {
  if (color == "red") {
    return gui::GetErrorColor();
  }
  if (color == "green") {
    return gui::GetSuccessColor();
  }
  if (color == "blue") {
    return gui::GetInfoColor();
  }
  if (color == "yellow" || color == "orange") {
    return gui::GetWarningColor();
  }
  return gui::GetAccentColor();
}

float Distance2(float ax, float ay, float bx, float by) {
  return (ax - bx) * (ax - bx) + (ay - by) * (ay - by);
}

int ParseId(const std::string& text) {
  if (text.empty()) {
    return -1;
  }
  char* end = nullptr;
  const long value = std::strtol(text.c_str(), &end, 0);
  return (end == text.c_str() || *end != '\0' || value < 0)
             ? -1
             : static_cast<int>(value);
}

template <typename Exists>
std::string NextId(const std::string& prefix, Exists exists) {
  for (int n = 1;; ++n) {
    std::string id = absl::StrCat(prefix, n);
    if (!exists(id)) {
      return id;
    }
  }
}

std::string NowUtc() {
  return absl::FormatTime("%Y-%m-%dT%H:%M:%SZ", absl::Now(),
                          absl::UTCTimeZone());
}

}  // namespace

const core::PlanWorld* PlanningEditor::CurrentWorld() const {
  return plan_ ? plan_->overworld.FindWorld(map_world_) : nullptr;
}

void PlanningEditor::SelectMarker(int index) {
  if (!plan_ || index < 0 ||
      index >= static_cast<int>(plan_->overworld.markers.size())) {
    return;
  }
  map_world_ = plan_->overworld.markers[index].world;
  map_selection_ = {MapPick::kMarker, index};
  map_scroll_to_selection_ = true;
}

void PlanningEditor::OpenMapOf(const std::string& map,
                               const core::PlanWorld* world, int x, int y) {
  int id = ParseId(map);
  if (id < 0 && world) {
    id = world->MapAt(x, y);
  }
  if (id >= 0) {
    OpenTarget("map:" + core::FormatPlanId(id));
  }
}

void PlanningEditor::DrawMaps() {
  if (!DrawPlanHeader()) {
    return;
  }
  auto& ow = plan_->overworld;
  if (ow.worlds.empty()) {
    ImGui::TextWrapped(
        "This plan has no overworld worlds yet. Add one to place markers, "
        "notes and regions over its areas.");
    if (ImGui::Button(ICON_MD_ADD " Add a 64-area world")) {
      core::PlanWorld world;
      world.id = "light";
      world.title = "Light World";
      world.base = "0x00";
      ow.worlds.push_back(world);
      map_world_ = world.id;
      MarkDirty();
    }
    return;
  }
  if (!CurrentWorld()) {
    map_world_ = ow.worlds.front().id;
    map_selection_ = {};
  }
  DrawMapToolbar();
  const core::PlanWorld world = *CurrentWorld();
  if (ImGui::BeginTable(
          "##maps_layout", 2,
          ImGuiTableFlags_Resizable | ImGuiTableFlags_BordersInnerV)) {
    ImGui::TableSetupColumn("Map", ImGuiTableColumnFlags_WidthStretch, 0.68f);
    ImGui::TableSetupColumn("Detail", ImGuiTableColumnFlags_WidthStretch,
                            0.32f);
    ImGui::TableNextRow();
    ImGui::TableNextColumn();
    DrawMapCanvas(world);
    ImGui::TableNextColumn();
    DrawMapSelection();
    DrawMapList(world);
    ImGui::EndTable();
  }
}

void PlanningEditor::DrawMapToolbar() {
  const auto& worlds = plan_->overworld.worlds;
  for (size_t i = 0; i < worlds.size(); ++i) {
    const auto& w = worlds[i];
    if (i > 0) {
      ImGui::SameLine();
    }
    ImGui::PushID(static_cast<int>(i));
    const bool active = w.id == map_world_;
    if (active) {
      ImGui::PushStyleColor(ImGuiCol_Button,
                            ImGui::GetStyleColorVec4(ImGuiCol_ButtonActive));
    }
    if (ImGui::Button(w.title.empty() ? w.id.c_str() : w.title.c_str())) {
      map_world_ = w.id;
      map_selection_ = {};
      map_drag_ = {};
    }
    if (active) {
      ImGui::PopStyleColor();
    }
    if (ImGui::IsItemHovered()) {
      ImGui::SetTooltip("Maps %s-%s", core::FormatPlanId(w.BaseMap()).c_str(),
                        core::FormatPlanId(w.BaseMap() + w.maps - 1).c_str());
    }
    ImGui::PopID();
  }
  ImGui::Checkbox("Markers", &show_markers_);
  ImGui::SameLine();
  ImGui::Checkbox("Notes", &show_notes_);
  ImGui::SameLine();
  ImGui::Checkbox("Regions", &show_regions_);
  ImGui::SameLine();
  ImGui::Checkbox("Lines", &show_lines_);
  ImGui::SameLine();
  ImGui::SetNextItemWidth(120);
  ImGui::SliderFloat("##zoom", &map_zoom_, 0.0f, 1.0f,
                     map_zoom_ < kMinZoom ? "Fit width" : "Zoom %.2fx");
  ImGui::SameLine();
  if (ImGui::SmallButton(ICON_MD_FIT_SCREEN)) {
    map_zoom_ = 0.0f;
  }
  if (ImGui::IsItemHovered()) {
    ImGui::SetTooltip("Fit the world to the panel width");
  }
}

void PlanningEditor::DrawMapCanvas(const core::PlanWorld& world) {
  const auto& ow = plan_->overworld;
  const float world_w =
      static_cast<float>(core::kPlanWorldColumns * core::kPlanAreaSize);
  const float world_h = static_cast<float>(world.Rows() * core::kPlanAreaSize);

  ImGui::BeginChild(
      "##map_canvas",
      ImVec2(0, std::max(240.0f, ImGui::GetContentRegionAvail().y)),
      ImGuiChildFlags_Borders, ImGuiWindowFlags_HorizontalScrollbar);
  const float avail_w = std::max(64.0f, ImGui::GetContentRegionAvail().x);
  const float scale = map_zoom_ >= kMinZoom ? map_zoom_ : avail_w / world_w;
  const ImVec2 origin = ImGui::GetCursorScreenPos();
  ImGui::InvisibleButton(
      "##map_surface", ImVec2(world_w * scale, world_h * scale),
      ImGuiButtonFlags_MouseButtonLeft | ImGuiButtonFlags_MouseButtonRight);
  const bool hovered = ImGui::IsItemHovered();
  ImDrawList* draw = ImGui::GetWindowDrawList();
  auto at = [&](float x, float y) {
    return ImVec2(origin.x + x * scale, origin.y + y * scale);
  };

  const float cell = core::kPlanAreaSize * scale;
  const ImU32 fill_even = ImGui::GetColorU32(ImGuiCol_FrameBg);
  const ImU32 fill_odd = ImGui::GetColorU32(ImGuiCol_FrameBgHovered, 0.5f);
  const ImU32 border = ImGui::GetColorU32(ImGuiCol_Border);
  const ImU32 dim_text = ImGui::GetColorU32(ImGuiCol_TextDisabled);
  const ImU32 text = ImGui::GetColorU32(ImGuiCol_Text);
  const ImU32 outline = ImGui::GetColorU32(ImGuiCol_WindowBg);
  for (int i = 0; i < world.maps; ++i) {
    const int column = i % core::kPlanWorldColumns;
    const int row = i / core::kPlanWorldColumns;
    const ImVec2 a = at(static_cast<float>(column * core::kPlanAreaSize),
                        static_cast<float>(row * core::kPlanAreaSize));
    const ImVec2 b(a.x + cell, a.y + cell);
    draw->AddRectFilled(a, b, (column + row) % 2 ? fill_odd : fill_even);
    draw->AddRect(a, b, border);
    if (cell >= 36.0f) {
      draw->AddText(ImVec2(a.x + 3, a.y + 2), dim_text,
                    core::FormatPlanId(world.BaseMap() + i).c_str());
    }
  }

  if (show_regions_) {
    for (size_t i = 0; i < ow.regions.size(); ++i) {
      const auto& g = ow.regions[i];
      if (g.world != world.id) {
        continue;
      }
      const bool selected = map_selection_.kind == MapPick::kRegion &&
                            map_selection_.index == static_cast<int>(i);
      const ImVec4 color = MarkerColor(g.status);
      const ImVec2 center =
          at(static_cast<float>(g.x), static_cast<float>(g.y));
      const float radius = g.radius * scale;
      draw->AddCircleFilled(center, radius, ToU32(color, 0.15f));
      draw->AddCircle(center, radius, selected ? text : ToU32(color, 0.7f), 0,
                      selected ? 2.5f : 1.0f);
      if (radius >= 30.0f && !g.label.empty()) {
        const ImVec2 size = ImGui::CalcTextSize(g.label.c_str());
        draw->AddText(ImVec2(center.x - size.x / 2, center.y - size.y / 2),
                      text, g.label.c_str());
      }
    }
  }

  if (show_lines_) {
    std::vector<ImVec2> points;
    for (const auto& line : ow.lines) {
      if (line.world != world.id || line.points.size() < 2) {
        continue;
      }
      points.clear();
      for (const auto& [x, y] : line.points) {
        points.push_back(at(static_cast<float>(x), static_cast<float>(y)));
      }
      draw->AddPolyline(points.data(), static_cast<int>(points.size()),
                        ToU32(LineColor(line.color)), ImDrawFlags_None, 2.0f);
    }
  }

  if (show_notes_) {
    for (size_t i = 0; i < ow.notes.size(); ++i) {
      const auto& n = ow.notes[i];
      if (n.world != world.id) {
        continue;
      }
      const ImVec2 p = at(static_cast<float>(n.x), static_cast<float>(n.y));
      draw->AddRectFilled(ImVec2(p.x - 4, p.y - 4), ImVec2(p.x + 4, p.y + 4),
                          ToU32(gui::GetWarningColor()));
      if (map_selection_.kind == MapPick::kNote &&
          map_selection_.index == static_cast<int>(i)) {
        draw->AddRect(ImVec2(p.x - 7, p.y - 7), ImVec2(p.x + 7, p.y + 7), text,
                      0.0f, 0, 2.0f);
      }
    }
  }

  if (show_markers_) {
    for (size_t i = 0; i < ow.markers.size(); ++i) {
      const auto& m = ow.markers[i];
      if (m.world != world.id) {
        continue;
      }
      const ImVec2 p = at(static_cast<float>(m.x), static_cast<float>(m.y));
      const bool major = m.kind == "dungeon" || m.kind == "shrine";
      const float radius = major ? 6.0f : 4.0f;
      draw->AddCircleFilled(p, radius, ToU32(MarkerColor(m.status)));
      draw->AddCircle(p, radius, outline);
      if (!m.code.empty()) {
        draw->AddText(ImVec2(p.x + radius + 2, p.y - 7), text, m.code.c_str());
      }
      if (map_selection_.kind == MapPick::kMarker &&
          map_selection_.index == static_cast<int>(i)) {
        draw->AddCircle(p, radius + 4, text, 0, 2.0f);
      }
    }
  }

  if (map_scroll_to_selection_ && map_selection_.kind == MapPick::kMarker &&
      map_selection_.index < static_cast<int>(ow.markers.size())) {
    const auto& m = ow.markers[map_selection_.index];
    ImGui::SetScrollX(
        std::max(0.0f, m.x * scale - ImGui::GetWindowWidth() / 2));
    ImGui::SetScrollY(
        std::max(0.0f, m.y * scale - ImGui::GetWindowHeight() / 2));
  }
  map_scroll_to_selection_ = false;

  const ImVec2 mouse = ImGui::GetIO().MousePos;
  const float wx = (mouse.x - origin.x) / scale;
  const float wy = (mouse.y - origin.y) / scale;
  HandleMapInput(world, wx, wy, scale, hovered);
  DrawMapContextMenu(world);

  if (hovered && map_drag_.kind == MapPick::kNone &&
      !ImGui::IsPopupOpen("##map_context")) {
    const MapHit hit = HitTest(world, wx, wy, kPickRadius / scale);
    const int area = world.MapAt(static_cast<int>(wx), static_cast<int>(wy));
    const std::string area_text = area >= 0 ? core::FormatPlanId(area) : "-";
    if (hit.kind == MapPick::kMarker) {
      const auto& m = ow.markers[hit.index];
      ImGui::SetTooltip("%s%s%s\n%s\n%s · map %s", m.code.c_str(),
                        m.code.empty() ? "" : "  ", m.label.c_str(),
                        m.detail.c_str(), m.status.c_str(),
                        m.map.empty() ? area_text.c_str() : m.map.c_str());
    } else if (hit.kind == MapPick::kNote) {
      ImGui::SetTooltip("%s", ow.notes[hit.index].text.c_str());
    } else if (hit.kind == MapPick::kRegion) {
      const auto& g = ow.regions[hit.index];
      ImGui::SetTooltip("%s\n%s", g.label.c_str(), g.detail.c_str());
    } else {
      ImGui::SetTooltip("Map %s", area_text.c_str());
    }
  }
  ImGui::EndChild();
}

PlanningEditor::MapHit PlanningEditor::HitTest(const core::PlanWorld& world,
                                               float wx, float wy,
                                               float radius) const {
  const auto& ow = plan_->overworld;
  MapHit hit;
  float best = radius * radius;
  if (show_notes_) {
    for (size_t i = 0; i < ow.notes.size(); ++i) {
      const auto& n = ow.notes[i];
      const float d =
          Distance2(wx, wy, static_cast<float>(n.x), static_cast<float>(n.y));
      if (n.world == world.id && d <= best) {
        best = d;
        hit = {MapPick::kNote, static_cast<int>(i)};
      }
    }
  }
  if (show_markers_) {
    for (size_t i = 0; i < ow.markers.size(); ++i) {
      const auto& m = ow.markers[i];
      const float d =
          Distance2(wx, wy, static_cast<float>(m.x), static_cast<float>(m.y));
      if (m.world == world.id && d <= best) {
        best = d;
        hit = {MapPick::kMarker, static_cast<int>(i)};
      }
    }
  }
  if (hit.kind == MapPick::kNone && show_regions_) {
    int smallest = 0;
    for (size_t i = 0; i < ow.regions.size(); ++i) {
      const auto& g = ow.regions[i];
      const float d =
          Distance2(wx, wy, static_cast<float>(g.x), static_cast<float>(g.y));
      if (g.world == world.id &&
          d <= static_cast<float>(g.radius) * static_cast<float>(g.radius) &&
          (hit.kind == MapPick::kNone || g.radius < smallest)) {
        smallest = g.radius;
        hit = {MapPick::kRegion, static_cast<int>(i)};
      }
    }
  }
  return hit;
}

void PlanningEditor::HandleMapInput(const core::PlanWorld& world, float wx,
                                    float wy, float scale, bool hovered) {
  auto& ow = plan_->overworld;
  const int max_x = core::kPlanWorldColumns * core::kPlanAreaSize - 1;
  const int max_y = world.Rows() * core::kPlanAreaSize - 1;
  const int x = std::clamp(static_cast<int>(std::lround(wx)), 0, max_x);
  const int y = std::clamp(static_cast<int>(std::lround(wy)), 0, max_y);

  auto position = [&](const MapHit& hit, int** px, int** py,
                      std::string** map) {
    if (hit.kind == MapPick::kMarker &&
        hit.index < static_cast<int>(ow.markers.size())) {
      auto& m = ow.markers[hit.index];
      *px = &m.x;
      *py = &m.y;
      *map = &m.map;
      return true;
    }
    if (hit.kind == MapPick::kNote &&
        hit.index < static_cast<int>(ow.notes.size())) {
      auto& n = ow.notes[hit.index];
      *px = &n.x;
      *py = &n.y;
      *map = &n.map;
      return true;
    }
    return false;
  };

  if (hovered && ImGui::IsMouseClicked(ImGuiMouseButton_Left)) {
    map_selection_ = HitTest(world, wx, wy, kPickRadius / scale);
    int* px = nullptr;
    int* py = nullptr;
    std::string* map = nullptr;
    if (position(map_selection_, &px, &py, &map)) {
      map_drag_ = map_selection_;
      map_drag_from_x_ = *px;
      map_drag_from_y_ = *py;
    }
  }

  int* px = nullptr;
  int* py = nullptr;
  std::string* map = nullptr;
  if (position(map_drag_, &px, &py, &map)) {
    if (ImGui::IsMouseDown(ImGuiMouseButton_Left)) {
      if (ImGui::IsMouseDragging(ImGuiMouseButton_Left, 2.0f)) {
        *px = x;
        *py = y;
      }
    } else {
      if (*px != map_drag_from_x_ || *py != map_drag_from_y_) {
        *map = world.MapAfterMove(*map, map_drag_from_x_, map_drag_from_y_, *px,
                                  *py);
        MarkDirty();
      }
      map_drag_ = {};
    }
  } else {
    map_drag_ = {};
  }

  if (hovered && ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left) &&
      HitTest(world, wx, wy, kPickRadius / scale).kind == MapPick::kNone) {
    core::PlanNote note;
    note.id = NextId("note-", [&](const std::string& id) {
      return std::any_of(ow.notes.begin(), ow.notes.end(),
                         [&](const auto& n) { return n.id == id; });
    });
    note.world = world.id;
    note.x = x;
    note.y = y;
    note.map = core::FormatPlanId(world.MapAt(x, y));
    note.text = "New note";
    note.created = NowUtc();
    ow.notes.push_back(std::move(note));
    map_selection_ = {MapPick::kNote, static_cast<int>(ow.notes.size()) - 1};
    MarkDirty();
  }

  if (hovered && ImGui::IsMouseClicked(ImGuiMouseButton_Right)) {
    map_context_hit_ = HitTest(world, wx, wy, kPickRadius / scale);
    map_context_x_ = x;
    map_context_y_ = y;
    ImGui::OpenPopup("##map_context");
  }
}

void PlanningEditor::DrawMapContextMenu(const core::PlanWorld& world) {
  if (!ImGui::BeginPopup("##map_context")) {
    return;
  }
  auto& ow = plan_->overworld;
  const int x = map_context_x_;
  const int y = map_context_y_;
  const int area = world.MapAt(x, y);
  ImGui::TextDisabled("Map %s  (%d, %d)",
                      area >= 0 ? core::FormatPlanId(area).c_str() : "-", x, y);
  ImGui::Separator();
  const MapHit hit = map_context_hit_;
  bool removed = false;
  if (hit.kind == MapPick::kMarker &&
      hit.index < static_cast<int>(ow.markers.size())) {
    const auto& m = ow.markers[hit.index];
    ImGui::TextUnformatted(m.label.c_str());
    if (ImGui::MenuItem(ICON_MD_MAP " Open map in Overworld editor")) {
      OpenMapOf(m.map, &world, m.x, m.y);
    }
    if (!m.room.empty() && ImGui::MenuItem(ICON_MD_DOOR_FRONT " Open room")) {
      OpenTarget("room:" + m.room);
    }
    if (ImGui::MenuItem(ICON_MD_DELETE " Delete marker")) {
      ow.markers.erase(ow.markers.begin() + hit.index);
      removed = true;
    }
  } else if (hit.kind == MapPick::kNote &&
             hit.index < static_cast<int>(ow.notes.size())) {
    const auto& n = ow.notes[hit.index];
    if (ImGui::MenuItem(ICON_MD_MAP " Open map in Overworld editor")) {
      OpenMapOf(n.map, &world, n.x, n.y);
    }
    if (ImGui::MenuItem(ICON_MD_DELETE " Delete note")) {
      ow.notes.erase(ow.notes.begin() + hit.index);
      removed = true;
    }
  } else if (hit.kind == MapPick::kRegion &&
             hit.index < static_cast<int>(ow.regions.size())) {
    ImGui::TextUnformatted(ow.regions[hit.index].label.c_str());
    if (ImGui::MenuItem(ICON_MD_DELETE " Delete region")) {
      ow.regions.erase(ow.regions.begin() + hit.index);
      removed = true;
    }
  } else {
    if (ImGui::MenuItem(ICON_MD_PLACE " Add marker here")) {
      core::PlanMarker marker;
      marker.id = NextId(world.id + ":m", [&](const std::string& id) {
        return ow.FindMarker(id) >= 0;
      });
      marker.world = world.id;
      marker.x = x;
      marker.y = y;
      marker.map = area >= 0 ? core::FormatPlanId(area) : "";
      marker.kind = "planned";
      marker.label = "New marker";
      marker.status = "idea";
      ow.markers.push_back(std::move(marker));
      map_selection_ = {MapPick::kMarker,
                        static_cast<int>(ow.markers.size()) - 1};
      MarkDirty();
    }
    if (ImGui::MenuItem(ICON_MD_CIRCLE " Add region here")) {
      core::PlanRegion region;
      region.id = NextId("region-", [&](const std::string& id) {
        return std::any_of(ow.regions.begin(), ow.regions.end(),
                           [&](const auto& g) { return g.id == id; });
      });
      region.world = world.id;
      region.x = x;
      region.y = y;
      region.radius = kNewRegionRadius;
      region.label = "New region";
      region.status = "idea";
      ow.regions.push_back(std::move(region));
      map_selection_ = {MapPick::kRegion,
                        static_cast<int>(ow.regions.size()) - 1};
      MarkDirty();
    }
    if (area >= 0 &&
        ImGui::MenuItem(ICON_MD_MAP " Open map in Overworld editor")) {
      OpenMapOf("", &world, x, y);
    }
  }
  if (removed) {
    map_selection_ = {};
    map_drag_ = {};
    MarkDirty();
  }
  ImGui::EndPopup();
}

void PlanningEditor::DrawMapSelection() {
  auto& ow = plan_->overworld;
  const core::PlanWorld* world = CurrentWorld();
  const MapHit sel = map_selection_;
  ImGui::PushID("map_selection");
  if (sel.kind == MapPick::kMarker &&
      sel.index < static_cast<int>(ow.markers.size())) {
    auto& m = ow.markers[sel.index];
    ImGui::SeparatorText(ICON_MD_PLACE " Marker");
    if (ImGui::InputText("Label", &m.label)) {
      MarkDirty();
    }
    if (ImGui::InputTextWithHint("Code", "D1, S2 (area: targets)", &m.code)) {
      MarkDirty();
    }
    if (StatusCombo("Kind", &m.kind, kMarkerKinds)) {
      MarkDirty();
    }
    if (StatusCombo("Status", &m.status, kMarkerStatuses)) {
      MarkDirty();
    }
    if (ImGui::InputTextWithHint("Room", "0x4A", &m.room)) {
      MarkDirty();
    }
    if (ImGui::InputTextMultiline("##detail", &m.detail,
                                  ImVec2(-1, ImGui::GetTextLineHeight() * 3))) {
      MarkDirty();
    }
    ImGui::TextDisabled("Map %s at (%d, %d) · id %s",
                        m.map.empty() ? "-" : m.map.c_str(), m.x, m.y,
                        m.id.c_str());
    if (ImGui::SmallButton(ICON_MD_MAP " Open map")) {
      OpenMapOf(m.map, world, m.x, m.y);
    }
    if (ParseId(m.room) >= 0) {
      ImGui::SameLine();
      if (ImGui::SmallButton(ICON_MD_DOOR_FRONT " Open room")) {
        OpenTarget("room:" + m.room);
      }
    }
    ImGui::SameLine();
    if (ImGui::SmallButton(ICON_MD_DELETE " Delete")) {
      ow.markers.erase(ow.markers.begin() + sel.index);
      map_selection_ = {};
      map_drag_ = {};
      MarkDirty();
    }
  } else if (sel.kind == MapPick::kNote &&
             sel.index < static_cast<int>(ow.notes.size())) {
    auto& n = ow.notes[sel.index];
    ImGui::SeparatorText(ICON_MD_STICKY_NOTE_2 " Note");
    if (ImGui::InputTextMultiline("##note_text", &n.text,
                                  ImVec2(-1, ImGui::GetTextLineHeight() * 4))) {
      MarkDirty();
    }
    ImGui::TextDisabled("Map %s at (%d, %d) · %s",
                        n.map.empty() ? "-" : n.map.c_str(), n.x, n.y,
                        n.created.c_str());
    if (ImGui::SmallButton(ICON_MD_MAP " Open map")) {
      OpenMapOf(n.map, world, n.x, n.y);
    }
    ImGui::SameLine();
    if (ImGui::SmallButton(ICON_MD_DELETE " Delete")) {
      ow.notes.erase(ow.notes.begin() + sel.index);
      map_selection_ = {};
      map_drag_ = {};
      MarkDirty();
    }
  } else if (sel.kind == MapPick::kRegion &&
             sel.index < static_cast<int>(ow.regions.size())) {
    auto& g = ow.regions[sel.index];
    ImGui::SeparatorText(ICON_MD_CIRCLE " Region");
    if (ImGui::InputText("Label", &g.label)) {
      MarkDirty();
    }
    if (StatusCombo("Status", &g.status, kMarkerStatuses)) {
      MarkDirty();
    }
    if (ImGui::DragInt("Radius", &g.radius, 4.0f, 16, 2048)) {
      MarkDirty();
    }
    if (ImGui::InputTextMultiline("##region_detail", &g.detail,
                                  ImVec2(-1, ImGui::GetTextLineHeight() * 4))) {
      MarkDirty();
    }
    if (ImGui::SmallButton(ICON_MD_DELETE " Delete")) {
      ow.regions.erase(ow.regions.begin() + sel.index);
      map_selection_ = {};
      MarkDirty();
    }
  } else {
    ImGui::TextWrapped(
        "Click a marker, note or region to edit it; drag markers and notes "
        "to move them. Double-click the map to add a note, right-click for "
        "more.");
  }
  ImGui::PopID();
}

void PlanningEditor::DrawMapList(const core::PlanWorld& world) {
  const auto& ow = plan_->overworld;
  int count = 0;
  for (const auto& m : ow.markers) {
    count += m.world == world.id ? 1 : 0;
  }
  ImGui::SeparatorText(absl::StrFormat("Markers (%d)", count).c_str());
  ImGui::SetNextItemWidth(-1);
  ImGui::InputTextWithHint("##map_filter", "Filter by label or code",
                           &map_filter_);
  ImGui::BeginChild("##marker_list");
  for (size_t i = 0; i < ow.markers.size(); ++i) {
    const auto& m = ow.markers[i];
    if (m.world != world.id ||
        (!map_filter_.empty() &&
         !absl::StrContainsIgnoreCase(m.label, map_filter_) &&
         !absl::StrContainsIgnoreCase(m.code, map_filter_))) {
      continue;
    }
    ImGui::PushID(static_cast<int>(i));
    const bool selected = map_selection_.kind == MapPick::kMarker &&
                          map_selection_.index == static_cast<int>(i);
    gui::ColoredText(ICON_MD_CIRCLE, MarkerColor(m.status));
    ImGui::SameLine();
    if (ImGui::Selectable(absl::StrFormat("%s%s%s", m.code,
                                          m.code.empty() ? "" : "  ", m.label)
                              .c_str(),
                          selected)) {
      SelectMarker(static_cast<int>(i));
    }
    ImGui::PopID();
  }

  auto unplaced = ow.extra.find("unplaced");
  if (unplaced != ow.extra.end() && unplaced->is_object()) {
    auto list = unplaced->find(world.id);
    if (list != unplaced->end() && list->is_array() && !list->empty() &&
        ImGui::TreeNode("##unplaced", "Not placed yet (%zu)", list->size())) {
      for (const auto& entry : *list) {
        if (entry.is_string()) {
          ImGui::BulletText("%s", entry.get<std::string>().c_str());
        }
      }
      ImGui::TreePop();
    }
  }
  ImGui::EndChild();
}

}  // namespace yaze::editor
