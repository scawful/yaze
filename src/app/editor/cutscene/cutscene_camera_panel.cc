#include "app/editor/cutscene/cutscene_camera_panel.h"

#include <algorithm>
#include <cmath>
#include <fstream>
#include <iterator>
#include <vector>

#include "absl/strings/str_format.h"
#include "app/editor/registry/content_registry.h"
#include "app/editor/registry/panel_registration.h"
#include "app/gui/core/ui_helpers.h"
#include "core/project.h"
#include "core/source_artifact_publisher.h"
#include "imgui/imgui.h"

namespace yaze::editor {

REGISTER_PANEL(CutsceneCameraPanel);

namespace {

constexpr float kMarkerRadius = 5.0f;
constexpr const char* kSizeLabels[] = {"Vanilla layout", "512 x 512",
                                       "1024 x 1024", "1024 x 512 (wide)",
                                       "512 x 1024 (tall)"};

ImU32 ToU32(ImVec4 color, float alpha = 1.0f) {
  color.w *= alpha;
  return ImGui::ColorConvertFloat4ToU32(color);
}

bool InputHex(const char* label, int* value, int max) {
  ImGui::SetNextItemWidth(90.0f);
  const bool changed = ImGui::InputInt(label, value, 1, 16,
                                       ImGuiInputTextFlags_CharsHexadecimal);
  *value = std::clamp(*value, 0, max);
  return changed;
}

bool InputWorld(const char* label, int* value) {
  ImGui::SetNextItemWidth(110.0f);
  const bool changed = ImGui::InputInt(label, value, 1, 8);
  *value = std::clamp(*value, 0, 0xFFFF);
  return changed;
}

}  // namespace

std::optional<std::filesystem::path> CutsceneCameraPanel::ShotsPath() const {
  const auto* project = ContentRegistry::Context::current_project();
  if (project == nullptr || !project->project_opened() ||
      project->cutscene_shots.empty()) {
    return std::nullopt;
  }
  return std::filesystem::path(
      project->GetAbsolutePath(project->cutscene_shots));
}

void CutsceneCameraPanel::Load() {
  loaded_once_ = true;
  dirty_ = false;
  shots_ = {};
  selected_ = -1;
  loaded_bytes_.reset();
  const auto path = ShotsPath();
  if (!path.has_value()) {
    loaded_path_.clear();
    status_ =
        "No [files] cutscene_shots in the project: read-only, use Copy JSON.";
    status_is_error_ = false;
    return;
  }
  loaded_path_ = path->string();
  std::ifstream in(*path, std::ios::binary);
  if (!in) {
    loaded_bytes_ = std::nullopt;  // Saving will create it.
    status_ = absl::StrFormat("%s does not exist yet; Save creates it.",
                              loaded_path_);
    status_is_error_ = false;
    return;
  }
  const std::string bytes((std::istreambuf_iterator<char>(in)), {});
  auto parsed = zelda3::ParseCutsceneShots(bytes);
  if (!parsed.ok()) {
    status_ = std::string(parsed.status().message());
    status_is_error_ = true;
    return;
  }
  shots_ = std::move(*parsed);
  loaded_bytes_ = bytes;
  selected_ = shots_.shots.empty() ? -1 : 0;
  status_ = absl::StrFormat("Loaded %zu shot(s) from %s", shots_.shots.size(),
                            loaded_path_);
  status_is_error_ = false;
}

void CutsceneCameraPanel::Save() {
  const auto path = ShotsPath();
  if (!path.has_value()) {
    return;
  }
  auto text = zelda3::SerializeCutsceneShots(shots_);
  if (!text.ok()) {
    status_ = std::string(text.status().message());
    status_is_error_ = true;
    return;
  }
  std::error_code ec;
  std::filesystem::create_directories(path->parent_path(), ec);
  const core::SourceArtifactPublisherLabels labels{
      .subject = "cutscene shots", .published_file = "cutscene shots"};
  auto lock = core::AcquireSourceArtifactPublicationLock({*path}, labels);
  if (!lock.ok()) {
    status_ = std::string(lock.status().message());
    status_is_error_ = true;
    return;
  }
  // Refuse to overwrite edits made outside yaze since the last load.
  std::ifstream in(*path, std::ios::binary);
  std::optional<std::string> on_disk;
  if (in) {
    on_disk = std::string((std::istreambuf_iterator<char>(in)), {});
  }
  if (on_disk != loaded_bytes_) {
    status_ = absl::StrFormat(
        "%s changed outside yaze since it was loaded. Reload before saving.",
        path->string());
    status_is_error_ = true;
    return;
  }
  absl::Status status;
  if (on_disk.has_value()) {
    std::vector<core::SourceArtifactUpdate> updates;
    updates.push_back(core::SourceArtifactUpdate{
        .target = *path, .before = on_disk, .after = *text});
    status = core::PublishSourceArtifacts(
        **lock, std::move(updates),
        core::ComputeSourceArtifactSha256(*on_disk));
  } else {
    // First save: the publisher replaces existing files only. Write a temp
    // file beside the target and move it into place while holding the lock.
    const std::filesystem::path temp =
        path->parent_path() / (path->filename().string() + ".yaze-new");
    {
      std::ofstream out(temp, std::ios::binary | std::ios::trunc);
      out << *text;
      if (!out) {
        status = absl::InternalError(
            absl::StrFormat("Could not write %s", temp.string()));
      }
    }
    if (status.ok()) {
      std::filesystem::rename(temp, *path, ec);
      if (ec) {
        status = absl::InternalError(absl::StrFormat(
            "Could not create %s: %s", path->string(), ec.message()));
      }
    }
    std::filesystem::remove(temp, ec);
  }
  if (!status.ok()) {
    status_ = std::string(status.message());
    status_is_error_ = true;
    return;
  }
  loaded_bytes_ = *text;
  dirty_ = false;
  status_ = absl::StrFormat("Saved %zu shot(s) to %s", shots_.shots.size(),
                            path->string());
  status_is_error_ = false;
}

zelda3::AreaExtent CutsceneCameraPanel::ExtentFor(
    const zelda3::CutsceneShot& shot) const {
  const int area = shot.area & 0x3F;
  const int grid_x = (area % 8) * 512;
  const int grid_y = (area / 8) * 512;
  switch (size_override_) {
    case 1:
      return {grid_x, grid_y, 512, 512};
    case 2:
      return {grid_x, grid_y, 1024, 1024};
    case 3:
      return {grid_x, grid_y, 1024, 512};
    case 4:
      return {grid_x, grid_y, 512, 1024};
    default: {
      auto extent = zelda3::VanillaAreaExtent(shot.area);
      return extent.ok() ? *extent : zelda3::AreaExtent{grid_x, grid_y};
    }
  }
}

void CutsceneCameraPanel::Draw(bool* p_open) {
  (void)p_open;
  if (!loaded_once_) {
    Load();
  }
  const auto path = ShotsPath();
  const bool writable = path.has_value();

  if (ImGui::Button(ICON_MD_REFRESH " Reload")) {
    Load();
  }
  ImGui::SameLine();
  ImGui::BeginDisabled(!writable || !dirty_);
  if (ImGui::Button(ICON_MD_SAVE " Save")) {
    Save();
  }
  ImGui::EndDisabled();
  ImGui::SameLine();
  if (ImGui::Button(ICON_MD_CONTENT_COPY " Copy JSON")) {
    auto text = zelda3::SerializeCutsceneShots(shots_);
    if (text.ok()) {
      ImGui::SetClipboardText(text->c_str());
      status_ = "Copied the shots JSON to the clipboard.";
      status_is_error_ = false;
    } else {
      status_ = std::string(text.status().message());
      status_is_error_ = true;
    }
  }
  ImGui::SameLine();
  ImGui::TextColored(
      status_is_error_ ? gui::GetErrorColor() : gui::GetDisabledColor(), "%s",
      status_.c_str());
  ImGui::Separator();

  if (ImGui::BeginTable(
          "##cutscene_layout", 2,
          ImGuiTableFlags_Resizable | ImGuiTableFlags_BordersInnerV)) {
    ImGui::TableSetupColumn("Shots", ImGuiTableColumnFlags_WidthFixed, 300.0f);
    ImGui::TableSetupColumn("Frame", ImGuiTableColumnFlags_WidthStretch);
    ImGui::TableNextRow();
    ImGui::TableNextColumn();
    DrawShotList();
    if (selected_ >= 0 && selected_ < static_cast<int>(shots_.shots.size())) {
      DrawShotFields(shots_.shots[selected_]);
    }
    ImGui::TableNextColumn();
    if (selected_ >= 0 && selected_ < static_cast<int>(shots_.shots.size())) {
      DrawCanvas(shots_.shots[selected_]);
    } else {
      ImGui::TextDisabled("Add or select a shot.");
    }
    ImGui::EndTable();
  }
}

void CutsceneCameraPanel::DrawShotList() {
  for (int i = 0; i < static_cast<int>(shots_.shots.size()); ++i) {
    ImGui::PushID(i);
    const auto& shot = shots_.shots[i];
    const std::string label =
        absl::StrFormat("%s  (area 0x%02X)", shot.name, shot.area);
    if (ImGui::Selectable(label.c_str(), selected_ == i)) {
      selected_ = i;
    }
    ImGui::PopID();
  }
  if (ImGui::Button(ICON_MD_ADD " Add shot")) {
    zelda3::CutsceneShot shot;
    int n = static_cast<int>(shots_.shots.size()) + 1;
    auto taken = [&](const std::string& name) {
      return std::any_of(shots_.shots.begin(), shots_.shots.end(),
                         [&](const auto& s) { return s.name == name; });
    };
    while (taken(absl::StrFormat("shot_%d", n)))
      ++n;
    shot.name = absl::StrFormat("shot_%d", n);
    shot.area = selected_ >= 0 ? shots_.shots[selected_].area : 0x2D;
    const auto extent = ExtentFor(shot);
    shot.camera_x = extent.origin_x;
    shot.camera_y = extent.origin_y;
    shot.link = {extent.origin_x + 128, extent.origin_y + 112, 2};
    shots_.shots.push_back(shot);
    selected_ = static_cast<int>(shots_.shots.size()) - 1;
    dirty_ = true;
  }
  ImGui::SameLine();
  ImGui::BeginDisabled(selected_ < 0);
  if (ImGui::Button(ICON_MD_DELETE " Delete")) {
    shots_.shots.erase(shots_.shots.begin() + selected_);
    selected_ = std::min(selected_, static_cast<int>(shots_.shots.size()) - 1);
    dirty_ = true;
  }
  ImGui::EndDisabled();
  ImGui::Separator();
}

void CutsceneCameraPanel::DrawShotFields(zelda3::CutsceneShot& shot) {
  char name[64];
  absl::SNPrintF(name, sizeof(name), "%s", shot.name);
  ImGui::SetNextItemWidth(200.0f);
  if (ImGui::InputText("Name", name, sizeof(name),
                       ImGuiInputTextFlags_CharsNoBlank)) {
    shot.name = name;
    dirty_ = true;
  }
  dirty_ |= InputHex("Area", &shot.area, 0x7F);
  ImGui::SetNextItemWidth(200.0f);
  ImGui::Combo("Area size", &size_override_, kSizeLabels,
               IM_ARRAYSIZE(kSizeLabels));

  const auto extent = ExtentFor(shot);
  const auto camera = zelda3::ClampCamera(extent, shot.camera_x, shot.camera_y);
  ImGui::SeparatorText("Camera");
  dirty_ |= InputWorld("X##cam", &shot.camera_x);
  dirty_ |= InputWorld("Y##cam", &shot.camera_y);
  ImGui::Text("$E2 = 0x%04X  $E8 = 0x%04X", camera.x, camera.y);
  if (camera.clamped()) {
    ImGui::TextColored(gui::GetWarningColor(),
                       "Requested %d, %d is outside the area's camera limits; "
                       "the game shows %d, %d.",
                       camera.requested_x, camera.requested_y, camera.x,
                       camera.y);
    if (ImGui::SmallButton("Use clamped position")) {
      shot.camera_x = camera.x;
      shot.camera_y = camera.y;
      dirty_ = true;
    }
  }

  ImGui::SeparatorText("Link");
  dirty_ |= InputWorld("X##link", &shot.link.x);
  dirty_ |= InputWorld("Y##link", &shot.link.y);
  ImGui::SetNextItemWidth(110.0f);
  dirty_ |= ImGui::SliderInt("Facing##link", &shot.link.facing, 0, 3);

  ImGui::SeparatorText("Actors");
  for (int i = 0; i < static_cast<int>(shot.actors.size()); ++i) {
    ImGui::PushID(i);
    auto& actor = shot.actors[i];
    dirty_ |= InputHex("Sprite", &actor.sprite, 0xFF);
    dirty_ |= InputWorld("X", &actor.x);
    ImGui::SameLine();
    dirty_ |= InputWorld("Y", &actor.y);
    ImGui::SetNextItemWidth(110.0f);
    dirty_ |= ImGui::SliderInt("Facing", &actor.facing, 0, 3);
    char note[96];
    absl::SNPrintF(note, sizeof(note), "%s", actor.note);
    ImGui::SetNextItemWidth(200.0f);
    if (ImGui::InputText("Note", note, sizeof(note))) {
      actor.note = note;
      dirty_ = true;
    }
    if (ImGui::SmallButton(ICON_MD_DELETE " Remove actor")) {
      shot.actors.erase(shot.actors.begin() + i);
      dirty_ = true;
      ImGui::PopID();
      break;
    }
    ImGui::Separator();
    ImGui::PopID();
  }
  if (ImGui::Button(ICON_MD_PERSON_ADD " Add actor")) {
    shot.actors.push_back({0, camera.x + 128, camera.y + 112, 0, ""});
    dirty_ = true;
  }
}

void CutsceneCameraPanel::DrawCanvas(zelda3::CutsceneShot& shot) {
  const auto extent = ExtentFor(shot);
  const float avail = std::max(ImGui::GetContentRegionAvail().x, 128.0f);
  const float scale = std::min(1.0f, avail / static_cast<float>(extent.width));
  const ImVec2 size(extent.width * scale, extent.height * scale);
  const ImVec2 origin = ImGui::GetCursorScreenPos();
  ImDrawList* draw = ImGui::GetWindowDrawList();

  ImGui::InvisibleButton("##cutscene_canvas", size);
  const bool hovered = ImGui::IsItemHovered();
  const ImVec2 mouse = ImGui::GetIO().MousePos;
  const int mouse_world_x =
      extent.origin_x +
      static_cast<int>(std::floor((mouse.x - origin.x) / scale));
  const int mouse_world_y =
      extent.origin_y +
      static_cast<int>(std::floor((mouse.y - origin.y) / scale));

  // Area background and 512-pixel screen seams.
  draw->AddRectFilled(origin, ImVec2(origin.x + size.x, origin.y + size.y),
                      ToU32(ImGui::GetStyleColorVec4(ImGuiCol_FrameBg)));
  for (int seam = 512; seam < extent.width; seam += 512) {
    draw->AddLine(ImVec2(origin.x + seam * scale, origin.y),
                  ImVec2(origin.x + seam * scale, origin.y + size.y),
                  ToU32(gui::GetDisabledColor(), 0.5f));
  }
  for (int seam = 512; seam < extent.height; seam += 512) {
    draw->AddLine(ImVec2(origin.x, origin.y + seam * scale),
                  ImVec2(origin.x + size.x, origin.y + seam * scale),
                  ToU32(gui::GetDisabledColor(), 0.5f));
  }

  const auto camera = zelda3::ClampCamera(extent, shot.camera_x, shot.camera_y);
  const auto view = zelda3::ViewportOnMap(extent, camera.x, camera.y, scale);
  const ImVec2 view_min(origin.x + view.x, origin.y + view.y);
  const ImVec2 view_max(view_min.x + view.width, view_min.y + view.height);

  auto marker_pos = [&](int x, int y) {
    const auto p = zelda3::PointOnMap(extent, x, y, scale);
    return ImVec2(origin.x + p.x, origin.y + p.y);
  };
  auto near = [&](ImVec2 p) {
    const float dx = mouse.x - p.x;
    const float dy = mouse.y - p.y;
    return dx * dx + dy * dy <= (kMarkerRadius + 3) * (kMarkerRadius + 3);
  };

  // Start a drag: markers win over the viewport.
  if (hovered && ImGui::IsMouseClicked(ImGuiMouseButton_Left)) {
    drag_ = Drag::kNone;
    if (near(marker_pos(shot.link.x, shot.link.y))) {
      drag_ = Drag::kLink;
    } else {
      for (int i = 0; i < static_cast<int>(shot.actors.size()); ++i) {
        if (near(marker_pos(shot.actors[i].x, shot.actors[i].y))) {
          drag_ = Drag::kActor;
          drag_actor_ = i;
          break;
        }
      }
    }
    if (drag_ == Drag::kNone && mouse.x >= view_min.x && mouse.x < view_max.x &&
        mouse.y >= view_min.y && mouse.y < view_max.y) {
      drag_ = Drag::kViewport;
    }
  }
  if (!ImGui::IsMouseDown(ImGuiMouseButton_Left)) {
    drag_ = Drag::kNone;
  }
  if (drag_ != Drag::kNone &&
      ImGui::IsMouseDragging(ImGuiMouseButton_Left, 0)) {
    const ImVec2 delta = ImGui::GetMouseDragDelta(ImGuiMouseButton_Left, 0);
    const int dx = static_cast<int>(std::lround(delta.x / scale));
    const int dy = static_cast<int>(std::lround(delta.y / scale));
    if (dx != 0 || dy != 0) {
      auto move = [&](int& x, int& y) {
        x = std::clamp(x + dx, 0, 0xFFFF);
        y = std::clamp(y + dy, 0, 0xFFFF);
      };
      if (drag_ == Drag::kViewport) {
        const auto moved =
            zelda3::ClampCamera(extent, camera.x + dx, camera.y + dy);
        shot.camera_x = moved.x;
        shot.camera_y = moved.y;
      } else if (drag_ == Drag::kLink) {
        move(shot.link.x, shot.link.y);
      } else if (drag_actor_ >= 0 &&
                 drag_actor_ < static_cast<int>(shot.actors.size())) {
        move(shot.actors[drag_actor_].x, shot.actors[drag_actor_].y);
      }
      ImGui::ResetMouseDragDelta(ImGuiMouseButton_Left);
      dirty_ = true;
    }
  }
  // Arrow keys nudge the camera: 1 pixel, 8 with Shift.
  if (hovered) {
    const int step = ImGui::GetIO().KeyShift ? 8 : 1;
    int nx = shot.camera_x;
    int ny = shot.camera_y;
    if (ImGui::IsKeyPressed(ImGuiKey_LeftArrow))
      nx -= step;
    if (ImGui::IsKeyPressed(ImGuiKey_RightArrow))
      nx += step;
    if (ImGui::IsKeyPressed(ImGuiKey_UpArrow))
      ny -= step;
    if (ImGui::IsKeyPressed(ImGuiKey_DownArrow))
      ny += step;
    if (nx != shot.camera_x || ny != shot.camera_y) {
      const auto moved = zelda3::ClampCamera(extent, nx, ny);
      shot.camera_x = moved.x;
      shot.camera_y = moved.y;
      dirty_ = true;
    }
  }

  // Viewport and markers (markers outside the viewport are dimmed).
  draw->AddRect(view_min, view_max, ToU32(gui::GetInfoColor()), 0, 0, 2.0f);
  auto draw_marker = [&](int x, int y, ImVec4 color, const std::string& label) {
    const bool inside = zelda3::PointInViewport(camera.x, camera.y, x, y);
    const float alpha = inside ? 1.0f : 0.35f;
    const ImVec2 p = marker_pos(x, y);
    draw->AddCircleFilled(p, kMarkerRadius, ToU32(color, alpha));
    draw->AddText(ImVec2(p.x + kMarkerRadius + 2, p.y - 7),
                  ToU32(ImGui::GetStyleColorVec4(ImGuiCol_Text), alpha),
                  label.c_str());
  };
  draw_marker(shot.link.x, shot.link.y, gui::GetSuccessColor(),
              absl::StrFormat("Link (%d)", shot.link.facing));
  for (const auto& actor : shot.actors) {
    draw_marker(actor.x, actor.y, gui::GetWarningColor(),
                absl::StrFormat("0x%02X", actor.sprite));
  }

  if (hovered) {
    ImGui::Text("Mouse: world %d, %d  screen %d, %d", mouse_world_x,
                mouse_world_y, mouse_world_x - camera.x,
                mouse_world_y - camera.y);
  } else {
    ImGui::TextDisabled(
        "Drag the viewport or a marker; arrow keys nudge "
        "(Shift: 8 px).");
  }
  ImGui::TextDisabled(
      "Schematic view: the area map and sprite graphics need the overworld "
      "editor hook.");
}

}  // namespace yaze::editor
