#ifndef YAZE_APP_EDITOR_GRAPHICS_GRAPHICS_SHEET_SYNC_H
#define YAZE_APP_EDITOR_GRAPHICS_GRAPHICS_SHEET_SYNC_H

#include <cstddef>
#include <cstdint>
#include <vector>

#include "absl/status/status.h"
#include "absl/types/span.h"

namespace yaze {
namespace zelda3 {
class GraphicsSheetStore;
struct GameData;
}  // namespace zelda3

namespace editor {

class GraphicsEditorState;

/**
 * @file graphics_sheet_sync.h
 * @brief The Graphics editor's path for sheet pixel writes.
 *
 * Pixels live in the session's zelda3::GraphicsSheetStore. Every edit
 * (pixel tools, paste, PNG import, undo and redo) writes the store; the
 * Arena's bitmaps are display copies, refreshed when a sheet's store
 * revision moves.
 */

/// The pixels one undo step changes in one sheet.
struct SheetPixelDiff {
  uint16_t sheet = 0;
  std::vector<uint16_t> offsets;  // y * 128 + x
  std::vector<uint8_t> before;
  std::vector<uint8_t> after;

  bool empty() const { return offsets.empty(); }
  size_t MemoryUsage() const {
    return offsets.size() * sizeof(uint16_t) + before.size() + after.size();
  }
};

/// The pixels that differ between two 4096-byte sheets.
SheetPixelDiff MakeSheetPixelDiff(uint16_t sheet,
                                  absl::Span<const uint8_t> before,
                                  absl::Span<const uint8_t> after);

/// Points `state` at a session's store. The Arena is taken to show the
/// store as it is now (both come from the same load).
void AttachSheetStore(GraphicsEditorState& state, zelda3::GameData* game_data);

/// The store behind the Arena's sheets, or null when graphics are not loaded
/// or the Arena holds another session's sheets.
zelda3::GraphicsSheetStore* ActiveSheetStore(const GraphicsEditorState& state);

/// Copies one sheet from the store into its Arena bitmap and queues a
/// texture update when the pixels differ.
void RefreshArenaSheet(GraphicsEditorState& state, uint16_t sheet);

/// Replaces a sheet through the store (one revision), refreshes its Arena
/// copy and marks it modified.
absl::Status CommitSheetPixels(GraphicsEditorState& state, uint16_t sheet,
                               absl::Span<const uint8_t> pixels);

/// Writes a diff's `before` values (undo) or `after` values (redo) through
/// the store.
absl::Status ApplySheetPixelDiff(GraphicsEditorState& state,
                                 const SheetPixelDiff& diff, bool redo);

/// Refreshes the Arena bitmaps whose store revision moved since their last
/// refresh. Returns how many were refreshed.
int SyncArenaFromStore(GraphicsEditorState& state);

}  // namespace editor
}  // namespace yaze

#endif  // YAZE_APP_EDITOR_GRAPHICS_GRAPHICS_SHEET_SYNC_H
