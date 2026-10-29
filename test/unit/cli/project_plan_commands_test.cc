// plan-read and plan-agent-apply (cli/handlers/rom/project_plan_commands.h).

#include "cli/handlers/rom/project_plan_commands.h"

#include <filesystem>
#include <fstream>
#include <iterator>
#include <string>

#include <gtest/gtest.h>

#include "core/project_plan.h"
#include "core/project_plan_agent.h"
#include "core/source_artifact_publisher.h"
#include "nlohmann/json.hpp"
#include "unique_temp_path.h"

namespace yaze::cli {
namespace {

namespace fs = std::filesystem;
using json = nlohmann::json;

std::string Read(const fs::path& path) {
  std::ifstream file(path, std::ios::binary);
  return std::string(std::istreambuf_iterator<char>(file), {});
}

void Write(const fs::path& path, const std::string& text) {
  std::ofstream(path, std::ios::binary | std::ios::trunc) << text;
}

class PlanCommandsTest : public ::testing::Test {
 protected:
  void SetUp() override {
    dir_ = test::UniqueTempPath("plan_commands");
    fs::create_directories(dir_);
    plan_ = dir_ / "hack.plan.json";
    auto plan = core::ProjectPlan::Parse(R"({
     "schema": "yaze.plan/1", "title": "Hack",
     "route": {"steps": [{"id": "D4", "note": "Human note"}]},
     "decisions": [{"id": "d4-rescue", "answer": "Approved"}]
    })");
    ASSERT_TRUE(plan.ok());
    Write(plan_, plan->Serialize());
    Write(dir_ / "hack.yaze",
          "[project]\nname=Hack\n[planning]\nfile=hack.plan.json\n");
  }
  void TearDown() override {
    std::error_code ec;
    fs::remove_all(dir_, ec);
  }
  fs::path WriteUpdate(const std::string& base) {
    const fs::path path = dir_ / "update.json";
    Write(path, R"({"schema": "yaze.plan.agent_update/1",
                    "base_sha256": ")" +
                    base + R"(", "agent": "agent",
                    "observed_utc": "2026-10-27T21:00:00Z",
                    "observations": {"step:D4": {"state": "DRAFT"}}})");
    return path;
  }
  fs::path dir_;
  fs::path plan_;
};

TEST_F(PlanCommandsTest, ReadReportsShaIdsAndResolvesProject) {
  handlers::PlanReadCommandHandler handler;
  std::string out;
  ASSERT_TRUE(
      handler
          .Run({"--project=" + (dir_ / "hack.yaze").string(), "--format=json"},
               nullptr, &out)
          .ok())
      << out;
  const json doc = json::parse(out);
  EXPECT_EQ(doc.at("sha256"), core::ComputeSourceArtifactSha256(Read(plan_)));
  EXPECT_EQ(doc.at("steps"), json::array({"D4"}));
  EXPECT_EQ(doc.at("decisions"), json::array({"d4-rescue"}));
  EXPECT_FALSE(doc.at("agent_status").at("present").get<bool>());

  EXPECT_FALSE(handler.Run({"--format=json"}, nullptr, &out).ok());
}

TEST_F(PlanCommandsTest, ApplyDryRunApplyAndStaleRefusal) {
  const std::string base = core::ComputeSourceArtifactSha256(Read(plan_));
  const fs::path update = WriteUpdate(base);
  handlers::PlanAgentApplyCommandHandler handler;
  std::string out;
  ASSERT_TRUE(
      handler
          .Run({"--plan=" + plan_.string(), "--update=" + update.string(),
                "--dry-run", "--format=json"},
               nullptr, &out)
          .ok())
      << out;
  json doc = json::parse(out);
  EXPECT_FALSE(doc.at("written").get<bool>());
  EXPECT_EQ(doc.at("changes"), json::array({"added step:D4"}));
  EXPECT_EQ(core::ComputeSourceArtifactSha256(Read(plan_)), base);

  out.clear();
  ASSERT_TRUE(handler
                  .Run({"--plan=" + plan_.string(),
                        "--update=" + update.string(), "--format=json"},
                       nullptr, &out)
                  .ok())
      << out;
  doc = json::parse(out);
  EXPECT_TRUE(doc.at("written").get<bool>());
  const std::string applied = Read(plan_);
  EXPECT_EQ(doc.at("after_sha256"), core::ComputeSourceArtifactSha256(applied));
  auto plan = core::ProjectPlan::Parse(applied);
  ASSERT_TRUE(plan.ok());
  EXPECT_EQ(plan->route_steps[0].note, "Human note");
  EXPECT_NE(core::FindPlanAgentObservation(*plan, "step:D4"), nullptr);

  // The same update now names an old base and must not write.
  out.clear();
  const auto stale =
      handler.Run({"--plan=" + plan_.string(), "--update=" + update.string()},
                  nullptr, &out);
  EXPECT_EQ(stale.code(), absl::StatusCode::kFailedPrecondition) << stale;
  EXPECT_EQ(Read(plan_), applied);
}

}  // namespace
}  // namespace yaze::cli
