#ifndef YAZE_ROM_ROM_DIFF_H
#define YAZE_ROM_ROM_DIFF_H

#include <cstddef>
#include <cstdint>
#include <utility>
#include <vector>

namespace yaze::rom {

// Summary of byte-level differences between two ROM buffers.
//
// Ranges use PC offsets with half-open semantics: [start, end).
struct DiffSummary {
  size_t total_bytes_changed = 0;
  std::vector<std::pair<uint32_t, uint32_t>> ranges;
};

// Compute contiguous diff ranges between `before` and `after`.
//
// Notes:
// - If sizes differ, the extra tail is reported as a single range.
// - `total_bytes_changed` counts the number of differing positions, plus any
//   size delta.
DiffSummary ComputeDiffRanges(const std::vector<uint8_t>& before,
                              const std::vector<uint8_t>& after);

}  // namespace yaze::rom

#endif  // YAZE_ROM_ROM_DIFF_H

