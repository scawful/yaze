#include "app/editor/dungeon/ui/window/object_coverage_panel.h"

#include <algorithm>
#include <cctype>
#include <chrono>
#include <cstdio>
#include <cstring>
#include <fstream>
#include <iterator>
#include <string>
#include <vector>

#include "absl/strings/str_cat.h"
#include "absl/strings/str_format.h"
#include "absl/time/clock.h"
#include "absl/time/time.h"
#include "app/editor/dungeon/dungeon_project_labels.h"
#include "app/gui/core/ui_helpers.h"
#include "imgui/imgui.h"
#include "rom/rom.h"
#include "util/file_util.h"
#include "util/i18n/tr.h"
#include "util/platform_paths.h"
#include "util/rom_hash.h"
#include "zelda3/dungeon/draw_routines/draw_routine_registry.h"
#include "zelda3/dungeon/draw_routines/draw_routine_symbology.h"
#include "zelda3/dungeon/game_tilemap_comparison.h"
#include "zelda3/dungeon/object_draw_code.h"
#include "zelda3/dungeon/room.h"
#include "zelda3/dungeon/room_object.h"

namespace yaze {
namespace editor {
namespace {

constexpr char kEvidenceSubdir[] = "dungeon_object_evidence";

ImVec4 StateColor(ObjectEvidenceState state) {
  switch (state) {
    case ObjectEvidenceState::kUntriaged:
      return gui::GetDisabledColor();
    case ObjectEvidenceState::kReproduced:
      return gui::GetErrorColor();
    case ObjectEvidenceState::kFixedAwaitingProof:
      return gui::GetWarningColor();
    case ObjectEvidenceState::kVerified:
      return gui::GetSuccessColor();
    case ObjectEvidenceState::kIntentionalPreviewLimit:
      return gui::GetInfoColor();
  }
  return gui::GetDisabledColor();
}

std::string SanitizeForFilename(const std::string& value) {
  std::string out;
  out.reserve(value.size());
  for (char ch : value) {
    const unsigned char uch = static_cast<unsigned char>(ch);
    out.push_back(std::isalnum(uch) ? static_cast<char>(std::tolower(uch))
                                    : '_');
  }
  return out.empty() ? std::string("unnamed") : out;
}

bool ContainsCaseInsensitive(const std::string& haystack, const char* needle) {
  if (needle == nullptr || needle[0] == '\0') {
    return true;
  }
  const std::string lower_needle = SanitizeForFilename(needle);
  const std::string lower_haystack = SanitizeForFilename(haystack);
  return lower_haystack.find(lower_needle) != std::string::npos;
}

// ObjectOccurrence::layer is the room object list, not a BG number.
const char* ObjectListLabel(int list_index) {
  switch (list_index) {
    case zelda3::RoomObject::kListPrimary:
      return "main list";
    case zelda3::RoomObject::kListBg2Overlay:
      return "BG2 overlay";
    case zelda3::RoomObject::kListBg1Overlay:
      return "BG1 overlay";
  }
  return "list ?";
}

// Continues the current line when `width` more pixels fit before
// `right_edge` (screen X); otherwise the next item starts a new line. The
// Workbench inspector is narrow, so rows of controls must wrap.
void SameLineIfFits(float width, float right_edge) {
  const float line_end =
      ImGui::GetItemRectMax().x + ImGui::GetStyle().ItemSpacing.x + width;
  if (line_end <= right_edge) {
    ImGui::SameLine();
  }
}

float RowRightEdge() {
  return ImGui::GetCursorScreenPos().x + ImGui::GetContentRegionAvail().x;
}

float ButtonWidth(const char* label) {
  return ImGui::CalcTextSize(label, nullptr, true).x +
         ImGui::GetStyle().FramePadding.x * 2.0f;
}

float CheckboxWidth(const char* label) {
  return ImGui::GetFrameHeight() + ImGui::GetStyle().ItemInnerSpacing.x +
         ImGui::CalcTextSize(label, nullptr, true).x;
}

std::string NowUtc() {
  return absl::FormatTime("%Y-%m-%dT%H:%M:%SZ", absl::Now(),
                          absl::UTCTimeZone());
}

}  // namespace

void ObjectCoveragePanel::SetProject(project::YazeProject* project) {
  project_ = project;
  loaded_context_.clear();
}

void ObjectCoveragePanel::SetRooms(DungeonRoomStore* rooms) {
  rooms_ = rooms;
  index_dirty_ = true;
}

std::string ObjectCoveragePanel::EvidenceContextName() const {
  if (project_ != nullptr && !project_->name.empty()) {
    return "project_" + SanitizeForFilename(project_->name);
  }
  if (!rom_sha1_.empty()) {
    return "rom_" + rom_sha1_.substr(0, 12);
  }
  return std::string();
}

void ObjectCoveragePanel::RebuildIndex() {
  usage_ = ObjectUsageIndex{};
  custom_code_.clear();
  rom_sha1_.clear();
  auto& registry = zelda3::DrawRoutineRegistry::Get();
  registry.Initialize();
  review_order_ = OrderObjectsForReview(registry.GetMappedObjectIds());
  index_dirty_ = false;

  if (rooms_ == nullptr || rooms_->rom() == nullptr ||
      !rooms_->rom()->is_loaded()) {
    return;
  }
  const auto& rom_bytes = rooms_->rom()->vector();
  rom_sha1_ = util::ComputeSha1Hex(rom_bytes.data(), rom_bytes.size());
  const std::vector<core::ProtectedRegion> no_hooks;
  custom_code_ = FindObjectsWithCustomDrawCode(
      zelda3::ReadObjectDrawCode(*rooms_->rom()),
      project_ != nullptr && project_->hack_manifest.loaded()
          ? project_->hack_manifest.protected_regions()
          : no_hooks);

  // Opened rooms carry unsaved edits, so they win over the ROM copy; other
  // rooms are parsed into a temporary, as the minecart audit does.
  for (int room_id = 0; room_id < static_cast<int>(rooms_->size()); ++room_id) {
    if (auto* room = rooms_->GetIfMaterialized(room_id)) {
      room->EnsureObjectsLoaded();
      usage_.AddRoom(room_id, room->GetTileObjects());
      continue;
    }
    zelda3::Room room(room_id, rooms_->rom(), rooms_->game_data());
    room.EnsureObjectsLoaded();
    usage_.AddRoom(room_id, room.GetTileObjects());
  }
  // The ROM may have changed; recheck that the captures still belong to it.
  if (!loaded_context_.empty()) {
    auto_results_.Clear();
    ReloadManifest();
  }
}

void ObjectCoveragePanel::EnsureEvidenceLoaded() {
  const std::string context = EvidenceContextName();
  if (context.empty() || context == loaded_context_) {
    return;
  }
  loaded_context_ = context;
  evidence_ = ObjectEvidenceStore{};
  auto app_data = util::PlatformPaths::GetAppDataDirectory();
  if (!app_data.ok()) {
    evidence_path_.clear();
    status_message_ = std::string(app_data.status().message());
    status_is_error_ = true;
    return;
  }
  // The context name comes from the project name or the ROM hash. Both are
  // sanitized already; pin that here so nothing can escape the folder.
  evidence_path_ =
      *app_data / kEvidenceSubdir / (SafeEvidenceFileName(context) + ".json");
  auto loaded = ObjectEvidenceStore::LoadFromFile(evidence_path_);
  if (!loaded.ok()) {
    // Keep the unreadable file untouched: saving would overwrite it.
    status_message_ = absl::StrCat(loaded.status().message(),
                                   ". Verdicts will not be saved until it "
                                   "is fixed or moved.");
    status_is_error_ = true;
    evidence_path_.clear();
    return;
  }
  evidence_ = *std::move(loaded);
  status_message_.clear();
  status_is_error_ = false;
  auto_results_.Clear();
  ReloadManifest();
}

void ObjectCoveragePanel::SetCaptureDir(const std::string& dir) {
  if (!IsPlainCaptureDir(std::filesystem::path(dir))) {
    status_message_ = absl::StrCat(dir, " must not contain \"..\"");
    status_is_error_ = true;
    return;
  }
  evidence_.set_capture_dir(dir);
  SaveEvidence();
  auto_results_.Clear();
  ReloadManifest();
}

void ObjectCoveragePanel::ReloadManifest() {
  manifest_.reset();
  manifest_error_.clear();
  manifest_dir_ = evidence_.capture_dir();
  if (manifest_dir_.empty()) {
    return;
  }
  auto manifest = LoadGameCaptureManifest(manifest_dir_);
  if (!manifest.ok()) {
    manifest_error_ = std::string(manifest.status().message());
    return;
  }
  if (manifest->rom_sha1 != rom_sha1_) {
    manifest_error_ = absl::StrFormat(
        "These captures are for ROM %s; the open ROM is %s. Capture this ROM "
        "or choose another folder.",
        manifest->rom_sha1.substr(0, 12), rom_sha1_.substr(0, 12));
    return;
  }
  manifest_ = *std::move(manifest);
}

void ObjectCoveragePanel::StartAutomaticCheck() {
  if (!manifest_) {
    return;
  }
  auto_results_.Clear();
  auto_room_errors_.clear();
  auto_queue_ = manifest_->captured_rooms;
  auto_next_ = 0;
  auto_running_ = !auto_queue_.empty();
}

void ObjectCoveragePanel::StepAutomaticCheck() {
  if (!auto_running_) {
    return;
  }
  // Rendering a room takes tens of milliseconds, so spread the rooms over
  // frames and keep the editor responsive.
  const auto start = std::chrono::steady_clock::now();
  constexpr auto kFrameBudget = std::chrono::milliseconds(12);
  while (auto_next_ < auto_queue_.size()) {
    CheckRoomAgainstGame(auto_queue_[auto_next_++]);
    if (std::chrono::steady_clock::now() - start > kFrameBudget) {
      break;
    }
  }
  if (auto_next_ >= auto_queue_.size()) {
    auto_running_ = false;
  }
}

void ObjectCoveragePanel::CheckRoomAgainstGame(int room_id) {
  const auto path = GameCaptureRoomPath(manifest_dir_, room_id);
  std::ifstream in(path, std::ios::binary);
  const std::vector<uint8_t> bytes((std::istreambuf_iterator<char>(in)),
                                   std::istreambuf_iterator<char>());
  auto game = zelda3::ParseGameRoomTilemaps(bytes);
  if (!game.ok()) {
    auto_room_errors_.push_back(absl::StrFormat(
        "Room 0x%03X: %s", room_id, std::string(game.status().message())));
    return;
  }
  // Compare the ROM as saved; captures come from the saved ROM too.
  zelda3::Room room = zelda3::LoadRoomFromRom(rooms_->rom(), room_id);
  room.SetGameData(rooms_->game_data());
  room.RenderRoomGraphics();
  const auto yaze = zelda3::ComposeYazeRoomTilemaps(room);
  const auto& objects = room.GetTileObjects();
  std::vector<bool> hidden(objects.size());
  for (size_t i = 0; i < objects.size(); ++i) {
    hidden[i] = zelda3::GameHidesObjectOnRoomLoad(static_cast<int>(room.tag1()),
                                                  static_cast<int>(room.tag2()),
                                                  objects[i]);
  }
  const auto owners = zelda3::ComputeObjectTileOwners(rooms_->rom(), room_id,
                                                      objects, yaze, hidden);
  const auto check =
      zelda3::CompareRoomTilemaps(room_id, *game, yaze, owners, objects);
  auto_results_.RecordRoom(check.differences.empty());
  for (const auto& placement : check.placements) {
    PlacementAutoResult result;
    result.tiles_owned = placement.tiles_owned;
    result.tiles_different = placement.tiles_different;
    result.difference_bits = placement.difference_bits;
    auto_results_.Record(room_id, placement.object_index, placement.object_id,
                         result);
  }
}

void ObjectCoveragePanel::ApplyAutomaticVerdicts(ObjectEvidenceState state) {
  int applied = 0;
  for (auto& [object_id, proposal] :
       ProposeAutomaticVerdicts(auto_results_, evidence_, rom_sha1_)) {
    if (proposal.state != state) {
      continue;
    }
    proposal.updated_utc = NowUtc();
    evidence_.Set(object_id, std::move(proposal));
    ++applied;
  }
  SaveEvidence();
  if (status_message_.empty()) {
    status_message_ = absl::StrFormat("Marked %d objects as \"%s\".", applied,
                                      ObjectEvidenceStateLabel(state));
  }
}

void ObjectCoveragePanel::SaveEvidence() {
  if (evidence_path_.empty()) {
    return;
  }
  const absl::Status status = evidence_.SaveToFile(evidence_path_);
  if (!status.ok()) {
    status_message_ = std::string(status.message());
    status_is_error_ = true;
    return;
  }
  status_message_.clear();
  status_is_error_ = false;
}

void ObjectCoveragePanel::SetVerdict(int object_id, ObjectEvidenceState state) {
  ObjectEvidence evidence;
  if (const ObjectEvidence* existing = evidence_.Find(object_id)) {
    evidence = *existing;
  }
  evidence.state = state;
  auto room_it = last_room_for_object_.find(object_id);
  if (room_it != last_room_for_object_.end()) {
    evidence.room_id = room_it->second;
  }
  evidence.rom_sha1 = rom_sha1_;
  evidence.updated_utc = NowUtc();
  evidence_.Set(object_id, std::move(evidence));
  SaveEvidence();
}

void ObjectCoveragePanel::FocusObject(int object_id, int room_id) {
  selected_object_ = object_id;
  if (room_id >= 0) {
    last_room_for_object_[object_id] = room_id;
  }
  // Clear filters that could hide the row being focused.
  filter_text_[0] = '\0';
  state_filter_ = -1;
  only_focus_ = false;
  only_placed_ = false;
  scroll_to_selected_ = true;
}

void ObjectCoveragePanel::GoToOccurrence(int object_id,
                                         const ObjectOccurrence& occurrence) {
  last_room_for_object_[object_id] = occurrence.room_id;
  if (navigate_) {
    navigate_(occurrence.room_id, occurrence.object_index, object_id);
  }
}

void ObjectCoveragePanel::GoToNextObject() {
  const std::optional<int> next =
      NextObjectToCheck(review_order_, evidence_, usage_, selected_object_);
  if (!next) {
    status_message_ = tr("Every placed object has a verdict.");
    status_is_error_ = false;
    return;
  }
  selected_object_ = *next;
  scroll_to_selected_ = true;
  if (const auto* occurrences = usage_.Find(*next);
      occurrences != nullptr && !occurrences->empty()) {
    GoToOccurrence(*next, occurrences->front());
  }
}

bool ObjectCoveragePanel::PassesFilters(int object_id) const {
  if (only_placed_ && usage_.Find(object_id) == nullptr) {
    return false;
  }
  if (only_focus_ && ReleaseFocusGroupFor(object_id) < 0) {
    return false;
  }
  if (only_auto_differences_) {
    const auto* summary = auto_results_.Summary(object_id);
    if (summary == nullptr || summary->placements_different == 0) {
      return false;
    }
  }
  if (state_filter_ >= 0 &&
      static_cast<int>(evidence_.StateOf(object_id)) != state_filter_) {
    return false;
  }
  if (filter_text_[0] != '\0') {
    const std::string haystack = absl::StrCat(
        FormatObjectId(object_id), " ", zelda3::GetObjectName(object_id), " ",
        zelda3::GetSymbologyForObject(static_cast<int16_t>(object_id)).family);
    if (!ContainsCaseInsensitive(haystack, filter_text_)) {
      return false;
    }
  }
  return true;
}

void ObjectCoveragePanel::Draw(bool* /*p_open*/) {
  if (rooms_ == nullptr || rooms_->rom() == nullptr ||
      !rooms_->rom()->is_loaded()) {
    ImGui::TextDisabled("%s", tr("Load a ROM to check dungeon objects."));
    return;
  }
  if (index_dirty_) {
    RebuildIndex();
  }
  EnsureEvidenceLoaded();

  StepAutomaticCheck();
  DrawSummary();
  DrawAutomaticCheck();
  ImGui::Separator();
  DrawFilters();

  const float details_height = ImGui::GetContentRegionAvail().y * 0.45f;
  DrawObjectTable(ImGui::GetContentRegionAvail().y - details_height);
  ImGui::Separator();
  DrawDetails();
}

void ObjectCoveragePanel::DrawSummary() {
  int placed = 0;
  int counts[std::size(kAllObjectEvidenceStates)] = {};
  for (int object_id : review_order_) {
    if (usage_.Find(object_id) == nullptr) {
      continue;
    }
    ++placed;
    ++counts[static_cast<int>(evidence_.StateOf(object_id))];
  }
  const int checked =
      placed - counts[static_cast<int>(ObjectEvidenceState::kUntriaged)];

  ImGui::TextWrapped(tr("Checked %d of %d objects placed in this ROM (%d "
                        "rooms scanned)"),
                     checked, placed, usage_.rooms_scanned());
  ImGui::ProgressBar(placed > 0 ? static_cast<float>(checked) / placed : 0.0f,
                     ImVec2(-1.0f, 0.0f));

  const float right_edge = RowRightEdge();
  bool first = true;
  for (ObjectEvidenceState state : kAllObjectEvidenceStates) {
    if (state == ObjectEvidenceState::kUntriaged) {
      continue;
    }
    const std::string label =
        absl::StrFormat("%s %d", ObjectEvidenceStateLabel(state),
                        counts[static_cast<int>(state)]);
    if (!first) {
      SameLineIfFits(ImGui::CalcTextSize(label.c_str()).x, right_edge);
    }
    first = false;
    ImGui::TextColored(StateColor(state), "%s", label.c_str());
  }

  const std::string next_label =
      absl::StrCat(ICON_MD_SKIP_NEXT " ", tr("Next object to check"));
  const std::string rescan_label =
      absl::StrCat(ICON_MD_REFRESH " ", tr("Rescan rooms"));
  if (ImGui::Button(next_label.c_str())) {
    GoToNextObject();
  }
  if (ImGui::IsItemHovered()) {
    ImGui::SetTooltip(
        "%s", tr("Opens the next unchecked object in release-focus order, in "
                 "a room that uses it, with the object selected."));
  }
  SameLineIfFits(ButtonWidth(rescan_label.c_str()), right_edge);
  if (ImGui::Button(rescan_label.c_str())) {
    index_dirty_ = true;
  }

  if (!status_message_.empty()) {
    ImGui::PushStyleColor(ImGuiCol_Text, status_is_error_
                                             ? gui::GetErrorColor()
                                             : gui::GetInfoColor());
    ImGui::TextWrapped("%s", status_message_.c_str());
    ImGui::PopStyleColor();
  }
  if (!evidence_path_.empty() && ImGui::IsItemHovered()) {
    ImGui::SetTooltip("%s", evidence_path_.string().c_str());
  }
}

// Compares yaze's tilemaps with the captured game tilemaps, a few rooms per
// frame, and offers to record the matching objects as verdicts.
void ObjectCoveragePanel::DrawAutomaticCheck() {
  if (!ImGui::CollapsingHeader(tr("Automatic check against the game"))) {
    return;
  }
  ImGui::PushID("ObjectCoverageAuto");
  const std::string& dir = evidence_.capture_dir();
  ImGui::TextWrapped("%s %s", tr("Game captures:"),
                     dir.empty() ? tr("none chosen") : dir.c_str());
  if (ImGui::Button(
          absl::StrCat(ICON_MD_FOLDER_OPEN " ", tr("Choose capture folder"))
              .c_str())) {
    const std::string chosen = util::FileDialogWrapper::ShowOpenFolderDialog();
    if (!chosen.empty()) {
      SetCaptureDir(chosen);
    }
  }
  if (ImGui::IsItemHovered()) {
    ImGui::SetTooltip(
        "%s",
        tr("A folder written by scripts/agents/capture-game-room-tilemaps.py "
           "from Mesen2-OOS running this same ROM. It holds each room's "
           "tilemaps as the game built them."));
  }

  if (!manifest_error_.empty()) {
    ImGui::PushStyleColor(ImGuiCol_Text, gui::GetErrorColor());
    ImGui::TextWrapped("%s", manifest_error_.c_str());
    ImGui::PopStyleColor();
  } else if (manifest_) {
    ImGui::TextWrapped(tr("%zu rooms captured from the game for this ROM."),
                       manifest_->captured_rooms.size());
    if (auto_running_) {
      const float fraction =
          auto_queue_.empty()
              ? 1.0f
              : static_cast<float>(auto_next_) / auto_queue_.size();
      ImGui::ProgressBar(
          fraction, ImVec2(-1.0f, 0.0f),
          absl::StrFormat("%zu / %zu rooms", auto_next_, auto_queue_.size())
              .c_str());
    } else if (ImGui::Button(absl::StrCat(ICON_MD_PLAY_ARROW " ",
                                          tr("Run automatic check"))
                                 .c_str())) {
      StartAutomaticCheck();
    }
  }

  DrawAutomaticCheckResults();
  ImGui::PopID();
}

// Summary of the last automatic check, its errors, and the buttons that turn
// its results into verdicts.
void ObjectCoveragePanel::DrawAutomaticCheckResults() {
  if (!auto_running_ && auto_results_.rooms_compared() > 0) {
    int objects_matching = 0;
    int objects_differing = 0;
    for (const auto& [object_id, summary] : auto_results_.summaries()) {
      (summary.placements_different == 0 ? objects_matching
                                         : objects_differing)++;
    }
    ImGui::TextWrapped(
        tr("Compared %d rooms (%d exact). Objects matching the game in every "
           "placement: %d. Objects with differences: %d."),
        auto_results_.rooms_compared(), auto_results_.rooms_exact(),
        objects_matching, objects_differing);
    ImGui::TextDisabled(
        "%s", tr("Compares tiles, flips, palette rows and priority. Colors and "
                 "graphics sheets are not compared."));
    for (const std::string& error : auto_room_errors_) {
      ImGui::TextColored(gui::GetErrorColor(), "%s", error.c_str());
    }

    int propose_verified = 0;
    int propose_broken = 0;
    for (const auto& [object_id, proposal] :
         ProposeAutomaticVerdicts(auto_results_, evidence_, rom_sha1_)) {
      (proposal.state == ObjectEvidenceState::kVerified ? propose_verified
                                                        : propose_broken)++;
    }
    const float right_edge = RowRightEdge();
    const std::string verified_label = absl::StrFormat(
        "%s %s (%d)", ICON_MD_CHECK, tr("Mark matches"), propose_verified);
    const std::string broken_label =
        absl::StrFormat("%s %s (%d)", ICON_MD_CLOSE,
                        tr("Mark differences as Broken"), propose_broken);
    ImGui::BeginDisabled(propose_verified == 0);
    if (ImGui::Button(verified_label.c_str())) {
      ApplyAutomaticVerdicts(ObjectEvidenceState::kVerified);
    }
    ImGui::EndDisabled();
    if (ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled)) {
      ImGui::SetTooltip(
          "%s", tr("Sets \"Matches game\" on unchecked objects whose every "
                   "checked placement matches. Objects you already judged are "
                   "left alone."));
    }
    SameLineIfFits(ButtonWidth(broken_label.c_str()), right_edge);
    ImGui::BeginDisabled(propose_broken == 0);
    if (ImGui::Button(broken_label.c_str())) {
      ApplyAutomaticVerdicts(ObjectEvidenceState::kReproduced);
    }
    ImGui::EndDisabled();
    if (ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled)) {
      ImGui::SetTooltip(
          "%s", tr("Sets \"Broken\" on unchecked objects with any differing "
                   "placement, with a note naming what differs."));
    }
  }
}

