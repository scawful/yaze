// yaze.plan/1 sidecar (core/project_plan.h) and the [planning] section of the
// .yaze project file.

#include "core/project_plan.h"

#include <filesystem>
#include <fstream>
#include <sstream>
#include <string>

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include "core/project.h"
#include "unique_temp_path.h"

namespace yaze::core {
namespace {

namespace fs = std::filesystem;
using ::testing::ElementsAre;
using ::testing::HasSubstr;
using ::testing::Not;

class ScopedDir {
 public:
  explicit ScopedDir(fs::path path) : path_(std::move(path)) {
    fs::create_directories(path_);
  }
  ~ScopedDir() {
    std::error_code ec;
    fs::remove_all(path_, ec);
  }
  const fs::path& path() const { return path_; }

 private:
  fs::path path_;
};

std::string ReadFile(const fs::path& path) {
  std::ifstream file(path, std::ios::binary);
  std::stringstream buffer;
  buffer << file.rdbuf();
  return buffer.str();
}

void WriteFile(const fs::path& path, const std::string& content) {
  std::ofstream(path, std::ios::binary | std::ios::trunc) << content;
}

constexpr char kPlan[] = R"({
  "schema": "yaze.plan/1",
  "title": "Test hack",
  "goal": {"title": "Demo", "due": "2026-12-25", "owner": "kept"},
  "milestones": [
    {"id": "w1", "title": "Window 1", "start": "2026-10-01",
     "end": "2026-10-31", "status": "planned"},
    {"id": "w2", "title": "Window 2", "start": "2026-11-01",
     "end": "2026-11-30", "status": "planned"}
  ],
  "route": {
    "steps": [
      {"id": "D1", "code": "D1", "title": "Mushroom Grotto", "kind": "dungeon",
       "targets": ["area:D1", "room:0x0C"], "size": "medium"},
      {"id": "S1", "title": "Shrine of Wisdom", "kind": "shrine"}
    ],
    "items": [
      {"id": "bow", "name": "Bow", "step": "D1", "category": "gear",
       "required": true, "status": "implemented",
       "needs": [{"step": "S1", "what": "targets", "hint": "kept"}],
       "change": "kept"}
    ],
    "legend": {"kept": true}
  },
  "decisions": [
    {"id": "late", "title": "Later call", "status": "open"},
    {"id": "done", "title": "Settled", "status": "decided", "priority": 1},
    {"id": "first", "title": "Most urgent", "status": "open", "priority": 1,
     "recommendation": "Do it", "targets": ["map:0x4B"]}
  ],
  "references": [{"label": "Notes", "path": "notes.md", "author": "me"}],
  "overworld": {"markers": [{"id": "m1", "world": "light", "x": 10, "y": 20}]},
  "concepts": [{"id": "fortress", "options": ["a", "b"]}]
})";

TEST(ProjectPlanTest, ParsesKnownFields) {
  auto plan = ProjectPlan::Parse(kPlan);
  ASSERT_TRUE(plan.ok()) << plan.status();
  EXPECT_EQ(plan->title, "Test hack");
  EXPECT_EQ(plan->goal.due, "2026-12-25");
  ASSERT_EQ(plan->milestones.size(), 2u);
  ASSERT_EQ(plan->route_steps.size(), 2u);
  EXPECT_THAT(plan->route_steps[0].targets,
              ElementsAre("area:D1", "room:0x0C"));
  ASSERT_EQ(plan->route_items.size(), 1u);
  EXPECT_TRUE(plan->route_items[0].required);
  ASSERT_EQ(plan->route_items[0].needs.size(), 1u);
  EXPECT_EQ(plan->route_items[0].needs[0].step, "S1");
  EXPECT_EQ(plan->FindStep("S1"), 1);
  EXPECT_EQ(plan->FindStep("missing"), -1);
  EXPECT_EQ(plan->ItemsAtStep("D1").size(), 1u);
}

TEST(ProjectPlanTest, UnknownKeysSurviveRoundTrip) {
  auto plan = ProjectPlan::Parse(kPlan);
  ASSERT_TRUE(plan.ok()) << plan.status();
  const std::string saved = plan->Serialize();
  auto reparsed = ProjectPlan::Parse(saved);
  ASSERT_TRUE(reparsed.ok()) << reparsed.status();
  EXPECT_EQ(reparsed->Serialize(), saved);

  const auto json = PlanJson::parse(saved);
  EXPECT_EQ(json["goal"]["owner"], "kept");
  EXPECT_EQ(json["route"]["steps"][0]["size"], "medium");
  EXPECT_EQ(json["route"]["items"][0]["change"], "kept");
  EXPECT_EQ(json["route"]["items"][0]["needs"][0]["hint"], "kept");
  EXPECT_EQ(json["route"]["legend"]["kept"], true);
  EXPECT_EQ(json["overworld"]["markers"][0]["x"], 10);
  EXPECT_EQ(json["concepts"][0]["options"][1], "b");
}

