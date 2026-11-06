#include <gtest/gtest.h>

#include <chrono>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <functional>
#include <memory>
#include <optional>
#include <thread>
#include <vector>

#include "app/editor/core/content_registry.h"
#include "app/editor/editor_manager.h"
#include "app/editor/system/session/session_coordinator.h"
#include "app/gfx/backend/null_renderer.h"
#include "core/native_save_receipt.h"
#include "core/project.h"
#include "core/source_artifact_publisher.h"
#include "editor_test_support.h"
#include "imgui/imgui.h"
#include "test_utils.h"
#include "testing.h"
#include "unique_temp_path.h"

namespace yaze::editor {
namespace {

void WriteRomFile(const std::filesystem::path& path,
                  const std::string& title = "YAZE TEST ROM") {
  std::vector<uint8_t> rom_data(512 * 1024, 0x00);
  for (size_t i = 0; i < title.size() && i < 21; ++i) {
    rom_data[0x7FC0 + i] = static_cast<uint8_t>(title[i]);
  }
  std::ofstream out(path, std::ios::binary | std::ios::trunc);
  ASSERT_TRUE(out.is_open());
  out.write(reinterpret_cast<const char*>(rom_data.data()),
            static_cast<std::streamsize>(rom_data.size()));
  ASSERT_TRUE(out.good());
}

std::string LoadedLoRomSha256(EditorManager& manager, size_t rom_size) {
  std::string bytes(rom_size, '\0');
  auto& memory = manager.emulator().snes().memory();
  for (size_t offset = 0; offset < bytes.size(); ++offset) {
    bytes[offset] = static_cast<char>(
        memory.cart_read(static_cast<uint8_t>(offset / 0x8000),
                         static_cast<uint16_t>(0x8000 | (offset & 0x7fff))));
  }
  return core::ComputeSourceArtifactSha256(bytes);
}

std::string DelayCommand() {
#ifdef _WIN32
  return "ping -n 3 127.0.0.1 >nul";
#else
  return "sleep 2";
#endif
}

std::string CopyCommand(const std::filesystem::path& source,
                        const std::filesystem::path& output) {
#ifdef _WIN32
  return "cmd /c copy /Y \"" + source.string() + "\" \"" + output.string() +
         "\" >nul";
#else
  return "cp \"" + source.string() + "\" \"" + output.string() + "\"";
#endif
}

int SetAppData(const std::string& value) {
#ifdef _WIN32
  return _putenv_s("YAZE_APP_DATA_DIR", value.c_str());
#else
  return setenv("YAZE_APP_DATA_DIR", value.c_str(), 1);
#endif
}

struct ProjectFiles {
  std::filesystem::path descriptor;
  std::filesystem::path source;
  std::filesystem::path output;
};

class ProjectBuildWorkflowStateTest : public ::testing::Test {
 protected:
  void SetUp() override {
    const auto unique = test::UniqueTempPath("yaze_build_workflow", "");
    temp_ =
        unique.parent_path() /
        ("yaze_bwf_" +
         std::to_string(std::hash<std::string>{}(unique.filename().string())));
    std::filesystem::create_directories(temp_);
    const char* previous = std::getenv("YAZE_APP_DATA_DIR");
    if (previous != nullptr) {
      previous_app_data_ = previous;
    }
    ASSERT_EQ(SetAppData((temp_ / "app-data").string()), 0);
    imgui_ = ImGui::CreateContext();
    ImGui::SetCurrentContext(imgui_);
    unsigned char* pixels = nullptr;
    int width = 0;
    int height = 0;
    ImGui::GetIO().Fonts->GetTexDataAsRGBA32(&pixels, &width, &height);
    renderer_ = std::make_unique<gfx::NullRenderer>();
    manager_ = std::make_unique<EditorManager>();
    test::InitializeWithIsolatedSettings(*manager_, renderer_.get());
    manager_->SetAssetLoadMode(AssetLoadMode::kLazy);
  }

