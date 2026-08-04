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
      if (token.starts_with('$')) {
        token.remove_prefix(1);
        base = 16;
      } else if (token.starts_with("0x") || token.starts_with("0X")) {
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
  ImGui::TextDisabled(
      tr("Publish Tracks changes the manifest-owned ASM source only; it does "
         "not update the open ROM."));
  ImGui::TextDisabled(
      tr("After publishing, save pending dungeon/ROM edits to the development "
         "ROM, rebuild the patched ROM, then reopen/reload the patched ROM in "
         "Yaze before testing."));
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
