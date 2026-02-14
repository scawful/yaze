#ifndef YAZE_CLI_Z3ED_ASCII_LOGO_H_
#define YAZE_CLI_Z3ED_ASCII_LOGO_H_

#include <string>

namespace yaze {
namespace cli {

// ASCII art logo for z3ed CLI
constexpr const char* kZ3edLogo = R"(
    ███████╗██████╗ ███████╗██████╗ 
    ╚══███╔╝╚════██╗██╔════╝██╔══██╗
      ███╔╝  █████╔╝█████╗  ██║  ██║
     ███╔╝   ╚═══██╗██╔══╝  ██║  ██║
    ███████╗██████╔╝███████╗██████╔╝
    ╚══════╝╚═════╝ ╚══════╝╚═════╝ 
          
       ▲      Zelda 3 Editor      
      ▲ ▲     AI-Powered CLI
     ▲▲▲▲▲    
)";

constexpr const char* kZ3edLogoCompact = R"(
 ╔════════════════════════════════╗
 ║  ███████╗██████╗ ███████╗██████╗  ║
 ║  ╚══███╔╝╚════██╗██╔════╝██╔══██╗ ║
 ║    ███╔╝  █████╔╝█████╗  ██║  ██║ ║
 ║   ███╔╝   ╚═══██╗██╔══╝  ██║  ██║ ║
 ║  ███████╗██████╔╝███████╗██████╔╝ ║
 ║  ╚══════╝╚═════╝ ╚══════╝╚═════╝  ║
 ║         ▲    Zelda 3 Editor      ║
 ║        ▲ ▲   AI-Powered CLI      ║
 ║       ▲▲▲▲▲  ROM Hacking Tool    ║
 ╚════════════════════════════════╝
)";

constexpr const char* kZ3edLogoMinimal = R"(
 ╭──────────────────────╮
 │  Z3ED - Zelda 3      │
 │    ▲   Editor CLI    │
 │   ▲ ▲  AI-Powered    │
 │  ▲▲▲▲▲               │
 ╰──────────────────────╯
)";

// Get logo with color codes for terminal
inline std::string GetColoredLogo() {
  return std::string("\033[1;36m") +  // Cyan
         "    ███████╗██████╗ ███████╗██████╗ \n"
         "    ╚══███╔╝╚════██╗██╔════╝██╔══██╗\n"
         "      ███╔╝  █████╔╝█████╗  ██║  ██║\n"
         "     ███╔╝   ╚═══██╗██╔══╝  ██║  ██║\n"
         "    ███████╗██████╔╝███████╗██████╔╝\n"
         "    ╚══════╝╚═════╝ ╚══════╝╚═════╝ \n"
         "\033[1;33m" +  // Yellow for triforce
         "          \n"
         "       ▲      " +
         "\033[1;37m" + "Zelda 3 Editor\n" +  // White
         "\033[1;33m" + "      ▲ ▲     " + "\033[0;37m" +
         "AI-Powered CLI\n" +  // Gray
         "\033[1;33m" + "     ▲▲▲▲▲    \n" + "\033[1;32m" +
         "  CLI ✦ Automation ✦ Command TODOs" + "\n" + "\033[0m";  // Reset
}

}  // namespace cli
}  // namespace yaze

#endif  // YAZE_CLI_Z3ED_ASCII_LOGO_H_
