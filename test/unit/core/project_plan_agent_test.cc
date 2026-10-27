// Agent observations in a yaze.plan/1 file (core/project_plan_agent.h).

#include "core/project_plan_agent.h"

#include <filesystem>
#include <fstream>
#include <iterator>
#include <string>
#include <utility>

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include "core/project_plan.h"
#include "core/source_artifact_publisher.h"
#include "unique_temp_path.h"

namespace yaze::core {
namespace {

namespace fs = std::filesystem;
using ::testing::HasSubstr;

constexpr char kPlan[] = R"({
 "schema": "yaze.plan/1",
 "title": "Hack",
 "goal": {"title": "Ship it", "owner_note": "keep"},
 "milestones": [{"id": "w1", "title": "Week 1", "status": "active"}],
 "route": {
  "steps": [
   {"id": "D4", "code": "D4", "title": "Zora Temple", "note": "Human note",
    "kind": "dungeon", "map_marker": "zora"},
   {"id": "D6", "code": "D6", "title": "Goron Mines", "note": "Mine note"}
  ],
  "items": [
   {"id": "flippers", "name": "Flippers", "step": "D4", "required": true,
    "status": "decided", "note": "Item note"}
  ],
  "lanes": ["unknown route section"]
 },
 "decisions": [
  {"id": "d4-rescue", "priority": 1, "status": "open", "title": "Rescue",
   "answer": "Approved answer text"}
 ],
 "overworld": {"markers": [{"id": "m1", "x": 3}], "notes": ["spatial"]},
 "concepts": [{"id": "c1"}]
})";

std::string Canonical() {
  auto plan = ProjectPlan::Parse(kPlan);
  EXPECT_TRUE(plan.ok()) << plan.status();
  return plan->Serialize();
}

PlanAgentUpdate Update(std::string json) {
  auto update = PlanAgentUpdate::Parse(json);
  EXPECT_TRUE(update.ok()) << update.status();
  return *update;
}

constexpr char kZoraUpdate[] = R"({
 "schema": "yaze.plan.agent_update/1",
 "agent": "agent-a",
 "observed_utc": "2026-10-27T21:00:00Z",
 "sources": {"oracle_master": "7bd8ca90"},
 "observations": {
  "step:D4": {"state": "DRAFT_CANDIDATE", "next": "Full route open",
              "prs": [262], "evidence": ["native pass 5/8"],
              "agent_extra": {"kept": true}},
  "item:flippers": {"state": "IMPLEMENTED"}
 }
})";

// Plan with agent_status removed, serialized: what a person owns.
std::string HumanPart(ProjectPlan plan) {
  plan.extra.erase(std::string(kPlanAgentStatusKey));
  return plan.Serialize();
}

std::string Read(const fs::path& path) {
  std::ifstream file(path, std::ios::binary);
  return std::string(std::istreambuf_iterator<char>(file), {});
}

void Write(const fs::path& path, const std::string& text) {
  std::ofstream(path, std::ios::binary | std::ios::trunc) << text;
}

class PlanFile : public ::testing::Test {
 protected:
  void SetUp() override {
    dir_ = test::UniqueTempPath("plan_agent");
    fs::create_directories(dir_);
    path_ = dir_ / "hack.plan.json";
    Write(path_, Canonical());
  }
  void TearDown() override {
    std::error_code ec;
    fs::remove_all(dir_, ec);
  }
  PlanAgentUpdate UpdateAtCurrentBase(std::string json) {
    PlanAgentUpdate update = Update(std::move(json));
    update.base_sha256 = ComputeSourceArtifactSha256(Read(path_));
    return update;
  }
  fs::path dir_;
  fs::path path_;
};