TEST(ProjectPlanTest, EditsAreWritten) {
  auto plan = ProjectPlan::Parse(kPlan);
  ASSERT_TRUE(plan.ok()) << plan.status();
  plan->decisions[0].status = "decided";
  plan->decisions[0].answer = "Yes";
  std::swap(plan->route_steps[0], plan->route_steps[1]);
  auto reparsed = ProjectPlan::Parse(plan->Serialize());
  ASSERT_TRUE(reparsed.ok()) << reparsed.status();
  EXPECT_EQ(reparsed->decisions[0].answer, "Yes");
  EXPECT_EQ(reparsed->route_steps[0].id, "S1");
  EXPECT_EQ(reparsed->route_steps[1].targets.size(), 2u);
}

TEST(ProjectPlanTest, OpenDecisionsByPriority) {
  auto plan = ProjectPlan::Parse(kPlan);
  ASSERT_TRUE(plan.ok()) << plan.status();
  const auto open = plan->OpenDecisions();
  ASSERT_EQ(open.size(), 2u);
  EXPECT_EQ(open[0]->id, "first");
  EXPECT_EQ(open[1]->id, "late");
}

TEST(ProjectPlanTest, CurrentMilestone) {
  auto plan = ProjectPlan::Parse(kPlan);
  ASSERT_TRUE(plan.ok()) << plan.status();
  ASSERT_NE(plan->CurrentMilestone("2026-11-10"), nullptr);
  EXPECT_EQ(plan->CurrentMilestone("2026-11-10")->id, "w2");
  EXPECT_EQ(plan->CurrentMilestone("2027-01-01"), nullptr);
  plan->milestones[0].status = "active";
  EXPECT_EQ(plan->CurrentMilestone("2026-11-10")->id, "w1");
}

TEST(ProjectPlanTest, RejectsBadInput) {
  EXPECT_FALSE(ProjectPlan::Parse("not json").ok());
  EXPECT_FALSE(ProjectPlan::Parse("[]").ok());
  EXPECT_FALSE(ProjectPlan::Parse(R"({"title": "x"})").ok());
  EXPECT_FALSE(ProjectPlan::Parse(R"({"schema": "yaze.plan/2"})").ok());
  auto bad_type = ProjectPlan::Parse(
      R"({"schema": "yaze.plan/1", "route": {"steps": [{"title": 3}]}})");
  ASSERT_FALSE(bad_type.ok());
  EXPECT_THAT(std::string(bad_type.status().message()),
              HasSubstr("route.steps[0]"));
}

TEST(ProjectPlanTest, EmptyPlanRoundTrips) {
  const ProjectPlan plan = ProjectPlan::CreateEmpty("Vanilla");
  auto reparsed = ProjectPlan::Parse(plan.Serialize());
  ASSERT_TRUE(reparsed.ok()) << reparsed.status();
  EXPECT_EQ(reparsed->title, "Vanilla");
  EXPECT_TRUE(reparsed->route_steps.empty());
}

TEST(ProjectPlanTest, SaveAndLoadFile) {
  ScopedDir dir(yaze::test::UniqueTempPath("project_plan_file"));
  const fs::path path = dir.path() / "hack.plan.json";
  auto plan = ProjectPlan::Parse(kPlan);
  ASSERT_TRUE(plan.ok()) << plan.status();
  ASSERT_TRUE(plan->SaveToFile(path.string()).ok());
  EXPECT_FALSE(fs::exists(path.string() + ".tmp"));
  auto loaded = ProjectPlan::LoadFromFile(path.string());
  ASSERT_TRUE(loaded.ok()) << loaded.status();
  EXPECT_EQ(loaded->Serialize(), plan->Serialize());
  EXPECT_FALSE(
      ProjectPlan::LoadFromFile((dir.path() / "missing.json").string()).ok());
}

