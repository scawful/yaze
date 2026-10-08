#ifndef YAZE_APP_EDITOR_DUNGEON_UI_CRUMBLE_CRUMBLE_FLOOR_MODEL_H_
#define YAZE_APP_EDITOR_DUNGEON_UI_CRUMBLE_CRUMBLE_FLOOR_MODEL_H_

#include <array>
#include <cstdint>
#include <optional>
#include <vector>

#include "absl/status/status.h"

namespace yaze::editor {

// Oracle's CrumbleFloorTag_Main compares the complete BG1 top-left word of
// each 16px foot cell. Palette/flip bits must not be masked off. This model
// starts with an explicitly chosen previous cell; it does not emulate room
// entry RAM, collision, elevation, jumping, Minish, or other room tags.
enum class CrumbleFloorCell { kOther, kIntact, kCracked, kPit };
CrumbleFloorCell ClassifyCrumbleFloorWord(uint16_t word);

class CrumbleFloorModel {
 public:
  static constexpr int kWidth = 32;
  static constexpr int kCellCount = kWidth * kWidth;
  static constexpr int kMaxSteps = 512;
  using Words = std::array<uint16_t, kCellCount>;

  void Reset(const Words& words);
  absl::Status Step(int x, int y);
  void Back();
  CrumbleFloorCell cell(int index) const { return cells_.at(index); }
  const std::vector<int>& route() const { return route_; }
  std::optional<int> current() const {
    return route_.empty() ? std::nullopt : std::optional<int>(route_.back());
  }

 private:
  void Rebuild();
  Words words_{};
  std::array<CrumbleFloorCell, kCellCount> cells_{};
  std::vector<int> route_;
};

}  // namespace yaze::editor
#endif
