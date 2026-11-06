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

void WriteRomFile(const std::filesystem::path& path,
                  const std::string& title = "YAZE TEST ROM") {
  std::vector<uint8_t> rom_data(512 * 1024, 0x00);
  for (size_t i = 0; i < title.size() && (0x7FC0 + i) < rom_data.size(); ++i) {
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

std::string CopyOutputBuildScript(const std::filesystem::path& source,
                                  const std::filesystem::path& output) {
#ifdef _WIN32
  return "cmd /c copy /Y \"" + source.string() + "\" \"" + output.string() +
         "\" >nul";
#else
  return "cp \"" + source.string() + "\" \"" + output.string() + "\"";
#endif
}

void PumpWorkflowUntil(EditorManager& manager, ProjectWorkflowState target,
                       int max_attempts = 200) {
  for (int attempt = 0; attempt < max_attempts; ++attempt) {
    ImGui::GetIO().DisplaySize = ImVec2(1280, 720);
    ImGui::NewFrame();
    ASSERT_OK(manager.Update());
    ImGui::Render();
    if (ContentRegistry::Context::build_workflow_status().state == target) {
      return;
    }
    std::this_thread::sleep_for(std::chrono::milliseconds(25));
  }
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
  WriteRomFile(source_rom, "BUILD RUN SOURCE");
  WriteRomFile(output_rom, "STALE OUTPUT");

  ASSERT_OK(manager->OpenRomOrProject(source_rom.string()));
  auto* project = manager->GetCurrentProject();
  project->name = "BuildRunIdentity";
  project->filepath = project_path.string();
  project->build_target = output_rom.filename().string();
  project->build_script = CopyOutputBuildScript(source_rom, output_rom);
  ASSERT_OK(project->Save());

  const auto run_before = manager->RunCurrentProject();
  EXPECT_FALSE(run_before.ok());
  EXPECT_NE(run_before.message().find("Build the project"), std::string::npos);

  ASSERT_OK(manager->BuildCurrentProject());
  const auto output_hash = core::ComputeSourceArtifactFileSha256(output_rom);
  ASSERT_TRUE(output_hash.ok());
  ASSERT_OK(manager->RunCurrentProject());
  EXPECT_EQ(LoadedLoRomSha256(*manager, std::filesystem::file_size(output_rom)),
            *output_hash);

  {
    std::fstream rom(output_rom,
                     std::ios::in | std::ios::out | std::ios::binary);
    ASSERT_TRUE(rom.good());
    rom.seekp(0);
    rom.put(static_cast<char>(0xAB));
  }
  const auto run_after_change = manager->RunCurrentProject();
  EXPECT_FALSE(run_after_change.ok());
}

TEST(ProjectBuildWorkflowStateTest, NoOpBuildDoesNotAdoptPreexistingOutput) {
  ScopedImGuiContext imgui;
  auto renderer = std::make_unique<gfx::NullRenderer>();
  auto manager = std::make_unique<EditorManager>();
  test::InitializeWithIsolatedSettings(*manager, renderer.get());
  manager->SetAssetLoadMode(AssetLoadMode::kLazy);

  ScopedTempDir temp(MakeTempDir("yaze_build_noop_output"));
  const auto project_path = temp.path / "project.yaze";
  const auto source_rom = temp.path / "source.sfc";
  const auto output_rom = temp.path / "output.sfc";
  WriteRomFile(source_rom, "SOURCE");
  WriteRomFile(output_rom, "LEFTOVER OUTPUT");

  ASSERT_OK(manager->OpenRomOrProject(source_rom.string()));
  auto* project = manager->GetCurrentProject();
  project->name = "NoOpOutput";
  project->filepath = project_path.string();
  project->build_target = output_rom.filename().string();
#ifdef _WIN32
  project->build_script = "cmd /c exit 0";
#else
  project->build_script = "true";
#endif
  ASSERT_OK(project->Save());

  const auto build = manager->BuildCurrentProject();
  EXPECT_FALSE(build.ok());
  EXPECT_NE(build.message().find("refresh"), std::string::npos);
  EXPECT_FALSE(manager->RunCurrentProject().ok());
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
  WriteRomFile(source_rom, "OVERLAP SOURCE");
  ASSERT_OK(manager->OpenRomOrProject(source_rom.string()));
  auto* project = manager->GetCurrentProject();
  project->name = "BuildOverlap";
  project->filepath = project_path.string();
#ifdef _WIN32
  project->build_script = "cmd /c ping -n 3 127.0.0.1 >nul";
#else
  project->build_script = "sleep 2";
#endif
  ASSERT_OK(project->Save());

  manager->QueueBuildCurrentProject();
  const auto sync = manager->BuildCurrentProject();
  EXPECT_FALSE(sync.ok());
  EXPECT_EQ(sync.code(), absl::StatusCode::kAlreadyExists);

  PumpWorkflowUntil(*manager, ProjectWorkflowState::kSuccess);
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
  WriteRomFile(source_rom, "RUN DURING BUILD");
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
  ASSERT_OK(project->Save());

  manager->QueueBuildCurrentProject();
  const auto run = manager->RunCurrentProject();
  EXPECT_FALSE(run.ok());
  EXPECT_NE(run.message().find("Wait for the active project build"),
            std::string::npos);

  PumpWorkflowUntil(*manager, ProjectWorkflowState::kSuccess);
}

TEST(ProjectBuildWorkflowStateTest, FailedBuildRevokesPriorRunEligibility) {
  ScopedImGuiContext imgui;
  auto renderer = std::make_unique<gfx::NullRenderer>();
  auto manager = std::make_unique<EditorManager>();
  test::InitializeWithIsolatedSettings(*manager, renderer.get());
  manager->SetAssetLoadMode(AssetLoadMode::kLazy);

  ScopedTempDir temp(MakeTempDir("yaze_failed_revokes_run"));
  const auto project_path = temp.path / "project.yaze";
  const auto source_rom = temp.path / "source.sfc";
  const auto output_rom = temp.path / "output.sfc";
  WriteRomFile(source_rom, "REVOKE SOURCE");
  WriteRomFile(output_rom, "STALE");

  ASSERT_OK(manager->OpenRomOrProject(source_rom.string()));
  auto* project = manager->GetCurrentProject();
  project->name = "RevokeRun";
  project->filepath = project_path.string();
  project->build_target = output_rom.filename().string();
  project->build_script = CopyOutputBuildScript(source_rom, output_rom);
  ASSERT_OK(project->Save());

  ASSERT_OK(manager->BuildCurrentProject());
  ASSERT_OK(manager->RunCurrentProject());

#ifdef _WIN32
  project->build_script = "cmd /c exit 1";
#else
  project->build_script = "false";
#endif
  ASSERT_OK(project->Save());
  EXPECT_FALSE(manager->BuildCurrentProject().ok());
  EXPECT_FALSE(manager->RunCurrentProject().ok());
}

TEST(ProjectBuildWorkflowStateTest, RunRefusesDirtyRomWithoutSavedChanges) {
  ScopedImGuiContext imgui;
  auto renderer = std::make_unique<gfx::NullRenderer>();
  auto manager = std::make_unique<EditorManager>();
  test::InitializeWithIsolatedSettings(*manager, renderer.get());
  manager->SetAssetLoadMode(AssetLoadMode::kLazy);

  ScopedTempDir temp(MakeTempDir("yaze_run_dirty_rom"));
  const auto project_path = temp.path / "project.yaze";
  const auto source_rom = temp.path / "source.sfc";
  const auto output_rom = temp.path / "output.sfc";
  WriteRomFile(source_rom, "DIRTY RUN SOURCE");
  WriteRomFile(output_rom, "OUTPUT");

  ASSERT_OK(manager->OpenRomOrProject(source_rom.string()));
  auto* project = manager->GetCurrentProject();
  project->name = "DirtyRun";
  project->filepath = project_path.string();
  project->build_target = output_rom.filename().string();
  project->build_script = CopyOutputBuildScript(source_rom, output_rom);
  ASSERT_OK(project->Save());

  ASSERT_OK(manager->BuildCurrentProject());
  ASSERT_OK(manager->GetCurrentRom()->WriteByte(0, 0x01));
  const auto run = manager->RunCurrentProject();
  EXPECT_FALSE(run.ok());
  EXPECT_NE(run.message().find("Save project and ROM changes"),
            std::string::npos);
}

}  // namespace
}  // namespace yaze::editor
