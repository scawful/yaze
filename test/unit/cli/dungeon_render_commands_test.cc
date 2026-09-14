#include "cli/handlers/game/dungeon_render_commands.h"

#include <chrono>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <limits>
#include <string>
#include <vector>

#include <gtest/gtest.h>

#include "absl/flags/declare.h"
#include "absl/flags/flag.h"
#include "app/service/render_service.h"
#include "cli/service/rom/rom_sandbox_manager.h"

ABSL_DECLARE_FLAG(bool, sandbox);

namespace yaze::cli {
namespace {

TEST(DungeonRenderScaleTest, RejectsMalformedNonFiniteAndOutOfRangeValues) {
  for (const char* value : {"", "nope", "1junk", "1 2", "nan", "NaN", "inf",
                            "-inf", "1e100", "0", "-1", "0.249", "8.001"}) {
    SCOPED_TRACE(value);
    EXPECT_TRUE(absl::IsInvalidArgument(
        app::service::ParseRenderScale(value).status()));
  }
}

TEST(DungeonRenderScaleTest, AcceptsBoundariesAndFractionalScales) {
  for (const char* value : {"0.25", "0.5", "1", "1.5", "8", "8e0"}) {
    SCOPED_TRACE(value);
    const auto scale = app::service::ParseRenderScale(value);
    ASSERT_TRUE(scale.ok()) << scale.status();
    EXPECT_FLOAT_EQ(*scale, std::stof(value));
  }
}

TEST(DungeonRenderServiceTest, RejectsUnsafeScalesBeforeRoomLoading) {
  app::service::RenderService service(nullptr, nullptr);
  for (float scale :
       {0.0f, -1.0f, 0.249f, 8.001f, std::numeric_limits<float>::denorm_min(),
        std::numeric_limits<float>::max(),
        std::numeric_limits<float>::infinity(),
        -std::numeric_limits<float>::infinity(),
        std::numeric_limits<float>::quiet_NaN()}) {
    SCOPED_TRACE(scale);
    app::service::RenderRequest request;
    request.scale = scale;
    EXPECT_TRUE(
        absl::IsInvalidArgument(service.RenderDungeonRoom(request).status()));
  }
}

TEST(DungeonRenderServiceTest, ValidScaleStillRequiresLoadedRom) {
  app::service::RenderService service(nullptr, nullptr);
  for (float scale : {0.25f, 1.0f, 8.0f}) {
    app::service::RenderRequest request;
    request.scale = scale;
    EXPECT_TRUE(absl::IsFailedPrecondition(
        service.RenderDungeonRoom(request).status()));
  }
}

#ifndef YAZE_CLI_HAS_PNG
TEST(DungeonRenderServiceTest, MissingPngEncoderNeverReturnsRawRgbaAsPng) {
  Rom rom;
  ASSERT_TRUE(rom.LoadFromData(std::vector<uint8_t>(0x200000, 0)).ok());
  zelda3::GameData game_data;
  app::service::RenderService service(&rom, &game_data);
  const auto result = service.RenderDungeonRoom({});
  EXPECT_TRUE(absl::IsUnimplemented(result.status()));
}
#endif

class DungeonRenderCommandsTest : public ::testing::Test {
 protected:
  void SetUp() override {
    previous_sandbox_ = absl::GetFlag(FLAGS_sandbox);
    absl::SetFlag(&FLAGS_sandbox, false);
    previous_sandbox_root_ = RomSandboxManager::Instance().RootDirectory();
    const auto nonce =
        std::chrono::steady_clock::now().time_since_epoch().count();
    directory_ = std::filesystem::temp_directory_path() /
                 ("yaze_render_safety_" + std::to_string(nonce));
    ASSERT_TRUE(std::filesystem::create_directories(directory_));
    RomSandboxManager::Instance().SetRootDirectory(directory_ / "sandboxes");

    ASSERT_TRUE(rom_.LoadFromData(std::vector<uint8_t>(0x200000, 0)).ok());
    source_ = directory_ / "source.sfc";
    std::ofstream out(source_, std::ios::binary);
    out.write(reinterpret_cast<const char*>(rom_.data()),
              static_cast<std::streamsize>(rom_.size()));
    out.close();
    ASSERT_TRUE(out.good());
    rom_.set_filename(source_.string());
    original_bytes_ = ReadBytes(source_);
  }

