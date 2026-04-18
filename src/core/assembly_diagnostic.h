#ifndef YAZE_CORE_ASSEMBLY_DIAGNOSTIC_H
#define YAZE_CORE_ASSEMBLY_DIAGNOSTIC_H

#include <cstdint>
#include <string>

namespace yaze {
namespace core {

enum class AssemblyDiagnosticSeverity {
  kError,
  kWarning,
  kNote,
};

// Structured diagnostic shared across assembler backends (Asar, z3dk).
// Populated natively by z3dk-core; best-effort parsed from flat strings
// by the Asar backend so downstream panels can present a uniform view.
struct AssemblyDiagnostic {
  AssemblyDiagnosticSeverity severity = AssemblyDiagnosticSeverity::kError;
  std::string message;
  std::string file;        // Source file (absolute or relative to patch root)
  int line = 0;            // 1-based; 0 if unknown
  int column = 0;          // 1-based; 0 if unknown
  std::string raw;         // Backend-formatted raw line, if available
};

}  // namespace core
}  // namespace yaze

#endif  // YAZE_CORE_ASSEMBLY_DIAGNOSTIC_H