  void TearDown() override {
    // Join/cancel owned children before removing their inputs or app-data root.
    manager_.reset();
    renderer_.reset();
    if (imgui_ != nullptr) {
      ImGui::DestroyContext(imgui_);
    }
    if (previous_app_data_.has_value()) {
      EXPECT_EQ(SetAppData(*previous_app_data_), 0);
    } else {
#ifdef _WIN32
      EXPECT_EQ(_putenv_s("YAZE_APP_DATA_DIR", ""), 0);
#else
      EXPECT_EQ(unsetenv("YAZE_APP_DATA_DIR"), 0);
#endif
    }
    std::error_code ec;
    std::filesystem::remove_all(temp_, ec);
  }

  ProjectFiles CreateProject(const std::string& name, bool delayed = false,
                             bool output_configured = true) {
    const auto root = temp_ / name;
    std::filesystem::create_directories(root);
    ProjectFiles files{root / "project.yaze", root / "source.sfc",
                       root / "output.sfc"};
    WriteRomFile(files.source, name);
    WriteRomFile(files.output, "STALE OUTPUT");
    project::YazeProject project;
    project.name = name;
    project.metadata.project_id = "workflow_" + name;
    project.filepath = files.descriptor.string();
    project.rom_filename = files.source.string();
    project.build_target =
        output_configured ? files.output.filename().string() : "";
    project.build_script = output_configured
                               ? CopyCommand(files.source, files.output)
                               : DelayCommand();
    if (delayed && output_configured) {
      project.build_script = DelayCommand() + " && " + project.build_script;
    }
    EXPECT_OK(project.Save());
    project::YazeProject saved;
    EXPECT_OK(saved.Open(files.descriptor.string()));
    EXPECT_EQ(std::filesystem::path(saved.rom_filename), files.source);
    return files;
  }

  absl::Status OpenProject(const ProjectFiles& files) {
    project::YazeProject saved;
    const auto status = saved.Open(files.descriptor.string());
    if (!status.ok()) {
      return status;
    }
    if (saved.rom_filename.empty() ||
        !std::filesystem::is_regular_file(saved.rom_filename)) {
      return absl::FailedPreconditionError(
          "Test project must have an existing ROM; never open a file picker");
    }
    return manager_->OpenRomOrProject(files.descriptor.string());
  }

  bool PumpUntil(const std::function<bool()>& finished) {
    const auto deadline =
        std::chrono::steady_clock::now() + std::chrono::seconds(10);
    do {
      ImGui::GetIO().DisplaySize = ImVec2(1280, 720);
      ImGui::NewFrame();
      const auto status = manager_->Update();
      ImGui::Render();
      if (!status.ok()) {
        ADD_FAILURE() << status;
        return false;
      }
      if (finished()) {
        return true;
      }
      std::this_thread::sleep_for(std::chrono::milliseconds(25));
    } while (std::chrono::steady_clock::now() < deadline);
    ADD_FAILURE() << "Timed out waiting for project workflow: "
                  << ContentRegistry::Context::build_workflow_status().summary
                  << ": "
                  << ContentRegistry::Context::build_workflow_status().detail;
    return false;
  }

  bool WaitFor(ProjectWorkflowState state) {
    return PumpUntil([state] {
      return ContentRegistry::Context::build_workflow_status().state == state;
    });
  }

  bool WaitForCopiedOutput(const ProjectFiles& files) {
    return PumpUntil([&files] {
      const auto source = core::ComputeSourceArtifactFileSha256(files.source);
      const auto output = core::ComputeSourceArtifactFileSha256(files.output);
      return source.ok() && output.ok() && *source == *output;
    });
  }

  std::string EditingSource() {
    const auto* session =
        manager_->session_coordinator()->GetActiveRomSession();
    return session != nullptr && session->rom_baseline.has_value()
               ? session->rom_baseline->editing_source_path
               : "";
  }

