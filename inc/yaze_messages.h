#ifndef YAZE_MESSAGES_H
#define YAZE_MESSAGES_H

/**
 * @file yaze_messages.h
 * @brief Message system helpers for the YAZE public API.
 */

#ifdef __cplusplus
extern "C" {
#endif

#include "yaze_errors.h"
#include "zelda.h"

/**
 * @brief Load all text messages from ROM
 *
 * Loads and parses all in-game text messages from the ROM.
 *
 * @param rom The ROM to load messages from
 * @param messages Pointer to store array of messages
 * @param message_count Pointer to store number of messages loaded
 * @return YAZE_OK on success, error code on failure
 *
 * @note Caller must free the messages array when done
 */
yaze_status yaze_load_messages(const zelda3_rom* rom, zelda3_message** messages, int* message_count);

/**
 * @brief Get a specific message by ID
 *
 * @param rom ROM to load from
 * @param message_id Message ID to retrieve
 * @return Pointer to message data, or NULL if not found
 */
const zelda3_message* yaze_get_message(const zelda3_rom* rom, int message_id);

/**
 * @brief Free message data
 *
 * @param messages Array of messages to free
 * @param message_count Number of messages in array
 */
void yaze_free_messages(zelda3_message* messages, int message_count);

#ifdef __cplusplus
}
#endif

#endif  // YAZE_MESSAGES_H
