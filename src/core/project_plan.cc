#include "core/project_plan.h"

#include <algorithm>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <initializer_list>
#include <optional>
#include <set>
#include <sstream>
#include <system_error>
#include <utility>

#include "absl/strings/str_format.h"
#include "core/project.h"
#include "core/source_artifact_publisher.h"
#include "util/macro.h"

namespace yaze::core {
namespace {

absl::StatusOr<std::string> ReadSource(const std::string& path) {
  std::error_code error;
  const auto status = std::filesystem::status(path, error);
  if (error == std::errc::no_such_file_or_directory ||
      (!error && !std::filesystem::exists(status))) {
    return absl::NotFoundError(
        absl::StrFormat("File does not exist: %s", path));
  }
  if (error || !std::filesystem::is_regular_file(status)) {
    return absl::FailedPreconditionError(
        absl::StrFormat("Not a readable regular file: %s", path));
  }
  std::ifstream file(path, std::ios::binary);
  if (!file.is_open()) {
    std::error_code error;
    if (!std::filesystem::exists(path, error) && !error) {
      return absl::NotFoundError(
          absl::StrFormat("File does not exist: %s", path));
    }
    return absl::PermissionDeniedError(
        absl::StrFormat("Cannot read file: %s", path));
  }
  std::stringstream buffer;
  buffer << file.rdbuf();
  if (file.bad()) {
    return absl::DataLossError(
        absl::StrFormat("Failed to read file: %s", path));
  }
  return buffer.str();
}

template <typename Entry>
absl::Status ValidateIds(const std::vector<Entry>& entries, const char* kind) {
  std::set<std::string> ids;
  for (const auto& entry : entries) {
    if (entry.id.empty() || !ids.insert(entry.id).second) {
      return absl::InvalidArgumentError(absl::StrFormat(
          "%s IDs must be nonempty and unique: %s", kind, entry.id));
    }
  }
  return absl::OkStatus();
}

template <typename Entry>
std::set<std::string> EntryIds(const std::vector<Entry>& entries) {
  std::set<std::string> ids;
  for (const auto& entry : entries) {
    ids.insert(entry.id);
  }
  return ids;
}

std::string UnusedId(const char* prefix, const std::set<std::string>& ids) {
  for (size_t number = 1; number <= ids.size() + 1; ++number) {
    const std::string id = absl::StrFormat("%s%zu", prefix, number);
    if (!ids.contains(id)) {
      return id;
    }
  }
  return "";
}

void RewriteTargets(std::vector<std::string>* targets,
                    const std::string& before, const std::string& after) {
  for (auto& target : *targets) {
    if (target == before) {
      target = after;
    }
  }
}

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

}  // namespace

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
        ReadReferences(root, &plan.references)}) {
    if (!status.ok()) {
      return status;
    }
  }
  plan.extra = Extras(root, {"schema", "title", "about", "goal", "milestones",
                             "route", "decisions", "references"});
  RETURN_IF_ERROR(plan.ValidateIds());
  return plan;
}

absl::StatusOr<ProjectPlan> ProjectPlan::LoadFromFile(const std::string& path,
                                                      std::string* source) {
  ASSIGN_OR_RETURN(auto contents, ReadSource(path));
  auto plan = Parse(contents);
  if (!plan.ok()) {
    return absl::Status(
        plan.status().code(),
        absl::StrFormat("%s: %s", path, plan.status().message()));
  }
  if (source != nullptr) {
    *source = std::move(contents);
  }
  return plan;
}

absl::Status ProjectPlan::CreateAndLink(project::YazeProject* project,
                                        const std::string& path) {
  if (project == nullptr || project->filepath.empty() || path.empty()) {
    return absl::InvalidArgumentError(
        "A project file and new plan path are required");
  }
  const auto plan = CreateEmpty(project->name);
#ifdef __EMSCRIPTEN__
  RETURN_IF_ERROR(plan.SaveToFile(path));
  auto updated = *project;
  updated.planning.file = updated.GetRelativePath(path);
  updated.planning.section_present = true;
  const auto status = updated.Save();
  if (!status.ok()) {
    const auto current = ReadSource(path);
    if (!current.ok() || *current != plan.Serialize()) {
      return absl::DataLossError(absl::StrFormat(
          "Project save failed (%s); changed plan preserved at %s",
          status.message(), path));
    }
    std::error_code error;
    if (!std::filesystem::remove(path, error) || error) {
      return absl::DataLossError(absl::StrFormat(
          "Project save failed (%s); plan cleanup failed at %s: %s",
          status.message(), path, error.message()));
    }
    return status;
  }
#else
  ASSIGN_OR_RETURN(auto lock, AcquireSourceArtifactPublicationLock(
                                  {project->filepath, path},
                                  {"project plan", "project plan"}));
  ASSIGN_OR_RETURN(auto before, ReadSource(project->filepath));
  std::error_code error;
  if (std::filesystem::exists(path, error)) {
    return absl::AlreadyExistsError(
        absl::StrFormat("Plan file already exists: %s", path));
  }
  if (error) {
    return absl::FailedPreconditionError(error.message());
  }
  auto updated = *project;
  updated.planning.file = updated.GetRelativePath(path);
  updated.planning.section_present = true;
  ASSIGN_OR_RETURN(auto descriptor, updated.SerializeToString());
  RETURN_IF_ERROR(
      PublishSourceArtifacts(*lock,
                             {{project->filepath, before, descriptor},
                              {path, std::nullopt, plan.Serialize()}},
                             ComputeSourceArtifactSha256(before),
                             [&]() { return LoadFromFile(path).status(); }));
#endif
  project->planning = std::move(updated.planning);
  return absl::OkStatus();
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

  PutExtras(root, extra);
  return root.dump(1) + "\n";
}

