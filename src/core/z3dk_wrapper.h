#ifndef YAZE_CORE_Z3DK_WRAPPER_H
#define YAZE_CORE_Z3DK_WRAPPER_H

// Adapter around z3dk::Assembler that mirrors the subset of
// core::AsarWrapper's surface that AssemblyEditor consumes. Only built when
// YAZE_WITH_Z3DK is defined (YAZE_ENABLE_Z3DK=ON at configure time).
//
// The adapter produces a core::AsarPatchResult for drop-in compatibility
// with the existing editor plumbing, but *always* populates the newer
// structured_diagnostics field so the DiagnosticsPanel can render with
// full file:line:column resolution.

#include <map>
#include <memory>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "core/asar_wrapper.h"

namespace z3dk {
struct AssembleResult;  // Complete in z3dk_core/assembler.h; only the
                        // wrapper .cc needs the definition.
}  // namespace z3dk

namespace yaze {
namespace core {

struct Z3dkMemoryRange {
  uint32_t start = 0;
  uint32_t end = 0;
  std::string reason;
};

struct Z3dkAssembleOptions {
  std::vector<std::string> include_paths;
  std::vector<std::pair<std::string, std::string>> defines;
  std::string std_includes_path;
  std::string std_defines_path;
  std::string mapper;
  int rom_size = 0;
  bool capture_nocash_symbols = false;
  std::vector<Z3dkMemoryRange> prohibited_memory_ranges;
  bool warn_unused_symbols = true;
  bool warn_branch_outside_bank = true;
  bool warn_unknown_width = true;
  bool warn_org_collision = true;
  bool warn_unauthorized_hook = true;
  bool warn_stack_balance = true;
  bool warn_hook_return = true;
  std::string hooks_rom_path;
};

class Z3dkWrapper {
 public:
  Z3dkWrapper();
  ~Z3dkWrapper();

  Z3dkWrapper(const Z3dkWrapper&) = delete;
  Z3dkWrapper& operator=(const Z3dkWrapper&) = delete;
  // Defined out-of-line because last_result_ holds a unique_ptr to a
  // forward-declared z3dk type.
  Z3dkWrapper(Z3dkWrapper&&) noexcept;
  Z3dkWrapper& operator=(Z3dkWrapper&&) noexcept;

  // Matches AsarWrapper::Initialize() shape; z3dk has no runtime init but
  // we keep the method so AssemblyEditor's call sites can stay
  // backend-agnostic.
  absl::Status Initialize();
  void Shutdown() { initialized_ = false; }
  bool IsInitialized() const { return initialized_; }

  std::string GetVersion() const;

  // Applies `patch_path` to the in-place `rom_data`. On assemble failure,
  // returns a StatusOr whose *value* still holds the diagnostics (so the
  // editor can paint markers even when the patch did not commit).
  absl::StatusOr<AsarPatchResult> ApplyPatch(
      const std::string& patch_path, std::vector<uint8_t>& rom_data,
      const std::vector<std::string>& include_paths = {});
  absl::StatusOr<AsarPatchResult> ApplyPatch(
      const std::string& patch_path, std::vector<uint8_t>& rom_data,
      const Z3dkAssembleOptions& options);

  // Assemble without mutating the ROM — the underlying assembler still
  // needs a writable buffer, so we use a scratch copy internally.
  absl::Status ValidateAssembly(
      const std::string& asm_path,
      const std::vector<std::string>& include_paths = {});
  absl::Status ValidateAssembly(const std::string& asm_path,
                                const Z3dkAssembleOptions& options);

  const std::map<std::string, AsarSymbol>& GetSymbolTable() const {
    return symbol_table_;
  }
  std::optional<AsarSymbol> FindSymbol(const std::string& name) const;

  std::vector<std::string> GetLastErrors() const { return last_errors_; }
  std::vector<std::string> GetLastWarnings() const { return last_warnings_; }

  // Re-runs z3dk::RunLint against the AssembleResult cached by the most
  // recent successful ApplyPatch, without re-assembling. Returns
  // FailedPreconditionError if no result is cached (fresh wrapper, failed
  // assemble, or post-Reset). Callers decide how to filter the returned
  // diagnostics by severity.
  absl::StatusOr<std::vector<AssemblyDiagnostic>> RunLintOnLastResult(
      const Z3dkAssembleOptions& options) const;

  void Reset();

 private:
  absl::StatusOr<AsarPatchResult> Assemble(const std::string& patch_path,
                                           std::vector<uint8_t>& rom_data,
                                           const Z3dkAssembleOptions& options,
                                           bool update_apply_cache);
  void ClearApplyCache();

  bool initialized_ = false;
  std::map<std::string, AsarSymbol> symbol_table_;
  std::vector<std::string> last_errors_;
  std::vector<std::string> last_warnings_;

  // Cached copy of the last successful ApplyPatch assemble output so
  // RunLintOnLastResult can re-run lint without re-assembling. Holds the
  // assembler's rom_data buffer (~4 MiB for LoROM); cleared by Reset().
  std::unique_ptr<::z3dk::AssembleResult> last_result_;
};

}  // namespace core
}  // namespace yaze

#endif  // YAZE_CORE_Z3DK_WRAPPER_H
