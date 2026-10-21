#ifndef YAZE_CORE_PROJECT_PLAN_H_
#define YAZE_CORE_PROJECT_PLAN_H_

#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "nlohmann/json.hpp"

namespace yaze::core {

// A project plan: goals, milestones, the intended play route, open design
// decisions and reference documents for a hack. Stored as a JSON sidecar
// (schema "yaze.plan/1") named by the `.yaze` file's [planning] section; see
// docs/public/usage/planning.md. Game-independent: nothing here reads a ROM.
//
// Every object keeps the keys this build does not model in `extra`, and the
// top level keeps unknown sections (for example "concepts"), so
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

// Overworld layer. Positions are world pixels: an overworld area is 512x512
// and a world is 8 areas wide, so a 64-area world spans 4096x4096.
inline constexpr int kPlanAreaSize = 512;
inline constexpr int kPlanWorldColumns = 8;

struct PlanWorld {
  std::string id;
  std::string title;
  std::string base;  // first map number, for example "0x40"
  int maps = 64;
  PlanJson extra = PlanJson::object();

  int BaseMap() const;
  int Rows() const;
  // Map number under a world-pixel position, or -1 outside the world.
  int MapAt(int x, int y) const;
  // The "map" to store after moving an entry from (old_x, old_y) to
  // (new_x, new_y). Within one 512x512 area the current value is kept, since
  // it may name the parent of a large area; otherwise the new area's number.
  std::string MapAfterMove(const std::string& current, int old_x, int old_y,
                           int new_x, int new_y) const;
};

// "0x1E" style map and room numbers, as written in plans.
std::string FormatPlanId(int id);

struct PlanMarker {
  std::string id;
  std::string world;
  int x = 0;
  int y = 0;
  std::string map;   // "0x10"; kept in step with x/y by the editor
  std::string kind;  // dungeon | shrine | place | item | planned
  std::string label;
  std::string detail;
  std::string status;  // implemented | decided | draft | proposed | idea
  std::string code;    // short label such as "D1", the id of area: targets
  std::string room;    // dungeon entrance room, for example "0x4A"
  PlanJson extra = PlanJson::object();
};

struct PlanNote {
  std::string id;
  std::string world;
  int x = 0;
  int y = 0;
  std::string map;
  std::string text;
  std::string created;
  PlanJson extra = PlanJson::object();
};

struct PlanLine {
  std::string id;
  std::string world;
  std::string color;
  std::vector<std::pair<int, int>> points;
  PlanJson extra = PlanJson::object();
};

struct PlanRegion {
  std::string id;
  std::string world;
  int x = 0;
  int y = 0;
  int radius = 0;
  std::string label;
  std::string detail;
  std::string status;
  PlanJson extra = PlanJson::object();
};

struct PlanOverworld {
  std::vector<PlanWorld> worlds;
  std::vector<PlanMarker> markers;
  std::vector<PlanNote> notes;
  std::vector<PlanLine> lines;
  std::vector<PlanRegion> regions;
  PlanJson extra = PlanJson::object();

  bool empty() const;
  const PlanWorld* FindWorld(std::string_view id) const;
  // Index into markers, or -1.
  int FindMarker(std::string_view id) const;
  int FindMarkerByCode(std::string_view code) const;
};

class ProjectPlan {
 public:
  static ProjectPlan CreateEmpty(std::string title);
  static absl::StatusOr<ProjectPlan> Parse(std::string_view text);
  static absl::StatusOr<ProjectPlan> LoadFromFile(const std::string& path);

  std::string Serialize() const;
  // Writes a sibling temporary file, then renames it over `path`.
  absl::Status SaveToFile(const std::string& path) const;

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
  PlanOverworld overworld;
  PlanJson extra = PlanJson::object();
};

}  // namespace yaze::core

#endif  // YAZE_CORE_PROJECT_PLAN_H_
