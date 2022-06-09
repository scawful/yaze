#ifndef YAZE_APPLICATION_UTILS_ROM_H
#define YAZE_APPLICATION_UTILS_ROM_H

#include <bits/postypes.h>

#include <cstddef>
#include <fstream>
#include <iostream>
#include <memory>
#include <string>
#include <vector>

#include "Compression.h"
#include "Core/Constants.h"

namespace yaze {
namespace Application {
namespace Utils {

class ROM {
 public:
  void LoadFromFile(const std::string& path);

 private:
  std::vector<char*> original_rom_;
  std::vector<std::unique_ptr<char>> working_rom_;

  ALTTPCompression alttp_compressor_;
};

}  // namespace Utils
}  // namespace Application
}  // namespace yaze

#endif