  void TearDown() override {
    for (const auto& sandbox : RomSandboxManager::Instance().ListSandboxes()) {
      if (sandbox.directory.parent_path() == directory_ / "sandboxes") {
        RomSandboxManager::Instance().RemoveSandbox(sandbox.id).IgnoreError();
      }
    }
    RomSandboxManager::Instance().SetRootDirectory(previous_sandbox_root_);
    absl::SetFlag(&FLAGS_sandbox, previous_sandbox_);
    std::error_code error;
    std::filesystem::remove_all(directory_, error);
  }

  static std::vector<uint8_t> ReadBytes(const std::filesystem::path& path) {
    std::ifstream in(path, std::ios::binary);
    return {std::istreambuf_iterator<char>(in),
            std::istreambuf_iterator<char>()};
  }

  void ExpectRejectedAlias(const std::filesystem::path& output) {
    handlers::DungeonRenderCommandHandler handler;
    std::string formatted;
    const auto status = handler.Run({"--room=0", "--output", output.string()},
                                    &rom_, &formatted);
    EXPECT_TRUE(absl::IsInvalidArgument(status)) << status;
    EXPECT_NE(status.message().find("aliases the active ROM"),
              absl::string_view::npos)
        << status;
    EXPECT_EQ(ReadBytes(source_), original_bytes_);
    EXPECT_EQ(ReadBytes(output), original_bytes_);
    EXPECT_EQ(rom_.vector(), original_bytes_);
  }

  void ExpectSymlinkParentAliasRejected(bool alias_is_source) {
    const auto logical = directory_ / "logical";
    const auto child = directory_ / "child";
    ASSERT_TRUE(std::filesystem::create_directory(logical));
    ASSERT_TRUE(std::filesystem::create_directory(child));
    const auto decoy = logical / "source.sfc";
    const std::string sentinel = "decoy file must survive";
    {
      std::ofstream out(decoy, std::ios::binary);
      out << sentinel;
      ASSERT_TRUE(out.good());
    }
    std::error_code error;
    std::filesystem::create_directory_symlink(child, logical / "link", error);
    if (error) {
      GTEST_SKIP() << "Filesystem cannot create directory symlink: "
                   << error.message();
    }
    const auto alias = logical / "link" / ".." / "source.sfc";
    // Filesystem resolution reaches the real source, while lexical cleanup
    // alone reaches the distinct decoy. Pin both facts before the command.
    ASSERT_TRUE(std::filesystem::equivalent(alias, source_));
    ASSERT_EQ(alias.lexically_normal(), decoy);
    ASSERT_EQ(ReadBytes(alias), original_bytes_);
    if (alias_is_source) {
      ASSERT_TRUE(rom_.LoadFromFile(alias.string()).ok());
      EXPECT_EQ(rom_.vector(), original_bytes_);
      ExpectRejectedAlias(source_);
    } else {
      ExpectRejectedAlias(alias);
    }
    EXPECT_EQ(ReadBytes(source_), original_bytes_);
    EXPECT_EQ(ReadBytes(alias), original_bytes_);
    EXPECT_EQ(ReadBytes(decoy),
              (std::vector<uint8_t>(sentinel.begin(), sentinel.end())));
    EXPECT_EQ(rom_.vector(), original_bytes_);
  }