TEST(PlanAgentUpdateTest, ParseRequiresSchemaAndKnownFields) {
  EXPECT_FALSE(PlanAgentUpdate::Parse("[]").ok());
  EXPECT_FALSE(PlanAgentUpdate::Parse(R"({"observations": {}})").ok());
  auto typo = PlanAgentUpdate::Parse(
      R"({"schema": "yaze.plan.agent_update/1", "observation": {}})");
  ASSERT_FALSE(typo.ok());
  EXPECT_THAT(typo.status().message(), HasSubstr("observation"));
  EXPECT_FALSE(PlanAgentUpdate::Parse(
                   R"({"schema": "yaze.plan.agent_update/1",
                       "base_sha256": "abc"})")
                   .ok());
  auto upper = PlanAgentUpdate::Parse(
      R"({"schema": "yaze.plan.agent_update/1", "base_sha256": ")" +
      std::string(64, 'A') + R"("})");
  ASSERT_TRUE(upper.ok()) << upper.status();
  EXPECT_EQ(upper->base_sha256, std::string(64, 'a'));
}

TEST(PlanAgentTest, ApplyWritesOnlyTheAgentSection) {
  auto plan = ProjectPlan::Parse(Canonical());
  ASSERT_TRUE(plan.ok());
  const std::string human = HumanPart(*plan);
  auto diff = ApplyPlanAgentUpdate(&*plan, Update(kZoraUpdate));
  ASSERT_TRUE(diff.ok()) << diff.status();
  EXPECT_TRUE(diff->HasChanges());
  EXPECT_TRUE(diff->sources_changed);
  EXPECT_EQ(HumanPart(*plan), human);

  const PlanJson* obs = FindPlanAgentObservation(*plan, "step:D4");
  ASSERT_NE(obs, nullptr);
  EXPECT_EQ(obs->at("state"), "DRAFT_CANDIDATE");
  EXPECT_EQ(obs->at("agent"), "agent-a");
  EXPECT_EQ(obs->at("observed_utc"), "2026-10-27T21:00:00Z");
  EXPECT_TRUE(obs->at("agent_extra").at("kept").get<bool>());
  const PlanJson* section = FindPlanAgentStatus(*plan);
  ASSERT_NE(section, nullptr);
  EXPECT_EQ(section->at("schema"), std::string(kPlanAgentStatusSchema));
  EXPECT_EQ(section->at("sources").at("oracle_master"), "7bd8ca90");

  // The written file keeps human notes, answers, unknown keys and sections.
  auto reread = ProjectPlan::Parse(plan->Serialize());
  ASSERT_TRUE(reread.ok()) << reread.status();
  EXPECT_EQ(reread->route_steps[0].note, "Human note");
  EXPECT_EQ(reread->route_steps[0].extra.at("map_marker"), "zora");
  EXPECT_EQ(reread->decisions[0].answer, "Approved answer text");
  EXPECT_EQ(reread->goal.extra.at("owner_note"), "keep");
  EXPECT_EQ(reread->route_extra.at("lanes")[0], "unknown route section");
  EXPECT_EQ(reread->extra.at("overworld").at("markers")[0].at("id"), "m1");
  EXPECT_NE(FindPlanAgentObservation(*reread, "item:flippers"), nullptr);
}

TEST(PlanAgentTest, UnknownIdsAndKindsAreRefusedBeforeMutation) {
  auto plan = ProjectPlan::Parse(Canonical());
  ASSERT_TRUE(plan.ok());
  const std::string before = plan->Serialize();
  for (const char* key : {"step:D9", "item:hookshot", "decision:none",
                          "milestone:w9", "room:0x0C", "D4", "step:"}) {
    PlanAgentUpdate update = Update(kZoraUpdate);
    update.observations[key] = {{"state", "X"}};
    auto diff = ApplyPlanAgentUpdate(&*plan, update);
    EXPECT_FALSE(diff.ok()) << key;
    EXPECT_EQ(plan->Serialize(), before) << key;
  }
  PlanAgentUpdate bad_type = Update(kZoraUpdate);
  bad_type.observations["step:D6"] = {{"evidence", "not a list"}};
  EXPECT_FALSE(ApplyPlanAgentUpdate(&*plan, bad_type).ok());
  PlanAgentUpdate both = Update(kZoraUpdate);
  both.remove = {"step:D4"};
  EXPECT_FALSE(ApplyPlanAgentUpdate(&*plan, both).ok());
  EXPECT_EQ(plan->Serialize(), before);
}