absl::Status ProjectPlan::SaveToFile(const std::string& path) const {
  RETURN_IF_ERROR(ValidateIds());
  return project::WriteProjectFileAtomically(path, Serialize(), false);
}

absl::Status ProjectPlan::SaveToFile(const std::string& path,
                                     const std::string& expected_source) const {
  RETURN_IF_ERROR(ValidateIds());
#ifdef __EMSCRIPTEN__
  ASSIGN_OR_RETURN(auto current, ReadSource(path));
  if (current != expected_source) {
    return absl::AbortedError("Plan changed since it was loaded");
  }
  return project::WriteProjectFileAtomically(path, Serialize(), true);
#else
  ASSIGN_OR_RETURN(auto lock, AcquireSourceArtifactPublicationLock(
                                  {path}, {"project plan", "project plan"}));
  return PublishSourceArtifacts(*lock, {{path, expected_source, Serialize()}},
                                ComputeSourceArtifactSha256(expected_source),
                                [&]() { return LoadFromFile(path).status(); });
#endif
}

absl::Status ProjectPlan::ValidateIds() const {
  RETURN_IF_ERROR(core::ValidateIds(milestones, "Milestone"));
  RETURN_IF_ERROR(core::ValidateIds(route_steps, "Route step"));
  RETURN_IF_ERROR(core::ValidateIds(route_items, "Route item"));
  return core::ValidateIds(decisions, "Decision");
}

std::string ProjectPlan::NewMilestoneId() const {
  return UnusedId("m", EntryIds(milestones));
}

std::string ProjectPlan::NewStepId() const {
  auto ids = EntryIds(route_steps);
  for (const auto& item : route_items) {
    ids.insert(item.step);
    for (const auto& need : item.needs) {
      ids.insert(need.step);
    }
  }
  for (const auto& step : route_steps) {
    for (const auto& target : step.targets) {
      const auto parsed = PlanTarget::Parse(target);
      if (parsed.kind == "step") {
        ids.insert(parsed.id);
      }
    }
  }
  for (const auto& decision : decisions) {
    for (const auto& target : decision.targets) {
      const auto parsed = PlanTarget::Parse(target);
      if (parsed.kind == "step") {
        ids.insert(parsed.id);
      }
    }
  }
  return UnusedId("step-", ids);
}

std::string ProjectPlan::NewItemId() const {
  auto ids = EntryIds(route_items);
  for (const auto& step : route_steps) {
    for (const auto& target : step.targets) {
      const auto parsed = PlanTarget::Parse(target);
      if (parsed.kind == "item") {
        ids.insert(parsed.id);
      }
    }
  }
  for (const auto& decision : decisions) {
    for (const auto& target : decision.targets) {
      const auto parsed = PlanTarget::Parse(target);
      if (parsed.kind == "item") {
        ids.insert(parsed.id);
      }
    }
  }
  return UnusedId("item-", ids);
}

std::string ProjectPlan::NewDecisionId() const {
  auto ids = EntryIds(decisions);
  for (const auto& step : route_steps) {
    for (const auto& target : step.targets) {
      const auto parsed = PlanTarget::Parse(target);
      if (parsed.kind == "decision") {
        ids.insert(parsed.id);
      }
    }
  }
  for (const auto& decision : decisions) {
    for (const auto& target : decision.targets) {
      const auto parsed = PlanTarget::Parse(target);
      if (parsed.kind == "decision") {
        ids.insert(parsed.id);
      }
    }
  }
  return UnusedId("decision-", ids);
}

absl::Status ProjectPlan::RenameStep(std::string_view id, std::string new_id) {
  RETURN_IF_ERROR(ValidateIds());
  const int index = FindStep(id);
  if (index < 0) {
    return absl::NotFoundError("Route step was not found");
  }
  if (new_id.empty()) {
    return absl::InvalidArgumentError("Route step ID must be nonempty");
  }
  if (new_id != id && FindStep(new_id) >= 0) {
    return absl::AlreadyExistsError(
        "Route step ID must be nonempty and unique");
  }
  for (auto& item : route_items) {
    if (item.step == id) {
      item.step = new_id;
    }
    for (auto& need : item.needs) {
      if (need.step == id) {
        need.step = new_id;
      }
    }
  }
  for (auto& step : route_steps) {
    RewriteTargets(&step.targets, "step:" + std::string(id), "step:" + new_id);
  }
  for (auto& decision : decisions) {
    RewriteTargets(&decision.targets, "step:" + std::string(id),
                   "step:" + new_id);
  }
  route_steps[index].id = std::move(new_id);
  return absl::OkStatus();
}

absl::Status ProjectPlan::RenameItem(std::string_view id, std::string new_id) {
  RETURN_IF_ERROR(ValidateIds());
  auto item = std::find_if(route_items.begin(), route_items.end(),
                           [&](const auto& value) { return value.id == id; });
  if (item == route_items.end()) {
    return absl::NotFoundError("Route item was not found");
  }
  if (new_id.empty()) {
    return absl::InvalidArgumentError("Route item ID must be nonempty");
  }
  if (new_id != id &&
      std::any_of(route_items.begin(), route_items.end(),
                  [&](const auto& value) { return value.id == new_id; })) {
    return absl::AlreadyExistsError(
        "Route item ID must be nonempty and unique");
  }
  for (auto& step : route_steps) {
    RewriteTargets(&step.targets, "item:" + std::string(id), "item:" + new_id);
  }
  for (auto& decision : decisions) {
    RewriteTargets(&decision.targets, "item:" + std::string(id),
                   "item:" + new_id);
  }
  item->id = std::move(new_id);
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
