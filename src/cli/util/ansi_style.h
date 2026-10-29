#ifndef YAZE_CLI_UTIL_ANSI_STYLE_H_
#define YAZE_CLI_UTIL_ANSI_STYLE_H_

#include <cstdio>

#include "absl/strings/string_view.h"

namespace yaze::cli::util {

// Semantic ANSI styles for z3ed output. Every accessor returns an empty string
// when color is disabled, so callers can stream styles unconditionally.
class AnsiStyle {
 public:
  constexpr explicit AnsiStyle(bool enabled) : enabled_(enabled) {}

  // Color is enabled only for an interactive terminal, and never when
  // NO_COLOR is set or TERM=dumb.
  static AnsiStyle ForStream(std::FILE* stream);

  constexpr bool enabled() const { return enabled_; }

  constexpr absl::string_view Heading() const { return Code("\033[1;36m"); }
  constexpr absl::string_view Highlight() const { return Code("\033[1;33m"); }
  constexpr absl::string_view Emphasis() const { return Code("\033[1;37m"); }
  constexpr absl::string_view Muted() const { return Code("\033[0;37m"); }
  constexpr absl::string_view Success() const { return Code("\033[1;32m"); }
  constexpr absl::string_view Failure() const { return Code("\033[1;31m"); }
  constexpr absl::string_view Reset() const { return Code("\033[0m"); }

 private:
  constexpr absl::string_view Code(absl::string_view code) const {
    return enabled_ ? code : absl::string_view();
  }

  bool enabled_;
};

}  // namespace yaze::cli::util

#endif  // YAZE_CLI_UTIL_ANSI_STYLE_H_