  std::filesystem::path temp_;
  std::optional<std::string> previous_app_data_;
  ImGuiContext* imgui_ = nullptr;
  std::unique_ptr<gfx::NullRenderer> renderer_;
  std::unique_ptr<EditorManager> manager_;
};

TEST_F(ProjectBuildWorkflowStateTest, RunRequiresSuccessfulBuildIdentity) {
  const auto files = CreateProject("BuildRunIdentity");
  ASSERT_OK(OpenProject(files));
  EXPECT_FALSE(manager_->RunCurrentProject().ok());
  ASSERT_OK(manager_->BuildCurrentProject());
  const auto hash = core::ComputeSourceArtifactFileSha256(files.output);
  ASSERT_TRUE(hash.ok());
  ASSERT_OK(manager_->RunCurrentProject());
  EXPECT_EQ(
      LoadedLoRomSha256(*manager_, std::filesystem::file_size(files.output)),
      *hash);
  WriteRomFile(files.output, "CHANGED OUTPUT");
  EXPECT_FALSE(manager_->RunCurrentProject().ok());
}

TEST_F(ProjectBuildWorkflowStateTest, NoOpBuildDoesNotAdoptPreexistingOutput) {
  const auto files = CreateProject("NoOpOutput");
  project::YazeProject project;
  ASSERT_OK(project.Open(files.descriptor.string()));
#ifdef _WIN32
  project.build_script = "cmd /c exit 0";
#else
  project.build_script = "true";
#endif
  ASSERT_OK(project.Save());
  ASSERT_OK(OpenProject(files));
  const auto status = manager_->BuildCurrentProject();
  EXPECT_FALSE(status.ok());
  EXPECT_NE(status.message().find("unchanged output"), std::string::npos);
  EXPECT_FALSE(manager_->RunCurrentProject().ok());
}

TEST_F(ProjectBuildWorkflowStateTest, DeterministicRepeatBuildRetainsProof) {
  const auto files = CreateProject("RepeatBuild");
  ASSERT_OK(OpenProject(files));
  ASSERT_OK(manager_->BuildCurrentProject());
  const auto first = core::ComputeSourceArtifactFileSha256(files.output);
  ASSERT_TRUE(first.ok());
  ASSERT_OK(manager_->BuildCurrentProject());
  const auto second = core::ComputeSourceArtifactFileSha256(files.output);
  ASSERT_TRUE(second.ok());
  EXPECT_EQ(*first, *second);
  ASSERT_OK(manager_->RunCurrentProject());
}

TEST_F(ProjectBuildWorkflowStateTest, OverlappingQueuedAndSyncBuildsRefused) {
  const auto files = CreateProject("BuildOverlap", false, false);
  ASSERT_OK(OpenProject(files));
  manager_->QueueBuildCurrentProject();
  EXPECT_EQ(manager_->BuildCurrentProject().code(),
            absl::StatusCode::kAlreadyExists);
  ASSERT_TRUE(WaitFor(ProjectWorkflowState::kSuccess));
}

TEST_F(ProjectBuildWorkflowStateTest, RunWhileBuildRunningIsRefused) {
  const auto files = CreateProject("RunDuringBuild", true);
  ASSERT_OK(OpenProject(files));
  manager_->QueueBuildCurrentProject();
  EXPECT_FALSE(manager_->RunCurrentProject().ok());
  ASSERT_TRUE(WaitFor(ProjectWorkflowState::kSuccess));
  ASSERT_OK(manager_->RunCurrentProject());
}

TEST_F(ProjectBuildWorkflowStateTest, FailedBuildRevokesPriorRunEligibility) {
  const auto files = CreateProject("FailedRevokesRun");
  ASSERT_OK(OpenProject(files));
  ASSERT_OK(manager_->BuildCurrentProject());
  const auto marker = temp_ / "failed-child-started.txt";
#ifdef _WIN32
  manager_->GetCurrentProject()->build_script =
      "cmd /c \"echo started>\"" + marker.string() + "\" & exit /b 7\"";
#else
  manager_->GetCurrentProject()->build_script =
      "echo started > \"" + marker.string() + "\"; exit 7";
#endif
  manager_->MarkCurrentProjectDirty();
  ASSERT_OK(manager_->SaveProject());
  EXPECT_FALSE(manager_->BuildCurrentProject().ok());
  EXPECT_TRUE(std::filesystem::exists(marker));
  EXPECT_TRUE(std::filesystem::exists(files.output));
  EXPECT_FALSE(manager_->RunCurrentProject().ok());
}

TEST_F(ProjectBuildWorkflowStateTest, RunRefusesDirtyRomWithoutSavedChanges) {
  const auto files = CreateProject("DirtyRun");
  ASSERT_OK(OpenProject(files));
  ASSERT_OK(manager_->BuildCurrentProject());
  ASSERT_OK(manager_->GetCurrentRom()->WriteByte(0, 0x01));
  EXPECT_FALSE(manager_->RunCurrentProject().ok());
}

TEST_F(ProjectBuildWorkflowStateTest, ForeignViewerCannotCancelOrAdoptBuild) {
  const auto a = CreateProject("ProjectA", true);
  const auto b = CreateProject("ProjectB");
  ASSERT_OK(OpenProject(a));
  const auto owner_id = manager_->GetCurrentSessionId();
  const auto owner_index = manager_->GetCurrentSessionIndex();
  manager_->QueueBuildCurrentProject();
  ASSERT_OK(OpenProject(b));
  ASSERT_NE(manager_->GetCurrentSessionId(), owner_id);
  manager_->CancelQueuedProjectBuild();
  EXPECT_FALSE(manager_->RunCurrentProject().ok());
  ASSERT_TRUE(WaitForCopiedOutput(a));
  EXPECT_NE(ContentRegistry::Context::build_workflow_status().state,
            ProjectWorkflowState::kSuccess);
  manager_->SwitchToSession(owner_index);
  ASSERT_TRUE(WaitFor(ProjectWorkflowState::kSuccess));
  ASSERT_OK(manager_->RunCurrentProject());
}

TEST_F(ProjectBuildWorkflowStateTest,
       OwnerCompletionSurvivesAnotherProjectBuild) {
  const auto a = CreateProject("HistoryOwnerA", true);
  const auto b = CreateProject("HistoryOwnerB", true);
  ASSERT_OK(OpenProject(a));
  const auto a_index = manager_->GetCurrentSessionIndex();
  manager_->QueueBuildCurrentProject();
  ASSERT_OK(OpenProject(b));
  const auto b_index = manager_->GetCurrentSessionIndex();
  ASSERT_TRUE(PumpUntil([] {
    return ContentRegistry::Context::build_workflow_status().state ==
           ProjectWorkflowState::kIdle;
  }));
  EXPECT_TRUE(ContentRegistry::Context::workflow_history().empty());
  manager_->QueueBuildCurrentProject();
  manager_->SwitchToSession(a_index);
  ASSERT_TRUE(PumpUntil(
      [] { return ContentRegistry::Context::workflow_history().size() == 1; }));
  EXPECT_EQ(ContentRegistry::Context::workflow_history()[0].status.state,
            ProjectWorkflowState::kSuccess);
  ASSERT_TRUE(WaitForCopiedOutput(b));
  manager_->SwitchToSession(b_index);
  ASSERT_TRUE(PumpUntil(
      [] { return ContentRegistry::Context::workflow_history().size() == 2; }));
  EXPECT_EQ(ContentRegistry::Context::workflow_history()[1].status.state,
            ProjectWorkflowState::kSuccess);
  // Another idle frame must not deliver either terminal event twice.
  ASSERT_TRUE(PumpUntil([] { return true; }));
  EXPECT_EQ(ContentRegistry::Context::workflow_history().size(), 2u);
}

TEST_F(ProjectBuildWorkflowStateTest, ClosedOwnerCannotTransferResultToReopen) {
  const auto a = CreateProject("ClosedOwner", true);
  const auto b = CreateProject("RemainingViewer");
  ASSERT_OK(OpenProject(a));
  const auto owner_id = manager_->GetCurrentSessionId();
  const auto owner_index = manager_->GetCurrentSessionIndex();
  manager_->QueueBuildCurrentProject();
  ASSERT_OK(OpenProject(b));
  manager_->session_coordinator()->CloseSession(owner_index);
  ASSERT_TRUE(WaitForCopiedOutput(a));
  ASSERT_OK(OpenProject(a));
  EXPECT_NE(manager_->GetCurrentSessionId(), owner_id);
  EXPECT_FALSE(manager_->RunCurrentProject().ok());
  EXPECT_TRUE(std::filesystem::exists(a.output));
}

TEST_F(ProjectBuildWorkflowStateTest, OwnerCancelRevokesRunAndRetainsOutput) {
  const auto files = CreateProject("OwnerCancel");
  ASSERT_OK(OpenProject(files));
  ASSERT_OK(manager_->BuildCurrentProject());
  manager_->GetCurrentProject()->build_script =
      DelayCommand() + " && " + CopyCommand(files.source, files.output);
  manager_->MarkCurrentProjectDirty();
  ASSERT_OK(manager_->SaveProject());
  manager_->QueueBuildCurrentProject();
  ASSERT_EQ(ContentRegistry::Context::build_workflow_status().state,
            ProjectWorkflowState::kRunning);
  manager_->CancelQueuedProjectBuild();
  ASSERT_TRUE(WaitFor(ProjectWorkflowState::kFailure));
  EXPECT_TRUE(std::filesystem::exists(files.output));
  EXPECT_FALSE(manager_->RunCurrentProject().ok());
}

TEST_F(ProjectBuildWorkflowStateTest, ChangedSourceDuringBuildFailsCompletion) {
  const auto files = CreateProject("InputChanged", true);
  ASSERT_OK(OpenProject(files));
  manager_->QueueBuildCurrentProject();
  WriteRomFile(files.source, "CHANGED SOURCE");
  ASSERT_TRUE(WaitFor(ProjectWorkflowState::kFailure));
  EXPECT_FALSE(manager_->RunCurrentProject().ok());
}

TEST_F(ProjectBuildWorkflowStateTest, MissingConfiguredSourceRefusesBuild) {
  const auto files = CreateProject("MissingSource");
  ASSERT_OK(OpenProject(files));
  std::filesystem::rename(files.source, files.source.string() + ".retained");
  EXPECT_FALSE(manager_->BuildCurrentProject().ok());
  EXPECT_FALSE(manager_->RunCurrentProject().ok());
}

TEST_F(ProjectBuildWorkflowStateTest, NewNativeSaveGenerationFailsCompletion) {
  const auto files = CreateProject("NativeGeneration", true);
  ASSERT_OK(OpenProject(files));
  ASSERT_OK(manager_->SaveProject());
  const auto first = core::ReadVerifiedCommittedNativeSaveIdentity(
      EditingSource(), files.descriptor.string());
  ASSERT_TRUE(first.ok()) << first.status();
  manager_->QueueBuildCurrentProject();
  ASSERT_EQ(ContentRegistry::Context::build_workflow_status().state,
            ProjectWorkflowState::kRunning);
  ASSERT_OK(manager_->SaveProject());
  const auto second = core::ReadVerifiedCommittedNativeSaveIdentity(
      EditingSource(), files.descriptor.string());
  ASSERT_TRUE(second.ok()) << second.status();
  ASSERT_GT(second->generation, first->generation);
  ASSERT_TRUE(WaitFor(ProjectWorkflowState::kFailure));
  EXPECT_FALSE(manager_->RunCurrentProject().ok());
}

TEST_F(ProjectBuildWorkflowStateTest, ReboundNativeContextCannotBecomeGeneric) {
  const auto files = CreateProject("ReboundNative");
  ASSERT_OK(OpenProject(files));
  ASSERT_OK(manager_->SaveProject());
  const auto context =
      core::GetNativeSaveContext(EditingSource(), files.descriptor.string());
  ASSERT_TRUE(context.ok()) << context.status();
  ASSERT_OK(core::RetireNativeSaveContext(*context, "rebound"));
  EXPECT_FALSE(manager_->BuildCurrentProject().ok());
  EXPECT_FALSE(manager_->RunCurrentProject().ok());
}

TEST_F(ProjectBuildWorkflowStateTest,
       MalformedNativeContextCannotBecomeGeneric) {
  const auto files = CreateProject("MalformedNative");
  ASSERT_OK(OpenProject(files));
  ASSERT_OK(manager_->SaveProject());
  const auto context =
      core::GetNativeSaveContext(EditingSource(), files.descriptor.string());
  ASSERT_TRUE(context.ok()) << context.status();
  {
    std::ofstream out(context->path, std::ios::binary | std::ios::trunc);
    out << "{broken-context";
    ASSERT_TRUE(out.good());
  }
  EXPECT_FALSE(manager_->BuildCurrentProject().ok());
  EXPECT_FALSE(manager_->RunCurrentProject().ok());
}

}  // namespace
}  // namespace yaze::editor
