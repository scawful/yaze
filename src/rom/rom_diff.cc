#include "rom/rom_diff.h"

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <utility>
#include <vector>

namespace yaze::rom {

DiffSummary ComputeDiffRanges(const std::vector<uint8_t>& before,
                              const std::vector<uint8_t>& after) {
  DiffSummary summary;

  const size_t min_size = std::min(before.size(), after.size());
  const size_t max_size = std::max(before.size(), after.size());

  bool in_diff = false;
  uint32_t diff_start = 0;

  for (size_t i = 0; i < min_size; ++i) {
    if (before[i] != after[i]) {
      if (!in_diff) {
        in_diff = true;
        diff_start = static_cast<uint32_t>(i);
      }
      summary.total_bytes_changed++;
      continue;
    }

    if (in_diff) {
      summary.ranges.emplace_back(diff_start, static_cast<uint32_t>(i));
      in_diff = false;
    }
  }

  if (in_diff) {
    summary.ranges.emplace_back(diff_start, static_cast<uint32_t>(min_size));
  }

  if (before.size() != after.size()) {
    // Treat the size delta as a single tail diff.
    summary.ranges.emplace_back(static_cast<uint32_t>(min_size),
                                static_cast<uint32_t>(max_size));
    summary.total_bytes_changed += (max_size - min_size);
  }

  // Merge adjacent ranges (possible when a diff ends exactly at min_size and we
  // also add a tail diff due to size delta).
  if (summary.ranges.size() > 1) {
    std::sort(summary.ranges.begin(), summary.ranges.end());
    std::vector<std::pair<uint32_t, uint32_t>> merged;
    merged.reserve(summary.ranges.size());
    for (const auto& r : summary.ranges) {
      if (merged.empty() || merged.back().second < r.first) {
        merged.push_back(r);
        continue;
      }
      merged.back().second = std::max(merged.back().second, r.second);
    }
    summary.ranges = std::move(merged);
  }

  return summary;
}

}  // namespace yaze::rom

