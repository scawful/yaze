#include "app/editor/planning/planning_editor.h"

#include <algorithm>
#include <cstdlib>
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
#include "core/project_plan_agent.h"
#include "core/source_artifact_publisher.h"
#include "editor_test_support.h"
#include "gtest/gtest.h"
#include "imgui/imgui.h"
#include "nlohmann/json.hpp"
#include "unique_temp_path.h"

namespace yaze::editor {

class PlanningEditorTestPeer {
 public:
  static void EditGoal(PlanningEditor* editor, const std::string& title) {
    editor->plan_->goal.title = title;
    editor->MarkDirty();
  }
  static void EditStepNote(PlanningEditor* editor, const std::string& note,
                           int index = 0) {
    editor->plan_->route_steps[index].note = note;
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

constexpr char kRoutePlan[] = R"({
 "schema": "yaze.plan/1",
 "title": "Plan",
 "route": {"steps": [{"id": "D4", "title": "Zora Temple", "note": "Old"}]},
 "decisions": [{"id": "d4-rescue", "status": "open", "answer": "Approved"}],
 "overworld": {"markers": [{"id": "m1"}]}
})";

absl::Status ApplyAgent(const fs::path& plan, const std::string& state,
                        const std::string& utc) {
  auto update = core::PlanAgentUpdate::Parse(
      R"({"schema": "yaze.plan.agent_update/1", "agent": "agent",
          "observed_utc": ")" +
      utc + R"(", "observations": {"step:D4": {"state": ")" + state +
      R"("}}})");
  if (!update.ok()) {
    return update.status();
  }
  update->base_sha256 = core::ComputeSourceArtifactSha256(Read(plan));
  return core::ApplyPlanAgentUpdateToFile(plan.string(), *update, false)
      .status();
}

TEST_F(PlanningEditorTest, SaveAdoptsConcurrentAgentUpdateAndFreshReopen) {
  Write(plan_path_, core::ProjectPlan::Parse(kRoutePlan)->Serialize());
  ASSERT_TRUE(PlanningEditorTestPeer::Reload(editor_, true).ok());
  PlanningEditorTestPeer::EditStepNote(editor_, "Annotated in yaze");
  ASSERT_TRUE(ApplyAgent(plan_path_, "DRAFT", "2026-10-27T21:00:00Z").ok());

  ASSERT_TRUE(editor_->Save().ok());
  EXPECT_FALSE(editor_->dirty());

  project::YazeProject reopened_project;
  ASSERT_TRUE(reopened_project.Open(project_path_.string()).ok());
  EditorSet reopened(nullptr, nullptr, nullptr, 2, nullptr);
  EditorDependencies dependencies;
  dependencies.project = &reopened_project;
  reopened.ApplyDependencies(dependencies);
  auto* fresh =
      static_cast<PlanningEditor*>(reopened.GetEditor(EditorType::kPlanning));
  ASSERT_TRUE(fresh->Update().ok());
  ASSERT_TRUE(fresh->plan().has_value());
  EXPECT_EQ(fresh->plan()->route_steps[0].note, "Annotated in yaze");
  EXPECT_EQ(fresh->plan()->decisions[0].answer, "Approved");
  const auto* obs = core::FindPlanAgentObservation(*fresh->plan(), "step:D4");
  ASSERT_NE(obs, nullptr);
  EXPECT_EQ(obs->at("state"), "DRAFT");

  ASSERT_TRUE(ApplyAgent(plan_path_, "MERGED", "2026-10-28T09:00:00Z").ok());
  auto saved = core::ProjectPlan::LoadFromFile(plan_path_.string());
  ASSERT_TRUE(saved.ok());
  EXPECT_EQ(saved->route_steps[0].note, "Annotated in yaze");
  EXPECT_EQ(saved->extra.at("overworld").at("markers")[0].at("id"), "m1");
  EXPECT_EQ(core::FindPlanAgentObservation(*saved, "step:D4")->at("state"),
            "MERGED");
}

TEST_F(PlanningEditorTest, SaveRefusesConcurrentHumanEditAndKeepsBoth) {
  Write(plan_path_, core::ProjectPlan::Parse(kRoutePlan)->Serialize());
  ASSERT_TRUE(PlanningEditorTestPeer::Reload(editor_, true).ok());
  PlanningEditorTestPeer::EditStepNote(editor_, "Mine");
  auto other = core::ProjectPlan::Parse(kRoutePlan);
  other->route_steps[0].note = "Theirs";
  Write(plan_path_, other->Serialize());
  const std::string disk = Read(plan_path_);

  const auto status = editor_->Save();
  EXPECT_EQ(status.code(), absl::StatusCode::kAborted) << status;
  EXPECT_TRUE(editor_->dirty());
  EXPECT_EQ(editor_->plan()->route_steps[0].note, "Mine");
  EXPECT_EQ(Read(plan_path_), disk);
}

// Copied-project probe: one process annotates a step through the normal
// project-save path, a later process reopens and checks it. Skipped unless
// YAZE_PLANNING_ROUNDTRIP_PROJECT, _STEP and _NOTE name a disposable copy.
struct CopiedProject {
  project::YazeProject project;
  std::unique_ptr<EditorSet> editors;
  PlanningEditor* editor = nullptr;
};

