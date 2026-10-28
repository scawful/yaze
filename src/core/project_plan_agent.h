#ifndef YAZE_CORE_PROJECT_PLAN_AGENT_H_
#define YAZE_CORE_PROJECT_PLAN_AGENT_H_

#include <string>
#include <string_view>
#include <vector>

#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "core/project_plan.h"

namespace yaze::core {

// Agent observations live in one top-level plan section that people do not
// edit. Every other plan field (notes, answers, titles, route order, markers,
// unknown sections) belongs to the person editing the plan, and an agent update
// cannot reach it. Observations are keyed by existing plan targets:
// "step:<id>", "item:<id>", "decision:<id>" or "milestone:<id>".
//
//   "agent_status": {
//     "schema": "yaze.plan.agent_status/1",
//     "updated_utc": "...", "agent": "...", "sources": {...},
//     "observations": {"step:D4": {"state": "...", "next": "...", ...}}
//   }
inline constexpr std::string_view kPlanAgentStatusKey = "agent_status";
inline constexpr std::string_view kPlanAgentStatusSchema =
    "yaze.plan.agent_status/1";
inline constexpr std::string_view kPlanAgentUpdateSchema =
    "yaze.plan.agent_update/1";

// One agent's proposed observations, read from a "yaze.plan.agent_update/1"
// document. `base_sha256` names the plan bytes the agent read; a file whose
// bytes have changed since then is refused without being written.
struct PlanAgentUpdate {
  std::string base_sha256;
  std::string agent;
  std::string observed_utc;
  PlanJson sources = PlanJson::object();
  // "<kind>:<id>" -> observation object. Each replaces the stored observation.
  PlanJson observations = PlanJson::object();
  // Observation keys to delete, including keys whose entry no longer exists.
  std::vector<std::string> remove;

  static absl::StatusOr<PlanAgentUpdate> Parse(std::string_view text);
};

enum class PlanAgentChangeKind { kAdded, kChanged, kRemoved, kUnchanged };

struct PlanAgentChange {
  std::string key;
  PlanAgentChangeKind kind = PlanAgentChangeKind::kUnchanged;
};

struct PlanAgentDiff {
  std::vector<PlanAgentChange> changes;
  bool sources_changed = false;

  bool HasChanges() const;
};

const char* PlanAgentChangeKindName(PlanAgentChangeKind kind);

// Validates the update against `plan` and reports what applying it changes.
// Comparison ignores the "agent" and "observed_utc" stamps, so resending the
// same facts later is reported as unchanged.
absl::StatusOr<PlanAgentDiff> DiffPlanAgentUpdate(
    const ProjectPlan& plan, const PlanAgentUpdate& update);

// Validates, then writes only `plan->extra["agent_status"]`. A rejected update
// leaves `plan` untouched; an unchanged update leaves it byte-identical.
absl::StatusOr<PlanAgentDiff> ApplyPlanAgentUpdate(
    ProjectPlan* plan, const PlanAgentUpdate& update);

struct PlanAgentFileResult {
  std::string before_sha256;
  std::string after_sha256;
  PlanAgentDiff diff;
  bool written = false;
};

// Reads the plan at `path`, refuses a stale `base_sha256`, applies the update,
// and publishes through the shared source-CAS publisher. A dry run, a refused
// update and an unchanged update never write the file. `base_sha256` may be
// empty only for a dry run.
absl::StatusOr<PlanAgentFileResult> ApplyPlanAgentUpdateToFile(
    const std::string& path, const PlanAgentUpdate& update, bool dry_run);

// The "agent_status" section, or nullptr when the plan has none.
const PlanJson* FindPlanAgentStatus(const ProjectPlan& plan);
// The stored observation for `key` ("step:D4"), or nullptr.
const PlanJson* FindPlanAgentObservation(const ProjectPlan& plan,
                                         std::string_view key);
// Observation keys whose step, item, decision or milestone no longer exists.
std::vector<std::string> OrphanedPlanAgentObservations(const ProjectPlan& plan);
// Moves an observation when its entry's ID is renamed.
void RenamePlanAgentObservation(ProjectPlan* plan, std::string_view before,
                                std::string_view after);

// A draft loaded from `loaded_source` can be saved over `disk_source` only
// when the two differ in nothing but agent status. Then the disk's agent
// status is copied into `draft` and this returns true; otherwise, including
// when either source cannot be parsed, `draft` is untouched and this returns
// false.
bool AdoptPlanAgentOnlyChanges(ProjectPlan* draft,
                               std::string_view loaded_source,
                               std::string_view disk_source);

}  // namespace yaze::core

#endif  // YAZE_CORE_PROJECT_PLAN_AGENT_H_
