#include <gtest/gtest.h>

#include <chrono>
#include <filesystem>
#include <fstream>
#include <thread>

#include "app/editor/core/content_registry.h"
#include "app/editor/editor_manager.h"
#include "app/gfx/backend/null_renderer.h"
#include "core/source_artifact_publisher.h"
#include "editor_test_support.h"
#include "test_utils.h"
#include "testing.h"

#include "imgui/imgui.h"

namespace yaze::editor {
namespace {

std::filesystem::path MakeTempDir(const std::string& stem) {
  const auto base = std::filesystem::temp_directory_path();
  return base /
         (stem + "_" +
          std::to_string(::testing::UnitTest::GetInstance()->random_seed()));
}

struct ScopedTempDir {
  std::filesystem::path path;
  explicit ScopedTempDir(std::filesystem::path p) : path(std::move(p)) {
    std::filesystem::create_directories(path);
  }
  ~ScopedTempDir() {
    std::error_code ec;
    std::filesystem::remove_all(path, ec);
  }
};

struct ScopedImGuiContext {
  ImGuiContext* ctx = nullptr;
  ScopedImGuiContext() {
    ctx = ImGui::CreateContext();
    ImGui::SetCurrentContext(ctx);
    ImGuiIO& io = ImGui::GetIO();
    unsigned char* pixels = nullptr;
    int width = 0;
    int height = 0;
    io.Fonts->GetTexDataAsRGBA32(&pixels, &width, &height);
  }
  ~ScopedImGuiContext() {
    if (ctx) {
      ImGui::DestroyContext(ctx);
    }
  }
};

std::string NoOpBuildScript() {
#ifdef _WIN32
  return "cmd /c exit 0";
#else
  return "true";
#endif
}

void WriteRomFile(const std::filesystem::path& path, const std::string& tag) {
  std::ofstream stream(path, std::ios::binary);
  stream << tag;
}

TEST(ProjectBuildWorkflowStateTest, RunRequiresSuccessfulBuildIdentity) {
  ScopedImGuiContext imgui;
  auto renderer = std::make_unique<gfx::NullRenderer>();
  auto manager = std::make_unique<EditorManager>();
  test::InitializeWithIsolatedSettings(*manager, renderer.get());
  manager->SetAssetLoadMode(AssetLoadMode::kLazy);

  ScopedTempDir temp(MakeTempDir("yaze_build_run_identity"));
  const auto project_path = temp.path / "project.yaze";
  const auto source_rom = temp.path / "source.sfc";
  const auto output_rom = temp.path / "output.sfc";
  WriteRomFile(source_rom, "SOURCE");
  WriteRomFile(output_rom, "OUTPUT V1");

  ASSERT_OK(manager->OpenRomOrProject(source_rom.string()));
  auto* project = manager->GetCurrentProject();
  project->name = "BuildRunIdentity";
  project->filepath = project_path.string();
  project->build_target = output_rom.filename().string();
  project->build_script = NoOpBuildScript();

  const auto run_before = manager->RunCurrentProject();
  EXPECT_FALSE(run_before.ok());
  EXPECT_NE(run_before.message().find("Build the project"), std::string::npos);

  ASSERT_OK(manager->BuildCurrentProject());
  ASSERT_OK(manager->RunCurrentProject());

  {
    std::fstream rom(output_rom,
                     std::ios::in | std::ios::out | std::ios::binary);
    ASSERT_TRUE(rom.good());
    rom.seekp(0);
    rom.write("X", 1);
  }
  const auto run_after_change = manager->RunCurrentProject();
  EXPECT_FALSE(run_after_change.ok());
}

TEST(ProjectBuildWorkflowStateTest, OverlappingQueuedAndSyncBuildsRefused) {
  ScopedImGuiContext imgui;
  auto renderer = std::make_unique<gfx::NullRenderer>();
  auto manager = std::make_unique<EditorManager>();
  test::InitializeWithIsolatedSettings(*manager, renderer.get());
  manager->SetAssetLoadMode(AssetLoadMode::kLazy);

  ScopedTempDir temp(MakeTempDir("yaze_build_overlap"));
  const auto project_path = temp.path / "project.yaze";
  const auto source_rom = temp.path / "source.sfc";
  WriteRomFile(source_rom, "SOURCE");
  ASSERT_OK(manager->OpenRomOrProject(source_rom.string()));
  auto* project = manager->GetCurrentProject();
  project->name = "BuildOverlap";
  project->filepath = project_path.string();
#ifdef _WIN32
  project->build_script = "cmd /c ping -n 3 127.0.0.1 >nul";
#else
  project->build_script = "sleep 2";
#endif

  manager->QueueBuildCurrentProject();
  const auto sync = manager->BuildCurrentProject();
  EXPECT_FALSE(sync.ok());
  EXPECT_EQ(sync.code(), absl::StatusCode::kAlreadyExists);

  for (int attempt = 0; attempt < 200; ++attempt) {
    manager->PollProjectWorkflowTasks();
    if (!manager->GetCurrentProject()->filepath.empty()) {
      const auto status = ContentRegistry::Context::build_workflow_status();
      if (status.state != ProjectWorkflowState::kRunning) {
        break;
      }
    }
    std::this_thread::sleep_for(std::chrono::milliseconds(25));
  }
}

TEST(ProjectBuildWorkflowStateTest, RunWhileBuildRunningIsRefused) {
  ScopedImGuiContext imgui;
  auto renderer = std::make_unique<gfx::NullRenderer>();
  auto manager = std::make_unique<EditorManager>();
  test::InitializeWithIsolatedSettings(*manager, renderer.get());
  manager->SetAssetLoadMode(AssetLoadMode::kLazy);

  ScopedTempDir temp(MakeTempDir("yaze_run_during_build"));
  const auto project_path = temp.path / "project.yaze";
  const auto source_rom = temp.path / "source.sfc";
  const auto output_rom = temp.path / "output.sfc";
  WriteRomFile(source_rom, "SOURCE");
  WriteRomFile(output_rom, "OUTPUT");
  ASSERT_OK(manager->OpenRomOrProject(source_rom.string()));
  auto* project = manager->GetCurrentProject();
  project->name = "RunDuringBuild";
  project->filepath = project_path.string();
  project->build_target = output_rom.filename().string();
#ifdef _WIN32
  project->build_script = "cmd /c ping -n 3 127.0.0.1 >nul";
#else
  project->build_script = "sleep 2";
#endif

  manager->QueueBuildCurrentProject();
  const auto run = manager->RunCurrentProject();
  EXPECT_FALSE(run.ok());
  EXPECT_NE(run.message().find("Wait for the active project build"),
            std::string::npos);

  for (int attempt = 0; attempt < 200; ++attempt) {
    manager->PollProjectWorkflowTasks();
    const auto status = ContentRegistry::Context::build_workflow_status();
    if (status.state != ProjectWorkflowState::kRunning) {
      break;
    }
    std::this_thread::sleep_for(std::chrono::milliseconds(25));
  }
}

}  // namespace
}  // namespace yaze::editor
