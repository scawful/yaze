#ifndef YAZE_APP_EDITOR_OVERWORLD_SCRATCH_WORKSPACE_H_
#define YAZE_APP_EDITOR_OVERWORLD_SCRATCH_WORKSPACE_H_

#include <array>
#include <cstdint>
#include <filesystem>
#include <optional>
#include <string>
#include <vector>

#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "absl/strings/string_view.h"
#include "app/editor/overworld/painting/tile_brush.h"

namespace yaze::editor {

// Detached layouts contain Tile16 references, never world-map mutations.
struct ScratchPage {
  uint64_t id = 1;
  std::string name = "Scratch Space";
  int width = 16;
  int height = 16;
  bool in_use = false;
  int source_map = -1;
  std::array<std::array<int, 32>, 32> tile_data{};  // [x][y]

  bool operator==(const ScratchPage& other) const;
  TileBrush AsBrush() const;
};

struct ScratchWorkspaceState {
  std::vector<ScratchPage> pages = {ScratchPage{}};
  uint64_t selected_id = 1;
  uint64_t next_id = 2;

  bool operator==(const ScratchWorkspaceState& other) const;
};

class ScratchWorkspace {
 public:
  static constexpr size_t kMaxPages = 32;

  const ScratchWorkspaceState& state() const { return state_; }
  void Restore(const ScratchWorkspaceState& state);
  ScratchPage& current();
  const ScratchPage& current() const;
  bool Select(uint64_t id);
  absl::Status AddPage(bool duplicate = false);
  absl::Status DeletePage();
  absl::Status Rename(absl::string_view name);
  absl::Status Resize(int width, int height);
  absl::Status Stamp(const TileBrush& brush, int x, int y);
  absl::Status Capture(const TileBrush& brush, int source_map);
  void Clear();

  // V1 is the original single ScratchPad.dat. Invalid input never mutates state.
  absl::Status Parse(absl::string_view contents);
  std::string Serialize() const;
  absl::Status Load(const std::filesystem::path& path);
  // Reject another session's changes instead of silently overwriting its pages.
  absl::Status Save(const std::filesystem::path& path);

 private:
  ScratchWorkspaceState state_;
  std::optional<std::string> saved_contents_;
  bool file_loaded_ = false;
};

}  // namespace yaze::editor

#endif  // YAZE_APP_EDITOR_OVERWORLD_SCRATCH_WORKSPACE_H_
