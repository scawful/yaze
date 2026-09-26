#include "zelda3/cutscene/cutscene_shot.h"

#include <algorithm>
#include <array>
#include <cctype>
#include <cstdint>
#include <set>
#include <string>

#include "absl/strings/str_format.h"
#include "nlohmann/json.hpp"

namespace yaze::zelda3 {
namespace {

using ordered_json = nlohmann::ordered_json;

// OverworldTransitionPositionY/X ($02A8C4/$02A944): each area's parent origin.
constexpr std::array<uint16_t, 64> kVanillaOriginY = {
    0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000,
    0x0000, 0x0000, 0x0200, 0x0000, 0x0000, 0x0000, 0x0000, 0x0200,
    0x0400, 0x0400, 0x0400, 0x0400, 0x0400, 0x0400, 0x0400, 0x0400,
    0x0600, 0x0600, 0x0600, 0x0600, 0x0600, 0x0600, 0x0600, 0x0600,
    0x0600, 0x0600, 0x0800, 0x0600, 0x0600, 0x0800, 0x0600, 0x0600,
    0x0A00, 0x0A00, 0x0A00, 0x0A00, 0x0A00, 0x0A00, 0x0A00, 0x0A00,
    0x0C00, 0x0C00, 0x0C00, 0x0C00, 0x0C00, 0x0C00, 0x0C00, 0x0C00,
    0x0C00, 0x0C00, 0x0E00, 0x0E00, 0x0E00, 0x0C00, 0x0C00, 0x0E00};
constexpr std::array<uint16_t, 64> kVanillaOriginX = {
    0x0000, 0x0000, 0x0400, 0x0600, 0x0600, 0x0A00, 0x0A00, 0x0E00,
    0x0000, 0x0000, 0x0400, 0x0600, 0x0600, 0x0A00, 0x0A00, 0x0E00,
    0x0000, 0x0200, 0x0400, 0x0600, 0x0800, 0x0A00, 0x0C00, 0x0E00,
    0x0000, 0x0000, 0x0400, 0x0600, 0x0600, 0x0A00, 0x0C00, 0x0C00,
    0x0000, 0x0000, 0x0400, 0x0600, 0x0600, 0x0A00, 0x0C00, 0x0C00,
    0x0000, 0x0200, 0x0400, 0x0600, 0x0800, 0x0A00, 0x0C00, 0x0E00,
    0x0000, 0x0000, 0x0400, 0x0600, 0x0800, 0x0A00, 0x0A00, 0x0E00,
    0x0000, 0x0000, 0x0400, 0x0600, 0x0800, 0x0A00, 0x0A00, 0x0E00};

constexpr int kMaxWorldPixel = 0xFFFF;

std::string Hex2(int value) {
  return absl::StrFormat("0x%02X", value);
}

absl::Status FieldError(const std::string& where, const std::string& what) {
  return absl::InvalidArgumentError(
      absl::StrFormat("cutscene shots: %s %s", where, what));
}

// Accepts an integer or a string ("0x2D" or "45").
absl::StatusOr<int> ReadInt(const ordered_json& object, const char* key,
                            const std::string& where, int min, int max) {
  if (!object.contains(key)) {
    return FieldError(where, absl::StrFormat("is missing \"%s\"", key));
  }
  const auto& value = object.at(key);
  long long number = 0;
  if (value.is_number_integer()) {
    number = value.get<long long>();
  } else if (value.is_string()) {
    const std::string text = value.get<std::string>();
    try {
      size_t used = 0;
      number = std::stoll(text, &used, 0);
      if (used != text.size()) {
        return FieldError(
            where, absl::StrFormat("\"%s\" is not a number: %s", key, text));
      }
    } catch (...) {
      return FieldError(
          where, absl::StrFormat("\"%s\" is not a number: %s", key, text));
    }
  } else {
    return FieldError(where, absl::StrFormat("\"%s\" must be a number", key));
  }
  if (number < min || number > max) {
    return FieldError(where, absl::StrFormat("\"%s\" %lld is outside %d-%d",
                                             key, number, min, max));
  }
  return static_cast<int>(number);
}

bool ValidName(const std::string& name) {
  return !name.empty() &&
         std::all_of(name.begin(), name.end(), [](unsigned char c) {
           return std::isalnum(c) || c == '_';
         });
}

}  // namespace

absl::StatusOr<CutsceneShotSet> ParseCutsceneShots(std::string_view json) {
  const ordered_json root = ordered_json::parse(json, nullptr, false);
  if (root.is_discarded() || !root.is_object()) {
    return absl::InvalidArgumentError("cutscene shots: not a JSON object");
  }
  if (!root.contains("version") || !root.at("version").is_number_integer() ||
      root.at("version").get<int>() != 1) {
    return absl::InvalidArgumentError(
        "cutscene shots: \"version\" must be the integer 1");
  }
  if (!root.contains("shots") || !root.at("shots").is_array()) {
    return absl::InvalidArgumentError(
        "cutscene shots: \"shots\" must be an array");
  }

  CutsceneShotSet set;
  std::set<std::string> names;
  for (size_t i = 0; i < root.at("shots").size(); ++i) {
    const ordered_json& entry = root.at("shots")[i];
    std::string where = absl::StrFormat("shot %zu", i);
    if (!entry.is_object()) {
      return FieldError(where, "must be an object");
    }
    CutsceneShot shot;
    if (!entry.contains("name") || !entry.at("name").is_string()) {
      return FieldError(where, "is missing a string \"name\"");
    }
    shot.name = entry.at("name").get<std::string>();
    if (!ValidName(shot.name)) {
      return FieldError(
          where, absl::StrFormat("name \"%s\" must use only A-Z, a-z, 0-9 "
                                 "and _",
                                 shot.name));
    }
    if (!names.insert(shot.name).second) {
      return FieldError(
          where, absl::StrFormat("name \"%s\" is used twice", shot.name));
    }
    where = absl::StrFormat("shot \"%s\"", shot.name);
    auto area = ReadInt(entry, "area", where, 0, 0x7F);
    if (!area.ok())
      return area.status();
    shot.area = *area;

    if (!entry.contains("camera") || !entry.at("camera").is_object()) {
      return FieldError(where, "is missing a \"camera\" object");
    }
    auto cx =
        ReadInt(entry.at("camera"), "x", where + " camera", 0, kMaxWorldPixel);
    if (!cx.ok())
      return cx.status();
    auto cy =
        ReadInt(entry.at("camera"), "y", where + " camera", 0, kMaxWorldPixel);
    if (!cy.ok())
      return cy.status();
    shot.camera_x = *cx;
    shot.camera_y = *cy;

    if (!entry.contains("link") || !entry.at("link").is_object()) {
      return FieldError(where, "is missing a \"link\" object");
    }
    const ordered_json& link = entry.at("link");
    auto lx = ReadInt(link, "x", where + " link", 0, kMaxWorldPixel);
    if (!lx.ok())
      return lx.status();
    auto ly = ReadInt(link, "y", where + " link", 0, kMaxWorldPixel);
    if (!ly.ok())
      return ly.status();
    auto lf = ReadInt(link, "facing", where + " link", 0, 3);
    if (!lf.ok())
      return lf.status();
    shot.link = CutsceneLink{*lx, *ly, *lf};

    if (entry.contains("actors")) {
      if (!entry.at("actors").is_array()) {
        return FieldError(where, "\"actors\" must be an array");
      }
      for (size_t a = 0; a < entry.at("actors").size(); ++a) {
        const ordered_json& actor_json = entry.at("actors")[a];
        const std::string actor_where =
            absl::StrFormat("%s actor %zu", where, a);
        if (!actor_json.is_object()) {
          return FieldError(actor_where, "must be an object");
        }
        CutsceneActor actor;
        auto sprite = ReadInt(actor_json, "sprite", actor_where, 0, 0xFF);
        if (!sprite.ok())
          return sprite.status();
        auto ax = ReadInt(actor_json, "x", actor_where, 0, kMaxWorldPixel);
        if (!ax.ok())
          return ax.status();
        auto ay = ReadInt(actor_json, "y", actor_where, 0, kMaxWorldPixel);
        if (!ay.ok())
          return ay.status();
        auto af = ReadInt(actor_json, "facing", actor_where, 0, 3);
        if (!af.ok())
          return af.status();
        actor.sprite = *sprite;
        actor.x = *ax;
        actor.y = *ay;
        actor.facing = *af;
        if (actor_json.contains("note")) {
          if (!actor_json.at("note").is_string()) {
            return FieldError(actor_where, "\"note\" must be a string");
          }
          actor.note = actor_json.at("note").get<std::string>();
        }
        shot.actors.push_back(std::move(actor));
      }
    }
    set.shots.push_back(std::move(shot));
  }
  return set;
}

absl::StatusOr<std::string> SerializeCutsceneShots(const CutsceneShotSet& set) {
  // Round-trip through the parser's rules so an invalid set is never written.
  ordered_json root;
  root["version"] = 1;
  root["shots"] = ordered_json::array();
  for (const CutsceneShot& shot : set.shots) {
    ordered_json entry;
    entry["name"] = shot.name;
    entry["area"] = Hex2(shot.area);
    entry["camera"] = {{"x", shot.camera_x}, {"y", shot.camera_y}};
    entry["link"] = {
        {"x", shot.link.x}, {"y", shot.link.y}, {"facing", shot.link.facing}};
    entry["actors"] = ordered_json::array();
    for (const CutsceneActor& actor : shot.actors) {
      ordered_json actor_json;
      actor_json["sprite"] = Hex2(actor.sprite);
      actor_json["x"] = actor.x;
      actor_json["y"] = actor.y;
      actor_json["facing"] = actor.facing;
      if (!actor.note.empty()) {
        actor_json["note"] = actor.note;
      }
      entry["actors"].push_back(std::move(actor_json));
    }
    root["shots"].push_back(std::move(entry));
  }
  std::string text = root.dump(2) + "\n";
  auto check = ParseCutsceneShots(text);
  if (!check.ok()) {
    return check.status();
  }
  return text;
}

CameraBounds CameraBoundsForArea(const AreaExtent& area) {
  CameraBounds bounds;
  bounds.min_x = area.origin_x;
  bounds.min_y = area.origin_y;
  bounds.max_x = area.origin_x + area.width - kCutsceneViewportWidth;
  bounds.max_y = area.origin_y + area.height - (kCutsceneViewportHeight + 2);
  return bounds;
}

ClampedCamera ClampCamera(const AreaExtent& area, int x, int y) {
  const CameraBounds bounds = CameraBoundsForArea(area);
  ClampedCamera camera;
  camera.requested_x = x;
  camera.requested_y = y;
  camera.x = std::clamp(x, bounds.min_x, bounds.max_x);
  camera.y = std::clamp(y, bounds.min_y, bounds.max_y);
  return camera;
}

absl::StatusOr<AreaExtent> VanillaAreaExtent(int area) {
  if (area < 0 || area > 0x7F) {
    return absl::InvalidArgumentError(absl::StrFormat(
        "Area 0x%02X has no vanilla camera layout (special areas are not "
        "supported)",
        area));
  }
  const int index = area & 0x3F;
  AreaExtent extent;
  extent.origin_x = kVanillaOriginX[index];
  extent.origin_y = kVanillaOriginY[index];
  // A large area's four screens all name the parent's origin, so the parent
  // (top-left) screen and the screen to its right share an X origin.
  const int parent = (extent.origin_y / 512) * 8 + extent.origin_x / 512;
  const int right = parent + 1;
  const bool large = (parent % 8) != 7 &&
                     kVanillaOriginX[right] == kVanillaOriginX[parent] &&
                     kVanillaOriginY[right] == kVanillaOriginY[parent];
  extent.width = large ? 1024 : 512;
  extent.height = large ? 1024 : 512;
  return extent;
}

MapRect ViewportOnMap(const AreaExtent& area, int camera_x, int camera_y,
                      float scale) {
  return MapRect{
      (camera_x - area.origin_x) * scale, (camera_y - area.origin_y) * scale,
      kCutsceneViewportWidth * scale, kCutsceneViewportHeight * scale};
}

MapRect PointOnMap(const AreaExtent& area, int world_x, int world_y,
                   float scale) {
  return MapRect{(world_x - area.origin_x) * scale,
                 (world_y - area.origin_y) * scale, 0, 0};
}

bool PointInViewport(int camera_x, int camera_y, int world_x, int world_y) {
  return world_x >= camera_x && world_x < camera_x + kCutsceneViewportWidth &&
         world_y >= camera_y && world_y < camera_y + kCutsceneViewportHeight;
}

}  // namespace yaze::zelda3
