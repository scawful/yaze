#ifndef YAZE_CLI_PATCH_H
#define YAZE_CLI_PATCH_H

#include <zlib.h>

#include <cstdint>
#include <cstring>
#include <fstream>
#include <iostream>
#include <vector>
namespace yaze {
namespace cli {
void encode(uint64_t data, std::vector<uint8_t>& output);

uint64_t decode(const std::vector<uint8_t>& input, size_t& offset);

uint32_t crc32(const std::vector<uint8_t>& data);

void CreateBpsPatch(const std::vector<uint8_t>& source,
                    const std::vector<uint8_t>& target,
                    std::vector<uint8_t>& patch);

void ApplyBpsPatch(const std::vector<uint8_t>& source,
                   const std::vector<uint8_t>& patch,
                   std::vector<uint8_t>& target);

}  // namespace cli
}  // namespace yaze
#endif