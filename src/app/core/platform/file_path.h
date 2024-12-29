#ifndef YAZE_APP_CORE_PLATFORM_FILE_PATH_H
#define YAZE_APP_CORE_PLATFORM_FILE_PATH_H

#include <string>

namespace yaze {
namespace core {

/**
 * @brief GetBundleResourcePath returns the path to the bundle resource
 * directory. Specific to MacOS.
 */
std::string GetBundleResourcePath();

}  // namespace core
}  // namespace yaze

#endif  // YAZE_APP_CORE_PLATFORM_FILE_PATH_H
