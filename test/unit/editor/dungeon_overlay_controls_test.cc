#include "app/editor/dungeon/dungeon_overlay_controls.h"

#include <set>

#include "gtest/gtest.h"

namespace yaze::editor {

TEST(DungeonOverlayControlsTest, SpecsAreUniqueAndNamed) {
  const auto& specs = GetDungeonOverlayControlSpecs();
  EXPECT_EQ(specs.size(), 10u);

  std::set<int> seen_ids;
  std::set<std::string> seen_labels;
  for (const auto& spec : specs) {
    EXPECT_NE(spec.label, nullptr);
    EXPECT_NE(std::string(spec.label), "");
    EXPECT_TRUE(seen_ids.insert(static_cast<int>(spec.id)).second);
    EXPECT_TRUE(seen_labels.insert(spec.label).second);
  }
}

TEST(DungeonOverlayControlsTest, SetAndGetRoundTripForAllControls) {
  DungeonCanvasViewer viewer;

  for (const auto& spec : GetDungeonOverlayControlSpecs()) {
    SetDungeonOverlayControlEnabled(viewer, spec.id, false);
    EXPECT_FALSE(GetDungeonOverlayControlEnabled(viewer, spec.id))
        << "control id " << static_cast<int>(spec.id);

    SetDungeonOverlayControlEnabled(viewer, spec.id, true);
    EXPECT_TRUE(GetDungeonOverlayControlEnabled(viewer, spec.id))
        << "control id " << static_cast<int>(spec.id);
  }
}

}  // namespace yaze::editor
