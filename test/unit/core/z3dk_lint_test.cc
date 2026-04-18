// Regression coverage for the z3dk prohibited-memory-range lint rule.
//
// Only built when YAZE_ENABLE_Z3DK=ON (registered conditionally in
// test/CMakeLists.txt next to z3dk_smoke_test.cc). The positive case
// confirms z3dk::RunLint emits an error when a written block overlaps a
// configured prohibited range. The negative case confirms the same
// assemble produces no prohibited-range diagnostic when no ranges are
// configured — guarding against a future change that would emit
// prohibited diagnostics unconditionally.

#include "z3dk_core/assembler.h"
#include "z3dk_core/lint.h"

#include <gtest/gtest.h>

#include <cstddef>
#include <cstdint>
#include <string>
#include <string_view>

namespace yaze_test {
namespace {

z3dk::AssembleResult AssembleInMemory(std::string_view source) {
  z3dk::AssembleOptions opts;
  opts.patch_path = "lint_fixture.asm";
  z3dk::MemoryFile file;
  file.path = "lint_fixture.asm";
  file.contents = std::string(source);
  opts.memory_files.push_back(std::move(file));
  opts.rom_data.assign(static_cast<std::size_t>(0x80000), std::uint8_t{0});
  z3dk::Assembler assembler;
  return assembler.Assemble(opts);
}

constexpr std::string_view kBankZeroWriter =
    "lorom\n"
    "org $008000\n"
    "Start:\n"
    "    LDA #$42\n"
    "    RTS\n";

TEST(Z3dkLint, ProhibitedRangeWriteProducesError) {
  const auto result = AssembleInMemory(kBankZeroWriter);
  ASSERT_TRUE(result.success) << [&] {
    std::string joined;
    for (const auto& diag : result.diagnostics) {
      joined += diag.message;
      joined += "; ";
    }
    return joined;
  }();

  z3dk::LintOptions lint_options;
  // z3dk reports written_block.snes_offset in the FastROM mirror form
  // ($80XXXX-$FFXXXX), not the SlowROM form ($00XXXX-$7FXXXX), so a LoROM
  // `org $008000` patch shows up as `$808000`. The prohibited range must
  // use the same mirror. End is exclusive (lint.cc:218 overlap check).
  lint_options.prohibited_memory_ranges.push_back(
      {.start = 0x808000u, .end = 0x810000u, .reason = "test fixture"});
  // Don't let the unauthorized-hook warning (no hooks.json fixture) spam
  // the diagnostics and mask regressions in future prohibited-range tests.
  lint_options.warn_unauthorized_hook = false;
  lint_options.warn_unused_symbols = false;

  const auto lint = z3dk::RunLint(result, lint_options);

  auto dump_diagnostics = [&] {
    std::string joined;
    for (const auto& d : lint.diagnostics) {
      joined += (d.severity == z3dk::DiagnosticSeverity::kError ? "[E] "
                                                                 : "[W] ");
      joined += d.message;
      joined += "\n";
    }
    return joined;
  };

  bool saw_prohibited_error = false;
  for (const auto& diag : lint.diagnostics) {
    if (diag.severity == z3dk::DiagnosticSeverity::kError &&
        diag.message.find("prohibited") != std::string::npos) {
      saw_prohibited_error = true;
      EXPECT_NE(diag.message.find("test fixture"), std::string::npos)
          << "Diagnostic should echo the reason string";
    }
  }
  EXPECT_TRUE(saw_prohibited_error)
      << "Expected a prohibited-range error for a write inside $008000-$00FFFF"
      << "\nDiagnostics:\n"
      << dump_diagnostics();
  EXPECT_FALSE(lint.success())
      << "LintResult.success() should be false when an error diagnostic is "
         "present\nDiagnostics:\n"
      << dump_diagnostics();
}

TEST(Z3dkLint, EmptyRangesNoProhibitedDiagnostic) {
  const auto result = AssembleInMemory(kBankZeroWriter);
  ASSERT_TRUE(result.success);

  z3dk::LintOptions lint_options;  // No prohibited_memory_ranges configured.
  const auto lint = z3dk::RunLint(result, lint_options);

  for (const auto& diag : lint.diagnostics) {
    EXPECT_EQ(diag.message.find("prohibited"), std::string::npos)
        << "Unexpected prohibited-range diagnostic with empty ranges: "
        << diag.message;
  }
}

}  // namespace
}  // namespace yaze_test
