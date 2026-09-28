#include "app/editor/dungeon/dungeon_room_document_file.h"

#include <algorithm>
#include <cctype>
#include <filesystem>
#include <fstream>

#include "app/editor/dungeon/dungeon_room_transfer.h"
#include "core/project.h"
#include "util/macro.h"

namespace yaze::editor {

absl::StatusOr<std::string> ReadDungeonRoomDocumentFile(
    const std::string& path) {
  std::error_code error;
  if (!std::filesystem::is_regular_file(path, error) || error) {
    return absl::InvalidArgumentError("Choose a regular room JSON file.");
  }
  std::ifstream file(path, std::ios::binary);
  if (!file) {
    return absl::PermissionDeniedError("Cannot open room JSON file.");
  }
  // Read at most limit + 1 even if the file grows after opening. Do not allocate
  // based on an untrusted on-disk size or read a whole oversized file first.
  std::string json(kMaxDungeonRoomDocumentBytes + 1, '\0');
  file.read(json.data(), static_cast<std::streamsize>(json.size()));
  json.resize(static_cast<size_t>(file.gcount()));
  if (file.bad()) {
    return absl::DataLossError("Failed to read room JSON file.");
  }
  if (json.size() > kMaxDungeonRoomDocumentBytes) {
    return absl::ResourceExhaustedError(
        "Room JSON exceeds the 1 MiB document limit.");
  }
  RETURN_IF_ERROR(ParseDungeonRoomDocument(json).status());
  return json;
}

absl::Status WriteDungeonRoomDocumentFile(const std::string& path,
                                          const std::string& json) {
#ifdef __EMSCRIPTEN__
  return absl::UnimplementedError("Use Copy Room JSON in the browser build.");
#else
  RETURN_IF_ERROR(ParseDungeonRoomDocument(json).status());
  std::string extension = std::filesystem::path(path).extension().string();
  std::transform(
      extension.begin(), extension.end(), extension.begin(),
      [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
  if (extension != ".json") {
    return absl::InvalidArgumentError(
        "Save room templates with a .json extension.");
  }
  std::error_code error;
  const auto status = std::filesystem::symlink_status(path, error);
  if (error && error != std::errc::no_such_file_or_directory) {
    return absl::PermissionDeniedError("Cannot inspect room JSON destination.");
  }
  const bool exists = std::filesystem::exists(status);
  if (exists) {
    if (!std::filesystem::is_regular_file(status)) {
      return absl::InvalidArgumentError(
          "Cannot replace a directory or symlink.");
    }
    if (!ReadDungeonRoomDocumentFile(path).ok()) {
      return absl::FailedPreconditionError(
          "The destination is not a valid room template. Choose another file.");
    }
  }
  return project::WriteProjectFileAtomically(path, json, exists);
#endif
}

}  // namespace yaze::editor
