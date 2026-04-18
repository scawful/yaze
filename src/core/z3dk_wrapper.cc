#include "core/z3dk_wrapper.h"

#include <algorithm>
#include <cstdint>
#include <filesystem>
#include <string>
#include <vector>

#include "absl/status/status.h"
#include "absl/strings/str_cat.h"
#include "util/json.h"
#if __has_include("z3dk_core/assembler.h")
#include "z3dk_core/assembler.h"
#include "z3dk_core/emit.h"
#include "z3dk_core/lint.h"
#else
namespace z3dk {
enum class DiagnosticSeverity { kError, kWarning };
struct Diagnostic {
  DiagnosticSeverity severity = DiagnosticSeverity::kError;
  std::string message;
  std::string filename;
  int line = 0;
  int column = 0;
  std::string raw;
};
struct Label {
  std::string name;
  uint32_t address = 0;
  bool used = false;
};
struct Define {
  std::string name;
  std::string value;
};
struct WrittenBlock {
  int pc_offset = 0;
  int snes_offset = 0;
  int num_bytes = 0;
};
struct SourceFile {
  int id = 0;
  uint32_t crc = 0;
  std::string path;
};
struct SourceMapEntry {
  uint32_t address = 0;
  int file_id = 0;
  int line = 0;
};
struct SourceMap {
  std::vector<SourceFile> files;
  std::vector<SourceMapEntry> entries;
};
struct AssembleOptions {
  std::string patch_path;
  std::vector<uint8_t> rom_data;
  std::vector<std::string> include_paths;
  std::vector<std::pair<std::string, std::string>> defines;
  std::string std_includes_path;
  std::string std_defines_path;
  bool capture_nocash_symbols = false;
};
struct AssembleResult {
  bool success = false;
  std::vector<Diagnostic> diagnostics;
  std::vector<Label> labels;
  std::vector<Define> defines;
  std::vector<WrittenBlock> written_blocks;
  std::vector<uint8_t> rom_data;
  int rom_size = 0;
  SourceMap source_map;
};
class Assembler {
 public:
  AssembleResult Assemble(const AssembleOptions& options) const;
};
struct MemoryRange {
  uint32_t start = 0;
  uint32_t end = 0;
  std::string reason;
};
struct Hook {
  std::string name;
  uint32_t address = 0;
  int size = 0;
};
struct LintOptions {
  bool warn_unknown_width = true;
  bool warn_branch_outside_bank = true;
  bool warn_org_collision = true;
  bool warn_unused_symbols = true;
  bool warn_unauthorized_hook = true;
  bool warn_stack_balance = true;
  bool warn_hook_return = true;
  std::vector<MemoryRange> prohibited_memory_ranges;
};
struct LintResult {
  std::vector<Diagnostic> diagnostics;
  bool success() const { return true; }
};
LintResult RunLint(const AssembleResult& result, const LintOptions& options);
std::string DiagnosticsListToJson(const std::vector<Diagnostic>& diagnostics,
                                  bool success);
std::string HooksToJson(const AssembleResult& result,
                        const std::string& rom_path);
std::string AnnotationsToJson(const AssembleResult& result);
std::string SourceMapToJson(const SourceMap& map);
std::string SymbolsToMlb(const std::vector<Label>& labels);
}  // namespace z3dk
#endif

