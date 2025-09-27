#ifndef YAZE_UTIL_BPS_H
#define YAZE_UTIL_BPS_H

#include <cstdint>
#include <vector>

namespace yaze {
namespace util {

void CreateBpsPatch(const std::vector<uint8_t> &source,
                    const std::vector<uint8_t> &target,
                    std::vector<uint8_t> &patch);

void ApplyBpsPatch(const std::vector<uint8_t> &source,
                   const std::vector<uint8_t> &patch,
                   std::vector<uint8_t> &target);

}  // namespace util
}  // namespace yaze

#endif  // YAZE_UTIL_BPS_H