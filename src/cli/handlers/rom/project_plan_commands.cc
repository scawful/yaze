#include "cli/handlers/rom/project_plan_commands.h"

#include <fstream>
#include <iterator>
#include <string>
#include <vector>

#include "absl/status/status.h"
#include "absl/strings/str_format.h"
#include "core/project.h"
#include "core/project_plan.h"
#include "core/project_plan_agent.h"
#include "core/source_artifact_publisher.h"
#include "util/macro.h"

namespace yaze::cli::handlers {
namespace {

absl::Status ValidatePlanSource(const resources::ArgumentParser& parser,
                                const char* command) {
  const auto plan = parser.GetString("plan");
  const auto project = parser.GetString("project");
  const bool has_plan = plan.has_value() && !plan->empty();
  const bool has_project = project.has_value() && !project->empty();
  if (has_plan == has_project) {
    return absl::InvalidArgumentError(absl::StrFormat(
        "%s: pass exactly one of --plan or --project", command));
  }
  return absl::OkStatus();
}

// The plan named by --plan, or by the --project file's [planning] section.
absl::StatusOr<std::string> ResolvePlanPath(
    const resources::ArgumentParser& parser) {
  if (auto plan = parser.GetString("plan");
      plan.has_value() && !plan->empty()) {
    return *plan;
  }
  project::YazeProject project;
  RETURN_IF_ERROR(project.Open(*parser.GetString("project")));
  const std::string path = project.PlanningFilePath();
  if (project.planning.file.empty() || path.empty()) {
    return absl::FailedPreconditionError(
        absl::StrFormat("Project %s has no [planning] file", project.filepath));
  }
  return path;
}

absl::StatusOr<std::string> ReadText(const std::string& path) {
  std::ifstream file(path, std::ios::binary);
  if (!file.is_open()) {
    return absl::NotFoundError(absl::StrFormat("Cannot read %s", path));
  }
  return std::string(std::istreambuf_iterator<char>(file), {});
}

template <typename Entry>
void AddIds(resources::OutputFormatter& formatter, const char* key,
            const std::vector<Entry>& entries) {
  formatter.BeginArray(key);
  for (const auto& entry : entries) {
    formatter.AddArrayItem(entry.id);
  }
  formatter.EndArray();
}

}  // namespace

resources::CommandHandler::Descriptor PlanReadCommandHandler::Describe() const {
  Descriptor desc;
  desc.display_name = "Plan Read";
  desc.summary =
      "Print a yaze.plan/1 file's SHA-256, entry IDs and agent status. Agents "
      "copy sha256 into an agent update's base_sha256.";
  desc.entries = {
      {"--plan", "Path to the yaze.plan/1 JSON file", ""},
      {"--project", "Path to a .yaze project; uses its [planning] file", ""},
  };
  return desc;
}

absl::Status PlanReadCommandHandler::ValidateArgs(
    const resources::ArgumentParser& parser) {
  return ValidatePlanSource(parser, "plan-read");
}

absl::Status PlanReadCommandHandler::Execute(
    Rom* /*rom*/, const resources::ArgumentParser& parser,
    resources::OutputFormatter& formatter) {
  ASSIGN_OR_RETURN(const std::string path, ResolvePlanPath(parser));
  std::string source;
  ASSIGN_OR_RETURN(const core::ProjectPlan plan,
                   core::ProjectPlan::LoadFromFile(path, &source));
  formatter.AddField("path", path);
  formatter.AddField("schema", std::string(core::kProjectPlanSchema));
  formatter.AddField("sha256", core::ComputeSourceArtifactSha256(source));
  formatter.AddField("title", plan.title);
  AddIds(formatter, "milestones", plan.milestones);
  AddIds(formatter, "steps", plan.route_steps);
  AddIds(formatter, "items", plan.route_items);
  AddIds(formatter, "decisions", plan.decisions);

  const core::PlanJson* section = core::FindPlanAgentStatus(plan);
  formatter.BeginObject("agent_status");
  formatter.AddField("present", section != nullptr);
  formatter.BeginArray("observations");
  if (section != nullptr) {
    if (auto it = section->find("observations");
        it != section->end() && it->is_object()) {
      for (auto obs = it->begin(); obs != it->end(); ++obs) {
        formatter.AddArrayItem(obs.key());
      }
    }
  }
  formatter.EndArray();
  formatter.BeginArray("orphaned");
  for (const auto& key : core::OrphanedPlanAgentObservations(plan)) {
    formatter.AddArrayItem(key);
  }
  formatter.EndArray();
  formatter.EndObject();
  return absl::OkStatus();
}

resources::CommandHandler::Descriptor PlanAgentApplyCommandHandler::Describe()
    const {
  Descriptor desc;
  desc.display_name = "Plan Agent Apply";
  desc.summary =
      "Check, diff and apply a yaze.plan.agent_update/1 document to a plan's "
      "agent_status section. Human notes, answers, titles, route order and "
      "unknown sections are never written. A stale base_sha256 is refused "
      "before writing; an unchanged update does not write.";
  desc.entries = {
      {"--plan", "Path to the yaze.plan/1 JSON file", ""},
      {"--project", "Path to a .yaze project; uses its [planning] file", ""},
      {"--update", "Path to the agent update JSON (required)", ""},
      {"--dry-run", "Validate and print the diff without writing", ""},
  };
  return desc;
}

absl::Status PlanAgentApplyCommandHandler::ValidateArgs(
    const resources::ArgumentParser& parser) {
  RETURN_IF_ERROR(ValidatePlanSource(parser, "plan-agent-apply"));
  const auto update = parser.GetString("update");
  if (!update.has_value() || update->empty()) {
    return absl::InvalidArgumentError("plan-agent-apply: --update is required");
  }
  return absl::OkStatus();
}

absl::Status PlanAgentApplyCommandHandler::Execute(
    Rom* /*rom*/, const resources::ArgumentParser& parser,
    resources::OutputFormatter& formatter) {
  ASSIGN_OR_RETURN(const std::string path, ResolvePlanPath(parser));
  ASSIGN_OR_RETURN(const std::string text,
                   ReadText(*parser.GetString("update")));
  ASSIGN_OR_RETURN(const core::PlanAgentUpdate update,
                   core::PlanAgentUpdate::Parse(text));
  const bool dry_run = parser.HasFlag("dry-run");
  ASSIGN_OR_RETURN(const core::PlanAgentFileResult result,
                   core::ApplyPlanAgentUpdateToFile(path, update, dry_run));
  formatter.AddField("path", path);
  formatter.AddField("dry_run", dry_run);
  formatter.AddField("before_sha256", result.before_sha256);
  formatter.AddField("after_sha256", result.after_sha256);
  formatter.AddField("changed", result.diff.HasChanges());
  formatter.AddField("written", result.written);
  formatter.AddField("sources_changed", result.diff.sources_changed);
  formatter.BeginArray("changes");
  for (const auto& change : result.diff.changes) {
    formatter.AddArrayItem(absl::StrFormat(
        "%s %s", core::PlanAgentChangeKindName(change.kind), change.key));
  }
  formatter.EndArray();
  return absl::OkStatus();
}

}  // namespace yaze::cli::handlers