namespace yaze {
namespace core {

namespace {

constexpr std::size_t kMinLoromScratchBytes = 0x80000;  // 512 KiB

AssemblyDiagnosticSeverity ConvertSeverity(z3dk::DiagnosticSeverity s) {
  switch (s) {
    case z3dk::DiagnosticSeverity::kWarning:
      return AssemblyDiagnosticSeverity::kWarning;
    case z3dk::DiagnosticSeverity::kError:
    default:
      return AssemblyDiagnosticSeverity::kError;
  }
}

AssemblyDiagnostic ConvertDiagnostic(const z3dk::Diagnostic& d) {
  AssemblyDiagnostic diag;
  diag.severity = ConvertSeverity(d.severity);
  diag.message = d.message;
  diag.file = d.filename;
  diag.line = d.line;
  diag.column = d.column;
  diag.raw = d.raw;
  return diag;
}

void AppendStructuredDiagnostic(const AssemblyDiagnostic& diag,
                                AsarPatchResult* out) {
  out->structured_diagnostics.push_back(diag);
  if (diag.severity == AssemblyDiagnosticSeverity::kWarning) {
    out->warnings.push_back(
        diag.file.empty()
            ? diag.message
            : absl::StrCat(diag.file, ":", diag.line, ": ", diag.message));
  } else if (diag.severity == AssemblyDiagnosticSeverity::kError) {
    out->errors.push_back(
        diag.file.empty()
            ? diag.message
            : absl::StrCat(diag.file, ":", diag.line, ": ", diag.message));
  }
}

std::vector<AssemblyDiagnostic> BuildAnnotationNotes(
    const std::string& annotations_json) {
  std::vector<AssemblyDiagnostic> notes;
  if (annotations_json.empty()) {
    return notes;
  }

  Json root;
  try {
    root = Json::parse(annotations_json);
  } catch (...) {
    return notes;
  }

  if (!root.contains("annotations") || !root["annotations"].is_array()) {
    return notes;
  }

  for (const auto& item : root["annotations"]) {
    AssemblyDiagnostic diag;
    diag.severity = AssemblyDiagnosticSeverity::kNote;
    const std::string type = item.value("type", "annotation");
    const std::string label = item.value("label", "");
    const std::string expr = item.value("expr", "");
    const std::string note = item.value("note", "");
    if (!label.empty()) {
      diag.message = absl::StrCat("@", type, " ", label);
    } else if (!expr.empty()) {
      diag.message = absl::StrCat("@", type, " ", expr);
    } else if (!note.empty()) {
      diag.message = absl::StrCat("@", type, " ", note);
    } else {
      diag.message = absl::StrCat("@", type);
    }

    const std::string source = item.value("source", "");
    if (!source.empty()) {
      const size_t colon = source.find_last_of(':');
      if (colon != std::string::npos) {
        diag.file = source.substr(0, colon);
        try {
          diag.line = std::stoi(source.substr(colon + 1));
        } catch (...) {
          diag.file = source;
          diag.line = 0;
        }
      } else {
        diag.file = source;
      }
    }
    notes.push_back(std::move(diag));
  }
  return notes;
}

z3dk::LintOptions BuildLintOptions(const Z3dkAssembleOptions& options) {
  z3dk::LintOptions lint_options;
  lint_options.warn_unused_symbols = options.warn_unused_symbols;
  lint_options.warn_branch_outside_bank = options.warn_branch_outside_bank;
  lint_options.warn_unknown_width = options.warn_unknown_width;
  lint_options.warn_org_collision = options.warn_org_collision;
  lint_options.warn_unauthorized_hook = options.warn_unauthorized_hook;
  lint_options.warn_stack_balance = options.warn_stack_balance;
  lint_options.warn_hook_return = options.warn_hook_return;
  for (const auto& range : options.prohibited_memory_ranges) {
    lint_options.prohibited_memory_ranges.push_back(
        {.start = range.start, .end = range.end, .reason = range.reason});
  }
  return lint_options;
}

void RebuildLegacyDiagnosticCaches(const AsarPatchResult& result,
                                   std::vector<std::string>* errors,
                                   std::vector<std::string>* warnings) {
  errors->clear();
  warnings->clear();
  for (const auto& d : result.structured_diagnostics) {
    if (d.severity == AssemblyDiagnosticSeverity::kNote) {
      continue;
    }
    std::string flat = d.file.empty()
                           ? d.message
                           : absl::StrCat(d.file, ":", d.line, ": ", d.message);
    if (d.severity == AssemblyDiagnosticSeverity::kWarning) {
      warnings->push_back(std::move(flat));
    } else {
      errors->push_back(std::move(flat));
    }
  }
}

// Convert z3dk's AssembleResult into the editor-shaped AsarPatchResult.
// We always populate structured_diagnostics; the flat errors/warnings
// vectors are kept in sync for legacy consumers.
AsarPatchResult ConvertResult(const z3dk::AssembleResult& src) {
  AsarPatchResult out;
  out.success = src.success;
  out.rom_size = static_cast<uint32_t>(src.rom_size);
  out.crc32 = 0;

  out.structured_diagnostics.reserve(src.diagnostics.size());
  for (const auto& d : src.diagnostics) {
    AppendStructuredDiagnostic(ConvertDiagnostic(d), &out);
  }

  out.symbols.reserve(src.labels.size());
  for (const auto& label : src.labels) {
    AsarSymbol sym;
    sym.name = label.name;
    sym.address = label.address;
    sym.line = 0;
    out.symbols.push_back(std::move(sym));
  }

  return out;
}

}  // namespace

// Defined out-of-line because last_result_ holds a unique_ptr to a
// forward-declared z3dk type; the destructor must see the complete type.
Z3dkWrapper::Z3dkWrapper() = default;
Z3dkWrapper::~Z3dkWrapper() = default;
Z3dkWrapper::Z3dkWrapper(Z3dkWrapper&&) noexcept = default;
Z3dkWrapper& Z3dkWrapper::operator=(Z3dkWrapper&&) noexcept = default;

absl::Status Z3dkWrapper::Initialize() {
  initialized_ = true;
  return absl::OkStatus();
}

std::string Z3dkWrapper::GetVersion() const {
  // z3dk-core pins a specific Asar fork; version string is informational.
  return "z3dk-core (embedded Asar fork)";
}

absl::StatusOr<AsarPatchResult> Z3dkWrapper::ApplyPatch(
    const std::string& patch_path, std::vector<uint8_t>& rom_data,
    const std::vector<std::string>& include_paths) {
  Z3dkAssembleOptions options;
  options.include_paths = include_paths;
  return ApplyPatch(patch_path, rom_data, options);
}

absl::StatusOr<AsarPatchResult> Z3dkWrapper::ApplyPatch(
    const std::string& patch_path, std::vector<uint8_t>& rom_data,
    const Z3dkAssembleOptions& options) {
  return Assemble(patch_path, rom_data, options, /*update_apply_cache=*/true);
}

absl::StatusOr<AsarPatchResult> Z3dkWrapper::Assemble(
    const std::string& patch_path, std::vector<uint8_t>& rom_data,
    const Z3dkAssembleOptions& options, bool update_apply_cache) {
  if (!initialized_) {
    auto st = Initialize();
    if (!st.ok()) {
      return st;
    }
  }

  z3dk::AssembleOptions opts;
  opts.patch_path = patch_path;
  opts.include_paths = options.include_paths;
  opts.defines = options.defines;
  if (!options.mapper.empty()) {
    const auto mapper_it = std::find_if(
        opts.defines.begin(), opts.defines.end(),
        [](const auto& define) { return define.first == "z3dk_mapper"; });
    if (mapper_it == opts.defines.end()) {
      opts.defines.emplace_back("z3dk_mapper", options.mapper);
    }
  }
  opts.std_includes_path = options.std_includes_path;
  opts.std_defines_path = options.std_defines_path;
  opts.capture_nocash_symbols = options.capture_nocash_symbols;
  opts.rom_data = rom_data;  // Copy in; z3dk resizes/edits its own buffer.
  if (options.rom_size > 0 &&
      opts.rom_data.size() < static_cast<std::size_t>(options.rom_size)) {
    opts.rom_data.resize(static_cast<std::size_t>(options.rom_size), 0);
  }
  if (opts.rom_data.size() < kMinLoromScratchBytes) {
    opts.rom_data.resize(kMinLoromScratchBytes, 0);
  }

  z3dk::Assembler assembler;
  auto z_result = assembler.Assemble(opts);
  auto result = ConvertResult(z_result);

  if (z_result.success) {
    z3dk::LintResult lint_result =
        z3dk::RunLint(z_result, BuildLintOptions(options));
    for (const auto& diag : lint_result.diagnostics) {
      AppendStructuredDiagnostic(ConvertDiagnostic(diag), &result);
    }

    result.symbols_mlb = z3dk::SymbolsToMlb(z_result.labels);
    result.sourcemap_json = z3dk::SourceMapToJson(z_result.source_map);
    result.annotations_json = z3dk::AnnotationsToJson(z_result);
    result.hooks_json = z3dk::HooksToJson(z_result, options.hooks_rom_path);
    result.lint_json = z3dk::DiagnosticsListToJson(lint_result.diagnostics,
                                                   lint_result.success());

    const auto annotation_notes = BuildAnnotationNotes(result.annotations_json);
    for (const auto& diag : annotation_notes) {
      result.structured_diagnostics.push_back(diag);
    }

    if (!lint_result.success()) {
      result.success = false;
    }
  }

  RebuildLegacyDiagnosticCaches(result, &last_errors_, &last_warnings_);

  if (!update_apply_cache) {
    return result;
  }

  if (result.success) {
    // Cache the full assemble result so RunLintOnLastResult can re-run lint
    // without re-assembling. rom_data for the caller is taken as a copy from
    // the cache — the ~4 MiB memcpy is negligible next to assemble time.
    last_result_ = std::make_unique<z3dk::AssembleResult>(std::move(z_result));
    rom_data = last_result_->rom_data;
    result.rom_size = static_cast<uint32_t>(rom_data.size());
    // Rebuild symbol table from labels.
    symbol_table_.clear();
    for (const auto& s : result.symbols) {
      symbol_table_[s.name] = s;
    }
  } else {
    ClearApplyCache();
  }

  return result;
}

absl::Status Z3dkWrapper::ValidateAssembly(
    const std::string& asm_path,
    const std::vector<std::string>& include_paths) {
  Z3dkAssembleOptions options;
  options.include_paths = include_paths;
  return ValidateAssembly(asm_path, options);
}

absl::Status Z3dkWrapper::ValidateAssembly(const std::string& asm_path,
                                           const Z3dkAssembleOptions& options) {
  if (!std::filesystem::exists(asm_path)) {
    return absl::NotFoundError(
        absl::StrCat("Assembly file not found: ", asm_path));
  }
  std::vector<uint8_t> scratch;
  auto result_or =
      Assemble(asm_path, scratch, options, /*update_apply_cache=*/false);
  if (!result_or.ok()) {
    return result_or.status();
  }
  if (!result_or->success) {
    return absl::InternalError("Assembly validation failed");
  }
  return absl::OkStatus();
}

std::optional<AsarSymbol> Z3dkWrapper::FindSymbol(
    const std::string& name) const {
  auto it = symbol_table_.find(name);
  if (it == symbol_table_.end())
    return std::nullopt;
  return it->second;
}

absl::StatusOr<std::vector<AssemblyDiagnostic>>
Z3dkWrapper::RunLintOnLastResult(const Z3dkAssembleOptions& options) const {
  if (!last_result_) {
    return absl::FailedPreconditionError(
        "No assemble result cached; call ApplyPatch first.");
  }
  z3dk::LintResult lint =
      z3dk::RunLint(*last_result_, BuildLintOptions(options));
  std::vector<AssemblyDiagnostic> out;
  out.reserve(lint.diagnostics.size());
  for (const auto& d : lint.diagnostics) {
    out.push_back(ConvertDiagnostic(d));
  }
  return out;
}

void Z3dkWrapper::ClearApplyCache() {
  symbol_table_.clear();
  last_result_.reset();
}

void Z3dkWrapper::Reset() {
  ClearApplyCache();
  last_errors_.clear();
  last_warnings_.clear();
}

}  // namespace core
}  // namespace yaze
