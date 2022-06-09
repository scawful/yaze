#ifndef YAZE_APPLICATION_UTILS_ROM_H
#define YAZE_APPLICATION_UTILS_ROM_H

#include <string>
#include <vector>
#include <bits/postypes.h>
#include <memory>
#include <iostream>
#include <fstream>
#include <cstddef>

#include "Compression.h"
#include "Core/Constants.h"

namespace yaze {
namespace Application {
namespace Utils {

class ROM {
 public:
  void LoadFromFile(const std::string & path);

 private:
  std::vector<char*> original_rom_;
  std::vector<std::unique_ptr<char>> working_rom_;

  ALTTPCompression alttp_compressor_;
};

}
}
}
 
#endif