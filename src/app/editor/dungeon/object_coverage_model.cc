#include "app/editor/dungeon/object_coverage_model.h"

#include <algorithm>
#include <cstdlib>
#include <fstream>
#include <set>
#include <sstream>
#include <system_error>
#include <utility>

#include "absl/strings/str_cat.h"
#include "absl/strings/str_format.h"
#include "nlohmann/json.hpp"
#include "zelda3/dungeon/game_tilemap_comparison.h"

namespace yaze::editor {
namespace {

constexpr int kEvidenceFileVersion = 1;

struct IdRange {
  int first;
  int last;
};

// Object IDs per focus group, copied from the family table in
// docs/internal/plans/dungeon-0.8.0-issue-test-backlog-2026-06-28.md. The
// order matches ReleaseFocusGroups().
const std::vector<std::vector<IdRange>>& FocusGroupRanges() {
  static const std::vector<std::vector<IdRange>> kRanges = {
      // Strips
      {{0x33, 0x34}, {0x70, 0x71}, {0x8D, 0x8E}, {0xB2, 0xB4}},
      // Bars
      {{0x4C, 0x4C}, {0x8F, 0x8F}, {0xFD6, 0xFD9}},
      // Water
      {{0x3F, 0x48}, {0x79, 0x7A}, {0xC8, 0xC9}, {0xD9, 0xD9}, {0xE7, 0xE7}},
      // Ice and moving floors
      {{0xCA, 0xCA}, {0xD1, 0xD2}, {0xE3, 0xE6}},
      // Flood controls
      {{0xD8, 0xD8}, {0xDA, 0xDA}},
      // Stairs
      {{0x12A, 0x12A},
       {0x12D, 0x133},
       {0x135, 0x136},
       {0x138, 0x13B},
       {0xF9B, 0xFA1},
       {0xFA6, 0xFA9},
       {0xFB3, 0xFB3}},
      // Corners
      {{0x100, 0x117}},
  };
  return kRanges;
}

std::optional<int> ParseObjectIdKey(const std::string& key) {
  if (key.size() < 3 || key[0] != '0' || (key[1] != 'x' && key[1] != 'X')) {
    return std::nullopt;
  }
  char* end = nullptr;
  const long value = std::strtol(key.c_str() + 2, &end, 16);
  if (end == nullptr || *end != '\0' || value < 0 || value > 0xFFF) {
    return std::nullopt;
  }
  return static_cast<int>(value);
}

}  // namespace

const char* ObjectEvidenceStateKey(ObjectEvidenceState state) {
  switch (state) {
    case ObjectEvidenceState::kUntriaged:
      return "untriaged";
    case ObjectEvidenceState::kReproduced:
      return "reproduced";
    case ObjectEvidenceState::kFixedAwaitingProof:
      return "fixed-awaiting-proof";
    case ObjectEvidenceState::kVerified:
      return "verified";
    case ObjectEvidenceState::kIntentionalPreviewLimit:
      return "intentional-preview-limit";
  }
  return "untriaged";
}

const char* ObjectEvidenceStateLabel(ObjectEvidenceState state) {
  switch (state) {
    case ObjectEvidenceState::kUntriaged:
      return "Not checked";
    case ObjectEvidenceState::kReproduced:
      return "Broken";
    case ObjectEvidenceState::kFixedAwaitingProof:
      return "Fixed, recheck";
    case ObjectEvidenceState::kVerified:
      return "Matches game";
    case ObjectEvidenceState::kIntentionalPreviewLimit:
      return "Preview limit";
  }
  return "Not checked";
}

std::optional<ObjectEvidenceState> ParseObjectEvidenceState(
    std::string_view key) {
  for (ObjectEvidenceState state : kAllObjectEvidenceStates) {
    if (key == ObjectEvidenceStateKey(state)) {
      return state;
    }
  }
  return std::nullopt;
}

const ObjectEvidence* ObjectEvidenceStore::Find(int object_id) const {
  auto it = entries_.find(object_id);
  return it == entries_.end() ? nullptr : &it->second;
}

ObjectEvidenceState ObjectEvidenceStore::StateOf(int object_id) const {
  const ObjectEvidence* evidence = Find(object_id);
  return evidence ? evidence->state : ObjectEvidenceState::kUntriaged;
}

void ObjectEvidenceStore::Set(int object_id, ObjectEvidence evidence) {
  if (evidence.state == ObjectEvidenceState::kUntriaged &&
      evidence.note.empty()) {
    entries_.erase(object_id);
    return;
  }
  entries_[object_id] = std::move(evidence);
}

std::string ObjectEvidenceStore::ToJson() const {
  nlohmann::json objects = nlohmann::json::object();
  for (const auto& [object_id, evidence] : entries_) {
    nlohmann::json entry;
    entry["state"] = ObjectEvidenceStateKey(evidence.state);
    if (!evidence.note.empty()) {
      entry["note"] = evidence.note;
    }
    if (evidence.room_id >= 0) {
      entry["room"] = evidence.room_id;
    }
    if (!evidence.rom_sha1.empty()) {
      entry["rom_sha1"] = evidence.rom_sha1;
    }
    if (!evidence.updated_utc.empty()) {
      entry["updated_utc"] = evidence.updated_utc;
    }
    objects[FormatObjectId(object_id)] = std::move(entry);
  }
  nlohmann::json root;
  root["version"] = kEvidenceFileVersion;
  root["objects"] = std::move(objects);
  if (!capture_dir_.empty()) {
    root["capture_dir"] = capture_dir_;
  }
  return root.dump(2) + "\n";
}

absl::StatusOr<ObjectEvidenceStore> ObjectEvidenceStore::FromJson(
    std::string_view json) {
  nlohmann::json root = nlohmann::json::parse(json, nullptr,
                                              /*allow_exceptions=*/false);
  if (root.is_discarded() || !root.is_object()) {
    return absl::InvalidArgumentError("Object evidence file is not JSON");
  }
  if (!root.contains("version") || !root["version"].is_number_integer() ||
      root["version"].get<int>() != kEvidenceFileVersion) {
    return absl::InvalidArgumentError(absl::StrCat(
        "Object evidence file version must be ", kEvidenceFileVersion));
  }
  ObjectEvidenceStore store;
  if (root.contains("capture_dir") && root["capture_dir"].is_string()) {
    store.capture_dir_ = root["capture_dir"].get<std::string>();
  }
  if (!root.contains("objects")) {
    return store;
  }
  const auto& objects = root["objects"];
  if (!objects.is_object()) {
    return absl::InvalidArgumentError("\"objects\" must be a JSON object");
  }
  for (const auto& [key, entry] : objects.items()) {
    const std::optional<int> object_id = ParseObjectIdKey(key);
    if (!object_id) {
      return absl::InvalidArgumentError(
          absl::StrCat("Bad object ID key \"", key, "\""));
    }
    if (!entry.is_object() || !entry.contains("state") ||
        !entry["state"].is_string()) {
      return absl::InvalidArgumentError(
          absl::StrCat("Object ", key, " has no state"));
    }
    const std::optional<ObjectEvidenceState> state =
        ParseObjectEvidenceState(entry["state"].get<std::string>());
    if (!state) {
      return absl::InvalidArgumentError(
          absl::StrCat("Object ", key, " has unknown state \"",
                       entry["state"].get<std::string>(), "\""));
    }
    ObjectEvidence evidence;
    evidence.state = *state;
    if (entry.contains("note") && entry["note"].is_string()) {
      evidence.note = entry["note"].get<std::string>();
    }
    if (entry.contains("room") && entry["room"].is_number_integer()) {
      evidence.room_id = entry["room"].get<int>();
    }
    if (entry.contains("rom_sha1") && entry["rom_sha1"].is_string()) {
      evidence.rom_sha1 = entry["rom_sha1"].get<std::string>();
    }
    if (entry.contains("updated_utc") && entry["updated_utc"].is_string()) {
      evidence.updated_utc = entry["updated_utc"].get<std::string>();
    }
    store.entries_[*object_id] = std::move(evidence);
  }
  return store;
}

absl::StatusOr<ObjectEvidenceStore> ObjectEvidenceStore::LoadFromFile(
    const std::filesystem::path& path) {
  std::error_code ec;
  if (!std::filesystem::exists(path, ec)) {
    return ObjectEvidenceStore{};
  }
  std::ifstream in(path, std::ios::binary);
  if (!in) {
    return absl::UnavailableError(absl::StrCat("Cannot read ", path.string()));
  }
  std::ostringstream contents;
  contents << in.rdbuf();
  return FromJson(contents.str());
}

absl::Status ObjectEvidenceStore::SaveToFile(
    const std::filesystem::path& path) const {
  std::error_code ec;
  if (path.has_parent_path()) {
    std::filesystem::create_directories(path.parent_path(), ec);
    if (ec) {
      return absl::UnavailableError(absl::StrCat(
          "Cannot create ", path.parent_path().string(), ": ", ec.message()));
    }
  }
  std::filesystem::path temp_path = path;
  temp_path += ".tmp";
  {
    std::ofstream out(temp_path, std::ios::binary | std::ios::trunc);
    if (!out) {
      return absl::UnavailableError(
          absl::StrCat("Cannot write ", temp_path.string()));
    }
    out << ToJson();
    if (!out.good()) {
      return absl::UnavailableError(
          absl::StrCat("Failed writing ", temp_path.string()));
    }
  }
  std::filesystem::rename(temp_path, path, ec);
  if (ec) {
    std::filesystem::remove(temp_path, ec);
    return absl::UnavailableError(
        absl::StrCat("Cannot replace ", path.string()));
  }
  return absl::OkStatus();
}

void ObjectUsageIndex::AddRoom(int room_id,
                               const std::vector<zelda3::RoomObject>& objects) {
  for (size_t i = 0; i < objects.size(); ++i) {
    const auto& object = objects[i];
    ObjectOccurrence occurrence;
    occurrence.room_id = room_id;
    occurrence.object_index = i;
    occurrence.x = object.x();
    occurrence.y = object.y();
    occurrence.size = object.size_;
    occurrence.layer = object.GetLayerValue();
    by_object_[object.id_].push_back(occurrence);
  }
  ++rooms_scanned_;
}

const std::vector<ObjectOccurrence>* ObjectUsageIndex::Find(
    int object_id) const {
  auto it = by_object_.find(object_id);
  return it == by_object_.end() ? nullptr : &it->second;
}

int ObjectUsageIndex::RoomCountFor(int object_id) const {
  const auto* occurrences = Find(object_id);
  if (occurrences == nullptr) {
    return 0;
  }
  std::set<int> rooms;
  for (const auto& occurrence : *occurrences) {
    rooms.insert(occurrence.room_id);
  }
  return static_cast<int>(rooms.size());
}

const std::vector<ReleaseFocusGroup>& ReleaseFocusGroups() {
  static const std::vector<ReleaseFocusGroup> kGroups = {
      {"Strips",
       "Anchor, repeat count and end-cap overlap. Try sizes 0, 1 and 15 and "
       "placement at the room edge."},
      {"Bars",
       "Horizontal, vertical and corner joins. Start with Oracle room 0x042."},
      {"Water",
       "Palette, edge versus interior tiles, and each layer on its own. "
       "Oracle witnesses: 0x04A, then 0x033."},
      {"Ice and moving floors",
       "Stamp size and palette in a real room. Oracle ice witnesses: "
       "0x08C at (10,11) on BG1 and 0x0CE at (29,23) on BG2."},
      {"Flood controls",
       "Check the editor indicator only. The in-game water is HDMA, so it is "
       "not expected to match."},
      {"Stairs",
       "Stairs, the wall behind them and doors all visible on the lower "
       "level. Room 0x077 has spiral stairs."},
      {"Corners", "Corner and seam tiles, and anchors at the room boundary."},
  };
  return kGroups;
}

int ReleaseFocusGroupFor(int object_id) {
  const auto& ranges = FocusGroupRanges();
  for (size_t group = 0; group < ranges.size(); ++group) {
    for (const IdRange& range : ranges[group]) {
      if (object_id >= range.first && object_id <= range.last) {
        return static_cast<int>(group);
      }
    }
  }
  return -1;
}

std::vector<int> OrderObjectsForReview(const std::vector<int16_t>& object_ids) {
  std::vector<int> order(object_ids.begin(), object_ids.end());
  const int other_group = static_cast<int>(ReleaseFocusGroups().size());
  auto rank = [other_group](int object_id) {
    const int group = ReleaseFocusGroupFor(object_id);
    return group < 0 ? other_group : group;
  };
  std::stable_sort(order.begin(), order.end(), [&rank](int a, int b) {
    const int rank_a = rank(a);
    const int rank_b = rank(b);
    return rank_a != rank_b ? rank_a < rank_b : a < b;
  });
  return order;
}

std::optional<int> NextObjectToCheck(const std::vector<int>& review_order,
                                     const ObjectEvidenceStore& evidence,
                                     const ObjectUsageIndex& usage,
                                     std::optional<int> after_object_id) {
  if (review_order.empty()) {
    return std::nullopt;
  }
  size_t start = 0;
  if (after_object_id) {
    auto it =
        std::find(review_order.begin(), review_order.end(), *after_object_id);
    if (it != review_order.end()) {
      start = static_cast<size_t>(it - review_order.begin()) + 1;
    }
  }
  for (size_t step = 0; step < review_order.size(); ++step) {
    const int object_id = review_order[(start + step) % review_order.size()];
    if (evidence.StateOf(object_id) == ObjectEvidenceState::kUntriaged &&
        usage.Find(object_id) != nullptr) {
      return object_id;
    }
  }
  return std::nullopt;
}

absl::StatusOr<GameCaptureManifest> LoadGameCaptureManifest(
    const std::filesystem::path& dir) {
  const std::filesystem::path path = dir / "manifest.json";
  std::ifstream in(path, std::ios::binary);
  if (!in) {
    return absl::NotFoundError(
        absl::StrCat("No manifest.json in ", dir.string()));
  }
  std::ostringstream contents;
  contents << in.rdbuf();
  nlohmann::json root = nlohmann::json::parse(contents.str(), nullptr,
                                              /*allow_exceptions=*/false);
  if (root.is_discarded() || !root.is_object() || !root.contains("rom_sha1") ||
      !root["rom_sha1"].is_string() || !root.contains("rooms") ||
      !root["rooms"].is_object()) {
    return absl::InvalidArgumentError(
        absl::StrCat(path.string(), " is not a room tilemap capture manifest"));
  }
  GameCaptureManifest manifest;
  manifest.rom_sha1 = root["rom_sha1"].get<std::string>();
  for (const auto& [key, entry] : root["rooms"].items()) {
    const std::optional<int> room_id = ParseObjectIdKey(key);
    if (!room_id || !entry.is_object() || !entry.contains("status") ||
        !entry["status"].is_string() ||
        entry["status"].get<std::string>().rfind("ok", 0) != 0) {
      continue;
    }
    manifest.captured_rooms.push_back(*room_id);
  }
  std::sort(manifest.captured_rooms.begin(), manifest.captured_rooms.end());
  return manifest;
}

std::filesystem::path GameCaptureRoomPath(const std::filesystem::path& dir,
                                          int room_id) {
  return dir / absl::StrFormat("room_%03X.tilemap", room_id);
}

void ObjectAutoCheckResults::Clear() {
  placements_.clear();
  summaries_.clear();
  rooms_compared_ = 0;
  rooms_exact_ = 0;
}

void ObjectAutoCheckResults::Record(int room_id, size_t object_index,
                                    int object_id,
                                    const PlacementAutoResult& result) {
  PlacementAutoResult stored = result;
  stored.object_id = object_id;
  placements_[{room_id, object_index}] = stored;
  if (result.tiles_owned == 0) {
    return;
  }
  ObjectSummary& summary = summaries_[object_id];
  ++summary.placements_checked;
  if (summary.rooms_checked.empty() ||
      summary.rooms_checked.back() != room_id) {
    summary.rooms_checked.push_back(room_id);
  }
  if (result.tiles_different > 0) {
    ++summary.placements_different;
    summary.difference_bits |= result.difference_bits;
    if (summary.first_room_different < 0) {
      summary.first_room_different = room_id;
    }
  }
}

const PlacementAutoResult* ObjectAutoCheckResults::Find(
    int room_id, size_t object_index) const {
  auto it = placements_.find({room_id, object_index});
  return it == placements_.end() ? nullptr : &it->second;
}

const ObjectAutoCheckResults::ObjectSummary* ObjectAutoCheckResults::Summary(
    int object_id) const {
  auto it = summaries_.find(object_id);
  return it == summaries_.end() ? nullptr : &it->second;
}

std::map<int, ObjectEvidence> ProposeAutomaticVerdicts(
    const ObjectAutoCheckResults& results, const ObjectEvidenceStore& evidence,
    const std::string& rom_sha1) {
  std::map<int, ObjectEvidence> proposals;
  const std::string rom_label = rom_sha1.substr(0, 8);
  for (const auto& [object_id, summary] : results.summaries()) {
    if (summary.placements_checked == 0 ||
        evidence.Find(object_id) != nullptr) {
      continue;
    }
    ObjectEvidence proposal;
    proposal.rom_sha1 = rom_sha1;
    if (summary.placements_different == 0) {
      proposal.state = ObjectEvidenceState::kVerified;
      proposal.room_id = summary.rooms_checked.front();
      proposal.note = absl::StrFormat(
          "Automatic: tilemap matches the game in all %d placements across %d "
          "rooms (Mesen capture, ROM %s). Colors and graphics not compared.",
          summary.placements_checked,
          static_cast<int>(summary.rooms_checked.size()), rom_label);
    } else {
      proposal.state = ObjectEvidenceState::kReproduced;
      proposal.room_id = summary.first_room_different;
      proposal.note = absl::StrFormat(
          "Automatic: %d of %d placements differ from the game (%s); first in "
          "room 0x%03X (Mesen capture, ROM %s).",
          summary.placements_different, summary.placements_checked,
          zelda3::DescribeTileWordDifference(summary.difference_bits),
          summary.first_room_different, rom_label);
    }
    proposals[object_id] = std::move(proposal);
  }
  return proposals;
}

std::map<int, ObjectCustomDrawCode> FindObjectsWithCustomDrawCode(
    const std::vector<zelda3::ObjectDrawCode>& draw_code,
    const std::vector<core::ProtectedRegion>& hooks) {
  std::map<int, ObjectCustomDrawCode> out;
  for (const auto& code : draw_code) {
    ObjectCustomDrawCode custom;
    bool found = false;
    if (code.jumps_to_expanded_code) {
      custom.replaced = true;
      custom.address = code.jump_target;
      found = true;
    }
    for (const auto& hook : hooks) {
      if (hook.start < code.routine_end && code.routine_start < hook.end) {
        if (!found) {
          custom.address = std::max(hook.start, code.routine_start);
        }
        custom.module = hook.module;
        found = true;
        break;
      }
    }
    if (found) {
      out.emplace(code.object_id, std::move(custom));
    }
  }
  return out;
}

std::string FormatObjectId(int object_id) {
  return absl::StrFormat("0x%03X", object_id);
}

}  // namespace yaze::editor