void ObjectCoveragePanel::DrawFilters() {
  const float right_edge = RowRightEdge();
  const float available = ImGui::GetContentRegionAvail().x;
  const float spacing = ImGui::GetStyle().ItemSpacing.x;
  // Two fields share a line when there is room for both at a usable width.
  const bool fields_share_line = available >= 300.0f + spacing;
  const float search_width =
      fields_share_line ? std::min(200.0f, available - 140.0f - spacing)
                        : available;
  ImGui::SetNextItemWidth(search_width);
  ImGui::InputTextWithHint("##ObjectCoverageFilter", tr("Search ID or name"),
                           filter_text_, sizeof(filter_text_));
  if (fields_share_line) {
    ImGui::SameLine();
  }
  ImGui::SetNextItemWidth(fields_share_line ? 140.0f : available);
  const char* preview =
      state_filter_ < 0 ? tr("Any status")
                        : ObjectEvidenceStateLabel(
                              static_cast<ObjectEvidenceState>(state_filter_));
  if (ImGui::BeginCombo("##ObjectCoverageState", preview)) {
    if (ImGui::Selectable(tr("Any status"), state_filter_ < 0)) {
      state_filter_ = -1;
    }
    for (ObjectEvidenceState state : kAllObjectEvidenceStates) {
      const int value = static_cast<int>(state);
      if (ImGui::Selectable(ObjectEvidenceStateLabel(state),
                            state_filter_ == value)) {
        state_filter_ = value;
      }
    }
    ImGui::EndCombo();
  }
  SameLineIfFits(CheckboxWidth(tr("Placed only")), right_edge);
  ImGui::Checkbox(tr("Placed only"), &only_placed_);
  SameLineIfFits(CheckboxWidth(tr("Release focus only")), right_edge);
  ImGui::Checkbox(tr("Release focus only"), &only_focus_);
  if (auto_results_.rooms_compared() > 0) {
    SameLineIfFits(CheckboxWidth(tr("Differs from game")), right_edge);
    ImGui::Checkbox(tr("Differs from game"), &only_auto_differences_);
  }
}

