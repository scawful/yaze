#include "app/editor/palette/palette_group_panel.h"

#include <gtest/gtest.h>

#include "app/gfx/util/palette_manager.h"
#include "util/json.h"
#include "zelda3/game_data.h"

namespace yaze::editor {
namespace {

class PaletteJsonTest : public ::testing::Test {
 protected:
  void SetUp() override { gfx::PaletteManager::Get().ResetForTesting(); }
  void TearDown() override { gfx::PaletteManager::Get().ResetForTesting(); }
};

#if defined(YAZE_WITH_JSON)

TEST_F(PaletteJsonTest, ExportContainsPaletteData) {
  zelda3::GameData game_data;
  auto* group = game_data.palette_groups.get_group("ow_main");
  ASSERT_NE(group, nullptr);
  group->clear();

  gfx::SnesPalette palette;
  palette.AddColor(gfx::SnesColor(0x7FFF));
  palette.AddColor(gfx::SnesColor(0x1234));
  group->AddPalette(palette);

  gfx::PaletteManager::Get().Initialize(&game_data);
  OverworldMainPalettePanel panel(nullptr, &game_data);

  const std::string json = panel.ExportToJson();
  const yaze::Json parsed = yaze::Json::parse(json);

  EXPECT_EQ(parsed["group"].get<std::string>(), "ow_main");
  ASSERT_TRUE(parsed["palettes"].is_array());
  ASSERT_EQ(parsed["palettes"].size(), 1);
  ASSERT_EQ(parsed["palettes"][0]["colors"].size(), 2);
  EXPECT_EQ(parsed["palettes"][0]["colors"][0].get<std::string>(), "$7FFF");
  EXPECT_EQ(parsed["palettes"][0]["colors"][1].get<std::string>(), "$1234");
}

TEST_F(PaletteJsonTest, ImportUpdatesPaletteColors) {
  zelda3::GameData game_data;
  auto* group = game_data.palette_groups.get_group("ow_main");
  ASSERT_NE(group, nullptr);
  group->clear();

  gfx::SnesPalette palette;
  palette.AddColor(gfx::SnesColor(0x0000));
  palette.AddColor(gfx::SnesColor(0x001F));
  group->AddPalette(palette);

  gfx::PaletteManager::Get().Initialize(&game_data);
  OverworldMainPalettePanel panel(nullptr, &game_data);

  yaze::Json root = yaze::Json::object();
  root["version"] = 1;
  root["group"] = "ow_main";
  root["palettes"] = yaze::Json::array();

  yaze::Json palette_json = yaze::Json::object();
  palette_json["index"] = 0;
  palette_json["colors"] = yaze::Json::array();
  palette_json["colors"].push_back("$0001");
  palette_json["colors"].push_back("$0002");
  root["palettes"].push_back(palette_json);

  const auto status = panel.ImportFromJson(root.dump());
  ASSERT_TRUE(status.ok()) << status.ToString();

  const auto& updated_palette = group->palette_ref(0);
  EXPECT_EQ(updated_palette[0].snes(), 0x0001);
  EXPECT_EQ(updated_palette[1].snes(), 0x0002);
}

TEST_F(PaletteJsonTest, ImportRejectsWrongGroup) {
  zelda3::GameData game_data;
  auto* group = game_data.palette_groups.get_group("ow_main");
  ASSERT_NE(group, nullptr);
  group->clear();

  gfx::SnesPalette palette;
  palette.AddColor(gfx::SnesColor(0x0000));
  palette.AddColor(gfx::SnesColor(0x001F));
  group->AddPalette(palette);

  gfx::PaletteManager::Get().Initialize(&game_data);
  OverworldMainPalettePanel panel(nullptr, &game_data);

  yaze::Json root = yaze::Json::object();
  root["version"] = 1;
  root["group"] = "wrong_group";
  root["palettes"] = yaze::Json::array();

  yaze::Json palette_json = yaze::Json::object();
  palette_json["index"] = 0;
  palette_json["colors"] = yaze::Json::array();
  palette_json["colors"].push_back("$0001");
  palette_json["colors"].push_back("$0002");
  root["palettes"].push_back(palette_json);

  const auto status = panel.ImportFromJson(root.dump());
  EXPECT_FALSE(status.ok());
}

TEST_F(PaletteJsonTest, ImportRejectsNonIntegerVersion) {
  zelda3::GameData game_data;
  auto* group = game_data.palette_groups.get_group("ow_main");
  ASSERT_NE(group, nullptr);
  group->clear();

  gfx::SnesPalette palette;
  palette.AddColor(gfx::SnesColor(0x0000));
  palette.AddColor(gfx::SnesColor(0x001F));
  group->AddPalette(palette);

  gfx::PaletteManager::Get().Initialize(&game_data);
  OverworldMainPalettePanel panel(nullptr, &game_data);

  yaze::Json root = yaze::Json::object();
  root["version"] = "one";
  root["group"] = "ow_main";
  root["palettes"] = yaze::Json::array();

  yaze::Json palette_json = yaze::Json::object();
  palette_json["index"] = 0;
  palette_json["colors"] = yaze::Json::array();
  palette_json["colors"].push_back("$0001");
  palette_json["colors"].push_back("$0002");
  root["palettes"].push_back(palette_json);

  const auto status = panel.ImportFromJson(root.dump());
  EXPECT_EQ(status.code(), absl::StatusCode::kInvalidArgument);
}

TEST_F(PaletteJsonTest, ImportRejectsNonStringGroup) {
  zelda3::GameData game_data;
  auto* group = game_data.palette_groups.get_group("ow_main");
  ASSERT_NE(group, nullptr);
  group->clear();

  gfx::SnesPalette palette;
  palette.AddColor(gfx::SnesColor(0x0000));
  palette.AddColor(gfx::SnesColor(0x001F));
  group->AddPalette(palette);

  gfx::PaletteManager::Get().Initialize(&game_data);
  OverworldMainPalettePanel panel(nullptr, &game_data);

  yaze::Json root = yaze::Json::object();
  root["version"] = 1;
  root["group"] = 123;
  root["palettes"] = yaze::Json::array();

  yaze::Json palette_json = yaze::Json::object();
  palette_json["index"] = 0;
  palette_json["colors"] = yaze::Json::array();
  palette_json["colors"].push_back("$0001");
  palette_json["colors"].push_back("$0002");
  root["palettes"].push_back(palette_json);

  const auto status = panel.ImportFromJson(root.dump());
  EXPECT_EQ(status.code(), absl::StatusCode::kInvalidArgument);
}

#else

TEST_F(PaletteJsonTest, JsonSupportDisabled) {
  GTEST_SKIP() << "JSON support is disabled";
}

#endif

}  // namespace
}  // namespace yaze::editor
