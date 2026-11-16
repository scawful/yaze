#ifndef YAZE_ROM_PAGE_TILE16_ADMISSION_H_
#define YAZE_ROM_PAGE_TILE16_ADMISSION_H_

#include "core/project.h"
#include "rom/rom.h"

namespace yaze {
namespace rom {

// Admit a loaded revision-5 ROM for a saved project's opt-in. The ROM must be
// that project's backing file and match its expected hash; otherwise revision 5
// keeps refusing. Does not admit Build, Run, installation, or format migration.
bool AdmitProjectExperimentalPageTile16(const project::YazeProject& project,
                                        Rom* rom);

// Environment variable that opts this machine into revision-5 reads, with the
// same effect as passing --experimental-page-tile16 on every z3ed command.
inline constexpr char kExperimentalPageTile16EnvVar[] =
    "YAZE_EXPERIMENTAL_PAGE_TILE16";

// True only when YAZE_EXPERIMENTAL_PAGE_TILE16 is exactly "1". Any other value,
// or an unset variable, leaves revision 5 refused.
bool EnvRequestsExperimentalPageTile16();

}  // namespace rom
}  // namespace yaze

#endif  // YAZE_ROM_PAGE_TILE16_ADMISSION_H_
