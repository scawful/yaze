// Deterministic source-draft lifecycle coverage. Full import persistence on a
// real Oracle ROM is a separately selected authoring Save ROM/reopen scenario.
#include "app/editor/overworld/overworld_editor.h"

#include <filesystem>
#include <fstream>
#include <iterator>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "app/gfx/resource/arena.h"
#include "core/project.h"
#include "core/source_artifact_publisher.h"
#include "gtest/gtest.h"
#include "imgui/imgui.h"
#include "nlohmann/json.hpp"
#include "rom/rom.h"
#include "unique_temp_path.h"
#include "zelda3/game_data.h"

namespace yaze::editor {

class ProjectGraphicsImportSaveTestPeer {
 public:
  static ProjectGraphicsSource* Source(OverworldEditor& editor) {
    return editor.graphics_source_.get();
  }
  static std::shared_ptr<ProjectGraphicsSource> RetainSource(
      OverworldEditor& editor) {
    return editor.graphics_source_;
  }
  static void SelectMap(OverworldEditor& editor, int map) {
    editor.current_map_ = map;
  }
  static void MarkCombinedHistory(OverworldEditor& editor) {
    editor.graphics_import_applied_ = true;
  }
  static void Draw(OverworldEditor& editor) {
    editor.DrawProjectGraphicsSource();
  }
};

namespace {
namespace fs = std::filesystem;
using Peer = ProjectGraphicsImportSaveTestPeer;

std::string Read(const fs::path& path) {
  std::ifstream input(path, std::ios::binary);
  EXPECT_TRUE(input.is_open()) << path;
  return {std::istreambuf_iterator<char>(input), {}};
}

void Write(const fs::path& path, const std::string& bytes) {
  std::ofstream output(path, std::ios::binary | std::ios::trunc);
  ASSERT_TRUE(output.is_open()) << path;
  output.write(bytes.data(), bytes.size());
  ASSERT_TRUE(output.good()) << path;
}

class ProjectGraphicsImportSaveTest : public ::testing::Test {
 protected:
  void SetUp() override {
    context_ = ImGui::CreateContext();
    ImGui::SetCurrentContext(context_);
    auto& io = ImGui::GetIO();
    io.IniFilename = nullptr;
    io.DisplaySize = ImVec2(1024, 900);
    io.DeltaTime = 1.0f / 60.0f;
    unsigned char* pixels = nullptr;
    int width = 0, height = 0;
    io.Fonts->GetTexDataAsRGBA32(&pixels, &width, &height);

    const auto temporary = test::UniqueTempPath("yaze_import_save_guard");
    ASSERT_TRUE(fs::create_directory(temporary));
    root_ = fs::canonical(temporary);
    fs::create_directories(root_ / "Source/Overworld");
    fs::create_directories(root_ / "Pack");
    std::string glyph(32, '\0');
    glyph[0] = static_cast<char>(0x80);
    glyph[1] = static_cast<char>(0x80);
    Write(root_ / "Source/Overworld/chars.bin", glyph);
    Write(root_ / "Pack/chars.bin", glyph);
    nlohmann::json catalog = {
        {"schema_version", 2},
        {"resources",
         {{"chars",
           {{"path", "chars.bin"},
            {"format", "snes4bpp"},
            {"byte_count", 32},
            {"sha256", core::ComputeSourceArtifactSha256(glyph)}}}}},
        {"profiles",
         {{"area",
           {{"bg_character_base_word", 0x2000},
            {"transfers",
             {{{"resource_id", "chars"},
               {"byte_count", 32},
               {"vram_word_address", 0x2C00}}}}}}}},
        {"bindings", nlohmann::json::array(
                         {{{"parent_maps", {0x30}}, {"profile_id", "area"}}})}};
    Write(root_ / "Source/Overworld/graphics_catalog.json", catalog.dump());
    Write(root_ / "Pack/catalog.json", catalog.dump());
    project_.filepath = (root_ / "Fixture.yaze").string();
    project_.git_repository = "Source";
    project_.overworld_graphics_preview = "Pack/catalog.json";

    std::vector<uint8_t> bytes(0x200000, 0);
    bytes[zelda3::OverworldCustomASMHasBeenApplied] = 3;
    for (int map = 0; map < zelda3::kNumOverworldMaps; ++map)
      bytes[zelda3::kOverworldMapParentIdExpanded + map] = map;
    ASSERT_TRUE(rom_.LoadFromData(bytes).ok());
    rom_.set_filename((root_ / "fixture.sfc").string());
    Write(rom_.filename(), std::string(bytes.begin(), bytes.end()));
    game_.graphics_buffer.resize(0x100000, 0);
    editor_ = std::make_unique<OverworldEditor>(&rom_);
    editor_->SetGameData(&game_);
    auto& maps = const_cast<std::vector<zelda3::OverworldMap>&>(
        editor_->overworld().overworld_maps());
    maps.reserve(zelda3::kNumOverworldMaps);
    for (int map = 0; map < zelda3::kNumOverworldMaps; ++map)
      maps.emplace_back(map, &rom_, &game_);
    EditorDependencies dependencies;
    dependencies.rom = &rom_;
    dependencies.game_data = &game_;
    dependencies.project = &project_;
    editor_->SetDependencies(dependencies);
    editor_->SetProjectContextProvider([this]() { return &project_; });
    Peer::SelectMap(*editor_, 0x30);
    ASSERT_TRUE(editor_->OpenProjectGraphicsSource().ok());
  }

