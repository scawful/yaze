#include "core/project_plan.h"

#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <initializer_list>
#include <sstream>
#include <system_error>
#include <utility>

#include "absl/strings/str_format.h"

namespace yaze::core {
namespace {

class Reader {
 public:
  explicit Reader(std::string context) : context_(std::move(context)) {}

  absl::Status status() const { return status_; }

  void String(const PlanJson& obj, const char* key, std::string* out) {
    auto it = obj.find(key);
    if (it == obj.end() || it->is_null()) {
      return;
    }
    if (!it->is_string()) {
      Fail(key, "a string");
      return;
    }
    *out = it->get<std::string>();
  }

  void Bool(const PlanJson& obj, const char* key, bool* out) {
    auto it = obj.find(key);
    if (it == obj.end() || it->is_null()) {
      return;
    }
    if (!it->is_boolean()) {
      Fail(key, "true or false");
      return;
    }
    *out = it->get<bool>();
  }

  void Int(const PlanJson& obj, const char* key, int* out) {
    auto it = obj.find(key);
    if (it == obj.end() || it->is_null()) {
      return;
    }
    if (!it->is_number_integer()) {
      Fail(key, "an integer");
      return;
    }
    *out = it->get<int>();
  }

  // Positions written by other tools may be fractional; they are rounded.
  void Coord(const PlanJson& obj, const char* key, int* out) {
    auto it = obj.find(key);
    if (it == obj.end() || it->is_null()) {
      return;
    }
    if (!it->is_number()) {
      Fail(key, "a number");
      return;
    }
    *out = static_cast<int>(std::lround(it->get<double>()));
  }

  void Strings(const PlanJson& obj, const char* key,
               std::vector<std::string>* out) {
    auto it = obj.find(key);
    if (it == obj.end() || it->is_null()) {
      return;
    }
    if (!it->is_array()) {
      Fail(key, "an array of strings");
      return;
    }
    for (const auto& value : *it) {
      if (!value.is_string()) {
        Fail(key, "an array of strings");
        return;
      }
      out->push_back(value.get<std::string>());
    }
  }

  // Returns the array at `key`, or an empty array when absent.
  const PlanJson& Array(const PlanJson& obj, const char* key) {
    static const PlanJson kEmpty = PlanJson::array();
    auto it = obj.find(key);
    if (it == obj.end() || it->is_null()) {
      return kEmpty;
    }
    if (!it->is_array()) {
      Fail(key, "an array");
      return kEmpty;
    }
    return *it;
  }

  // [[x, y], ...]
  void Points(const PlanJson& obj, const char* key,
              std::vector<std::pair<int, int>>* out) {
    auto it = obj.find(key);
    if (it == obj.end() || it->is_null()) {
      return;
    }
    if (!it->is_array()) {
      Fail(key, "an array of [x, y] pairs");
      return;
    }
    for (const auto& point : *it) {
      if (!point.is_array() || point.size() != 2 || !point[0].is_number() ||
          !point[1].is_number()) {
        Fail(key, "an array of [x, y] pairs");
        return;
      }
      out->emplace_back(static_cast<int>(std::lround(point[0].get<double>())),
                        static_cast<int>(std::lround(point[1].get<double>())));
    }
  }

  bool RequireObject(const PlanJson& value, const std::string& where) {
    if (value.is_object()) {
      return true;
    }
    if (status_.ok()) {
      status_ = absl::InvalidArgumentError(
          absl::StrFormat("%s: %s must be an object", context_, where));
    }
    return false;
  }

 private:
  void Fail(const char* key, const char* expected) {
    if (status_.ok()) {
      status_ = absl::InvalidArgumentError(
          absl::StrFormat("%s: \"%s\" must be %s", context_, key, expected));
    }
  }

