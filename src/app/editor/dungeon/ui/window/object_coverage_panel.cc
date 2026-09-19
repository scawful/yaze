#include "app/editor/dungeon/ui/window/object_coverage_panel.h"

#include <algorithm>
#include <cctype>
#include <cstdio>
#include <cstring>
#include <iterator>
#include <string>

#include "absl/strings/str_cat.h"
#include "absl/strings/str_format.h"
#include "absl/time/clock.h"
#include "absl/time/time.h"
#include "app/editor/dungeon/dungeon_project_labels.h"
#include "app/gui/core/ui_helpers.h"
#include "imgui/imgui.h"
#include "rom/rom.h"
#include "util/i18n/tr.h"
#include "util/platform_paths.h"
#include "util/rom_hash.h"
#include "zelda3/dungeon/draw_routines/draw_routine_registry.h"
#include "zelda3/dungeon/draw_routines/draw_routine_symbology.h"
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
  evidence_path_ = *app_data / kEvidenceSubdir / (context + ".json");
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

  DrawSummary();
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

  ImGui::Text(tr("Checked %d of %d objects placed in this ROM"), checked,
              placed);
  ImGui::SameLine();
  ImGui::TextDisabled(tr("(%d rooms scanned)"), usage_.rooms_scanned());
  ImGui::ProgressBar(placed > 0 ? static_cast<float>(checked) / placed : 0.0f,
                     ImVec2(-1.0f, 0.0f));

  bool first = true;
  for (ObjectEvidenceState state : kAllObjectEvidenceStates) {
    if (state == ObjectEvidenceState::kUntriaged) {
      continue;
    }
    if (!first) {
      ImGui::SameLine();
    }
    first = false;
    ImGui::TextColored(StateColor(state), "%s %d",
                       ObjectEvidenceStateLabel(state),
                       counts[static_cast<int>(state)]);
  }

  if (ImGui::Button(
          absl::StrCat(ICON_MD_SKIP_NEXT " ", tr("Next object to check"))
              .c_str())) {
    GoToNextObject();
  }
  if (ImGui::IsItemHovered()) {
    ImGui::SetTooltip(
        "%s", tr("Opens the next unchecked object in release-focus order, in "
                 "a room that uses it, with the object selected."));
  }
  ImGui::SameLine();
  if (ImGui::Button(
          absl::StrCat(ICON_MD_REFRESH " ", tr("Rescan rooms")).c_str())) {
    index_dirty_ = true;
  }

  if (!status_message_.empty()) {
    ImGui::TextColored(
        status_is_error_ ? gui::GetErrorColor() : gui::GetInfoColor(), "%s",
        status_message_.c_str());
  }
  if (!evidence_path_.empty() && ImGui::IsItemHovered()) {
    ImGui::SetTooltip("%s", evidence_path_.string().c_str());
  }
}

void ObjectCoveragePanel::DrawFilters() {
  ImGui::SetNextItemWidth(160.0f);
  ImGui::InputTextWithHint("##ObjectCoverageFilter", tr("Search ID or name"),
                           filter_text_, sizeof(filter_text_));
  ImGui::SameLine();
  ImGui::SetNextItemWidth(140.0f);
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
  ImGui::SameLine();
  ImGui::Checkbox(tr("Placed only"), &only_placed_);
  ImGui::SameLine();
  ImGui::Checkbox(tr("Release focus only"), &only_focus_);
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
  if (!ImGui::BeginTable("##ObjectCoverageTable", 5, kFlags,
                         ImVec2(0.0f, std::max(height, 80.0f)))) {
    return;
  }
  ImGui::TableSetupScrollFreeze(0, 1);
  ImGui::TableSetupColumn(tr("ID"), ImGuiTableColumnFlags_WidthFixed, 48.0f);
  ImGui::TableSetupColumn(tr("Name"), ImGuiTableColumnFlags_WidthStretch, 3.0f);
  ImGui::TableSetupColumn(tr("Focus"), ImGuiTableColumnFlags_WidthStretch,
                          1.5f);
  ImGui::TableSetupColumn(tr("Rooms"), ImGuiTableColumnFlags_WidthFixed, 44.0f);
  ImGui::TableSetupColumn(tr("Status"), ImGuiTableColumnFlags_WidthStretch,
                          1.5f);
  ImGui::TableHeadersRow();

  const auto& groups = ReleaseFocusGroups();
  ImGuiListClipper clipper;
  clipper.Begin(static_cast<int>(rows.size()));
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
      ImGui::TableSetColumnIndex(1);
      ImGui::TextUnformatted(zelda3::GetObjectName(object_id).c_str());
      ImGui::TableSetColumnIndex(2);
      const int group = ReleaseFocusGroupFor(object_id);
      if (group >= 0) {
        ImGui::TextUnformatted(groups[static_cast<size_t>(group)].name);
      }
      ImGui::TableSetColumnIndex(3);
      const int room_count = usage_.RoomCountFor(object_id);
      if (room_count > 0) {
        ImGui::Text("%d", room_count);
      } else {
        ImGui::TextDisabled("-");
      }
      ImGui::TableSetColumnIndex(4);
      const ObjectEvidenceState state = evidence_.StateOf(object_id);
      ImGui::TextColored(StateColor(state), "%s",
                         ObjectEvidenceStateLabel(state));
      ImGui::PopID();
    }
  }
  ImGui::EndTable();
}

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
  ImGui::TextDisabled("%s: %s", tr("Routine"),
                      routine != nullptr ? routine->name.c_str() : "-");
  ImGui::SameLine();
  ImGui::TextDisabled("| %s", symbology.family.c_str());

  const int group = ReleaseFocusGroupFor(object_id);
  if (group >= 0) {
    const auto& focus = ReleaseFocusGroups()[static_cast<size_t>(group)];
    ImGui::TextColored(gui::GetInfoColor(), "%s:", focus.name);
    ImGui::SameLine();
    ImGui::TextWrapped("%s", focus.what_to_check);
  }

  // Verdict buttons, current one highlighted.
  const ObjectEvidenceState current = evidence_.StateOf(object_id);
  for (ObjectEvidenceState state : kAllObjectEvidenceStates) {
    const bool is_current = state == current;
    if (is_current) {
      ImGui::PushStyleColor(ImGuiCol_Button, StateColor(state));
    }
    if (ImGui::Button(ObjectEvidenceStateLabel(state))) {
      SetVerdict(object_id, state);
    }
    if (is_current) {
      ImGui::PopStyleColor();
    }
    ImGui::SameLine();
  }
  ImGui::NewLine();

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

  const auto* occurrences = usage_.Find(object_id);
  if (occurrences == nullptr) {
    ImGui::TextDisabled(
        "%s", tr("Not placed in any room of this ROM. Place it in a scratch "
                 "room to check it."));
    ImGui::EndChild();
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
    ImGui::PopID();
  }
  ImGui::EndChild();
}

}  // namespace editor
}  // namespace yaze
