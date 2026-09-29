// Every path that applies TailMapExpansion.asm refuses when the
// overworld-allocation tail check says unsafe.

#include <gtest/gtest.h>

#include <filesystem>
#include <fstream>
#include <string>
#include <vector>

#include "absl/status/status.h"
#include "cli/handlers/tools/overworld_doctor_commands.h"
#include "core/patch/patch_manager.h"
#include "rom/rom.h"
#include "unique_temp_path.h"
#include "unit/zelda3/overworld_allocation_test_rom.h"

namespace yaze {
namespace {

namespace fs = std::filesystem;

class TempPatchDir {
 public:
  TempPatchDir() : root_(test::UniqueTempPath("tail_guard_patches")) {
    fs::create_directories(root_ / "Overworld");
    fs::create_directories(root_ / "Misc");
    std::ofstream(root_ / "Overworld" / "TailMapExpansion.asm")
        << ";#ENABLED=false\n;#PATCH_NAME=Overworld Tail Map Expansion\n"
           "lorom\norg $28A3FF\ndb $EA\n";
    std::ofstream(root_ / "Misc" / "Harmless.asm")
        << ";#ENABLED=false\n;#PATCH_NAME=Harmless\nlorom\n";
  }
  ~TempPatchDir() {
    std::error_code ec;
    fs::remove_all(root_, ec);
  }
  std::string path() const { return root_.string(); }

 private:
  fs::path root_;
};

Rom LoadRom(const std::vector<uint8_t>& bytes) {
  Rom rom;
  EXPECT_TRUE(rom.LoadFromData(bytes).ok());
  return rom;
}

TEST(TailExpansionGuardTest, PatchManagerRefusesTheTailPatch) {
  TempPatchDir dir;
  core::PatchManager manager;
  ASSERT_TRUE(manager.LoadPatches(dir.path()).ok());
  auto* tail = manager.GetPatch("Overworld", "TailMapExpansion.asm");
  ASSERT_NE(tail, nullptr);
  tail->set_enabled(true);

  Rom rom = LoadRom(test::BuildOverworldAllocationTestRom(/*zs_v3=*/true));
  const absl::Status status = manager.ApplyEnabledPatches(&rom);
  ASSERT_TRUE(absl::IsFailedPrecondition(status)) << status;
  EXPECT_NE(std::string(status.message()).find("Refusing"), std::string::npos);
  EXPECT_EQ(rom.data()[0x1423FF], 0x00);
}

TEST(TailExpansionGuardTest, PatchManagerLeavesOtherPatchesAlone) {
  TempPatchDir dir;
  core::PatchManager manager;
  ASSERT_TRUE(manager.LoadPatches(dir.path()).ok());
  auto* harmless = manager.GetPatch("Misc", "Harmless.asm");
  ASSERT_NE(harmless, nullptr);
  harmless->set_enabled(true);

  Rom rom = LoadRom(test::BuildOverworldAllocationTestRom(/*zs_v3=*/true));
  const absl::Status status = manager.ApplyEnabledPatches(&rom);
  // Without an assembler backend this fails for other reasons; it must not
  // be the tail refusal.
  EXPECT_EQ(std::string(status.message()).find("TailMapExpansion"),
            std::string::npos)
      << status;
}

TEST(TailExpansionGuardTest, OverworldDoctorRefusesTheTailPatch) {
  Rom rom = LoadRom(test::BuildOverworldAllocationTestRom(/*zs_v3=*/true));
  cli::OverworldDoctorCommandHandler handler;
  std::string out;
  const absl::Status status =
      handler.Run({"--format=json", "--apply-tail-expansion"}, &rom, &out);
  ASSERT_TRUE(absl::IsFailedPrecondition(status)) << status << "\n" << out;
  EXPECT_NE(std::string(status.message()).find("Refusing"), std::string::npos);
  EXPECT_EQ(rom.data()[0x1423FF], 0x00);
  EXPECT_NE(out.find("tail_expansion_error"), std::string::npos) << out;
}

}  // namespace
}  // namespace yaze
