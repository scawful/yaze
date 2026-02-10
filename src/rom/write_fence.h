#ifndef YAZE_ROM_WRITE_FENCE_H
#define YAZE_ROM_WRITE_FENCE_H

#include <algorithm>
#include <cstdint>
#include <limits>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "absl/status/status.h"
#include "absl/strings/str_format.h"
#include "rom/rom.h"

namespace yaze::rom {

// Half-open PC offset range [start, end).
struct WriteRange {
  uint32_t start = 0;
  uint32_t end = 0;
  std::string label;
};

// A strict allow-list for ROM writes.
//
// When a WriteFence is active on a Rom (via ScopedWriteFence), Rom::Write* calls
// will be rejected unless the full write range is contained within one of the
// allowed ranges.
class WriteFence {
 public:
  absl::Status Allow(uint32_t start, uint32_t end, std::string_view label) {
    if (start >= end) {
      return absl::InvalidArgumentError(
          absl::StrFormat("WriteFence: invalid range [%u, %u)", start, end));
    }
    WriteRange r;
    r.start = start;
    r.end = end;
    r.label = std::string(label);

    for (const auto& existing : allowed_) {
      if (RangesOverlap(r.start, r.end, existing.start, existing.end)) {
        return absl::AlreadyExistsError(absl::StrFormat(
            "WriteFence: range [%u, %u) overlaps existing [%u, %u) (%s)",
            r.start, r.end, existing.start, existing.end,
            existing.label.c_str()));
      }
    }

    allowed_.push_back(std::move(r));
    std::sort(allowed_.begin(), allowed_.end(),
              [](const WriteRange& a, const WriteRange& b) {
                return a.start < b.start;
              });
    return absl::OkStatus();
  }

  absl::Status Check(uint32_t start, uint32_t size,
                     std::string_view op) const {
    if (size == 0) {
      return absl::OkStatus();
    }
    const uint64_t end64 = static_cast<uint64_t>(start) + size;
    if (end64 > static_cast<uint64_t>(std::numeric_limits<uint32_t>::max()) +
                    1ULL) {
      return absl::OutOfRangeError(
          "WriteFence: write range overflows uint32");
    }
    const uint32_t end = static_cast<uint32_t>(end64);
    for (const auto& a : allowed_) {
      if (start >= a.start && end <= a.end) {
        return absl::OkStatus();
      }
    }
    return absl::PermissionDeniedError(absl::StrFormat(
        "ROM write fence blocked %s at [0x%06X, 0x%06X)",
        std::string(op).c_str(), start, end));
  }

  void RecordWrite(uint32_t start, uint32_t size) {
    if (size == 0) {
      return;
    }
    const uint64_t end64 = static_cast<uint64_t>(start) + size;
    if (end64 > static_cast<uint64_t>(std::numeric_limits<uint32_t>::max()) +
                    1ULL) {
      // Can't represent end in uint32; ignore for recording.
      return;
    }
    const uint32_t end = static_cast<uint32_t>(end64);
    AddWrittenRange(start, end);
  }

  const std::vector<WriteRange>& allowed_ranges() const { return allowed_; }
  const std::vector<std::pair<uint32_t, uint32_t>>& written_ranges() const {
    return written_;
  }

  void ClearWritten() { written_.clear(); }

 private:
  static bool RangesOverlap(uint32_t a_start, uint32_t a_end, uint32_t b_start,
                            uint32_t b_end) {
    return a_start < b_end && b_start < a_end;
  }

  void AddWrittenRange(uint32_t start, uint32_t end) {
    if (start >= end) {
      return;
    }

    // Insert and merge into `written_` (kept sorted by start, non-overlapping).
    std::pair<uint32_t, uint32_t> r{start, end};
    auto it = std::lower_bound(
        written_.begin(), written_.end(), r,
        [](const auto& a, const auto& b) { return a.first < b.first; });
    written_.insert(it, r);

    // Merge pass (written_ is typically tiny).
    std::vector<std::pair<uint32_t, uint32_t>> merged;
    merged.reserve(written_.size());
    for (const auto& cur : written_) {
      if (merged.empty()) {
        merged.push_back(cur);
        continue;
      }
      auto& back = merged.back();
      if (cur.first <= back.second) {
        back.second = std::max(back.second, cur.second);
      } else {
        merged.push_back(cur);
      }
    }
    written_.swap(merged);
  }

  std::vector<WriteRange> allowed_;
  std::vector<std::pair<uint32_t, uint32_t>> written_;
};

// Scoped activation of a WriteFence. Fences are stacked; writes must be allowed
// by all fences currently active on the Rom (logical AND).
class ScopedWriteFence {
 public:
  ScopedWriteFence(Rom* rom, WriteFence* fence) : rom_(rom), fence_(fence) {
    if (rom_ && fence_) {
      rom_->PushWriteFence(fence_);
    }
  }

  ~ScopedWriteFence() {
    if (rom_ && fence_) {
      rom_->PopWriteFence(fence_);
    }
  }

  ScopedWriteFence(const ScopedWriteFence&) = delete;
  ScopedWriteFence& operator=(const ScopedWriteFence&) = delete;

 private:
  Rom* rom_ = nullptr;
  WriteFence* fence_ = nullptr;
};

}  // namespace yaze::rom

#endif  // YAZE_ROM_WRITE_FENCE_H

