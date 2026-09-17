// Pins the draw-routine symbology rules.
//
// The rules are worth pinning because the obvious implementation — read the
// routine's NAME and infer behaviour from it — is wrong in both directions,
// and both directions are represented here by real registry entries.

#include "zelda3/dungeon/draw_routines/draw_routine_symbology.h"

#include <string>
#include <vector>

#include "gtest/gtest.h"
#include "zelda3/dungeon/draw_routines/draw_routine_registry.h"
#include "zelda3/dungeon/draw_routines/draw_routine_types.h"

namespace yaze::zelda3 {
namespace {

// The registry is a lazily-initialised singleton; every test needs it built.
const std::vector<DrawRoutineInfo>& AllRoutines() {
  auto& registry = DrawRoutineRegistry::Get();
  registry.Initialize();
  return registry.GetAllRoutines();
}

const DrawRoutineInfo* FindRoutineByName(const std::string& name) {
  for (const auto& info : AllRoutines()) {
    if (info.name == name) {
      return &info;
    }
  }
  return nullptr;
}

TEST(DrawRoutineSymbologyTest, NullRoutineIsUnknownRatherThanACrash) {
  const auto symbology = GetSymbologyForRoutine(nullptr);
  EXPECT_EQ(symbology.badge, "?");
  EXPECT_EQ(symbology.family, "Unknown");
  EXPECT_FALSE(symbology.dual_layer);
}

TEST(DrawRoutineSymbologyTest, UnmappedObjectIsReportedAsUnmapped) {
  // Far outside every range the registry maps.
  const auto symbology = GetSymbologyForObject(0x7FFF);
  EXPECT_EQ(symbology.badge, "?");
  EXPECT_EQ(symbology.family, "Unmapped");
}

// A corner routine must keep its corner identity. The previous implementation
// inferred "large fixed" from the substring "4x4" in the routine name and
// replaced the category badge with "4", so this routine — which is a Corner
// and says so in its own category field — reported as Large fixed.
TEST(DrawRoutineSymbologyTest, CornerRoutineKeepsItsCategoryDespiteA4x4Name) {
  const DrawRoutineInfo* info = FindRoutineByName("Corner4x4");
  ASSERT_NE(info, nullptr) << "registry no longer defines Corner4x4";
  ASSERT_EQ(info->category, DrawRoutineInfo::Category::Corner);

  const auto symbology = GetSymbologyForRoutine(info);
  EXPECT_EQ(symbology.badge.front(), 'L') << symbology.badge;
  EXPECT_NE(symbology.family.find("Corner"), std::string::npos)
      << symbology.family;
}

// The mirror image: a routine whose name contains a tile pattern but which
// REPEATS that pattern (the _1to16 suffix) is not a single fixed block. The
// name heuristic claimed it was.
TEST(DrawRoutineSymbologyTest, RepeatingRoutineIsNotLabelledFixed) {
  const DrawRoutineInfo* info = FindRoutineByName("BigHole4x4_1to16");
  ASSERT_NE(info, nullptr) << "registry no longer defines BigHole4x4_1to16";

  const auto symbology = GetSymbologyForRoutine(info);
  EXPECT_EQ(symbology.family.find("Large fixed"), std::string::npos)
      << symbology.family;
}

// Object semantics, matched by name on purpose: a chest is a chest, not a
// direction of extension.
TEST(DrawRoutineSymbologyTest, NamedObjectRoutinesGetTheirOwnGlyph) {
  struct Expectation {
    const char* routine;
    char badge;
    const char* family;
  };
  constexpr Expectation kExpected[] = {
      {"Chest", 'C', "Chest"},
      {"BigKeyLock", 'K', "Big key lock"},
      {"BombableFloor", 'B', "Bombable floor"},
      {"PrisonCell", 'P', "Prison cell"},
  };

  for (const auto& expected : kExpected) {
    SCOPED_TRACE(expected.routine);
    const DrawRoutineInfo* info = FindRoutineByName(expected.routine);
    ASSERT_NE(info, nullptr);
    const auto symbology = GetSymbologyForRoutine(info);
    EXPECT_EQ(symbology.badge.front(), expected.badge);
    EXPECT_NE(symbology.family.find(expected.family), std::string::npos)
        << symbology.family;
    // A named routine reports the object, not its pattern size.
    EXPECT_EQ(symbology.family.find('x'), std::string::npos)
        << symbology.family;
  }
}

TEST(DrawRoutineSymbologyTest, BothBgRoutinesAreMarkedOnBadgeAndFamily) {
  int checked = 0;
  for (const auto& info : AllRoutines()) {
    if (!info.draws_to_both_bgs) {
      continue;
    }
    const auto symbology = GetSymbologyForRoutine(&info);
    EXPECT_TRUE(symbology.dual_layer) << info.name;
    EXPECT_EQ(symbology.badge.back(), '2')
        << info.name << ": " << symbology.badge;
    EXPECT_NE(symbology.family.find("BothBG"), std::string::npos)
        << info.name << ": " << symbology.family;
    ++checked;
  }
  EXPECT_GT(checked, 0) << "no BothBG routines found; test proves nothing";
}

// Every routine the registry defines must produce a usable badge. A blank or
// over-long badge would silently render as an empty or clipped grid cell.
TEST(DrawRoutineSymbologyTest, EveryRegisteredRoutineProducesAShortBadge) {
  ASSERT_FALSE(AllRoutines().empty());
  for (const auto& info : AllRoutines()) {
    SCOPED_TRACE(info.name);
    const auto symbology = GetSymbologyForRoutine(&info);
    EXPECT_FALSE(symbology.badge.empty());
    EXPECT_LE(symbology.badge.size(), 2u) << symbology.badge;
    EXPECT_FALSE(symbology.family.empty());
    // '?' means the category switch fell through — every routine has one.
    EXPECT_NE(symbology.badge.front(), '?') << "unhandled category";
  }
}

// The dimensions come from the registry, so they must match it exactly rather
// than being re-derived from the name.
TEST(DrawRoutineSymbologyTest, BasePatternSizeMirrorsTheRegistry) {
  const DrawRoutineInfo* info = FindRoutineByName("Corner4x4");
  ASSERT_NE(info, nullptr);
  const auto symbology = GetSymbologyForRoutine(info);
  EXPECT_EQ(symbology.base_width, info->base_width);
  EXPECT_EQ(symbology.base_height, info->base_height);
}

}  // namespace
}  // namespace yaze::zelda3