void ObjectCoveragePanel::DrawObjectTable(float height) {
  std::vector<int> rows;
  rows.reserve(review_order_.size());
  for (int object_id : review_order_) {
    if (PassesFilters(object_id)) {
      rows.push_back(object_id);
    }
  }

  constexpr ImGuiTableFlags kFlags =
      ImGuiTableFlags_RowBg | ImGuiTableFlags_BordersInnerV |
      ImGuiTableFlags_ScrollY | ImGuiTableFlags_Resizable |
      ImGuiTableFlags_SizingStretchProp;
  // The Workbench inspector is narrow; there the details pane shows the
  // focus family instead of a column.
  const bool show_focus_column = ImGui::GetContentRegionAvail().x >= 420.0f;
  const bool show_auto_column = auto_results_.rooms_compared() > 0;
  const int column_count =
      4 + (show_focus_column ? 1 : 0) + (show_auto_column ? 1 : 0);
  if (!ImGui::BeginTable("##ObjectCoverageTable", column_count, kFlags,
                         ImVec2(0.0f, std::max(height, 80.0f)))) {
    return;
  }
  ImGui::TableSetupScrollFreeze(0, 1);
  ImGui::TableSetupColumn(tr("ID"), ImGuiTableColumnFlags_WidthFixed, 48.0f);
  ImGui::TableSetupColumn(tr("Name"), ImGuiTableColumnFlags_WidthStretch, 3.0f);
  if (show_focus_column) {
    ImGui::TableSetupColumn(tr("Focus"), ImGuiTableColumnFlags_WidthStretch,
                            1.5f);
  }
  ImGui::TableSetupColumn(tr("Rooms"), ImGuiTableColumnFlags_WidthFixed, 44.0f);
  if (show_auto_column) {
    ImGui::TableSetupColumn(tr("Game"), ImGuiTableColumnFlags_WidthFixed,
                            56.0f);
  }
  ImGui::TableSetupColumn(tr("Status"), ImGuiTableColumnFlags_WidthStretch,
                          1.5f);
  ImGui::TableHeadersRow();

  const auto& groups = ReleaseFocusGroups();
  ImGuiListClipper clipper;
  clipper.Begin(static_cast<int>(rows.size()));
  int scroll_row = -1;
  if (scroll_to_selected_ && selected_object_) {
    auto it = std::find(rows.begin(), rows.end(), *selected_object_);
    if (it != rows.end()) {
      scroll_row = static_cast<int>(it - rows.begin());
      clipper.IncludeItemByIndex(scroll_row);
    }
    scroll_to_selected_ = false;
  }
  while (clipper.Step()) {
    for (int row = clipper.DisplayStart; row < clipper.DisplayEnd; ++row) {
      const int object_id = rows[static_cast<size_t>(row)];
      ImGui::TableNextRow();
      ImGui::PushID(object_id);

      ImGui::TableSetColumnIndex(0);
      const bool selected = selected_object_ == object_id;
      if (ImGui::Selectable(FormatObjectId(object_id).c_str(), selected,
                            ImGuiSelectableFlags_SpanAllColumns)) {
        selected_object_ = object_id;
      }
      if (row == scroll_row) {
        ImGui::SetScrollHereY(0.3f);
      }
      ImGui::TableSetColumnIndex(1);
      ImGui::TextUnformatted(zelda3::GetObjectName(object_id).c_str());
      if (custom_code_.contains(object_id)) {
        ImGui::SameLine();
        ImGui::TextColored(gui::GetWarningColor(), ICON_MD_CODE);
        if (ImGui::IsItemHovered()) {
          ImGui::SetTooltip("%s",
                            tr("Your ROM changes this object's draw code"));
        }
      }
      int column = 2;
      if (show_focus_column) {
        ImGui::TableSetColumnIndex(column++);
        const int group = ReleaseFocusGroupFor(object_id);
        if (group >= 0) {
          ImGui::TextUnformatted(groups[static_cast<size_t>(group)].name);
        }
      }
      ImGui::TableSetColumnIndex(column++);
      const int room_count = usage_.RoomCountFor(object_id);
      if (room_count > 0) {
        ImGui::Text("%d", room_count);
      } else {
        ImGui::TextDisabled("-");
      }
      if (show_auto_column) {
        ImGui::TableSetColumnIndex(column++);
        if (const auto* summary = auto_results_.Summary(object_id)) {
          if (summary->placements_different == 0) {
            ImGui::TextColored(gui::GetSuccessColor(), "%s %d", ICON_MD_CHECK,
                               summary->placements_checked);
          } else {
            ImGui::TextColored(gui::GetErrorColor(), "%s %d/%d", ICON_MD_CLOSE,
                               summary->placements_different,
                               summary->placements_checked);
          }
          if (ImGui::IsItemHovered()) {
            ImGui::SetTooltip(
                "%s",
                summary->placements_different == 0
                    ? tr("Every checked placement matches the game's tilemap.")
                    : absl::StrFormat(
                          "%d of %d placements differ from the game: %s",
                          summary->placements_different,
                          summary->placements_checked,
                          zelda3::DescribeTileWordDifference(
                              summary->difference_bits))
                          .c_str());
          }
        }
      }
      ImGui::TableSetColumnIndex(column);
      const ObjectEvidenceState state = evidence_.StateOf(object_id);
      if (selected) {
        // State colors are unreadable on the selection highlight.
        ImGui::TextUnformatted(ObjectEvidenceStateLabel(state));
      } else {
        ImGui::TextColored(StateColor(state), "%s",
                           ObjectEvidenceStateLabel(state));
      }
      ImGui::PopID();
    }
  }
  ImGui::EndTable();
}

