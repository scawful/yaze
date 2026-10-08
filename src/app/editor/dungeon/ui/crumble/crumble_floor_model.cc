#include "app/editor/dungeon/ui/crumble/crumble_floor_model.h"

#include <cstdlib>

namespace yaze::editor {

CrumbleFloorCell ClassifyCrumbleFloorWord(uint16_t word) {
  switch (word) {
    case 0x0C62:
    case 0x0C63:
      return CrumbleFloorCell::kIntact;
    case 0x0CCC:
      return CrumbleFloorCell::kCracked;
    case 0x01E9:
      return CrumbleFloorCell::kPit;
    default:
      return CrumbleFloorCell::kOther;
  }
}

namespace {
void Depart(CrumbleFloorCell& cell) {
  if (cell == CrumbleFloorCell::kIntact) {
    cell = CrumbleFloorCell::kCracked;
  } else if (cell == CrumbleFloorCell::kCracked) {
    cell = CrumbleFloorCell::kPit;
  }
}
}  // namespace

void CrumbleFloorModel::Reset(const Words& words) {
  words_ = words;
  route_.clear();
  Rebuild();
}

absl::Status CrumbleFloorModel::Step(int x, int y) {
  if (x < 0 || y < 0 || x >= kWidth || y >= kWidth) {
    return absl::InvalidArgumentError("Choose a cell inside this room.");
  }
  const int next = y * kWidth + x;
  if (current() == next) {
    return absl::OkStatus();  // Standing still does not wear the tile.
  }
  if (route_.size() >= kMaxSteps) {
    return absl::ResourceExhaustedError(
        "512-cell rehearsal limit. Reset to start again.");
  }
  if (current() &&
      std::abs(x - *current() % kWidth) + std::abs(y - *current() / kWidth) !=
          1) {
    return absl::InvalidArgumentError(
        "Choose an adjacent cell; jumps are not modeled.");
  }
  if (cells_[next] == CrumbleFloorCell::kPit) {
    return absl::FailedPreconditionError(
        "That cell is a pit. Back removes the last step.");
  }
  if (cells_[next] == CrumbleFloorCell::kOther) {
    return absl::FailedPreconditionError(
        "Only highlighted crumble cells are modeled.");
  }
  if (current()) {
    Depart(cells_[*current()]);
  }
  route_.push_back(next);
  return absl::OkStatus();
}

void CrumbleFloorModel::Back() {
  if (!route_.empty()) {
    route_.pop_back();
    Rebuild();
  }
}

void CrumbleFloorModel::Rebuild() {
  for (int i = 0; i < kCellCount; ++i) {
    cells_[i] = ClassifyCrumbleFloorWord(words_[i]);
  }
  for (size_t i = 1; i < route_.size(); ++i) {
    Depart(cells_[route_[i - 1]]);
  }
}

}  // namespace yaze::editor
