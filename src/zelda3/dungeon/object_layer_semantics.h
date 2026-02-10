#ifndef YAZE_ZELDA3_DUNGEON_OBJECT_LAYER_SEMANTICS_H_
#define YAZE_ZELDA3_DUNGEON_OBJECT_LAYER_SEMANTICS_H_

#include <cstdint>

#include "zelda3/dungeon/draw_routines/draw_routine_registry.h"
#include "zelda3/dungeon/room_object.h"

namespace yaze {
namespace zelda3 {

enum class EffectiveBgLayer {
  kBg1,
  kBg2,
  kBothBg1Bg2,
};

struct ObjectLayerSemantics {
  int routine_id = -1;
  bool draws_to_both_bgs = false;
  EffectiveBgLayer effective_bg_layer = EffectiveBgLayer::kBg1;
};

inline ObjectLayerSemantics GetObjectLayerSemantics(const RoomObject& object) {
  ObjectLayerSemantics out;

  out.routine_id = DrawRoutineRegistry::Get().GetRoutineIdForObject(object.id_);
  const bool routine_both =
      (out.routine_id >= 0) &&
      DrawRoutineRegistry::Get().RoutineDrawsToBothBGs(out.routine_id);
  out.draws_to_both_bgs = object.all_bgs_ || routine_both;

  if (out.draws_to_both_bgs) {
    out.effective_bg_layer = EffectiveBgLayer::kBothBg1Bg2;
    return out;
  }

  out.effective_bg_layer =
      (object.layer_ == RoomObject::LayerType::BG2) ? EffectiveBgLayer::kBg2
                                                    : EffectiveBgLayer::kBg1;
  return out;
}

inline const char* EffectiveBgLayerLabel(EffectiveBgLayer layer) {
  switch (layer) {
    case EffectiveBgLayer::kBg1:
      return "BG1";
    case EffectiveBgLayer::kBg2:
      return "BG2";
    case EffectiveBgLayer::kBothBg1Bg2:
      return "Both BGs";
  }
  return "Unknown";
}

}  // namespace zelda3
}  // namespace yaze

#endif  // YAZE_ZELDA3_DUNGEON_OBJECT_LAYER_SEMANTICS_H_

