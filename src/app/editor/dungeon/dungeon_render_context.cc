#include "app/editor/dungeon/dungeon_render_context.h"

#include <algorithm>
#include <map>

namespace yaze::editor {

namespace {

int OwnerIndexForRoom(const zelda3::RoomCensus* census, int room_id) {
  if (census == nullptr || room_id < 0 ||
      room_id >= static_cast<int>(census->rooms.size())) {
    return -1;
  }
  const int owner_index = census->rooms[room_id].owner_index;
  return owner_index >= 0 &&
                 owner_index < static_cast<int>(census->owners.size())
             ? owner_index
             : -1;
}

const DungeonRenderEntranceCandidate* FindCandidate(
    std::span<const DungeonRenderEntranceCandidate> entrances, int slot) {
  const auto it =
      std::find_if(entrances.begin(), entrances.end(),
                   [slot](const DungeonRenderEntranceCandidate& candidate) {
                     return candidate.slot == slot;
                   });
  return it == entrances.end() ? nullptr : &*it;
}

void SetOwnerMetadata(DungeonRenderContext* context,
                      const zelda3::RoomCensus* census) {
  if (context == nullptr || census == nullptr || context->owner_index < 0 ||
      context->owner_index >= static_cast<int>(census->owners.size())) {
    return;
  }
  const auto& owner = census->owners[context->owner_index];
  context->owner_id = owner.id;
  context->owner_name = owner.name;
}

void UseEntrance(DungeonRenderContext* context,
                 DungeonRenderContextSource source,
                 const DungeonRenderEntranceCandidate& entrance) {
  context->source = source;
  context->entrance_slot = entrance.slot;
  context->entrance_blockset = entrance.main_gfx;
  context->candidate_slots = {entrance.slot};
}

}  // namespace

const char* DungeonRenderContextSourceName(DungeonRenderContextSource source) {
  switch (source) {
    case DungeonRenderContextSource::kSelectedEntrance:
      return "Selected entrance";
    case DungeonRenderContextSource::kInferredOwnerEntrance:
      return "Inferred owner entrance";
    case DungeonRenderContextSource::kRoomHeaderFallback:
      return "Room header fallback";
    case DungeonRenderContextSource::kAmbiguous:
      return "Ambiguous owner entrances";
  }
  return "Unknown";
}

DungeonRenderContext ResolveDungeonRenderContext(
    int room_id, int selected_entrance_slot, uint8_t room_header_blockset,
    const zelda3::RoomCensus* census,
    std::span<const DungeonRenderEntranceCandidate> entrances) {
  DungeonRenderContext context;
  context.room_id = room_id;
  context.room_header_blockset = room_header_blockset;
  context.owner_index = OwnerIndexForRoom(census, room_id);
  SetOwnerMetadata(&context, census);

  const DungeonRenderEntranceCandidate* selected =
      FindCandidate(entrances, selected_entrance_slot);
  if (selected != nullptr) {
    if (selected->room_id == room_id) {
      UseEntrance(&context, DungeonRenderContextSource::kSelectedEntrance,
                  *selected);
      return context;
    }
    const int selected_owner = OwnerIndexForRoom(census, selected->room_id);
    if (context.owner_index >= 0 && selected_owner == context.owner_index) {
      UseEntrance(&context, DungeonRenderContextSource::kSelectedEntrance,
                  *selected);
      return context;
    }
  }

  std::map<uint8_t, std::vector<const DungeonRenderEntranceCandidate*>>
      candidates_by_gfx;
  for (const auto& candidate : entrances) {
    const bool same_owner =
        context.owner_index >= 0 &&
        OwnerIndexForRoom(census, candidate.room_id) == context.owner_index;
    if (same_owner) {
      candidates_by_gfx[candidate.main_gfx].push_back(&candidate);
      context.candidate_slots.push_back(candidate.slot);
    }
  }

  std::sort(context.candidate_slots.begin(), context.candidate_slots.end());
  context.candidate_slots.erase(std::unique(context.candidate_slots.begin(),
                                            context.candidate_slots.end()),
                                context.candidate_slots.end());

  if (candidates_by_gfx.empty()) {
    return context;
  }
  if (candidates_by_gfx.size() > 1) {
    context.source = DungeonRenderContextSource::kAmbiguous;
    return context;
  }

  auto& agreeing = candidates_by_gfx.begin()->second;
  const auto inferred =
      *std::min_element(agreeing.begin(), agreeing.end(),
                        [](const DungeonRenderEntranceCandidate* lhs,
                           const DungeonRenderEntranceCandidate* rhs) {
                          return lhs->slot < rhs->slot;
                        });
  context.source = DungeonRenderContextSource::kInferredOwnerEntrance;
  context.entrance_slot = inferred->slot;
  context.entrance_blockset = inferred->main_gfx;
  return context;
}

}  // namespace yaze::editor
