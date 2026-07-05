#ifndef YAZE_UTIL_I18N_TR_H
#define YAZE_UTIL_I18N_TR_H

// Umbrella header included by every translation unit that wraps UI strings.
// It exposes the unqualified name `tr(...)` inside `namespace yaze`, so call
// sites in yaze::editor / yaze::gui / yaze::emu can simply write tr("File").
//
// Keep this header tiny and dependency-light: it only forwards to the i18n
// translator so adding it to thousands of files stays cheap to compile.

#include "util/i18n/translator.h"

namespace yaze {
using ::yaze::i18n::tr;
}  // namespace yaze

#endif  // YAZE_UTIL_I18N_TR_H