constexpr char kOverworldPlan[] = R"({
  "schema": "yaze.plan/1",
  "overworld": {
    "worlds": [
      {"id": "light", "title": "Light World", "base": "0x00", "maps": 64},
      {"id": "dark", "title": "Dark World", "base": "0x40", "maps": 64,
       "tint": "purple"}
    ],
    "markers": [
      {"id": "light:m0", "world": "light", "x": 112, "y": 1040, "map": "0x10",
       "kind": "dungeon", "label": "Grotto", "status": "implemented",
       "code": "D1", "room": "0x4A", "approx": true, "icon": "lamp"},
      {"id": "dark:m0", "world": "dark", "x": 3904.4, "y": 2640.6,
       "kind": "place", "label": "Fort"}
    ],
    "notes": [{"id": "n1", "world": "light", "x": 5, "y": 6, "map": "0x00",
               "text": "idea", "created": "2026-10-20T00:36:14Z"}],
    "lines": [{"id": "l1", "world": "light", "color": "red",
               "points": [[1, 2], [3, 4]], "width": 2}],
    "regions": [{"id": "r1", "world": "dark", "x": 512, "y": 512,
                 "radius": 256, "label": "Village", "status": "proposed"}],
    "area_plans": [],
    "unplaced": {"dark": ["Wolf Mask"]}
  }
})";

TEST(ProjectPlanOverworldTest, ParsesLayers) {
  auto plan = ProjectPlan::Parse(kOverworldPlan);
  ASSERT_TRUE(plan.ok()) << plan.status();
  const PlanOverworld& ow = plan->overworld;
  ASSERT_EQ(ow.worlds.size(), 2u);
  ASSERT_EQ(ow.markers.size(), 2u);
  EXPECT_EQ(ow.markers[0].code, "D1");
  EXPECT_EQ(ow.markers[0].room, "0x4A");
  EXPECT_EQ(ow.markers[1].x, 3904);
  EXPECT_EQ(ow.markers[1].y, 2641);
  ASSERT_EQ(ow.lines.size(), 1u);
  EXPECT_THAT(ow.lines[0].points,
              ElementsAre(std::pair(1, 2), std::pair(3, 4)));
  ASSERT_EQ(ow.regions.size(), 1u);
  EXPECT_EQ(ow.regions[0].radius, 256);
  EXPECT_EQ(ow.notes[0].text, "idea");
  EXPECT_EQ(ow.FindMarker("dark:m0"), 1);
  EXPECT_EQ(ow.FindMarkerByCode("D1"), 0);
  EXPECT_EQ(ow.FindMarkerByCode(""), -1);
  ASSERT_NE(ow.FindWorld("dark"), nullptr);
  EXPECT_EQ(ow.FindWorld("dark")->BaseMap(), 0x40);
  EXPECT_EQ(ow.FindWorld("missing"), nullptr);
}

TEST(ProjectPlanOverworldTest, UnmodeledKeysSurviveRoundTrip) {
  auto plan = ProjectPlan::Parse(kOverworldPlan);
  ASSERT_TRUE(plan.ok()) << plan.status();
  plan->overworld.markers[0].x = 600;
  const std::string saved = plan->Serialize();
  auto reparsed = ProjectPlan::Parse(saved);
  ASSERT_TRUE(reparsed.ok()) << reparsed.status();
  EXPECT_EQ(reparsed->Serialize(), saved);

  const auto json = PlanJson::parse(saved);
  const auto& ow = json["overworld"];
  EXPECT_EQ(ow["markers"][0]["x"], 600);
  EXPECT_EQ(ow["markers"][0]["approx"], true);
  EXPECT_EQ(ow["markers"][0]["icon"], "lamp");
  EXPECT_EQ(ow["worlds"][1]["tint"], "purple");
  EXPECT_EQ(ow["lines"][0]["width"], 2);
  EXPECT_EQ(ow["lines"][0]["points"][1][0], 3);
  EXPECT_EQ(ow["unplaced"]["dark"][0], "Wolf Mask");
  EXPECT_TRUE(ow["area_plans"].is_array());
}

TEST(ProjectPlanOverworldTest, AbsentSectionIsNotWritten) {
  auto plan = ProjectPlan::Parse(R"({"schema": "yaze.plan/1"})");
  ASSERT_TRUE(plan.ok()) << plan.status();
  EXPECT_TRUE(plan->overworld.empty());
  EXPECT_FALSE(PlanJson::parse(plan->Serialize()).contains("overworld"));
}

TEST(ProjectPlanOverworldTest, RejectsBadLayers) {
  auto bad_points = ProjectPlan::Parse(
      R"({"schema": "yaze.plan/1",
          "overworld": {"lines": [{"points": [[1, 2, 3]]}]}})");
  ASSERT_FALSE(bad_points.ok());
  EXPECT_THAT(std::string(bad_points.status().message()),
              HasSubstr("overworld.lines[0]"));
  EXPECT_FALSE(ProjectPlan::Parse(R"({"schema": "yaze.plan/1",
          "overworld": {"markers": [{"x": "left"}]}})")
                   .ok());
  EXPECT_FALSE(
      ProjectPlan::Parse(R"({"schema": "yaze.plan/1", "overworld": []})").ok());
}

