#ifndef YAZE_CORE_DUNGEON_STREAM_LAYOUT_ADAPTER_H_
#define YAZE_CORE_DUNGEON_STREAM_LAYOUT_ADAPTER_H_

#include "absl/status/statusor.h"
#include "core/hack_manifest.h"
#include "zelda3/dungeon/dungeon_stream_allocator.h"

namespace yaze::core {

// Converts the manifest's canonical LoROM SNES addresses into the headerless
// PC offsets consumed by the dungeon stream inventory/allocator. The manifest
// write strategy is intentionally not represented in the returned layout;
// callers must enforce that policy before planning writes.
absl::StatusOr<zelda3::DungeonStreamLayout> ToDungeonStreamAllocatorLayout(
    DungeonStreamType stream_type, const DungeonStreamLayout& manifest_layout);

}  // namespace yaze::core

#endif  // YAZE_CORE_DUNGEON_STREAM_LAYOUT_ADAPTER_H_
