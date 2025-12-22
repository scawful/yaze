#ifndef YAZE_UTIL_MACRO_H
#define YAZE_UTIL_MACRO_H

using uint = unsigned int;

#define TAB_ITEM(w) if (ImGui::BeginTabItem(w)) {
#define END_TAB_ITEM() \
  ImGui::EndTabItem(); \
  }

#define BEGIN_TABLE(l, n, f) if (ImGui::BeginTable(l, n, f, ImVec2(0, 0))) {
#define SETUP_COLUMN(l) ImGui::TableSetupColumn(l);

#define TABLE_HEADERS()     \
  ImGui::TableHeadersRow(); \
  ImGui::TableNextRow();

#define NEXT_COLUMN() ImGui::TableNextColumn();

#define END_TABLE()  \
  ImGui::EndTable(); \
  }

#define HOVER_HINT(string)    \
  if (ImGui::IsItemHovered()) \
  ImGui::SetTooltip(string)

#define PRINT_IF_ERROR(expression)                \
  {                                               \
    auto error = expression;                      \
    if (!error.ok()) {                            \
      std::cout << error.ToString() << std::endl; \
    }                                             \
  }

#define EXIT_IF_ERROR(expression)                 \
  {                                               \
    auto error = expression;                      \
    if (!error.ok()) {                            \
      std::cout << error.ToString() << std::endl; \
      return EXIT_FAILURE;                        \
    }                                             \
  }

#define RETURN_VOID_IF_ERROR(expression)          \
  {                                               \
    auto error = expression;                      \
    if (!error.ok()) {                            \
      std::cout << error.ToString() << std::endl; \
      return;                                     \
    }                                             \
  }

#define RETURN_IF_ERROR(expression) \
  {                                 \
    auto error = expression;        \
    if (!error.ok()) {              \
      return error;                 \
    }                               \
  }

#define ASSIGN_OR_RETURN(type_variable_name, expression)         \
  ASSIGN_OR_RETURN_IMPL(APPEND_NUMBER(error_or_value, __LINE__), \
                        type_variable_name, expression)

#define ASSIGN_OR_RETURN_IMPL(error_or_value, type_variable_name, expression) \
  auto error_or_value = expression;                                           \
  if (!error_or_value.ok()) {                                                 \
    return error_or_value.status();                                           \
  }                                                                           \
  type_variable_name = std::move(*error_or_value)

#define ASSIGN_OR_LOG_ERROR(type_variable_name, expression)         \
  ASSIGN_OR_LOG_ERROR_IMPL(APPEND_NUMBER(error_or_value, __LINE__), \
                           type_variable_name, expression)

#define ASSIGN_OR_LOG_ERROR_IMPL(error_or_value, type_variable_name, \
                                 expression)                         \
  auto error_or_value = expression;                                  \
  if (!error_or_value.ok()) {                                        \
    std::cout << error_or_value.status().ToString() << std::endl;    \
  }                                                                  \
  type_variable_name = std::move(*error_or_value);

#define APPEND_NUMBER(expression, number) \
  APPEND_NUMBER_INNER(expression, number)

#define APPEND_NUMBER_INNER(expression, number) expression##number

#define TEXT_WITH_SEPARATOR(text) \
  ImGui::Text(text);              \
  ImGui::Separator();

#define TABLE_BORDERS_RESIZABLE \
  ImGuiTableFlags_Borders | ImGuiTableFlags_Resizable

#define CLEAR_AND_RETURN_STATUS(status) \
  if (!status.ok()) {                   \
    auto temp = status;                 \
    status = absl::OkStatus();          \
    return temp;                        \
  }

#define RETURN_IF_EXCEPTION(expression) \
  try {                                 \
    expression;                         \
  } catch (const std::exception& e) {   \
    std::cerr << e.what() << std::endl; \
    return EXIT_FAILURE;                \
  }

#define SDL_RETURN_IF_ERROR()                   \
  if (SDL_GetError() != nullptr) {              \
    return absl::InternalError(SDL_GetError()); \
  }

// ===========================================================================
// ROM Bounds Checking Macros
// ===========================================================================
// These macros provide consistent bounds checking for ROM operations,
// ensuring all ROM reads and writes are validated against the ROM size.

/**
 * @brief Validate ROM offset is within bounds before reading/writing
 *
 * Returns absl::OutOfRangeError if the offset + size exceeds ROM size.
 *
 * @param rom Pointer to the Rom object
 * @param offset Start offset for the operation
 * @param size Number of bytes to read/write
 */
#define RETURN_IF_ROM_OUT_OF_RANGE(rom, offset, size)                     \
  do {                                                                    \
    if (((offset) + (size)) > (rom)->size()) {                            \
      return absl::OutOfRangeError(                                       \
          absl::StrFormat("ROM access out of range: offset 0x%X + size "  \
                          "0x%X exceeds ROM size 0x%X",                   \
                          (offset), (size), (rom)->size()));              \
    }                                                                     \
  } while (0)

/**
 * @brief Validate ROM offset is within bounds, returning false on failure
 *
 * Use this variant for bool-returning functions that can't return Status.
 *
 * @param rom Pointer to the Rom object
 * @param offset Start offset for the operation
 * @param size Number of bytes to read/write
 */
#define RETURN_FALSE_IF_ROM_OUT_OF_RANGE(rom, offset, size) \
  do {                                                      \
    if (((offset) + (size)) > (rom)->size()) {              \
      return false;                                         \
    }                                                       \
  } while (0)

/**
 * @brief Validate room ID is within valid dungeon room range
 *
 * SNES A Link to the Past has 296 dungeon rooms (0x000-0x127).
 *
 * @param room_id The room ID to validate
 */
#define RETURN_IF_ROOM_OUT_OF_RANGE(room_id)                               \
  do {                                                                     \
    constexpr int kMaxRoomId = 296;                                        \
    if ((room_id) < 0 || (room_id) >= kMaxRoomId) {                        \
      return absl::OutOfRangeError(absl::StrFormat(                        \
          "Room ID %d out of range [0, %d)", (room_id), kMaxRoomId));      \
    }                                                                      \
  } while (0)

/**
 * @brief Validate overworld map ID is within valid range
 *
 * SNES A Link to the Past has 160 overworld maps (0x00-0x9F).
 *
 * @param map_id The map ID to validate
 */
#define RETURN_IF_MAP_OUT_OF_RANGE(map_id)                                 \
  do {                                                                     \
    constexpr int kMaxMapId = 160;                                         \
    if ((map_id) < 0 || (map_id) >= kMaxMapId) {                           \
      return absl::OutOfRangeError(absl::StrFormat(                        \
          "Map ID %d out of range [0, %d)", (map_id), kMaxMapId));         \
    }                                                                      \
  } while (0)

/**
 * @brief Validate palette index is within SNES palette group range
 *
 * SNES palettes have 16 entries (0-15) per sub-palette.
 *
 * @param palette_id The palette index to validate
 * @param max_palettes Maximum number of palettes in the group
 */
#define RETURN_IF_PALETTE_OUT_OF_RANGE(palette_id, max_palettes)           \
  do {                                                                     \
    if ((palette_id) < 0 || (palette_id) >= (max_palettes)) {              \
      return absl::OutOfRangeError(absl::StrFormat(                        \
          "Palette ID %d out of range [0, %d)",                            \
          (palette_id), (max_palettes)));                                  \
    }                                                                      \
  } while (0)

#endif  // YAZE_UTIL_MACRO_H