TEST(PlanWorldTest, MapAtUsesEightWideGrid) {
  PlanWorld world;
  world.base = "0x40";
  world.maps = 64;
  EXPECT_EQ(world.MapAt(0, 0), 0x40);
  EXPECT_EQ(world.MapAt(112, 1040), 0x50);
  EXPECT_EQ(world.MapAt(3904, 2640), 0x40 + 0x2F);
  EXPECT_EQ(world.MapAt(4095, 4095), 0x7F);
  EXPECT_EQ(world.MapAt(4096, 0), -1);
  EXPECT_EQ(world.MapAt(0, 4096), -1);
  EXPECT_EQ(world.MapAt(-1, 0), -1);
  EXPECT_EQ(world.Rows(), 8);

  // 0x1E is the parent of a large area; moving inside one 512x512 cell keeps
  // it, moving to another cell takes that cell's number.
  PlanWorld light;
  EXPECT_EQ(light.MapAfterMove("0x1E", 3800, 1600, 3900, 1700), "0x1E");
  EXPECT_EQ(light.MapAfterMove("0x1E", 3800, 1600, 3800, 2100), "0x27");
  EXPECT_EQ(light.MapAfterMove("", 10, 10, 20, 20), "0x00");
  EXPECT_EQ(light.MapAfterMove("0x00", 10, 10, 5000, 20), "0x00");
  EXPECT_EQ(FormatPlanId(0x4A), "0x4A");
  EXPECT_EQ(FormatPlanId(5), "0x05");

  PlanWorld small;
  small.maps = 12;
  EXPECT_EQ(small.Rows(), 2);
  EXPECT_EQ(small.MapAt(3 * kPlanAreaSize, kPlanAreaSize), 11);
  EXPECT_EQ(small.MapAt(4 * kPlanAreaSize, kPlanAreaSize), -1);
}

TEST(PlanTargetTest, ParsesKindsAndNumbers) {
  EXPECT_EQ(PlanTarget::Parse("room:0x0C").NumericId(), 0x0C);
  EXPECT_EQ(PlanTarget::Parse("map:75").NumericId(), 75);
  EXPECT_EQ(PlanTarget::Parse("area:D1").NumericId(), -1);
  EXPECT_EQ(PlanTarget::Parse("room:zz").NumericId(), -1);
  const PlanTarget marker = PlanTarget::Parse("marker:abyss:m3");
  EXPECT_EQ(marker.kind, "marker");
  EXPECT_EQ(marker.id, "abyss:m3");
}

TEST(ProjectPlanningSettingsTest, SectionRoundTrips) {
  ScopedDir dir(yaze::test::UniqueTempPath("project_planning_section"));
  const fs::path source = dir.path() / "Hack.yaze";
  WriteFile(source,
            "[project]\nname=Hack\n\n[planning]\n"
            "# Shared with the notes repo.\n"
            "file=../notes/hack.plan.json\n"
            "future_planning_key=keep\n");
  project::YazeProject project;
  ASSERT_TRUE(project.Open(source.string()).ok());
  EXPECT_TRUE(project.planning.section_present);
  EXPECT_EQ(project.planning.file, "../notes/hack.plan.json");
  EXPECT_EQ(fs::path(project.PlanningFilePath()),
            fs::path(project.GetAbsolutePath("../notes/hack.plan.json")));
  ASSERT_TRUE(project.Save().ok());
  EXPECT_THAT(ReadFile(source),
              HasSubstr("[planning]\n# Shared with the notes repo.\n"
                        "file=../notes/hack.plan.json\n"
                        "future_planning_key=keep\n"));

  project::YazeProject reopened;
  ASSERT_TRUE(reopened.Open(source.string()).ok());
  EXPECT_EQ(reopened.planning, project.planning);
}

TEST(ProjectPlanningSettingsTest, AbsolutePathAndAbsentSection) {
  ScopedDir dir(yaze::test::UniqueTempPath("project_planning_absent"));
  const fs::path source = dir.path() / "Hack.yaze";
  WriteFile(source, "[project]\nname=Hack\n");
  project::YazeProject project;
  ASSERT_TRUE(project.Open(source.string()).ok());
  EXPECT_FALSE(project.planning.section_present);
  EXPECT_TRUE(project.PlanningFilePath().empty());
  ASSERT_TRUE(project.Save().ok());
  EXPECT_THAT(ReadFile(source), Not(HasSubstr("[planning]")));

  const fs::path absolute = dir.path() / "elsewhere" / "plan.json";
  project.planning.file = absolute.string();
  ASSERT_TRUE(project.Save().ok());
  project::YazeProject reopened;
  ASSERT_TRUE(reopened.Open(source.string()).ok());
  EXPECT_EQ(fs::path(reopened.PlanningFilePath()), absolute.lexically_normal());
}

}  // namespace
}  // namespace yaze::core