  void TearDown() override {
    editor_.reset();
    gfx::Arena::Get().ClearTextureQueue();
    if (context_)
      ImGui::DestroyContext(context_);
    std::error_code error;
    if (!root_.empty())
      fs::remove_all(root_, error);
  }

  std::pair<bool, bool> DrawFrame() {
    ImGui::NewFrame();
    ImGui::Begin("Import save lifecycle fixture");
    ImGui::PushID("SourceActions");
    const bool before = ImGui::IsPopupOpen("Edit Project Graphics");
    Peer::Draw(*editor_);
    const bool after = ImGui::IsPopupOpen("Edit Project Graphics");
    ImGui::PopID();
    ImGui::End();
    ImGui::Render();
    return {before, after};
  }

  fs::path root_;
  ImGuiContext* context_ = nullptr;
  Rom rom_;
  zelda3::GameData game_;
  project::YazeProject project_;
  std::unique_ptr<OverworldEditor> editor_;
};

TEST_F(ProjectGraphicsImportSaveTest,
       UnpublishedBulkDraftBlocksSaveReloadAndSaveAsWithoutDiskCommit) {
  auto* source = Peer::Source(*editor_);
  ASSERT_NE(source, nullptr);
  const auto memory_before = rom_.vector();
  const auto disk_before = Read(rom_.filename());
  const auto canonical_before =
      Read(root_ / "Source/Overworld/graphics_catalog.json");
  const auto portable_before = Read(root_ / "Pack/catalog.json");
  const auto source_before = Read(source->selected_resource()->source_path);
  const auto pack_before = Read(source->selected_resource()->portable_path);
  auto bytes = source->selected_resource()->pending_bytes;
  bytes[0] ^= 1;
  ASSERT_TRUE(source->ReplaceResourceBytes(bytes).ok());
  EXPECT_TRUE(editor_->HasPendingProjectDraftChanges());

  const auto save = editor_->Save();
  EXPECT_EQ(save.code(), absl::StatusCode::kFailedPrecondition);
  EXPECT_NE(save.message().find("graphics"), std::string::npos);
  EXPECT_EQ(editor_->PrepareProjectSave().code(),
            absl::StatusCode::kFailedPrecondition);
  EXPECT_EQ(
      editor_->SaveProjectTerrainAs(&project_, (root_ / "Fork.yaze").string())
          .code(),
      absl::StatusCode::kFailedPrecondition);
  EXPECT_EQ(editor_->Load().code(), absl::StatusCode::kFailedPrecondition);
  EXPECT_EQ(Peer::Source(*editor_), source);
  EXPECT_EQ(source->selected_resource()->pending_bytes, bytes);
  EXPECT_EQ(rom_.vector(), memory_before);
  EXPECT_EQ(Read(rom_.filename()), disk_before);
  EXPECT_EQ(Read(root_ / "Source/Overworld/graphics_catalog.json"),
            canonical_before);
  EXPECT_EQ(Read(root_ / "Pack/catalog.json"), portable_before);
  EXPECT_EQ(Read(source->selected_resource()->source_path), source_before);
  EXPECT_EQ(Read(source->selected_resource()->portable_path), pack_before);
  EXPECT_FALSE(fs::exists(root_ / "Fork.yaze"));
}

TEST_F(ProjectGraphicsImportSaveTest,
       CleanDocumentWithCombinedHistorySurvivesPanelReopen) {
  const auto retained = Peer::RetainSource(*editor_);
  ASSERT_NE(retained, nullptr);
  ASSERT_FALSE(retained->any_dirty());
  Peer::MarkCombinedHistory(*editor_);
  ASSERT_TRUE(editor_->OpenProjectGraphicsSource().ok());
  EXPECT_EQ(Peer::RetainSource(*editor_), retained);
  EXPECT_FALSE(editor_->HasPendingProjectDraftChanges());

  project_.git_repository = (root_ / "AnotherSource").string();
  EXPECT_EQ(editor_->OpenProjectGraphicsSource().code(),
            absl::StatusCode::kFailedPrecondition);
  EXPECT_EQ(Peer::RetainSource(*editor_), retained);
  EXPECT_EQ(retained->source_root(), (root_ / "Source").string());
}

TEST_F(ProjectGraphicsImportSaveTest,
       ClearClosesRetainedModalWithoutDestroyingHistoryOwnedDocument) {
  auto retained = Peer::RetainSource(*editor_);
  Peer::MarkCombinedHistory(*editor_);
  ASSERT_TRUE(DrawFrame().second);
  ASSERT_TRUE(editor_->Clear().ok());
  EXPECT_EQ(Peer::Source(*editor_), nullptr);
  ASSERT_NE(retained, nullptr);
  EXPECT_TRUE(retained->loaded());
  const auto after = DrawFrame();
  EXPECT_TRUE(after.first) << "Exercise the retained modal after editor Clear";
  EXPECT_FALSE(after.second);
  EXPECT_FALSE(editor_->HasPendingProjectDraftChanges());
}

}  // namespace
}  // namespace yaze::editor
