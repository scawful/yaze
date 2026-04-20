#include <gtest/gtest.h>

#include <chrono>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <string>
#include <thread>
#include <vector>

#include "absl/status/status.h"
#include "app/editor/core/content_registry.h"
#include "app/editor/editor_manager.h"
#include "app/gfx/backend/null_renderer.h"
#include "testing.h"

#include "imgui/imgui.h"

namespace yaze::editor {
namespace {

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

std::filesystem::path MakeTempDir(const std::string& stem) {
  const auto nonce = static_cast<uint64_t>(
      std::chrono::high_resolution_clock::now().time_since_epoch().count());
  return std::filesystem::temp_directory_path() /
         (stem + "_" + std::to_string(nonce));
}

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

std::string ReadFile(const std::filesystem::path& path) {
  std::ifstream in(path);
  EXPECT_TRUE(in.is_open());
  return std::string((std::istreambuf_iterator<char>(in)),
                     std::istreambuf_iterator<char>());
}

TEST(EditorManagerProjectActionsTest,
     BuildCurrentProjectUsesProjectBuildScript) {
#ifdef _WIN32
  GTEST_SKIP() << "Test uses #!/bin/sh shebang + chmod exec, POSIX-only";
#else
  ScopedImGuiContext imgui;
  auto renderer = std::make_unique<gfx::NullRenderer>();
  auto manager = std::make_unique<EditorManager>();
  manager->Initialize(renderer.get(), "");
  manager->SetAssetLoadMode(AssetLoadMode::kLazy);

  ScopedTempDir temp_dir(MakeTempDir("yaze_build_project_script"));
  const auto project_path = temp_dir.path / "project.yaze";
  const auto marker_path = temp_dir.path / "build.marker";
  const auto script_path = temp_dir.path / "build_ok.sh";

  {
    std::ofstream script(script_path);
    ASSERT_TRUE(script.is_open());
    script << "#!/bin/sh\n";
    script << "printf 'project-build' > build.marker\n";
  }
  std::filesystem::permissions(script_path,
                               std::filesystem::perms::owner_exec |
                                   std::filesystem::perms::owner_read |
                                   std::filesystem::perms::owner_write,
                               std::filesystem::perm_options::add);

  auto* project = manager->GetCurrentProject();
  ASSERT_NE(project, nullptr);
  project->name = "BuildProjectScript";
  project->filepath = project_path.string();
  project->build_script = "./build_ok.sh";

  ASSERT_OK(manager->BuildCurrentProject());
  ASSERT_TRUE(std::filesystem::exists(marker_path));
  EXPECT_EQ(ReadFile(marker_path), "project-build");
#endif  // _WIN32
}

TEST(EditorManagerProjectActionsTest,
     BuildCurrentProjectFallsBackToManifestBuildScript) {
#ifdef _WIN32
  GTEST_SKIP() << "Test uses #!/bin/sh shebang + chmod exec, POSIX-only";
#else
  ScopedImGuiContext imgui;
  auto renderer = std::make_unique<gfx::NullRenderer>();
  auto manager = std::make_unique<EditorManager>();
  manager->Initialize(renderer.get(), "");
  manager->SetAssetLoadMode(AssetLoadMode::kLazy);

  ScopedTempDir temp_dir(MakeTempDir("yaze_build_manifest_script"));
  const auto project_path = temp_dir.path / "project.yaze";
  const auto marker_path = temp_dir.path / "manifest.marker";
  const auto script_path = temp_dir.path / "manifest_build.sh";

  {
    std::ofstream script(script_path);
    ASSERT_TRUE(script.is_open());
    script << "#!/bin/sh\n";
    script << "printf 'manifest-build' > manifest.marker\n";
  }
  std::filesystem::permissions(script_path,
                               std::filesystem::perms::owner_exec |
                                   std::filesystem::perms::owner_read |
                                   std::filesystem::perms::owner_write,
                               std::filesystem::perm_options::add);

  auto* project = manager->GetCurrentProject();
  ASSERT_NE(project, nullptr);
  project->name = "BuildManifestScript";
  project->filepath = project_path.string();
  project->build_script.clear();
  ASSERT_TRUE(project->hack_manifest
                  .LoadFromString(R"json(
{
  "manifest_version": 2,
  "hack_name": "Generic Hack",
  "build_pipeline": {
    "build_script": "./manifest_build.sh"
  }
}
)json")
                  .ok());

  ASSERT_OK(manager->BuildCurrentProject());
  ASSERT_TRUE(std::filesystem::exists(marker_path));
  EXPECT_EQ(ReadFile(marker_path), "manifest-build");
#endif  // _WIN32
}

TEST(EditorManagerProjectActionsTest,
     RunCurrentProjectPrefersManifestPatchedRomAndReloadsEmulator) {
#ifdef _WIN32
  GTEST_SKIP() << "Test uses #!/bin/sh shebang + chmod exec, POSIX-only";
#else
  ScopedImGuiContext imgui;
  auto renderer = std::make_unique<gfx::NullRenderer>();
  auto manager = std::make_unique<EditorManager>();
  manager->Initialize(renderer.get(), "");
  manager->SetAssetLoadMode(AssetLoadMode::kLazy);

  ScopedTempDir temp_dir(MakeTempDir("yaze_run_project_output"));
  const auto project_path = temp_dir.path / "project.yaze";
  const auto valid_rom = temp_dir.path / "patched_valid.sfc";
  WriteRomFile(valid_rom, "RUN TARGET ROM");

  auto* project = manager->GetCurrentProject();
  ASSERT_NE(project, nullptr);
  project->name = "RunProjectOutput";
  project->filepath = project_path.string();
  project->build_target = "missing_output.sfc";
  ASSERT_TRUE(project->hack_manifest
                  .LoadFromString(R"json(
{
  "manifest_version": 2,
  "hack_name": "Generic Hack",
  "build_pipeline": {
    "patched_rom": "patched_valid.sfc"
  }
}
)json")
                  .ok());

  ASSERT_OK(manager->RunCurrentProject());
  EXPECT_TRUE(manager->emulator().is_snes_initialized());
  EXPECT_TRUE(manager->emulator().running());
  ASSERT_NE(manager->ui_coordinator(), nullptr);
  EXPECT_TRUE(manager->ui_coordinator()->IsEmulatorVisible());

  const auto& history = manager->toast_manager()->GetHistory();
  ASSERT_FALSE(history.empty());
  EXPECT_NE(history.front().message.find("patched_valid.sfc"),
            std::string::npos);
#endif  // _WIN32
}

TEST(EditorManagerProjectActionsTest,
     WorkflowCallbacksTriggerBuildRunAndOpenOutputPanel) {
#ifdef _WIN32
  GTEST_SKIP() << "Test uses #!/bin/sh shebang + chmod exec, POSIX-only";
#else
  ScopedImGuiContext imgui;
  auto renderer = std::make_unique<gfx::NullRenderer>();
  auto manager = std::make_unique<EditorManager>();
  manager->Initialize(renderer.get(), "");
  manager->SetAssetLoadMode(AssetLoadMode::kLazy);

  ScopedTempDir temp_dir(MakeTempDir("yaze_workflow_callbacks"));
  const auto project_path = temp_dir.path / "project.yaze";
  const auto marker_path = temp_dir.path / "build.marker";
  const auto script_path = temp_dir.path / "build_ok.sh";
  const auto valid_rom = temp_dir.path / "patched_valid.sfc";

  {
    std::ofstream script(script_path);
    ASSERT_TRUE(script.is_open());
    script << "#!/bin/sh\n";
    script << "printf 'callback-build' > build.marker\n";
  }
  std::filesystem::permissions(script_path,
                               std::filesystem::perms::owner_exec |
                                   std::filesystem::perms::owner_read |
                                   std::filesystem::perms::owner_write,
                               std::filesystem::perm_options::add);
  WriteRomFile(valid_rom, "CALLBACK RUN ROM");

  auto* project = manager->GetCurrentProject();
  ASSERT_NE(project, nullptr);
  project->name = "WorkflowCallbacks";
  project->filepath = project_path.string();
  project->build_script = "./build_ok.sh";
  ASSERT_TRUE(project->hack_manifest
                  .LoadFromString(R"json(
{
  "manifest_version": 2,
  "hack_name": "Generic Hack",
  "build_pipeline": {
    "patched_rom": "patched_valid.sfc"
  }
}
)json")
                  .ok());

  auto show_output = ContentRegistry::Context::show_workflow_output_callback();
  ASSERT_TRUE(show_output);
  show_output();
  EXPECT_TRUE(manager->window_manager().IsWindowOpen(
      manager->GetCurrentSessionId(), "workflow.output"));

  auto start_build = ContentRegistry::Context::start_build_workflow_callback();
  ASSERT_TRUE(start_build);
  start_build();

  const auto build_status = ContentRegistry::Context::build_workflow_status();
  EXPECT_TRUE(build_status.visible);
  EXPECT_TRUE(build_status.can_cancel);
  EXPECT_EQ(build_status.state, ProjectWorkflowState::kRunning);

  bool build_completed = false;
  for (int i = 0; i < 40; ++i) {
    if (std::filesystem::exists(marker_path)) {
      build_completed = true;
      break;
    }
    std::this_thread::sleep_for(std::chrono::milliseconds(50));
  }

  ASSERT_TRUE(build_completed);
  EXPECT_EQ(ReadFile(marker_path), "callback-build");

  auto run_project = ContentRegistry::Context::run_project_workflow_callback();
  ASSERT_TRUE(run_project);
  run_project();

  EXPECT_TRUE(manager->emulator().is_snes_initialized());
  EXPECT_TRUE(manager->emulator().running());
  ASSERT_FALSE(ContentRegistry::Context::workflow_history().empty());
  EXPECT_EQ(ContentRegistry::Context::workflow_history().front().kind, "Run");
#endif  // _WIN32
}

}  // namespace
}  // namespace yaze::editor
