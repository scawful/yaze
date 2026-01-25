#ifndef YAZE_ERRORS_H
#define YAZE_ERRORS_H

/**
 * @file yaze_errors.h
 * @brief Status codes and error helpers for the YAZE public API.
 */

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Status codes returned by YAZE functions
 *
 * All YAZE functions that can fail return a status code to indicate
 * success or the type of error that occurred.
 */
typedef enum yaze_status {
  YAZE_OK = 0,                    /**< Operation completed successfully */
  YAZE_ERROR_UNKNOWN = -1,        /**< Unknown error occurred */
  YAZE_ERROR_INVALID_ARG = 1,     /**< Invalid argument provided */
  YAZE_ERROR_FILE_NOT_FOUND = 2,  /**< File not found */
  YAZE_ERROR_MEMORY = 3,          /**< Memory allocation failed */
  YAZE_ERROR_IO = 4,              /**< I/O operation failed */
  YAZE_ERROR_CORRUPTION = 5,      /**< Data corruption detected */
  YAZE_ERROR_NOT_INITIALIZED = 6, /**< Component not initialized */
} yaze_status;

/**
 * @brief Convert a status code to a human-readable string
 *
 * @param status The status code to convert
 * @return A null-terminated string describing the status
 */
const char* yaze_status_to_string(yaze_status status);

#ifdef __cplusplus
}
#endif

#endif  // YAZE_ERRORS_H
