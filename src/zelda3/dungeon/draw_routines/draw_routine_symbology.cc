#include "zelda3/dungeon/draw_routines/draw_routine_symbology.h"

#include "zelda3/dungeon/draw_routines/draw_routine_registry.h"
#include "zelda3/dungeon/draw_routines/draw_routine_types.h"

namespace yaze {
namespace zelda3 {
namespace {

bool IsLargeFixedRoutineName(const std::string& name) {
  return name.find("4x4") != std::string::npos ||
         name.find("SuperSquare") != std::string::npos ||
         name.find("BigHole") != std::string::npos ||
         name.find("8x8") != std::string::npos ||
         name.find("3x3") != std::string::npos;
}

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

}  // namespace

DrawRoutineSymbology GetSymbologyForRoutine(const DrawRoutineInfo* info) {
  DrawRoutineSymbology symbology;
  if (info == nullptr) {
    symbology.badge = "?";
    symbology.family = "Unknown";
    return symbology;
  }

  symbology.dual_layer = info->draws_to_both_bgs;
  symbology.large_fixed = IsLargeFixedRoutineName(info->name);

  if (info->name == "Chest") {
    symbology.badge = "C";
    symbology.family = "Chest";
  } else if (info->name == "BigKeyLock") {
    symbology.badge = "K";
    symbology.family = "Big key lock";
  } else if (info->name == "BombableFloor") {
    symbology.badge = "B";
    symbology.family = "Bombable floor";
  } else if (info->name == "PrisonCell") {
    symbology.badge = "P";
    symbology.family = "Prison cell";
  } else if (symbology.large_fixed) {
    symbology.badge = "4";
    symbology.family = "Large fixed";
  } else {
    symbology.badge = std::string(1, CategoryGlyph(info->category));
    symbology.family = CategoryFamilyLabel(info->category);
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