// Shows the selected object's routine, check hint, rooms and verdict editor.
void ObjectCoveragePanel::DrawDetails() {
  if (!ImGui::BeginChild("##ObjectCoverageDetails", ImVec2(0, 0), false)) {
    ImGui::EndChild();
    return;
  }
  if (!selected_object_) {
    ImGui::TextWrapped(
        "%s",
        tr("Press \"Next object to check\", or pick an object above. For each "
           "object: open a room that uses it, compare it with the same room "
           "in the game, then set its status."));
    ImGui::EndChild();
    return;
  }

  const int object_id = *selected_object_;
  const auto symbology =
      zelda3::GetSymbologyForObject(static_cast<int16_t>(object_id));
  const auto& registry = zelda3::DrawRoutineRegistry::Get();
  const auto* routine = registry.GetRoutineInfo(
      registry.GetRoutineIdForObject(static_cast<int16_t>(object_id)));

  ImGui::Text("%s  %s", FormatObjectId(object_id).c_str(),
              zelda3::GetObjectName(object_id).c_str());
  ImGui::PushStyleColor(ImGuiCol_Text, gui::GetDisabledColor());
  ImGui::TextWrapped("%s: %s | %s", tr("Routine"),
                     routine != nullptr ? routine->name.c_str() : "-",
                     symbology.family.c_str());
  ImGui::PopStyleColor();

  if (auto it = custom_code_.find(object_id); it != custom_code_.end()) {
    const auto& custom = it->second;
    const std::string where =
        custom.module.empty()
            ? absl::StrFormat("$%06X", custom.address)
            : absl::StrFormat("$%06X (%s)", custom.address, custom.module);
    ImGui::PushStyleColor(ImGuiCol_Text, gui::GetWarningColor());
    ImGui::TextWrapped(
        custom.replaced
            ? tr(ICON_MD_CODE " Your ROM replaces this object's draw routine "
                              "with its own code at %s. yaze may not draw it "
                              "the way the game does; a mismatch here is not "
                              "necessarily a yaze bug.")
            : tr(ICON_MD_CODE " Your ROM patches this object's draw routine "
                              "at %s. A mismatch with the game may come from "
                              "that patch, not from yaze."),
        where.c_str());
    ImGui::PopStyleColor();
  }

  const int group = ReleaseFocusGroupFor(object_id);
  if (group >= 0) {
    const auto& focus = ReleaseFocusGroups()[static_cast<size_t>(group)];
    ImGui::TextColored(gui::GetInfoColor(), "%s:", focus.name);
    ImGui::SameLine();
    ImGui::TextWrapped("%s", focus.what_to_check);
  }

  if (const auto* summary = auto_results_.Summary(object_id)) {
    if (summary->placements_different == 0) {
      ImGui::PushStyleColor(ImGuiCol_Text, gui::GetSuccessColor());
      ImGui::TextWrapped(
          tr(ICON_MD_CHECK " Matches the game in all %d checked placements"),
          summary->placements_checked);
    } else {
      ImGui::PushStyleColor(ImGuiCol_Text, gui::GetErrorColor());
      const std::string what =
          zelda3::DescribeTileWordDifference(summary->difference_bits);
      ImGui::TextWrapped(
          tr(ICON_MD_CLOSE " %d of %d checked placements differ from the "
                           "game: %s"),
          summary->placements_different, summary->placements_checked,
          what.c_str());
    }
    ImGui::PopStyleColor();
  }

  DrawVerdictEditor(object_id);
  DrawObjectOccurrences(object_id);
  ImGui::EndChild();
}

