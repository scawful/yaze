// z3dk-core link-time smoke test.
//
// Only built when YAZE_ENABLE_Z3DK=ON (registered conditionally in
// test/CMakeLists.txt). Confirms the library is linked correctly and
// Assembler::Assemble produces a usable result for a trivial in-memory
// fixture — no patch file on disk, no ROM load, no lint pass.

#include "z3dk_core/assembler.h"

#include <gtest/gtest.h>

#include <cstddef>
#include <cstdint>
#include <string>

namespace yaze_test {
namespace {

TEST(Z3dkSmoke, AssemblesTrivialPatchAndReportsLabel) {
  z3dk::AssembleOptions opts;
  opts.patch_path = "smoke.asm";

  z3dk::MemoryFile file;
  file.path = "smoke.asm";
  file.contents =
      "lorom\n"
      "org $008000\n"
      "Start:\n"
      "    LDA #$01\n"
      "    RTS\n";
  opts.memory_files.push_back(std::move(file));

  // Minimum LoROM sized scratch buffer; Assembler resizes as needed.
  opts.rom_data.assign(static_cast<std::size_t>(0x80000), std::uint8_t{0});

  z3dk::Assembler assembler;
  const auto result = assembler.Assemble(opts);

  ASSERT_TRUE(result.success) << [&] {
    std::string joined;
    for (const auto& diag : result.diagnostics) {
      joined += diag.message;
      joined += "; ";
    }
    return joined;
  }();

  bool saw_start_label = false;
  for (const auto& label : result.labels) {
    if (label.name == "Start") {
      saw_start_label = true;
      EXPECT_EQ(label.address & 0xFFFFFFu, 0x008000u);
      break;
    }
  }
  EXPECT_TRUE(saw_start_label) << "Expected 'Start' label in assemble result";
}

}  // namespace
}  // namespace yaze_test
