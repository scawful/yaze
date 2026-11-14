#include "core/proposal_workspace.h"

#include <chrono>
#include <filesystem>
#include <fstream>

#include "core/source_artifact_publisher.h"
#include "gtest/gtest.h"

#if defined(YAZE_WITH_JSON)
#include "nlohmann/json.hpp"

namespace yaze::core {
namespace {
namespace fs = std::filesystem;
using nlohmann::json;

class ProposalWorkspaceTest : public ::testing::Test {
 protected:
  void SetUp() override {
    root_ = fs::temp_directory_path() /
            ("yaze_proposal_" +
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
               {"proposal_id", "water-control"},
               {"domain", "dungeon"},
               {"target", 0x25},
               {"graphics_entrance_slot", 0x31},
               {"draft_project", "Draft.yazeproj"},
               {"rom", "rom"},
               {"baseline_rom_sha256", Hash("baseline.rom")},
               {"baseline_inventory_sha256", Hash("baseline.json")},
               {"baseline_files_inventory_sha256", Hash("baseline-files.json")},
               {"references", json::array()}};
    Publish();
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
  void Publish() { Write("proposal.json", packet_.dump()); }
  fs::path root_;
  json packet_;
};

TEST_F(ProposalWorkspaceTest, LoadsDungeonWithExplicitEntranceContext) {
  auto result = LoadProposalWorkspace(root_);
  ASSERT_TRUE(result.ok()) << result.status();
  EXPECT_EQ(result->target, 0x25);
  EXPECT_EQ(result->graphics_entrance_slot, 0x31);
  EXPECT_EQ(result->project_file, root_ / "Draft.yazeproj/project.yaze");
}

TEST_F(ProposalWorkspaceTest, LoadsNativeSpecialOverworldTarget) {
  packet_["domain"] = "overworld";
  packet_["target"] = 0xBF;
  packet_["graphics_entrance_slot"] = nullptr;
  Publish();
  auto result = LoadProposalWorkspace(root_);
  ASSERT_TRUE(result.ok()) << result.status();
  EXPECT_EQ(result->target, 0xBF);
  EXPECT_EQ(result->graphics_entrance_slot, -1);
}

TEST_F(ProposalWorkspaceTest, RefusesInvalidRoomAndEntrance) {
  packet_["target"] = 296;
  Publish();
  EXPECT_FALSE(LoadProposalWorkspace(root_).ok());
  packet_["target"] = 0x25;
  packet_["graphics_entrance_slot"] = 140;
  Publish();
  EXPECT_FALSE(LoadProposalWorkspace(root_).ok());
}

TEST_F(ProposalWorkspaceTest, RefusesDraftEscape) {
  packet_["draft_project"] = "../Main.yazeproj";
  Publish();
  EXPECT_FALSE(LoadProposalWorkspace(root_).ok());
}

TEST_F(ProposalWorkspaceTest, NativeEntryRefusesDescriptorRomRedirect) {
  Write("shared-rom", "shared game");
  Write("Draft.yazeproj/project.yaze", "[files]\nrom_filename=../shared-rom\n");
  auto result = LoadProposalWorkspace(root_);
  ASSERT_FALSE(result.ok());
  EXPECT_NE(std::string(result.status().message()).find("draft ROM"),
            std::string::npos);
}

TEST_F(ProposalWorkspaceTest, NativeEntryRefusesExternalSourceAndOutput) {
  for (const auto& field :
       {"code_folder", "custom_collision_json", "output_folder"}) {
    Write("Draft.yazeproj/project.yaze",
          std::string("[files]\nrom_filename=rom\n") + field + "=../shared\n");
    EXPECT_FALSE(LoadProposalWorkspace(root_).ok()) << field;
  }
}

TEST_F(ProposalWorkspaceTest, NativeEntryRefusesExternalGraphicsSpecification) {
  Write("Draft.yazeproj/project.yaze",
        "[files]\nrom_filename=rom\ncode_folder=source\n[graphics_sheets]"
        "\nsource_spec=../../shared.json\n");
  EXPECT_FALSE(LoadProposalWorkspace(root_).ok());
}

TEST_F(ProposalWorkspaceTest, RefusesTamperedBaseline) {
  Write("baseline.rom", "different original");
  EXPECT_FALSE(LoadProposalWorkspace(root_).ok());
}

TEST_F(ProposalWorkspaceTest, RefusesTamperedSourceGraphicsBaseline) {
  Write("baseline-files/source/sheet.bin", "changed baseline pixel");
  EXPECT_FALSE(LoadProposalWorkspace(root_).ok());
}

TEST_F(ProposalWorkspaceTest,
       ExportPreservesBaselineAndIncludesSourceGraphics) {
  auto workspace = LoadProposalWorkspace(root_);
  ASSERT_TRUE(workspace.ok());
  const auto baseline = Hash("baseline.rom");
  Write("Draft.yazeproj/source/sheet.bin", "user pixel edit");
  Write("Draft.yazeproj/rom", "user room edit");
  auto exported = ExportProposalFeedback(*workspace, "Move the inlet one tile");
  ASSERT_TRUE(exported.ok()) << exported.status();
  std::ifstream stream(*exported);
  const auto feedback = json::parse(stream);
  EXPECT_EQ(feedback["changed_files"].size(), 2);
  EXPECT_EQ(feedback["note"], "Move the inlet one tile");
  EXPECT_EQ(feedback["graphics_entrance_slot"], 0x31);
  EXPECT_EQ(Hash("baseline.rom"), baseline);
  EXPECT_EQ(Hash("Draft.yazeproj/rom"),
            ComputeSourceArtifactSha256("user room edit"));
}

TEST_F(ProposalWorkspaceTest, ExportRefusesChangedPacketIdentity) {
  auto workspace = LoadProposalWorkspace(root_);
  ASSERT_TRUE(workspace.ok());
  Write("Draft.yazeproj/other-rom", "other");
  packet_["rom"] = "other-rom";
  Publish();
  EXPECT_FALSE(ExportProposalFeedback(*workspace, "note").ok());
}

}  // namespace
}  // namespace yaze::core
#endif
