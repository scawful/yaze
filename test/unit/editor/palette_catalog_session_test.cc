#include "app/editor/palette/palette_editor.h"

#include <chrono>
#include <filesystem>
#include <fstream>

#include "core/project.h"
#include "gtest/gtest.h"

namespace yaze::editor {
namespace {
constexpr const char* kCatalog = R"({"schema_version":1,"game_id":"oracle",
"entries":[{"key":"oracle.lw","name":"Light World sprites",
"group":"global_sprites","index":0,"world":"light_world","use":"mixed",
"description":"Four shared OBJ rows","provenance":"Palettes_Load_SpriteMain",
"ownership":"rom","usages":[]}]})";

class PaletteCatalogSessionTest : public ::testing::Test {
 protected:
  void SetUp() override {
    root_ = std::filesystem::temp_directory_path() /
            ("yaze_palette_session_" +
             std::to_string(
                 std::chrono::steady_clock::now().time_since_epoch().count()));
    std::filesystem::create_directories(root_);
    std::ofstream(root_ / "catalog.json") << kCatalog;
    project_.filepath = (root_ / "Oracle.yaze").string();
    project_.palette_catalog_file = "catalog.json";
  }
  void TearDown() override {
    std::error_code ec;
    std::filesystem::remove_all(root_, ec);
  }
  std::filesystem::path root_;
  project::YazeProject project_;
};

TEST_F(PaletteCatalogSessionTest,
       ProjectAndSessionRebindingClearPriorMetadata) {
  PaletteEditor editor;
  EditorDependencies deps;
  deps.project = &project_;
  deps.session_id = 0;
  editor.SetDependencies(deps);
  ASSERT_NE(editor.palette_catalog(), nullptr);
  // Dependency refresh does not rescan files when identity is unchanged.
  std::filesystem::remove(root_ / "catalog.json");
  editor.SetDependencies(deps);
  EXPECT_NE(editor.palette_catalog(), nullptr);
  deps.session_id = 1;
  editor.SetDependencies(deps);
  EXPECT_EQ(editor.palette_catalog(), nullptr);
  EXPECT_FALSE(editor.palette_catalog_status().ok());

  std::ofstream(root_ / "catalog.json") << kCatalog;
  deps.session_id = 0;
  editor.SetDependencies(deps);
  ASSERT_NE(editor.palette_catalog(), nullptr);
  ASSERT_TRUE(editor.ReloadPaletteCatalog().ok());
  project::YazeProject other;
  other.filepath = (root_ / "Other.yaze").string();
  deps.project = &other;
  editor.SetDependencies(deps);
  EXPECT_EQ(editor.palette_catalog(), nullptr);
  EXPECT_FALSE(editor.palette_catalog_status().ok());
}

TEST_F(PaletteCatalogSessionTest, PathChangesAndFailedReloadNeverRetainNames) {
  PaletteEditor editor;
  EditorDependencies deps;
  deps.project = &project_;
  editor.SetDependencies(deps);
  ASSERT_NE(editor.palette_catalog(), nullptr);
  std::ofstream(root_ / "bad.json") << R"({"schema_version":2})";
  project_.palette_catalog_file = "bad.json";
  editor.SetDependencies(deps);
  EXPECT_EQ(editor.palette_catalog(), nullptr);
  project_.palette_catalog_file = "catalog.json";
  editor.SetDependencies(deps);
  ASSERT_NE(editor.palette_catalog(), nullptr);
  std::ofstream(root_ / "catalog.json") << "broken";
  EXPECT_FALSE(editor.ReloadPaletteCatalog().ok());
  EXPECT_EQ(editor.palette_catalog(), nullptr);
  deps.project = nullptr;
  editor.SetDependencies(deps);
  EXPECT_EQ(editor.palette_catalog(), nullptr);
}
}  // namespace
}  // namespace yaze::editor
