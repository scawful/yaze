#include "app/editor/overworld/overworld_editor.h"

#include <filesystem>
#include <fstream>
#include <iterator>
#include <memory>
#include <string>
#include <vector>

#include "app/gfx/resource/arena.h"
#include "core/source_artifact_publisher.h"
#include "gtest/gtest.h"
#include "imgui/imgui.h"
#include "nlohmann/json.hpp"
#include "unique_temp_path.h"
#include "zelda3/game_data.h"

namespace yaze::editor {

class ProjectGraphicsSourceUiTestPeer {
 public:
  static void SelectMap(OverworldEditor& editor, int map_id) {
    editor.current_map_ = map_id;
  }
  static absl::Status Open(OverworldEditor& editor) {
    return editor.OpenProjectGraphicsSource();
  }
  static ProjectGraphicsSource* Source(OverworldEditor& editor) {
    return editor.graphics_source_.get();
  }
};

namespace {
namespace fs = std::filesystem;
using Peer = ProjectGraphicsSourceUiTestPeer;

std::string Read(const fs::path& path) {
  std::ifstream file(path, std::ios::binary);
  EXPECT_TRUE(file.is_open());
  return {std::istreambuf_iterator<char>(file),
          std::istreambuf_iterator<char>()};
}

void Write(const fs::path& path, const std::string& bytes) {
  std::ofstream file(path, std::ios::binary | std::ios::trunc);
  ASSERT_TRUE(file.is_open());
  file << bytes;
  ASSERT_TRUE(file.good());
}

class ProjectGraphicsSourceUiTest : public ::testing::Test {
 protected:
  void SetUp() override {
    context_ = ImGui::CreateContext();
    ImGui::SetCurrentContext(context_);
    const auto temporary = test::UniqueTempPath("yaze_graphics_source_ui");
    ASSERT_TRUE(fs::create_directory(temporary));
    root_ = fs::canonical(temporary);
    WriteProjectFixture(root_ / "live", 3, live_);
    WriteProjectFixture(root_ / "snapshot", 6, snapshot_);

    std::vector<uint8_t> bytes(0x200000, 0);
    bytes[zelda3::OverworldCustomASMHasBeenApplied] = 3;
    for (int id = 0; id < zelda3::kNumOverworldMaps; ++id)
      bytes[zelda3::kOverworldMapParentIdExpanded + id] = id;
    ASSERT_TRUE(rom_.LoadFromData(bytes).ok());
    game_.graphics_buffer.resize(0x100000, 0);
    editor_ = std::make_unique<OverworldEditor>(&rom_);
    editor_->SetGameData(&game_);
    auto& maps = const_cast<std::vector<zelda3::OverworldMap>&>(
        editor_->overworld().overworld_maps());
    maps.reserve(zelda3::kNumOverworldMaps);
    for (int id = 0; id < zelda3::kNumOverworldMaps; ++id)
      maps.emplace_back(id, &rom_, &game_);
    EditorDependencies dependencies;
    dependencies.rom = &rom_;
    dependencies.game_data = &game_;
    // EditorManager retains a session snapshot here while the provider points
    // at the live project being edited. Writable source ownership uses live.
    dependencies.project = &snapshot_;
    editor_->SetDependencies(dependencies);
    active_ = &live_;
    editor_->SetProjectContextProvider([this]() { return active_; });
    Peer::SelectMap(*editor_, 0x30);
  }

  void TearDown() override {
    editor_.reset();
    gfx::Arena::Get().ClearTextureQueue();
    ImGui::DestroyContext(context_);
    std::error_code error;
    fs::remove_all(root_, error);
  }

  void WriteProjectFixture(const fs::path& directory, uint8_t color,
                           project::YazeProject& project) {
    const auto source = directory / "Source" / "Overworld";
    const auto pack = directory / "Pack";
    fs::create_directories(source);
    fs::create_directories(pack);
    std::string bytes(32, '\0');
    for (int plane = 0; plane < 4; ++plane) {
      if (color & (1 << plane))
        bytes[(plane / 2) * 16 + plane % 2] = static_cast<char>(0x80);
    }
    Write(source / "chars.bin", bytes);
    Write(pack / "chars.bin", bytes);
    nlohmann::json catalog = {
        {"schema_version", 2},
        {"resources",
         {{"chars",
           {{"path", "chars.bin"},
            {"format", "snes4bpp"},
            {"byte_count", 32},
            {"sha256", core::ComputeSourceArtifactSha256(bytes)}}}}},
        {"profiles",
         {{"area",
           {{"bg_character_base_word", 0x2000},
            {"transfers",
             {{{"resource_id", "chars"},
               {"byte_count", 32},
               {"vram_word_address", 0x2C00}}}}}}}},
        {"bindings", nlohmann::json::array(
                         {{{"parent_maps", {0x30}}, {"profile_id", "area"}}})}};
    Write(source / "graphics_catalog.json", catalog.dump());
    Write(pack / "catalog.json", catalog.dump());
    project.filepath = (directory / "Oracle.yaze").string();
    project.git_repository = "Source";
    project.overworld_graphics_preview = "Pack/catalog.json";
  }

