// yaze.plan/1 sidecar (core/project_plan.h) and the [planning] section of the
// .yaze project file.

#include "core/project_plan.h"
#include "core/source_artifact_publisher.h"

#include <filesystem>
#include <fstream>
#include <optional>
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

TEST(ProjectPlanTest, RejectsDuplicateIdsOnImportAndSave) {
  for (const char* section : {"milestones", "steps", "items", "decisions"}) {
    auto json = PlanJson::parse(kPlan);
    auto& entries =
        std::string(section) == "steps" || std::string(section) == "items"
            ? json["route"][section]
            : json[section];
    entries.push_back(entries.front());
    EXPECT_FALSE(ProjectPlan::Parse(json.dump()).ok()) << section;
  }
  auto plan = ProjectPlan::Parse(kPlan);
  ASSERT_TRUE(plan.ok());
  plan->route_steps.push_back(plan->route_steps.front());
  ScopedDir dir(yaze::test::UniqueTempPath("duplicate_plan"));
  const auto path = dir.path() / "plan.json";
  EXPECT_FALSE(plan->SaveToFile(path.string()).ok());
  EXPECT_FALSE(fs::exists(path));
}

TEST(ProjectPlanTest, DeletedIdsDoNotCollideOrRetargetReferences) {
  auto plan = ProjectPlan::CreateEmpty("IDs");
  for (int index = 0; index < 3; ++index) {
    PlanRouteStep step;
    step.id = plan.NewStepId();
    plan.route_steps.push_back(std::move(step));
  }
  PlanRouteItem item;
  item.id = "item-1";
  item.step = "step-2";
  item.needs.push_back({.step = "step-3"});
  plan.route_items.push_back(item);
  plan.route_steps.erase(plan.route_steps.begin() + 1);
  EXPECT_EQ(plan.NewStepId(), "step-4");
  EXPECT_EQ(plan.route_items.front().step, "step-2");
  plan.route_steps.pop_back();
  EXPECT_EQ(plan.NewStepId(), "step-4");
  EXPECT_EQ(plan.route_items.front().needs.front().step, "step-3");
  plan.route_steps.front().targets.push_back("item:item-2");
  EXPECT_EQ(plan.NewItemId(), "item-3");
  plan.route_steps.front().targets.push_back("decision:decision-1");
  EXPECT_EQ(plan.NewDecisionId(), "decision-2");
}

TEST(ProjectPlanTest, RenameValidatesBeforeChangingLinkedReferences) {
  auto plan = ProjectPlan::Parse(kPlan);
  ASSERT_TRUE(plan.ok());
  plan->route_steps[1].targets = {"step:D1", "item:bow"};
  plan->decisions[0].targets = {"step:D1", "item:bow"};
  const std::string before = plan->Serialize();
  EXPECT_FALSE(plan->RenameStep("D1", "S1").ok());
  EXPECT_EQ(plan->Serialize(), before);
  ASSERT_TRUE(plan->RenameStep("D1", "intro").ok());
  EXPECT_EQ(plan->route_items.front().step, "intro");
  EXPECT_EQ(plan->route_items.front().needs.front().step, "S1");
  EXPECT_EQ(plan->route_steps[1].targets.front(), "step:intro");
  EXPECT_EQ(plan->decisions[0].targets.front(), "step:intro");
  ASSERT_TRUE(plan->RenameStep("S1", "wisdom").ok());
  EXPECT_EQ(plan->route_items.front().needs.front().step, "wisdom");
  ASSERT_TRUE(plan->RenameItem("bow", "shortbow").ok());
  EXPECT_EQ(plan->route_steps[1].targets[1], "item:shortbow");
  EXPECT_EQ(plan->decisions[0].targets[1], "item:shortbow");
}

TEST(ProjectPlanTest, CheckedSavePreservesExternalChanges) {
  ScopedDir dir(yaze::test::UniqueTempPath("checked_plan"));
  const auto path = dir.path() / "plan.json";
  auto plan = ProjectPlan::CreateEmpty("Original");
  ASSERT_TRUE(plan.SaveToFile(path.string()).ok());
  std::string source;
  auto loaded = ProjectPlan::LoadFromFile(path.string(), &source);
  ASSERT_TRUE(loaded.ok());
  loaded->goal.title = "Draft";
  WriteFile(path, "external recovery bytes");
  EXPECT_FALSE(loaded->SaveToFile(path.string(), source).ok());
  EXPECT_EQ(ReadFile(path), "external recovery bytes");
  EXPECT_EQ(loaded->goal.title, "Draft");
}

