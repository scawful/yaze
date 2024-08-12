#include "file_util.h"

#if defined(_WIN32)
#include <windows.h>
#else
#include <dirent.h>
#include <sys/stat.h>
#endif

#include <fstream>
#include <sstream>

namespace yaze {
namespace app {
namespace core {

std::string LoadFile(const std::string& filename) {
  std::string contents;
  // TODO: Load a file based on the platform and add error handling
  return contents;
}

}  // namespace core
}  // namespace app
}  // namespace yaze