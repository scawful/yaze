#include "app/editor/overworld/tile16/tile16_edit_session.h"

#include <algorithm>
#include <cstdint>
#include <set>

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

bool ContainsTile(const std::vector<Tile16Commit>& edits, int tile_id) {
  return std::any_of(edits.begin(), edits.end(), [tile_id](const auto& edit) {
    return edit.tile_id == tile_id;
  });
}

bool SameOwner(const Tile16Commit& a, const Tile16Commit& b) {
  return a.owner == b.owner && a.owner_map_id == b.owner_map_id;
}

bool ShouldMutateBoundDocument(const Tile16Commit& edit, int bound_map_id) {
  if (edit.owner == Tile16DefinitionOwner::kUnbound)
    return true;
  if (edit.owner_map_id < 0)
    return edit.owner == Tile16DefinitionOwner::kGlobal;
  if (bound_map_id < 0)
    return edit.owner == Tile16DefinitionOwner::kGlobal;
  return edit.owner_map_id == bound_map_id;
}

class Tile16DefinitionEditAction final : public UndoAction {
 public:
  using Apply = std::function<absl::Status(const std::vector<Tile16Commit>&)>;
  Tile16DefinitionEditAction(std::vector<Tile16Commit> before,
                             std::vector<Tile16Commit> after, Apply apply,
                             std::string description = {},
                             const void* owner = nullptr,
                             uint64_t edit_group = 0)
      : before_(std::move(before)),
        after_(std::move(after)),
        apply_(std::move(apply)),
        description_(std::move(description)),
        owner_(owner),
        edit_group_(edit_group) {}
  absl::Status Undo() override { return apply_(before_); }
  absl::Status Redo() override { return apply_(after_); }
  std::string Description() const override {
    if (!description_.empty())
      return description_;
    return before_.size() == 1 ? "Edit Tile16 definition" : "Edit Tile16 stamp";
  }
  size_t MemoryUsage() const override {
    return (before_.size() + after_.size()) * sizeof(Tile16Commit);
  }
  bool CanMergeWith(const UndoAction& prev) const override {
    const auto* previous =
        dynamic_cast<const Tile16DefinitionEditAction*>(&prev);
    if (edit_group_ == 0 || previous == nullptr || previous->owner_ != owner_ ||
        previous->edit_group_ != edit_group_) {
      return false;
    }
    // Owner-bound page/global commits must not merge across maps or scopes.
    if (!after_.empty() && !previous->after_.empty() &&
        !SameOwner(after_.front(), previous->after_.front())) {
      return false;
    }
    return true;
  }
  // Keeps the earliest before state and the latest after state of each tile.
  void MergeWith(UndoAction& prev) override {
    auto& previous = static_cast<Tile16DefinitionEditAction&>(prev);
    std::vector<Tile16Commit> before = std::move(previous.before_);
    for (auto& edit : before_) {
      if (!ContainsTile(before, edit.tile_id))
        before.push_back(std::move(edit));
    }
    for (auto& edit : previous.after_) {
      if (!ContainsTile(after_, edit.tile_id))
        after_.push_back(std::move(edit));
    }
    before_ = std::move(before);
  }

