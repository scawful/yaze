#ifndef YAZE_APP_EDITOR_DUNGEON_DUNGEON_ROOM_DOCUMENT_FILE_H_
#define YAZE_APP_EDITOR_DUNGEON_DUNGEON_ROOM_DOCUMENT_FILE_H_

#include <string>

#include "absl/status/status.h"
#include "absl/status/statusor.h"

namespace yaze::editor {

// Bounded, schema-validated file interchange. Neither operation touches a ROM.
absl::StatusOr<std::string> ReadDungeonRoomDocumentFile(
    const std::string& path);
// Only .json paths are accepted. Existing files must themselves be valid room
// documents; unrelated files and symlinks are never replaced.
absl::Status WriteDungeonRoomDocumentFile(const std::string& path,
                                          const std::string& json);

}  // namespace yaze::editor

#endif  // YAZE_APP_EDITOR_DUNGEON_DUNGEON_ROOM_DOCUMENT_FILE_H_