TEST(ProjectPlanTest, CreateAndLinkRefusesExistingValidAndCorruptDestinations) {
  ScopedDir dir(yaze::test::UniqueTempPath("exclusive_plan"));
  const auto descriptor = dir.path() / "Hack.yaze";
  const auto path = dir.path() / "Hack.plan.json";
  const std::string project_source = "[project]\nname=Hack\n";
  WriteFile(descriptor, project_source);
  project::YazeProject project;
  ASSERT_TRUE(project.Open(descriptor.string()).ok());
  for (const auto& original :
       {std::string("broken but recoverable"),
        ProjectPlan::CreateEmpty("Existing").Serialize()}) {
    WriteFile(path, original);
    EXPECT_FALSE(ProjectPlan::CreateAndLink(&project, path.string()).ok());
    EXPECT_EQ(ReadFile(path), original);
    EXPECT_EQ(ReadFile(descriptor), project_source);
    EXPECT_TRUE(project.planning.file.empty());
  }
}

TEST(ProjectPlanTest, CreateAndLinkPublishesBothFiles) {
  ScopedDir dir(yaze::test::UniqueTempPath("linked_plan"));
  const auto descriptor = dir.path() / "Hack.yaze";
  const auto path = dir.path() / "Hack.plan.json";
  WriteFile(descriptor, "[project]\nname=Hack\n");
  project::YazeProject project;
  ASSERT_TRUE(project.Open(descriptor.string()).ok());
  ASSERT_TRUE(ProjectPlan::CreateAndLink(&project, path.string()).ok());
  auto plan = ProjectPlan::LoadFromFile(path.string());
  ASSERT_TRUE(plan.ok());
  EXPECT_EQ(plan->title, "Hack");
  project::YazeProject reopened;
  ASSERT_TRUE(reopened.Open(descriptor.string()).ok());
  EXPECT_EQ(reopened.PlanningFilePath(), path.string());
  EXPECT_EQ(reopened.planning, project.planning);
}

TEST(ProjectPlanTest, DescriptorFailurePreservesOriginalPlanAndProjectLink) {
  ScopedDir dir(yaze::test::UniqueTempPath("failed_plan_link"));
  const auto descriptor = dir.path() / "Hack.yaze";
  const auto recovery = dir.path() / "original.plan.json";
  const auto path = dir.path() / "new.plan.json";
  WriteFile(recovery, "recoverable original");
  project::YazeProject project;
  project.filepath = descriptor.string();
  project.planning.file = recovery.filename().string();
  fs::create_directory(descriptor);
  EXPECT_FALSE(ProjectPlan::CreateAndLink(&project, path.string()).ok());
  EXPECT_FALSE(fs::exists(path));
  EXPECT_EQ(ReadFile(recovery), "recoverable original");
  EXPECT_EQ(project.planning.file, recovery.filename().string());
}

TEST(ProjectPlanTest, PublicationFailureRollsBackDescriptorAndNewSidecar) {
  ScopedDir dir(yaze::test::UniqueTempPath("plan_publication_rollback"));
  const auto descriptor = dir.path() / "Hack.yaze";
  const auto path = dir.path() / "new.plan.json";
  const auto recovery = dir.path() / "original.plan.json";
  const std::string before =
      "[project]\nname=Hack\n[planning]\nfile=original.plan.json\n";
  WriteFile(descriptor, before);
  WriteFile(recovery, "original recovery bytes");
  auto lock = AcquireSourceArtifactPublicationLock({descriptor, path});
  ASSERT_TRUE(lock.ok()) << lock.status();
  const auto status = PublishSourceArtifacts(
      **lock,
      {{descriptor, before,
        "[project]\nname=Hack\n[planning]\nfile=new.plan.json\n"},
       {path, std::nullopt, ProjectPlan::CreateEmpty("New").Serialize()}},
      ComputeSourceArtifactSha256(before),
      [] { return absl::InternalError("descriptor readback failure"); });
  EXPECT_FALSE(status.ok());
  EXPECT_EQ(ReadFile(descriptor), before);
  EXPECT_FALSE(fs::exists(path));
  EXPECT_EQ(ReadFile(recovery), "original recovery bytes");
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
