#ifndef YAZE_CORE_PROJECT_SECTIONS_H_
#define YAZE_CORE_PROJECT_SECTIONS_H_

#include "absl/strings/string_view.h"

namespace yaze::project {

// Modeled sections shared by the parser and raw project editor validation.
bool IsKnownProjectFileSection(absl::string_view name);

}  // namespace yaze::project

#endif  // YAZE_CORE_PROJECT_SECTIONS_H_