  std::filesystem::path directory_;
  std::filesystem::path source_;
  std::filesystem::path previous_sandbox_root_;
  bool previous_sandbox_ = false;
  Rom rom_;
  std::vector<uint8_t> original_bytes_;
};

TEST_F(DungeonRenderCommandsTest, RejectsDirectAndNormalizedRomOutputAliases) {
  ExpectRejectedAlias(source_);
  ASSERT_TRUE(std::filesystem::create_directory(directory_ / "nested"));
  ExpectRejectedAlias(directory_ / "nested" / ".." / "source.sfc");
}

TEST_F(DungeonRenderCommandsTest, RejectsSymlinkRomOutputAlias) {
  const auto alias = directory_ / "symlink.png";
  std::error_code error;
  std::filesystem::create_symlink(source_, alias, error);
  if (error) {
    GTEST_SKIP() << "Filesystem cannot create test symlink: "
                 << error.message();
  }
  ExpectRejectedAlias(alias);
}

TEST_F(DungeonRenderCommandsTest, ResolvesSourceSymlinkBeforeParentTraversal) {
  ExpectSymlinkParentAliasRejected(/*alias_is_source=*/true);
}

TEST_F(DungeonRenderCommandsTest, ResolvesOutputSymlinkBeforeParentTraversal) {
  ExpectSymlinkParentAliasRejected(/*alias_is_source=*/false);
}

TEST_F(DungeonRenderCommandsTest, RejectsHardlinkRomOutputAlias) {
  const auto alias = directory_ / "hardlink.png";
  std::error_code error;
  std::filesystem::create_hard_link(source_, alias, error);
  if (error) {
    GTEST_SKIP() << "Filesystem cannot create test hardlink: "
                 << error.message();
  }
  ExpectRejectedAlias(alias);
}

TEST_F(DungeonRenderCommandsTest, DirectExecuteAlsoProtectsRomIdentity) {
  handlers::DungeonRenderCommandHandler handler;
  resources::ArgumentParser parser({"--room=0", "--output", source_.string()});
  resources::OutputFormatter formatter(
      resources::OutputFormatter::Format::kJson);
  const auto status = handler.Execute(&rom_, parser, formatter);
  EXPECT_TRUE(absl::IsInvalidArgument(status)) << status;
  EXPECT_EQ(ReadBytes(source_), original_bytes_);
}

TEST_F(DungeonRenderCommandsTest, SandboxCannotOverwriteOriginalSourceRom) {
  handlers::DungeonRenderCommandHandler handler;
  std::string formatted;
  const auto status =
      handler.Run({"--room=0", "--output", source_.string(), "--sandbox"},
                  &rom_, &formatted);
  EXPECT_TRUE(absl::IsInvalidArgument(status)) << status;
  EXPECT_NE(status.message().find("aliases the sandbox source ROM"),
            absl::string_view::npos)
      << status;
  EXPECT_EQ(ReadBytes(source_), original_bytes_);
  EXPECT_EQ(rom_.vector(), original_bytes_);
  const auto sandbox = RomSandboxManager::Instance().ActiveSandbox();
  ASSERT_TRUE(sandbox.ok()) << sandbox.status();
  EXPECT_EQ(ReadBytes(sandbox->rom_path), original_bytes_);
}

TEST_F(DungeonRenderCommandsTest, MalformedScaleDoesNotTouchExistingOutput) {
  const auto output = directory_ / "output.png";
  const std::string sentinel = "existing image must survive";
  {
    std::ofstream out(output, std::ios::binary);
    out << sentinel;
  }
  handlers::DungeonRenderCommandHandler handler;
  for (const char* scale : {"nan", "inf", "1junk", "0", "8.1"}) {
    SCOPED_TRACE(scale);
    std::string formatted;
    const auto status =
        handler.Run({"--room=0", "--output", output.string(), "--scale", scale},
                    nullptr, &formatted);
    EXPECT_TRUE(absl::IsInvalidArgument(status)) << status;
    EXPECT_EQ(ReadBytes(output),
              (std::vector<uint8_t>(sentinel.begin(), sentinel.end())));
  }
  EXPECT_EQ(ReadBytes(source_), original_bytes_);
}

TEST_F(DungeonRenderCommandsTest, SharedPathGuardAllowsSeparateOutput) {
  const auto output = directory_ / "room.png";
  resources::CommandInvocationContext context;
  context.source_rom_path = source_;
  context.active_rom_path = source_;
  const auto resolved = resources::ResolveStableArtifactPath(output);
  ASSERT_TRUE(resolved.ok()) << resolved.status();
  EXPECT_TRUE(
      resources::RejectArtifactRomAliases("--output", *resolved, context).ok());
  EXPECT_FALSE(std::filesystem::exists(output));
  EXPECT_EQ(ReadBytes(source_), original_bytes_);
}

}  // namespace
}  // namespace yaze::cli
