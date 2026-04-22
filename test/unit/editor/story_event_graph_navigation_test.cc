#include "app/editor/oracle/story_event_graph_navigation.h"

#include <optional>

#include <gtest/gtest.h>

namespace yaze::editor {
namespace {

TEST(StoryEventGraphNavigationTest, PrefersValidOverworldTarget) {
  EXPECT_EQ(ResolveStoryLocationMapJumpTarget(std::optional<int>(0x33),
                                              std::optional<int>(0x80)),
            std::optional<int>(0x33));
}

TEST(StoryEventGraphNavigationTest, FallsBackToValidSpecialWorldTarget) {
  EXPECT_EQ(ResolveStoryLocationMapJumpTarget(std::optional<int>(0x103),
                                              std::optional<int>(0x80)),
            std::optional<int>(0x80));
}

TEST(StoryEventGraphNavigationTest, RejectsUnsupportedTargets) {
  EXPECT_EQ(ResolveStoryLocationMapJumpTarget(std::optional<int>(-1),
                                              std::optional<int>(0x103)),
            std::nullopt);
}

}  // namespace
}  // namespace yaze::editor
