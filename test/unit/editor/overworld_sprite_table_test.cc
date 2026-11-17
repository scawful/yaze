#include "app/editor/overworld/entity/entity.h"

#include <cctype>

#include "gtest/gtest.h"
#include "zelda3/resource_labels.h"

namespace yaze::editor {

TEST(OverworldSpriteTableTest, UsesProjectLabelsForReassignedOracleSprites) {
  zelda3::ResourceLabelProvider labels;
  zelda3::ResourceLabelProvider::ProjectLabels project;
  // Oracle reassigned vanilla slot $22 (Hopping Bulb Plant) to Tingle.
  project["sprite"]["34"] = "Tingle";
  labels.SetProjectLabels(&project);

  const auto items = BuildSpriteTableItems(labels);

  ASSERT_EQ(items.size(), 256u);
  EXPECT_EQ(items[0x22].id, 0x22);
  EXPECT_EQ(items[0x22].name, "Tingle");
  EXPECT_NE(items[0x21].name, "Tingle");
}

TEST(OverworldSpriteTableTest, AcceptsHexPrefixedRegistryKeys) {
  zelda3::ResourceLabelProvider labels;
  zelda3::ResourceLabelProvider::ProjectLabels project;
  project["sprite"]["0x76"] = "Impa";
  labels.SetProjectLabels(&project);

  EXPECT_EQ(BuildSpriteTableItems(labels)[0x76].name, "Impa");
}

TEST(OverworldSpriteTableTest, VanillaFallbackHasNoIdPrefix) {
  zelda3::ResourceLabelProvider labels;

  const auto items = BuildSpriteTableItems(labels);

  ASSERT_EQ(items.size(), 256u);
  for (const auto& item : items) {
    EXPECT_FALSE(item.name.size() > 2 && item.name[2] == ' ' &&
                 std::isxdigit(static_cast<unsigned char>(item.name[0])))
        << "id " << item.id << " kept its id prefix: " << item.name;
  }
}

}  // namespace yaze::editor
