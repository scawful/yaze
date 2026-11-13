#ifndef YAZE_CLI_Z3ED_ASCII_LOGO_H_
#define YAZE_CLI_Z3ED_ASCII_LOGO_H_

#include <string>

#include "absl/strings/str_cat.h"
#include "cli/util/ansi_style.h"

namespace yaze::cli {

// The z3ed banner printed by --help and --version.
inline std::string RenderLogo(const util::AnsiStyle& style) {
  return absl::StrCat(style.Heading(),
                      "    ███████╗██████╗ ███████╗██████╗ \n"
                      "    ╚══███╔╝╚════██╗██╔════╝██╔══██╗\n"
                      "      ███╔╝  █████╔╝█████╗  ██║  ██║\n"
                      "     ███╔╝   ╚═══██╗██╔══╝  ██║  ██║\n"
                      "    ███████╗██████╔╝███████╗██████╔╝\n"
                      "    ╚══════╝╚═════╝ ╚══════╝╚═════╝ \n",
                      style.Highlight(), "          \n       ▲      ",
                      style.Emphasis(), "Zelda 3 Editor\n", style.Highlight(),
                      "      ▲ ▲     ", style.Muted(), "AI-Powered CLI\n",
                      style.Highlight(), "     ▲▲▲▲▲    \n", style.Success(),
                      "  CLI ✦ Automation ✦ Command TODOs\n", style.Reset());
}

}  // namespace yaze::cli

#endif  // YAZE_CLI_Z3ED_ASCII_LOGO_H_