TEST(PlanAgentTest, RepeatedSyncIsIdempotentDespiteNewStamps) {
  auto plan = ProjectPlan::Parse(Canonical());
  ASSERT_TRUE(plan.ok());
  ASSERT_TRUE(ApplyPlanAgentUpdate(&*plan, Update(kZoraUpdate)).ok());
  const std::string once = plan->Serialize();

  PlanAgentUpdate later = Update(kZoraUpdate);
  later.agent = "agent-b";
  later.observed_utc = "2026-10-28T09:00:00Z";
  auto diff = ApplyPlanAgentUpdate(&*plan, later);
  ASSERT_TRUE(diff.ok()) << diff.status();
  EXPECT_FALSE(diff->HasChanges());
  for (const auto& change : diff->changes) {
    EXPECT_EQ(change.kind, PlanAgentChangeKind::kUnchanged) << change.key;
  }
  EXPECT_EQ(plan->Serialize(), once);
}

TEST(PlanAgentTest, ChangedObservationIsReplacedAndOthersKeepStamps) {
  auto plan = ProjectPlan::Parse(Canonical());
  ASSERT_TRUE(plan.ok());
  ASSERT_TRUE(ApplyPlanAgentUpdate(&*plan, Update(kZoraUpdate)).ok());
  PlanAgentUpdate next = Update(kZoraUpdate);
  next.agent = "agent-b";
  next.observed_utc = "2026-10-28T09:00:00Z";
  next.observations["step:D4"] = {{"state", "MERGED"}};
  auto diff = ApplyPlanAgentUpdate(&*plan, next);
  ASSERT_TRUE(diff.ok()) << diff.status();
  const PlanJson* zora = FindPlanAgentObservation(*plan, "step:D4");
  ASSERT_NE(zora, nullptr);
  EXPECT_EQ(zora->at("state"), "MERGED");
  EXPECT_FALSE(zora->contains("prs"));
  EXPECT_EQ(zora->at("agent"), "agent-b");
  const PlanJson* flippers = FindPlanAgentObservation(*plan, "item:flippers");
  ASSERT_NE(flippers, nullptr);
  EXPECT_EQ(flippers->at("agent"), "agent-a");
  EXPECT_EQ(FindPlanAgentStatus(*plan)->at("updated_utc"),
            "2026-10-28T09:00:00Z");
}

TEST(PlanAgentTest, RemoveDeletesObservationsAndAbsentKeysAreNoOps) {
  auto plan = ProjectPlan::Parse(Canonical());
  ASSERT_TRUE(plan.ok());
  ASSERT_TRUE(ApplyPlanAgentUpdate(&*plan, Update(kZoraUpdate)).ok());
  auto remove = Update(R"({"schema": "yaze.plan.agent_update/1",
                           "remove": ["item:flippers", "step:gone"]})");
  auto diff = ApplyPlanAgentUpdate(&*plan, remove);
  ASSERT_TRUE(diff.ok()) << diff.status();
  ASSERT_EQ(diff->changes.size(), 1u);
  EXPECT_EQ(diff->changes[0].kind, PlanAgentChangeKind::kRemoved);
  EXPECT_EQ(FindPlanAgentObservation(*plan, "item:flippers"), nullptr);
  EXPECT_NE(FindPlanAgentObservation(*plan, "step:D4"), nullptr);
  auto again = ApplyPlanAgentUpdate(&*plan, remove);
  ASSERT_TRUE(again.ok());
  EXPECT_FALSE(again->HasChanges());
}

TEST(PlanAgentTest, RenamesCarryObservationsAndDeletionsReportOrphans) {
  auto plan = ProjectPlan::Parse(Canonical());
  ASSERT_TRUE(plan.ok());
  ASSERT_TRUE(ApplyPlanAgentUpdate(&*plan, Update(kZoraUpdate)).ok());
  ASSERT_TRUE(plan->RenameStep("D4", "zora").ok());
  ASSERT_TRUE(plan->RenameItem("flippers", "fins").ok());
  EXPECT_EQ(FindPlanAgentObservation(*plan, "step:D4"), nullptr);
  EXPECT_NE(FindPlanAgentObservation(*plan, "step:zora"), nullptr);
  EXPECT_NE(FindPlanAgentObservation(*plan, "item:fins"), nullptr);
  EXPECT_TRUE(OrphanedPlanAgentObservations(*plan).empty());
  plan->route_items.clear();
  EXPECT_THAT(OrphanedPlanAgentObservations(*plan),
              ::testing::ElementsAre("item:fins"));
}

