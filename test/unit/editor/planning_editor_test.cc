#include "app/editor/planning/planning_editor.h"

#include <algorithm>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <memory>
#include <string>
#include <system_error>
#include <utility>
#include <vector>

#include "app/editor/editor_manager.h"
#include "app/editor/session_types.h"
#include "app/gfx/backend/null_renderer.h"
#include "core/project.h"
#include "editor_test_support.h"
#include "gtest/gtest.h"
#include "imgui/imgui.h"
#include "unique_temp_path.h"

namespace yaze::editor {

class PlanningEditorTestPeer {
 public:
  static void EditGoal(PlanningEditor* editor, const std::string& title) {
    editor->plan_->goal.title = title;
    editor->MarkDirty();
  }
  static absl::Status Reload(PlanningEditor* editor, bool discard = false) {
    return editor->Reload(discard);
  }
};

namespace {
namespace fs = std::filesystem;

std::string Read(const fs::path& path) {
  std::ifstream file(path, std::ios::binary);
  return std::string(std::istreambuf_iterator<char>(file), {});
}

void Write(const fs::path& path, const std::string& content) {
  std::ofstream(path, std::ios::binary | std::ios::trunc) << content;
}

class PlanningEditorTest : public ::testing::Test {
 protected:
  void SetUp() override {
    directory_ = test::UniqueTempPath("planning_editor");
    fs::create_directories(directory_);
    project_path_ = directory_ / "hack.yaze";
    plan_path_ = directory_ / "hack.plan.json";
    Write(project_path_,
          "[project]\nname=Hack\n[planning]\nfile=hack.plan.json\n");
    ASSERT_TRUE(core::ProjectPlan::CreateEmpty("Plan")
                    .SaveToFile(plan_path_.string())
                    .ok());
    ASSERT_TRUE(project_.Open(project_path_.string()).ok());
    editors_ =
        std::make_unique<EditorSet>(nullptr, nullptr, nullptr, 1, nullptr);
    EditorDependencies dependencies;
    dependencies.project = &project_;
    editors_->ApplyDependencies(dependencies);
    editor_ = static_cast<PlanningEditor*>(
        editors_->GetEditor(EditorType::kPlanning));
    ASSERT_NE(editor_, nullptr);
    ASSERT_TRUE(editor_->Update().ok());
  }

  void TearDown() override {
    editors_.reset();
    std::error_code error;
    fs::remove_all(directory_, error);
  }

