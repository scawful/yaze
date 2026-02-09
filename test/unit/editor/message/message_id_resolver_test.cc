#include "app/editor/message/message_id_resolver.h"

#include "gtest/gtest.h"

namespace yaze::editor {

TEST(MessageIdResolverTest, ResolvesVanillaIds) {
  auto resolved = ResolveMessageDisplayId(/*display_id=*/10,
                                          /*vanilla_count=*/200,
                                          /*expanded_base_id=*/400,
                                          /*expanded_count=*/50);
  ASSERT_TRUE(resolved.has_value());
  EXPECT_FALSE(resolved->is_expanded);
  EXPECT_EQ(resolved->index, 10);
  EXPECT_EQ(resolved->display_id, 10);
}

TEST(MessageIdResolverTest, ResolvesExpandedIds) {
  auto resolved = ResolveMessageDisplayId(/*display_id=*/405,
                                          /*vanilla_count=*/200,
                                          /*expanded_base_id=*/400,
                                          /*expanded_count=*/50);
  ASSERT_TRUE(resolved.has_value());
  EXPECT_TRUE(resolved->is_expanded);
  EXPECT_EQ(resolved->index, 5);
  EXPECT_EQ(resolved->display_id, 405);
}

TEST(MessageIdResolverTest, RejectsGapAndOutOfRangeIds) {
  // Gap between vanilla_count and expanded_base_id.
  EXPECT_FALSE(ResolveMessageDisplayId(/*display_id=*/250,
                                       /*vanilla_count=*/200,
                                       /*expanded_base_id=*/400,
                                       /*expanded_count=*/50)
                   .has_value());

  // Out of expanded range.
  EXPECT_FALSE(ResolveMessageDisplayId(/*display_id=*/999,
                                       /*vanilla_count=*/200,
                                       /*expanded_base_id=*/400,
                                       /*expanded_count=*/50)
                   .has_value());

  // Negative.
  EXPECT_FALSE(ResolveMessageDisplayId(/*display_id=*/-1,
                                       /*vanilla_count=*/200,
                                       /*expanded_base_id=*/400,
                                       /*expanded_count=*/50)
                   .has_value());
}

}  // namespace yaze::editor

