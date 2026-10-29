#include "zelda3/dungeon/object_templates.h"

#include <algorithm>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <limits>
#include <set>
#include <tuple>

#include "gtest/gtest.h"

namespace yaze::zelda3 {
namespace {

ObjectTemplate Template(std::vector<TemplateObject> objects) {
  return {"Test pattern", "Ordered object assembly", "Test",
          std::move(objects)};
}

bool HasConflict(const ObjectTemplatePlacementPlan& plan,
                 ObjectTemplatePlanConflictKind kind) {
  return std::any_of(
      plan.conflicts.begin(), plan.conflicts.end(),
      [kind](const auto& conflict) { return conflict.kind == kind; });
}

class ObjectTemplatePlanTest : public testing::Test {
 protected:
  void SetUp() override {
    ASSERT_TRUE(rom_.LoadFromData(std::vector<uint8_t>(0x200000, 0)).ok());
  }
  ObjectTemplatePlacementPlan Plan(
      const ObjectTemplate& object_template, ObjectTemplateTile8Anchor anchor,
      std::span<const RoomObject> existing = {},
      const ObjectTemplatePlanOptions& options = {}) {
    return PlanObjectTemplatePlacement(&rom_, 0x36, object_template, anchor,
                                       existing, options);
  }
  Rom rom_;
};

TEST_F(ObjectTemplatePlanTest, StrictReaderRejectsPartialAndAmbiguousPayloads) {
  const std::string prefix =
      R"({"name":"A","description":"","category":"Test","objects":[)";
  const std::string object =
      R"({"id":4,"rel_x":0,"rel_y":0,"size":3,"layer":0})";
  ASSERT_TRUE(ParseObjectTemplateJson(prefix + object + "]}").ok());
  const auto wrap = [&](const std::string& entry) {
    return prefix + entry + "]}";
  };
  for (
      const std::string& payload : std::vector<std::string>{
          "{}", prefix + object + ",{}]}", prefix + object + "]} trailing",
          R"({"name":"A","name":"B","description":"","category":"Test","objects":[)" +
              object + "]}",
          wrap(R"({"id":4,"rel_x":0,"rel_y":0,"size":3.0,"layer":0})"),
          wrap(R"({"id":4,"rel_x":true,"rel_y":0,"size":3,"layer":0})"),
          wrap(
              R"({"id":18446744073709551615,"rel_x":0,"rel_y":0,"size":0,"layer":0})"),
          wrap(R"({"id":4,"rel_x":64,"rel_y":0,"size":0,"layer":0})"),
          wrap(
              R"({"id":4,"rel_x":0,"rel_y":0,"size":3,"layer":0,"rule":"ignored"})")}) {
    EXPECT_FALSE(ParseObjectTemplateJson(payload).ok()) << payload;
  }
}

TEST_F(ObjectTemplatePlanTest, StrictFileReaderReturnsNoPartialTemplate) {
  const auto path =
      std::filesystem::temp_directory_path() /
      ("plan-" +
       std::to_string(
           std::chrono::steady_clock::now().time_since_epoch().count()));
  ASSERT_FALSE(LoadObjectTemplateFile(path.string()).ok());
  {
    std::ofstream output(path);
    output << R"({"name":"A","description":"","category":"Test","objects":[)"
           << R"({"id":4,"rel_x":0,"rel_y":0,"size":3,"layer":0}]})";
  }
  EXPECT_TRUE(LoadObjectTemplateFile(path.string()).ok());
  {
    std::ofstream output(path, std::ios::app);
    output << "{}";
  }
  EXPECT_FALSE(LoadObjectTemplateFile(path.string()).ok());
  std::error_code error;
  std::filesystem::remove(path, error);
}

TEST_F(ObjectTemplatePlanTest, SwampIslandRetainsOrderedCompletePerimeter) {
  // Vanilla Swamp36 original ordered indices58..65, relative to (23,21).
  const auto island = Template({{0x10F, 0, 0, 0, 0},
                                {0x04, 4, 0, 3, 0},
                                {0x10D, 12, 0, 0, 0},
                                {0x64, 0, 4, 7, 0},
                                {0x10E, 0, 20, 0, 0},
                                {0x03, 4, 20, 3, 0},
                                {0x10C, 12, 20, 0, 0},
                                {0x63, 12, 4, 7, 0}});
  const auto before = rom_.vector();
  ObjectTemplatePlanOptions options;
  options.clearance.push_back({{27, 25, 8, 16}, std::nullopt});
  const auto plan = Plan(island, {23, 21}, {}, options);
  ASSERT_TRUE(plan.valid);
  ASSERT_EQ(plan.placements.size(), 8);
  ASSERT_EQ(plan.footprints.size(), 8);
  std::set<std::tuple<int, int, int>> cells;
  for (size_t i = 0; i < plan.footprints.size(); ++i) {
    EXPECT_EQ(plan.placements[i].id_, island.objects[i].id);
    EXPECT_EQ(plan.footprints[i].object_index, i);
    for (const auto& cell : plan.footprints[i].cells) {
      EXPECT_GE(cell.x, 23);
      EXPECT_LT(cell.x, 39);
      EXPECT_GE(cell.y, 21);
      EXPECT_LT(cell.y, 45);
      cells.emplace(cell.layer, cell.y, cell.x);
    }
  }
  EXPECT_EQ(cells.size(), 512);  // 256 ring cells on each physical BG.
  const auto again = Plan(island, {23, 21}, {}, options);
  ASSERT_TRUE(again.valid);
  for (size_t i = 0; i < plan.placements.size(); ++i) {
    const auto first = plan.placements[i].EncodeObjectToBytes();
    const auto second = again.placements[i].EncodeObjectToBytes();
    EXPECT_EQ(std::make_tuple(first.b1, first.b2, first.b3),
              std::make_tuple(second.b1, second.b2, second.b3));
  }
  EXPECT_EQ(rom_.vector(), before);
}

TEST_F(ObjectTemplatePlanTest, InvalidTranslatedAnchorNeverReturnsASubset) {
  const auto object_template = Template({{4, -1, 0, 0, 0}, {4, 8, 0, 0, 0}});
  const auto plan = Plan(object_template, {0, 10});
  EXPECT_FALSE(plan.valid);
  EXPECT_TRUE(plan.placements.empty());
  EXPECT_TRUE(
      HasConflict(plan, ObjectTemplatePlanConflictKind::kInvalidPlacement));
  EXPECT_FALSE(
      Plan(object_template, {std::numeric_limits<int>::max(), 0}).valid);
}

TEST_F(ObjectTemplatePlanTest, RenderedBoundsRejectAnchorOnlyAcceptance) {
  const auto object_template =
      Template({{4, 0, 0, 3, 0}});  // Eight cells wide.
  const auto plan = Plan(object_template, {60, 10});
  ASSERT_EQ(plan.placements.size(), 1);
  EXPECT_FALSE(plan.valid);
  EXPECT_TRUE(HasConflict(plan, ObjectTemplatePlanConflictKind::kOutsideRoom));
  // Preserve the existing UI instantiation contract while strict planning is new.
  ObjectTemplateManager manager;
  EXPECT_EQ(
      manager.InstantiateTemplate(object_template, 60, 10, nullptr).size(), 1);
}

TEST_F(ObjectTemplatePlanTest, RejectsAliasingAndDrawListReordering) {
  for (const auto& object : std::vector<TemplateObject>{{0xF8, 0, 0, 0, 0},
                                                        {0x108, 0, 0, 1, 0},
                                                        {0xF99, 0, 0, 0, 0},
                                                        {4, 0, 0, 0, 3}}) {
    EXPECT_FALSE(Plan(Template({object}), {10, 10}).valid);
  }
  EXPECT_FALSE(Plan(Template({{4, 0, 0, 0, 0}}), {63, 10}).valid);
  const auto order =
      Plan(Template({{0xC9, 0, 0, 0, 1}, {0xC9, 8, 0, 0, 0}}), {10, 10});
  EXPECT_FALSE(order.valid);
  EXPECT_EQ(order.placements.size(), 2);
  EXPECT_TRUE(
      HasConflict(order, ObjectTemplatePlanConflictKind::kInvalidPlacement));
}

TEST_F(ObjectTemplatePlanTest,
       ClearanceUsesPhysicalPlanesIncludingFinalRepaint) {
  const auto object_template = Template({{0xC9, 0, 0, 0, 2}});
  ObjectTemplatePlanOptions options;
  options.clearance.push_back({{20, 20, 4, 4}, 1});
  EXPECT_TRUE(Plan(object_template, {20, 20}, {}, options).valid);
  options.clearance[0].plane = 0;
  const auto blocked = Plan(object_template, {20, 20}, {}, options);
  EXPECT_FALSE(blocked.valid);
  ASSERT_EQ(blocked.conflicts.size(), 1);
  EXPECT_EQ(blocked.conflicts[0].cell_count, 16);
  options.clearance[0].plane = 2;
  EXPECT_FALSE(Plan(object_template, {20, 20}, {}, options).valid);
}

TEST_F(ObjectTemplatePlanTest, ExplicitExistingOverlapPreservesOrderedRepaint) {
  const auto object_template =
      Template({{0xC9, 0, 0, 15, 0}, {0xC9, 0, 0, 15, 0}});
  const std::vector<RoomObject> existing = {RoomObject(0xC9, 20, 20, 15, 0)};
  const auto blocked = Plan(object_template, {20, 20}, existing);
  EXPECT_FALSE(blocked.valid);
  ASSERT_EQ(blocked.conflicts.size(), 3);
  EXPECT_EQ(blocked.conflicts[0].cell_count, 256);
  EXPECT_EQ(blocked.conflicts[0].cells.size(), 64);
  EXPECT_FALSE(blocked.conflicts.back().blocking);
  ObjectTemplatePlanOptions options;
  options.allowed_existing_pairs = {{0, 0}};
  EXPECT_FALSE(Plan(object_template, {20, 20}, existing, options).valid);
  options.allowed_existing_pairs.push_back({1, 0});
  EXPECT_TRUE(Plan(object_template, {20, 20}, existing, options).valid);
  options.allowed_existing_pairs.clear();
  options.allowed_existing_indices = {0};
  EXPECT_TRUE(Plan(object_template, {20, 20}, existing, options).valid);
  const std::vector<RoomObject> other_plane = {RoomObject(0xC9, 20, 20, 15, 1)};
  EXPECT_TRUE(Plan(object_template, {20, 20}, other_plane).valid);
  options.allowed_existing_indices = {1};
  EXPECT_FALSE(Plan(object_template, {20, 20}, existing, options).valid);
}

}  // namespace
}  // namespace yaze::zelda3
