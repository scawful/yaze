#include "app/editor/overworld/tile16/tile16_edit_session.h"

#include "app/editor/core/undo_action.h"
#include "app/gfx/resource/arena.h"
#include "util/macro.h"
#include "zelda3/overworld/overworld.h"
#include "zelda3/overworld/tile16_metadata.h"

namespace yaze::editor {
namespace {
bool SameDefinition(const gfx::Tile16& a, const gfx::Tile16& b) {
  for (int q = 0; q < 4; ++q) {
    if (gfx::TileInfoToShort(zelda3::Tile16QuadrantInfo(a, q)) !=
        gfx::TileInfoToShort(zelda3::Tile16QuadrantInfo(b, q))) {
      return false;
    }
  }
  return true;
}

class Tile16DefinitionEditAction final : public UndoAction {
 public:
  using Apply = std::function<absl::Status(const std::vector<Tile16Commit>&)>;
  Tile16DefinitionEditAction(std::vector<Tile16Commit> before,
                             std::vector<Tile16Commit> after, Apply apply)
      : before_(std::move(before)),
        after_(std::move(after)),
        apply_(std::move(apply)) {}
  absl::Status Undo() override { return apply_(before_); }
  absl::Status Redo() override { return apply_(after_); }
  std::string Description() const override {
    return before_.size() == 1 ? "Edit Tile16 definition" : "Edit Tile16 stamp";
  }
  size_t MemoryUsage() const override {
    return (before_.size() + after_.size()) * sizeof(Tile16Commit);
  }

 private:
  std::vector<Tile16Commit> before_, after_;
  Apply apply_;
};
}  // namespace

absl::StatusOr<gfx::Tile16> Tile16EditSession::ReadDocumentTile(int id) const {
  if (document_definitions_) {
    if (id < 0 || id >= static_cast<int>(document_definitions_->size())) {
      return absl::OutOfRangeError("Tile16 is outside the document");
    }
    return (*document_definitions_)[id];
  }
  const auto it = document_edits_.find(id);
  if (it != document_edits_.end())
    return it->second;
  if (!rom_)
    return absl::FailedPreconditionError("No Tile16 document");
  return rom_->ReadTile16(id, zelda3::kTile16Ptr);
}

absl::Status Tile16EditSession::ApplyDefinitions(
    const std::vector<Tile16Commit>& definitions) {
  // Validate and render the complete batch before publishing any definition.
  // History stores metadata, not pixels tied to the map's graphics/palette.
  std::vector<gfx::Bitmap> bitmaps(definitions.size());
  for (size_t i = 0; i < definitions.size(); ++i) {
    const auto& edit = definitions[i];
    if (edit.tile_id < 0 || edit.tile_id >= zelda3::kNumTile16Individual ||
        (document_definitions_ &&
         edit.tile_id >= static_cast<int>(document_definitions_->size()))) {
      return absl::OutOfRangeError("Tile16 edit is outside the document");
    }
    RETURN_IF_ERROR(BuildTile16BitmapFromData(edit.tile_data, &bitmaps[i]));
    bitmaps[i].SetPalette(current_tile16_bmp_.palette());
  }
  for (size_t i = 0; i < definitions.size(); ++i) {
    const auto& edit = definitions[i];
    document_edits_[edit.tile_id] = edit.tile_data;
    edited_tile_bitmaps_[edit.tile_id] = bitmaps[i];
    if (document_definitions_) {
      (*document_definitions_)[edit.tile_id] = edit.tile_data;
    }
    CopyTileBitmapToBlockset(edit.tile_id, bitmaps[i]);
  }
  tile8_usage_cache_dirty_ = true;
  preview_dirty_ = true;
  RETURN_IF_ERROR(SetCurrentTile(current_tile16_));
  if (on_document_changed_)
    on_document_changed_(definitions);
  if (rom_)
    rom_->set_dirty(true);
  if (HasTile16BlocksetBitmap()) {
    gfx::Arena::Get().QueueTextureCommand(
        gfx::Arena::TextureCommandType::UPDATE, tile16_blockset_bmp_);
  }
  if (tile16_blockset_ && tile16_blockset_->atlas.is_active()) {
    gfx::Arena::Get().QueueTextureCommand(
        gfx::Arena::TextureCommandType::UPDATE, &tile16_blockset_->atlas);
  }
  return absl::OkStatus();
}

absl::Status Tile16EditSession::RunEdit(
    const std::function<absl::Status()>& operation) {
  // A map stroke must enter history before the following definition edit.
  if (before_edit_)
    before_edit_();
  const auto previous = document_edits_;
  const auto old_current = current_tile16_data_;
  const auto result = operation();
  const auto proposed = document_edits_;
  document_edits_ = previous;
  current_tile16_data_ = old_current;

  std::vector<Tile16Commit> before, after;
  for (const auto& [id, data] : proposed) {
    auto original = ReadDocumentTile(id);
    if (!original.ok()) {
      (void)SetCurrentTile(current_tile16_);
      return original.status();
    }
    if (!SameDefinition(*original, data)) {
      before.push_back({id, *original});
      after.push_back({id, data});
    }
  }
  if (!result.ok()) {
    // Restore derived previews changed by a failed render/stamp operation.
    for (const auto& edit : before) {
      gfx::Bitmap bitmap;
      if (BuildTile16BitmapFromData(edit.tile_data, &bitmap).ok()) {
        CopyTileBitmapToBlockset(edit.tile_id, bitmap);
      }
    }
    (void)SetCurrentTile(current_tile16_);
    return result;
  }
  if (after.empty())
    return SetCurrentTile(current_tile16_);
  const auto applied = ApplyDefinitions(after);
  if (!applied.ok()) {
    (void)ApplyDefinitions(before);
    document_edits_ = previous;
    return applied;
  }
  history_->Push(std::make_unique<Tile16DefinitionEditAction>(
      std::move(before), std::move(after),
      [this](const std::vector<Tile16Commit>& edits) {
        return ApplyDefinitions(edits);
      }));
  return absl::OkStatus();
}

absl::Status Tile16EditSession::ReplaceCurrentTile(const gfx::Tile16& data) {
  return RunEdit([&]() {
    current_tile16_data_ = data;
    zelda3::SyncTile16TilesInfo(&current_tile16_data_);
    RETURN_IF_ERROR(RegenerateTile16BitmapFromROM());
    MarkCurrentTileModified();
    return absl::OkStatus();
  });
}
}  // namespace yaze::editor
