#include "minecart_track_editor_panel.h"

#include <algorithm>
#include <cctype>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <sstream>
#include <string_view>
#include <system_error>

#include "absl/status/status.h"
#include "absl/strings/str_format.h"
#include "absl/strings/str_split.h"
#include "imgui/imgui.h"
#include "imgui/misc/cpp/imgui_stdlib.h"
#include "util/i18n/tr.h"

#include "app/gui/core/icons.h"
#include "app/gui/core/input.h"
#include "app/gui/core/style_guard.h"
#include "util/log.h"
#include "zelda3/dungeon/custom_collision.h"
#include "zelda3/dungeon/track_collision_generator.h"

namespace yaze::editor {

namespace {
constexpr int kTrackSlotCount = 32;
constexpr int kDefaultTrackRoom = 0x89;
constexpr int kDefaultTrackX = 0x1300;
constexpr int kDefaultTrackY = 0x1100;
constexpr std::string_view kTrackSourceRelativePath =
    "Sprites/Objects/data/minecart_tracks.asm";
constexpr std::string_view kPlannedTrackGuard =
    "if !ENABLE_MINECART_PLANNED_TRACK_TABLE == 1";

std::string Trim(std::string_view value) {
  size_t first = 0;
  while (first < value.size() &&
         std::isspace(static_cast<unsigned char>(value[first]))) {
    ++first;
  }
  size_t last = value.size();
  while (last > first &&
         std::isspace(static_cast<unsigned char>(value[last - 1]))) {
    --last;
  }
  return std::string(value.substr(first, last - first));
}

std::string StripCommentAndTrim(const std::string& line) {
  const size_t comment = line.find(';');
  return Trim(std::string_view(line).substr(0, comment));
}

bool IsStrictDescendant(const std::filesystem::path& path,
                        const std::filesystem::path& root) {
  auto path_it = path.begin();
  auto root_it = root.begin();
  for (; root_it != root.end(); ++root_it, ++path_it) {
    if (path_it == path.end() || *path_it != *root_it) {
      return false;
    }
  }
  return path_it != path.end();
}

absl::StatusOr<std::vector<int>> ParseDwValues(const std::string& line,
                                               const std::string& label) {
  const std::string code = StripCommentAndTrim(line);
  if (code.size() < 2 ||
      std::tolower(static_cast<unsigned char>(code[0])) != 'd' ||
      std::tolower(static_cast<unsigned char>(code[1])) != 'w' ||
      (code.size() > 2 && !std::isspace(static_cast<unsigned char>(code[2])))) {
    return absl::InvalidArgumentError(
        absl::StrFormat("Unexpected statement in %s: %s", label, code));
  }

  const std::string operands = Trim(std::string_view(code).substr(2));
  if (operands.empty()) {
    return absl::InvalidArgumentError(
        absl::StrFormat("Empty dw statement in %s", label));
  }

  std::vector<int> values;
  for (absl::string_view operand_view : absl::StrSplit(operands, ',')) {
    const std::string operand = Trim(operand_view);
    if (operand.size() < 2 || operand.size() > 5 || operand[0] != '$' ||
        !std::all_of(operand.begin() + 1, operand.end(), [](char c) {
          return std::isxdigit(static_cast<unsigned char>(c));
        })) {
      return absl::InvalidArgumentError(absl::StrFormat(
          "Invalid 16-bit hex value in %s: %s", label, operand));
    }
    try {
      const unsigned long value = std::stoul(operand.substr(1), nullptr, 16);
      if (value > 0xFFFF) {
        return absl::InvalidArgumentError(
            absl::StrFormat("Value out of range in %s: %s", label, operand));
      }
      values.push_back(static_cast<int>(value));
    } catch (...) {
      return absl::InvalidArgumentError(absl::StrFormat(
          "Invalid 16-bit hex value in %s: %s", label, operand));
    }
  }
  return values;
}

std::string FormatHexList(const std::vector<uint16_t>& values) {
  std::string out;
  for (size_t i = 0; i < values.size(); ++i) {
    if (i > 0) {
      out += ", ";
    }
    out += absl::StrFormat("0x%X", values[i]);
  }
  return out;
}

std::vector<uint16_t> ParseHexList(const std::string& input) {
  std::vector<uint16_t> out;
  for (absl::string_view token_view :
       absl::StrSplit(input, absl::ByAnyChar(", \n\t"), absl::SkipEmpty())) {
    if (token_view.empty()) {
      continue;
    }
    std::string token(token_view);
    if (token[0] == '$') {
      token = "0x" + token.substr(1);
    }
    int base = 10;
    if (token.rfind("0x", 0) == 0 || token.rfind("0X", 0) == 0) {
      token = token.substr(2);
      base = 16;
    }
    try {
      auto value = std::stoul(token, nullptr, base);
      if (value <= 0xFFFF) {
        out.push_back(static_cast<uint16_t>(value));
      }
    } catch (...) {
      continue;
    }
  }
  return out;
}
}  // namespace

void MinecartTrackEditorPanel::ResetTrackSession() {
  tracks_.clear();
  loaded_tracks_.clear();
  load_attempted_ = false;
  loaded_ = false;
  source_is_guarded_ = false;
  CancelCoordinatePicking();
  audit_dirty_ = true;
}

absl::Status MinecartTrackEditorPanel::SetProject(
    project::YazeProject* project) {
  const std::string next_filepath = project ? project->filepath : "";
  if (project_ == project && bound_project_filepath_ == next_filepath) {
    return absl::OkStatus();
  }
  if (HasUnpublishedChanges()) {
    return absl::FailedPreconditionError(
        "Discard minecart track drafts before changing projects");
  }

  project_ = project;
  bound_project_filepath_ = next_filepath;
  overlay_inputs_initialized_ = false;
  ResetTrackSession();
  return absl::OkStatus();
}

absl::StatusOr<std::filesystem::path>
MinecartTrackEditorPanel::ResolveTrackSourcePath() const {
  if (project_ == nullptr || bound_project_filepath_.empty()) {
    return absl::FailedPreconditionError(
        "An open project descriptor is required for minecart tracks");
  }

  std::error_code ec;
  std::filesystem::path descriptor_path(bound_project_filepath_);
  if (descriptor_path.is_relative()) {
    descriptor_path = std::filesystem::absolute(descriptor_path, ec);
    if (ec) {
      return absl::InvalidArgumentError(absl::StrFormat(
          "Could not resolve project descriptor path: %s", ec.message()));
    }
  }

  const std::filesystem::path project_root =
      std::filesystem::weakly_canonical(descriptor_path.parent_path(), ec);
  if (ec || project_root.empty() ||
      !std::filesystem::is_directory(project_root, ec) || ec) {
    return absl::InvalidArgumentError(
        "Project descriptor parent is not an existing directory");
  }

  const std::filesystem::path candidate =
      project_root / kTrackSourceRelativePath;
  const std::filesystem::path resolved =
      std::filesystem::canonical(candidate, ec);
  if (ec) {
    return absl::NotFoundError(
        absl::StrFormat("Minecart source not found: %s", candidate.string()));
  }
  if (!IsStrictDescendant(resolved, project_root)) {
    return absl::PermissionDeniedError(
        "Minecart source resolves outside the project root");
  }
  if (!std::filesystem::is_regular_file(resolved, ec) || ec) {
    return absl::InvalidArgumentError(
        "Minecart source must be an existing regular file");
  }
  return resolved;
}

bool MinecartTrackEditorPanel::HasUnpublishedChanges() const {
  return loaded_ && tracks_ != loaded_tracks_;
}

absl::Status MinecartTrackEditorPanel::UpdateTrack(size_t track_index,
                                                   const MinecartTrack& track) {
  if (!loaded_) {
    return absl::FailedPreconditionError("Minecart tracks are not loaded");
  }
  if (track_index >= tracks_.size()) {
    return absl::InvalidArgumentError("Minecart track index is out of range");
  }
  MinecartTrack updated = track;
  updated.id = static_cast<int>(track_index);
  tracks_[track_index] = updated;
  audit_dirty_ = true;
  return absl::OkStatus();
}

absl::Status MinecartTrackEditorPanel::DiscardUnpublishedChanges() {
  if (!loaded_) {
    return absl::FailedPreconditionError("Minecart tracks are not loaded");
  }
  tracks_ = loaded_tracks_;
  CancelCoordinatePicking();
  audit_dirty_ = true;
  return absl::OkStatus();
}

absl::Status MinecartTrackEditorPanel::ReloadTracks() {
  if (HasUnpublishedChanges()) {
    return absl::FailedPreconditionError(
        "Discard minecart track drafts before reloading the source");
  }
  return LoadTracks();
}

void MinecartTrackEditorPanel::InitializeOverlayInputs() {
  if (overlay_inputs_initialized_ || !project_) {
    return;
  }
  overlay_track_tiles_input_ =
      FormatHexList(project_->dungeon_overlay.track_tiles);
  overlay_track_stop_tiles_input_ =
      FormatHexList(project_->dungeon_overlay.track_stop_tiles);
  overlay_track_switch_tiles_input_ =
      FormatHexList(project_->dungeon_overlay.track_switch_tiles);
  overlay_track_object_ids_input_ =
      FormatHexList(project_->dungeon_overlay.track_object_ids);
  overlay_minecart_sprite_ids_input_ =
      FormatHexList(project_->dungeon_overlay.minecart_sprite_ids);
  overlay_inputs_initialized_ = true;
}

bool MinecartTrackEditorPanel::UpdateOverlayList(
    const char* label, std::string& input, std::vector<uint16_t>& target) {
  bool changed = ImGui::InputText(label, &input);
  if (changed && ImGui::IsItemDeactivatedAfterEdit()) {
    target = ParseHexList(input);
    audit_dirty_ = true;
    return true;
  }
  return false;
}

void MinecartTrackEditorPanel::DrawOverlaySettings() {
  if (!project_) {
    return;
  }

  InitializeOverlayInputs();

  if (!ImGui::CollapsingHeader(ICON_MD_TUNE " Overlay Config",
                               ImGuiTreeNodeFlags_DefaultOpen)) {
    return;
  }

  ImGui::TextDisabled(tr("Empty list = defaults. Use hex (0xB0) or decimal."));
  ImGui::TextDisabled(
      tr("Defaults: Track 0xB0-0xBE | Stop 0xB7-0xBA | Switch 0xD0-0xD3 | "
         "Track Obj 0x31 | Cart Sprite 0xA3"));

  bool changed = false;
  changed |= UpdateOverlayList("Track Tiles", overlay_track_tiles_input_,
                               project_->dungeon_overlay.track_tiles);
  changed |= UpdateOverlayList("Stop Tiles", overlay_track_stop_tiles_input_,
                               project_->dungeon_overlay.track_stop_tiles);
  changed |=
      UpdateOverlayList("Switch Tiles", overlay_track_switch_tiles_input_,
                        project_->dungeon_overlay.track_switch_tiles);
  changed |=
      UpdateOverlayList("Track Object IDs", overlay_track_object_ids_input_,
                        project_->dungeon_overlay.track_object_ids);
  changed |= UpdateOverlayList("Minecart Sprite IDs",
                               overlay_minecart_sprite_ids_input_,
                               project_->dungeon_overlay.minecart_sprite_ids);

  if (ImGui::Button(tr("Reset Overlay Defaults"))) {
    project_->dungeon_overlay.track_tiles.clear();
    project_->dungeon_overlay.track_stop_tiles.clear();
    project_->dungeon_overlay.track_switch_tiles.clear();
    project_->dungeon_overlay.track_object_ids.clear();
    project_->dungeon_overlay.minecart_sprite_ids.clear();
    overlay_track_tiles_input_.clear();
    overlay_track_stop_tiles_input_.clear();
    overlay_track_switch_tiles_input_.clear();
    overlay_track_object_ids_input_.clear();
    overlay_minecart_sprite_ids_input_.clear();
    audit_dirty_ = true;
    changed = true;
  }

  if (changed) {
    ImGui::TextDisabled(tr("Remember to save the project to persist changes."));
  }
}

const std::vector<MinecartTrack>& MinecartTrackEditorPanel::GetTracks() {
  if (!load_attempted_) {
    const absl::Status status = LoadTracks();
    if (!status.ok()) {
      status_message_ = std::string(status.message());
      show_success_ = false;
    }
  }
  return tracks_;
}

void MinecartTrackEditorPanel::SetPickedCoordinates(int room_id,
                                                    uint16_t camera_x,
                                                    uint16_t camera_y) {
  if (picking_mode_ && picking_track_index_ >= 0 &&
      picking_track_index_ < static_cast<int>(tracks_.size())) {
    tracks_[picking_track_index_].room_id = room_id;
    tracks_[picking_track_index_].start_x = camera_x;
    tracks_[picking_track_index_].start_y = camera_y;

    last_picked_x_ = camera_x;
    last_picked_y_ = camera_y;
    has_picked_coords_ = true;
    audit_dirty_ = true;

    status_message_ =
        absl::StrFormat("Track %d: Set to Room $%04X, Pos ($%04X, $%04X)",
                        picking_track_index_, room_id, camera_x, camera_y);
    show_success_ = true;
  }

  // Exit picking mode
  picking_mode_ = false;
  picking_track_index_ = -1;
}

void MinecartTrackEditorPanel::StartCoordinatePicking(int track_index) {
  picking_mode_ = true;
  picking_track_index_ = track_index;
  status_message_ = absl::StrFormat(
      "Click on the dungeon canvas to set Track %d position", track_index);
  show_success_ = false;
}

void MinecartTrackEditorPanel::CancelCoordinatePicking() {
  picking_mode_ = false;
  picking_track_index_ = -1;
  status_message_ = "";
}

bool MinecartTrackEditorPanel::IsDefaultTrack(
    const MinecartTrack& track) const {
  return track.room_id == kDefaultTrackRoom &&
         track.start_x == kDefaultTrackX && track.start_y == kDefaultTrackY;
}

void MinecartTrackEditorPanel::RebuildAuditCache() {
  room_audit_.clear();
  track_usage_rooms_.clear();
  track_subtype_used_.assign(kTrackSlotCount, false);

  if (!rooms_) {
    audit_dirty_ = false;
    return;
  }

  std::array<bool, 256> track_tiles{};
  std::array<bool, 256> stop_tiles{};
  std::array<bool, 256> switch_tiles{};
  auto apply_list = [](std::array<bool, 256>& dest,
                       const std::vector<uint16_t>& values) {
    dest.fill(false);
    for (uint16_t value : values) {
      if (value < dest.size()) {
        dest[value] = true;
      }
    }
  };

  if (project_ && !project_->dungeon_overlay.track_tiles.empty()) {
    apply_list(track_tiles, project_->dungeon_overlay.track_tiles);
  } else {
    std::vector<uint16_t> default_track_tiles;
    for (uint16_t tile = 0xB0; tile <= 0xBE; ++tile) {
      default_track_tiles.push_back(tile);
    }
    apply_list(track_tiles, default_track_tiles);
  }

  if (project_ && !project_->dungeon_overlay.track_stop_tiles.empty()) {
    apply_list(stop_tiles, project_->dungeon_overlay.track_stop_tiles);
  } else {
    apply_list(stop_tiles, {0xB7, 0xB8, 0xB9, 0xBA});
  }

  if (project_ && !project_->dungeon_overlay.track_switch_tiles.empty()) {
    apply_list(switch_tiles, project_->dungeon_overlay.track_switch_tiles);
  } else {
    apply_list(switch_tiles, {0xD0, 0xD1, 0xD2, 0xD3});
  }

  std::vector<uint16_t> track_object_ids = {0x31};
  std::vector<uint16_t> minecart_sprite_ids = {0xA3};
  if (project_) {
    if (!project_->dungeon_overlay.track_object_ids.empty()) {
      track_object_ids = project_->dungeon_overlay.track_object_ids;
    }
    if (!project_->dungeon_overlay.minecart_sprite_ids.empty()) {
      minecart_sprite_ids = project_->dungeon_overlay.minecart_sprite_ids;
    }
  }

  std::unordered_map<int, bool> track_object_id_map;
  for (uint16_t id : track_object_ids) {
    track_object_id_map[static_cast<int>(id)] = true;
  }
  std::unordered_map<int, bool> minecart_sprite_id_map;
  for (uint16_t id : minecart_sprite_ids) {
    minecart_sprite_id_map[static_cast<int>(id)] = true;
  }

  for (int room_id = 0; room_id < static_cast<int>(rooms_->size()); ++room_id) {
    auto& room = (*rooms_)[room_id];
    RoomTrackAudit audit;

    if (room.GetTileObjects().empty()) {
      room.LoadObjects();
    }
    if (room.GetSprites().empty()) {
      room.LoadSprites();
    }

    std::array<bool, kTrackSlotCount> seen_subtype{};

    for (const auto& obj : room.GetTileObjects()) {
      if (!track_object_id_map[static_cast<int>(obj.id_)]) {
        continue;
      }
      int subtype = obj.size_ & 0x1F;
      if (subtype >= 0 && subtype < kTrackSlotCount) {
        if (!seen_subtype[static_cast<size_t>(subtype)]) {
          seen_subtype[static_cast<size_t>(subtype)] = true;
          track_subtype_used_[static_cast<size_t>(subtype)] = true;
          track_usage_rooms_[subtype].push_back(room_id);
          audit.track_subtypes.push_back(subtype);
        }
      }
    }

    std::unordered_map<int, bool> stop_positions;
    auto map_or = zelda3::LoadCustomCollisionMap(room.rom(), room_id);
    if (map_or.ok() && map_or.value().has_data) {
      const auto& map = map_or.value().tiles;
      for (int y = 0; y < 64; ++y) {
        for (int x = 0; x < 64; ++x) {
          uint8_t tile = map[static_cast<size_t>(y * 64 + x)];
          if (track_tiles[tile] || stop_tiles[tile] || switch_tiles[tile]) {
            audit.has_track_collision = true;
          }
          if (stop_tiles[tile]) {
            audit.has_stop_tiles = true;
            stop_positions[y * 64 + x] = true;
          }
        }
      }
    }

    if (audit.has_track_collision) {
      for (const auto& sprite : room.GetSprites()) {
        if (!minecart_sprite_id_map[static_cast<int>(sprite.id())]) {
          continue;
        }
        audit.has_minecart_sprite = true;
        int tile_x = sprite.x() * 2;
        int tile_y = sprite.y() * 2;
        if (tile_x >= 0 && tile_x < 64 && tile_y >= 0 && tile_y < 64) {
          int idx = tile_y * 64 + tile_x;
          if (stop_positions[idx]) {
            audit.has_minecart_on_stop = true;
          }
        }
      }
    }

    if (audit.has_track_collision || !audit.track_subtypes.empty() ||
        audit.has_minecart_sprite) {
      room_audit_[room_id] = audit;
    }
  }

  audit_dirty_ = false;
}

void MinecartTrackEditorPanel::Draw(bool* p_open) {
  if (project_ == nullptr || bound_project_filepath_.empty()) {
    ImGui::TextColored(ImVec4(1, 0, 0, 1),
                       tr("Open a project to edit minecart tracks."));
    return;
  }

  if (!load_attempted_) {
    const absl::Status status = LoadTracks();
    if (!status.ok()) {
      status_message_ = std::string(status.message());
      show_success_ = false;
    }
  }

  if (audit_dirty_) {
    RebuildAuditCache();
  }

  ImGui::Text(tr("Minecart Track Editor"));
  const bool has_unpublished_changes = HasUnpublishedChanges();
  if (!has_unpublished_changes) {
    ImGui::BeginDisabled();
  }
  if (ImGui::Button(ICON_MD_SAVE " Save Tracks")) {
    const absl::Status status = SaveTracks();
    status_message_ = std::string(status.message());
    show_success_ = status.ok();
  }
  if (!has_unpublished_changes) {
    ImGui::EndDisabled();
  }
  ImGui::SameLine();
  if (!has_unpublished_changes) {
    ImGui::BeginDisabled();
  }
  if (ImGui::Button(ICON_MD_RESTORE " Discard Drafts")) {
    const absl::Status status = DiscardUnpublishedChanges();
    status_message_ = status.ok() ? "Minecart track drafts discarded."
                                  : std::string(status.message());
    show_success_ = status.ok();
  }
  if (!has_unpublished_changes) {
    ImGui::EndDisabled();
  }
  ImGui::SameLine();
  if (ImGui::Button(ICON_MD_REFRESH " Reload Source")) {
    const absl::Status status = ReloadTracks();
    status_message_ = status.ok() ? "Minecart track source reloaded."
                                  : std::string(status.message());
    show_success_ = status.ok();
  }
  ImGui::SameLine();
  const bool can_save_project = project_ && project_->project_opened();
  if (!can_save_project) {
    ImGui::BeginDisabled();
  }
  if (ImGui::Button(ICON_MD_SAVE " Save Project")) {
    auto status = project_->Save();
    if (status.ok()) {
      status_message_ = "Project saved.";
      show_success_ = true;
    } else {
      status_message_ =
          absl::StrFormat("Project save failed: %s", status.message());
      show_success_ = false;
    }
  }
  if (!can_save_project) {
    ImGui::EndDisabled();
  }

  // Show picking mode indicator
  if (picking_mode_) {
    ImGui::SameLine();
    if (ImGui::Button(ICON_MD_CANCEL " Cancel Pick")) {
      CancelCoordinatePicking();
    }
    ImGui::SameLine();
    ImGui::TextColored(ImVec4(1.0f, 0.8f, 0.2f, 1.0f),
                       ICON_MD_MY_LOCATION " Picking for Track %d...",
                       picking_track_index_);
  }

  if (!status_message_.empty() && !picking_mode_) {
    ImGui::SameLine();
    ImGui::TextColored(show_success_ ? ImVec4(0, 1, 0, 1) : ImVec4(1, 0, 0, 1),
                       "%s", status_message_.c_str());
  }

  DrawOverlaySettings();
  ImGui::Separator();

  // Coordinate format help
  ImGui::TextDisabled(tr(
      "Camera coordinates use $1XXX format (base $1000 + room offset + local "
      "position)"));
  ImGui::TextDisabled(tr(
      "Hover over dungeon canvas to see coordinates, or click 'Pick' button."));
  ImGui::Separator();

  if (ImGui::BeginTable("TracksTable", 7,
                        ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg |
                            ImGuiTableFlags_Resizable)) {
    ImGui::TableSetupColumn("ID", ImGuiTableColumnFlags_WidthFixed, 30.0f);
    ImGui::TableSetupColumn("Room ID", ImGuiTableColumnFlags_WidthFixed, 80.0f);
    ImGui::TableSetupColumn("Camera X", ImGuiTableColumnFlags_WidthFixed,
                            80.0f);
    ImGui::TableSetupColumn("Camera Y", ImGuiTableColumnFlags_WidthFixed,
                            80.0f);
    ImGui::TableSetupColumn("Pick", ImGuiTableColumnFlags_WidthFixed, 50.0f);
    ImGui::TableSetupColumn("Go", ImGuiTableColumnFlags_WidthFixed, 40.0f);
    ImGui::TableSetupColumn("Status", ImGuiTableColumnFlags_WidthFixed, 60.0f);
    ImGui::TableHeadersRow();

    for (auto& track : tracks_) {
      ImGui::TableNextRow();

      const bool is_default = IsDefaultTrack(track);
      const bool used_in_rooms =
          track.id >= 0 &&
          track.id < static_cast<int>(track_subtype_used_.size()) &&
          track_subtype_used_[track.id];
      const bool missing_start = used_in_rooms && is_default;

      if (missing_start) {
        ImGui::TableSetBgColor(ImGuiTableBgTarget_RowBg0,
                               IM_COL32(120, 40, 40, 120));
      } else if (is_default) {
        ImGui::TableSetBgColor(ImGuiTableBgTarget_RowBg0,
                               IM_COL32(60, 60, 60, 80));
      }

      // Highlight the row being picked
      if (picking_mode_ && track.id == picking_track_index_) {
        ImGui::TableSetBgColor(ImGuiTableBgTarget_RowBg0,
                               IM_COL32(80, 80, 0, 100));
      }

      ImGui::TableNextColumn();
      ImGui::Text("%d", track.id);

      ImGui::TableNextColumn();
      uint16_t room_id = static_cast<uint16_t>(track.room_id);
      if (yaze::gui::InputHexWordCustom(
              absl::StrFormat("##Room%d", track.id).c_str(), &room_id, 60.0f)) {
        track.room_id = room_id;
        audit_dirty_ = true;
      }

      ImGui::TableNextColumn();
      uint16_t start_x = static_cast<uint16_t>(track.start_x);
      if (yaze::gui::InputHexWordCustom(
              absl::StrFormat("##StartX%d", track.id).c_str(), &start_x,
              60.0f)) {
        track.start_x = start_x;
        audit_dirty_ = true;
      }

      ImGui::TableNextColumn();
      uint16_t start_y = static_cast<uint16_t>(track.start_y);
      if (yaze::gui::InputHexWordCustom(
              absl::StrFormat("##StartY%d", track.id).c_str(), &start_y,
              60.0f)) {
        track.start_y = start_y;
        audit_dirty_ = true;
      }

      // Pick button to select coordinates from canvas
      ImGui::TableNextColumn();
      ImGui::PushID(track.id);
      bool is_picking_this = picking_mode_ && picking_track_index_ == track.id;
      {
        std::optional<gui::StyleColorGuard> pick_guard;
        if (is_picking_this) {
          pick_guard.emplace(ImGuiCol_Button, ImVec4(0.8f, 0.6f, 0.0f, 1.0f));
        }
        if (ImGui::SmallButton(ICON_MD_MY_LOCATION)) {
          if (is_picking_this) {
            CancelCoordinatePicking();
          } else {
            StartCoordinatePicking(track.id);
          }
        }
      }
      if (ImGui::IsItemHovered()) {
        ImGui::SetTooltip(is_picking_this ? "Cancel picking"
                                          : "Pick coordinates from canvas");
      }
      ImGui::PopID();

      // Go to room button
      ImGui::TableNextColumn();
      ImGui::PushID(track.id + 1000);
      if (ImGui::SmallButton(ICON_MD_ARROW_FORWARD)) {
        if (room_navigation_callback_) {
          room_navigation_callback_(track.room_id);
        }
      }
      if (ImGui::IsItemHovered()) {
        ImGui::SetTooltip(tr("Navigate to room $%04X"), track.room_id);
      }
      ImGui::PopID();

      // Status column
      ImGui::TableNextColumn();
      if (missing_start) {
        ImGui::TextColored(ImVec4(1.0f, 0.6f, 0.1f, 1.0f),
                           ICON_MD_WARNING_AMBER);
      } else if (is_default) {
        ImGui::TextColored(ImVec4(0.7f, 0.7f, 0.7f, 1.0f), ICON_MD_INFO);
      } else if (used_in_rooms) {
        ImGui::TextColored(ImVec4(0.4f, 0.9f, 0.4f, 1.0f),
                           ICON_MD_CHECK_CIRCLE);
      } else {
        ImGui::Text("-");
      }

      if (ImGui::IsItemHovered()) {
        ImGui::BeginTooltip();
        if (missing_start) {
          ImGui::TextColored(ImVec4(1.0f, 0.6f, 0.1f, 1.0f),
                             tr("Used in rooms but still default"));
        } else if (is_default) {
          ImGui::Text(tr("Default filler slot"));
        } else if (used_in_rooms) {
          ImGui::Text(tr("Used in rooms"));
        } else {
          ImGui::Text(tr("No usage detected"));
        }

        auto rooms_it = track_usage_rooms_.find(track.id);
        if (rooms_it != track_usage_rooms_.end()) {
          ImGui::Separator();
          ImGui::Text(tr("Rooms:"));
          for (int room_id : rooms_it->second) {
            ImGui::BulletText(tr("0x%03X"), room_id);
          }
        }
        ImGui::EndTooltip();
      }
    }

    ImGui::EndTable();
  }

  // Summary + room audit
  int default_count = 0;
  int used_count = 0;
  int missing_start_count = 0;
  for (const auto& track : tracks_) {
    bool is_default = IsDefaultTrack(track);
    bool used_in_rooms =
        track.id >= 0 &&
        track.id < static_cast<int>(track_subtype_used_.size()) &&
        track_subtype_used_[track.id];
    if (is_default) {
      default_count++;
    }
    if (used_in_rooms) {
      used_count++;
    }
    if (used_in_rooms && is_default) {
      missing_start_count++;
    }
  }

  ImGui::Separator();
  ImGui::Text(tr("Usage Summary: used %d/%d, default %d, missing starts %d"),
              used_count, kTrackSlotCount, default_count, missing_start_count);

  if (!room_audit_.empty()) {
    ImGui::Separator();
    ImGui::Text(tr("Rooms with track objects:"));

    // "Generate All" button: batch-generate collision for all rooms that have
    // rail objects but no collision data yet.
    if (rom_ && rooms_) {
      // Count rooms that need generation
      int rooms_needing_collision = 0;
      for (const auto& [rid, audit] : room_audit_) {
        if (!audit.track_subtypes.empty() && !audit.has_track_collision) {
          rooms_needing_collision++;
        }
      }

      if (rooms_needing_collision > 0) {
        if (ImGui::Button(absl::StrFormat(ICON_MD_AUTO_FIX_HIGH
                                          " Generate All (%d rooms)",
                                          rooms_needing_collision)
                              .c_str())) {
          int generated_rooms = 0;
          int total_tiles = 0;
          bool had_error = false;

          for (auto& [rid, audit] : room_audit_) {
            if (audit.track_subtypes.empty() || audit.has_track_collision) {
              continue;
            }

            auto& target_room = (*rooms_)[rid];
            zelda3::GeneratorOptions opts;
            auto gen_result =
                zelda3::GenerateTrackCollision(&target_room, opts);
            if (!gen_result.ok()) {
              status_message_ =
                  absl::StrFormat("Generate failed for room 0x%03X: %s", rid,
                                  gen_result.status().message());
              show_success_ = false;
              had_error = true;
              break;
            }

            auto write_status = zelda3::WriteTrackCollision(
                rom_, rid, gen_result->collision_map);
            if (!write_status.ok()) {
              status_message_ =
                  absl::StrFormat("Write failed for room 0x%03X: %s", rid,
                                  write_status.message());
              show_success_ = false;
              had_error = true;
              break;
            }

            generated_rooms++;
            total_tiles += gen_result->tiles_generated;
          }

          if (!had_error) {
            status_message_ = absl::StrFormat(
                "Generated collision for %d rooms (%d tiles total)",
                generated_rooms, total_tiles);
            show_success_ = true;
          }
          audit_dirty_ = true;
        }
        if (ImGui::IsItemHovered()) {
          ImGui::SetTooltip(
              tr("Generate collision for all %d rooms with rail objects "
                 "but no collision data"),
              rooms_needing_collision);
        }
      }
    }

    ImGui::BeginChild("##TrackAuditRooms", ImVec2(0, 160), true);
    for (const auto& [room_id, audit] : room_audit_) {
      if (audit.track_subtypes.empty() && !audit.has_track_collision) {
        continue;
      }

      // Status icon
      if (!audit.has_track_collision) {
        ImGui::TextColored(ImVec4(1.0f, 0.4f, 0.1f, 1.0f),
                           ICON_MD_ERROR " Room 0x%03X (no collision)",
                           room_id);
      } else if (!audit.has_minecart_on_stop) {
        ImGui::TextColored(
            ImVec4(1.0f, 0.6f, 0.1f, 1.0f),
            ICON_MD_WARNING_AMBER " Room 0x%03X (no cart on stop)", room_id);
      } else {
        ImGui::TextColored(ImVec4(0.4f, 0.9f, 0.4f, 1.0f),
                           ICON_MD_CHECK_CIRCLE " Room 0x%03X", room_id);
      }

      ImGui::SameLine();
      ImGui::PushID(room_id);
      if (ImGui::SmallButton(ICON_MD_ARROW_FORWARD)) {
        if (room_navigation_callback_) {
          room_navigation_callback_(room_id);
        }
      }
      if (ImGui::IsItemHovered()) {
        ImGui::SetTooltip(tr("Navigate to room 0x%03X"), room_id);
      }

      // Generate Collision button (only if rom available and no collision yet)
      if (rom_ && rooms_ && !audit.has_track_collision) {
        ImGui::SameLine();
        if (ImGui::SmallButton(
                absl::StrFormat(ICON_MD_AUTO_FIX_HIGH " Generate##%d", room_id)
                    .c_str())) {
          auto& target_room = (*rooms_)[room_id];
          zelda3::GeneratorOptions opts;
          auto gen_result = zelda3::GenerateTrackCollision(&target_room, opts);
          if (gen_result.ok()) {
            auto write_status = zelda3::WriteTrackCollision(
                rom_, room_id, gen_result->collision_map);
            if (write_status.ok()) {
              status_message_ = absl::StrFormat(
                  "Room 0x%03X: Generated %d tiles (%d stops, %d corners)",
                  room_id, gen_result->tiles_generated, gen_result->stop_count,
                  gen_result->corner_count);
              show_success_ = true;
              audit_dirty_ = true;
            } else {
              status_message_ =
                  absl::StrFormat("Write failed: %s", write_status.message());
              show_success_ = false;
            }
          } else {
            status_message_ = absl::StrFormat("Generate failed: %s",
                                              gen_result.status().message());
            show_success_ = false;
          }
        }
        if (ImGui::IsItemHovered()) {
          ImGui::SetTooltip(tr(
              "Auto-generate collision tiles from rail objects in this room"));
        }
      }

      ImGui::PopID();
    }
    ImGui::EndChild();
  }
}

absl::Status MinecartTrackEditorPanel::LoadTracks() {
  load_attempted_ = true;

  auto path_or = ResolveTrackSourcePath();
  if (!path_or.ok()) {
    return path_or.status();
  }

  std::ifstream file(*path_or, std::ios::binary);
  if (!file.is_open()) {
    return absl::NotFoundError(absl::StrFormat(
        "Could not open minecart source: %s", path_or->string()));
  }
  std::stringstream buffer;
  buffer << file.rdbuf();
  if (file.bad()) {
    return absl::DataLossError(absl::StrFormat(
        "Could not read minecart source: %s", path_or->string()));
  }
  const std::string content = buffer.str();

  auto rooms_or = ParseSection(content, ".TrackStartingRooms");
  if (!rooms_or.ok()) {
    return rooms_or.status();
  }
  auto xs_or = ParseSection(content, ".TrackStartingX");
  if (!xs_or.ok()) {
    return xs_or.status();
  }
  auto ys_or = ParseSection(content, ".TrackStartingY");
  if (!ys_or.ok()) {
    return ys_or.status();
  }
  if (rooms_or->guarded != xs_or->guarded ||
      rooms_or->guarded != ys_or->guarded) {
    return absl::InvalidArgumentError(
        "Minecart sections must use the same flat or guarded layout");
  }

  std::vector<MinecartTrack> candidate_tracks;
  candidate_tracks.reserve(kTrackSlotCount);
  for (size_t i = 0; i < kTrackSlotCount; ++i) {
    candidate_tracks.push_back({static_cast<int>(i), rooms_or->values[i],
                                xs_or->values[i], ys_or->values[i]});
  }

  tracks_ = candidate_tracks;
  loaded_tracks_ = std::move(candidate_tracks);
  source_is_guarded_ = rooms_or->guarded;
  loaded_ = true;
  audit_dirty_ = true;
  status_message_.clear();
  show_success_ = true;
  return absl::OkStatus();
}

absl::StatusOr<MinecartTrackEditorPanel::ParsedSection>
MinecartTrackEditorPanel::ParseSection(const std::string& content,
                                       const std::string& label) {
  std::vector<std::string> lines;
  std::stringstream stream(content);
  std::string line;
  while (std::getline(stream, line)) {
    lines.push_back(line);
  }

  size_t section_line = 0;
  int label_count = 0;
  for (size_t i = 0; i < lines.size(); ++i) {
    if (StripCommentAndTrim(lines[i]) == label) {
      section_line = i;
      ++label_count;
    }
  }
  if (label_count != 1) {
    return absl::InvalidArgumentError(absl::StrFormat(
        "Expected exactly one %s label, found %d", label, label_count));
  }

  size_t section_end = lines.size();
  for (size_t i = section_line + 1; i < lines.size(); ++i) {
    const std::string code = StripCommentAndTrim(lines[i]);
    if (!code.empty() && code[0] == '.') {
      section_end = i;
      break;
    }
  }

  enum class GuardState { kPrefix, kEnabled, kDisabled, kComplete };
  GuardState state = GuardState::kPrefix;
  bool saw_guard = false;
  bool saw_else = false;
  bool saw_endif = false;
  std::vector<int> prefix;
  std::vector<int> enabled;
  std::vector<int> disabled;

  for (size_t i = section_line + 1; i < section_end; ++i) {
    const std::string code = StripCommentAndTrim(lines[i]);
    if (code.empty()) {
      continue;
    }
    if (code == kPlannedTrackGuard) {
      if (state != GuardState::kPrefix || saw_guard) {
        return absl::InvalidArgumentError(
            absl::StrFormat("Nested or repeated guard in %s", label));
      }
      saw_guard = true;
      state = GuardState::kEnabled;
      continue;
    }
    if (code == "else") {
      if (!saw_guard || state != GuardState::kEnabled || saw_else) {
        return absl::InvalidArgumentError(
            absl::StrFormat("Unexpected else in %s", label));
      }
      saw_else = true;
      state = GuardState::kDisabled;
      continue;
    }
    if (code == "endif") {
      if (!saw_else || state != GuardState::kDisabled || saw_endif) {
        return absl::InvalidArgumentError(
            absl::StrFormat("Unexpected endif in %s", label));
      }
      saw_endif = true;
      state = GuardState::kComplete;
      continue;
    }
    if (state == GuardState::kComplete) {
      return absl::InvalidArgumentError(absl::StrFormat(
          "Unexpected content after guarded table in %s: %s", label, code));
    }

    auto values_or = ParseDwValues(lines[i], label);
    if (!values_or.ok()) {
      return values_or.status();
    }
    std::vector<int>* destination = &prefix;
    if (state == GuardState::kEnabled) {
      destination = &enabled;
    } else if (state == GuardState::kDisabled) {
      destination = &disabled;
    }
    destination->insert(destination->end(), values_or->begin(),
                        values_or->end());
  }

  ParsedSection result;
  if (!saw_guard) {
    if (prefix.size() != kTrackSlotCount) {
      return absl::InvalidArgumentError(
          absl::StrFormat("%s must contain exactly %d values; found %zu", label,
                          kTrackSlotCount, prefix.size()));
    }
    result.values = std::move(prefix);
    return result;
  }

  if (!saw_else || !saw_endif) {
    return absl::InvalidArgumentError(
        absl::StrFormat("Incomplete planned-track guard in %s", label));
  }
  if (prefix.size() + enabled.size() != kTrackSlotCount ||
      prefix.size() + disabled.size() != kTrackSlotCount) {
    return absl::InvalidArgumentError(absl::StrFormat(
        "%s guarded branches must each resolve to exactly %d values; found "
        "%zu enabled and %zu disabled",
        label, kTrackSlotCount, prefix.size() + enabled.size(),
        prefix.size() + disabled.size()));
  }

  result.values = prefix;
  result.values.insert(result.values.end(), enabled.begin(), enabled.end());
  result.guarded = true;
  return result;
}

absl::Status MinecartTrackEditorPanel::SaveTracks() {
  if (!loaded_) {
    return absl::FailedPreconditionError("Minecart tracks are not loaded");
  }
  if (!HasUnpublishedChanges()) {
    return absl::FailedPreconditionError(
        "No unpublished minecart track drafts to save");
  }
  if (source_is_guarded_) {
    return absl::FailedPreconditionError(
        "Guarded minecart source publishing is disabled until a lossless "
        "publisher is available; drafts were kept");
  }
  return absl::UnimplementedError(
      "Minecart source publishing is disabled until atomic source guards are "
      "available; drafts were kept");
}

}  // namespace yaze::editor
