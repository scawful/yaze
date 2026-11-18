#include "core/build_run_acceptance_admission.h"

#include <chrono>
#include <filesystem>
#include <fstream>

#include "core/proposal_workspace.h"
#include "core/source_artifact_publisher.h"
#include "gtest/gtest.h"

#if defined(YAZE_WITH_JSON)
#include "nlohmann/json.hpp"

namespace yaze::core {
namespace {
namespace fs = std::filesystem;
using nlohmann::json;

json BaseFixture() {
  return json{{"phase", "edit"},
              {"map_id", 144},
              {"x", 17},
              {"y", 11},
              {"original_tile", 4057},
              {"brush_id", 4050},
              {"input_rom_sha256", std::string(64, 'a')},
              {"wait_for_window_observation", true}};
}

TEST(BuildRunAcceptanceAdmissionTest, ParsesLegacyByDefault) {
  auto config = ParseBuildRunAcceptanceConfig(BaseFixture());
  ASSERT_TRUE(config.ok()) << config.status();
  EXPECT_EQ(config->workflow, BuildRunWorkflow::kLegacy);
  EXPECT_EQ(config->phase, "edit");
  EXPECT_TRUE(config->wait_for_window_observation);
}

TEST(BuildRunAcceptanceAdmissionTest, LegacyRequiresBuildRunNameAndEditingRom) {
  EXPECT_TRUE(ValidateLegacyBuildRunProject("[Build Run acceptance] Sea Zora",
                                            "/tmp/case/editing.sfc",
                                            "/tmp/case")
                  .ok());
  EXPECT_FALSE(ValidateLegacyBuildRunProject(
                   "Ordinary draft", "/tmp/case/editing.sfc", "/tmp/case")
                   .ok());
  EXPECT_FALSE(ValidateLegacyBuildRunProject("[Build Run acceptance] Sea Zora",
                                             "/tmp/case/other.sfc", "/tmp/case")
                   .ok());
}

std::string AbsoluteTestPath(const std::string& leaf) {
  return (fs::temp_directory_path() / leaf).lexically_normal().string();
}

TEST(BuildRunAcceptanceAdmissionTest, ProposalEditRequiresAbsoluteRoot) {
  auto fixture = BaseFixture();
  fixture["workflow"] = "proposal_edit";
  EXPECT_FALSE(ParseBuildRunAcceptanceConfig(fixture).ok());
  fixture["proposal_root"] = "relative/packet";
  EXPECT_FALSE(ParseBuildRunAcceptanceConfig(fixture).ok());
  fixture["proposal_root"] = AbsoluteTestPath("packet");
  auto config = ParseBuildRunAcceptanceConfig(fixture);
  ASSERT_TRUE(config.ok()) << config.status();
  EXPECT_EQ(config->workflow, BuildRunWorkflow::kProposalEdit);
}

TEST(BuildRunAcceptanceAdmissionTest, ProposalEditRefusesExtraCells) {
  auto fixture = BaseFixture();
  fixture["workflow"] = "proposal_edit";
  fixture["proposal_root"] = AbsoluteTestPath("packet");
  fixture["extra_cells"] = json::array(
      {json{{"x", 1}, {"y", 1}, {"brush_id", 1}, {"original_tile", 2}}});
  EXPECT_FALSE(ParseBuildRunAcceptanceConfig(fixture).ok());
}

TEST(BuildRunAcceptanceAdmissionTest, ProposalEditReopenRequiresExpectedHash) {
  auto fixture = BaseFixture();
  fixture["workflow"] = "proposal_edit";
  fixture["phase"] = "reopen";
  fixture["proposal_root"] = AbsoluteTestPath("packet");
  fixture.erase("input_rom_sha256");
  EXPECT_FALSE(ParseBuildRunAcceptanceConfig(fixture).ok());
  fixture["expected_rom_sha256"] = std::string(64, 'b');
  ASSERT_TRUE(ParseBuildRunAcceptanceConfig(fixture).ok());
}

class ProposalEditAdmissionBindingTest : public ::testing::Test {
 protected:
  void SetUp() override {
    root_ = fs::temp_directory_path() /
            ("yaze_proposal_edit_admit_" +
             std::to_string(
                 std::chrono::steady_clock::now().time_since_epoch().count()));
    fs::create_directories(root_ / "Draft.yazeproj/source");
    root_ = fs::canonical(root_);
    Write("Draft.yazeproj/project.yaze", "[files]\nrom_filename=rom\n");
    Write("Draft.yazeproj/rom", "original ROM");
    Write("Draft.yazeproj/source/sheet.bin", "source-backed graphics");
    Write("baseline.rom", "original ROM");
    json baseline = {
        {"rom", Hash("Draft.yazeproj/rom")},
        {"project.yaze", Hash("Draft.yazeproj/project.yaze")},
        {"source/sheet.bin", Hash("Draft.yazeproj/source/sheet.bin")}};
    Write("baseline.json", baseline.dump());
    fs::create_directories(root_ / "baseline-files/source");
    Write("baseline-files/source/sheet.bin", "source-backed graphics");
    Write("baseline-files.json",
          json({{"source/sheet.bin", Hash("baseline-files/source/sheet.bin")}})
              .dump());
    packet_ = {{"schema", "yaze.proposal-workbench.v1"},
               {"proposal_id", "sea-zora-edit"},
               {"domain", "overworld"},
               {"target", 144},
               {"draft_project", "Draft.yazeproj"},
               {"rom", "rom"},
               {"baseline_rom_sha256", Hash("baseline.rom")},
               {"baseline_inventory_sha256", Hash("baseline.json")},
               {"baseline_files_inventory_sha256", Hash("baseline-files.json")},
               {"references", json::array()}};
    Write("proposal.json", packet_.dump());
  }
  void TearDown() override {
    std::error_code error;
    fs::remove_all(root_, error);
  }
  void Write(const std::string& name, const std::string& content) {
    std::ofstream(root_ / name, std::ios::binary) << content;
  }
  std::string Hash(const std::string& name) {
    auto value = ComputeSourceArtifactFileSha256(root_ / name);
    EXPECT_TRUE(value.ok());
    return value.ok() ? *value : "";
  }
  fs::path root_;
  json packet_;
};

TEST_F(ProposalEditAdmissionBindingTest, AcceptsMatchingDraftBindings) {
  auto workspace = LoadProposalWorkspace(root_);
  ASSERT_TRUE(workspace.ok()) << workspace.status();
  BuildRunAcceptanceConfig config;
  config.workflow = BuildRunWorkflow::kProposalEdit;
  config.phase = "edit";
  config.map_id = 144;
  config.proposal_root = root_;
  config.input_rom_sha256 = workspace->baseline_rom_sha256;
  EXPECT_TRUE(ValidateProposalEditAdmission(
                  config, *workspace, workspace->project_file,
                  workspace->rom_file, workspace->baseline_rom_sha256)
                  .ok());
}

TEST_F(ProposalEditAdmissionBindingTest, RefusesWrongMapOrRomHash) {
  auto workspace = LoadProposalWorkspace(root_);
  ASSERT_TRUE(workspace.ok()) << workspace.status();
  BuildRunAcceptanceConfig config;
  config.workflow = BuildRunWorkflow::kProposalEdit;
  config.phase = "edit";
  config.map_id = 90;
  config.proposal_root = root_;
  config.input_rom_sha256 = workspace->baseline_rom_sha256;
  EXPECT_FALSE(ValidateProposalEditAdmission(
                   config, *workspace, workspace->project_file,
                   workspace->rom_file, workspace->baseline_rom_sha256)
                   .ok());
  config.map_id = 144;
  config.input_rom_sha256 = std::string(64, '0');
  EXPECT_FALSE(ValidateProposalEditAdmission(
                   config, *workspace, workspace->project_file,
                   workspace->rom_file, workspace->baseline_rom_sha256)
                   .ok());
}

}  // namespace
}  // namespace yaze::core
#endif
