#include "Compression.h"

#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <cstddef>
#include <iostream>
#include <memory>
#include <vector>

#include "compressions/alttpcompression.h"
#include "compressions/stdnintendo.h"

#define INITIAL_ALLOC_SIZE 1024

#define D_CMD_COPY 0
#define D_CMD_BYTE_REPEAT 1
#define D_CMD_WORD_REPEAT 2
#define D_CMD_BYTE_INC 3
#define D_CMD_COPY_EXISTING 4

#define D_MAX_NORMAL_length 32
#define D_max_length 1024

#define D_NINTENDO_C_MODE1 0
#define D_NINTENDO_C_MODE2 1

#define X_ std::byte {
#define _X }

#define MY_BUILD_HEADER(command, length) (command << 5) + ((length)-1)

namespace yaze {
namespace Application {
namespace Utils {

char* ALTTPCompression::DecompressGfx(const char* c_data,
                                      const unsigned int start,
                                      unsigned int max_length,
                                      unsigned int* uncompressed_data_size,
                                      unsigned int* compressed_length) {
  char* data = alttp_decompress_gfx(c_data, start, max_length,
                                    uncompressed_data_size, compressed_length);
  return data;
}

char* ALTTPCompression::DecompressOverworld(
    const char* c_data, const unsigned int start, unsigned int max_length,
    unsigned int* uncompressed_data_size, unsigned int* compressed_length) {
  char* toret = alttp_decompress_overworld(
      c_data, start, max_length, uncompressed_data_size, compressed_length);
  return toret;
}

char* ALTTPCompression::CompressGfx(const char* u_data,
                                    const unsigned int start,
                                    const unsigned int length,
                                    unsigned int* compressed_size) {
  return alttp_compress_gfx(u_data, start, length, compressed_size);
}

char* ALTTPCompression::CompressOverworld(const char* u_data,
                                          const unsigned int start,
                                          const unsigned int length,
                                          unsigned int* compressed_size) {
  return alttp_compress_overworld(u_data, start, length, compressed_size);
}

}  // namespace Utils
}  // namespace Application
}  // namespace yaze
