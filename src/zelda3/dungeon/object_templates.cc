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
NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE(ObjectTemplate, name, description, category,
                                   objects)

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
        !HasExactFields(value,
                        {"name", "description", "category", "objects"}) ||
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
      try {
        std::ifstream i(entry.path());
        json j;
        i >> j;
        ObjectTemplate tmpl = j.get<ObjectTemplate>();
        templates_.push_back(tmpl);
      } catch (const std::exception& e) {
        std::cerr << "Failed to load template " << entry.path() << ": "
                  << e.what() << std::endl;
        // Continue loading other templates
      }
    }
  }

  return absl::OkStatus();
}

absl::Status ObjectTemplateManager::SaveTemplate(
    const ObjectTemplate& tmpl, const std::string& directory_path) {
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