  fs::path root_;
  ImGuiContext* context_ = nullptr;
  Rom rom_;
  zelda3::GameData game_;
  project::YazeProject live_, snapshot_;
  project::YazeProject* active_ = nullptr;
  std::unique_ptr<OverworldEditor> editor_;
};

TEST_F(ProjectGraphicsSourceUiTest, OpensLiveProjectInsteadOfSessionSnapshot) {
  ASSERT_TRUE(Peer::Open(*editor_).ok());
  const auto* source = Peer::Source(*editor_);
  ASSERT_NE(source, nullptr);
  EXPECT_EQ(source->source_root(), (root_ / "live" / "Source").string());
  EXPECT_EQ(source->portable_catalog(),
            (root_ / "live" / "Pack" / "catalog.json").string());
  EXPECT_EQ(source->GetPixel(0, 0, 0).value(), 3);
  EXPECT_FALSE(editor_->HasPendingProjectDraftChanges());
}

TEST_F(ProjectGraphicsSourceUiTest,
       PixelDraftBlocksSaveSaveAsAndReloadWithoutLosingHistory) {
  ASSERT_TRUE(Peer::Open(*editor_).ok());
  auto* source = Peer::Source(*editor_);
  ASSERT_NE(source, nullptr);
  const auto canonical = root_ / "live" / "Source" / "Overworld" / "chars.bin";
  const auto portable = root_ / "live" / "Pack" / "chars.bin";
  const auto canonical_before = Read(canonical);
  const auto portable_before = Read(portable);
  ASSERT_TRUE(source->SetPixel(0, 0, 0, 5).ok());
  EXPECT_TRUE(editor_->HasPendingProjectDraftChanges());
  EXPECT_EQ(editor_->PrepareProjectSave().code(),
            absl::StatusCode::kFailedPrecondition);
  EXPECT_EQ(
      editor_->SaveProjectTerrainAs(&live_, (root_ / "Fork.yaze").string())
          .code(),
      absl::StatusCode::kFailedPrecondition);
  EXPECT_EQ(editor_->Load().code(), absl::StatusCode::kFailedPrecondition);
  EXPECT_EQ(Peer::Source(*editor_), source);
  EXPECT_EQ(source->GetPixel(0, 0, 0).value(), 5);
  EXPECT_TRUE(source->can_undo());
  EXPECT_EQ(Read(canonical), canonical_before);
  EXPECT_EQ(Read(portable), portable_before);
  EXPECT_FALSE(fs::exists(root_ / "Fork.yaze"));

  ASSERT_TRUE(source->Undo().ok());
  EXPECT_FALSE(editor_->HasPendingProjectDraftChanges());
  ASSERT_TRUE(source->Redo().ok());
  EXPECT_TRUE(editor_->HasPendingProjectDraftChanges());
}

TEST_F(ProjectGraphicsSourceUiTest,
       ExistingDraftReopensFromLyraButRefusesChangedProjectBinding) {
  ASSERT_TRUE(Peer::Open(*editor_).ok());
  auto* source = Peer::Source(*editor_);
  ASSERT_NE(source, nullptr);
  ASSERT_TRUE(source->SetPixel(0, 0, 0, 5).ok());
  Peer::SelectMap(*editor_, 0xA0);
  EXPECT_TRUE(Peer::Open(*editor_).ok())
      << "An existing draft must stay reachable from the Lyra selection";
  EXPECT_EQ(Peer::Source(*editor_), source);

  const auto original_root = live_.git_repository;
  live_.git_repository = (root_ / "snapshot" / "Source").string();
  EXPECT_EQ(Peer::Open(*editor_).code(), absl::StatusCode::kFailedPrecondition);
  EXPECT_EQ(Peer::Source(*editor_), source);
  EXPECT_EQ(source->source_root(), (root_ / "live" / "Source").string());
  live_.git_repository = original_root;

  live_.overworld_graphics_preview =
      (root_ / "snapshot" / "Pack" / "catalog.json").string();
  EXPECT_EQ(Peer::Open(*editor_).code(), absl::StatusCode::kFailedPrecondition);
  live_.overworld_graphics_preview = "Pack/catalog.json";
  active_ = &snapshot_;
  EXPECT_EQ(Peer::Open(*editor_).code(), absl::StatusCode::kFailedPrecondition);
  EXPECT_EQ(Peer::Source(*editor_), source);
  EXPECT_TRUE(editor_->HasPendingProjectDraftChanges());
  EXPECT_EQ(source->GetPixel(0, 0, 0).value(), 5);
}

}  // namespace
}  // namespace yaze::editor
