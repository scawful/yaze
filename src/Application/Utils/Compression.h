#ifndef YAZE_APPLICATION_UTILS_COMPRESSION_H
#define YAZE_APPLICATION_UTILS_COMPRESSION_H

#include <cstddef>
#include <cstdlib>
#include <string>
#include <vector>

namespace yaze {
namespace Application {
namespace Utils {

class StdNintendoCompression {
 public:
  /*
   * This function decompress the c_data string starting at start and return the
   * decompressed data. returns NULL if an error occured.
   *
   * if max_length is set to 0 the function will stop when reaching the
   * 'compression end' marker (header == 0xFF) otherwise it will stop with an
   * error if max_length is reached.
   *
   * uncompressed_data_size is the resulting size of the decompressed string.
   * compressed_length is the length of the compressed data, meaning the number
   * of bytes read in c_data. mode is the variation of the compression, use one
   * of the define for it: D_NINTENDO_C_MODEX... 1 is is SMW, 2 is zelda3 gfx
   */

  char* Decompress(const char* c_data, const unsigned int start,
                   unsigned int max_length,
                   unsigned int* uncompressed_data_size,
                   unsigned int* compressed_length, char mode);

  /*
   * This function compress u_data following the compression format used by
   * Nintendo and return the resulting string or NULL if an error occured.
   *
   * start is the starting offset in u_ßdata to compress.
   * length is the length of u_data to compress
   * compressed_size is the resulting size of the compressed string.
   * mode is the variation of the compression.
   */

  char* Compress(const char* u_data, const unsigned int start,
                 const unsigned int length, unsigned int* compressed_size,
                 char mode);

 private:
  std::string compression_error_;
  std::string decompression_error_;
  bool std_nintendo_compression_sanity_check = false;

  struct CompressionComponent_;
  using CompressionComponent = CompressionComponent_;
  struct CompressionComponent_ {
    char command;
    unsigned int length;
    char* argument;
    unsigned int argument_length;
    CompressionComponent* next;
  };

  void PrintComponent(CompressionComponent* piece);
  CompressionComponent* CreateComponent(const char command,
                                        const unsigned int length,
                                        const char* args,
                                        const unsigned int argument_length);
  void DestroyComponent(CompressionComponent* piece);
  void DestroyChain(CompressionComponent* piece);
  CompressionComponent* merge_copy(CompressionComponent* start);
  unsigned int create_compression_string(CompressionComponent* start,
                                         char* output, char mode);
};

class ALTTPCompression {
 public:
  /*
   * This function decompress the c_data string starting at start and return the
   * decompressed data. returns NULL if an error occured.
   *
   * if max_length is set to 0 the function will stop when reaching the
   * 'compression end' marker (header == 0xFF) otherwise it will stop with an
   * error if max_length is reached.
   *
   * uncompressed_data_size is the resulting size of the decompressed string.
   * compressed_length is the length of the compressed data, meaning the number
   * of bytes read in c_data.
   */
  char* Decompress(const char* c_data, const unsigned int start,
                   unsigned int max_length,
                   unsigned int* uncompressed_data_size,
                   unsigned int* compressed_length, char mode);

  char* DecompressGfx(const char* c_data, const unsigned int start,
                      unsigned int max_length,
                      unsigned int* uncompressed_data_size,
                      unsigned int* compressed_length);
  char* DecompressOverworld(const char* c_data, const unsigned int start,
                            unsigned int max_length,
                            unsigned int* uncompressed_data_size,
                            unsigned int* compressed_length);

  /*
   * This function compress u_data following the compression format used by
   * Zelda3: a link to the past and return the resulting string or NULL if an
   * error occured.
   *
   * start is the starting offset in u_data to compress.
   * length is the length of u_data to compress
   * compressed_size is the resulting size of the compressed string.
   */
  char* Compress(const char* u_data, const unsigned int start,
                 const unsigned int length, unsigned int* compressed_size,
                 char mode);

  char* CompressGfx(const char* u_data, const unsigned int start,
                    const unsigned int length, unsigned int* compressed_size);
  char* CompressOverworld(const char* u_data, const unsigned int start,
                          const unsigned int length,
                          unsigned int* compressed_size);

 private:
  StdNintendoCompression std_nintendo_;
  std::string compression_error_;
  std::string decompression_error_;
};

}  // namespace Utils
}  // namespace Application
}  // namespace yaze

#endif