// Verdict buttons, the note field, and when the verdict was last set.
void ObjectCoveragePanel::DrawVerdictEditor(int object_id) {
  // Verdict buttons, current one highlighted.
  const ObjectEvidenceState current = evidence_.StateOf(object_id);
  const float verdict_right_edge = RowRightEdge();
  bool first_verdict = true;
  for (ObjectEvidenceState state : kAllObjectEvidenceStates) {
    const bool is_current = state == current;
    if (!first_verdict) {
      SameLineIfFits(ButtonWidth(ObjectEvidenceStateLabel(state)),
                     verdict_right_edge);
    }
    first_verdict = false;
    if (is_current) {
      ImGui::PushStyleColor(ImGuiCol_Button, StateColor(state));
    }
    if (ImGui::Button(ObjectEvidenceStateLabel(state))) {
      SetVerdict(object_id, state);
    }
    if (is_current) {
      ImGui::PopStyleColor();
    }
  }

  // Note, saved when the field loses focus.
  if (note_buffer_object_ != object_id) {
    const ObjectEvidence* evidence = evidence_.Find(object_id);
    std::snprintf(note_buffer_, sizeof(note_buffer_), "%s",
                  evidence ? evidence->note.c_str() : "");
    note_buffer_object_ = object_id;
  }
  ImGui::SetNextItemWidth(-1.0f);
  ImGui::InputTextWithHint("##ObjectCoverageNote",
                           tr("Note: what differs, which room, which size"),
                           note_buffer_, sizeof(note_buffer_));
  if (ImGui::IsItemDeactivatedAfterEdit()) {
    ObjectEvidence evidence;
    if (const ObjectEvidence* existing = evidence_.Find(object_id)) {
      evidence = *existing;
    }
    evidence.note = note_buffer_;
    evidence.updated_utc = NowUtc();
    evidence_.Set(object_id, std::move(evidence));
    SaveEvidence();
  }
  if (const ObjectEvidence* evidence = evidence_.Find(object_id);
      evidence != nullptr && evidence->room_id >= 0) {
    ImGui::TextDisabled(tr("Last verdict in room 0x%03X at %s"),
                        evidence->room_id, evidence->updated_utc.c_str());
  }
}

