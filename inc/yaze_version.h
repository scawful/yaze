#ifndef YAZE_VERSION_H
#define YAZE_VERSION_H

/**
 * @file yaze_version.h
 * @brief YAZE version helpers and compatibility checks.
 */

#ifdef __cplusplus
extern "C" {
#endif

#include <stdbool.h>

/**
 * @defgroup version Version Information
 * @{
 *
 * Version information is generated from CMakeLists.txt project() version.
 * When building with CMake, include yaze_config.h (from build directory) for:
 *   - YAZE_VERSION_MAJOR
 *   - YAZE_VERSION_MINOR
 *   - YAZE_VERSION_PATCH
 *   - YAZE_VERSION_STRING (e.g., "0.5.4")
 *   - YAZE_VERSION_NUMBER (e.g., 505)
 *
 * Single source of truth: project(yaze VERSION X.Y.Z) in CMakeLists.txt
 */

#ifndef YAZE_VERSION_STRING
/* Fallback if yaze_config.h not included - will be overridden by build */
#define YAZE_VERSION_STRING "0.5.4"
#define YAZE_VERSION_NUMBER 505
#endif

/** @} */

/**
 * @brief Check if the current YAZE version is compatible with the expected version
 *
 * @param expected_version Expected version string (e.g., "0.3.2")
 * @return true if compatible, false otherwise
 */
bool yaze_check_version_compatibility(const char* expected_version);

/**
 * @brief Get the current YAZE version string
 *
 * @return A null-terminated string containing the version
 */
const char* yaze_get_version_string(void);

/**
 * @brief Get the current YAZE version number
 *
 * @return Version number (major * 10000 + minor * 100 + patch)
 */
int yaze_get_version_number(void);

#ifdef __cplusplus
}
#endif

#endif  // YAZE_VERSION_H
