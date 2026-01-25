#ifndef YAZE_CORE_H
#define YAZE_CORE_H

/**
 * @file yaze_core.h
 * @brief Core initialization and editor context for the YAZE public API.
 */

#ifdef __cplusplus
extern "C" {
#endif

#include <stddef.h>

#include "yaze_errors.h"
#include "zelda.h"

typedef struct yaze_editor_context {
  zelda3_rom* rom;
  const char* error_message;
} yaze_editor_context;

/**
 * @brief Initialize the YAZE library
 *
 * This function must be called before using any other YAZE functions.
 * It initializes internal subsystems and prepares the library for use.
 *
 * @return YAZE_OK on success, error code on failure
 */
yaze_status yaze_library_init(void);

/**
 * @brief Shutdown the YAZE library
 *
 * This function cleans up resources allocated by yaze_library_init().
 * After calling this function, no other YAZE functions should be called
 * until yaze_library_init() is called again.
 */
void yaze_library_shutdown(void);

/**
 * @brief Main entry point for the YAZE application
 *
 * @param argc Number of command line arguments
 * @param argv Array of command line argument strings
 * @return Exit code (0 for success, non-zero for error)
 */
int yaze_app_main(int argc, char** argv);

/**
 * @brief Initialize a YAZE editor context
 *
 * Creates and initializes an editor context for working with ROM files.
 * The context manages the ROM data and provides access to editing functions.
 *
 * @param context Pointer to context structure to initialize
 * @param rom_filename Path to the ROM file to load (can be NULL to create empty context)
 * @return YAZE_OK on success, error code on failure
 *
 * @note The caller is responsible for calling yaze_shutdown() to clean up the context
 */
yaze_status yaze_init(yaze_editor_context* context, const char* rom_filename);

/**
 * @brief Shutdown and clean up a YAZE editor context
 *
 * Releases all resources associated with the context, including ROM data.
 * After calling this function, the context should not be used.
 *
 * @param context Pointer to context to shutdown
 * @return YAZE_OK on success, error code on failure
 */
yaze_status yaze_shutdown(yaze_editor_context* context);

#ifdef __cplusplus
}
#endif

#endif  // YAZE_CORE_H
