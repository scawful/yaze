#include "zelda3/dungeon/object_templates.h"

#include <algorithm>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <limits>
#include <set>
#include <tuple>

#include "core/features.h"
#include "gtest/gtest.h"
#include "zelda3/dungeon/draw_routines/draw_routine_registry.h"
#include "zelda3/dungeon/geometry/object_geometry.h"

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

ObjectTemplate DeclareLoop(ObjectTemplate object_template,
                           const std::string& name = "dark_contour") {
  ObjectTemplateRetainingWallLoop loop;
  loop.name = name;
  for (size_t i = 0; i < object_template.objects.size(); ++i)
    loop.members.push_back(i);
  object_template.retaining_wall_loops.push_back(std::move(loop));
  return object_template;
}

ObjectTemplate IslandLoop() {
  // Complete vanilla36 source58..65, relative to(23,21).
  return DeclareLoop(Template({{0x10F, 0, 0, 0, 0},
                               {0x004, 4, 0, 3, 0},
                               {0x10D, 12, 0, 0, 0},
                               {0x064, 0, 4, 7, 0},
                               {0x10E, 0, 20, 0, 0},
                               {0x003, 4, 20, 3, 0},
                               {0x10C, 12, 20, 0, 0},
                               {0x063, 12, 4, 7, 0}}));
}

ObjectTemplate SteppedLoop() {
  // Existing vanilla36 full dark outer contour, source10..57 inclusive.
  // Coordinates and order are retained, without a ROM or graphics fixture.
  return DeclareLoop(Template(
      {{0x108, 5, 21, 0, 0},   {0x003, 9, 21, 3, 0},  {0x10C, 17, 21, 0, 0},
       {0x063, 17, 11, 4, 0},  {0x108, 17, 7, 0, 0},  {0x003, 21, 7, 0, 0},
       {0x10A, 23, 7, 0, 0},   {0x064, 23, 11, 1, 0}, {0x10E, 23, 15, 0, 0},
       {0x003, 27, 15, 4, 0},  {0x10C, 37, 15, 0, 0}, {0x063, 37, 11, 1, 0},
       {0x108, 37, 7, 0, 0},   {0x003, 41, 7, 3, 0},  {0x10A, 49, 7, 0, 0},
       {0x064, 49, 11, 4, 0},  {0x10E, 49, 21, 0, 0}, {0x003, 53, 21, 0, 0},
       {0x10A, 55, 21, 0, 0},  {0x064, 55, 25, 0, 0}, {0x10B, 55, 27, 0, 0},
       {0x004, 45, 27, 4, 0},  {0x10F, 41, 27, 0, 0}, {0x064, 41, 31, 1, 0},
       {0x10E, 41, 35, 0, 0},  {0x003, 45, 35, 4, 0}, {0x10A, 55, 35, 0, 0},
       {0x064, 55, 39, 0, 0},  {0x10B, 55, 41, 0, 0}, {0x004, 47, 41, 3, 0},
       {0x10F, 43, 41, 0, 0},  {0x064, 43, 45, 4, 0}, {0x10B, 43, 55, 0, 0},
       {0x004, 19, 55, 11, 0}, {0x109, 15, 55, 0, 0}, {0x063, 15, 45, 4, 0},
       {0x10D, 15, 41, 0, 0},  {0x004, 9, 41, 2, 0},  {0x109, 5, 41, 0, 0},
       {0x063, 5, 39, 0, 0},   {0x108, 5, 35, 0, 0},  {0x003, 9, 35, 3, 0},
       {0x10C, 17, 35, 0, 0},  {0x063, 17, 31, 1, 0}, {0x10D, 17, 27, 0, 0},
       {0x004, 9, 27, 3, 0},   {0x109, 5, 27, 0, 0},  {0x063, 5, 25, 0, 0}}));
}

class ObjectTemplatePlanTest : public testing::Test {
 protected:
  void SetUp() override {
    flags_ = core::FeatureFlags::get();
    core::FeatureFlags::get().kEnableCustomObjects = false;
    DrawRoutineRegistry::Get().RefreshFeatureFlagMappings();
    ObjectGeometry::Get().ClearCache();
    ASSERT_TRUE(rom_.LoadFromData(std::vector<uint8_t>(0x200000, 0)).ok());
  }
  void TearDown() override {
    core::FeatureFlags::get() = flags_;
    DrawRoutineRegistry::Get().RefreshFeatureFlagMappings();
    ObjectGeometry::Get().ClearCache();
  }
  ObjectTemplatePlacementPlan Plan(
      const ObjectTemplate& object_template, ObjectTemplateTile8Anchor anchor,
      std::span<const RoomObject> existing = {},
      const ObjectTemplatePlanOptions& options = {}) {
    return PlanObjectTemplatePlacement(&rom_, 0x36, object_template, anchor,
                                       existing, options);
  }
  Rom rom_;
  core::FeatureFlags::Flags flags_;
};

