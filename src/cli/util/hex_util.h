#ifndef YAZE_SRC_CLI_UTIL_HEX_UTIL_H_
#define YAZE_SRC_CLI_UTIL_HEX_UTIL_H_

#include <cstdint>

#include "absl/strings/string_view.h"
#include "util/hex.h"

namespace yaze {
namespace cli {
namespace util {

// The parser now lives in yaze::util (src/util/hex.h) so core, the editor and
// the CLI share one implementation instead of three. These aliases keep the
// existing yaze::cli::util::ParseHexString call sites working.
//
// Behaviour change from the previous strtoul-based version: a value that does
// not fit the requested type is always rejected. The old code relied on
// `unsigned long`, so on WebAssembly (where it is 32 bits) "-1" wrapped to
// 0xFFFFFFFF and passed the range check.
using yaze::util::ParseHexString;

}  // namespace util
}  // namespace cli
}  // namespace yaze

#endif  // YAZE_SRC_CLI_UTIL_HEX_UTIL_H_
