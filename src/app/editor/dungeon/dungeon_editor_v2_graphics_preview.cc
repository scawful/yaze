#include "app/editor/dungeon/dungeon_editor_v2.h"

#include <array>
#include <cstdint>
#include <memory>
#include <optional>
#include <utility>
#include <vector>

#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "app/editor/dungeon/dungeon_room_transfer.h"
#include "app/editor/graphics/graphics_edit_context.h"
#include "app/editor/graphics/usage_preview.h"
#include "app/gfx/util/palette_manager.h"
#include "util/macro.h"
#include "zelda3/dungeon/room.h"
#include "zelda3/dungeon/room_layer_manager.h"

namespace yaze::editor {

void DungeonEditorV2::ConfigureStagedGraphicsPreview(
    GraphicsEditContext& context, int room_id,
    std::function<absl::Status(GraphicsEditContext&)> resolve_live_source) {
  struct StagedSourceStamp {
    std::optional<DungeonRoomDocument> document;
    usage_preview::SheetPixels sheets;
    std::array<uint8_t, 16> blocks{};
    uint8_t main_blockset = 0xff;
    uint64_t palette_revision = 0;
    uint64_t revision = 0;
  };
  auto staged_stamp = std::make_shared<StagedSourceStamp>();
  context.preview_revision = [this, room_id, owner = game_data_,
                              source_rom = rom_,
                              staged_stamp]() -> absl::StatusOr<uint64_t> {
    if (game_data_ != owner || rom_ != source_rom || !rom_ ||
        !rom_->is_loaded()) {
      return absl::FailedPreconditionError("Dungeon source session changed");
    }
    const auto* source = rooms_.GetIfMaterialized(room_id);
    if (!source || !source->IsLoaded())
      return absl::FailedPreconditionError("Dungeon source room changed");
    // Cache invalidation also advances composite_source_revision. Compare
    // authored inputs instead, so ordinary source-preview refreshes do not
    // discard a candidate. Full pixels also catch an uncommitted stroke.
    auto document = CaptureDungeonRoomDocument(*source);
    usage_preview::SheetPixels sheets;
    for (const auto sheet : source->blocks()) {
      if (!owner->sheet_store.HasSheet(sheet))
        continue;
      const auto pixels = owner->sheet_store.Sheet(sheet);
      sheets.emplace(sheet, std::vector<uint8_t>(pixels.begin(), pixels.end()));
    }
    const auto palette_revision =
        gfx::PaletteManager::Get().ColorRevision(owner);
    if (!staged_stamp->document ||
        !SameDungeonRoomDocument(*staged_stamp->document, document) ||
        staged_stamp->sheets != sheets ||
        staged_stamp->blocks != source->blocks() ||
        staged_stamp->main_blockset != source->resolved_main_blockset() ||
        staged_stamp->palette_revision != palette_revision) {
      staged_stamp->document = std::move(document);
      staged_stamp->sheets = std::move(sheets);
      staged_stamp->blocks = source->blocks();
      staged_stamp->main_blockset = source->resolved_main_blockset();
      staged_stamp->palette_revision = palette_revision;
      ++staged_stamp->revision;
    }
    return staged_stamp->revision;
  };
  context.render_staged_preview =
      [this, room_id, owner = game_data_, source_rom = rom_,
       resolve_live_source = std::move(resolve_live_source)](
          const usage_preview::SheetPixels& sheets)
      -> absl::StatusOr<usage_preview::PreviewImage> {
    GraphicsEditContext fresh;
    RETURN_IF_ERROR(resolve_live_source(fresh));
    if (game_data_ != owner || rom_ != source_rom)
      return absl::FailedPreconditionError("Dungeon source session changed");
    auto* source = rooms_.GetIfMaterialized(room_id);
    if (!source || !source->IsLoaded())
      return absl::FailedPreconditionError("Dungeon source room changed");
    // Reuse the authored-room document and normal room renderer on a detached
    // room. Neither the active Room nor the session sheet store receives the
    // candidate pixels merely to try an asset.
    auto detached = zelda3::LoadRoomFromRom(source_rom, room_id);
    detached.SetGameData(owner);
    detached.EnsureObjectsLoaded();
    ApplyDungeonRoomDocument(detached, CaptureDungeonRoomDocument(*source));
    detached.SetRenderEntranceBlockset(source->render_entrance_blockset());
    detached.SetGraphicsSheetOverrides(sheets);
    detached.PrepareForRender();
    zelda3::RoomLayerManager layers;
    layers.ApplyLayerMerging(detached.layer_merging());
    layers.ApplyRoomEffect(detached.effect());
    layers.ApplyGameLayerRegisters(detached.GameLayerRegisters());
    gfx::Bitmap composite;
    detached.RenderComposite(layers, composite);
    return usage_preview::SnapshotBitmap(composite);
  };
}

}  // namespace yaze::editor
