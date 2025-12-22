#include "draw_routine_registry.h"

#include "zelda3/dungeon/draw_routines/corner_routines.h"
#include "zelda3/dungeon/draw_routines/diagonal_routines.h"
#include "zelda3/dungeon/draw_routines/downwards_routines.h"
#include "zelda3/dungeon/draw_routines/rightwards_routines.h"
#include "zelda3/dungeon/draw_routines/special_routines.h"

namespace yaze {
namespace zelda3 {

DrawRoutineRegistry& DrawRoutineRegistry::Get() {
  static DrawRoutineRegistry instance;
  if (!instance.initialized_) {
    instance.Initialize();
  }
  return instance;
}

void DrawRoutineRegistry::Initialize() {
  if (initialized_) return;
  BuildRegistry();
  initialized_ = true;
}

void DrawRoutineRegistry::BuildRegistry() {
  routines_.clear();
  routine_map_.clear();

  // Register routines from all modules
  draw_routines::RegisterRightwardsRoutines(routines_);
  draw_routines::RegisterDownwardsRoutines(routines_);
  draw_routines::RegisterDiagonalRoutines(routines_);
  draw_routines::RegisterCornerRoutines(routines_);
  draw_routines::RegisterSpecialRoutines(routines_);

  // Build lookup map
  for (auto& info : routines_) {
    routine_map_[info.id] = &info;
  }
}

const DrawRoutineInfo* DrawRoutineRegistry::GetRoutineInfo(int routine_id) const {
  auto it = routine_map_.find(routine_id);
  if (it == routine_map_.end()) {
    return nullptr;
  }
  return it->second;
}

bool DrawRoutineRegistry::RoutineDrawsToBothBGs(int routine_id) const {
  const DrawRoutineInfo* info = GetRoutineInfo(routine_id);
  return info != nullptr && info->draws_to_both_bgs;
}

bool DrawRoutineRegistry::GetRoutineDimensions(int routine_id, 
                                                int* base_width, 
                                                int* base_height) const {
  const DrawRoutineInfo* info = GetRoutineInfo(routine_id);
  if (info == nullptr) {
    return false;
  }
  if (base_width) *base_width = info->base_width;
  if (base_height) *base_height = info->base_height;
  return true;
}

}  // namespace zelda3
}  // namespace yaze