  std::string context_;
  absl::Status status_;
};

PlanJson Extras(const PlanJson& obj, std::initializer_list<const char*> known) {
  PlanJson extra = PlanJson::object();
  for (auto it = obj.begin(); it != obj.end(); ++it) {
    const bool is_known =
        std::any_of(known.begin(), known.end(),
                    [&](const char* key) { return it.key() == key; });
    if (!is_known) {
      extra[it.key()] = it.value();
    }
  }
  return extra;
}

void PutString(PlanJson& obj, const char* key, const std::string& value) {
  if (!value.empty()) {
    obj[key] = value;
  }
}

void PutStrings(PlanJson& obj, const char* key,
                const std::vector<std::string>& values) {
  if (!values.empty()) {
    obj[key] = values;
  }
}

void PutExtras(PlanJson& obj, const PlanJson& extra) {
  if (!extra.is_object()) {
    return;
  }
  for (auto it = extra.begin(); it != extra.end(); ++it) {
    if (!obj.contains(it.key())) {
      obj[it.key()] = it.value();
    }
  }
}

absl::Status ReadGoal(const PlanJson& root, PlanGoal* goal) {
  auto it = root.find("goal");
  if (it == root.end() || it->is_null()) {
    return absl::OkStatus();
  }
  Reader reader("goal");
  if (!reader.RequireObject(*it, "goal")) {
    return reader.status();
  }
  reader.String(*it, "title", &goal->title);
  reader.String(*it, "detail", &goal->detail);
  reader.String(*it, "due", &goal->due);
  goal->extra = Extras(*it, {"title", "detail", "due"});
  return reader.status();
}

absl::Status ReadMilestones(const PlanJson& root,
                            std::vector<PlanMilestone>* out) {
  Reader top("plan");
  const PlanJson& list = top.Array(root, "milestones");
  for (size_t i = 0; i < list.size() && top.status().ok(); ++i) {
    Reader reader(absl::StrFormat("milestones[%d]", i));
    if (!reader.RequireObject(list[i], "entry")) {
      return reader.status();
    }
    PlanMilestone m;
    reader.String(list[i], "id", &m.id);
    reader.String(list[i], "title", &m.title);
    reader.String(list[i], "start", &m.start);
    reader.String(list[i], "end", &m.end);
    reader.String(list[i], "status", &m.status);
    reader.String(list[i], "detail", &m.detail);
    m.extra =
        Extras(list[i], {"id", "title", "start", "end", "status", "detail"});
    if (!reader.status().ok()) {
      return reader.status();
    }
    out->push_back(std::move(m));
  }
  return top.status();
}

absl::Status ReadNeeds(const PlanJson& item, const std::string& context,
                       std::vector<PlanNeed>* out) {
  Reader top(context);
  const PlanJson& list = top.Array(item, "needs");
  for (size_t i = 0; i < list.size() && top.status().ok(); ++i) {
    Reader reader(absl::StrFormat("%s.needs[%d]", context, i));
    if (!reader.RequireObject(list[i], "entry")) {
      return reader.status();
    }
    PlanNeed need;
    reader.String(list[i], "step", &need.step);
    reader.String(list[i], "what", &need.what);
    need.extra = Extras(list[i], {"step", "what"});
    if (!reader.status().ok()) {
      return reader.status();
    }
    out->push_back(std::move(need));
  }
  return top.status();
}

absl::Status ReadRoute(const PlanJson& root, ProjectPlan* plan) {
  auto it = root.find("route");
  if (it == root.end() || it->is_null()) {
    return absl::OkStatus();
  }
  Reader top("route");
  if (!top.RequireObject(*it, "route")) {
    return top.status();
  }
  const PlanJson& route = *it;
  plan->route_extra = Extras(route, {"steps", "items"});

  const PlanJson& steps = top.Array(route, "steps");
  for (size_t i = 0; i < steps.size() && top.status().ok(); ++i) {
    Reader reader(absl::StrFormat("route.steps[%d]", i));
    if (!reader.RequireObject(steps[i], "entry")) {
      return reader.status();
    }
    PlanRouteStep step;
    reader.String(steps[i], "id", &step.id);
    reader.String(steps[i], "code", &step.code);
    reader.String(steps[i], "title", &step.title);
    reader.String(steps[i], "note", &step.note);
    reader.String(steps[i], "group", &step.group);
    reader.String(steps[i], "kind", &step.kind);
    reader.Strings(steps[i], "targets", &step.targets);
    step.extra = Extras(
        steps[i], {"id", "code", "title", "note", "group", "kind", "targets"});
    if (!reader.status().ok()) {
      return reader.status();
    }
    plan->route_steps.push_back(std::move(step));
  }

  const PlanJson& items = top.Array(route, "items");
  for (size_t i = 0; i < items.size() && top.status().ok(); ++i) {
    const std::string context = absl::StrFormat("route.items[%d]", i);
    Reader reader(context);
    if (!reader.RequireObject(items[i], "entry")) {
      return reader.status();
    }
    PlanRouteItem item;
    reader.String(items[i], "id", &item.id);
    reader.String(items[i], "name", &item.name);
    reader.String(items[i], "step", &item.step);
    reader.String(items[i], "category", &item.category);
    reader.Bool(items[i], "required", &item.required);
    reader.String(items[i], "status", &item.status);
    reader.String(items[i], "where", &item.where);
    reader.String(items[i], "note", &item.note);
    item.extra = Extras(items[i], {"id", "name", "step", "category", "required",
                                   "status", "where", "note", "needs"});
    if (!reader.status().ok()) {
      return reader.status();
    }
    auto needs_status = ReadNeeds(items[i], context, &item.needs);
    if (!needs_status.ok()) {
      return needs_status;
    }
    plan->route_items.push_back(std::move(item));
  }
  return top.status();
}

absl::Status ReadDecisions(const PlanJson& root,
                           std::vector<PlanDecision>* out) {
  Reader top("plan");
  const PlanJson& list = top.Array(root, "decisions");
  for (size_t i = 0; i < list.size() && top.status().ok(); ++i) {
    Reader reader(absl::StrFormat("decisions[%d]", i));
    if (!reader.RequireObject(list[i], "entry")) {
      return reader.status();
    }
    PlanDecision d;
    reader.String(list[i], "id", &d.id);
    reader.String(list[i], "title", &d.title);
    reader.String(list[i], "status", &d.status);
    reader.Int(list[i], "priority", &d.priority);
    reader.String(list[i], "recommendation", &d.recommendation);
    reader.String(list[i], "answer", &d.answer);
    reader.Strings(list[i], "targets", &d.targets);
    reader.Strings(list[i], "refs", &d.refs);
    d.extra = Extras(list[i], {"id", "title", "status", "priority",
                               "recommendation", "answer", "targets", "refs"});
    if (!reader.status().ok()) {
      return reader.status();
    }
    out->push_back(std::move(d));
  }
  return top.status();
}

absl::Status ReadReferences(const PlanJson& root,
                            std::vector<PlanReference>* out) {
  Reader top("plan");
  const PlanJson& list = top.Array(root, "references");
  for (size_t i = 0; i < list.size() && top.status().ok(); ++i) {
    Reader reader(absl::StrFormat("references[%d]", i));
    if (!reader.RequireObject(list[i], "entry")) {
      return reader.status();
    }
    PlanReference r;
    reader.String(list[i], "label", &r.label);
    reader.String(list[i], "path", &r.path);
    reader.String(list[i], "author", &r.author);
    reader.String(list[i], "date", &r.date);
    r.extra = Extras(list[i], {"label", "path", "author", "date"});
    if (!reader.status().ok()) {
      return reader.status();
    }
    out->push_back(std::move(r));
  }
  return top.status();
}

template <typename T, typename ReadEntry>
absl::Status ReadEntries(const PlanJson& parent, const char* key,
                         const std::string& context, std::vector<T>* out,
                         ReadEntry read_entry) {
  Reader top(context);
  const PlanJson& list = top.Array(parent, key);
  for (size_t i = 0; i < list.size() && top.status().ok(); ++i) {
    Reader reader(absl::StrFormat("%s.%s[%d]", context, key, i));
    if (!reader.RequireObject(list[i], "entry")) {
      return reader.status();
    }
    T entry;
    read_entry(reader, list[i], &entry);
    if (!reader.status().ok()) {
      return reader.status();
    }
    out->push_back(std::move(entry));
  }
  return top.status();
}

absl::Status ReadOverworld(const PlanJson& root, PlanOverworld* ow) {
  auto it = root.find("overworld");
  if (it == root.end() || it->is_null()) {
    return absl::OkStatus();
  }
  Reader top("overworld");
  if (!top.RequireObject(*it, "overworld")) {
    return top.status();
  }
  const PlanJson& section = *it;
  ow->extra =
      Extras(section, {"worlds", "markers", "notes", "lines", "regions"});

  auto worlds = ReadEntries(section, "worlds", "overworld", &ow->worlds,
                            [](Reader& r, const PlanJson& j, PlanWorld* w) {
                              r.String(j, "id", &w->id);
                              r.String(j, "title", &w->title);
                              r.String(j, "base", &w->base);
                              r.Int(j, "maps", &w->maps);
                              w->extra =
                                  Extras(j, {"id", "title", "base", "maps"});
                            });
  auto markers = ReadEntries(
      section, "markers", "overworld", &ow->markers,
      [](Reader& r, const PlanJson& j, PlanMarker* m) {
        r.String(j, "id", &m->id);
        r.String(j, "world", &m->world);
        r.Coord(j, "x", &m->x);
        r.Coord(j, "y", &m->y);
        r.String(j, "map", &m->map);
        r.String(j, "kind", &m->kind);
        r.String(j, "label", &m->label);
        r.String(j, "detail", &m->detail);
        r.String(j, "status", &m->status);
        r.String(j, "code", &m->code);
        r.String(j, "room", &m->room);
        m->extra = Extras(j, {"id", "world", "x", "y", "map", "kind", "label",
                              "detail", "status", "code", "room"});
      });
  auto notes = ReadEntries(section, "notes", "overworld", &ow->notes,
                           [](Reader& r, const PlanJson& j, PlanNote* n) {
                             r.String(j, "id", &n->id);
                             r.String(j, "world", &n->world);
                             r.Coord(j, "x", &n->x);
                             r.Coord(j, "y", &n->y);
                             r.String(j, "map", &n->map);
                             r.String(j, "text", &n->text);
                             r.String(j, "created", &n->created);
                             n->extra = Extras(j, {"id", "world", "x", "y",
                                                   "map", "text", "created"});
                           });
  auto lines = ReadEntries(section, "lines", "overworld", &ow->lines,
                           [](Reader& r, const PlanJson& j, PlanLine* l) {
                             r.String(j, "id", &l->id);
                             r.String(j, "world", &l->world);
                             r.String(j, "color", &l->color);
                             r.Points(j, "points", &l->points);
                             l->extra =
                                 Extras(j, {"id", "world", "color", "points"});
                           });
  auto regions = ReadEntries(section, "regions", "overworld", &ow->regions,
                             [](Reader& r, const PlanJson& j, PlanRegion* g) {
                               r.String(j, "id", &g->id);
                               r.String(j, "world", &g->world);
                               r.Coord(j, "x", &g->x);
                               r.Coord(j, "y", &g->y);
                               r.Coord(j, "radius", &g->radius);
                               r.String(j, "label", &g->label);
                               r.String(j, "detail", &g->detail);
                               r.String(j, "status", &g->status);
                               g->extra =
                                   Extras(j, {"id", "world", "x", "y", "radius",
                                              "label", "detail", "status"});
                             });
  for (const auto& status : {worlds, markers, notes, lines, regions}) {
    if (!status.ok()) {
      return status;
    }
  }
  return absl::OkStatus();
}

PlanJson SerializeOverworld(const PlanOverworld& ow) {
  PlanJson section = PlanJson::object();
  PlanJson worlds = PlanJson::array();
  for (const auto& w : ow.worlds) {
    PlanJson j = PlanJson::object();
    PutString(j, "id", w.id);
    PutString(j, "title", w.title);
    PutString(j, "base", w.base);
    j["maps"] = w.maps;
    PutExtras(j, w.extra);
    worlds.push_back(std::move(j));
  }
  section["worlds"] = std::move(worlds);

  PlanJson markers = PlanJson::array();
  for (const auto& m : ow.markers) {
    PlanJson j = PlanJson::object();
    PutString(j, "id", m.id);
    PutString(j, "world", m.world);
    j["x"] = m.x;
    j["y"] = m.y;
    PutString(j, "map", m.map);
    PutString(j, "kind", m.kind);
    PutString(j, "label", m.label);
    PutString(j, "detail", m.detail);
    PutString(j, "status", m.status);
    PutString(j, "code", m.code);
    PutString(j, "room", m.room);
    PutExtras(j, m.extra);
    markers.push_back(std::move(j));
  }
  section["markers"] = std::move(markers);

  PlanJson notes = PlanJson::array();
  for (const auto& n : ow.notes) {
    PlanJson j = PlanJson::object();
    PutString(j, "id", n.id);
    PutString(j, "world", n.world);
    j["x"] = n.x;
    j["y"] = n.y;
    PutString(j, "map", n.map);
    PutString(j, "text", n.text);
    PutString(j, "created", n.created);
    PutExtras(j, n.extra);
    notes.push_back(std::move(j));
  }
  section["notes"] = std::move(notes);

  PlanJson lines = PlanJson::array();
  for (const auto& l : ow.lines) {
    PlanJson j = PlanJson::object();
    PutString(j, "id", l.id);
    PutString(j, "world", l.world);
    PutString(j, "color", l.color);
    PlanJson points = PlanJson::array();
    for (const auto& [x, y] : l.points) {
      points.push_back(PlanJson::array({x, y}));
    }
    j["points"] = std::move(points);
    PutExtras(j, l.extra);
    lines.push_back(std::move(j));
  }
  section["lines"] = std::move(lines);

  PlanJson regions = PlanJson::array();
  for (const auto& g : ow.regions) {
    PlanJson j = PlanJson::object();
    PutString(j, "id", g.id);
    PutString(j, "world", g.world);
    j["x"] = g.x;
    j["y"] = g.y;
    j["radius"] = g.radius;
    PutString(j, "label", g.label);
    PutString(j, "detail", g.detail);
    PutString(j, "status", g.status);
    PutExtras(j, g.extra);
    regions.push_back(std::move(j));
  }
  section["regions"] = std::move(regions);

  PutExtras(section, ow.extra);
  return section;
}

}  // namespace

int PlanWorld::BaseMap() const {
  char* end = nullptr;
  const long value = std::strtol(base.c_str(), &end, 0);
  if (end == base.c_str() || *end != '\0' || value < 0) {
    return 0;
  }
  return static_cast<int>(value);
}

int PlanWorld::Rows() const {
  return std::max(1, (maps + kPlanWorldColumns - 1) / kPlanWorldColumns);
}

int PlanWorld::MapAt(int x, int y) const {
  if (x < 0 || y < 0) {
    return -1;
  }
  const int column = x / kPlanAreaSize;
  const int row = y / kPlanAreaSize;
  const int index = row * kPlanWorldColumns + column;
  if (column >= kPlanWorldColumns || index >= maps) {
    return -1;
  }
  return BaseMap() + index;
}

std::string PlanWorld::MapAfterMove(const std::string& current, int old_x,
                                    int old_y, int new_x, int new_y) const {
  const int new_map = MapAt(new_x, new_y);
  if (new_map < 0) {
    return current;
  }
  if (!current.empty() && MapAt(old_x, old_y) == new_map) {
    return current;
  }
  return FormatPlanId(new_map);
}

std::string FormatPlanId(int id) {
  return absl::StrFormat("0x%02X", id);
}

bool PlanOverworld::empty() const {
  return worlds.empty() && markers.empty() && notes.empty() && lines.empty() &&
         regions.empty() && extra.empty();
}

const PlanWorld* PlanOverworld::FindWorld(std::string_view id) const {
  for (const auto& world : worlds) {
    if (world.id == id) {
      return &world;
    }
  }
  return nullptr;
}

int PlanOverworld::FindMarker(std::string_view id) const {
  for (size_t i = 0; i < markers.size(); ++i) {
    if (markers[i].id == id) {
      return static_cast<int>(i);
    }
  }
  return -1;
}

int PlanOverworld::FindMarkerByCode(std::string_view code) const {
  if (code.empty()) {
    return -1;
  }
  for (size_t i = 0; i < markers.size(); ++i) {
    if (markers[i].code == code) {
      return static_cast<int>(i);
    }
  }
  return -1;
}

PlanTarget PlanTarget::Parse(std::string_view text) {
  PlanTarget target;
  const size_t colon = text.find(':');
  if (colon == std::string_view::npos) {
    target.id = std::string(text);
    return target;
  }
  target.kind = std::string(text.substr(0, colon));
  target.id = std::string(text.substr(colon + 1));
  return target;
}

int PlanTarget::NumericId() const {
  if ((kind != "room" && kind != "map") || id.empty()) {
    return -1;
  }
  char* end = nullptr;
  const long value = std::strtol(id.c_str(), &end, 0);
  if (end == id.c_str() || *end != '\0' || value < 0) {
    return -1;
  }
  return static_cast<int>(value);
}

ProjectPlan ProjectPlan::CreateEmpty(std::string title) {
  ProjectPlan plan;
  plan.title = std::move(title);
  return plan;
}

absl::StatusOr<ProjectPlan> ProjectPlan::Parse(std::string_view text) {
  PlanJson root = PlanJson::parse(text.begin(), text.end(), nullptr,
                                  /*allow_exceptions=*/false);
  if (root.is_discarded()) {
    return absl::InvalidArgumentError("Plan file is not valid JSON");
  }
  if (!root.is_object()) {
    return absl::InvalidArgumentError("Plan file must be a JSON object");
  }
  auto schema = root.find("schema");
  if (schema == root.end() || !schema->is_string() ||
      schema->get<std::string>() != kProjectPlanSchema) {
    return absl::InvalidArgumentError(
        absl::StrFormat("Plan file must declare \"schema\": \"%s\"",
                        std::string(kProjectPlanSchema)));
  }

  ProjectPlan plan;
  Reader reader("plan");
  reader.String(root, "title", &plan.title);
  reader.String(root, "about", &plan.about);
  if (!reader.status().ok()) {
    return reader.status();
  }
  for (auto status :
       {ReadGoal(root, &plan.goal), ReadMilestones(root, &plan.milestones),
        ReadRoute(root, &plan), ReadDecisions(root, &plan.decisions),
        ReadReferences(root, &plan.references),
        ReadOverworld(root, &plan.overworld)}) {
    if (!status.ok()) {
      return status;
    }
  }
  plan.extra = Extras(root, {"schema", "title", "about", "goal", "milestones",
                             "route", "decisions", "references", "overworld"});
  return plan;
}

absl::StatusOr<ProjectPlan> ProjectPlan::LoadFromFile(const std::string& path) {
  std::ifstream file(path, std::ios::binary);
  if (!file.is_open()) {
    return absl::NotFoundError(
        absl::StrFormat("Cannot open plan file: %s", path));
  }
  std::stringstream buffer;
  buffer << file.rdbuf();
  auto plan = Parse(buffer.str());
  if (!plan.ok()) {
    return absl::Status(
        plan.status().code(),
        absl::StrFormat("%s: %s", path, plan.status().message()));
  }
  return plan;
}

std::string ProjectPlan::Serialize() const {
  PlanJson root = PlanJson::object();
  root["schema"] = std::string(kProjectPlanSchema);
  PutString(root, "title", title);
  PutString(root, "about", about);

  PlanJson goal_json = PlanJson::object();
  PutString(goal_json, "title", goal.title);
  PutString(goal_json, "detail", goal.detail);
  PutString(goal_json, "due", goal.due);
  PutExtras(goal_json, goal.extra);
  if (!goal_json.empty()) {
    root["goal"] = std::move(goal_json);
  }

  PlanJson milestones_json = PlanJson::array();
  for (const auto& m : milestones) {
    PlanJson j = PlanJson::object();
    PutString(j, "id", m.id);
    PutString(j, "title", m.title);
    PutString(j, "start", m.start);
    PutString(j, "end", m.end);
    PutString(j, "status", m.status);
    PutString(j, "detail", m.detail);
    PutExtras(j, m.extra);
    milestones_json.push_back(std::move(j));
  }
  root["milestones"] = std::move(milestones_json);

  PlanJson route = PlanJson::object();
  PlanJson steps = PlanJson::array();
  for (const auto& s : route_steps) {
    PlanJson j = PlanJson::object();
    PutString(j, "id", s.id);
    PutString(j, "code", s.code);
    PutString(j, "title", s.title);
    PutString(j, "note", s.note);
    PutString(j, "group", s.group);
    PutString(j, "kind", s.kind);
    PutStrings(j, "targets", s.targets);
    PutExtras(j, s.extra);
    steps.push_back(std::move(j));
  }
  route["steps"] = std::move(steps);
  PlanJson items = PlanJson::array();
  for (const auto& item : route_items) {
    PlanJson j = PlanJson::object();
    PutString(j, "id", item.id);
    PutString(j, "name", item.name);
    PutString(j, "step", item.step);
    PutString(j, "category", item.category);
    j["required"] = item.required;
    PutString(j, "status", item.status);
    PutString(j, "where", item.where);
    PutString(j, "note", item.note);
    if (!item.needs.empty()) {
      PlanJson needs = PlanJson::array();
      for (const auto& need : item.needs) {
        PlanJson n = PlanJson::object();
        PutString(n, "step", need.step);
        PutString(n, "what", need.what);
        PutExtras(n, need.extra);
        needs.push_back(std::move(n));
      }
      j["needs"] = std::move(needs);
    }
    PutExtras(j, item.extra);
    items.push_back(std::move(j));
  }
  route["items"] = std::move(items);
  PutExtras(route, route_extra);
  root["route"] = std::move(route);

  PlanJson decisions_json = PlanJson::array();
  for (const auto& d : decisions) {
    PlanJson j = PlanJson::object();
    PutString(j, "id", d.id);
    if (d.priority != 0) {
      j["priority"] = d.priority;
    }
    PutString(j, "status", d.status);
    PutString(j, "title", d.title);
    PutString(j, "recommendation", d.recommendation);
    PutString(j, "answer", d.answer);
    PutStrings(j, "targets", d.targets);
    PutStrings(j, "refs", d.refs);
    PutExtras(j, d.extra);
    decisions_json.push_back(std::move(j));
  }
  root["decisions"] = std::move(decisions_json);

  PlanJson references_json = PlanJson::array();
  for (const auto& r : references) {
    PlanJson j = PlanJson::object();
    PutString(j, "label", r.label);
    PutString(j, "path", r.path);
    PutString(j, "author", r.author);
    PutString(j, "date", r.date);
    PutExtras(j, r.extra);
    references_json.push_back(std::move(j));
  }
  root["references"] = std::move(references_json);

  if (!overworld.empty()) {
    root["overworld"] = SerializeOverworld(overworld);
  }

  PutExtras(root, extra);
  return root.dump(1) + "\n";
}

absl::Status ProjectPlan::SaveToFile(const std::string& path) const {
  const std::filesystem::path target(path);
  std::filesystem::path temp = target;
  temp += ".tmp";
  {
    std::ofstream file(temp, std::ios::binary | std::ios::trunc);
    if (!file.is_open()) {
      return absl::InvalidArgumentError(
          absl::StrFormat("Cannot write plan file: %s", temp.string()));
    }
    const std::string text = Serialize();
    file.write(text.data(), static_cast<std::streamsize>(text.size()));
    file.close();
    if (file.fail()) {
      std::error_code ignored;
      std::filesystem::remove(temp, ignored);
      return absl::InternalError(
          absl::StrFormat("Failed to write plan file: %s", temp.string()));
    }
  }
  std::error_code error;
  std::filesystem::rename(temp, target, error);
  if (error) {
    std::error_code ignored;
    std::filesystem::remove(temp, ignored);
    return absl::InternalError(absl::StrFormat(
        "Failed to replace plan file %s: %s", path, error.message()));
  }
  return absl::OkStatus();
}

int ProjectPlan::FindStep(std::string_view id) const {
  for (size_t i = 0; i < route_steps.size(); ++i) {
    if (route_steps[i].id == id) {
      return static_cast<int>(i);
    }
  }
  return -1;
}

std::vector<const PlanRouteItem*> ProjectPlan::ItemsAtStep(
    std::string_view step) const {
  std::vector<const PlanRouteItem*> out;
  for (const auto& item : route_items) {
    if (item.step == step) {
      out.push_back(&item);
    }
  }
  return out;
}

std::vector<const PlanDecision*> ProjectPlan::OpenDecisions() const {
  std::vector<const PlanDecision*> out;
  for (const auto& d : decisions) {
    if (d.status.empty() || d.status == "open") {
      out.push_back(&d);
    }
  }
  std::stable_sort(out.begin(), out.end(),
                   [](const PlanDecision* a, const PlanDecision* b) {
                     const int pa = a->priority > 0 ? a->priority : 1 << 30;
                     const int pb = b->priority > 0 ? b->priority : 1 << 30;
                     return pa < pb;
                   });
  return out;
}

const PlanMilestone* ProjectPlan::CurrentMilestone(
    std::string_view today) const {
  for (const auto& m : milestones) {
    if (m.status == "active") {
      return &m;
    }
  }
  for (const auto& m : milestones) {
    if (!m.start.empty() && !m.end.empty() && m.start <= today &&
        today <= m.end) {
      return &m;
    }
  }
  return nullptr;
}

}  // namespace yaze::core
