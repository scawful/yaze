#include "app/editor/hack/workflow/hack_workflow_backend_factory.h"
#include "app/editor/hack/workflow/hack_workflow_backend.h"

#include "core/project.h"
#include "gtest/gtest.h"

namespace yaze::editor::workflow {
namespace {

TEST(HackWorkflowBackendFactoryTest, SelectsManifestBackendForGenericHack) {
  project::YazeProject project;
  ASSERT_TRUE(project.hack_manifest.LoadFromString(R"json(
{
  "manifest_version": 2,
  "hack_name": "Generic Hack"
}
)json")
                  .ok());

  auto backend = CreateHackWorkflowBackendForProject(&project);
  ASSERT_NE(backend, nullptr);
  EXPECT_EQ(backend->GetBackendId(), "manifest");
  EXPECT_EQ(dynamic_cast<ValidationCapability*>(backend.get()), nullptr);
  EXPECT_EQ(dynamic_cast<ProgressionCapability*>(backend.get()), nullptr);
  EXPECT_NE(dynamic_cast<PlanningCapability*>(backend.get()), nullptr);
}

TEST(HackWorkflowBackendFactoryTest, SelectsOracleBackendForOracleHack) {
  project::YazeProject project;
  ASSERT_TRUE(project.hack_manifest.LoadFromString(R"json(
{
  "manifest_version": 2,
  "hack_name": "Oracle of Secrets"
}
)json")
                  .ok());

  auto backend = CreateHackWorkflowBackendForProject(&project);
  ASSERT_NE(backend, nullptr);
  EXPECT_EQ(backend->GetBackendId(), "oracle");
  EXPECT_NE(dynamic_cast<ValidationCapability*>(backend.get()), nullptr);
  EXPECT_NE(dynamic_cast<ProgressionCapability*>(backend.get()), nullptr);
  EXPECT_NE(dynamic_cast<PlanningCapability*>(backend.get()), nullptr);
}

TEST(HackWorkflowBackendFactoryTest,
     SelectsOracleBackendWhenProgressionStateIsPresentDespiteName) {
  // Regression: an Oracle-capable project renamed to something else should
  // still get the Oracle backend (capability-based detection), not silently
  // fall back to the manifest-only backend and lose validation/progression.
  project::YazeProject project;
  ASSERT_TRUE(project.hack_manifest
                  .LoadFromString(R"json(
{
  "manifest_version": 2,
  "hack_name": "Generic Zelda Hack Fork"
}
)json")
                  .ok());
  project.hack_manifest.SetOracleProgressionState(
      core::OracleProgressionState{});

  auto backend = CreateHackWorkflowBackendForProject(&project);
  ASSERT_NE(backend, nullptr);
  EXPECT_EQ(backend->GetBackendId(), "oracle");
  EXPECT_NE(dynamic_cast<ValidationCapability*>(backend.get()), nullptr);
  EXPECT_NE(dynamic_cast<ProgressionCapability*>(backend.get()), nullptr);
}

}  // namespace
}  // namespace yaze::editor::workflow
