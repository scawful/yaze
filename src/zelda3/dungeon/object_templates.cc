#include "object_templates.h"

#include <algorithm>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <initializer_list>
#include <iostream>
#include <map>
#include <set>
#include <stdexcept>
#include <tuple>

#include "absl/strings/str_format.h"
#include "nlohmann/json.hpp"

namespace yaze {
namespace zelda3 {

using json = nlohmann::json;

// JSON serialization helpers
NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE(TemplateObject, id, rel_x, rel_y, size,
                                   layer)
NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE(ObjectTemplateRetainingWallLoop, name,
                                   members)

void to_json(json& value, const ObjectTemplate& object_template) {
  value = {{"name", object_template.name},
           {"description", object_template.description},
           {"category", object_template.category},
           {"objects", object_template.objects}};
  if (!object_template.retaining_wall_loops.empty())
    value["retaining_wall_loops"] = object_template.retaining_wall_loops;
}

void from_json(const json& value, ObjectTemplate& object_template) {
  auto parsed = ParseObjectTemplateJson(value.dump());
  if (!parsed.ok())
    throw std::invalid_argument(std::string(parsed.status().message()));
  object_template = std::move(*parsed);
}

namespace {

constexpr size_t kMaxTemplateBytes = 1024 * 1024;
constexpr size_t kMaxTemplateObjects = 512;
constexpr size_t kMaxReportedCells = 64;

bool HasExactFields(const json& value,
                    std::initializer_list<const char*> fields) {
  if (!value.is_object() || value.size() != fields.size())
    return false;
  return std::all_of(fields.begin(), fields.end(),
                     [&](const char* key) { return value.contains(key); });
}

bool IsBoundedInteger(const json& value, int minimum, int maximum) {
  if (value.is_number_unsigned()) {
    const auto number = value.get<uint64_t>();
    return number <= static_cast<uint64_t>(maximum) &&
           (minimum <= 0 || number >= static_cast<uint64_t>(minimum));
  }
  if (!value.is_number_integer())
    return false;
  const auto number = value.get<int64_t>();
  return number >= minimum && number <= maximum;
}

absl::Status ValidateTemplateContract(const ObjectTemplate& object_template) {
  if (object_template.name.empty() || object_template.name.size() > 256 ||
      object_template.category.size() > 256 ||
      object_template.description.size() > 4096 ||
      object_template.name.find('\0') != std::string::npos ||
      object_template.category.find('\0') != std::string::npos ||
      object_template.description.find('\0') != std::string::npos) {
    return absl::InvalidArgumentError("Invalid template text fields");
  }
  if (object_template.objects.empty() ||
      object_template.objects.size() > kMaxTemplateObjects) {
    return absl::InvalidArgumentError("Template must contain 1..512 objects");
  }
  for (size_t i = 0; i < object_template.objects.size(); ++i) {
    const auto& object = object_template.objects[i];
    if (object.id < 0 || object.id > 0xFFF || object.rel_x < -63 ||
        object.rel_x > 63 || object.rel_y < -63 || object.rel_y > 63 ||
        object.size < 0 || object.size > 15 || object.layer < 0 ||
        object.layer > 2) {
      return absl::InvalidArgumentError(absl::StrFormat(
          "Template object %d has out-of-range integer fields", i));
    }
  }
  if (object_template.retaining_wall_loops.size() > 128)
    return absl::InvalidArgumentError("At most 128 retaining-wall loops");
  std::set<std::string> loop_names;
  size_t total_membership = 0;
  for (const auto& loop : object_template.retaining_wall_loops) {
    if (loop.name.empty() || loop.name.size() > 256 ||
        loop.name.find('\0') != std::string::npos ||
        !loop_names.insert(loop.name).second || loop.members.empty() ||
        loop.members.size() > object_template.objects.size())
      return absl::InvalidArgumentError(
          "Invalid retaining-wall loop declaration");
    if (loop.members.size() > kMaxTemplateObjects - total_membership)
      return absl::InvalidArgumentError(
          "At most 512 total retaining-wall memberships");
    total_membership += loop.members.size();
    std::set<size_t> members;
    for (size_t member : loop.members) {
      if (member >= object_template.objects.size() ||
          !members.insert(member).second)
        return absl::InvalidArgumentError(
            "Retaining-wall loop members must be unique existing indices");
    }
  }
  return absl::OkStatus();
}

using CellKey = std::tuple<uint8_t, int, int>;  // Physical plane, y, x.
using CellMap = std::map<CellKey, RoomSpatialCell>;

CellMap UniqueCells(const RoomObjectFootprint& footprint) {
  CellMap cells;
  for (const auto& cell : footprint.cells) {
    cells[{cell.layer, cell.y, cell.x}] = cell;
  }
  return cells;
}

ObjectTemplatePlanConflict& AddConflict(ObjectTemplatePlacementPlan& plan,
                                        ObjectTemplatePlanConflictKind kind,
                                        size_t index, std::string message,
                                        bool blocking = true) {
  ObjectTemplatePlanConflict conflict;
  conflict.kind = kind;
  conflict.template_index = index;
  conflict.message = std::move(message);
  conflict.blocking = blocking;
  plan.conflicts.push_back(std::move(conflict));
  return plan.conflicts.back();
}

void AddCell(ObjectTemplatePlanConflict& conflict,
             const RoomSpatialCell& cell) {
  ++conflict.cell_count;
  if (conflict.cells.size() < kMaxReportedCells)
    conflict.cells.push_back(cell);
}

ObjectTemplatePlanConflict IntersectCells(const CellMap& first,
                                          const CellMap& second) {
  ObjectTemplatePlanConflict conflict;
  auto a = first.begin();
  auto b = second.begin();
  while (a != first.end() && b != second.end()) {
    if (a->first < b->first) {
      ++a;
    } else if (b->first < a->first) {
      ++b;
    } else {
      AddCell(conflict, a->second);
      ++a;
      ++b;
    }
  }
  return conflict;
}

bool IsRetainingStraight(int id) {
  return id == 0x003 || id == 0x004 || id == 0x063 || id == 0x064;
}

bool IsRetainingMember(int id) {
  return IsRetainingStraight(id) || (id >= 0x108 && id <= 0x10F);
}

bool IsGrayArchitecture(int id) {
  return id == 0x001 || id == 0x002 || id == 0x061 || id == 0x062 ||
         (id >= 0x100 && id <= 0x107);
}

std::vector<ObjectTemplateWallPort> RetainingPorts(
    const RoomObjectFootprint& object, size_t member) {
  std::vector<ObjectTemplateWallPort> ports;
  const auto add = [&](const char* direction, int family) {
    int x = object.anchor_x, y = object.anchor_y;
    if (std::string_view(direction) == "east")
      x = object.bounds.right();
    if (std::string_view(direction) == "south")
      y = object.bounds.bottom();
    ports.push_back({member, object.object_id, family, direction, x, y});
  };
  switch (object.object_id) {
    case 0x003:
    case 0x004:
      add("west", object.object_id);
      add("east", object.object_id);
      break;
    case 0x063:
    case 0x064:
      add("north", object.object_id);
      add("south", object.object_id);
      break;
    case 0x108:
      add("east", 0x003);
      add("south", 0x063);
      break;
    case 0x109:
      add("east", 0x004);
      add("north", 0x063);
      break;
    case 0x10A:
      add("west", 0x003);
      add("south", 0x064);
      break;
    case 0x10B:
      add("west", 0x004);
      add("north", 0x064);
      break;
    case 0x10C:
      add("west", 0x003);
      add("north", 0x063);
      break;
    case 0x10D:
      add("west", 0x004);
      add("south", 0x063);
      break;
    case 0x10E:
      add("east", 0x003);
      add("north", 0x064);
      break;
    case 0x10F:
      add("east", 0x004);
      add("south", 0x064);
      break;
  }
  return ports;
}

bool OppositePorts(const ObjectTemplateWallPort& a,
                   const ObjectTemplateWallPort& b) {
  return (a.direction == "north" && b.direction == "south") ||
         (a.direction == "south" && b.direction == "north") ||
         (a.direction == "east" && b.direction == "west") ||
         (a.direction == "west" && b.direction == "east");
}

bool MatchingPortLocations(const ObjectTemplateWallPort& a,
                           const ObjectTemplateWallPort& b) {
  if (a.x == b.x && a.y == b.y)
    return true;
  const auto north_overlap = [](const auto& wall, const auto& corner) {
    return wall.object_id == 0x063 && wall.direction == "south" &&
           corner.object_id == 0x10C && corner.direction == "north" &&
           wall.x == corner.x && wall.y == corner.y + 1;
  };
  return north_overlap(a, b) || north_overlap(b, a);
}

RoomSpatialBounds ConnectionRegion(const ObjectTemplateWallPort& port) {
  int x = port.x, y = port.y;
  if (port.direction == "west")
    x -= 4;
  if (port.direction == "north")
    y -= 4;
  return {x, y, 4, 4};
}

void CheckRetainingWallLoops(ObjectTemplatePlacementPlan& plan,
                             const ObjectTemplate& object_template,
                             std::span<const RoomObjectFootprint> existing,
                             size_t existing_count) {
  if (object_template.retaining_wall_loops.empty())
    return;
  std::map<size_t, const RoomObjectFootprint*> measured;
  for (const auto& footprint : plan.footprints)
    measured[footprint.object_index] = &footprint;
  std::vector<RoomObjectFootprint> combined(existing.begin(), existing.end());
  for (auto footprint : plan.footprints) {
    footprint.object_index += existing_count;
    combined.push_back(std::move(footprint));
  }
  std::stable_sort(
      combined.begin(), combined.end(),
      [](const auto& a, const auto& b) { return a.list_index < b.list_index; });
  // Only native architecture participates. This is deliberately not a final
  // pixel replay: water, floors, masks and decorations retain their own rules.
  std::map<CellKey, std::pair<size_t, uint16_t>> architecture;
  for (size_t i = 0; i < combined.size(); ++i) {
    const auto& footprint = combined[i];
    if ((!IsRetainingMember(footprint.object_id) &&
         !IsGrayArchitecture(footprint.object_id)) ||
        !footprint.measured || !footprint.full_bounds_measured ||
        footprint.outside_room || footprint.layers.custom_override_active)
      continue;
    for (const auto& cell : footprint.cells)
      architecture[{cell.layer, cell.y, cell.x}] = {i, cell.word};
  }
  for (const auto& loop : object_template.retaining_wall_loops) {
    ObjectTemplateWallLoopCheck report;
    report.name = loop.name;
    report.members = loop.members;
    bool blocked = false;
    const auto fail = [&](size_t member, const std::string& message,
                          std::optional<RoomSpatialBounds> region =
                              std::nullopt) {
      blocked = true;
      report.diagnostics.push_back(message);
      auto& conflict =
          AddConflict(plan, ObjectTemplatePlanConflictKind::kAssemblyConnection,
                      member, loop.name + ": " + message);
      conflict.expected_region = region;
      if (!region && member < plan.placements.size())
        conflict.expected_region = RoomSpatialBounds{
            plan.placements[member].x_, plan.placements[member].y_, 4, 4};
    };
    for (size_t member : loop.members) {
      const auto found = measured.find(member);
      if (found == measured.end() ||
          !IsRetainingMember(object_template.objects[member].id)) {
        fail(member, "Unsupported or unmeasured native retaining-wall member");
        continue;
      }
      const auto& footprint = *found->second;
      if (CountUnassessedRetainingWallObjects({&footprint, 1}) != 0) {
        fail(member,
             "Native retaining-wall binding or complete trace is unqualified",
             footprint.bounds);
        continue;
      }
      const auto ports = RetainingPorts(footprint, member);
      report.required_ports.insert(report.required_ports.end(), ports.begin(),
                                   ports.end());
    }
    auto& ports = report.required_ports;
    for (size_t i = 0; i < loop.members.size(); ++i) {
      const auto first = measured.find(loop.members[i]);
      if (first == measured.end())
        continue;
      for (size_t j = i + 1; j < loop.members.size(); ++j) {
        const auto second = measured.find(loop.members[j]);
        if (second == measured.end())
          continue;
        auto overlap = IntersectCells(UniqueCells(*first->second),
                                      UniqueCells(*second->second));
        if (overlap.cell_count == 0)
          continue;
        const auto proven_overlap = [](const auto& wall, const auto& corner) {
          return wall.object_id == 0x063 && corner.object_id == 0x10C &&
                 wall.anchor_x == corner.anchor_x &&
                 wall.anchor_y < corner.anchor_y &&
                 wall.bounds.bottom() == corner.anchor_y + 1;
        };
        if (overlap.cell_count == 8 &&
            (proven_overlap(*first->second, *second->second) ||
             proven_overlap(*second->second, *first->second)))
          continue;
        blocked = true;
        overlap.kind = ObjectTemplatePlanConflictKind::kAssemblyConnection;
        overlap.template_index = loop.members[i];
        overlap.other_template_index = loop.members[j];
        overlap.blocking = true;
        overlap.expected_region = RoomSpatialBounds{
            overlap.cells.front().x, overlap.cells.front().y, 1, 1};
        overlap.message = loop.name +
                          ": declared wall members overlap outside the proven "
                          "native corner seam";
        report.diagnostics.push_back(overlap.message);
        plan.conflicts.push_back(std::move(overlap));
      }
    }
    std::vector<std::vector<size_t>> matches(ports.size());
    std::set<size_t> unsupported_corner_ports;
    for (size_t i = 0; i < ports.size(); ++i) {
      for (size_t j = i + 1; j < ports.size(); ++j) {
        if (ports[i].template_index == ports[j].template_index ||
            ports[i].straight_id != ports[j].straight_id ||
            !OppositePorts(ports[i], ports[j]) ||
            !MatchingPortLocations(ports[i], ports[j]))
          continue;
        if (!IsRetainingStraight(ports[i].object_id) &&
            !IsRetainingStraight(ports[j].object_id)) {
          unsupported_corner_ports.insert(i);
          unsupported_corner_ports.insert(j);
          continue;
        }
        matches[i].push_back(j);
        matches[j].push_back(i);
      }
    }
    std::map<size_t, std::set<size_t>> neighbors;
    for (size_t i = 0; i < ports.size(); ++i) {
      if (matches[i].size() != 1) {
        const std::string reason =
            unsupported_corner_ports.contains(i)
                ? "Unsupported corner-to-corner connection; this profile "
                  "requires a straight shoulder"
                : "Expected exactly one opposite " + ports[i].direction +
                      " port of native family " +
                      std::to_string(ports[i].straight_id);
        fail(ports[i].template_index, reason, ConnectionRegion(ports[i]));
        continue;
      }
      const size_t j = matches[i].front();
      if (matches[j].size() != 1)
        continue;
      neighbors[ports[i].template_index].insert(ports[j].template_index);
      if (i < j)
        ++report.matched_connection_count;
    }
    if (!blocked) {
      std::set<size_t> visited;
      std::vector<size_t> pending{loop.members.front()};
      while (!pending.empty()) {
        const size_t member = pending.back();
        pending.pop_back();
        if (!visited.insert(member).second)
          continue;
        for (size_t neighbor : neighbors[member])
          pending.push_back(neighbor);
      }
      if (visited.size() != loop.members.size() ||
          report.matched_connection_count != loop.members.size())
        fail(loop.members.front(),
             "Members must form one connected closed cycle");
    }
    for (size_t member : loop.members) {
      const auto found = measured.find(member);
      if (found == measured.end())
        continue;
      std::map<size_t, ObjectTemplatePlanConflict> gray_losses;
      for (const auto& cell : found->second->cells) {
        const auto owner = architecture.find({cell.layer, cell.y, cell.x});
        if (owner == architecture.end())
          continue;
        const auto& writer = combined[owner->second.first];
        if (IsGrayArchitecture(writer.object_id) &&
            owner->second.second != cell.word)
          AddCell(gray_losses[owner->second.first], cell);
      }
      for (auto& [position, conflict] : gray_losses) {
        blocked = true;
        const auto& writer = combined[position];
        conflict.kind = ObjectTemplatePlanConflictKind::kAssemblyConnection;
        conflict.template_index = member;
        conflict.blocking = true;
        conflict.expected_region = found->second->bounds;
        if (writer.object_index < existing_count)
          conflict.existing_index = writer.object_index;
        else
          conflict.other_template_index = writer.object_index - existing_count;
        conflict.message = loop.name +
                           ": later native gray architecture replaces declared "
                           "dark retaining cells";
        report.diagnostics.push_back(conflict.message);
        plan.conflicts.push_back(std::move(conflict));
      }
    }
    if (!blocked)
      report.status = "closed_ports_not_final_pixels";
    report.diagnostics.push_back(
        "Source-bound native port cycle and gray-architecture survival only; "
        "not full room, final pixels, collision, stairs or gameplay "
        "qualification");
    plan.retaining_wall_loop_checks.push_back(std::move(report));
  }
}

void CheckNewWallJoinLosses(ObjectTemplatePlacementPlan& plan,
                            std::vector<RoomObjectFootprint> existing,
                            size_t existing_count) {
  const auto painter_order = [](const auto& a, const auto& b) {
    return a.list_index < b.list_index;
  };
  std::stable_sort(existing.begin(), existing.end(), painter_order);
  const auto baseline = InspectRetainingWallJoins(existing);
  auto combined = existing;
  for (auto footprint : plan.footprints) {
    footprint.object_index += existing_count;
    combined.push_back(std::move(footprint));
  }
  // Stable original identities survive sorting. Within a list, normal append
  // places proposed objects after existing objects, as the room writer does.
  std::stable_sort(combined.begin(), combined.end(), painter_order);
  plan.retaining_wall_join_unassessed_objects =
      CountUnassessedRetainingWallObjects(combined);
  for (auto issue : InspectRetainingWallJoins(combined)) {
    std::set<CellKey> already_lost;
    for (const auto& prior : baseline) {
      if (prior.face_index == issue.face_index &&
          prior.face_is_layout == issue.face_is_layout &&
          prior.corner_index == issue.corner_index &&
          prior.corner_is_layout == issue.corner_is_layout) {
        for (const auto& cell : prior.lost_cells)
          already_lost.emplace(cell.layer, cell.y, cell.x);
      }
    }
    ObjectTemplatePlanConflict conflict;
    for (const auto& cell : issue.lost_cells) {
      if (!already_lost.contains({cell.layer, cell.y, cell.x}))
        AddCell(conflict, cell);
    }
    if (conflict.cell_count == 0)
      continue;  // An unrelated plan must not inherit baseline failures.
    conflict.kind = ObjectTemplatePlanConflictKind::kWallJoinLoss;
    conflict.blocking = true;  // Geometry overlap allowances cannot waive this.
    std::set<CellKey> newly_lost;
    for (const auto& cell : conflict.cells)
      newly_lost.emplace(cell.layer, cell.y, cell.x);
    std::map<CellKey, std::pair<bool, size_t>> new_cell_owners;
    // Attribute only the new cells, retaining last-writer order. An earlier
    // proposed repaint can own baseline losses without causing this conflict.
    for (const auto& footprint : combined) {
      const bool reported_owner =
          std::any_of(issue.overwriters.begin(), issue.overwriters.end(),
                      [&](const auto& owner) {
                        return owner.object_index == footprint.object_index &&
                               owner.is_layout == footprint.is_layout;
                      });
      if (!reported_owner)
        continue;
      for (const auto& cell : footprint.cells) {
        const CellKey key{cell.layer, cell.y, cell.x};
        if (newly_lost.contains(key))
          new_cell_owners[key] = {footprint.is_layout, footprint.object_index};
      }
    }
    bool proposed_owner = false;
    for (const auto& [key, owner] : new_cell_owners) {
      if (!owner.first && owner.second >= existing_count) {
        conflict.template_index = owner.second - existing_count;
        proposed_owner = true;
        break;
      }
    }
    if (!proposed_owner) {
      const size_t source = issue.face_index >= existing_count
                                ? issue.face_index
                                : issue.corner_index;
      conflict.template_index =
          source >= existing_count ? source - existing_count : 0;
    }
    if (issue.corner_index < existing_count)
      conflict.existing_index = issue.corner_index;
    else if (issue.face_index < existing_count)
      conflict.existing_index = issue.face_index;
    conflict.message =
        "New native 003-to-10C retaining-wall endpoint loss; reconnect the "
        "wall assembly. Explicit overlap allowances do not waive join loss.";
    plan.conflicts.push_back(std::move(conflict));
    plan.retaining_wall_join_issues.push_back(std::move(issue));
  }
}

}  // namespace

absl::StatusOr<ObjectTemplate> ParseObjectTemplateJson(
    std::string_view payload) {
  if (payload.empty() || payload.size() > kMaxTemplateBytes) {
    return absl::InvalidArgumentError(
        "Template JSON must contain 1..1048576 bytes");
  }
  try {
    bool duplicate_key = false;
    std::vector<std::set<std::string>> keys;
    const json value = json::parse(
        payload.begin(), payload.end(),
        [&](int depth, json::parse_event_t event, json& parsed) {
          if (depth > 8)
            throw std::invalid_argument("Template JSON is too deep");
          if (event == json::parse_event_t::object_start)
            keys.emplace_back();
          if (event == json::parse_event_t::key &&
              !keys.back().insert(parsed.get<std::string>()).second) {
            duplicate_key = true;
          }
          if (event == json::parse_event_t::object_end)
            keys.pop_back();
          return true;
        });
    if (duplicate_key ||
        !(HasExactFields(value,
                         {"name", "description", "category", "objects"}) ||
          HasExactFields(value, {"name", "description", "category", "objects",
                                 "retaining_wall_loops"})) ||
        !value["name"].is_string() || !value["description"].is_string() ||
        !value["category"].is_string() || !value["objects"].is_array() ||
        value["objects"].empty() ||
        value["objects"].size() > kMaxTemplateObjects) {
      return absl::InvalidArgumentError(
          "Invalid or incomplete template payload");
    }
    ObjectTemplate object_template;
    object_template.name = value["name"].get<std::string>();
    object_template.description = value["description"].get<std::string>();
    object_template.category = value["category"].get<std::string>();
    for (const auto& entry : value["objects"]) {
      if (!HasExactFields(entry, {"id", "rel_x", "rel_y", "size", "layer"}) ||
          !IsBoundedInteger(entry["id"], 0, 0xFFF) ||
          !IsBoundedInteger(entry["rel_x"], -63, 63) ||
          !IsBoundedInteger(entry["rel_y"], -63, 63) ||
          !IsBoundedInteger(entry["size"], 0, 15) ||
          !IsBoundedInteger(entry["layer"], 0, 2)) {
        return absl::InvalidArgumentError("Invalid template object fields");
      }
      object_template.objects.push_back(
          {entry["id"].get<int>(), entry["rel_x"].get<int>(),
           entry["rel_y"].get<int>(), entry["size"].get<int>(),
           entry["layer"].get<int>()});
    }
    if (value.contains("retaining_wall_loops")) {
      const auto& loops = value["retaining_wall_loops"];
      if (!loops.is_array() || loops.size() > 128)
        return absl::InvalidArgumentError("Invalid retaining_wall_loops array");
      for (const auto& entry : loops) {
        if (!HasExactFields(entry, {"name", "members"}) ||
            !entry["name"].is_string() || !entry["members"].is_array())
          return absl::InvalidArgumentError(
              "Invalid retaining-wall loop fields");
        ObjectTemplateRetainingWallLoop loop;
        loop.name = entry["name"].get<std::string>();
        for (const auto& member : entry["members"]) {
          if (!IsBoundedInteger(member, 0, object_template.objects.size() - 1))
            return absl::InvalidArgumentError("Invalid loop member index");
          loop.members.push_back(member.get<size_t>());
        }
        object_template.retaining_wall_loops.push_back(std::move(loop));
      }
    }
    const auto status = ValidateTemplateContract(object_template);
    if (!status.ok())
      return status;
    return object_template;
  } catch (const std::exception& error) {
    return absl::InvalidArgumentError(
        absl::StrFormat("Invalid template JSON: %s", error.what()));
  }
}

absl::StatusOr<ObjectTemplate> LoadObjectTemplateFile(const std::string& path) {
  std::ifstream input(path, std::ios::binary);
  if (!input.is_open()) {
    return absl::NotFoundError(
        absl::StrFormat("Cannot open template: %s", path));
  }
  std::string payload(kMaxTemplateBytes + 1, '\0');
  input.read(payload.data(), static_cast<std::streamsize>(payload.size()));
  if (input.bad()) {
    return absl::DataLossError(
        absl::StrFormat("Cannot read template: %s", path));
  }
  payload.resize(static_cast<size_t>(input.gcount()));
  return ParseObjectTemplateJson(payload);
}

ObjectTemplatePlacementPlan PlanObjectTemplatePlacement(
    Rom* rom, int room_id, const ObjectTemplate& object_template,
    ObjectTemplateTile8Anchor anchor,
    std::span<const RoomObject> existing_objects,
    const ObjectTemplatePlanOptions& options, const DungeonState* state) {
  ObjectTemplatePlacementPlan plan;
  const auto contract = ValidateTemplateContract(object_template);
  if (!contract.ok()) {
    AddConflict(plan, ObjectTemplatePlanConflictKind::kInvalidTemplate, 0,
                std::string(contract.message()));
    return plan;
  }
  if (anchor.x < 0 || anchor.x > 63 || anchor.y < 0 || anchor.y > 63) {
    AddConflict(plan, ObjectTemplatePlanConflictKind::kInvalidPlacement, 0,
                "Tile8 anchor must be in 0..63 on each axis");
    return plan;
  }
  for (const auto& clearance : options.clearance) {
    const auto& bounds = clearance.bounds;
    if (bounds.x < 0 || bounds.y < 0 || bounds.width <= 0 ||
        bounds.height <= 0 ||
        static_cast<int64_t>(bounds.x) + bounds.width > 64 ||
        static_cast<int64_t>(bounds.y) + bounds.height > 64 ||
        (clearance.plane && *clearance.plane > 1)) {
      AddConflict(plan, ObjectTemplatePlanConflictKind::kInvalidTemplate, 0,
                  "Invalid tile8 clearance rectangle or physical plane");
      return plan;
    }
  }
  for (size_t index : options.allowed_existing_indices) {
    if (index >= existing_objects.size()) {
      AddConflict(plan, ObjectTemplatePlanConflictKind::kInvalidTemplate, 0,
                  "Allowed overlap references a missing existing object");
      return plan;
    }
  }
  for (const auto& [proposed, existing] : options.allowed_existing_pairs) {
    if (proposed >= object_template.objects.size() ||
        existing >= existing_objects.size()) {
      AddConflict(plan, ObjectTemplatePlanConflictKind::kInvalidTemplate, 0,
                  "Allowed overlap pair references a missing object");
      return plan;
    }
  }
  // Check every coordinate before narrowing to RoomObject's byte fields.
  for (size_t i = 0; i < object_template.objects.size(); ++i) {
    const auto& object = object_template.objects[i];
    const int x = anchor.x + object.rel_x;
    const int y = anchor.y + object.rel_y;
    if (x < 0 || x > 63 || y < 0 || y > 63) {
      AddConflict(plan, ObjectTemplatePlanConflictKind::kInvalidPlacement, i,
                  "Translated tile8 anchor lies outside the room");
    }
  }
  if (!plan.conflicts.empty())
    return plan;

  std::vector<CellMap> proposed_cells(object_template.objects.size());
  for (size_t i = 0; i < object_template.objects.size(); ++i) {
    const auto& source = object_template.objects[i];
    plan.placements.emplace_back(source.id, anchor.x + source.rel_x,
                                 anchor.y + source.rel_y, source.size,
                                 source.layer);
    if (i > 0 && source.layer < object_template.objects[i - 1].layer) {
      AddConflict(plan, ObjectTemplatePlanConflictKind::kInvalidPlacement, i,
                  "Template draw-list order must be nondecreasing");
    }
    const auto encoding =
        ValidateRoomObjectStreamEntryForSave(plan.placements.back());
    if (!encoding.ok()) {
      AddConflict(plan, ObjectTemplatePlanConflictKind::kInvalidPlacement, i,
                  std::string(encoding.message()));
      continue;
    }
    auto footprint =
        MeasureRoomObjectFootprint(rom, room_id, plan.placements.back(), state);
    if (!footprint.ok()) {
      AddConflict(plan, ObjectTemplatePlanConflictKind::kUnmeasuredFootprint, i,
                  std::string(footprint.status().message()));
      continue;
    }
    footprint->object_index = i;
    if (!footprint->measured || !footprint->full_bounds_measured) {
      AddConflict(plan, ObjectTemplatePlanConflictKind::kUnmeasuredFootprint, i,
                  "Full unclipped rendered bounds could not be established");
      plan.footprints.push_back(std::move(*footprint));
      continue;
    }
    if (footprint->outside_room) {
      AddConflict(plan, ObjectTemplatePlanConflictKind::kOutsideRoom, i,
                  "Rendered footprint extends outside the 64x64 tile8 room");
    }
    proposed_cells[i] = UniqueCells(*footprint);
    for (size_t c = 0; c < options.clearance.size(); ++c) {
      const auto& clearance = options.clearance[c];
      const auto& bounds = clearance.bounds;
      ObjectTemplatePlanConflict conflict;
      for (const auto& [key, cell] : proposed_cells[i]) {
        if ((!clearance.plane || cell.layer == *clearance.plane) &&
            cell.x >= bounds.x && cell.y >= bounds.y &&
            cell.x < bounds.x + bounds.width &&
            cell.y < bounds.y + bounds.height) {
          AddCell(conflict, cell);
        }
      }
      if (conflict.cell_count != 0) {
        conflict.kind = ObjectTemplatePlanConflictKind::kClearance;
        conflict.template_index = i;
        conflict.message = absl::StrFormat("Footprint enters clearance %d", c);
        plan.conflicts.push_back(std::move(conflict));
      }
    }
    plan.footprints.push_back(std::move(*footprint));
  }

  std::vector<RoomObjectFootprint> existing_footprints;
  for (size_t e = 0; e < existing_objects.size(); ++e) {
    auto footprint =
        MeasureRoomObjectFootprint(rom, room_id, existing_objects[e], state);
    if (footprint.ok()) {
      footprint->object_index = e;
      existing_footprints.push_back(*footprint);
    } else {
      RoomObjectFootprint missing;
      missing.object_index = e;
      missing.object_id = existing_objects[e].id_;
      missing.list_index = existing_objects[e].GetLayerValue();
      existing_footprints.push_back(std::move(missing));
    }
    if (!footprint.ok() || !footprint->measured ||
        !footprint->full_bounds_measured) {
      const std::string reason =
          footprint.ok() ? "full unclipped bounds unavailable"
                         : std::string(footprint.status().message());
      auto& conflict = AddConflict(
          plan, ObjectTemplatePlanConflictKind::kUnmeasuredFootprint, 0,
          absl::StrFormat("Cannot measure existing object %d: %s", e, reason));
      conflict.existing_index = e;
      continue;
    }
    const CellMap existing_cells = UniqueCells(*footprint);
    for (size_t i = 0; i < proposed_cells.size(); ++i) {
      auto conflict = IntersectCells(proposed_cells[i], existing_cells);
      if (conflict.cell_count == 0)
        continue;
      conflict.kind = ObjectTemplatePlanConflictKind::kExistingOverlap;
      conflict.template_index = i;
      conflict.existing_index = e;
      const bool allowed =
          std::find(options.allowed_existing_indices.begin(),
                    options.allowed_existing_indices.end(),
                    e) != options.allowed_existing_indices.end() ||
          std::find(options.allowed_existing_pairs.begin(),
                    options.allowed_existing_pairs.end(),
                    std::make_pair(i, e)) !=
              options.allowed_existing_pairs.end();
      conflict.blocking = !allowed;
      conflict.message = allowed ? "Explicitly allowed existing-object overlap"
                                 : "Unapproved existing-object overlap";
      plan.conflicts.push_back(std::move(conflict));
    }
  }
  for (size_t i = 0; i < proposed_cells.size(); ++i) {
    for (size_t previous = 0; previous < i; ++previous) {
      auto conflict =
          IntersectCells(proposed_cells[i], proposed_cells[previous]);
      if (conflict.cell_count == 0)
        continue;
      conflict.kind = ObjectTemplatePlanConflictKind::kTemplateOverlap;
      conflict.template_index = i;
      conflict.other_template_index = previous;
      conflict.blocking = false;
      conflict.message = "Ordered intra-template repaint";
      plan.conflicts.push_back(std::move(conflict));
    }
  }
  CheckRetainingWallLoops(plan, object_template, existing_footprints,
                          existing_objects.size());
  CheckNewWallJoinLosses(plan, std::move(existing_footprints),
                         existing_objects.size());
  plan.valid =
      std::none_of(plan.conflicts.begin(), plan.conflicts.end(),
                   [](const auto& conflict) { return conflict.blocking; });
  return plan;
}

ObjectTemplateManager::ObjectTemplateManager() {
  // Initialize with some default templates if needed
}

absl::Status ObjectTemplateManager::LoadTemplates(
    const std::string& directory_path) {
  templates_.clear();

  if (!std::filesystem::exists(directory_path)) {
    // It's okay if the directory doesn't exist yet, just return empty
    return absl::OkStatus();
  }

  for (const auto& entry :
       std::filesystem::directory_iterator(directory_path)) {
    if (entry.path().extension() == ".json") {
      auto object_template = LoadObjectTemplateFile(entry.path().string());
      if (object_template.ok())
        templates_.push_back(std::move(*object_template));
      else
        std::cerr << "Failed to load template " << entry.path() << ": "
                  << object_template.status() << std::endl;
    }
  }

  return absl::OkStatus();
}

absl::Status ObjectTemplateManager::SaveTemplate(
    const ObjectTemplate& tmpl, const std::string& directory_path) {
  const auto contract = ValidateTemplateContract(tmpl);
  if (!contract.ok())
    return contract;
  if (!std::filesystem::exists(directory_path)) {
    std::filesystem::create_directories(directory_path);
  }

  // Sanitize filename
  std::string filename = tmpl.name;
  std::replace(filename.begin(), filename.end(), ' ', '_');
  std::string path =
      absl::StrFormat("%s/%s.json", directory_path.c_str(), filename.c_str());

  try {
    json j = tmpl;
    std::ofstream o(path);
    o << std::setw(4) << j << std::endl;

    // Add to in-memory list if not present
    bool exists = false;
    for (auto& t : templates_) {
      if (t.name == tmpl.name) {
        t = tmpl;  // Update existing
        exists = true;
        break;
      }
    }
    if (!exists) {
      templates_.push_back(tmpl);
    }

    return absl::OkStatus();
  } catch (const std::exception& e) {
    return absl::InternalError(
        absl::StrFormat("Failed to save template: %s", e.what()));
  }
}

ObjectTemplate ObjectTemplateManager::CreateFromObjects(
    const std::string& name, const std::string& description,
    const std::vector<RoomObject>& objects, int origin_x, int origin_y) {
  ObjectTemplate tmpl;
  tmpl.name = name;
  tmpl.description = description;
  tmpl.category = "Custom";  // Default category

  for (const auto& obj : objects) {
    TemplateObject t_obj;
    t_obj.id = obj.id_;
    t_obj.rel_x = obj.x_ - origin_x;
    t_obj.rel_y = obj.y_ - origin_y;
    t_obj.size = obj.size_;
    t_obj.layer = static_cast<int>(obj.layer_);
    tmpl.objects.push_back(t_obj);
  }

  return tmpl;
}

std::vector<RoomObject> ObjectTemplateManager::InstantiateTemplate(
    const ObjectTemplate& tmpl, int x, int y, Rom* rom) {
  std::vector<RoomObject> new_objects;

  for (const auto& t_obj : tmpl.objects) {
    int obj_x = x + t_obj.rel_x;
    int obj_y = y + t_obj.rel_y;

    // Basic bounds check
    if (obj_x >= 0 && obj_x < 64 && obj_y >= 0 && obj_y < 64) {
      RoomObject obj(t_obj.id, obj_x, obj_y, t_obj.size, t_obj.layer);
      if (rom) {
        obj.SetRom(rom);
        obj.EnsureTilesLoaded();
      }
      new_objects.push_back(obj);
    }
  }

  return new_objects;
}

}  // namespace zelda3
}  // namespace yaze