TEST(PlanAgentTest, MalformedAgentSectionIsNeverOverwritten) {
  auto plan = ProjectPlan::Parse(Canonical());
  ASSERT_TRUE(plan.ok());
  plan->extra[std::string(kPlanAgentStatusKey)] = "hand edited";
  const std::string before = plan->Serialize();
  auto diff = ApplyPlanAgentUpdate(&*plan, Update(kZoraUpdate));
  EXPECT_EQ(diff.status().code(), absl::StatusCode::kFailedPrecondition);
  EXPECT_EQ(plan->Serialize(), before);
}

TEST(PlanAgentTest, AdoptsOnlyAgentOnlyDiskChanges) {
  const std::string loaded = Canonical();
  auto disk = ProjectPlan::Parse(loaded);
  ASSERT_TRUE(disk.ok());
  ASSERT_TRUE(ApplyPlanAgentUpdate(&*disk, Update(kZoraUpdate)).ok());
  const std::string agent_disk = disk->Serialize();

  auto draft = ProjectPlan::Parse(loaded);
  ASSERT_TRUE(draft.ok());
  draft->route_steps[0].note = "Edited in yaze";
  ASSERT_TRUE(AdoptPlanAgentOnlyChanges(&*draft, loaded, agent_disk));
  EXPECT_EQ(draft->route_steps[0].note, "Edited in yaze");
  EXPECT_NE(FindPlanAgentObservation(*draft, "step:D4"), nullptr);

  disk->route_steps[1].note = "Someone else's edit";
  auto other = ProjectPlan::Parse(loaded);
  ASSERT_TRUE(other.ok());
  const std::string other_before = other->Serialize();
  EXPECT_FALSE(AdoptPlanAgentOnlyChanges(&*other, loaded, disk->Serialize()));
  EXPECT_EQ(other->Serialize(), other_before);
  EXPECT_FALSE(AdoptPlanAgentOnlyChanges(&*other, loaded, "not json"));
}

TEST_F(PlanFile, ApplyPublishesAndUnchangedResyncDoesNotWrite) {
  const std::string before = Read(path_);
  auto result = ApplyPlanAgentUpdateToFile(path_.string(),
                                           UpdateAtCurrentBase(kZoraUpdate),
                                           /*dry_run=*/false);
  ASSERT_TRUE(result.ok()) << result.status();
  EXPECT_TRUE(result->written);
  EXPECT_EQ(result->before_sha256, ComputeSourceArtifactSha256(before));
  const std::string after = Read(path_);
  EXPECT_EQ(result->after_sha256, ComputeSourceArtifactSha256(after));
  auto reread = ProjectPlan::Parse(after);
  ASSERT_TRUE(reread.ok());
  EXPECT_EQ(HumanPart(*reread), before);

  auto again = ApplyPlanAgentUpdateToFile(path_.string(),
                                          UpdateAtCurrentBase(kZoraUpdate),
                                          /*dry_run=*/false);
  ASSERT_TRUE(again.ok()) << again.status();
  EXPECT_FALSE(again->written);
  EXPECT_FALSE(again->diff.HasChanges());
  EXPECT_EQ(Read(path_), after);
}

TEST_F(PlanFile, ApplyKeepsEveryHumanByteIncludingOrderAndEmptyValues) {
  // Key order and empty arrays the model would normalize on its own save.
  const std::string human = PlanJson::parse(R"({
       "schema": "yaze.plan/1", "title": "Hack",
       "route": {
        "steps": [{"id": "D4", "title": "Zora", "kind": "dungeon",
                   "code": "D4", "note": ""}],
        "items": [{"id": "flippers", "name": "Flippers", "step": "D4",
                   "required": true, "needs": []}]
       },
       "decisions": [], "zz_unknown": {"keep": [1, 2]}
      })")
                                .dump(1) +
                            "\n";
  Write(path_, human);
  for (const char* utc : {"2026-10-27T21:00:00Z", "2026-10-28T09:00:00Z"}) {
    PlanAgentUpdate update = UpdateAtCurrentBase(kZoraUpdate);
    update.observations.erase(std::string("item:flippers"));
    update.observations["step:D4"]["state"] = utc;
    auto result = ApplyPlanAgentUpdateToFile(path_.string(), update, false);
    ASSERT_TRUE(result.ok()) << result.status();
    ASSERT_TRUE(result->written);
    PlanJson after = PlanJson::parse(Read(path_));
    after.erase(std::string(kPlanAgentStatusKey));
    EXPECT_EQ(after.dump(1) + "\n", human);
  }
}

