// [atlas] in the .yaze project file: parse, serialize and a load/save/reload
// round trip on a temp copy of the Oracle of Secrets project file with the
// section Oracle will add (its two existing annotation files, in place).

#include <filesystem>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include "core/project.h"
#include "unique_temp_path.h"
#include "yaze_test_paths.h"

namespace yaze::project {
namespace {

namespace fs = std::filesystem;
using ::testing::ElementsAre;
using ::testing::HasSubstr;
using ::testing::Not;

class ScopedDir {
 public:
  explicit ScopedDir(fs::path path) : path_(std::move(path)) {
    fs::create_directories(path_);
  }
  ~ScopedDir() {
    std::error_code ec;
    fs::remove_all(path_, ec);
  }
  const fs::path& path() const { return path_; }

 private:
  fs::path path_;
};

std::string ReadFile(const fs::path& path) {
  std::ifstream file(path, std::ios::binary);
  std::stringstream buffer;
  buffer << file.rdbuf();
  return buffer.str();
}

void WriteFile(const fs::path& path, const std::string& content) {
  std::ofstream(path, std::ios::binary | std::ios::trunc) << content;
}

std::string OracleProjectFixture() {
  return ReadFile(fs::path(YAZE_TEST_FIXTURE_DIR) / "project" /
                  "oracle_of_secrets_2026_09_27.yaze");
}

constexpr char kOracleAtlasSection[] =
    "\n[atlas]\n"
    "# Same files the Scripts/Maps renderers read.\n"
    "annotations=Scripts/Maps/room_annotations.json,"
    "Scripts/Maps/overworld_annotations.json\n";

TEST(ProjectAtlasSettingsTest, OracleAtlasSectionRoundTrips) {
  ScopedDir dir(yaze::test::UniqueTempPath("project_atlas_round_trip"));
  const fs::path source = dir.path() / "Oracle-of-Secrets.yaze";
  const std::string original = OracleProjectFixture();
  ASSERT_FALSE(original.empty());
  WriteFile(source, original + kOracleAtlasSection);

  YazeProject project;
  ASSERT_TRUE(project.Open(source.string()).ok());
  EXPECT_THAT(project.atlas.annotations,
              ElementsAre("Scripts/Maps/room_annotations.json",
                          "Scripts/Maps/overworld_annotations.json"));
  EXPECT_EQ(project.atlas.defaults, "vanilla");
  EXPECT_TRUE(project.atlas.section_present);
  const auto resolved = project.AtlasAnnotationPaths();
  ASSERT_EQ(resolved.size(), 2u);
  EXPECT_EQ(
      fs::path(resolved[0]),
      fs::path(project.GetAbsolutePath("Scripts/Maps/room_annotations.json")));
  EXPECT_TRUE(fs::path(resolved[1]).is_absolute());

  const fs::path saved_path = dir.path() / "saved.yaze";
  ASSERT_TRUE(project.SaveAs(saved_path.string()).ok());
  const std::string saved = ReadFile(saved_path);
  // The comment stays above the key it preceded.
  EXPECT_THAT(saved, HasSubstr("[atlas]\n"
                               "# Same files the Scripts/Maps renderers read.\n"
                               "annotations=Scripts/Maps/room_annotations.json,"
                               "Scripts/Maps/overworld_annotations.json\n"));
  EXPECT_EQ(saved.find("[atlas]"), saved.rfind("[atlas]"));
  // The rest of the Oracle file is still there (passthrough + known keys).
  EXPECT_THAT(saved, HasSubstr("reserved_sheets=0x7B,0x7C"));
  EXPECT_THAT(saved, HasSubstr("custom_collision_json=Data/dungeons/"
                               "custom_collision.json"));

  YazeProject reloaded;
  ASSERT_TRUE(reloaded.Open(saved_path.string()).ok());
  EXPECT_EQ(reloaded.atlas, project.atlas);
  EXPECT_EQ(reloaded.AtlasAnnotationPaths(),
            std::vector<std::string>(
                {reloaded.GetAbsolutePath("Scripts/Maps/room_annotations.json"),
                 reloaded.GetAbsolutePath(
                     "Scripts/Maps/overworld_annotations.json")}));
}

TEST(ProjectAtlasSettingsTest, ProjectWithoutAtlasDoesNotGainOne) {
  ScopedDir dir(yaze::test::UniqueTempPath("project_atlas_absent"));
  const fs::path source = dir.path() / "Oracle-of-Secrets.yaze";
  WriteFile(source, OracleProjectFixture());
  YazeProject project;
  ASSERT_TRUE(project.Open(source.string()).ok());
  EXPECT_FALSE(project.atlas.section_present);
  EXPECT_TRUE(project.atlas.IsDefault());
  ASSERT_TRUE(project.Save().ok());
  EXPECT_THAT(ReadFile(source), Not(HasSubstr("[atlas]")));
}

TEST(ProjectAtlasSettingsTest, AllKeysAndUnknownKeysSurvive) {
  ScopedDir dir(yaze::test::UniqueTempPath("project_atlas_keys"));
  const fs::path source = dir.path() / "Hack.yaze";
  WriteFile(source,
            "[project]\nname=Hack\n\n[atlas]\n"
            "annotations=Docs/Atlas/atlas.json\n"
            "defaults=none\n"
            "symbols=Roms/hack.sym\n"
            "export_folder=Roms/Maps\n"
            "future_atlas_key=keep\n");
  YazeProject project;
  ASSERT_TRUE(project.Open(source.string()).ok());
  EXPECT_THAT(project.atlas.annotations, ElementsAre("Docs/Atlas/atlas.json"));
  EXPECT_EQ(project.atlas.defaults, "none");
  EXPECT_EQ(project.atlas.symbols, "Roms/hack.sym");
  EXPECT_EQ(project.atlas.export_folder, "Roms/Maps");
  EXPECT_EQ(fs::path(project.AtlasSymbolsPath()),
            fs::path(project.GetAbsolutePath("Roms/hack.sym")));
  ASSERT_TRUE(project.Save().ok());
  const std::string saved = ReadFile(source);
  EXPECT_THAT(saved, HasSubstr("[atlas]\nannotations=Docs/Atlas/atlas.json\n"
                               "defaults=none\nsymbols=Roms/hack.sym\n"
                               "export_folder=Roms/Maps\n"
                               "future_atlas_key=keep\n"));

  // Set in code (no section in the file yet): written on save.
  YazeProject fresh;
  fresh.filepath = (dir.path() / "Fresh.yaze").string();
  fresh.name = "Fresh";
  fresh.atlas.annotations = {"a.json", "b.json"};
  ASSERT_TRUE(fresh.Save().ok());
  YazeProject reopened;
  ASSERT_TRUE(reopened.Open(fresh.filepath).ok());
  EXPECT_EQ(reopened.atlas, fresh.atlas);
}

TEST(ProjectAtlasSettingsTest, EmptySectionStaysAndDefaultsToVanilla) {
  ScopedDir dir(yaze::test::UniqueTempPath("project_atlas_empty"));
  const fs::path source = dir.path() / "Hack.yaze";
  WriteFile(source, "[project]\nname=Hack\n\n[atlas]\ndefaults=\n");
  YazeProject project;
  ASSERT_TRUE(project.Open(source.string()).ok());
  EXPECT_TRUE(project.atlas.section_present);
  EXPECT_EQ(project.atlas.defaults, "vanilla");
  ASSERT_TRUE(project.Save().ok());
  EXPECT_THAT(ReadFile(source), HasSubstr("[atlas]\nannotations=\n"
                                          "defaults=vanilla\n"));
}

}  // namespace
}  // namespace yaze::project
