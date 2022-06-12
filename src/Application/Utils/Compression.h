#ifndef YAZE_APPLICATION_UTILS_COMPRESSION_H
#define YAZE_APPLICATION_UTILS_COMPRESSION_H

#include <cstddef>
#include <cstdlib>
#include <string>
#include <vector>

namespace yaze {
namespace Application {
namespace Utils {

class ALTTPCompression {
 public:

 char* DecompressGfx(const char* c_data, const unsigned int start,
                      unsigned int max_length,
                      unsigned int* uncompressed_data_size,
                      unsigned int* compressed_length);
 char* DecompressOverworld(const char* c_data, const unsigned int start,
                            unsigned int max_length,
                            unsigned int* uncompressed_data_size,
                            unsigned int* compressed_length);

 char* CompressGfx(const char* u_data, const unsigned int start,
                    const unsigned int length, unsigned int* compressed_size);
 char* CompressOverworld(const char* u_data, const unsigned int start,
                          const unsigned int length,
                          unsigned int* compressed_size);

 private:
  std::string compression_error_;
  std::string decompression_error_;
};

}  // namespace Utils
}  // namespace Application
}  // namespace yaze

#endif