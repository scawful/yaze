#include <filesystem>
#include <string>

#include "app/editor/layout/layout_designer/dock_tree.h"
#include "app/editor/layout/layout_designer/dock_tree_json.h"
#include "app/editor/system/session/user_settings.h"
#include "gtest/gtest.h"
#include "nlohmann/json.hpp"

namespace yaze::editor {
namespace {

using ::yaze::editor::layout_designer::DockNode;
using ::yaze::editor::layout_designer::DockTree;
using ::yaze::editor::layout_designer::DockTreeFromJson;
using ::yaze::editor::layout_designer::DockTreeToJson;
using ::yaze::editor::layout_designer::PanelEntry;
using ::yaze::editor::layout_designer::SplitDirection;

std::filesystem::path TempSettingsPath(const std::string& slug) {
  auto dir =
      std::filesystem::temp_directory_path() / "yaze_named_layouts_persist";
  std::filesystem::create_directories(dir);
  return dir / (slug + "_settings.json");
}

std::string MakeSampleLayoutJson(const std::string& name) {
  DockTree tree(name);
  tree.root = DockNode::MakeSplit(SplitDirection::kLeft, 0.3f,
                                  DockNode::MakeLeaf({{"a", "A", "ICON_A"}}),
                                  DockNode::MakeLeaf({{"b", "B", "ICON_B"}}));
  return DockTreeToJson(tree).dump();
}

TEST(UserSettingsNamedLayoutsTest, DefaultsAreEmpty) {
  UserSettings settings;
  EXPECT_TRUE(settings.prefs().named_layouts.empty());
  EXPECT_TRUE(settings.prefs().last_applied_layout_name.empty());
}

TEST(UserSettingsNamedLayoutsTest, JsonRoundTripPreservesNamedLayouts) {
  auto path = TempSettingsPath("json_roundtrip");
  std::filesystem::remove(path);

  const std::string body = MakeSampleLayoutJson("Dungeon Expert");

  {
    UserSettings settings;
    settings.SetSettingsFilePathForTesting(path.string());
    settings.prefs().named_layouts["Dungeon Expert"] = body;
    settings.prefs().last_applied_layout_name = "Dungeon Expert";
    auto save_status = settings.Save();
    ASSERT_TRUE(save_status.ok()) << save_status.ToString();
  }

  UserSettings restored;
  restored.SetSettingsFilePathForTesting(path.string());
  auto load_status = restored.Load();
  ASSERT_TRUE(load_status.ok()) << load_status.ToString();

  ASSERT_EQ(restored.prefs().named_layouts.size(), 1u);
  ASSERT_TRUE(restored.prefs().named_layouts.contains("Dungeon Expert"));
  EXPECT_EQ(restored.prefs().last_applied_layout_name, "Dungeon Expert");

  // Content survives through parse + re-dump (whitespace may shift, so
  // compare by re-parsing through DockTreeFromJson instead of strcmp).
  auto original_tree = DockTreeFromJson(nlohmann::json::parse(body));
  ASSERT_TRUE(original_tree.ok()) << original_tree.status();
  auto restored_tree = DockTreeFromJson(nlohmann::json::parse(
      restored.prefs().named_layouts.at("Dungeon Expert")));
  ASSERT_TRUE(restored_tree.ok()) << restored_tree.status();
  EXPECT_EQ(restored_tree->name, original_tree->name);
  ASSERT_NE(restored_tree->root, nullptr);
  EXPECT_EQ(restored_tree->root->type, DockNode::Type::kSplit);
  EXPECT_EQ(restored_tree->root->split_direction, SplitDirection::kLeft);
  EXPECT_FLOAT_EQ(restored_tree->root->split_ratio, 0.3f);

  std::filesystem::remove(path);
}

TEST(UserSettingsNamedLayoutsTest, JsonRoundTripPreservesMalformedBody) {
  auto path = TempSettingsPath("malformed_roundtrip");
  std::filesystem::remove(path);

  const std::string malformed = "not valid json at all";

  {
    UserSettings settings;
    settings.SetSettingsFilePathForTesting(path.string());
    settings.prefs().named_layouts["Busted"] = malformed;
    auto save_status = settings.Save();
    ASSERT_TRUE(save_status.ok()) << save_status.ToString();
  }

  UserSettings restored;
  restored.SetSettingsFilePathForTesting(path.string());
  ASSERT_TRUE(restored.Load().ok());

  ASSERT_TRUE(restored.prefs().named_layouts.contains("Busted"));
  EXPECT_EQ(restored.prefs().named_layouts.at("Busted"), malformed);

  std::filesystem::remove(path);
}

TEST(UserSettingsNamedLayoutsTest, RevisionSixteenConvertsSavedLayouts) {
  UserSettings settings;
  auto& prefs = settings.prefs();
  // Start at revision 15 so the visibility-only saved_layouts entries are
  // preserved through earlier migrations (revision 4 is the last one that
  // clears them).
  prefs.panel_layout_defaults_revision = 15;
  prefs.saved_layouts["Legacy Workspace"]["dungeon.room_selector"] = true;
  prefs.saved_layouts["Legacy Workspace"]["dungeon.object_editor"] = true;
  prefs.saved_layouts["Legacy Workspace"]["dungeon.room_graphics"] = false;

  ASSERT_TRUE(settings.ApplyPanelLayoutDefaultsRevision(16));
  EXPECT_EQ(prefs.panel_layout_defaults_revision, 16);

  ASSERT_TRUE(prefs.named_layouts.contains("Legacy Workspace"));
  auto parsed = nlohmann::json::parse(
      prefs.named_layouts.at("Legacy Workspace"), nullptr, false);
  ASSERT_FALSE(parsed.is_discarded());
  auto tree = DockTreeFromJson(parsed);
  ASSERT_TRUE(tree.ok()) << tree.status();
  EXPECT_EQ(tree->name, "Legacy Workspace");
  ASSERT_NE(tree->root, nullptr);
  ASSERT_EQ(tree->root->type, DockNode::Type::kLeaf);
  EXPECT_EQ(tree->root->panels.size(), 2u);  // only the two visible entries
  std::string err;
  EXPECT_TRUE(tree->Validate(&err)) << err;
}

TEST(UserSettingsNamedLayoutsTest, RevisionSixteenDoesNotOverwriteExisting) {
  UserSettings settings;
  auto& prefs = settings.prefs();
  prefs.panel_layout_defaults_revision = 15;

  const std::string preserved = MakeSampleLayoutJson("Already Authored");
  prefs.named_layouts["Already Authored"] = preserved;
  prefs.saved_layouts["Already Authored"]["dungeon.room_selector"] = true;

  ASSERT_TRUE(settings.ApplyPanelLayoutDefaultsRevision(16));
  EXPECT_EQ(prefs.named_layouts.at("Already Authored"), preserved);
}

TEST(UserSettingsNamedLayoutsTest, RevisionSixteenNoSavedLayoutsIsNoOp) {
  UserSettings settings;
  auto& prefs = settings.prefs();
  prefs.panel_layout_defaults_revision = 15;
  EXPECT_TRUE(prefs.saved_layouts.empty());

  ASSERT_TRUE(settings.ApplyPanelLayoutDefaultsRevision(16));
  EXPECT_EQ(prefs.panel_layout_defaults_revision, 16);
  EXPECT_TRUE(prefs.named_layouts.empty());
}

TEST(UserSettingsNamedLayoutsTest, LatestRevisionIncludesSixteen) {
  // Guards against forgetting to bump the constant when adding a new
  // migration block.
  EXPECT_GE(UserSettings::kLatestPanelLayoutDefaultsRevision, 16);
}

}  // namespace
}  // namespace yaze::editor
