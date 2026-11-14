#include "cli/handlers/tools/test_helpers_commands.h"

#include <chrono>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <string>
#include <vector>

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include "rom/rom.h"
#include "zelda3/common.h"

namespace yaze::cli::handlers {
namespace {

namespace fs = std::filesystem;
using ::testing::HasSubstr;

class ScopedTempDir {
 public:
  ScopedTempDir() {
    const auto nonce =
        std::chrono::steady_clock::now().time_since_epoch().count();
    path_ =
        fs::temp_directory_path() / ("yaze_tools_cli_" + std::to_string(nonce));
    fs::create_directories(path_);
  }

  ~ScopedTempDir() {
    std::error_code ec;
    fs::remove_all(path_, ec);
  }

  const fs::path& path() const { return path_; }

 private:
  fs::path path_;
};

std::vector<uint8_t> ReadBinaryFile(const fs::path& path) {
  std::ifstream input(path, std::ios::binary);
  return {std::istreambuf_iterator<char>(input),
          std::istreambuf_iterator<char>()};
}

void WriteBinaryFile(const fs::path& path, const std::vector<uint8_t>& data) {
  std::ofstream output(path, std::ios::binary | std::ios::trunc);
  ASSERT_TRUE(output.is_open());
  output.write(reinterpret_cast<const char*>(data.data()),
               static_cast<std::streamsize>(data.size()));
}

TEST(ToolsTestHelpersCommandsTest, ExtractValuesUsesCommandRomWithoutRomArg) {
  Rom rom;
  ASSERT_TRUE(rom.LoadFromData(std::vector<uint8_t>(0x100000, 0)).ok());

  ToolsExtractValuesCommandHandler handler;
  std::string output;
  ::testing::internal::CaptureStdout();
  const auto status = handler.Run({"--format=json"}, &rom, &output);
  const std::string values = ::testing::internal::GetCapturedStdout();

  ASSERT_TRUE(status.ok()) << status;
  EXPECT_THAT(values, HasSubstr("\"asm_version\": \"0xff\""));
}

TEST(ToolsTestHelpersCommandsTest, FileToolsRequireCommandRomFile) {
  Rom rom;
  ASSERT_TRUE(rom.LoadFromData(std::vector<uint8_t>(0x200000, 0)).ok());
  ScopedTempDir temp;
  const std::string output_arg =
      "--output=" + (temp.path() / "out.sfc").string();

  ToolsPatchV3CommandHandler handler;
  std::string output;
  const auto status = handler.Run({output_arg}, &rom, &output);

  EXPECT_EQ(status.code(), absl::StatusCode::kFailedPrecondition) << status;
  EXPECT_THAT(std::string(status.message()), HasSubstr("--rom"));
  EXPECT_FALSE(fs::exists(temp.path() / "out.sfc"));
}

TEST(ToolsTestHelpersCommandsTest, PatchV3ReopensCommandRomFile) {
  ScopedTempDir temp;
  const fs::path input = temp.path() / "expanded.sfc";
  const fs::path patched = temp.path() / "patched.sfc";
  const std::vector<uint8_t> original(0x200000, 0);
  WriteBinaryFile(input, original);
  Rom rom;
  ASSERT_TRUE(rom.LoadFromFile(input.string()).ok());

  ToolsPatchV3CommandHandler handler;
  std::string output;
  const auto status =
      handler.Run({"--output=" + patched.string()}, &rom, &output);

  ASSERT_TRUE(status.ok()) << status << "\n" << output;
  const auto patched_bytes = ReadBinaryFile(patched);
  ASSERT_GT(patched_bytes.size(),
            static_cast<size_t>(zelda3::OverworldCustomASMHasBeenApplied));
  EXPECT_EQ(patched_bytes[zelda3::OverworldCustomASMHasBeenApplied], 0x03);
  EXPECT_EQ(ReadBinaryFile(input), original);
}

TEST(ToolsTestHelpersCommandsTest, PatchV3RefusesUnexpandedRom) {
  ScopedTempDir temp;
  const fs::path input = temp.path() / "retail.sfc";
  const fs::path patched = temp.path() / "patched.sfc";
  WriteBinaryFile(input, std::vector<uint8_t>(0x100000, 0));
  Rom rom;
  ASSERT_TRUE(rom.LoadFromFile(input.string()).ok());

  ToolsPatchV3CommandHandler handler;
  std::string output;
  const auto status =
      handler.Run({"--output=" + patched.string()}, &rom, &output);

  EXPECT_FALSE(status.ok());
  EXPECT_FALSE(fs::exists(patched));
}

}  // namespace
}  // namespace yaze::cli::handlers