TEST_F(ObjectTemplatePlanTest, DeclaredFullSteppedAndIslandCyclesPass) {
  for (const auto& object_template : {SteppedLoop(), IslandLoop()}) {
    const auto plan = Plan(object_template, {0, 0});
    EXPECT_TRUE(plan.valid);
    ASSERT_EQ(plan.retaining_wall_loop_checks.size(), 1);
    const auto& check = plan.retaining_wall_loop_checks.front();
    EXPECT_EQ(check.status, "closed_ports_not_final_pixels");
    EXPECT_EQ(check.required_ports.size(), object_template.objects.size() * 2);
    EXPECT_EQ(check.matched_connection_count, object_template.objects.size());
  }
}

TEST_F(ObjectTemplatePlanTest, MissingMovedAndFlippedCornersKeepPortWitnesses) {
  auto missing = IslandLoop();
  missing.objects.erase(missing.objects.begin() + 6);
  missing.retaining_wall_loops.clear();
  missing = DeclareLoop(std::move(missing));
  auto moved = IslandLoop();
  moved.objects[6].rel_x += 1;
  auto flipped = IslandLoop();
  flipped.objects[6].id = 0x10E;
  for (const auto& object_template : {missing, moved, flipped}) {
    const auto plan = Plan(object_template, {23, 21});
    EXPECT_FALSE(plan.valid);
    EXPECT_TRUE(
        HasConflict(plan, ObjectTemplatePlanConflictKind::kAssemblyConnection));
    ASSERT_EQ(plan.retaining_wall_loop_checks.size(), 1);
    EXPECT_EQ(plan.retaining_wall_loop_checks[0].status, "blocked");
    EXPECT_EQ(plan.retaining_wall_loop_checks[0].required_ports.size(),
              object_template.objects.size() * 2);
    EXPECT_TRUE(std::any_of(
        plan.conflicts.begin(), plan.conflicts.end(), [](const auto& conflict) {
          return conflict.kind ==
                     ObjectTemplatePlanConflictKind::kAssemblyConnection &&
                 conflict.expected_region && conflict.expected_region->x > 0 &&
                 conflict.expected_region->y > 0;
        }));
  }
}

TEST_F(ObjectTemplatePlanTest, CornerShortcutsAndDisjointCyclesAreBlocked) {
  const auto shortcut = DeclareLoop(Template({{0x10F, 0, 0, 0, 0},
                                              {0x10D, 4, 0, 0, 0},
                                              {0x063, 4, 4, 1, 0},
                                              {0x10C, 4, 8, 0, 0},
                                              {0x10E, 0, 8, 0, 0},
                                              {0x064, 0, 4, 1, 0}}));
  const auto short_plan = Plan(shortcut, {10, 10});
  EXPECT_FALSE(short_plan.valid);
  ASSERT_EQ(short_plan.retaining_wall_loop_checks.size(), 1);
  EXPECT_TRUE(std::any_of(
      short_plan.retaining_wall_loop_checks[0].diagnostics.begin(),
      short_plan.retaining_wall_loop_checks[0].diagnostics.end(),
      [](const auto& message) {
        return message.find("corner-to-corner") != std::string::npos;
      }));
  auto disjoint = IslandLoop();
  auto second = disjoint.objects;
  for (auto& object : second)
    object.rel_x += 24;
  disjoint.objects.insert(disjoint.objects.end(), second.begin(), second.end());
  disjoint.retaining_wall_loops.clear();
  disjoint = DeclareLoop(std::move(disjoint));
  const auto plan = Plan(disjoint, {4, 4});
  EXPECT_FALSE(plan.valid);
  ASSERT_EQ(plan.retaining_wall_loop_checks.size(), 1);
  EXPECT_EQ(plan.retaining_wall_loop_checks[0].matched_connection_count, 16);
  EXPECT_EQ(plan.retaining_wall_loop_checks[0].status, "blocked");
}