// Lists the rooms that place this object, each one a link that opens it.
void ObjectCoveragePanel::DrawObjectOccurrences(int object_id) {
  const auto* occurrences = usage_.Find(object_id);
  if (occurrences == nullptr) {
    ImGui::TextDisabled(
        "%s", tr("Not placed in any room of this ROM. Place it in a scratch "
                 "room to check it."));
    return;
  }
  ImGui::Text(tr("Placed %zu times in %d rooms:"), occurrences->size(),
              usage_.RoomCountFor(object_id));
  for (size_t i = 0; i < occurrences->size(); ++i) {
    const ObjectOccurrence& occurrence = (*occurrences)[i];
    ImGui::PushID(static_cast<int>(i));
    if (ImGui::SmallButton(tr("Go"))) {
      GoToOccurrence(object_id, occurrence);
    }
    ImGui::SameLine();
    const std::string label =
        dungeon_project_labels::GetRoomLabel(project_, occurrence.room_id);
    ImGui::Text("0x%03X %s  (%d,%d) size %d, %s", occurrence.room_id,
                label.c_str(), occurrence.x, occurrence.y, occurrence.size,
                ObjectListLabel(occurrence.layer));
    const PlacementAutoResult* result =
        auto_results_.Find(occurrence.room_id, occurrence.object_index);
    if (result != nullptr && result->object_id == object_id &&
        result->tiles_owned > 0) {
      ImGui::SameLine();
      if (result->tiles_different == 0) {
        ImGui::TextColored(gui::GetSuccessColor(), "%s", ICON_MD_CHECK);
      } else {
        ImGui::TextColored(
            gui::GetErrorColor(), "%s %d/%d tiles: %s", ICON_MD_CLOSE,
            result->tiles_different, result->tiles_owned,
            zelda3::DescribeTileWordDifference(result->difference_bits)
                .c_str());
      }
    }
    ImGui::PopID();
  }
}

}  // namespace editor
}  // namespace yaze