 private:
  std::vector<Tile16Commit> before_, after_;
  Apply apply_;
  std::string description_;
  const void* owner_ = nullptr;
  uint64_t edit_group_ = 0;
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
  // Fallible model publication runs before any session mutation so a refusal
  // leaves document/preview/history/dirty state unchanged.
  if (document_publisher_) {
    RETURN_IF_ERROR(document_publisher_(definitions));
  }
  for (size_t i = 0; i < definitions.size(); ++i) {
    const auto& edit = definitions[i];
    document_edits_[edit.tile_id] = edit.tile_data;
    edited_tile_bitmaps_[edit.tile_id] = bitmaps[i];
    if (document_definitions_ &&
        ShouldMutateBoundDocument(edit, bound_document_map_id_)) {
      (*document_definitions_)[edit.tile_id] = edit.tile_data;
    }
    if (ShouldMutateBoundDocument(edit, bound_document_map_id_)) {
      CopyTileBitmapToBlockset(edit.tile_id, bitmaps[i]);
    }
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

absl::Status Tile16EditSession::ValidateDefinitionImport(
    const std::vector<Tile16Commit>& definitions) const {
  if (definitions.empty())
    return absl::OkStatus();
  if (!rom_ || !tile16_blockset_) {
    return absl::FailedPreconditionError(
        "Tile16 blockset or ROM not initialized");
  }
  std::set<int> destinations;
  for (const auto& edit : definitions) {
    if (edit.tile_id < 0 || edit.tile_id >= zelda3::kNumTile16Individual ||
        (document_definitions_ &&
         edit.tile_id >= static_cast<int>(document_definitions_->size()))) {
      return absl::OutOfRangeError("Tile16 import is outside the document");
    }
    if (!destinations.insert(edit.tile_id).second) {
      return absl::InvalidArgumentError(
          "Tile16 import contains a duplicate destination");
    }
    ASSIGN_OR_RETURN(const auto original, ReadDocumentTile(edit.tile_id));
    for (int q = 0; q < 4; ++q) {
      const auto& info = zelda3::Tile16QuadrantInfo(edit.tile_data, q);
      if (info.id_ >= 1024 ||
          info.id_ >= static_cast<int>(current_gfx_individual_.size()) ||
          info.palette_ > 7) {
        return absl::OutOfRangeError(
            "Tile16 import has invalid Tile8 metadata");
      }
    }
    gfx::Bitmap before, after;
    RETURN_IF_ERROR(BuildTile16BitmapFromData(original, &before));
    RETURN_IF_ERROR(BuildTile16BitmapFromData(edit.tile_data, &after));
  }
  // ApplyDefinitions refreshes the current selection after publishing. Check
  // that render now too, so its last fallible operation cannot partially apply
  // a batch. Redo/Undo repeat this preflight against the current source.
  ASSIGN_OR_RETURN(const auto current, ReadDocumentTile(current_tile16_));
  gfx::Bitmap preview;
  return BuildTile16BitmapFromData(current, &preview);
}

absl::StatusOr<std::unique_ptr<UndoAction>>
Tile16EditSession::PrepareDefinitionImport(
    const std::vector<Tile16Commit>& definitions, std::string description) {
  RETURN_IF_ERROR(ValidateDefinitionImport(definitions));
  std::vector<Tile16Commit> before, after;
  for (const auto& edit : definitions) {
    ASSIGN_OR_RETURN(auto original, ReadDocumentTile(edit.tile_id));
    auto normalized = edit.tile_data;
    zelda3::SyncTile16TilesInfo(&normalized);
    Tile16Commit before_commit{edit.tile_id, std::move(original)};
    Tile16Commit after_commit = edit;
    after_commit.tile_data = std::move(normalized);
    if (commit_owner_stamper_) {
      commit_owner_stamper_(&before_commit, &after_commit);
    } else if (edit.owner != Tile16DefinitionOwner::kUnbound) {
      before_commit.owner =
          edit.owner == Tile16DefinitionOwner::kClearPageOverride
              ? Tile16DefinitionOwner::kPageLocal
              : edit.owner;
      before_commit.owner_map_id = edit.owner_map_id;
      if (edit.owner == Tile16DefinitionOwner::kClearPageOverride) {
        // Preserve caller-provided clear after-state.
      }
    }
    if (!SameDefinition(before_commit.tile_data, after_commit.tile_data) ||
        before_commit.owner != after_commit.owner) {
      before.push_back(std::move(before_commit));
      after.push_back(std::move(after_commit));
    }
  }
  if (after.empty())
    return std::unique_ptr<UndoAction>{};
  return std::unique_ptr<UndoAction>(
      std::make_unique<Tile16DefinitionEditAction>(
          std::move(before), std::move(after),
          [this](const std::vector<Tile16Commit>& edits) {
            RETURN_IF_ERROR(ValidateDefinitionImport(edits));
            return ApplyDefinitions(edits);
          },
          std::move(description)));
}

absl::Status Tile16EditSession::ImportDefinitions(
    const std::vector<Tile16Commit>& definitions, std::string description) {
  ASSIGN_OR_RETURN(auto action, PrepareDefinitionImport(
                                    definitions, std::move(description)));
  if (!action)
    return absl::OkStatus();
  // Match RunEdit's chronological ordering without finalizing pending painting
  // for a refused/no-op preparation or for an externally owned action.
  if (before_edit_)
    before_edit_();
  RETURN_IF_ERROR(action->Redo());
  history_->Push(std::move(action));
  return absl::OkStatus();
}

absl::Status Tile16EditSession::RunEdit(
    const std::function<absl::Status()>& operation, uint64_t edit_group) {
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
      Tile16Commit before_commit{id, *original};
      Tile16Commit after_commit{id, data};
      if (commit_owner_stamper_) {
        commit_owner_stamper_(&before_commit, &after_commit);
      }
      before.push_back(std::move(before_commit));
      after.push_back(std::move(after_commit));
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
    // Publisher-first ApplyDefinitions leaves session state untouched on
    // refusal; only roll back if a later step somehow diverged.
    document_edits_ = previous;
    (void)SetCurrentTile(current_tile16_);
    return applied;
  }
  history_->Push(std::make_unique<Tile16DefinitionEditAction>(
      std::move(before), std::move(after),
      [this](const std::vector<Tile16Commit>& edits) {
        return ApplyDefinitions(edits);
      },
      std::string{}, this, edit_group));
  return absl::OkStatus();
}

absl::Status Tile16EditSession::ReplaceCurrentTile(const gfx::Tile16& data,
                                                   uint64_t edit_group) {
  return RunEdit(
      [&]() {
        current_tile16_data_ = data;
        zelda3::SyncTile16TilesInfo(&current_tile16_data_);
        RETURN_IF_ERROR(RegenerateTile16BitmapFromROM());
        MarkCurrentTileModified();
        return absl::OkStatus();
      },
      edit_group);
}
}  // namespace yaze::editor
