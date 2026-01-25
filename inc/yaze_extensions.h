#ifndef YAZE_EXTENSIONS_H
#define YAZE_EXTENSIONS_H

/**
 * @file yaze_extensions.h
 * @brief Extension interface for the YAZE public API.
 */

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>

#include "yaze_core.h"
#include "yaze_errors.h"

/**
 * @brief Function pointer to initialize the extension.
 *
 * @param context The editor context.
 */
typedef void (*yaze_initialize_func)(yaze_editor_context* context);
typedef void (*yaze_cleanup_func)(void);

/**
 * @defgroup extensions Extension System
 * @{
 */

/**
 * @brief Extension interface for YAZE
 *
 * Defines the interface for YAZE extensions. Extensions can add new
 * functionality to YAZE and can be written in C or other languages.
 */
typedef struct yaze_extension {
  const char* name;        /**< Extension name (must not be NULL) */
  const char* version;     /**< Extension version string */
  const char* description; /**< Brief description of functionality */
  const char* author;      /**< Extension author */
  int api_version;         /**< Required YAZE API version */

  /**
   * @brief Initialize the extension
   *
   * Called when the extension is loaded. Use this to set up
   * any resources or state needed by the extension.
   *
   * @param context Editor context provided by YAZE
   * @return YAZE_OK on success, error code on failure
   */
  yaze_status (*initialize)(yaze_editor_context* context);

  /**
   * @brief Clean up the extension
   *
   * Called when the extension is unloaded. Use this to clean up
   * any resources or state used by the extension.
   */
  void (*cleanup)(void);

  /**
   * @brief Get extension capabilities
   *
   * Returns a bitmask indicating what features this extension provides.
   *
   * @return Capability flags (see YAZE_EXT_CAP_* constants)
   */
  uint32_t (*get_capabilities)(void);
} yaze_extension;

/** Extension capability flags */
#define YAZE_EXT_CAP_ROM_EDITING    (1 << 0)  /**< Can edit ROM data */
#define YAZE_EXT_CAP_GRAPHICS       (1 << 1)  /**< Provides graphics functions */
#define YAZE_EXT_CAP_AUDIO          (1 << 2)  /**< Provides audio functions */
#define YAZE_EXT_CAP_SCRIPTING      (1 << 3)  /**< Provides scripting support */
#define YAZE_EXT_CAP_IMPORT_EXPORT  (1 << 4)  /**< Can import/export data */

/**
 * @brief Register an extension with YAZE
 *
 * @param extension Extension to register
 * @return YAZE_OK on success, error code on failure
 */
yaze_status yaze_register_extension(const yaze_extension* extension);

/**
 * @brief Unregister an extension
 *
 * @param name Name of extension to unregister
 * @return YAZE_OK on success, error code on failure
 */
yaze_status yaze_unregister_extension(const char* name);

/** @} */

#ifdef __cplusplus
}
#endif

#endif  // YAZE_EXTENSIONS_H