  fs::path directory_;
  fs::path project_path_;
  fs::path plan_path_;
  project::YazeProject project_;
  std::unique_ptr<EditorSet> editors_;
  PlanningEditor* editor_ = nullptr;
};

TEST_F(PlanningEditorTest, DraftQueryAndProjectSaveIncludePlanning) {
  EXPECT_FALSE(editors_->HasPendingProjectDraftChanges());
  PlanningEditorTestPeer::EditGoal(editor_, "Edited");
  ASSERT_TRUE(editors_->HasPendingProjectDraftChanges());
  ASSERT_TRUE(editors_->PrepareProjectSave().ok());
  EXPECT_FALSE(editor_->dirty());
  EXPECT_FALSE(editors_->HasPendingProjectDraftChanges());
  auto saved = core::ProjectPlan::LoadFromFile(plan_path_.string());
  ASSERT_TRUE(saved.ok());
  EXPECT_EQ(saved->goal.title, "Edited");
}

TEST_F(PlanningEditorTest, FailedSaveAndReloadKeepDraftAndRecoveryBytes) {
  PlanningEditorTestPeer::EditGoal(editor_, "Edited");
  Write(plan_path_, "external recovery content");
  EXPECT_FALSE(editors_->PrepareProjectSave().ok());
  EXPECT_TRUE(editor_->dirty());
  EXPECT_EQ(Read(plan_path_), "external recovery content");
  EXPECT_FALSE(PlanningEditorTestPeer::Reload(editor_).ok());
  EXPECT_EQ(editor_->plan()->goal.title, "Edited");
  EXPECT_FALSE(PlanningEditorTestPeer::Reload(editor_, true).ok());
  EXPECT_TRUE(editor_->dirty());
  EXPECT_EQ(editor_->plan()->goal.title, "Edited");
}

TEST_F(PlanningEditorTest, DirtyReloadRequiresExplicitDiscard) {
  PlanningEditorTestPeer::EditGoal(editor_, "Edited");
  EXPECT_FALSE(PlanningEditorTestPeer::Reload(editor_).ok());
  EXPECT_TRUE(editor_->dirty());
  ASSERT_TRUE(PlanningEditorTestPeer::Reload(editor_, true).ok());
  EXPECT_FALSE(editor_->dirty());
  EXPECT_TRUE(editor_->plan()->goal.title.empty());
}

TEST_F(PlanningEditorTest, BindingChangeCannotSilentlyDiscardOrSaveOldDraft) {
  PlanningEditorTestPeer::EditGoal(editor_, "Edited");
  const auto other = directory_ / "other.plan.json";
  ASSERT_TRUE(
      core::ProjectPlan::CreateEmpty("Other").SaveToFile(other.string()).ok());
  project_.planning.file = other.filename().string();
  EXPECT_FALSE(editor_->Update().ok());
  EXPECT_FALSE(editor_->Save().ok());
  EXPECT_TRUE(editor_->dirty());
  EXPECT_EQ(editor_->plan()->goal.title, "Edited");
  editors_->DiscardPlanningProjectDraft();
  EXPECT_FALSE(editor_->dirty());
  EXPECT_EQ(editor_->plan()->title, "Other");
}

TEST_F(PlanningEditorTest, ExplicitDiscardReloadsOriginalBinding) {
  PlanningEditorTestPeer::EditGoal(editor_, "Edited");
  editors_->DiscardPlanningProjectDraft();
  EXPECT_FALSE(editor_->dirty());
  EXPECT_TRUE(editor_->plan()->goal.title.empty());
}

class PlanningProjectActionsTest : public ::testing::Test {
 protected:
  void SetUp() override {
    imgui_ = ImGui::CreateContext();
    unsigned char* pixels = nullptr;
    int width = 0;
    int height = 0;
    ImGui::GetIO().Fonts->GetTexDataAsRGBA32(&pixels, &width, &height);
    renderer_ = std::make_unique<gfx::NullRenderer>();
    manager_ = std::make_unique<EditorManager>();
    test::InitializeWithIsolatedSettings(*manager_, renderer_.get());
    manager_->SetAssetLoadMode(AssetLoadMode::kLazy);
    directory_ = test::UniqueTempPath("planning_project_actions");
    fs::create_directories(directory_);
    const auto rom = directory_ / "fixture.sfc";
    std::vector<char> bytes(512 * 1024, 0);
    const std::string title = "PLANNING TEST";
    std::copy(title.begin(), title.end(), bytes.begin() + 0x7FC0);
    Write(rom, std::string(bytes.begin(), bytes.end()));
    plan_path_ = directory_ / "plan.json";
    ASSERT_TRUE(core::ProjectPlan::CreateEmpty("Original")
                    .SaveToFile(plan_path_.string())
                    .ok());
    const auto descriptor = directory_ / "hack.yaze";
    Write(descriptor,
          "[project]\nname=Plan\n[files]\nrom_filename=" + rom.string() +
              "\n[planning]\nfile=plan.json\n[workspace]\nautosave_enabled="
              "false\nbackup_on_save=false\n");
    ASSERT_TRUE(manager_->OpenRomOrProject(descriptor.string()).ok());
    editor_ = static_cast<PlanningEditor*>(
        manager_->GetCurrentEditorSet()->GetEditor(EditorType::kPlanning));
    ASSERT_TRUE(editor_->Update().ok());
    PlanningEditorTestPeer::EditGoal(editor_, "Edited");
  }
  void TearDown() override {
    manager_.reset();
    renderer_.reset();
    ImGui::DestroyContext(imgui_);
    std::error_code error;
    fs::remove_all(directory_, error);
  }
  ImGuiContext* imgui_ = nullptr;
  std::unique_ptr<gfx::NullRenderer> renderer_;
  std::unique_ptr<EditorManager> manager_;
  fs::path directory_;
  fs::path plan_path_;
  PlanningEditor* editor_ = nullptr;
};

TEST_F(PlanningProjectActionsTest,
       DirtyPlanGuardsQuitCloseAndProjectReplacement) {
  manager_->Quit();
  ASSERT_TRUE(manager_->HasPendingUnsavedSessionAction());
  EXPECT_FALSE(manager_->quit());
  manager_->CancelPendingUnsavedSessionAction();
  manager_->RemoveSession(manager_->GetCurrentSessionIndex());
  EXPECT_TRUE(manager_->HasPendingUnsavedSessionAction());
  manager_->CancelPendingUnsavedSessionAction();
  EXPECT_TRUE(
      manager_->OpenRomOrProject((directory_ / "missing.yaze").string()).ok());
  ASSERT_TRUE(manager_->HasPendingUnsavedSessionAction());
  manager_->ConfirmPendingUnsavedSessionActionDiscardAndContinue();
  EXPECT_TRUE(editor_->dirty());
  EXPECT_EQ(editor_->plan()->goal.title, "Edited");
}

TEST_F(PlanningProjectActionsTest, FailedPlanSavePreventsQuit) {
  Write(plan_path_, "external recovery content");
  manager_->Quit();
  ASSERT_TRUE(manager_->HasPendingUnsavedSessionAction());
  manager_->ConfirmPendingUnsavedSessionActionSaveAndContinue();
  EXPECT_FALSE(manager_->quit());
  EXPECT_TRUE(editor_->dirty());
  EXPECT_EQ(editor_->plan()->goal.title, "Edited");
  EXPECT_EQ(Read(plan_path_), "external recovery content");
}

TEST_F(PlanningProjectActionsTest, SaveAsRefusalPreservesOriginalPlanDraft) {
  const std::string original = Read(plan_path_);
  const auto destination = directory_ / "copy.yaze";
  const auto failed_destination = directory_ / "directory.yaze";
  fs::create_directory(failed_destination);
  for (const auto& path : {destination, failed_destination}) {
    const auto status = manager_->SaveProjectAs(path.string());
    EXPECT_EQ(status.code(), absl::StatusCode::kFailedPrecondition) << status;
    EXPECT_TRUE(editor_->dirty());
    EXPECT_EQ(editor_->plan()->goal.title, "Edited");
    EXPECT_EQ(Read(plan_path_), original);
  }
  EXPECT_FALSE(fs::exists(destination));
  EXPECT_TRUE(fs::is_empty(failed_destination));
  EXPECT_FALSE(manager_->SaveProjectAs("").ok());
  EXPECT_TRUE(editor_->dirty());
  EXPECT_EQ(Read(plan_path_), original);
}

TEST_F(PlanningProjectActionsTest, SaveAndContinuePersistsPlanBeforeQuit) {
  manager_->Quit();
  ASSERT_TRUE(manager_->HasPendingUnsavedSessionAction());
  manager_->ConfirmPendingUnsavedSessionActionSaveAndContinue();
  EXPECT_TRUE(manager_->quit());
  EXPECT_FALSE(editor_->dirty());
  auto saved = core::ProjectPlan::LoadFromFile(plan_path_.string());
  ASSERT_TRUE(saved.ok());
  EXPECT_EQ(saved->goal.title, "Edited");
}

}  // namespace
}  // namespace yaze::editor
