#include "cli/util/ansi_style.h"

#include <cstdlib>
#include <cstring>

#ifdef _WIN32
#include <io.h>
#else
#include <unistd.h>
#endif

namespace yaze::cli::util {
namespace {

bool IsTerminal(std::FILE* stream) {
#ifdef _WIN32
  return _isatty(_fileno(stream)) != 0;
#else
  return isatty(fileno(stream)) != 0;
#endif
}

}  // namespace

AnsiStyle AnsiStyle::ForStream(std::FILE* stream) {
  const char* no_color = std::getenv("NO_COLOR");
  if (no_color != nullptr && no_color[0] != '\0') {
    return AnsiStyle(false);
  }
  const char* term = std::getenv("TERM");
  if (term != nullptr && std::strcmp(term, "dumb") == 0) {
    return AnsiStyle(false);
  }
  return AnsiStyle(stream != nullptr && IsTerminal(stream));
}

}  // namespace yaze::cli::util
