#include "app/editor/editor_manager.h"
#include "app/gfx/backend/null_renderer.h"

#include <gtest/gtest.h>

#include <algorithm>
#include <chrono>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <string>
#include <vector>

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

struct ScopedFileCleanup {
  std::filesystem::path path;
  ~ScopedFileCleanup() {
    std::error_code ec;
    std::filesystem::remove(path, ec);
  }
};

std::filesystem::path MakeTempFilePath(const std::string& basename) {
  const auto nonce = static_cast<uint64_t>(
      std::chrono::high_resolution_clock::now().time_since_epoch().count());
  return std::filesystem::temp_directory_path() /
         (basename + "_" + std::to_string(nonce));
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

TEST(UICoordinatorWorkflowTest, RegistersProjectBuildAndRunCommands) {
  ScopedImGuiContext imgui;
  auto renderer = std::make_unique<gfx::NullRenderer>();
  auto manager = std::make_unique<EditorManager>();
  manager->Initialize(renderer.get(), "");
  manager->SetAssetLoadMode(AssetLoadMode::kLazy);

  const std::filesystem::path rom_path =
      MakeTempFilePath("yaze_ui_workflow_test.sfc");
  ScopedFileCleanup cleanup{rom_path};
  WriteRomFile(rom_path);
  ASSERT_TRUE(manager->OpenRomOrProject(rom_path.string()).ok());

  auto* project = manager->GetCurrentProject();
  ASSERT_NE(project, nullptr);
  project->name = "WorkflowProject";
  project->filepath = (rom_path.parent_path() / "workflow_project.yaze").string();

  ASSERT_NE(manager->ui_coordinator(), nullptr);
  manager->ui_coordinator()->InitializeCommandPalette(manager->GetCurrentSessionId());

  const auto commands = manager->ui_coordinator()->command_palette()->GetAllCommands();
  const auto has_command = [&](const std::string& name) {
    return std::any_of(commands.begin(), commands.end(),
                       [&](const CommandEntry& entry) { return entry.name == name; });
  };

  EXPECT_TRUE(has_command("Build & Run: Build Project"));
  EXPECT_TRUE(has_command("Build & Run: Run Project Output"));
  EXPECT_TRUE(has_command("Build & Run: Workflow Output"));
}

}  // namespace
}  // namespace yaze::editor
