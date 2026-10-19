#include "app/editor/overworld/lyra_terrain_canvas.h"

#include <chrono>
#include <filesystem>
#include <fstream>
#include <iterator>

#include "gtest/gtest.h"

namespace yaze::editor {
namespace {
namespace fs = std::filesystem;

core::LyraTerrainProfile FixtureProfile() {
  core::LyraTerrainProfile profile;
  profile.tile_count = 4096;
  profile.tile16_definition_fingerprint = "definitions:fixture";
  profile.graphics_fingerprint = "graphics:fixture";
  profile.palette_fingerprint = "palette:fixture";
  profile.animation_fingerprint = "animation:fixture";
  return profile;
}

void Write(const fs::path& path, const std::string& bytes) {
  std::ofstream file(path, std::ios::binary | std::ios::trunc);
  ASSERT_TRUE(file.is_open());
  file << bytes;
  ASSERT_TRUE(file.good());
}

std::string Read(const fs::path& path) {
  std::ifstream file(path, std::ios::binary);
  EXPECT_TRUE(file.is_open());
  return {std::istreambuf_iterator<char>(file),
          std::istreambuf_iterator<char>()};
}

class LyraTerrainSourceTest : public ::testing::Test {
 protected:
  void SetUp() override {
    const auto nonce =
        std::chrono::steady_clock::now().time_since_epoch().count();
    directory_ = fs::temp_directory_path() /
                 ("yaze_lyra_source_" + std::to_string(nonce));
    fs::create_directories(directory_);
    project_.name = "Lyra source fixture";
    project_.filepath = (directory_ / "Oracle.yaze").string();
    project_.rom_filename = (directory_ / "private.sfc").string();
    source_ = directory_ / "Oracle.lyra-terrain.json";
    Write(project_.rom_filename, "immutable ROM fixture");
    ASSERT_TRUE(project_.Save().ok());
    project_before_ = Read(project_.filepath);
    project_.lyra_terrain_file = source_.string();
  }

  void TearDown() override {
    std::error_code ec;
    fs::remove_all(directory_, ec);
  }

  void ExpectProtectedFilesUnchanged() {
    EXPECT_EQ(Read(project_.filepath), project_before_);
    EXPECT_EQ(Read(project_.rom_filename), "immutable ROM fixture");
  }

  fs::path directory_;
  fs::path source_;
  project::YazeProject project_;
  std::string project_before_;
};

TEST_F(LyraTerrainSourceTest, CreatesSidecarReopensAndPreservesUndoAfterSave) {
  auto document = core::LyraTerrainDocument::Create(FixtureProfile(), 8);
  ASSERT_TRUE(document.ok());
  ASSERT_TRUE(document->BeginStroke().ok());
  ASSERT_TRUE(document->Paint(7, 31, 101).ok());
  ASSERT_TRUE(document->Paint(7, 32, 102).ok());
  ASSERT_TRUE(document->Paint(7, 64, 103).ok());
  auto stroke = document->EndStroke();
  ASSERT_TRUE(stroke.ok());
  auto published = PublishLyraTerrainSource(project_, source_.string(),
                                            std::nullopt, *document);
  ASSERT_TRUE(published.ok()) << published.status();
  EXPECT_EQ(Read(source_), *published);
  EXPECT_TRUE(
      document->dirty());  // Publication does not silently clear drafts.
  ExpectProtectedFilesUnchanged();
  ASSERT_TRUE(project_.Save().ok());
  document->MarkSaved();

  project::YazeProject reopened_project;
  ASSERT_TRUE(reopened_project.Open(project_.filepath).ok());
  EXPECT_EQ(reopened_project.lyra_terrain_file, source_.string());
  auto reopened = core::LyraTerrainDocument::Parse(
      Read(reopened_project.lyra_terrain_file), FixtureProfile());
  ASSERT_TRUE(reopened.ok()) << reopened.status();
  EXPECT_EQ(reopened->maps(), document->maps());
  EXPECT_FALSE(reopened->dirty());

  ASSERT_TRUE(document->ApplyStroke(*stroke, true).ok());
  EXPECT_TRUE(document->dirty());
  auto undone = PublishLyraTerrainSource(project_, source_.string(), *published,
                                         *document);
  ASSERT_TRUE(undone.ok()) << undone.status();
  auto undone_reopened =
      core::LyraTerrainDocument::Parse(*undone, FixtureProfile());
  ASSERT_TRUE(undone_reopened.ok());
  EXPECT_EQ(undone_reopened->MapTile(0xA8, 7, 0).value(), 8);
  ASSERT_TRUE(document->ApplyStroke(*stroke, false).ok());
  EXPECT_FALSE(document->dirty());
}

TEST_F(LyraTerrainSourceTest, RefusesChangedAndDeletedSidecar) {
  auto document = core::LyraTerrainDocument::Create(FixtureProfile());
  ASSERT_TRUE(document.ok());
  auto before = PublishLyraTerrainSource(project_, source_.string(),
                                         std::nullopt, *document);
  ASSERT_TRUE(before.ok());
  ASSERT_TRUE(document->BeginStroke().ok());
  ASSERT_TRUE(document->Paint(1, 32, 17).ok());
  ASSERT_TRUE(document->EndStroke().ok());
  Write(source_, "external edit");
  EXPECT_EQ(
      PublishLyraTerrainSource(project_, source_.string(), *before, *document)
          .status()
          .code(),
      absl::StatusCode::kFailedPrecondition);
  EXPECT_EQ(Read(source_), "external edit");
  EXPECT_TRUE(document->dirty());
  fs::remove(source_);
  EXPECT_EQ(
      PublishLyraTerrainSource(project_, source_.string(), *before, *document)
          .status()
          .code(),
      absl::StatusCode::kFailedPrecondition);
  EXPECT_FALSE(fs::exists(source_));
  ExpectProtectedFilesUnchanged();
}

TEST_F(LyraTerrainSourceTest,
       RefusesFileCreatedSinceNewDraftAndMissingProject) {
  auto document = core::LyraTerrainDocument::Create(FixtureProfile());
  ASSERT_TRUE(document.ok());
  Write(source_, "someone else's source");
  EXPECT_FALSE(PublishLyraTerrainSource(project_, source_.string(),
                                        std::nullopt, *document)
                   .ok());
  EXPECT_EQ(Read(source_), "someone else's source");
  ExpectProtectedFilesUnchanged();
  fs::remove(source_);
  fs::remove(project_.filepath);
  EXPECT_FALSE(PublishLyraTerrainSource(project_, source_.string(),
                                        std::nullopt, *document)
                   .ok());
  EXPECT_FALSE(fs::exists(source_));
}

TEST_F(LyraTerrainSourceTest, RefusesChangedReferenceAndRomAlias) {
  auto document = core::LyraTerrainDocument::Create(FixtureProfile());
  ASSERT_TRUE(document.ok());
  project_.lyra_terrain_file = (directory_ / "another.json").string();
  EXPECT_FALSE(PublishLyraTerrainSource(project_, source_.string(),
                                        std::nullopt, *document)
                   .ok());
  project_.lyra_terrain_file = source_.string();
  fs::create_hard_link(project_.rom_filename, source_);
  EXPECT_FALSE(PublishLyraTerrainSource(project_, source_.string(),
                                        Read(source_), *document)
                   .ok());
  ExpectProtectedFilesUnchanged();
}

}  // namespace
}  // namespace yaze::editor
