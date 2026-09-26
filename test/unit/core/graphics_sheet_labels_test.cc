#include "core/graphics_sheet_labels.h"

#include <gtest/gtest.h>

#include <filesystem>
#include <fstream>
#include <iterator>
#include <string>

#include "core/project.h"
#include "unique_temp_path.h"

namespace yaze::core {
namespace {

std::string ReadFile(const std::filesystem::path& path) {
  std::ifstream file(path);
  return std::string((std::istreambuf_iterator<char>(file)),
                     std::istreambuf_iterator<char>());
}

project::YazeProject LoadProject(const std::string& body,
                                 const std::filesystem::path& path) {
  project::YazeProject project;
  EXPECT_TRUE(
      project.LoadFromString("[project]\nname=Labels\n\n" + body, path.string())
          .ok());
  return project;
}

TEST(GraphicsSheetLabelsTest, SheetLabelsRoundTripThroughTheProjectFile) {
  const auto path = test::UniqueTempPath("sheet_labels", ".yaze");
  auto project = LoadProject("", path);
  SetGraphicsSheetLabel(project, 0xC8, "Farore");
  SetGraphicsSheetLabel(project, 0x92, "Stalfos, Miri");
  EXPECT_EQ(GetGraphicsSheetLabel(project, 0xC8), "Farore");

  project.filepath = path.string();
  ASSERT_TRUE(project.Save().ok());
  const std::string saved = ReadFile(path);
  EXPECT_NE(saved.find("[labels_graphics]"), std::string::npos) << saved;
  EXPECT_NE(saved.find("200=Farore"), std::string::npos) << saved;

  project::YazeProject reopened;
  ASSERT_TRUE(reopened.LoadFromString(saved, path.string()).ok());
  EXPECT_EQ(GetGraphicsSheetLabel(reopened, 0xC8), "Farore");
  EXPECT_EQ(GetGraphicsSheetLabel(reopened, 0x92), "Stalfos, Miri");
  std::filesystem::remove(path);

  SetGraphicsSheetLabel(project, 0xC8, "");
  EXPECT_EQ(GetGraphicsSheetLabel(project, 0xC8), "");
}

TEST(GraphicsSheetLabelsTest, ImportsSpritesetCsvWithoutOverwriting) {
  const auto path = test::UniqueTempPath("sheet_labels_import", ".yaze");
  auto project = LoadProject("", path);
  SetGraphicsSheetLabel(project, 0x1F + 0x73, "My Stalfos sheet");
  const std::string csv =
      "ID,Usage,Slot 1,Slot 2,Slot 3,Slot 4\n"
      "0x09,,\"0x1F Stalfos, Miri\",0x49 Soldiers,0x55 Farore,\n"
      "0x0C,,0x00,0x43 Kydrog Cutscene,0x55 Farore,\n";

  EXPECT_EQ(ImportSpritesetSheetLabels(project, csv), 3);  // 0x49,0x55,0x43
  EXPECT_EQ(GetGraphicsSheetLabel(project, 0x1F + 0x73), "My Stalfos sheet");
  EXPECT_EQ(GetGraphicsSheetLabel(project, 0x55 + 0x73), "Farore");
  EXPECT_EQ(GetGraphicsSheetLabel(project, 0x49 + 0x73), "Soldiers");

  EXPECT_EQ(ImportSpritesetSheetLabels(project, csv, /*overwrite=*/true), 4);
  EXPECT_EQ(GetGraphicsSheetLabel(project, 0x1F + 0x73), "Stalfos, Miri");
}

// yaze wrote spriteset 12 as "0x12"; those keys must come back as 12, not 18.
TEST(GraphicsSheetLabelsTest, MigratesOldGfxGroupLabelKeysInProjects) {
  const auto path = test::UniqueTempPath("label_migration", ".yaze");
  const auto project = LoadProject(
      "[labels_spriteset]\n"
      "0x12=Old twelve\n"
      "0x0C=Hex twelve\n"
      "18=Eighteen\n"
      "0x1F=Thirty-one\n"
      "0xZZ=Garbage kept\n\n"
      "[labels_blockset]\n"
      "0x3=Three\n\n"
      "[labels_room]\n"
      "0x12=Room hex key\n",
      path);
  const auto& spritesets = project.resource_labels.at("spriteset");
  EXPECT_EQ(spritesets.at("12"), "Old twelve");  // old format beats real hex
  EXPECT_EQ(spritesets.at("18"), "Eighteen");
  EXPECT_EQ(spritesets.at("31"), "Thirty-one");
  EXPECT_EQ(spritesets.at("0xZZ"), "Garbage kept");
  EXPECT_EQ(spritesets.count("0x12"), 0u);
  EXPECT_EQ(spritesets.size(), 4u);
  EXPECT_EQ(project.resource_labels.at("blockset").at("3"), "Three");
  // Other label types are not touched.
  EXPECT_EQ(project.resource_labels.at("room").at("0x12"), "Room hex key");
}

TEST(GraphicsSheetLabelsTest, MigratesOldGfxGroupLabelKeysInRomLabelFiles) {
  const auto path = test::UniqueTempPath("rom_labels", ".labels");
  {
    std::ofstream file(path);
    file << "[spriteset]\n0x12=Old twelve\n[roomset]\n0x81=Roomset 81\n";
  }
  project::ResourceLabelManager labels;
  ASSERT_TRUE(labels.LoadLabels(path.string()));
  std::filesystem::remove(path);
  EXPECT_EQ(labels.GetLabel("spriteset", "12"), "Old twelve");
  EXPECT_EQ(labels.GetLabel("roomset", "81"), "Roomset 81");
}

}  // namespace
}  // namespace yaze::core
