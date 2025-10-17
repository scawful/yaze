#ifndef YAZE_UTIL_BPS_H
#define YAZE_UTIL_BPS_H

#include <cstdint>
#include <vector>

#include "absl/status/status.h"

namespace yaze {
namespace util {

absl::Status CreateBpsPatch(const std::vector<uint8_t> &source,
                    const std::vector<uint8_t> &target,
                    std::vector<uint8_t> &patch);

absl::Status ApplyBpsPatch(const std::vector<uint8_t> &source,
                   const std::vector<uint8_t> &patch,
                   std::vector<uint8_t> &target);

}  // namespace util
}  // namespace yaze

#endif  // YAZE_UTIL_BPS_H