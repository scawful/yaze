#ifndef YAZE_ZELDA3_DUNGEON_OBJECT_LAYER_SEMANTICS_H_
#define YAZE_ZELDA3_DUNGEON_OBJECT_LAYER_SEMANTICS_H_

#include <cstdint>

#include "zelda3/dungeon/draw_routines/draw_routine_registry.h"
#include "zelda3/dungeon/object_render_routing.h"
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
  // Compatibility field for the legacy all_bgs_/routine-metadata decision.
  // Object-ID-specific routing (for example auto and straight stairs) is
  // represented by render_routing instead.
  bool draws_to_both_bgs = false;
  EffectiveBgLayer effective_bg_layer = EffectiveBgLayer::kBg1;
  ObjectRenderRouting render_routing = ObjectRenderRouting::kStoredPlacement;
};

inline bool UsesRoomObjectStream(const RoomObject& object) {
  const auto options = object.options();
  return (options & ObjectOption::Torch) == ObjectOption::Nothing &&
         (options & ObjectOption::Block) == ObjectOption::Nothing;
}

inline bool UsesSpecialLayerSelector(const RoomObject& object) {
  return !UsesRoomObjectStream(object);
}

// Reports built-in routine routing. A project custom-object override can
// preempt the built-in routine and use stored placement instead.
inline ObjectLayerSemantics GetObjectLayerSemantics(const RoomObject& object) {
  ObjectLayerSemantics out;

  out.routine_id = DrawRoutineRegistry::Get().GetRoutineIdForObject(object.id_);
  const bool routine_both =
      (out.routine_id >= 0) &&
      DrawRoutineRegistry::Get().RoutineDrawsToBothBGs(out.routine_id);
  out.draws_to_both_bgs = object.all_bgs_ || routine_both;

  if (out.draws_to_both_bgs) {
    out.effective_bg_layer = EffectiveBgLayer::kBothBg1Bg2;
    out.render_routing = ObjectRenderRouting::kFullBothBg1Bg2;
    return out;
  }

  if (out.routine_id == DrawRoutineIds::kAutoStairs &&
      object_render_routing::IsFullBothAutoStairsObject(object.id_)) {
    out.effective_bg_layer = EffectiveBgLayer::kBothBg1Bg2;
    out.render_routing = ObjectRenderRouting::kFullBothBg1Bg2;
    return out;
  }

  if (out.routine_id == DrawRoutineIds::kAgahnimsAltar ||
      out.routine_id == DrawRoutineIds::kFortuneTellerRoom ||
      out.routine_id == DrawRoutineIds::kSpiralStairsGoingUpUpper ||
      out.routine_id == DrawRoutineIds::kSpiralStairsGoingDownUpper) {
    out.effective_bg_layer = EffectiveBgLayer::kBg1;
    out.render_routing = ObjectRenderRouting::kFixedBg1;
    return out;
  }

  if (out.routine_id == DrawRoutineIds::kSpiralStairsGoingUpLower ||
      out.routine_id == DrawRoutineIds::kSpiralStairsGoingDownLower) {
    out.effective_bg_layer = EffectiveBgLayer::kBg2;
    out.render_routing = ObjectRenderRouting::kFixedBg2;
    return out;
  }

  if (out.routine_id == DrawRoutineIds::kStraightInterRoomStairs) {
    if (object_render_routing::IsMixedStraightInterroomObject(object.id_)) {
      out.effective_bg_layer = EffectiveBgLayer::kBothBg1Bg2;
      out.render_routing = ObjectRenderRouting::kMixedBg1Bg2;
      return out;
    }

    out.effective_bg_layer = EffectiveBgLayer::kBg1;
    out.render_routing = ObjectRenderRouting::kFixedBg1;
    return out;
  }

  out.effective_bg_layer = (object.layer_ == RoomObject::LayerType::BG2)
                               ? EffectiveBgLayer::kBg2
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

inline const char* ObjectRenderRoutingLabel(ObjectRenderRouting routing) {
  switch (routing) {
    case ObjectRenderRouting::kStoredPlacement:
      return "Stored placement";
    case ObjectRenderRouting::kFixedBg1:
      return "BG1 (fixed)";
    case ObjectRenderRouting::kFixedBg2:
      return "BG2 (fixed)";
    case ObjectRenderRouting::kFullBothBg1Bg2:
      return "Both BGs (full)";
    case ObjectRenderRouting::kMixedBg1Bg2:
      return "Mixed BG1/BG2";
  }
  return "Unknown";
}

inline const char* ObjectRenderRoutingDisplayLabel(
    const ObjectLayerSemantics& semantics) {
  if (semantics.render_routing != ObjectRenderRouting::kStoredPlacement) {
    return ObjectRenderRoutingLabel(semantics.render_routing);
  }
  return semantics.effective_bg_layer == EffectiveBgLayer::kBg2
             ? "BG2 (stored placement)"
             : "BG1 (stored placement)";
}

inline const char* ObjectRenderRoutingToken(ObjectRenderRouting routing) {
  switch (routing) {
    case ObjectRenderRouting::kStoredPlacement:
      return "stored";
    case ObjectRenderRouting::kFixedBg1:
      return "fixed_bg1";
    case ObjectRenderRouting::kFixedBg2:
      return "fixed_bg2";
    case ObjectRenderRouting::kFullBothBg1Bg2:
      return "full_both";
    case ObjectRenderRouting::kMixedBg1Bg2:
      return "mixed";
  }
  return "unknown";
}

}  // namespace zelda3
}  // namespace yaze

#endif  // YAZE_ZELDA3_DUNGEON_OBJECT_LAYER_SEMANTICS_H_
