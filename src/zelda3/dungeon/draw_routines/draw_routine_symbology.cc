#include "zelda3/dungeon/draw_routines/draw_routine_symbology.h"

#include <string>

#include "zelda3/dungeon/draw_routines/draw_routine_registry.h"
#include "zelda3/dungeon/draw_routines/draw_routine_types.h"

namespace yaze {
namespace zelda3 {
namespace {

const char* CategoryFamilyLabel(DrawRoutineInfo::Category category) {
  switch (category) {
    case DrawRoutineInfo::Category::Rightwards:
      return "Rightwards";
    case DrawRoutineInfo::Category::Downwards:
      return "Downwards";
    case DrawRoutineInfo::Category::Diagonal:
      return "Diagonal";
    case DrawRoutineInfo::Category::Corner:
      return "Corner";
    case DrawRoutineInfo::Category::Special:
      return "Special";
  }
  return "Unknown";
}

char CategoryGlyph(DrawRoutineInfo::Category category) {
  switch (category) {
    case DrawRoutineInfo::Category::Rightwards:
      return '>';
    case DrawRoutineInfo::Category::Downwards:
      return 'v';
    case DrawRoutineInfo::Category::Diagonal:
      return '/';
    case DrawRoutineInfo::Category::Corner:
      return 'L';
    case DrawRoutineInfo::Category::Special:
      return 'S';
  }
  return '?';
}

// Object semantics, not geometry: these four are recognisable things rather
// than a direction of extension, so they earn their own glyph.
struct NamedRoutine {
  const char* name;
  const char* badge;
  const char* family;
};

constexpr NamedRoutine kNamedRoutines[] = {
    {"Chest", "C", "Chest"},
    {"BigKeyLock", "K", "Big key lock"},
    {"BombableFloor", "B", "Bombable floor"},
    {"PrisonCell", "P", "Prison cell"},
};

}  // namespace

DrawRoutineSymbology GetSymbologyForRoutine(const DrawRoutineInfo* info) {
  DrawRoutineSymbology symbology;
  if (info == nullptr) {
    symbology.badge = "?";
    symbology.family = "Unknown";
    return symbology;
  }

  symbology.dual_layer = info->draws_to_both_bgs;
  symbology.base_width = info->base_width;
  symbology.base_height = info->base_height;

  bool named = false;
  for (const auto& entry : kNamedRoutines) {
    if (info->name == entry.name) {
      symbology.badge = entry.badge;
      symbology.family = entry.family;
      named = true;
      break;
    }
  }
  if (!named) {
    symbology.badge = std::string(1, CategoryGlyph(info->category));
    symbology.family = CategoryFamilyLabel(info->category);
  }

  // The base pattern size is real registry metadata, so it goes in the label
  // rather than being folded into the badge. An earlier version overrode the
  // category with a "large fixed" badge inferred from the routine's name,
  // which cost Corner4x4 its Corner identity while wrongly claiming
  // BigHole4x4_1to16 was fixed — that routine repeats.
  if (!named && symbology.base_width > 0 && symbology.base_height > 0) {
    symbology.family += " " + std::to_string(symbology.base_width) + "x" +
                        std::to_string(symbology.base_height);
  }

  if (symbology.dual_layer) {
    symbology.badge += '2';
    symbology.family += " (BothBG)";
  }

  return symbology;
}

DrawRoutineSymbology GetSymbologyForObject(int16_t object_id) {
  const int routine_id =
      DrawRoutineRegistry::Get().GetRoutineIdForObject(object_id);
  if (routine_id < 0) {
    DrawRoutineSymbology symbology;
    symbology.badge = "?";
    symbology.family = "Unmapped";
    return symbology;
  }
  return GetSymbologyForRoutine(
      DrawRoutineRegistry::Get().GetRoutineInfo(routine_id));
}

}  // namespace zelda3
}  // namespace yaze
