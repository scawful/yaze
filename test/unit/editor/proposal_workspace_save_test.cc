#include <chrono>
#include <filesystem>
#include <fstream>
#include <memory>

#include "app/application.h"
#include "app/editor/editor_manager.h"
#include "app/gfx/backend/null_renderer.h"
#include "core/gfx_sheet_source.h"
#include "core/native_save_receipt.h"
#include "core/proposal_workspace.h"
#include "core/source_artifact_publisher.h"
#include "editor_test_support.h"
#include "gtest/gtest.h"
#include "imgui/imgui.h"
#include "imgui/imgui_internal.h"
#include "nlohmann/json.hpp"
#include "oracle_rom_fixture.h"
#include "util/rom_hash.h"

namespace yaze::editor {
namespace {
namespace fs = std::filesystem;
using nlohmann::json;

class ProposalWorkspaceSaveTest : public ::testing::Test {
 protected:
  void SetUp() override {
    const auto fixture = test::FindOracleRomFixture();
    if (fixture.empty())
      GTEST_SKIP() << "Oracle ROM fixture is unavailable";
    root_ = fs::temp_directory_path() /
            ("yaze_proposal_save_" +
             std::to_string(
                 std::chrono::steady_clock::now().time_since_epoch().count()));
    fs::create_directories(root_ / "Draft.yazeproj/source/Data/graphics");
    root_ = fs::canonical(root_);
    draft_ = root_ / "Draft.yazeproj";
    fs::copy_file(fixture, draft_ / "rom");
    fs::copy_file(fixture, root_ / "baseline.rom");
    const std::vector<uint8_t> pixels(4096, 0);
    Write("Draft.yazeproj/source/Data/graphics/pixels.txt",
          core::GfxSheetSource::EncodeText(pixels));
    const json spec = {
        {"schema_version", 2},
        {"start", "0x40D000"},
        {"capacity", "0x3000"},
        {"encoding", "lz2"},
        {"sheets", json::array({{{"global_sheet", 0xDF},
                                 {"source", "pixels.txt"},
                                 {"decoded_sha256",
                                  core::GfxSheetSource::DecodedSha256(pixels)},
                                 {"why", "Clean proposal save regression"}}})}};
    const auto spec_text = spec.dump(2) + "\n";
    Write("Draft.yazeproj/source/Data/graphics/source_sprite_sheets.json",
          spec_text);
    project::YazeProject project;
    project.filepath = (draft_ / "project.yaze").string();
    project.name = "Clean proposal";
    project.metadata.project_id = "clean-proposal-test";
    project.rom_filename = (draft_ / "rom").string();
    project.code_folder = (draft_ / "source").string();
    project.rom_metadata.role = project::RomRole::kDev;
    project.rom_metadata.write_policy = project::RomWritePolicy::kAllow;
    project.rom_metadata.expected_hash =
        util::ComputeFileSha1Hex(project.rom_filename);
    project.workspace_settings.backup_on_save = false;
    project.workspace_settings.autosave_enabled = false;
    project.experimental_page_tile16 = true;
    project.graphics_sheets.source_spec =
        "Data/graphics/source_sprite_sheets.json";
    project.graphics_sheets.source_spec_sha1 =
        core::GfxSheetSource::SpecSha1(spec_text);
    ASSERT_TRUE(project.Save().ok());
    json baseline = json::object();
    json sources = json::object();
    for (const auto& entry : fs::recursive_directory_iterator(draft_)) {
      if (!entry.is_regular_file())
        continue;
      const auto relative = fs::relative(entry.path(), draft_);
      const auto hash = Hash(entry.path());
      baseline[relative.generic_string()] = hash;
      if (relative == "rom")
        continue;
      const auto target = root_ / "baseline-files" / relative;
      fs::create_directories(target.parent_path());
      fs::copy_file(entry.path(), target);
      sources[relative.generic_string()] = hash;
    }
    Write("baseline.json", baseline.dump());
    Write("baseline-files.json", sources.dump());
    const json packet = {
        {"schema", "yaze.proposal-workbench.v1"},
        {"proposal_id", "clean-save"},
        {"domain", "overworld"},
        {"target", 0xA0},
        {"draft_project", "Draft.yazeproj"},
        {"rom", "rom"},
        {"references", json::array()},
        {"baseline_rom_sha256", Hash(root_ / "baseline.rom")},
        {"baseline_inventory_sha256", Hash(root_ / "baseline.json")},
        {"baseline_files_inventory_sha256",
         Hash(root_ / "baseline-files.json")}};
    Write("proposal.json", packet.dump());
    auto workspace = core::LoadProposalWorkspace(root_);
    ASSERT_TRUE(workspace.ok()) << workspace.status();
    context_ = ImGui::CreateContext();
    ImGui::SetCurrentContext(context_);
    unsigned char* data = nullptr;
    int width = 0, height = 0;
    ImGui::GetIO().Fonts->GetTexDataAsRGBA32(&data, &width, &height);
    renderer_ = std::make_unique<gfx::NullRenderer>();
    manager_ = std::make_unique<EditorManager>();
    test::InitializeWithIsolatedSettings(*manager_, renderer_.get());
    manager_->SetAssetLoadMode(AssetLoadMode::kLazy);
    const auto opened =
        manager_->OpenRomOrProject((draft_ / "project.yaze").string());
    ASSERT_TRUE(opened.ok()) << opened;
  }
  void TearDown() override {
    manager_.reset();
    renderer_.reset();
    if (context_)
      ImGui::DestroyContext(context_);
    if (!root_.empty()) {
      std::error_code error;
      fs::remove_all(root_, error);
    }
  }
  void Write(const std::string& relative, const std::string& bytes) {
    std::ofstream(root_ / relative, std::ios::binary) << bytes;
  }
  std::string Hash(const fs::path& path) {
    auto result = core::ComputeSourceArtifactFileSha256(path);
    EXPECT_TRUE(result.ok());
    return result.ok() ? *result : "";
  }
  fs::path root_, draft_;
  ImGuiContext* context_ = nullptr;
  std::unique_ptr<gfx::NullRenderer> renderer_;
  std::unique_ptr<EditorManager> manager_;
};

TEST_F(ProposalWorkspaceSaveTest, CleanSourceDraftGetsItsOwnCommittedSave) {
  const auto receipt = draft_ / "project.yaze.native-save.json";
  ASSERT_FALSE(fs::exists(receipt));
  ASSERT_TRUE(core::GfxSheetSource::Selected(*manager_->GetCurrentProject()));
  ASSERT_FALSE(manager_->IsCurrentProjectDirty());
  ASSERT_FALSE(manager_->GetCurrentRom()->dirty());
  // This is the regression: ordinary clean session Save alone is a no-op.
  ASSERT_TRUE(manager_->SaveCurrentSession(false).ok());
  ASSERT_FALSE(fs::exists(receipt));
  const auto saved = manager_->SaveProposalDraft();
  ASSERT_TRUE(saved.ok()) << saved;
  const auto identity = core::ReadVerifiedCommittedNativeSaveIdentity(
      (draft_ / "rom").string(), (draft_ / "project.yaze").string(),
      (draft_ / "rom").string());
  ASSERT_TRUE(identity.ok()) << identity.status();
  EXPECT_GE(identity->generation, 1u);
  EXPECT_EQ(Hash(root_ / "baseline.rom"), Hash(draft_ / "rom"));
}

TEST_F(ProposalWorkspaceSaveTest, StartupPanelDrawsAfterTargetEditorActivates) {
  AppConfig config;
  config.startup_editor = "Overworld";
  config.jump_to_map = 0xA0;
  config.welcome_mode = StartupVisibility::kHide;
  config.dashboard_mode = StartupVisibility::kHide;
  config.sidebar_mode = StartupVisibility::kHide;
  config.open_panels = {"proposal.workbench"};
  manager_->ProcessStartupActions(config);

  auto& windows = manager_->window_manager();
  const size_t session = manager_->GetCurrentSessionId();
  ASSERT_TRUE(windows.IsWindowOpen(session, "proposal.workbench"));
  // Deferred editor activation leaves the target editor's category active.
  windows.SetActiveCategory("Overworld", /*notify=*/false);

  ImGuiIO& io = ImGui::GetIO();
  io.IniFilename = nullptr;
  io.DisplaySize = ImVec2(1280, 800);
  io.DeltaTime = 1.0f / 60.0f;
  for (int i = 0; i < 3; ++i) {
    ImGui::NewFrame();
    windows.DrawVisibleWindows();
    ImGui::Render();
  }
  const auto* descriptor =
      windows.GetWindowDescriptor(session, "proposal.workbench");
  ASSERT_NE(descriptor, nullptr);
  const auto name = windows.GetWorkspaceWindowName(*descriptor);
  const auto* window = ImGui::FindWindowByName(name.c_str());
  ASSERT_NE(window, nullptr) << name;
  EXPECT_TRUE(window->Active) << name;
}

TEST_F(ProposalWorkspaceSaveTest, DifferentActiveRomRefusesBeforeWriting) {
  const auto before = Hash(draft_ / "project.yaze");
  fs::copy_file(draft_ / "rom", root_ / "other-rom");
  manager_->GetCurrentRom()->set_filename((root_ / "other-rom").string());
  EXPECT_FALSE(manager_->SaveProposalDraft().ok());
  EXPECT_EQ(Hash(draft_ / "project.yaze"), before);
  EXPECT_FALSE(fs::exists(draft_ / "project.yaze.native-save.json"));
}
}  // namespace
}  // namespace yaze::editor