TEST_F(ObjectTemplatePlanTest, DeclaredCyclesAllowProvenStraightContinuation) {
  auto object_template = IslandLoop();
  // Replace width8 bottom003 by two adjacent width4 members, retaining cycle.
  object_template.objects[5].size = 1;
  object_template.objects.push_back({0x003, 8, 20, 1, 0});
  object_template.retaining_wall_loops[0].members.push_back(8);
  const auto plan = Plan(object_template, {23, 21});
  EXPECT_TRUE(plan.valid);
  ASSERT_EQ(plan.retaining_wall_loop_checks.size(), 1);
  EXPECT_EQ(plan.retaining_wall_loop_checks[0].matched_connection_count, 9);
}

TEST_F(ObjectTemplatePlanTest, APortClosedCycleStillRejectsAnInteriorCrossing) {
  const auto crossed = DeclareLoop(Template({{0x10F, 5, 5, 0, 0},
                                             {0x004, 9, 5, 12, 0},
                                             {0x10D, 35, 5, 0, 0},
                                             {0x063, 35, 9, 12, 0},
                                             {0x10C, 35, 35, 0, 0},
                                             {0x003, 19, 35, 7, 0},
                                             {0x10E, 15, 35, 0, 0},
                                             {0x064, 15, 19, 7, 0},
                                             {0x10F, 15, 15, 0, 0},
                                             {0x004, 19, 15, 12, 0},
                                             {0x10D, 45, 15, 0, 0},
                                             {0x063, 45, 19, 12, 0},
                                             {0x10C, 45, 45, 0, 0},
                                             {0x003, 9, 45, 8, 0},
                                             {0x003, 27, 45, 8, 0},
                                             {0x10E, 5, 45, 0, 0},
                                             {0x064, 5, 9, 8, 0},
                                             {0x064, 5, 27, 8, 0}}));
  const auto plan = Plan(crossed, {0, 0});
  EXPECT_FALSE(plan.valid);
  ASSERT_EQ(plan.retaining_wall_loop_checks.size(), 1);
  EXPECT_EQ(plan.retaining_wall_loop_checks[0].matched_connection_count, 18);
  EXPECT_TRUE(std::any_of(
      plan.conflicts.begin(), plan.conflicts.end(), [](const auto& conflict) {
        return conflict.kind ==
                   ObjectTemplatePlanConflictKind::kAssemblyConnection &&
               conflict.cell_count != 0 && conflict.other_template_index &&
               conflict.message.find("members overlap") != std::string::npos;
      }));
}

TEST_F(ObjectTemplatePlanTest,
       LaterGrayWallBlocksButEarlierGrayCanBeRepainted) {
  auto bytes = rom_.vector();
  // Distinct valid synthetic words make the gray001 repaint observably
  // incompatible with the native dark003/108/10A shoulder of the full loop.
  bytes[kRoomObjectSubtype1 + 2] = 0x40;
  for (size_t i = 0; i < 64; ++i) {
    bytes[kRoomObjectTileAddress + i * 2] = static_cast<uint8_t>(i + 1);
    bytes[kRoomObjectTileAddress + i * 2 + 1] = 0x20;
  }
  ASSERT_TRUE(rom_.LoadFromData(bytes).ok());
  auto later = SteppedLoop();
  later.objects.push_back({0x001, 21, 7, 1, 2});
  const auto blocked = Plan(later, {0, 0});
  EXPECT_FALSE(blocked.valid);
  EXPECT_TRUE(HasConflict(blocked,
                          ObjectTemplatePlanConflictKind::kAssemblyConnection));
  ASSERT_EQ(blocked.retaining_wall_loop_checks.size(), 1);
  EXPECT_EQ(blocked.retaining_wall_loop_checks[0].matched_connection_count, 48);
  EXPECT_EQ(blocked.retaining_wall_loop_checks[0].status, "blocked");
  EXPECT_TRUE(std::any_of(
      blocked.conflicts.begin(), blocked.conflicts.end(),
      [](const auto& conflict) {
        return conflict.kind ==
                   ObjectTemplatePlanConflictKind::kAssemblyConnection &&
               conflict.cell_count == 8 && conflict.other_template_index == 48;
      }));
  auto earlier = SteppedLoop();
  earlier.objects.insert(earlier.objects.begin(), {0x001, 21, 7, 1, 0});
  for (auto& member : earlier.retaining_wall_loops[0].members)
    ++member;
  const auto admitted = Plan(earlier, {0, 0});
  EXPECT_TRUE(admitted.valid);
  ASSERT_EQ(admitted.retaining_wall_loop_checks.size(), 1);
  EXPECT_EQ(admitted.retaining_wall_loop_checks[0].status,
            "closed_ports_not_final_pixels");
}

