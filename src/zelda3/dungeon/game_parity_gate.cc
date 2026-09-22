#include "zelda3/dungeon/game_parity_gate.h"

#include <algorithm>
#include <fstream>
#include <iterator>
#include <set>
#include <sstream>
#include <tuple>

#include "absl/strings/str_cat.h"
#include "absl/strings/str_format.h"
#include "absl/strings/str_join.h"
#include "nlohmann/json.hpp"
#include "util/rom_hash.h"
#include "zelda3/dungeon/game_tilemap_comparison.h"

namespace yaze::zelda3::parity {

namespace {

using json = nlohmann::json;
using TileKey = std::tuple<int, int, int, int>;  // room, layer, x, y

constexpr int kBaselineVersion = 1;

std::string RoomKey(int room_id) {
  return absl::StrFormat("0x%03X", room_id);
}

bool ReadFileBytes(const std::filesystem::path& path,
                   std::vector<uint8_t>& out) {
  std::ifstream in(path, std::ios::binary);
  if (!in) {
    return false;
  }
  out.assign(std::istreambuf_iterator<char>(in), {});
  return true;
}

std::string ManifestString(const json& manifest, const char* key) {
  if (!manifest.contains(key) || !manifest[key].is_string()) {
    return std::string();
  }
  return manifest[key].get<std::string>();
}

// Room ids are written as "0x0A4" in manifests and baselines.
bool ParseRoomId(std::string_view text, int& room_id) {
  if (text.size() < 3 || text[0] != '0' || (text[1] != 'x' && text[1] != 'X')) {
    return false;
  }
  int value = 0;
  for (char ch : text.substr(2)) {
    int digit = -1;
    if (ch >= '0' && ch <= '9')
      digit = ch - '0';
    if (ch >= 'a' && ch <= 'f')
      digit = ch - 'a' + 10;
    if (ch >= 'A' && ch <= 'F')
      digit = ch - 'A' + 10;
    if (digit < 0 || value > 0xFFF) {
      return false;
    }
    value = value * 16 + digit;
  }
  room_id = value;
  return true;
}

}  // namespace

std::string CaptureRomSha1(std::span<const uint8_t> rom) {
  if (rom.size() >= kCaptureRomMinSize) {
    return util::ComputeSha1Hex(rom.data(), rom.size());
  }
  std::vector<uint8_t> padded(kCaptureRomMinSize, 0);
  std::copy(rom.begin(), rom.end(), padded.begin());
  return util::ComputeSha1Hex(padded.data(), padded.size());
}

absl::StatusOr<CaptureSet> LoadVerifiedCaptureSet(
    const std::filesystem::path& dir, const CaptureExpectation& expected,
    std::string_view rom_sha1) {
  std::vector<uint8_t> manifest_bytes;
  if (!ReadFileBytes(dir / "manifest.json", manifest_bytes)) {
    return absl::NotFoundError(
        absl::StrCat("No manifest.json in ", dir.string()));
  }
  const json manifest =
      json::parse(manifest_bytes.begin(), manifest_bytes.end(), nullptr,
                  /*allow_exceptions=*/false);
  if (manifest.is_discarded() || !manifest.is_object() ||
      !manifest.contains("rooms") || !manifest["rooms"].is_object()) {
    return absl::InvalidArgumentError(
        absl::StrCat(dir.string(), "/manifest.json is malformed"));
  }

  std::vector<std::string> problems;
  const std::string manifest_rom = ManifestString(manifest, "rom_sha1");
  if (manifest_rom != expected.rom_sha1) {
    problems.push_back(absl::StrCat("capture is for ROM ", manifest_rom,
                                    ", baseline expects ", expected.rom_sha1));
  }
  if (rom_sha1 != expected.rom_sha1) {
    problems.push_back(absl::StrCat("ROM under test is ", rom_sha1,
                                    ", baseline expects ", expected.rom_sha1));
  }
  const std::string entrance = ManifestString(manifest, "entrance");
  if (entrance != expected.entrance) {
    problems.push_back(absl::StrCat("capture entrance is \"", entrance,
                                    "\", baseline expects \"",
                                    expected.entrance, "\""));
  }
  const std::string room_flags = ManifestString(manifest, "room_flags");
  if (room_flags != expected.room_flags) {
    problems.push_back(absl::StrCat("capture room_flags is \"", room_flags,
                                    "\", baseline expects \"",
                                    expected.room_flags, "\""));
  }
  if (expected.required_rooms.empty()) {
    problems.push_back("baseline requires no rooms");
  }

  CaptureSet set;
  set.dir = dir;
  const json& rooms = manifest["rooms"];
  for (int room_id : expected.required_rooms) {
    const std::string key = RoomKey(room_id);
    if (!rooms.contains(key) || !rooms[key].is_object()) {
      problems.push_back(absl::StrCat("room ", key, ": not in the manifest"));
      continue;
    }
    const json& entry = rooms[key];
    const std::string status = ManifestString(entry, "status");
    if (status != "ok") {
      problems.push_back(
          absl::StrCat("room ", key, ": status \"", status, "\", not ok"));
      continue;
    }
    const std::string file = ManifestString(entry, "file");
    const std::string sha1 = ManifestString(entry, "sha1");
    if (file.empty() || sha1.empty() ||
        std::filesystem::path(file).filename().string() != file) {
      problems.push_back(
          absl::StrCat("room ", key, ": missing or unsafe file/sha1 entry"));
      continue;
    }
    std::vector<uint8_t> bytes;
    if (!ReadFileBytes(dir / file, bytes)) {
      problems.push_back(absl::StrCat("room ", key, ": cannot read ", file));
      continue;
    }
    if (bytes.size() != kGameRoomTilemapBytes) {
      problems.push_back(absl::StrCat("room ", key, ": ", file, " is ",
                                      bytes.size(), " bytes, expected ",
                                      kGameRoomTilemapBytes));
      continue;
    }
    if (util::ComputeSha1Hex(bytes.data(), bytes.size()) != sha1) {
      problems.push_back(absl::StrCat("room ", key, ": ", file,
                                      " does not match its manifest SHA-1"));
      continue;
    }
    set.room_tilemaps.emplace(room_id, std::move(bytes));
  }

  if (!problems.empty()) {
    return absl::FailedPreconditionError(
        absl::StrCat("Capture ", dir.string(), " cannot be used:\n  ",
                     absl::StrJoin(problems, "\n  ")));
  }
  return set;
}

std::string DifferenceDigest(uint16_t game_word, uint16_t yaze_word) {
  const std::string text = absl::StrFormat("yaze-parity-v1 game=%04X yaze=%04X",
                                           game_word, yaze_word);
  return util::ComputeSha1Hex(reinterpret_cast<const uint8_t*>(text.data()),
                              text.size())
      .substr(0, 12);
}

absl::StatusOr<Baseline> ParseBaseline(std::string_view text) {
  const json root = json::parse(text.begin(), text.end(), nullptr,
                                /*allow_exceptions=*/false);
  if (root.is_discarded() || !root.is_object()) {
    return absl::InvalidArgumentError("Baseline is not a JSON object");
  }
  Baseline baseline;
  if (!root.contains("version") || !root["version"].is_number_integer() ||
      root["version"].get<int>() != kBaselineVersion) {
    return absl::InvalidArgumentError(
        absl::StrCat("Baseline version must be ", kBaselineVersion));
  }
  baseline.version = kBaselineVersion;
  baseline.state = ManifestString(root, "state");
  if (baseline.state.empty()) {
    return absl::InvalidArgumentError("Baseline needs a \"state\"");
  }
  if (!root.contains("capture") || !root["capture"].is_object()) {
    return absl::InvalidArgumentError("Baseline needs a \"capture\" object");
  }
  const json& capture = root["capture"];
  baseline.capture.rom_sha1 = ManifestString(capture, "rom_sha1");
  baseline.capture.entrance = ManifestString(capture, "entrance");
  baseline.capture.room_flags = ManifestString(capture, "room_flags");
  if (baseline.capture.rom_sha1.size() != 40 ||
      baseline.capture.entrance.empty()) {
    return absl::InvalidArgumentError(
        "Baseline capture needs a 40-digit rom_sha1 and an entrance");
  }
  if (!capture.contains("required_rooms") ||
      !capture["required_rooms"].is_array()) {
    return absl::InvalidArgumentError(
        "Baseline capture needs a required_rooms array");
  }
  for (const auto& room : capture["required_rooms"]) {
    int room_id = -1;
    if (!room.is_string() || !ParseRoomId(room.get<std::string>(), room_id)) {
      return absl::InvalidArgumentError(
          "required_rooms entries must look like \"0x0A4\"");
    }
    baseline.capture.required_rooms.push_back(room_id);
  }

  if (!root.contains("groups") || !root["groups"].is_array()) {
    return absl::InvalidArgumentError("Baseline needs a \"groups\" array");
  }
  for (const auto& item : root["groups"]) {
    BaselineGroup group;
    if (!item.is_object() || !item.contains("room") ||
        !item["room"].is_string() ||
        !ParseRoomId(item["room"].get<std::string>(), group.room_id)) {
      return absl::InvalidArgumentError(
          "Each group needs a \"room\" like \"0x0A4\"");
    }
    group.reason = ManifestString(item, "reason");
    group.evidence = ManifestString(item, "evidence");
    group.reviewed = item.contains("reviewed") &&
                     item["reviewed"].is_boolean() &&
                     item["reviewed"].get<bool>();
    if (!item.contains("tiles") || !item["tiles"].is_array() ||
        item["tiles"].empty()) {
      return absl::InvalidArgumentError(
          absl::StrCat("Group for room ", RoomKey(group.room_id),
                       " needs a non-empty \"tiles\" array"));
    }
    for (const auto& tile_json : item["tiles"]) {
      BaselineTile tile;
      if (!tile_json.is_object() || !tile_json.contains("layer") ||
          !tile_json.contains("x") || !tile_json.contains("y") ||
          !tile_json["layer"].is_number_integer() ||
          !tile_json["x"].is_number_integer() ||
          !tile_json["y"].is_number_integer()) {
        return absl::InvalidArgumentError(
            absl::StrCat("Bad tile in room ", RoomKey(group.room_id)));
      }
      tile.layer = tile_json["layer"].get<int>();
      tile.x = tile_json["x"].get<int>();
      tile.y = tile_json["y"].get<int>();
      tile.digest = ManifestString(tile_json, "digest");
      if ((tile.layer != 1 && tile.layer != 2) || tile.x < 0 || tile.x > 63 ||
          tile.y < 0 || tile.y > 63 || tile.digest.size() != 12) {
        return absl::InvalidArgumentError(
            absl::StrCat("Bad tile in room ", RoomKey(group.room_id)));
      }
      group.tiles.push_back(tile);
    }
    if (std::find(baseline.capture.required_rooms.begin(),
                  baseline.capture.required_rooms.end(),
                  group.room_id) == baseline.capture.required_rooms.end()) {
      // It would never be checked, so it could never go stale.
      return absl::InvalidArgumentError(
          absl::StrCat("Group for room ", RoomKey(group.room_id),
                       " is not in required_rooms"));
    }
    baseline.groups.push_back(std::move(group));
  }
  return baseline;
}

std::string SerializeBaseline(const Baseline& baseline) {
  json root;
  root["version"] = baseline.version;
  root["state"] = baseline.state;
  json capture;
  capture["rom_sha1"] = baseline.capture.rom_sha1;
  capture["entrance"] = baseline.capture.entrance;
  if (!baseline.capture.room_flags.empty()) {
    capture["room_flags"] = baseline.capture.room_flags;
  }
  json rooms = json::array();
  for (int room_id : baseline.capture.required_rooms) {
    rooms.push_back(RoomKey(room_id));
  }
  capture["required_rooms"] = rooms;
  root["capture"] = capture;
  json groups = json::array();
  for (const auto& group : baseline.groups) {
    json item;
    item["room"] = RoomKey(group.room_id);
    item["reason"] = group.reason;
    item["evidence"] = group.evidence;
    item["reviewed"] = group.reviewed;
    json tiles = json::array();
    for (const auto& tile : group.tiles) {
      tiles.push_back({{"layer", tile.layer},
                       {"x", tile.x},
                       {"y", tile.y},
                       {"digest", tile.digest}});
    }
    item["tiles"] = tiles;
    groups.push_back(item);
  }
  root["groups"] = groups;
  return root.dump(2) + "\n";
}

const char* GateFindingKindName(GateFinding::Kind kind) {
  switch (kind) {
    case GateFinding::Kind::kNew:
      return "new difference";
    case GateFinding::Kind::kChanged:
      return "changed difference";
    case GateFinding::Kind::kStale:
      return "stale baseline entry";
    case GateFinding::Kind::kUnreviewed:
      return "unreviewed baseline entry";
    case GateFinding::Kind::kDuplicate:
      return "duplicate baseline entry";
  }
  return "finding";
}

std::string DescribeFinding(const GateFinding& finding) {
  return absl::StrFormat(
      "%s: room %s layer %d (%d,%d)%s%s", GateFindingKindName(finding.kind),
      RoomKey(finding.room_id), finding.layer, finding.x, finding.y,
      finding.detail.empty() ? "" : " - ", finding.detail);
}

GateReport EvaluateParity(const Baseline& baseline,
                          const std::vector<ObservedDifference>& observed,
                          const std::vector<int>& rooms_checked) {
  GateReport report;
  const std::set<int> checked(rooms_checked.begin(), rooms_checked.end());

  struct Expected {
    std::string digest;
    bool approved = false;
    std::string reason;
  };
  std::map<TileKey, Expected> expected;
  for (const auto& group : baseline.groups) {
    if (!checked.contains(group.room_id)) {
      continue;
    }
    const bool approved =
        group.reviewed && !group.evidence.empty() && !group.reason.empty();
    for (const auto& tile : group.tiles) {
      const TileKey key{group.room_id, tile.layer, tile.x, tile.y};
      if (expected.contains(key)) {
        report.findings.push_back({GateFinding::Kind::kDuplicate, group.room_id,
                                   tile.layer, tile.x, tile.y, group.reason});
        continue;
      }
      expected.emplace(key, Expected{tile.digest, approved, group.reason});
      if (!approved) {
        report.findings.push_back({GateFinding::Kind::kUnreviewed,
                                   group.room_id, tile.layer, tile.x, tile.y,
                                   group.evidence.empty()
                                       ? "no evidence recorded"
                                       : "reviewed is false"});
      }
    }
  }

  std::set<TileKey> seen;
  for (const auto& difference : observed) {
    if (!checked.contains(difference.room_id)) {
      continue;
    }
    const TileKey key{difference.room_id, difference.layer, difference.x,
                      difference.y};
    seen.insert(key);
    const std::string digest =
        DifferenceDigest(difference.game, difference.yaze);
    auto it = expected.find(key);
    if (it == expected.end()) {
      report.findings.push_back(
          {GateFinding::Kind::kNew, difference.room_id, difference.layer,
           difference.x, difference.y,
           absl::StrFormat("game %04X, yaze %04X", difference.game,
                           difference.yaze)});
      continue;
    }
    if (it->second.digest != digest) {
      report.findings.push_back(
          {GateFinding::Kind::kChanged, difference.room_id, difference.layer,
           difference.x, difference.y,
           absl::StrFormat("baseline %s (%s), now game %04X, yaze %04X",
                           it->second.digest, it->second.reason,
                           difference.game, difference.yaze)});
      continue;
    }
    if (it->second.approved) {
      ++report.expected_differences_matched;
    }
  }

  for (const auto& [key, entry] : expected) {
    if (!seen.contains(key)) {
      const auto& [room_id, layer, x, y] = key;
      report.findings.push_back(
          {GateFinding::Kind::kStale, room_id, layer, x, y,
           absl::StrCat(entry.reason,
                        ": yaze now matches the game; remove the entry")});
    }
  }

  std::sort(report.findings.begin(), report.findings.end(),
            [](const GateFinding& a, const GateFinding& b) {
              return std::tie(a.room_id, a.layer, a.y, a.x, a.kind) <
                     std::tie(b.room_id, b.layer, b.y, b.x, b.kind);
            });
  return report;
}

}  // namespace yaze::zelda3::parity
