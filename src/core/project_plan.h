#ifndef YAZE_CORE_PROJECT_PLAN_H_
#define YAZE_CORE_PROJECT_PLAN_H_

#include <string>
#include <string_view>
#include <vector>

#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "nlohmann/json.hpp"

namespace yaze::project {
struct YazeProject;
}

namespace yaze::core {

// A project plan: goals, milestones, the intended play route, open design
// decisions and reference documents for a hack. Stored as a JSON sidecar
// (schema "yaze.plan/1") named by the `.yaze` file's [planning] section; see
// docs/public/usage/planning.md. Game-independent: nothing here reads a ROM.
//
// Every object keeps the keys this build does not model in `extra`, and the
// top level keeps unknown sections (for example "overworld" or "concepts"), so
// a plan written by a newer build or another tool survives a save.

using PlanJson = nlohmann::ordered_json;

inline constexpr std::string_view kProjectPlanSchema = "yaze.plan/1";

// A link from plan data to something the editor can show. Written as
// "<kind>:<id>", for example "room:0x0C", "map:0x4B", "area:D1", "step:D4",
// "item:flippers", "decision:power-approach", "marker:abyss:m3".
struct PlanTarget {
  std::string kind;
  std::string id;

  static PlanTarget Parse(std::string_view text);
  // Room or overworld map number for "room:" / "map:" targets, else -1.
  int NumericId() const;
};

struct PlanGoal {
  std::string title;
  std::string detail;
  std::string due;  // YYYY-MM-DD, optional
  PlanJson extra = PlanJson::object();
};

struct PlanMilestone {
  std::string id;
  std::string title;
  std::string start;   // YYYY-MM-DD, optional
  std::string end;     // YYYY-MM-DD, optional
  std::string status;  // planned | active | done
  std::string detail;
  PlanJson extra = PlanJson::object();
};

struct PlanRouteStep {
  std::string id;
  std::string code;  // short label such as "D4", optional
  std::string title;
  std::string note;
  std::string group;  // chapter or act heading, optional
  std::string kind;   // story | errand | dungeon | shrine | optional | boss
  std::vector<std::string> targets;
  PlanJson extra = PlanJson::object();
};

struct PlanNeed {
  std::string step;
  std::string what;
  PlanJson extra = PlanJson::object();
};

struct PlanRouteItem {
  std::string id;
  std::string name;
  std::string step;      // route step where the player gets it
  std::string category;  // gear | mask | song | quest | ...
  bool required = false;
  std::string status;  // implemented | prototype | decided | proposed | open
  std::string where;
  std::string note;
  std::vector<PlanNeed> needs;  // later steps that require it
  PlanJson extra = PlanJson::object();
};

struct PlanDecision {
  std::string id;
  std::string title;
  std::string status;  // open | decided | dropped
  int priority = 0;    // 1 = most urgent; 0 = unranked
  std::string recommendation;
  std::string answer;
  std::vector<std::string> targets;
  std::vector<std::string> refs;  // file paths or URLs
  PlanJson extra = PlanJson::object();
};

struct PlanReference {
  std::string label;
  std::string path;  // file path or URL
  std::string author;
  std::string date;
  PlanJson extra = PlanJson::object();
};

class ProjectPlan {
 public:
  static ProjectPlan CreateEmpty(std::string title);
  static absl::StatusOr<ProjectPlan> Parse(std::string_view text);
  static absl::StatusOr<ProjectPlan> LoadFromFile(
      const std::string& path, std::string* source = nullptr);
  // Creates a new sidecar and its project link. Native publication uses one
  // rollback-protected set; browser projects retain their IndexedDB writer.
  // An existing destination is never replaced, even when it cannot be parsed.
  static absl::Status CreateAndLink(project::YazeProject* project,
                                    const std::string& path);

  std::string Serialize() const;
  // Exclusively creates a new file. Existing destinations are never replaced.
  absl::Status SaveToFile(const std::string& path) const;
  // Publishes only if the file still contains the exact loaded source bytes.
  absl::Status SaveToFile(const std::string& path,
                          const std::string& expected_source) const;
  // Publishes `text`, which must parse as a valid plan, under the same lock
  // and exact-bytes check as SaveToFile.
  static absl::Status PublishSource(const std::string& path,
                                    const std::string& expected_source,
                                    const std::string& text);

  absl::Status ValidateIds() const;
  std::string NewMilestoneId() const;
  std::string NewStepId() const;
  std::string NewItemId() const;
  std::string NewDecisionId() const;
  // A failed rename leaves IDs and all references untouched.
  absl::Status RenameStep(std::string_view id, std::string new_id);
  absl::Status RenameItem(std::string_view id, std::string new_id);

  // Index into route_steps, or -1.
  int FindStep(std::string_view id) const;
  std::vector<const PlanRouteItem*> ItemsAtStep(std::string_view step) const;
  // Decisions with status "open", most urgent first (unranked last).
  std::vector<const PlanDecision*> OpenDecisions() const;
  // The milestone marked "active", else the first one whose dates contain
  // `today` (YYYY-MM-DD), else nullptr.
  const PlanMilestone* CurrentMilestone(std::string_view today) const;

  std::string title;
  std::string about;
  PlanGoal goal;
  std::vector<PlanMilestone> milestones;
  std::vector<PlanRouteStep> route_steps;
  std::vector<PlanRouteItem> route_items;
  PlanJson route_extra = PlanJson::object();
  std::vector<PlanDecision> decisions;
  std::vector<PlanReference> references;
  PlanJson extra = PlanJson::object();
};

}  // namespace yaze::core

#endif  // YAZE_CORE_PROJECT_PLAN_H_
