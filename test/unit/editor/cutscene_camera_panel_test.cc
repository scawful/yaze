#include "app/editor/cutscene/ui/window/cutscene_camera_panel.h"

#include <gtest/gtest.h>

#include <filesystem>
#include <fstream>
#include <iterator>
#include <string>

#include "app/editor/registry/content_registry.h"
#include "core/project.h"
#include "imgui/imgui.h"
#include "unique_temp_path.h"

namespace yaze::editor {

class CutsceneCameraPanelTestPeer {
 public:
  static void SetShots(CutsceneCameraPanel& panel,
                       zelda3::CutsceneShotSet shots) {
    panel.shots_ = std::move(shots);
    panel.selected_ = 0;
    panel.loaded_once_ = true;
  }
  static void Save(CutsceneCameraPanel& panel) { panel.Save(); }
  static void Load(CutsceneCameraPanel& panel) { panel.Load(); }
  static zelda3::CutsceneShotSet& Shots(CutsceneCameraPanel& panel) {
    return panel.shots_;
  }
  static const std::string& Status(CutsceneCameraPanel& panel) {
    return panel.status_;
  }
};

namespace {

// Draws the window for a few frames with no project open (read-only mode).
// ImGui asserts on unbalanced Begin/End, PushID/PopID and table calls.
TEST(CutsceneCameraPanelTest, DrawsWithoutAProject) {
  ImGuiContext* ctx = ImGui::CreateContext();
  ImGui::SetCurrentContext(ctx);
  ImGuiIO& io = ImGui::GetIO();
  io.DisplaySize = ImVec2(1280, 800);
  io.DeltaTime = 1.0f / 60.0f;
  unsigned char* pixels = nullptr;
  int width = 0;
  int height = 0;
  io.Fonts->GetTexDataAsRGBA32(&pixels, &width, &height);

  CutsceneCameraPanel panel;
  EXPECT_EQ(panel.GetId(), "overworld.cutscene_camera");
  for (int frame = 0; frame < 3; ++frame) {
    ImGui::NewFrame();
    ImGui::Begin("Cutscene Camera");
    panel.Draw(nullptr);
    ImGui::End();
    ImGui::Render();
  }

  // With a shot selected, the fields and canvas draw too, including a camera
  // outside the area's limits.
  zelda3::CutsceneShot shot;
  shot.name = "tail_pond_open";
  shot.area = 0x2D;
  shot.camera_x = 0;
  shot.camera_y = 0;
  shot.link = {2720, 2800, 2};
  shot.actors.push_back({0xF5, 2690, 2770, 1, "Zora A"});
  CutsceneCameraPanelTestPeer::SetShots(panel, {{shot}});
  for (int frame = 0; frame < 3; ++frame) {
    ImGui::NewFrame();
    ImGui::Begin("Cutscene Camera");
    panel.Draw(nullptr);
    ImGui::End();
    ImGui::Render();
  }
  ImGui::DestroyContext(ctx);
}

std::string ReadAll(const std::filesystem::path& path) {
  std::ifstream in(path, std::ios::binary);
  return std::string(std::istreambuf_iterator<char>(in), {});
}

// Save creates the project's shots file, replaces it on later saves, and
// refuses to overwrite a file edited outside yaze since the last load.
TEST(CutsceneCameraPanelTest, SavesToTheProjectShotsFile) {
  const auto root = ::yaze::test::UniqueTempPath("yaze_cutscene_shots", "");
  std::filesystem::create_directories(root);
  project::YazeProject project;
  project.name = "shots_test";
  project.filepath = (root / "test.yaze").string();
  project.cutscene_shots = "Data/cutscenes/shots.json";
  ContentRegistry::Context::SetCurrentProject(&project);
  const auto shots_path = root / "Data" / "cutscenes" / "shots.json";

  CutsceneCameraPanel panel;
  CutsceneCameraPanelTestPeer::Load(panel);  // file absent: will be created
  zelda3::CutsceneShot shot;
  shot.name = "tail_pond_open";
  shot.area = 0x2D;
  shot.camera_x = 0x0A40;
  shot.camera_y = 0x0A20;
  shot.link = {2720, 2800, 2};
  CutsceneCameraPanelTestPeer::SetShots(panel, {{shot}});
  CutsceneCameraPanelTestPeer::Save(panel);
  auto expected = zelda3::SerializeCutsceneShots({{shot}});
  ASSERT_TRUE(expected.ok());
  EXPECT_EQ(ReadAll(shots_path), *expected)
      << CutsceneCameraPanelTestPeer::Status(panel);

  CutsceneCameraPanelTestPeer::Shots(panel).shots[0].camera_x = 0x0A80;
  CutsceneCameraPanelTestPeer::Save(panel);
  auto parsed = zelda3::ParseCutsceneShots(ReadAll(shots_path));
  ASSERT_TRUE(parsed.ok());
  EXPECT_EQ(parsed->shots[0].camera_x, 0x0A80);

  // An outside edit since the last load is not overwritten.
  const std::string outside = *expected;
  std::ofstream(shots_path, std::ios::binary | std::ios::trunc) << outside;
  CutsceneCameraPanelTestPeer::Shots(panel).shots[0].camera_x = 0x0A90;
  CutsceneCameraPanelTestPeer::Save(panel);
  EXPECT_EQ(ReadAll(shots_path), outside);
  EXPECT_NE(CutsceneCameraPanelTestPeer::Status(panel).find("changed outside"),
            std::string::npos);

  ContentRegistry::Context::SetCurrentProject(nullptr);
  std::error_code ec;
  std::filesystem::remove_all(root, ec);
}

}  // namespace
}  // namespace yaze::editor