TEST_F(ObjectTemplatePlanTest,
       LoopDeclarationsParseStrictlyAndManagerRoundTrips) {
  const auto directory =
      std::filesystem::temp_directory_path() /
      ("retaining-loop-" +
       std::to_string(
           std::chrono::steady_clock::now().time_since_epoch().count()));
  ASSERT_FALSE(std::filesystem::exists(directory));
  ObjectTemplateManager writer;
  auto source = IslandLoop();
  source.name = "Retaining loop";
  ASSERT_TRUE(writer.SaveTemplate(source, directory.string()).ok());
  const auto filename = directory / "Retaining_loop.json";
  const auto loaded = LoadObjectTemplateFile(filename.string());
  ASSERT_TRUE(loaded.ok()) << loaded.status();
  ASSERT_EQ(loaded->retaining_wall_loops.size(), 1);
  EXPECT_EQ(loaded->retaining_wall_loops[0].members,
            source.retaining_wall_loops[0].members);
  ObjectTemplateManager reader;
  ASSERT_TRUE(reader.LoadTemplates(directory.string()).ok());
  ASSERT_EQ(reader.GetTemplates().size(), 1);
  EXPECT_EQ(reader.GetTemplates()[0].retaining_wall_loops[0].name,
            "dark_contour");
  std::ifstream input(filename);
  const std::string payload((std::istreambuf_iterator<char>(input)), {});
  input.close();
  for (const std::string& bad : {"true", "1.0", "-1", "99"}) {
    auto invalid = payload;
    const auto begin = invalid.find("\"members\"");
    const auto member = invalid.find('0', begin);
    ASSERT_NE(member, std::string::npos);
    invalid.replace(member, 1, bad);
    EXPECT_FALSE(ParseObjectTemplateJson(invalid).ok());
  }
  auto duplicate = source;
  duplicate.retaining_wall_loops[0].members.push_back(0);
  EXPECT_FALSE(Plan(duplicate, {23, 21}).valid);
  auto excess = source;
  for (int i = 1; i < 65; ++i)
    excess.retaining_wall_loops.push_back(
        {"loop_" + std::to_string(i), source.retaining_wall_loops[0].members});
  EXPECT_FALSE(
      Plan(excess, {23, 21}).valid);  // 520 memberships, under128 loops.
  EXPECT_TRUE(std::filesystem::remove(filename));
  EXPECT_TRUE(std::filesystem::remove(directory));
}

TEST_F(ObjectTemplatePlanTest, ZoraJoinLossCannotBeWaivedByOverlapPairs) {
  // Original vector identities differ from painter positions after list sort.
  const std::vector<RoomObject> existing = {RoomObject(0x10C, 37, 44, 0),
                                            RoomObject(0x003, 21, 44, 7)};
  const auto graft = Template({{0x064, 0, 0, 1, 2}, {0x10E, 0, 4, 0, 2}});
  ObjectTemplatePlanOptions options;
  options.allowed_existing_pairs = {{0, 0}, {1, 0}};
  const auto before = rom_.vector();
  const auto plan = Plan(graft, {37, 43}, existing, options);
  EXPECT_FALSE(plan.valid);
  ASSERT_EQ(plan.retaining_wall_join_issues.size(), 1);
  const auto& issue = plan.retaining_wall_join_issues.front();
  EXPECT_EQ(issue.face_index, 1);
  EXPECT_EQ(issue.corner_index, 0);
  EXPECT_EQ(issue.overwriters[0].object_index, 2);
  EXPECT_EQ(issue.overwriters[1].object_index, 3);
  EXPECT_EQ(issue.upper_cells, 16);
  EXPECT_EQ(issue.lower_cells, 16);
  const auto conflict = std::find_if(
      plan.conflicts.begin(), plan.conflicts.end(), [](const auto& value) {
        return value.kind == ObjectTemplatePlanConflictKind::kWallJoinLoss;
      });
  ASSERT_NE(conflict, plan.conflicts.end());
  EXPECT_TRUE(conflict->blocking);
  EXPECT_EQ(conflict->template_index, 0);
  EXPECT_EQ(conflict->existing_index, 0);
  EXPECT_EQ(conflict->cell_count, 32);
  EXPECT_EQ(rom_.vector(), before);
}