TEST_F(PlanFile, StaleBaseIsRefusedWithBytesUnchanged) {
  PlanAgentUpdate update = UpdateAtCurrentBase(kZoraUpdate);
  auto edited = ProjectPlan::Parse(Read(path_));
  ASSERT_TRUE(edited.ok());
  edited->route_steps[0].note = "Human edit after the agent read the plan";
  Write(path_, edited->Serialize());
  const std::string human_bytes = Read(path_);

  auto result = ApplyPlanAgentUpdateToFile(path_.string(), update, false);
  EXPECT_EQ(result.status().code(), absl::StatusCode::kFailedPrecondition);
  EXPECT_THAT(result.status().message(), HasSubstr("Nothing was written"));
  EXPECT_EQ(Read(path_), human_bytes);
}

TEST_F(PlanFile, DryRunAndMissingBaseNeverWrite) {
  const std::string before = Read(path_);
  auto dry = ApplyPlanAgentUpdateToFile(path_.string(), Update(kZoraUpdate),
                                        /*dry_run=*/true);
  ASSERT_TRUE(dry.ok()) << dry.status();
  EXPECT_TRUE(dry->diff.HasChanges());
  EXPECT_FALSE(dry->written);
  EXPECT_NE(dry->after_sha256, dry->before_sha256);
  auto no_base =
      ApplyPlanAgentUpdateToFile(path_.string(), Update(kZoraUpdate), false);
  EXPECT_EQ(no_base.status().code(), absl::StatusCode::kInvalidArgument);
  PlanAgentUpdate unknown = UpdateAtCurrentBase(kZoraUpdate);
  unknown.observations["step:nope"] = {{"state", "X"}};
  EXPECT_FALSE(ApplyPlanAgentUpdateToFile(path_.string(), unknown, false).ok());
  EXPECT_EQ(Read(path_), before);
}

TEST_F(PlanFile, AgentUserAgentRoundTripPreservesTheAnnotation) {
  ASSERT_TRUE(ApplyPlanAgentUpdateToFile(
                  path_.string(), UpdateAtCurrentBase(kZoraUpdate), false)
                  .ok());
  // A person annotates through the model's CAS save path.
  std::string loaded;
  auto plan = ProjectPlan::LoadFromFile(path_.string(), &loaded);
  ASSERT_TRUE(plan.ok());
  plan->route_steps[0].note = "Play D4 with the new water gates";
  plan->decisions[0].answer = "Approved answer text";
  ASSERT_TRUE(plan->SaveToFile(path_.string(), loaded).ok());

  PlanAgentUpdate second = UpdateAtCurrentBase(kZoraUpdate);
  second.observed_utc = "2026-10-28T09:00:00Z";
  second.observations["step:D6"] = {{"state", "PRIVATE_CANDIDATE"}};
  auto result = ApplyPlanAgentUpdateToFile(path_.string(), second, false);
  ASSERT_TRUE(result.ok()) << result.status();
  EXPECT_TRUE(result->written);
  auto reread = ProjectPlan::LoadFromFile(path_.string());
  ASSERT_TRUE(reread.ok());
  EXPECT_EQ(reread->route_steps[0].note, "Play D4 with the new water gates");
  EXPECT_EQ(reread->decisions[0].answer, "Approved answer text");
  EXPECT_NE(FindPlanAgentObservation(*reread, "step:D6"), nullptr);
  EXPECT_EQ(FindPlanAgentObservation(*reread, "step:D4")->at("observed_utc"),
            "2026-10-27T21:00:00Z");
}

}  // namespace
}  // namespace yaze::core
