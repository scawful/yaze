#include "minecart_track_editor_panel.h"

#include <algorithm>
#include <charconv>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <memory>
#include <system_error>
#include <utility>
#include <vector>

#include "absl/status/status.h"
#include "absl/strings/ascii.h"
#include "absl/strings/match.h"
#include "absl/strings/str_format.h"
#include "absl/strings/str_split.h"
#include "core/source_artifact_publisher.h"
#include "imgui/imgui.h"
#include "imgui/misc/cpp/imgui_stdlib.h"
#include "util/i18n/tr.h"
#include "util/macro.h"

#include "app/gui/core/icons.h"
#include "app/gui/core/input.h"
#include "app/gui/core/style_guard.h"
#include "util/log.h"
#include "zelda3/dungeon/custom_collision.h"
#include "zelda3/dungeon/track_collision_generator.h"

namespace yaze::editor {

namespace {
constexpr int kTrackSlotCount = static_cast<int>(kMinecartTrackSlotCount);
constexpr int kDefaultTrackRoom = 0x89;
constexpr int kDefaultTrackX = 0x1300;
constexpr int kDefaultTrackY = 0x1100;
#if defined(__EMSCRIPTEN__)
constexpr bool kSourcePublishingAvailable = false;
#else
constexpr bool kSourcePublishingAvailable = true;
#endif

const core::SourceArtifactPublisherLabels kMinecartSourcePublisherLabels{
    .subject = "minecart track source",
    .published_file = "published minecart track source",
};

std::optional<core::MinecartTrackLayout::Source> ProjectSourceIdentity(
    const project::YazeProject* project) {
  if (project == nullptr || !project->hack_manifest.loaded()) {
    return std::nullopt;
  }
  return project->hack_manifest.minecart_track_layout().source;
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

absl::StatusOr<std::string> ReadSourceFile(const std::filesystem::path& path) {
  std::ifstream input(path, std::ios::binary);
  if (!input.is_open()) {
    return absl::NotFoundError(
        absl::StrFormat("Could not open minecart source: %s", path.string()));
  }
  std::string content{std::istreambuf_iterator<char>(input),
                      std::istreambuf_iterator<char>()};
  if (!input.good() && !input.eof()) {
    return absl::DataLossError(
        absl::StrFormat("Could not read minecart source: %s", path.string()));
  }
  return content;
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

bool OverlaySettingsEqual(const project::DungeonOverlaySettings& lhs,
                          const project::DungeonOverlaySettings& rhs) {
  return lhs.track_tiles == rhs.track_tiles &&
         lhs.track_stop_tiles == rhs.track_stop_tiles &&
         lhs.track_switch_tiles == rhs.track_switch_tiles &&
         lhs.track_object_ids == rhs.track_object_ids &&
         lhs.minecart_sprite_ids == rhs.minecart_sprite_ids;
}

bool HasAnyCustomCollision(const zelda3::CustomCollisionMap& map) {
  return map.has_data || std::any_of(map.tiles.begin(), map.tiles.end(),
                                     [](uint8_t tile) { return tile != 0; });
}

absl::StatusOr<std::vector<uint16_t>> ParseHexList(const std::string& input) {
  std::vector<uint16_t> out;
  const absl::string_view trimmed_input = absl::StripAsciiWhitespace(input);
  if (trimmed_input.empty()) {
    return out;
  }

  for (absl::string_view comma_group : absl::StrSplit(trimmed_input, ',')) {
    comma_group = absl::StripAsciiWhitespace(comma_group);
    if (comma_group.empty()) {
      return absl::InvalidArgumentError("Overlay list contains an empty value");
    }

    for (absl::string_view token : absl::StrSplit(
             comma_group, absl::ByAnyChar(" \n\t\r"), absl::SkipEmpty())) {
      const absl::string_view original_token = token;
      int base = 10;
      if (absl::StartsWith(token, "$")) {
        token.remove_prefix(1);
        base = 16;
      } else if (absl::StartsWith(token, "0x") ||
                 absl::StartsWith(token, "0X")) {
        token.remove_prefix(2);
        base = 16;
      }
      if (token.empty()) {
        return absl::InvalidArgumentError(absl::StrFormat(
            "Overlay value is missing digits: %s", original_token));
      }

      uint32_t value = 0;
      const auto [end, error] = std::from_chars(
          token.data(), token.data() + token.size(), value, base);
      if (error == std::errc::result_out_of_range || value > 0xFFFF) {
        return absl::OutOfRangeError(absl::StrFormat(
            "Overlay value is outside 16-bit range: %s", original_token));
      }
      if (error != std::errc() || end != token.data() + token.size()) {
        return absl::InvalidArgumentError(
            absl::StrFormat("Invalid overlay value: %s", original_token));
      }
      out.push_back(static_cast<uint16_t>(value));
    }
  }
  return out;
}
}  // namespace

void MinecartTrackEditorPanel::ResetTrackSession() {
  tracks_.clear();
  loaded_tracks_.clear();
  source_document_.reset();
  loaded_source_identity_.reset();
  loaded_source_path_.clear();
  loaded_source_sha256_.clear();
  load_attempted_ = false;
  loaded_ = false;
  CancelCoordinatePicking();
  ClearCollisionPreview();
  audit_dirty_ = true;
}

absl::Status MinecartTrackEditorPanel::RefreshProjectBinding() {
  const std::string current_filepath = project_ ? project_->filepath : "";
  const auto current_source_identity = ProjectSourceIdentity(project_);
  if (current_filepath == bound_project_filepath_ &&
      current_source_identity == bound_source_identity_) {
    return absl::OkStatus();
  }
  if (HasUnpublishedChanges()) {
    return absl::FailedPreconditionError(
        "Project descriptor moved or minecart source changed; discard "
        "minecart track drafts before rebinding the source");
  }

  bound_project_filepath_ = current_filepath;
  bound_source_identity_ = current_source_identity;
  overlay_inputs_initialized_ = false;
  overlay_inputs_model_.reset();
  ResetTrackSession();
  return absl::OkStatus();
}

absl::Status MinecartTrackEditorPanel::SetProject(
    project::YazeProject* project) {
  const std::string next_filepath = project ? project->filepath : "";
  const auto next_source_identity = ProjectSourceIdentity(project);
  if (project_ == project && bound_project_filepath_ == next_filepath &&
      bound_source_identity_ == next_source_identity) {
    return absl::OkStatus();
  }
  if (HasUnpublishedChanges()) {
    return absl::FailedPreconditionError(
        "Discard minecart track drafts before changing projects");
  }

  project_ = project;
  bound_project_filepath_ = next_filepath;
  bound_source_identity_ = next_source_identity;
  overlay_inputs_initialized_ = false;
  overlay_inputs_model_.reset();
  ResetTrackSession();
  return absl::OkStatus();
}

absl::Status MinecartTrackEditorPanel::RebindProjectContext(
    project::YazeProject* project) {
  const absl::Status status = SetProject(project);
  if (!status.ok()) {
    status_message_ = std::string(status.message());
    show_success_ = false;
    return status;
  }

  // Project descriptors are reapplied into stable session storage. Refresh a
  // same-pointer binding only when the model changed; otherwise preserve text
  // currently being edited until it can be validated and committed.
  if (project != nullptr && (!overlay_inputs_model_.has_value() ||
                             !OverlaySettingsEqual(*overlay_inputs_model_,
                                                   project->dungeon_overlay))) {
    InvalidateRoomAudit();
    overlay_inputs_initialized_ = false;
    InitializeOverlayInputs();
  }
  return absl::OkStatus();
}

absl::Status MinecartTrackEditorPanel::ValidateLoadedManifestIdentity() const {
  if (project_ == nullptr || !project_->hack_manifest.loaded()) {
    return absl::FailedPreconditionError(
        "A loaded hack manifest is required to publish minecart tracks");
  }
  const auto& current_source =
      project_->hack_manifest.minecart_track_layout().source;
  if (!current_source.has_value()) {
    return absl::FailedPreconditionError(
        "Hack manifest does not define minecart_tracks.source");
  }
  if (!loaded_source_identity_.has_value() ||
      *current_source != *loaded_source_identity_) {
    return absl::FailedPreconditionError(
        "Hack manifest minecart_tracks.source changed after the tracks were "
        "loaded; drafts were kept");
  }
  return absl::OkStatus();
}

absl::StatusOr<std::filesystem::path>
MinecartTrackEditorPanel::ResolveTrackSourcePath() const {
  if (project_ != nullptr && project_->filepath != bound_project_filepath_) {
    return absl::FailedPreconditionError(
        "Project descriptor moved; reload minecart tracks to rebind the "
        "source");
  }
  if (project_ == nullptr || bound_project_filepath_.empty()) {
    return absl::FailedPreconditionError(
        "An open project descriptor is required for minecart tracks");
  }
  if (!project_->hack_manifest.loaded()) {
    return absl::FailedPreconditionError(
        "A loaded hack manifest is required for minecart tracks");
  }
  const auto& source = project_->hack_manifest.minecart_track_layout().source;
  if (!source.has_value()) {
    return absl::FailedPreconditionError(
        "Hack manifest does not define minecart_tracks.source");
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
      (project_root / source->path).lexically_normal();
  const std::filesystem::file_status candidate_status =
      std::filesystem::symlink_status(candidate, ec);
  if (ec) {
    return absl::NotFoundError(
        absl::StrFormat("Minecart source not found: %s", candidate.string()));
  }
  if (std::filesystem::is_symlink(candidate_status)) {
    return absl::PermissionDeniedError(
        "Minecart source may not be a symbolic link");
  }
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

bool MinecartTrackEditorPanel::HasPendingProjectDraftChanges() const {
  if (!overlay_inputs_initialized_ || project_ == nullptr) {
    return false;
  }
  return overlay_track_tiles_input_ !=
             FormatHexList(project_->dungeon_overlay.track_tiles) ||
         overlay_track_stop_tiles_input_ !=
             FormatHexList(project_->dungeon_overlay.track_stop_tiles) ||
         overlay_track_switch_tiles_input_ !=
             FormatHexList(project_->dungeon_overlay.track_switch_tiles) ||
         overlay_track_object_ids_input_ !=
             FormatHexList(project_->dungeon_overlay.track_object_ids) ||
         overlay_minecart_sprite_ids_input_ !=
             FormatHexList(project_->dungeon_overlay.minecart_sprite_ids);
}

absl::Status MinecartTrackEditorPanel::PrepareProjectSave() {
  const absl::StatusOr<bool> committed = CommitOverlayInputsForSave();
  return committed.ok() ? absl::OkStatus() : committed.status();
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
  const absl::Status binding_status = RefreshProjectBinding();
  if (!binding_status.ok()) {
    return binding_status;
  }
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
  overlay_inputs_model_ = project_->dungeon_overlay;
  overlay_inputs_initialized_ = true;
}

void MinecartTrackEditorPanel::ClearOverlayInputs() {
  overlay_track_tiles_input_.clear();
  overlay_track_stop_tiles_input_.clear();
  overlay_track_switch_tiles_input_.clear();
  overlay_track_object_ids_input_.clear();
  overlay_minecart_sprite_ids_input_.clear();
}

bool MinecartTrackEditorPanel::UpdateOverlayList(const char* label,
                                                 std::string& input,
                                                 OverlayListMember member) {
  if (ImGui::InputText(label, &input)) {
    const absl::Status draft_status = NotifyProjectDraftChanged();
    if (!draft_status.ok()) {
      input = FormatHexList(project_->dungeon_overlay.*member);
      status_message_ =
          absl::StrFormat("Overlay draft rejected: %s", draft_status.message());
      show_success_ = false;
      return false;
    }
  }
  if (ImGui::IsItemDeactivatedAfterEdit()) {
    const absl::StatusOr<bool> changed = CommitOverlayList(input, member);
    return changed.ok() && *changed;
  }
  return false;
}

absl::StatusOr<bool> MinecartTrackEditorPanel::CommitOverlayList(
    std::string& input, OverlayListMember member) {
  if (project_ == nullptr) {
    const absl::Status status =
        absl::FailedPreconditionError("No project is bound to the panel");
    status_message_ = std::string(status.message());
    show_success_ = false;
    return status;
  }

  const auto parsed_or = ParseHexList(input);
  if (!parsed_or.ok()) {
    status_message_ = absl::StrFormat("Overlay update rejected: %s",
                                      parsed_or.status().message());
    show_success_ = false;
    return parsed_or.status();
  }

  project::DungeonOverlaySettings candidate = project_->dungeon_overlay;
  std::vector<uint16_t>& candidate_target = candidate.*member;
  if (*parsed_or == candidate_target) {
    input = FormatHexList(*parsed_or);
    overlay_inputs_model_ = project_->dungeon_overlay;
    status_message_.clear();
    show_success_ = false;
    return false;
  }

  candidate_target = *parsed_or;
  const absl::Status notify_status = NotifyProjectChanged(candidate);
  if (!notify_status.ok()) {
    input = FormatHexList(project_->dungeon_overlay.*member);
    status_message_ =
        absl::StrFormat("Overlay update rejected: %s", notify_status.message());
    show_success_ = false;
    return notify_status;
  }

  project_->dungeon_overlay = std::move(candidate);
  input = FormatHexList(project_->dungeon_overlay.*member);
  overlay_inputs_model_ = project_->dungeon_overlay;
  ClearCollisionPreview();
  audit_dirty_ = true;
  status_message_ = "Overlay settings updated; save the project to persist.";
  show_success_ = true;
  return true;
}

absl::StatusOr<bool> MinecartTrackEditorPanel::CommitOverlayInputsForSave() {
  if (project_ == nullptr) {
    return absl::FailedPreconditionError("No project is bound to the panel");
  }

  InitializeOverlayInputs();
  project::DungeonOverlaySettings candidate;
  auto parse_field = [](const char* field_name, const std::string& input,
                        std::vector<uint16_t>* target) -> absl::Status {
    const auto parsed_or = ParseHexList(input);
    if (!parsed_or.ok()) {
      return absl::Status(
          parsed_or.status().code(),
          absl::StrFormat("%s: %s", field_name, parsed_or.status().message()));
    }
    *target = *parsed_or;
    return absl::OkStatus();
  };

  absl::Status parse_status = parse_field(
      "Track Tiles", overlay_track_tiles_input_, &candidate.track_tiles);
  if (parse_status.ok()) {
    parse_status = parse_field("Stop Tiles", overlay_track_stop_tiles_input_,
                               &candidate.track_stop_tiles);
  }
  if (parse_status.ok()) {
    parse_status =
        parse_field("Switch Tiles", overlay_track_switch_tiles_input_,
                    &candidate.track_switch_tiles);
  }
  if (parse_status.ok()) {
    parse_status =
        parse_field("Track Object IDs", overlay_track_object_ids_input_,
                    &candidate.track_object_ids);
  }
  if (parse_status.ok()) {
    parse_status =
        parse_field("Minecart Sprite IDs", overlay_minecart_sprite_ids_input_,
                    &candidate.minecart_sprite_ids);
  }
  if (!parse_status.ok()) {
    status_message_ =
        absl::StrFormat("Project save blocked: %s", parse_status.message());
    show_success_ = false;
    return parse_status;
  }

  auto normalize_inputs = [this]() {
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
    overlay_inputs_model_ = project_->dungeon_overlay;
  };

  if (OverlaySettingsEqual(candidate, project_->dungeon_overlay)) {
    normalize_inputs();
    return false;
  }

  const absl::Status notify_status = NotifyProjectChanged(candidate);
  if (!notify_status.ok()) {
    status_message_ =
        absl::StrFormat("Project save blocked: %s", notify_status.message());
    show_success_ = false;
    return notify_status;
  }

  project_->dungeon_overlay = std::move(candidate);
  normalize_inputs();
  ClearCollisionPreview();
  audit_dirty_ = true;
  return true;
}

absl::StatusOr<bool> MinecartTrackEditorPanel::ResetOverlaySettings() {
  if (project_ == nullptr) {
    const absl::Status status =
        absl::FailedPreconditionError("No project is bound to the panel");
    status_message_ = std::string(status.message());
    show_success_ = false;
    return status;
  }

  project::DungeonOverlaySettings overlay = project_->dungeon_overlay;
  const bool changed =
      !overlay.track_tiles.empty() || !overlay.track_stop_tiles.empty() ||
      !overlay.track_switch_tiles.empty() ||
      !overlay.track_object_ids.empty() || !overlay.minecart_sprite_ids.empty();
  if (!changed) {
    ClearOverlayInputs();
    overlay_inputs_model_ = project_->dungeon_overlay;
    status_message_.clear();
    show_success_ = false;
    return false;
  }

  overlay.track_tiles.clear();
  overlay.track_stop_tiles.clear();
  overlay.track_switch_tiles.clear();
  overlay.track_object_ids.clear();
  overlay.minecart_sprite_ids.clear();
  const absl::Status notify_status = NotifyProjectChanged(overlay);
  if (!notify_status.ok()) {
    status_message_ =
        absl::StrFormat("Overlay reset rejected: %s", notify_status.message());
    show_success_ = false;
    return notify_status;
  }

  project_->dungeon_overlay = std::move(overlay);
  ClearOverlayInputs();
  overlay_inputs_model_ = project_->dungeon_overlay;
  ClearCollisionPreview();
  audit_dirty_ = true;
  status_message_ = "Overlay settings reset; save the project to persist.";
  show_success_ = true;
  return true;
}

absl::Status MinecartTrackEditorPanel::NotifyProjectChanged(
    const project::DungeonOverlaySettings& overlay) {
  if (project_ == nullptr) {
    return absl::FailedPreconditionError("No project is bound to the panel");
  }
  if (!project_changed_callback_) {
    return absl::FailedPreconditionError(
        "Project change tracking is unavailable for minecart overlays");
  }
  return project_changed_callback_(overlay);
}

absl::Status MinecartTrackEditorPanel::NotifyProjectDraftChanged() {
  if (project_ == nullptr) {
    return absl::FailedPreconditionError("No project is bound to the panel");
  }
  if (!project_draft_changed_callback_) {
    return absl::FailedPreconditionError(
        "Project draft tracking is unavailable for minecart overlays");
  }
  return project_draft_changed_callback_();
}

absl::Status MinecartTrackEditorPanel::SaveProjectSettings() {
  if (project_ == nullptr || !project_->project_opened()) {
    return absl::FailedPreconditionError("No open project to save");
  }
  if (!project_save_callback_) {
    return absl::FailedPreconditionError(
        "Project save is unavailable outside the editor manager");
  }
  const absl::StatusOr<bool> committed = CommitOverlayInputsForSave();
  if (!committed.ok()) {
    return committed.status();
  }
  return project_save_callback_();
}

void MinecartTrackEditorPanel::DrawOverlaySettings() {
  if (!project_) {
    return;
  }

  InitializeOverlayInputs();

  if (!ImGui::CollapsingHeader(ICON_MD_TUNE " Advanced")) {
    return;
  }

  ImGui::TextDisabled(tr("Advanced collision detection IDs and tile values."));
  ImGui::TextDisabled(tr("Empty list = defaults. Use hex (0xB0) or decimal."));
  ImGui::TextDisabled(
      tr("Defaults: Track 0xB0-0xBE | Stop 0xB7-0xBA | Switch 0xD0-0xD3 | "
         "Track Obj 0x31 | Cart Sprite 0xA3"));

  bool changed = false;
  changed |= UpdateOverlayList("Track Tiles", overlay_track_tiles_input_,
                               &project::DungeonOverlaySettings::track_tiles);
  changed |=
      UpdateOverlayList("Stop Tiles", overlay_track_stop_tiles_input_,
                        &project::DungeonOverlaySettings::track_stop_tiles);
  changed |=
      UpdateOverlayList("Switch Tiles", overlay_track_switch_tiles_input_,
                        &project::DungeonOverlaySettings::track_switch_tiles);
  changed |=
      UpdateOverlayList("Track Object IDs", overlay_track_object_ids_input_,
                        &project::DungeonOverlaySettings::track_object_ids);
  changed |= UpdateOverlayList(
      "Minecart Sprite IDs", overlay_minecart_sprite_ids_input_,
      &project::DungeonOverlaySettings::minecart_sprite_ids);

  if (ImGui::Button(tr("Reset Overlay Defaults"))) {
    const absl::StatusOr<bool> reset = ResetOverlaySettings();
    changed |= reset.ok() && *reset;
  }

  if (changed) {
    ImGui::TextDisabled(tr("Remember to save the project to persist changes."));
  }
}

const std::vector<MinecartTrack>& MinecartTrackEditorPanel::GetTracks() {
  const absl::Status binding_status = RefreshProjectBinding();
  if (!binding_status.ok()) {
    status_message_ = std::string(binding_status.message());
    show_success_ = false;
    return tracks_;
  }
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

void MinecartTrackEditorPanel::RebuildAuditCache(bool include_unmaterialized) {
  room_audit_.clear();
  route_usage_rooms_.clear();
  route_slot_used_.assign(kTrackSlotCount, false);

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

  auto audit_room = [&](int room_id, zelda3::Room& room) {
    RoomTrackAudit audit;

    room.EnsureObjectsLoaded();
    room.EnsureSpritesLoaded();

    std::array<bool, kTrackSlotCount> seen_subtype{};

    for (const auto& obj : room.GetTileObjects()) {
      if (!track_object_id_map[static_cast<int>(obj.id_)]) {
        continue;
      }
      int subtype = obj.size_ & 0x1F;
      if (zelda3::IsMinecartTrackGraphicsSubtype(obj.id_, subtype) &&
          subtype >= 0 && subtype < kTrackSlotCount) {
        if (!seen_subtype[static_cast<size_t>(subtype)]) {
          seen_subtype[static_cast<size_t>(subtype)] = true;
          audit.track_subtypes.push_back(subtype);
        }
      }
    }

    std::unordered_map<int, bool> stop_positions;
    const auto& collision = room.custom_collision();
    audit.has_any_custom_collision = HasAnyCustomCollision(collision);
    if (audit.has_any_custom_collision) {
      const auto& map = collision.tiles;
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

    std::array<bool, kTrackSlotCount> seen_route_slot{};
    for (const auto& sprite : room.GetSprites()) {
      if (!minecart_sprite_id_map[static_cast<int>(sprite.id())]) {
        continue;
      }
      audit.has_minecart_sprite = true;
      const int route_slot = sprite.subtype();
      if (route_slot >= 0 && route_slot < kTrackSlotCount &&
          !seen_route_slot[static_cast<size_t>(route_slot)]) {
        seen_route_slot[static_cast<size_t>(route_slot)] = true;
        route_slot_used_[static_cast<size_t>(route_slot)] = true;
        route_usage_rooms_[route_slot].push_back(room_id);
        audit.route_slots.push_back(route_slot);
      }
      int tile_x = sprite.x() * 2;
      int tile_y = sprite.y() * 2;
      if (tile_x >= 0 && tile_x < 64 && tile_y >= 0 && tile_y < 64) {
        int idx = tile_y * 64 + tile_x;
        if (stop_positions[idx]) {
          audit.has_minecart_on_stop = true;
        }
      }
    }

    if (audit.has_track_collision || !audit.track_subtypes.empty() ||
        audit.has_minecart_sprite) {
      room_audit_[room_id] = audit;
    }
  };

  // Inspect every room. Materialized rooms are the authority for unsaved
  // editor changes; unopened rooms are parsed into temporary models so a
  // global audit does not depend on which room tabs happen to be open.
  for (int room_id = 0; room_id < static_cast<int>(rooms_->size()); ++room_id) {
    if (auto* room = rooms_->GetIfMaterialized(room_id)) {
      audit_room(room_id, *room);
      continue;
    }
    if (!include_unmaterialized) {
      continue;
    }
    if (rooms_->rom() == nullptr || !rooms_->rom()->is_loaded()) {
      continue;
    }
    zelda3::Room room(room_id, rooms_->rom(), rooms_->game_data());
    audit_room(room_id, room);
  }

  audit_dirty_ = false;
  audit_includes_all_rooms_ = include_unmaterialized;
}

absl::StatusOr<zelda3::GeneratorOptions>
MinecartTrackEditorPanel::ResolveGeneratorOptions() const {
  zelda3::GeneratorOptions options;
  if (project_ == nullptr ||
      project_->dungeon_overlay.track_object_ids.empty()) {
    return options;
  }
  if (project_->dungeon_overlay.track_object_ids.size() != 1) {
    return absl::FailedPreconditionError(
        "Collision preview requires exactly one configured track object ID");
  }
  options.track_object_id = project_->dungeon_overlay.track_object_ids.front();
  return options;
}

absl::Status MinecartTrackEditorPanel::BuildCollisionPreview(
    const std::vector<int>& room_ids) {
  if (rooms_ == nullptr) {
    return absl::FailedPreconditionError("Dungeon rooms are unavailable");
  }
  if (room_ids.empty()) {
    return absl::InvalidArgumentError("No eligible rooms to preview");
  }

  ASSIGN_OR_RETURN(const auto options, ResolveGeneratorOptions());
  std::vector<int> sorted_room_ids = room_ids;
  std::sort(sorted_room_ids.begin(), sorted_room_ids.end());
  if (std::adjacent_find(sorted_room_ids.begin(), sorted_room_ids.end()) !=
      sorted_room_ids.end()) {
    return absl::InvalidArgumentError(
        "Collision preview contains a duplicate room ID");
  }

  std::vector<zelda3::TrackCollisionResult> preview;
  preview.reserve(sorted_room_ids.size());
  for (int room_id : sorted_room_ids) {
    if (room_id < 0 || room_id >= static_cast<int>(rooms_->size())) {
      return absl::OutOfRangeError(absl::StrFormat(
          "Collision preview room 0x%03X is out of range", room_id));
    }
    auto* room = rooms_->GetIfMaterialized(room_id);
    if (room == nullptr) {
      if (rooms_->rom() == nullptr || !rooms_->rom()->is_loaded()) {
        return absl::FailedPreconditionError(absl::StrFormat(
            "Collision preview room 0x%03X cannot be loaded without a ROM",
            room_id));
      }
      auto& materialized = (*rooms_)[room_id];
      materialized = zelda3::LoadRoomFromRom(rooms_->rom(), room_id);
      materialized.SetGameData(rooms_->game_data());
      room = &materialized;
    }
    room->EnsureObjectsLoaded();
    if (HasAnyCustomCollision(room->custom_collision())) {
      return absl::FailedPreconditionError(absl::StrFormat(
          "Room 0x%03X already has custom collision; generation will not "
          "replace it",
          room_id));
    }

    ASSIGN_OR_RETURN(auto generated,
                     zelda3::GenerateTrackCollision(room, options));
    generated.room_id = room_id;
    if (!generated.collision_map.has_data || generated.tiles_generated <= 0) {
      return absl::FailedPreconditionError(absl::StrFormat(
          "Room 0x%03X has no supported minecart track pieces", room_id));
    }
    preview.push_back(std::move(generated));
  }

  collision_preview_options_ = options;
  collision_preview_ = std::move(preview);
  return absl::OkStatus();
}

absl::Status MinecartTrackEditorPanel::BuildAllEligibleCollisionPreview() {
  RebuildAuditCache(/*include_unmaterialized=*/true);
  std::vector<int> room_ids;
  for (const auto& [room_id, audit] : room_audit_) {
    if (!audit.track_subtypes.empty() && !audit.has_any_custom_collision) {
      room_ids.push_back(room_id);
    }
  }
  return BuildCollisionPreview(room_ids);
}

absl::Status MinecartTrackEditorPanel::ApplyCollisionPreview() {
  if (collision_preview_.empty()) {
    return absl::FailedPreconditionError("No collision preview to apply");
  }
  if (!collision_batch_apply_callback_) {
    return absl::FailedPreconditionError(
        "Minecart collision apply is unavailable outside the dungeon editor");
  }
  RETURN_IF_ERROR(collision_batch_apply_callback_(collision_preview_,
                                                  collision_preview_options_));
  ClearCollisionPreview();
  audit_dirty_ = true;
  return absl::OkStatus();
}

void MinecartTrackEditorPanel::ClearCollisionPreview() {
  collision_preview_.clear();
  collision_preview_options_ = {};
}

void MinecartTrackEditorPanel::Draw(bool* p_open) {
  if (project_ == nullptr) {
    ImGui::TextColored(ImVec4(1, 0, 0, 1),
                       tr("Open a project to edit minecart tracks."));
    return;
  }

  const absl::Status binding_status = RefreshProjectBinding();
  if (!binding_status.ok()) {
    status_message_ = std::string(binding_status.message());
    show_success_ = false;
  }
  if (bound_project_filepath_.empty()) {
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
  if (picking_mode_) {
    if (ImGui::Button(ICON_MD_CANCEL " Cancel Pick")) {
      CancelCoordinatePicking();
    }
    ImGui::SameLine();
    ImGui::TextColored(ImVec4(1.0f, 0.8f, 0.2f, 1.0f),
                       ICON_MD_MY_LOCATION " Picking for Track %d...",
                       picking_track_index_);
  }
  if (!ImGui::BeginTabBar("##MinecartTasks")) {
    return;
  }

  if (ImGui::BeginTabItem(tr("Routes"))) {
    ImGui::TextDisabled(
        tr("Edit route start slots and publish the manifest-owned ASM "
           "source."));
#if defined(__EMSCRIPTEN__)
    ImGui::TextDisabled(
        tr("Source publishing is unavailable in browser builds; drafts are "
           "retained."));
#endif
    const bool has_unpublished_changes = HasUnpublishedChanges();
    const bool can_publish =
        has_unpublished_changes && kSourcePublishingAvailable;
    if (!can_publish) {
      ImGui::BeginDisabled();
    }
    if (ImGui::Button(ICON_MD_SAVE " Publish Tracks")) {
      const absl::Status status = SaveTracks();
      status_message_ =
          status.ok()
              ? "Minecart ASM source published only. Save pending dungeon/ROM "
                "edits to the development ROM, rebuild the patched ROM, then "
                "reopen/reload the patched ROM in Yaze before testing."
              : std::string(status.message());
      show_success_ = status.ok();
    }
    if (!can_publish) {
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
    const bool can_save_project =
        project_ && project_->project_opened() && project_save_callback_;
    if (!can_save_project) {
      ImGui::BeginDisabled();
    }
    if (ImGui::Button(ICON_MD_SAVE " Save Project")) {
      auto status = SaveProjectSettings();
      if (status.ok()) {
        status_message_ = has_unpublished_changes
                              ? "Project saved; minecart track drafts remain "
                                "unsaved."
                              : "Project saved.";
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

    ImGui::Separator();

    // Coordinate format help
    ImGui::TextDisabled(tr(
        "Camera coordinates use $1XXX format (base $1000 + room offset + local "
        "position)"));
    ImGui::TextDisabled(
        tr("Hover over dungeon canvas to see coordinates, or click 'Pick' "
           "button."));
    ImGui::Separator();

    if (ImGui::BeginTable("TracksTable", 7,
                          ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg |
                              ImGuiTableFlags_Resizable)) {
      ImGui::TableSetupColumn("ID", ImGuiTableColumnFlags_WidthFixed, 30.0f);
      ImGui::TableSetupColumn("Room ID", ImGuiTableColumnFlags_WidthFixed,
                              80.0f);
      ImGui::TableSetupColumn("Camera X", ImGuiTableColumnFlags_WidthFixed,
                              80.0f);
      ImGui::TableSetupColumn("Camera Y", ImGuiTableColumnFlags_WidthFixed,
                              80.0f);
      ImGui::TableSetupColumn("Pick", ImGuiTableColumnFlags_WidthFixed, 50.0f);
      ImGui::TableSetupColumn("Go", ImGuiTableColumnFlags_WidthFixed, 40.0f);
      ImGui::TableSetupColumn("Status", ImGuiTableColumnFlags_WidthFixed,
                              60.0f);
      ImGui::TableHeadersRow();

      for (auto& track : tracks_) {
        ImGui::TableNextRow();

        const bool is_default = IsDefaultTrack(track);
        const bool used_in_rooms =
            track.id >= 0 &&
            track.id < static_cast<int>(route_slot_used_.size()) &&
            route_slot_used_[track.id];
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
                absl::StrFormat("##Room%d", track.id).c_str(), &room_id,
                60.0f)) {
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
        bool is_picking_this =
            picking_mode_ && picking_track_index_ == track.id;
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
                               tr("Referenced by a cart but still default"));
          } else if (is_default) {
            ImGui::Text(tr("Default filler slot"));
          } else if (used_in_rooms) {
            ImGui::Text(tr("Referenced by a minecart sprite"));
          } else {
            ImGui::Text(tr("No route reference detected"));
          }

          auto rooms_it = route_usage_rooms_.find(track.id);
          if (rooms_it != route_usage_rooms_.end()) {
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
          track.id < static_cast<int>(route_slot_used_.size()) &&
          route_slot_used_[track.id];
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
    ImGui::Text(tr("%s route slots: used %d/%d, default %d, missing starts %d"),
                audit_includes_all_rooms_ ? "Project" : "Loaded-room",
                used_count, kTrackSlotCount, default_count,
                missing_start_count);
    ImGui::TextDisabled(
        tr("Route usage comes from minecart sprite subtypes, not visual track "
           "piece subtypes."));
    ImGui::EndTabItem();
  }

  if (ImGui::BeginTabItem(tr("Collision"))) {
    ImGui::TextDisabled(
        tr("Audit room collision, preview generated changes, then apply them "
           "as one undoable dungeon edit."));

    int rooms_needing_collision = 0;
    int protected_rooms = 0;
    for (const auto& [room_id, audit] : room_audit_) {
      if (!audit.track_subtypes.empty() && !audit.has_any_custom_collision) {
        ++rooms_needing_collision;
      } else if (!audit.track_subtypes.empty() &&
                 audit.has_any_custom_collision) {
        ++protected_rooms;
      }
    }
    ImGui::TextDisabled(tr("%s audit: %d room(s) need collision."),
                        audit_includes_all_rooms_ ? "Project" : "Loaded-room",
                        rooms_needing_collision);

    if (collision_preview_.empty()) {
      if (ImGui::Button(ICON_MD_PREVIEW " Preview All Rooms")) {
        const absl::Status status = BuildAllEligibleCollisionPreview();
        status_message_ = status.ok()
                              ? absl::StrFormat("Preview ready for %d rooms.",
                                                collision_preview_.size())
                              : std::string(status.message());
        show_success_ = status.ok();
      }
      ImGui::TextDisabled(
          tr("Scans all %d rooms. The list below otherwise reflects loaded "
             "rooms only."),
          static_cast<int>(rooms_->size()));
    } else {
      int preview_tiles = 0;
      for (const auto& result : collision_preview_) {
        preview_tiles += result.tiles_generated;
      }
      ImGui::Text(ICON_MD_PREVIEW " Preview: %d rooms, %d collision tiles",
                  collision_preview_.size(), preview_tiles);
      if (!collision_batch_apply_callback_) {
        ImGui::BeginDisabled();
      }
      if (ImGui::Button(absl::StrFormat(ICON_MD_CHECK_CIRCLE
                                        " Apply Preview to %d Rooms",
                                        collision_preview_.size())
                            .c_str())) {
        ImGui::OpenPopup("Confirm Minecart Collision Apply");
      }
      if (!collision_batch_apply_callback_) {
        ImGui::EndDisabled();
      }
      ImGui::SameLine();
      if (ImGui::Button(ICON_MD_CANCEL " Discard Preview")) {
        ClearCollisionPreview();
        status_message_ = "Collision preview discarded.";
        show_success_ = true;
      }

      if (ImGui::BeginPopupModal("Confirm Minecart Collision Apply", nullptr,
                                 ImGuiWindowFlags_AlwaysAutoResize)) {
        ImGui::Text(tr("Apply generated collision to %d rooms?"),
                    collision_preview_.size());
        ImGui::TextDisabled(tr("%d collision tiles will be added."),
                            preview_tiles);
        ImGui::TextDisabled(
            tr("This changes dungeon room models as one undoable edit. ROM "
               "bytes change only when you save."));
        if (ImGui::Button(ICON_MD_CHECK_CIRCLE " Apply Changes")) {
          const int applied_rooms = static_cast<int>(collision_preview_.size());
          const absl::Status status = ApplyCollisionPreview();
          status_message_ =
              status.ok()
                  ? absl::StrFormat(
                        "Applied collision to %d rooms. Save the ROM to "
                        "publish it.",
                        applied_rooms)
                  : std::string(status.message());
          show_success_ = status.ok();
          ImGui::CloseCurrentPopup();
        }
        ImGui::SameLine();
        if (ImGui::Button(ICON_MD_CANCEL " Cancel")) {
          ImGui::CloseCurrentPopup();
        }
        ImGui::EndPopup();
      }
    }

    if (protected_rooms > 0) {
      ImGui::TextDisabled(
          tr("%d room(s) with existing custom collision are protected and "
             "excluded."),
          protected_rooms);
    }

    ImGui::BeginChild("##TrackAuditRooms", ImVec2(0, 160), true);
    std::vector<int> audited_room_ids;
    audited_room_ids.reserve(room_audit_.size());
    for (const auto& [room_id, audit] : room_audit_) {
      audited_room_ids.push_back(room_id);
    }
    std::sort(audited_room_ids.begin(), audited_room_ids.end());
    for (int room_id : audited_room_ids) {
      const auto& audit = room_audit_.at(room_id);

      // Status icon
      if (!audit.has_track_collision && audit.has_any_custom_collision) {
        ImGui::TextColored(
            ImVec4(1.0f, 0.6f, 0.1f, 1.0f),
            ICON_MD_WARNING_AMBER
            " Room 0x%03X (existing custom collision; protected)",
            room_id);
      } else if (!audit.has_track_collision) {
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

      // Preview only; applying is a separate, explicit batch action above.
      if (rooms_ && !audit.track_subtypes.empty() &&
          !audit.has_any_custom_collision) {
        ImGui::SameLine();
        if (ImGui::SmallButton(
                absl::StrFormat(ICON_MD_PREVIEW " Preview##%d", room_id)
                    .c_str())) {
          const absl::Status status = BuildCollisionPreview({room_id});
          status_message_ =
              status.ok()
                  ? absl::StrFormat("Preview ready for room 0x%03X.", room_id)
                  : std::string(status.message());
          show_success_ = status.ok();
        }
        if (ImGui::IsItemHovered()) {
          ImGui::SetTooltip(
              tr("Preview generated collision without changing the room"));
        }
      }

      ImGui::PopID();
    }
    ImGui::EndChild();
    DrawOverlaySettings();
    ImGui::EndTabItem();
  }

  ImGui::EndTabBar();

  if (!status_message_.empty() && !picking_mode_) {
    ImGui::Separator();
    ImGui::TextColored(show_success_ ? ImVec4(0, 1, 0, 1) : ImVec4(1, 0, 0, 1),
                       "%s", status_message_.c_str());
  }
}

absl::Status MinecartTrackEditorPanel::LoadTracks() {
  load_attempted_ = true;

  if (project_ == nullptr || !project_->hack_manifest.loaded() ||
      !project_->hack_manifest.minecart_track_layout().source.has_value()) {
    return absl::FailedPreconditionError(
        "Hack manifest does not define minecart_tracks.source");
  }
  const core::MinecartTrackLayout::Source source_identity =
      *project_->hack_manifest.minecart_track_layout().source;

  std::filesystem::path source_path;
  ASSIGN_OR_RETURN(source_path, ResolveTrackSourcePath());
  std::string source_bytes;
  ASSIGN_OR_RETURN(source_bytes, ReadSourceFile(source_path));
  auto document_or =
      MinecartTrackSourceDocument::Parse(std::move(source_bytes));
  if (!document_or.ok()) {
    return document_or.status();
  }
  if (!project_->hack_manifest.minecart_track_layout().source.has_value() ||
      *project_->hack_manifest.minecart_track_layout().source !=
          source_identity) {
    return absl::AbortedError(
        "Hack manifest minecart_tracks.source changed while loading tracks");
  }

  std::vector<MinecartTrack> candidate_tracks = document_or->tracks();
  const std::string source_sha256 =
      core::ComputeSourceArtifactSha256(document_or->source_bytes());

  tracks_ = candidate_tracks;
  loaded_tracks_ = std::move(candidate_tracks);
  source_document_ = std::move(*document_or);
  loaded_source_identity_ = source_identity;
  loaded_source_path_ = std::move(source_path);
  loaded_source_sha256_ = source_sha256;
  loaded_ = true;
  audit_dirty_ = true;
  status_message_.clear();
  show_success_ = true;
  return absl::OkStatus();
}

absl::Status MinecartTrackEditorPanel::SaveTracks() {
  if (!loaded_ || !source_document_.has_value() ||
      !loaded_source_identity_.has_value() || loaded_source_path_.empty() ||
      loaded_source_sha256_.empty()) {
    return absl::FailedPreconditionError("Minecart tracks are not loaded");
  }
  if (!HasUnpublishedChanges()) {
    return absl::FailedPreconditionError(
        "No unpublished minecart track drafts to publish");
  }
#if defined(__EMSCRIPTEN__)
  return absl::FailedPreconditionError(
      "Minecart source publishing is unavailable in browser builds because "
      "durable atomic filesystem publication cannot be guaranteed; drafts "
      "were kept");
#else
  RETURN_IF_ERROR(RefreshProjectBinding());
  RETURN_IF_ERROR(ValidateLoadedManifestIdentity());

  std::filesystem::path source_path;
  ASSIGN_OR_RETURN(source_path, ResolveTrackSourcePath());
  if (source_path != loaded_source_path_) {
    return absl::FailedPreconditionError(
        "Minecart source path changed after the tracks were loaded; drafts "
        "were kept");
  }

  std::unique_ptr<core::SourceArtifactPublicationLock> publication_lock;
  ASSIGN_OR_RETURN(publication_lock,
                   core::AcquireSourceArtifactPublicationLock(
                       {source_path}, kMinecartSourcePublisherLabels));

  // Recheck every source identity after acquiring the durable publication
  // lock. A manifest reload or path replacement must fail before mutation.
  RETURN_IF_ERROR(ValidateLoadedManifestIdentity());
  std::filesystem::path locked_source_path;
  ASSIGN_OR_RETURN(locked_source_path, ResolveTrackSourcePath());
  if (locked_source_path != loaded_source_path_) {
    return absl::FailedPreconditionError(
        "Minecart source path changed while acquiring its publication lock; "
        "drafts were kept");
  }

  std::string source_before;
  ASSIGN_OR_RETURN(source_before, ReadSourceFile(locked_source_path));
  const std::string source_sha256_before =
      core::ComputeSourceArtifactSha256(source_before);
  if (source_sha256_before != loaded_source_sha256_ ||
      source_before != source_document_->source_bytes()) {
    return absl::AbortedError(absl::StrFormat(
        "Minecart source SHA-256 CAS failed: expected %s, got %s; drafts were "
        "kept",
        loaded_source_sha256_, source_sha256_before));
  }

  std::string source_after;
  ASSIGN_OR_RETURN(source_after, source_document_->Render(tracks_));
  auto published_document_or = MinecartTrackSourceDocument::Parse(source_after);
  if (!published_document_or.ok()) {
    return absl::DataLossError(
        absl::StrFormat("Rendered minecart source failed strict validation: %s",
                        published_document_or.status().message()));
  }
  if (published_document_or->tracks() != tracks_) {
    return absl::DataLossError(
        "Rendered minecart source did not reproduce the draft tracks");
  }
  const std::string source_sha256_after =
      core::ComputeSourceArtifactSha256(source_after);
  const std::vector<MinecartTrack> draft_tracks = tracks_;

  std::vector<core::SourceArtifactUpdate> updates;
  updates.push_back(core::SourceArtifactUpdate{
      .target = locked_source_path,
      .before = source_before,
      .after = source_after,
  });
  RETURN_IF_ERROR(core::PublishSourceArtifacts(
      *publication_lock, std::move(updates), loaded_source_sha256_,
      [&]() -> absl::Status {
        std::string reopened_source;
        ASSIGN_OR_RETURN(reopened_source, ReadSourceFile(locked_source_path));
        if (reopened_source != source_after ||
            core::ComputeSourceArtifactSha256(reopened_source) !=
                source_sha256_after) {
          return absl::DataLossError(
              "Published minecart source failed exact SHA-256 readback");
        }
        auto reopened_document_or =
            MinecartTrackSourceDocument::Parse(std::move(reopened_source));
        if (!reopened_document_or.ok()) {
          return absl::DataLossError(absl::StrFormat(
              "Published minecart source failed strict readback validation: "
              "%s",
              reopened_document_or.status().message()));
        }
        if (reopened_document_or->tracks() != draft_tracks) {
          return absl::DataLossError(
              "Published minecart source track readback did not match the "
              "draft");
        }
        return absl::OkStatus();
      }));

  tracks_ = published_document_or->tracks();
  loaded_tracks_ = tracks_;
  source_document_ = std::move(*published_document_or);
  loaded_source_path_ = std::move(locked_source_path);
  loaded_source_sha256_ = source_sha256_after;
  CancelCoordinatePicking();
  audit_dirty_ = true;
  return absl::OkStatus();
#endif
}

}  // namespace yaze::editor
