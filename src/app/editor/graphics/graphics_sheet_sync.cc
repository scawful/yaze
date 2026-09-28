#include "app/editor/graphics/graphics_sheet_sync.h"

#include <algorithm>

#include "absl/strings/str_format.h"
#include "app/editor/graphics/graphics_editor_state.h"
#include "app/gfx/resource/arena.h"
#include "util/macro.h"
#include "zelda3/game_data.h"
#include "zelda3/graphics_sheet_store.h"

namespace yaze::editor {

namespace {

constexpr size_t kSheetBytes = zelda3::GraphicsSheetStore::kSheetBytes;

}  // namespace

SheetPixelDiff MakeSheetPixelDiff(uint16_t sheet,
                                  absl::Span<const uint8_t> before,
                                  absl::Span<const uint8_t> after) {
  SheetPixelDiff diff;
  diff.sheet = sheet;
  const size_t size = std::min(before.size(), after.size());
  for (size_t i = 0; i < size; ++i) {
    if (before[i] != after[i]) {
      diff.offsets.push_back(static_cast<uint16_t>(i));
      diff.before.push_back(before[i]);
      diff.after.push_back(after[i]);
    }
  }
  return diff;
}

void AttachSheetStore(GraphicsEditorState& state, zelda3::GameData* game_data) {
  state.game_data = game_data;
  state.arena_revisions.fill(0);
  if (game_data == nullptr) {
    return;
  }
  for (uint16_t sheet = 0; sheet < state.arena_revisions.size(); ++sheet) {
    state.arena_revisions[sheet] = game_data->sheet_store.Revision(sheet);
  }
}

zelda3::GraphicsSheetStore* ActiveSheetStore(const GraphicsEditorState& state) {
  if (state.game_data == nullptr ||
      gfx::Arena::Get().gfx_sheets_owner() != state.game_data) {
    return nullptr;
  }
  return &state.game_data->sheet_store;
}

void RefreshArenaSheet(GraphicsEditorState& state, uint16_t sheet) {
  auto* store = ActiveSheetStore(state);
  if (store == nullptr || !store->HasSheet(sheet)) {
    return;
  }
  auto& bitmap = gfx::Arena::Get().mutable_gfx_sheets()->at(sheet);
  if (!bitmap.is_active() || bitmap.vector().size() != kSheetBytes) {
    return;  // not on screen; nothing to refresh
  }
  const auto pixels = store->Sheet(sheet);
  if (!std::equal(pixels.begin(), pixels.end(), bitmap.vector().begin())) {
    bitmap.set_data(std::vector<uint8_t>(pixels.begin(), pixels.end()));
    gfx::Arena::Get().NotifySheetModified(sheet);
  }
  state.arena_revisions[sheet] = store->Revision(sheet);
}

absl::Status CommitSheetPixels(GraphicsEditorState& state, uint16_t sheet,
                               absl::Span<const uint8_t> pixels) {
  auto* store = ActiveSheetStore(state);
  if (store == nullptr) {
    return absl::FailedPreconditionError(
        "Graphics for this ROM are not loaded, or another open ROM's sheets "
        "are on screen");
  }
  const uint64_t revision = store->Revision(sheet);
  if (absl::Status written = store->WriteSheet(sheet, pixels); !written.ok()) {
    return written;
  }
  RefreshArenaSheet(state, sheet);
  if (store->Revision(sheet) != revision) {
    state.MarkSheetModified(sheet);
  }
  return absl::OkStatus();
}

absl::Status ApplySheetPixelDiff(GraphicsEditorState& state,
                                 const SheetPixelDiff& diff, bool redo) {
  auto* store = ActiveSheetStore(state);
  if (store == nullptr || !store->HasSheet(diff.sheet)) {
    return absl::FailedPreconditionError(absl::StrFormat(
        "Sheet 0x%02X is not in this ROM's loaded graphics", diff.sheet));
  }
  const auto current = store->Sheet(diff.sheet);
  std::vector<uint8_t> pixels(current.begin(), current.end());
  const auto& values = redo ? diff.after : diff.before;
  for (size_t i = 0; i < diff.offsets.size(); ++i) {
    pixels[diff.offsets[i]] = values[i];
  }
  RETURN_IF_ERROR(CommitSheetPixels(state, diff.sheet, pixels));
  // Undo and redo leave the sheet different from the ROM either way.
  state.MarkSheetModified(diff.sheet);
  return absl::OkStatus();
}

int SyncArenaFromStore(GraphicsEditorState& state) {
  auto* store = ActiveSheetStore(state);
  if (store == nullptr) {
    return 0;
  }
  int refreshed = 0;
  for (uint16_t sheet = 0; sheet < state.arena_revisions.size(); ++sheet) {
    if (store->Revision(sheet) != state.arena_revisions[sheet]) {
      RefreshArenaSheet(state, sheet);
      ++refreshed;
    }
  }
  return refreshed;
}

}  // namespace yaze::editor
