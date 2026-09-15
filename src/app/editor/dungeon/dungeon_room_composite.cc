#include "app/editor/dungeon/dungeon_room_composite.h"

#include <memory>
#include <vector>

#include "app/gfx/core/bitmap.h"
#include "app/gfx/resource/arena.h"
#include "zelda3/dungeon/palette_debug.h"
#include "zelda3/dungeon/room.h"
#include "zelda3/dungeon/room_layer_manager.h"

namespace yaze::editor {

namespace {

constexpr int kRoomCompositeWidth = 512;
constexpr int kRoomCompositeHeight = 512;

}  // namespace

RoomCompositeOutput::RoomCompositeOutput()
    : bitmap_(std::make_unique<gfx::Bitmap>()) {}

RoomCompositeOutput::~RoomCompositeOutput() {
  if (bitmap_) {
    zelda3::PaletteDebugger::Get().ClearCurrentBitmapIf(bitmap_.get());
    gfx::Arena::Get().RetireBitmap(*bitmap_);
  }
}

gfx::Bitmap& RoomCompositeOutput::Prepare(
    zelda3::Room& room, const zelda3::RoomLayerManager& layer_manager) {
  room.PrepareForRender();
  const uint64_t room_revision = room.composite_source_revision();
  const uint64_t layer_signature = layer_manager.CompositeStateSignature();
  if (!valid_ || rendered_room_ != &room ||
      rendered_room_revision_ != room_revision ||
      rendered_layer_signature_ != layer_signature) {
    // Retire() releases the resources but deliberately preserves this Bitmap
    // object's address because Canvas may still retain it. Recreate the SDL
    // surface in place before RoomLayerManager reuses the output dimensions.
    if (bitmap_->surface() == nullptr) {
      bitmap_->Create(
          kRoomCompositeWidth, kRoomCompositeHeight, 8,
          std::vector<uint8_t>(kRoomCompositeWidth * kRoomCompositeHeight, 0));
    }
    room.RenderComposite(layer_manager, *bitmap_);
    rendered_room_ = &room;
    rendered_room_revision_ = room_revision;
    rendered_layer_signature_ = layer_signature;
    valid_ = true;
  }
  return *bitmap_;
}

void RoomCompositeOutput::Retire() {
  zelda3::PaletteDebugger::Get().ClearCurrentBitmapIf(bitmap_.get());
  gfx::Arena::Get().RetireBitmap(*bitmap_);
  // Keep the shell at a stable address. Canvas and its modal helpers retain a
  // raw Bitmap pointer across refreshes; the next Prepare() rebuilds resources
  // in this same object.
  rendered_room_ = nullptr;
  rendered_room_revision_ = 0;
  rendered_layer_signature_ = 0;
  valid_ = false;
}

gfx::Bitmap& PrepareCanonicalRoomComposite(zelda3::Room& room,
                                           RoomCompositeOutput& output) {
  zelda3::RoomLayerManager layer_manager;
  layer_manager.ApplyLayerMerging(room.layer_merging());
  layer_manager.ApplyRoomEffect(room.effect());
  return output.Prepare(room, layer_manager);
}

}  // namespace yaze::editor