void OpenCopiedProject(CopiedProject* copied, const std::string& path) {
  ASSERT_TRUE(copied->project.Open(path).ok());
  copied->editors =
      std::make_unique<EditorSet>(nullptr, nullptr, nullptr, 1, nullptr);
  EditorDependencies dependencies;
  dependencies.project = &copied->project;
  copied->editors->ApplyDependencies(dependencies);
  copied->editor = static_cast<PlanningEditor*>(
      copied->editors->GetEditor(EditorType::kPlanning));
  ASSERT_TRUE(copied->editor->Update().ok());
  ASSERT_TRUE(copied->editor->plan().has_value());
}

TEST(PlanningCopiedProjectProbe, AnnotateStepThroughProjectSave) {
  const char* path = std::getenv("YAZE_PLANNING_ROUNDTRIP_PROJECT");
  const char* step = std::getenv("YAZE_PLANNING_ROUNDTRIP_STEP");
  const char* note = std::getenv("YAZE_PLANNING_ROUNDTRIP_NOTE");
  if (!path || !step || !note) {
    GTEST_SKIP() << "Set YAZE_PLANNING_ROUNDTRIP_{PROJECT,STEP,NOTE}";
  }
  CopiedProject copied;
  OpenCopiedProject(&copied, path);
  const int index = copied.editor->plan()->FindStep(step);
  ASSERT_GE(index, 0);
  PlanningEditorTestPeer::EditStepNote(copied.editor, note, index);
  ASSERT_TRUE(copied.editors->PrepareProjectSave().ok());
  EXPECT_FALSE(copied.editors->HasPendingProjectDraftChanges());
}

TEST(PlanningCopiedProjectProbe, FreshReopenShowsAnnotationAndAgentStatus) {
  const char* path = std::getenv("YAZE_PLANNING_ROUNDTRIP_PROJECT");
  const char* step = std::getenv("YAZE_PLANNING_ROUNDTRIP_STEP");
  const char* note = std::getenv("YAZE_PLANNING_ROUNDTRIP_NOTE");
  if (!path || !step || !note) {
    GTEST_SKIP() << "Set YAZE_PLANNING_ROUNDTRIP_{PROJECT,STEP,NOTE}";
  }
  CopiedProject copied;
  OpenCopiedProject(&copied, path);
  const auto& plan = *copied.editor->plan();
  const int index = plan.FindStep(step);
  ASSERT_GE(index, 0);
  EXPECT_EQ(plan.route_steps[index].note, note);
  EXPECT_NE(core::FindPlanAgentObservation(plan, std::string("step:") + step),
            nullptr);
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

// The plan is a live reference, not a build input: a Save with only plan edits
// writes only the plan. A Save that also changes project settings rewrites the
// descriptor and publishes a native receipt as usual.
TEST_F(PlanningProjectActionsTest,
       PlanOnlySaveLeavesDescriptorAndNativeReceiptUntouched) {
  const auto descriptor = directory_ / "hack.yaze";
  const auto receipt = fs::path(descriptor.string() + ".native-save.json");
  manager_->MarkCurrentProjectDirty();
  ASSERT_TRUE(manager_->SaveCurrentSession().ok());
  EXPECT_FALSE(editor_->dirty());
  EXPECT_EQ(core::ProjectPlan::LoadFromFile(plan_path_.string())->goal.title,
            "Edited");
  ASSERT_TRUE(fs::is_regular_file(receipt));
  const auto descriptor_bytes = Read(descriptor);
  const auto descriptor_time = fs::last_write_time(descriptor);
  const auto receipt_bytes = Read(receipt);

  PlanningEditorTestPeer::EditGoal(editor_, "Plan only");
  ASSERT_TRUE(manager_->SaveCurrentSession().ok());
  EXPECT_FALSE(editor_->dirty());
  EXPECT_EQ(core::ProjectPlan::LoadFromFile(plan_path_.string())->goal.title,
            "Plan only");
  EXPECT_EQ(Read(descriptor), descriptor_bytes);
  EXPECT_EQ(fs::last_write_time(descriptor), descriptor_time);
  EXPECT_EQ(Read(receipt), receipt_bytes);

  // A concurrent human edit still refuses the plan CAS; nothing else moves.
  PlanningEditorTestPeer::EditGoal(editor_, "Mine");
  const std::string theirs =
      core::ProjectPlan::CreateEmpty("Theirs").Serialize();
  Write(plan_path_, theirs);
  const auto refused = manager_->SaveCurrentSession();
  EXPECT_EQ(refused.code(), absl::StatusCode::kAborted) << refused;
  EXPECT_TRUE(editor_->dirty());
  EXPECT_EQ(Read(plan_path_), theirs);
  EXPECT_EQ(Read(descriptor), descriptor_bytes);
  EXPECT_EQ(Read(receipt), receipt_bytes);
  manager_->GetCurrentEditorSet()->DiscardPlanningProjectDraft();

  // Project settings plus a plan edit: both persist and the receipt advances.
  PlanningEditorTestPeer::EditGoal(editor_, "Dual");
  manager_->GetCurrentProject()->name = "Renamed with plan";
  manager_->MarkCurrentProjectDirty();
  ASSERT_TRUE(manager_->SaveCurrentSession().ok());
  EXPECT_FALSE(editor_->dirty());
  EXPECT_EQ(core::ProjectPlan::LoadFromFile(plan_path_.string())->goal.title,
            "Dual");
  EXPECT_NE(Read(descriptor), descriptor_bytes);
  const auto advanced = nlohmann::json::parse(Read(receipt));
  EXPECT_EQ(
      advanced.at("generation"),
      nlohmann::json::parse(receipt_bytes).at("generation").get<int>() + 1);
  EXPECT_TRUE(advanced.at("bindings_complete")) << advanced.dump(2);
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
