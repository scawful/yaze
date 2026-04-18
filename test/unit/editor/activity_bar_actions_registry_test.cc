#include "app/editor/menu/activity_bar_actions_registry.h"

#include <string>
#include <vector>

#include "gtest/gtest.h"

namespace yaze::editor {
namespace {

std::vector<std::string> CollectIds(const MoreActionsRegistry& registry) {
  std::vector<std::string> ids;
  registry.ForEach(
      [&](const MoreAction& action) { ids.push_back(action.id); });
  return ids;
}

TEST(MoreActionsRegistryTest, StartsEmpty) {
  MoreActionsRegistry registry;
  EXPECT_TRUE(registry.empty());
  EXPECT_EQ(registry.size(), 0u);
  int seen = 0;
  registry.ForEach([&](const MoreAction&) { ++seen; });
  EXPECT_EQ(seen, 0);
}

TEST(MoreActionsRegistryTest, RegisterPreservesInsertionOrder) {
  MoreActionsRegistry registry;
  registry.Register({"a", "A", nullptr, [] {}, {}});
  registry.Register({"b", "B", nullptr, [] {}, {}});
  registry.Register({"c", "C", nullptr, [] {}, {}});
  EXPECT_EQ(CollectIds(registry), (std::vector<std::string>{"a", "b", "c"}));
}

TEST(MoreActionsRegistryTest, RegisterWithExistingIdReplacesInPlace) {
  MoreActionsRegistry registry;
  int original_calls = 0;
  int replacement_calls = 0;
  registry.Register(
      {"toggle", "Original", nullptr, [&] { ++original_calls; }, {}});
  registry.Register({"middle", "Middle", nullptr, [] {}, {}});
  // Re-registering "toggle" must not move it to the tail.
  registry.Register(
      {"toggle", "Replacement", nullptr, [&] { ++replacement_calls; }, {}});

  auto ids = CollectIds(registry);
  ASSERT_EQ(ids.size(), 2u);
  EXPECT_EQ(ids[0], "toggle");
  EXPECT_EQ(ids[1], "middle");

  // Invoking the now-only "toggle" action must hit the replacement, not the
  // original callback.
  registry.ForEach([&](const MoreAction& a) {
    if (a.id == "toggle" && a.on_invoke) a.on_invoke();
  });
  EXPECT_EQ(original_calls, 0);
  EXPECT_EQ(replacement_calls, 1);
}

TEST(MoreActionsRegistryTest, UnregisterRemovesById) {
  MoreActionsRegistry registry;
  registry.Register({"a", "A", nullptr, [] {}, {}});
  registry.Register({"b", "B", nullptr, [] {}, {}});
  registry.Register({"c", "C", nullptr, [] {}, {}});

  registry.Unregister("b");
  EXPECT_EQ(CollectIds(registry), (std::vector<std::string>{"a", "c"}));

  // Unregistering an unknown id is a no-op.
  registry.Unregister("does-not-exist");
  EXPECT_EQ(CollectIds(registry), (std::vector<std::string>{"a", "c"}));
}

TEST(MoreActionsRegistryTest, ClearDropsEverything) {
  MoreActionsRegistry registry;
  registry.Register({"a", "A", nullptr, [] {}, {}});
  registry.Register({"b", "B", nullptr, [] {}, {}});
  registry.Clear();
  EXPECT_TRUE(registry.empty());
  EXPECT_EQ(registry.size(), 0u);
}

TEST(MoreActionsRegistryTest, EnabledFnGatesInvocationByContract) {
  // ForEach itself doesn't filter by enabled_fn — consumers do. Verify that
  // enabled_fn is surfaced untouched so callers can use it.
  MoreActionsRegistry registry;
  bool gate = false;
  registry.Register({"a", "A", nullptr, [] {}, [&] { return gate; }});

  bool observed = true;
  registry.ForEach([&](const MoreAction& action) {
    if (action.id == "a" && action.enabled_fn) {
      observed = action.enabled_fn();
    }
  });
  EXPECT_FALSE(observed);

  gate = true;
  registry.ForEach([&](const MoreAction& action) {
    if (action.id == "a" && action.enabled_fn) {
      observed = action.enabled_fn();
    }
  });
  EXPECT_TRUE(observed);
}

}  // namespace
}  // namespace yaze::editor
