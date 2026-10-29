#include "core/project_plan_agent.h"

#include <algorithm>
#include <cctype>
#include <initializer_list>
#include <utility>

#include "absl/strings/ascii.h"
#include "absl/strings/str_format.h"
#include "core/source_artifact_publisher.h"
#include "util/macro.h"

namespace yaze::core {
namespace {

constexpr const char* kTargetKinds[] = {"step", "item", "decision",
                                        "milestone"};
constexpr const char* kStampKeys[] = {"agent", "observed_utc"};

bool IsSha256(std::string_view text) {
  return text.size() == 64 &&
         std::all_of(text.begin(), text.end(), [](unsigned char c) {
           return std::isdigit(c) || (c >= 'a' && c <= 'f');
         });
}

absl::StatusOr<PlanTarget> ParseObservationKey(std::string_view key) {
  PlanTarget target = PlanTarget::Parse(key);
  const bool known_kind =
      std::any_of(std::begin(kTargetKinds), std::end(kTargetKinds),
                  [&](const char* kind) { return target.kind == kind; });
  if (!known_kind || target.id.empty()) {
    return absl::InvalidArgumentError(absl::StrFormat(
        "Observation key must be step:, item:, decision: or milestone: "
        "followed by an ID: \"%s\"",
        std::string(key)));
  }
  return target;
}

template <typename Entry>
bool HasId(const std::vector<Entry>& entries, const std::string& id) {
  return std::any_of(entries.begin(), entries.end(),
                     [&](const Entry& entry) { return entry.id == id; });
}

bool PlanHasTarget(const ProjectPlan& plan, const PlanTarget& target) {
  if (target.kind == "step") {
    return HasId(plan.route_steps, target.id);
  }
  if (target.kind == "item") {
    return HasId(plan.route_items, target.id);
  }
  if (target.kind == "decision") {
    return HasId(plan.decisions, target.id);
  }
  if (target.kind == "milestone") {
    return HasId(plan.milestones, target.id);
  }
  return false;
}

absl::Status ValidateObservation(const std::string& key, const PlanJson& obs) {
  if (!obs.is_object()) {
    return absl::InvalidArgumentError(
        absl::StrFormat("Observation \"%s\" must be an object", key));
  }
  for (const char* field : {"state", "summary", "next"}) {
    auto it = obs.find(field);
    if (it != obs.end() && !it->is_string()) {
      return absl::InvalidArgumentError(absl::StrFormat(
          "Observation \"%s\": \"%s\" must be a string", key, field));
    }
  }
  if (auto it = obs.find("evidence"); it != obs.end()) {
    if (!it->is_array() ||
        !std::all_of(it->begin(), it->end(),
                     [](const PlanJson& v) { return v.is_string(); })) {
      return absl::InvalidArgumentError(absl::StrFormat(
          "Observation \"%s\": \"evidence\" must be an array of strings", key));
    }
  }
  if (auto it = obs.find("prs"); it != obs.end()) {
    if (!it->is_array() ||
        !std::all_of(it->begin(), it->end(), [](const PlanJson& v) {
          return v.is_number_integer() || v.is_string();
        })) {
      return absl::InvalidArgumentError(absl::StrFormat(
          "Observation \"%s\": \"prs\" must be an array of numbers or strings",
          key));
    }
  }
  return absl::OkStatus();
}

// Key order is not meaningful for comparison; stamps are not facts.
nlohmann::json Facts(const PlanJson& obs) {
  nlohmann::json facts = nlohmann::json::parse(obs.dump());
  for (const char* stamp : kStampKeys) {
    facts.erase(std::string(stamp));
  }
  return facts;
}

absl::StatusOr<const PlanJson*> ExistingSection(const ProjectPlan& plan) {
  auto it = plan.extra.find(std::string(kPlanAgentStatusKey));
  if (it == plan.extra.end()) {
    return static_cast<const PlanJson*>(nullptr);
  }
  if (!it->is_object()) {
    return absl::FailedPreconditionError(
        "The plan's \"agent_status\" section is not an object; repair it "
        "before applying agent updates");
  }
  auto observations = it->find("observations");
  if (observations != it->end() && !observations->is_object()) {
    return absl::FailedPreconditionError(
        "The plan's \"agent_status.observations\" is not an object; repair "
        "it before applying agent updates");
  }
  return &*it;
}

absl::Status ReadOptionalString(const PlanJson& root, const char* key,
                                std::string* out) {
  auto it = root.find(key);
  if (it == root.end() || it->is_null()) {
    return absl::OkStatus();
  }
  if (!it->is_string()) {
    return absl::InvalidArgumentError(
        absl::StrFormat("Agent update \"%s\" must be a string", key));
  }
  *out = it->get<std::string>();
  return absl::OkStatus();
}

}  // namespace

absl::StatusOr<PlanAgentUpdate> PlanAgentUpdate::Parse(std::string_view text) {
  PlanJson root = PlanJson::parse(text.begin(), text.end(), nullptr,
                                  /*allow_exceptions=*/false);
  if (root.is_discarded() || !root.is_object()) {
    return absl::InvalidArgumentError("Agent update must be a JSON object");
  }
  auto schema = root.find("schema");
  if (schema == root.end() || !schema->is_string() ||
      schema->get<std::string>() != kPlanAgentUpdateSchema) {
    return absl::InvalidArgumentError(
        absl::StrFormat("Agent update must declare \"schema\": \"%s\"",
                        std::string(kPlanAgentUpdateSchema)));
  }
  for (auto it = root.begin(); it != root.end(); ++it) {
    static constexpr const char* kKnown[] = {
        "schema",  "base_sha256",  "agent", "observed_utc",
        "sources", "observations", "remove"};
    if (std::none_of(std::begin(kKnown), std::end(kKnown),
                     [&](const char* key) { return it.key() == key; })) {
      return absl::InvalidArgumentError(absl::StrFormat(
          "Agent update has an unknown field \"%s\"", it.key()));
    }
  }

  PlanAgentUpdate update;
  RETURN_IF_ERROR(ReadOptionalString(root, "base_sha256", &update.base_sha256));
  update.base_sha256 = absl::AsciiStrToLower(update.base_sha256);
  if (!update.base_sha256.empty() && !IsSha256(update.base_sha256)) {
    return absl::InvalidArgumentError(
        "Agent update \"base_sha256\" must be 64 hexadecimal characters");
  }
  RETURN_IF_ERROR(ReadOptionalString(root, "agent", &update.agent));
  RETURN_IF_ERROR(
      ReadOptionalString(root, "observed_utc", &update.observed_utc));

  if (auto it = root.find("sources"); it != root.end() && !it->is_null()) {
    if (!it->is_object()) {
      return absl::InvalidArgumentError(
          "Agent update \"sources\" must be an object");
    }
    update.sources = *it;
  }
  if (auto it = root.find("observations"); it != root.end() && !it->is_null()) {
    if (!it->is_object()) {
      return absl::InvalidArgumentError(
          "Agent update \"observations\" must be an object");
    }
    update.observations = *it;
  }
  if (auto it = root.find("remove"); it != root.end() && !it->is_null()) {
    if (!it->is_array()) {
      return absl::InvalidArgumentError(
          "Agent update \"remove\" must be an array of observation keys");
    }
    for (const auto& key : *it) {
      if (!key.is_string()) {
        return absl::InvalidArgumentError(
            "Agent update \"remove\" must be an array of observation keys");
      }
      update.remove.push_back(key.get<std::string>());
    }
  }
  return update;
}

bool PlanAgentDiff::HasChanges() const {
  return sources_changed ||
         std::any_of(changes.begin(), changes.end(), [](const auto& change) {
           return change.kind != PlanAgentChangeKind::kUnchanged;
         });
}

const char* PlanAgentChangeKindName(PlanAgentChangeKind kind) {
  switch (kind) {
    case PlanAgentChangeKind::kAdded:
      return "added";
    case PlanAgentChangeKind::kChanged:
      return "changed";
    case PlanAgentChangeKind::kRemoved:
      return "removed";
    case PlanAgentChangeKind::kUnchanged:
      return "unchanged";
  }
  return "unchanged";
}

absl::StatusOr<PlanAgentDiff> DiffPlanAgentUpdate(
    const ProjectPlan& plan, const PlanAgentUpdate& update) {
  ASSIGN_OR_RETURN(const PlanJson* section, ExistingSection(plan));
  static const PlanJson kEmpty = PlanJson::object();
  const PlanJson* stored = &kEmpty;
  if (section != nullptr) {
    if (auto it = section->find("observations"); it != section->end()) {
      stored = &*it;
    }
  }

  PlanAgentDiff diff;
  for (auto it = update.observations.begin(); it != update.observations.end();
       ++it) {
    const std::string& key = it.key();
    ASSIGN_OR_RETURN(PlanTarget target, ParseObservationKey(key));
    if (!PlanHasTarget(plan, target)) {
      return absl::NotFoundError(absl::StrFormat(
          "The plan has no %s with ID \"%s\"; agent updates use existing "
          "plan IDs",
          target.kind, target.id));
    }
    RETURN_IF_ERROR(ValidateObservation(key, it.value()));
    if (std::find(update.remove.begin(), update.remove.end(), key) !=
        update.remove.end()) {
      return absl::InvalidArgumentError(absl::StrFormat(
          "Observation \"%s\" is both updated and removed", key));
    }
    auto current = stored->find(key);
    PlanAgentChangeKind kind = PlanAgentChangeKind::kAdded;
    if (current != stored->end()) {
      kind = Facts(*current) == Facts(it.value())
                 ? PlanAgentChangeKind::kUnchanged
                 : PlanAgentChangeKind::kChanged;
    }
    diff.changes.push_back({key, kind});
  }
  for (const auto& key : update.remove) {
    RETURN_IF_ERROR(ParseObservationKey(key).status());
    if (stored->contains(key)) {
      diff.changes.push_back({key, PlanAgentChangeKind::kRemoved});
    }
  }
  if (!update.sources.empty()) {
    const PlanJson* current_sources = nullptr;
    if (section != nullptr) {
      if (auto it = section->find("sources"); it != section->end()) {
        current_sources = &*it;
      }
    }
    diff.sources_changed = current_sources == nullptr ||
                           nlohmann::json::parse(current_sources->dump()) !=
                               nlohmann::json::parse(update.sources.dump());
  }
  return diff;
}

absl::StatusOr<PlanAgentDiff> ApplyPlanAgentUpdate(
    ProjectPlan* plan, const PlanAgentUpdate& update) {
  if (plan == nullptr) {
    return absl::InvalidArgumentError("A plan is required");
  }
  ASSIGN_OR_RETURN(PlanAgentDiff diff, DiffPlanAgentUpdate(*plan, update));
  if (!diff.HasChanges()) {
    return diff;
  }
  ASSIGN_OR_RETURN(const PlanJson* existing, ExistingSection(*plan));
  PlanJson section = existing != nullptr ? *existing : PlanJson::object();
  section["schema"] = std::string(kPlanAgentStatusSchema);
  if (!update.observed_utc.empty()) {
    section["updated_utc"] = update.observed_utc;
  }
  if (!update.agent.empty()) {
    section["agent"] = update.agent;
  }
  if (!update.sources.empty()) {
    section["sources"] = update.sources;
  }
  PlanJson& observations = section["observations"];
  if (observations.is_null()) {
    observations = PlanJson::object();
  }
  for (const auto& change : diff.changes) {
    switch (change.kind) {
      case PlanAgentChangeKind::kAdded:
      case PlanAgentChangeKind::kChanged: {
        PlanJson obs = update.observations.at(change.key);
        for (const char* stamp : kStampKeys) {
          obs.erase(std::string(stamp));
        }
        if (!update.agent.empty()) {
          obs["agent"] = update.agent;
        }
        if (!update.observed_utc.empty()) {
          obs["observed_utc"] = update.observed_utc;
        }
        observations[change.key] = std::move(obs);
        break;
      }
      case PlanAgentChangeKind::kRemoved:
        observations.erase(change.key);
        break;
      case PlanAgentChangeKind::kUnchanged:
        break;
    }
  }
  plan->extra[std::string(kPlanAgentStatusKey)] = std::move(section);
  return diff;
}

absl::StatusOr<PlanAgentFileResult> ApplyPlanAgentUpdateToFile(
    const std::string& path, const PlanAgentUpdate& update, bool dry_run) {
  if (!dry_run && update.base_sha256.empty()) {
    return absl::InvalidArgumentError(
        "Agent update needs \"base_sha256\" (the plan bytes it read) before "
        "it can be applied");
  }
  std::string source;
  ASSIGN_OR_RETURN(ProjectPlan plan, ProjectPlan::LoadFromFile(path, &source));
  PlanAgentFileResult result;
  result.before_sha256 = ComputeSourceArtifactSha256(source);
  if (!update.base_sha256.empty() &&
      update.base_sha256 != result.before_sha256) {
    return absl::FailedPreconditionError(absl::StrFormat(
        "Plan changed since the agent read it (base %s, current %s); reread "
        "the plan and rebuild the update. Nothing was written.",
        update.base_sha256, result.before_sha256));
  }
  ASSIGN_OR_RETURN(result.diff, ApplyPlanAgentUpdate(&plan, update));
  if (!result.diff.HasChanges()) {
    result.after_sha256 = result.before_sha256;
    return result;
  }
  // Replace only the agent section in the file's own JSON, so every other key,
  // its order and empty values keep the bytes the person last saved.
  PlanJson root = PlanJson::parse(source, nullptr, /*allow_exceptions=*/false);
  const std::string key(kPlanAgentStatusKey);
  root[key] = plan.extra.at(key);
  const std::string text = root.dump(1) + "\n";
  auto written = ProjectPlan::Parse(text);
  if (!written.ok() || written->Serialize() != plan.Serialize()) {
    return absl::InternalError(
        "Agent update did not reproduce the validated plan; nothing was "
        "written");
  }
  result.after_sha256 = ComputeSourceArtifactSha256(text);
  if (dry_run) {
    return result;
  }
  RETURN_IF_ERROR(ProjectPlan::PublishSource(path, source, text));
  result.written = true;
  return result;
}

const PlanJson* FindPlanAgentStatus(const ProjectPlan& plan) {
  auto it = plan.extra.find(std::string(kPlanAgentStatusKey));
  return it != plan.extra.end() && it->is_object() ? &*it : nullptr;
}

const PlanJson* FindPlanAgentObservation(const ProjectPlan& plan,
                                         std::string_view key) {
  const PlanJson* section = FindPlanAgentStatus(plan);
  if (section == nullptr) {
    return nullptr;
  }
  auto observations = section->find("observations");
  if (observations == section->end() || !observations->is_object()) {
    return nullptr;
  }
  auto it = observations->find(std::string(key));
  return it != observations->end() && it->is_object() ? &*it : nullptr;
}

std::vector<std::string> OrphanedPlanAgentObservations(
    const ProjectPlan& plan) {
  std::vector<std::string> orphans;
  const PlanJson* section = FindPlanAgentStatus(plan);
  if (section == nullptr) {
    return orphans;
  }
  auto observations = section->find("observations");
  if (observations == section->end() || !observations->is_object()) {
    return orphans;
  }
  for (auto it = observations->begin(); it != observations->end(); ++it) {
    auto target = ParseObservationKey(it.key());
    if (!target.ok() || !PlanHasTarget(plan, *target)) {
      orphans.push_back(it.key());
    }
  }
  return orphans;
}

void RenamePlanAgentObservation(ProjectPlan* plan, std::string_view before,
                                std::string_view after) {
  auto section = plan->extra.find(std::string(kPlanAgentStatusKey));
  if (section == plan->extra.end() || !section->is_object()) {
    return;
  }
  auto observations = section->find("observations");
  if (observations == section->end() || !observations->is_object()) {
    return;
  }
  auto it = observations->find(std::string(before));
  if (it == observations->end() || before == after) {
    return;
  }
  PlanJson moved = std::move(*it);
  observations->erase(std::string(before));
  (*observations)[std::string(after)] = std::move(moved);
}

bool AdoptPlanAgentOnlyChanges(ProjectPlan* draft,
                               std::string_view loaded_source,
                               std::string_view disk_source) {
  if (draft == nullptr) {
    return false;
  }
  auto loaded = ProjectPlan::Parse(loaded_source);
  auto disk = ProjectPlan::Parse(disk_source);
  if (!loaded.ok() || !disk.ok()) {
    return false;
  }
  const std::string key(kPlanAgentStatusKey);
  PlanJson disk_section;
  const bool disk_has_section = disk->extra.contains(key);
  if (disk_has_section) {
    disk_section = disk->extra[key];
  }
  loaded->extra.erase(key);
  disk->extra.erase(key);
  if (loaded->Serialize() != disk->Serialize()) {
    return false;
  }
  if (disk_has_section) {
    draft->extra[key] = std::move(disk_section);
  } else {
    draft->extra.erase(key);
  }
  return true;
}

}  // namespace yaze::core