TEST_F(ObjectTemplatePlanTest, ExistingJoinLossDoesNotBlockAnUnrelatedPlan) {
  const std::vector<RoomObject> existing = {
      RoomObject(0x003, 21, 44, 7), RoomObject(0x10C, 37, 44, 0),
      RoomObject(0x064, 37, 43, 1, 2), RoomObject(0x10E, 37, 47, 0, 2)};
  const auto plan = Plan(Template({{0xC9, 0, 0, 0, 0}}), {52, 5}, existing);
  EXPECT_TRUE(plan.valid);
  EXPECT_TRUE(plan.retaining_wall_join_issues.empty());
  EXPECT_FALSE(
      HasConflict(plan, ObjectTemplatePlanConflictKind::kWallJoinLoss));
}

TEST_F(ObjectTemplatePlanTest,
       WorseningPartialBaselineReportsOnlyNewLostCells) {
  const std::vector<RoomObject> existing = {RoomObject(0x003, 21, 44, 7),
                                            RoomObject(0x10C, 37, 44, 0),
                                            RoomObject(0x064, 37, 43, 1, 2)};
  ObjectTemplatePlanOptions options;
  options.allowed_existing_indices = {1, 2};
  // Proposed0 only repaints the24 cells already lost to old064. Proposed1
  // causes the new row47 loss and must be named by the blocking conflict.
  const auto plan = Plan(Template({{0x064, 0, 0, 1, 2}, {0x10E, 0, 4, 0, 2}}),
                         {37, 43}, existing, options);
  EXPECT_FALSE(plan.valid);
  const auto conflict = std::find_if(
      plan.conflicts.begin(), plan.conflicts.end(), [](const auto& value) {
        return value.kind == ObjectTemplatePlanConflictKind::kWallJoinLoss;
      });
  ASSERT_NE(conflict, plan.conflicts.end());
  EXPECT_EQ(conflict->template_index, 1);
  EXPECT_EQ(conflict->cell_count, 8);  // New row47: four per physical plane.
  for (const auto& cell : conflict->cells)
    EXPECT_EQ(cell.y, 47);
}

TEST_F(ObjectTemplatePlanTest, DrawListMergeDoesNotPretendEarlyPaintWins) {
  const std::vector<RoomObject> existing = {RoomObject(0x003, 21, 44, 7, 0),
                                            RoomObject(0x10C, 37, 44, 0, 2)};
  ObjectTemplatePlanOptions options;
  options.allowed_existing_indices = {1};
  // Although appended to the source vector, list0 is drawn before old list2.
  const auto plan =
      Plan(Template({{0x064, 0, 0, 1, 0}}), {37, 43}, existing, options);
  EXPECT_TRUE(plan.valid);
  EXPECT_TRUE(plan.retaining_wall_join_issues.empty());
}

TEST_F(ObjectTemplatePlanTest, EquivalentCornerRepaintRemainsAllowed) {
  const std::vector<RoomObject> existing = {RoomObject(0x003, 21, 44, 7),
                                            RoomObject(0x10C, 37, 44, 0)};
  ObjectTemplatePlanOptions options;
  options.allowed_existing_indices = {1};
  const auto plan =
      Plan(Template({{0x10C, 0, 0, 0, 2}}), {37, 44}, existing, options);
  EXPECT_TRUE(plan.valid);
  EXPECT_TRUE(plan.retaining_wall_join_issues.empty());
}

TEST_F(ObjectTemplatePlanTest, ConnectedVanilla36BankAndWaterPlanPass) {
  // Exact ordered source14..23, relative to(17,7), followed by water/decor.
  const auto donor = Template({{0x108, 0, 0, 0, 0},
                               {0x003, 4, 0, 0, 0},
                               {0x10A, 6, 0, 0, 0},
                               {0x064, 6, 4, 1, 0},
                               {0x10E, 6, 8, 0, 0},
                               {0x003, 10, 8, 4, 0},
                               {0x10C, 20, 8, 0, 0},
                               {0x063, 20, 4, 1, 0},
                               {0x108, 20, 0, 0, 0},
                               {0x003, 24, 0, 3, 0},
                               {0xC9, 10, 8, 15, 1},
                               {0x00, 20, 8, 1, 2}});
  const auto plan = Plan(donor, {17, 7});
  EXPECT_TRUE(plan.valid);
  EXPECT_EQ(plan.placements.size(), 12);
  EXPECT_EQ(plan.retaining_wall_join_unassessed_objects, 0);
  EXPECT_TRUE(plan.retaining_wall_join_issues.empty());
}

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
