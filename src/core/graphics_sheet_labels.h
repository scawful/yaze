#ifndef YAZE_CORE_GRAPHICS_SHEET_LABELS_H_
#define YAZE_CORE_GRAPHICS_SHEET_LABELS_H_

#include <cstdint>
#include <string>

#include "core/project.h"

namespace yaze::core {

// Project label type for graphics sheets. Keys are decimal sheet ids, the
// canonical key format of every project label type.
inline constexpr char kGraphicsSheetLabelType[] = "graphics";

// The project's label for `sheet`, or "" when it has none. Also accepts a
// legacy "0x%02X" key.
std::string GetGraphicsSheetLabel(const project::YazeProject& project,
                                  uint16_t sheet);

// Sets the project's label for `sheet`; an empty label removes it.
void SetGraphicsSheetLabel(project::YazeProject& project, uint16_t sheet,
                           const std::string& label);

// Labels sheet (value + 0x73) with the names Oracle's Spritesets CSV lists
// for each sprite value (cells such as "0x46 Cannon Soldiers"), joined with
// "; ". Existing labels are kept unless `overwrite`. Returns the number of
// labels written.
int ImportSpritesetSheetLabels(project::YazeProject& project,
                               const std::string& csv_text,
                               bool overwrite = false);

}  // namespace yaze::core

#endif  // YAZE_CORE_GRAPHICS_SHEET_LABELS_H_
