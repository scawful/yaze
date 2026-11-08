#include "cli/handlers/game/dungeon_render_commands.h"

#include <chrono>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <limits>
#include <string>
#include <utility>
#include <vector>

#include <gtest/gtest.h>

#include "absl/flags/declare.h"
#include "absl/flags/flag.h"
#include "app/service/render_service.h"
#include "cli/service/rom/rom_sandbox_manager.h"
#include "cli/service/usage_schema.h"
#include "core/project.h"
#include "unit/zelda3/gfx_sheet_test_rom.h"
#include "util/rom_hash.h"
#include "zelda3/dungeon/entrance_layout.h"
#include "zelda3/dungeon/room_entrance.h"

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
    auto decoy = logical / "source.sfc";
#ifdef _WIN32
    // Win32 normalizes '..' before following the directory symlink. Put the
    // active ROM at that native destination and the decoy at the target's
    // parent; both platforms must still reject an alias of the active ROM.
    ASSERT_TRUE(std::filesystem::copy_file(source_, decoy));
    std::swap(source_, decoy);
    rom_.set_filename(source_.string());
#endif
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
    // Pin the native filesystem identity before exercising command safety.
    ASSERT_TRUE(std::filesystem::equivalent(alias, source_));
#ifdef _WIN32
    ASSERT_EQ(alias.lexically_normal(), source_);
#else
    // POSIX follows the link first, so premature lexical cleanup reaches the
    // distinct decoy instead of the active ROM.
    ASSERT_EQ(alias.lexically_normal(), decoy);
#endif
    ASSERT_FALSE(std::filesystem::equivalent(source_, decoy));
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

TEST_F(DungeonRenderCommandsTest,
       ProjectGraphicsLayoutRejectsStaleRomIdentity) {
  project::YazeProject project;
  project.rom_metadata.expected_hash = std::string(40, '0');
  handlers::DungeonRenderCommandHandler handler;
  handler.SetProjectContext(&project);
  resources::ArgumentParser parser(
      {"--room=0", "--output", (directory_ / "room.png").string()});
  resources::OutputFormatter formatter(
      resources::OutputFormatter::Format::kJson);
  const auto status = handler.Execute(&rom_, parser, formatter);
  EXPECT_TRUE(absl::IsFailedPrecondition(status)) << status;
  EXPECT_NE(status.message().find("expected ROM identity"),
            absl::string_view::npos)
      << status;
  EXPECT_FALSE(std::filesystem::exists(directory_ / "room.png"));
  EXPECT_EQ(rom_.vector(), original_bytes_);
}

TEST_F(DungeonRenderCommandsTest, ProjectGraphicsLayoutRejectsConflictingRole) {
  project::YazeProject project;
  ASSERT_TRUE(project.hack_manifest
                  .LoadFromString(test::ExpandedGfxSheetTestManifest(rom_))
                  .ok());
  // The fixture declares development; a compiled project must not borrow it.
  project.rom_metadata.role = project::RomRole::kPatched;
  handlers::DungeonRenderCommandHandler handler;
  handler.SetProjectContext(&project);
  resources::ArgumentParser parser(
      {"--room=0", "--output", (directory_ / "room.png").string()});
  resources::OutputFormatter formatter(
      resources::OutputFormatter::Format::kJson);
  const auto status = handler.Execute(&rom_, parser, formatter);
  EXPECT_TRUE(absl::IsFailedPrecondition(status)) << status;
  EXPECT_NE(status.message().find("role conflicts"), absl::string_view::npos)
      << status;
  EXPECT_FALSE(std::filesystem::exists(directory_ / "room.png"));
  EXPECT_EQ(rom_.vector(), original_bytes_);
}

TEST_F(DungeonRenderCommandsTest,
       ProjectGraphicsLayoutValidatesExpandedSheetBeforeRendering) {
  auto fixture = test::BuildOracleGfxSheetTestRom();
  fixture.bytes.resize(0x200000);
  test::CorruptExpandedDfStream(fixture,
                                test::ExpandedDfStreamFailure::kMalformedCopy);
  ASSERT_TRUE(rom_.LoadFromData(fixture.bytes).ok());
  project::YazeProject project;
  std::string manifest = test::ExpandedGfxSheetTestManifest(rom_);
  manifest.replace(manifest.find("development"),
                   std::string("development").size(), "compiled");
  ASSERT_TRUE(project.hack_manifest.LoadFromString(manifest).ok());
  project.rom_metadata.role = project::RomRole::kPatched;
  project.rom_metadata.expected_hash =
      util::ComputeSha1Hex(rom_.data(), rom_.size());
  handlers::DungeonRenderCommandHandler handler;
  handler.SetProjectContext(&project);
  resources::ArgumentParser parser(
      {"--room=0", "--output", (directory_ / "room.png").string()});
  resources::OutputFormatter formatter(
      resources::OutputFormatter::Format::kJson);
  const auto status = handler.Execute(&rom_, parser, formatter);
  EXPECT_TRUE(absl::IsInternal(status)) << status;
  EXPECT_NE(status.message().find("Graphics sheet 0xDF"),
            absl::string_view::npos)
      << status;
  EXPECT_FALSE(std::filesystem::exists(directory_ / "room.png"));
  EXPECT_EQ(rom_.vector(), fixture.bytes);
  EXPECT_FALSE(rom_.dirty());
}

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

TEST(DungeonRenderEntranceTest, AcceptsOptionalRegularHexEntranceIds) {
  handlers::DungeonRenderCommandHandler handler;
  resources::ArgumentParser defaults({"--room=0", "--output=room.png"});
  EXPECT_TRUE(handler.ValidateArgs(defaults).ok());
  for (const char* value : {"0", "79", "0x79", "84", "0x84"}) {
    SCOPED_TRACE(value);
    resources::ArgumentParser parser(
        {"--room=0", "--output=room.png", "--entrance", value});
    EXPECT_TRUE(handler.ValidateArgs(parser).ok());
  }
}

TEST(DungeonRenderEntranceTest, InheritedGraphicsEntranceIsDiscoverable) {
  handlers::DungeonRenderCommandHandler handler;
  const auto schema =
      UsageParametersToJsonSchema(ParseUsageParameters(handler.GetUsage()));
  ASSERT_TRUE(schema["properties"].contains("graphics-entrance"));
  EXPECT_EQ(schema["properties"]["graphics-entrance"]["type"], "string");
  const auto descriptor = handler.Describe();
  ASSERT_EQ(descriptor.entries.size(), 2u);
  EXPECT_EQ(descriptor.entries[1].name, "graphics-entrance");
  EXPECT_NE(descriptor.entries[1].description.find("mutually exclusive"),
            std::string::npos);
  for (const char* id : {"0", "29", "0x29", "84"}) {
    resources::ArgumentParser parser(
        {"--room=0x95", "--output=room.png", "--graphics-entrance", id});
    EXPECT_TRUE(handler.ValidateArgs(parser).ok());
  }
}

TEST_F(DungeonRenderCommandsTest,
       InvalidGraphicsEntrancePreservesExistingOutput) {
  const auto output = directory_ / "output.png";
  const std::string sentinel = "existing image must survive";
  {
    std::ofstream out(output);
    out << sentinel;
  }
  handlers::DungeonRenderCommandHandler handler;
  for (const char* id :
       {"", "nope", "0x29junk", "-1", "0x100000000", "85", "FF"}) {
    SCOPED_TRACE(id);
    std::string formatted;
    const auto status = handler.Run(
        {"--room=0x95", "--output", output.string(), "--graphics-entrance", id},
        nullptr, &formatted);
    EXPECT_TRUE(absl::IsInvalidArgument(status) || absl::IsOutOfRange(status))
        << status;
  }
  for (const std::vector<std::string>& context :
       {std::vector<std::string>{"--graphics-entrance"},
        std::vector<std::string>{"--entrance=29", "--graphics-entrance=29"}}) {
    std::vector<std::string> args = {"--room=0x95", "--output",
                                     output.string()};
    args.insert(args.end(), context.begin(), context.end());
    std::string formatted;
    EXPECT_TRUE(
        absl::IsInvalidArgument(handler.Run(args, nullptr, &formatted)));
  }
  EXPECT_EQ(ReadBytes(output),
            (std::vector<uint8_t>(sentinel.begin(), sentinel.end())));
  EXPECT_EQ(ReadBytes(source_), original_bytes_);
}

TEST_F(DungeonRenderCommandsTest, ServiceRejectsBothEntranceContexts) {
  zelda3::GameData game_data;
  app::service::RenderService service(&rom_, &game_data);
  app::service::RenderRequest request;
  request.entrance_id = 0;
  request.graphics_entrance_id = 0;
  const auto result = service.RenderDungeonRoom(request);
  EXPECT_TRUE(absl::IsInvalidArgument(result.status())) << result.status();
  EXPECT_NE(result.status().message().find("mutually exclusive"),
            absl::string_view::npos);
  EXPECT_EQ(rom_.vector(), original_bytes_);
}

TEST_F(DungeonRenderCommandsTest, ServiceChecksInheritedEntranceRange) {
  zelda3::GameData game_data;
  app::service::RenderService service(&rom_, &game_data);
  for (int id : {-1, zelda3::kNumRegularDungeonEntrances, 0xFF}) {
    app::service::RenderRequest request;
    request.graphics_entrance_id = id;
    EXPECT_TRUE(
        absl::IsOutOfRange(service.RenderDungeonRoom(request).status()));
  }
  EXPECT_EQ(rom_.vector(), original_bytes_);
}

TEST_F(DungeonRenderCommandsTest,
       InheritedEntranceStillValidatesActualBlocksetOnAnotherRoom) {
  const auto layout = zelda3::ResolveEntranceLayout(rom_.vector());
  ASSERT_TRUE(layout.ok()) << layout.status();
  ASSERT_TRUE(
      rom_.WriteByte(layout->address(zelda3::EntranceField::kBlockset, 0), 0xFF)
          .ok());
  zelda3::GameData game_data;
  app::service::RenderService service(&rom_, &game_data);
  app::service::RenderRequest request;
  request.room_id = 1;  // Entrance0 actually leads into room0.
  request.graphics_entrance_id = 0;
  const auto before = rom_.vector();
  const auto result = service.RenderDungeonRoom(request);
  EXPECT_TRUE(absl::IsOutOfRange(result.status())) << result.status();
  EXPECT_NE(result.status().message().find("main blockset"),
            absl::string_view::npos);
  EXPECT_EQ(rom_.vector(), before);
}

TEST_F(DungeonRenderCommandsTest,
       InheritedEntranceRejectsMalformedExpandedLoader) {
  ASSERT_TRUE(rom_.WriteByte(0x7F000, 0).ok());
  ASSERT_TRUE(rom_.WriteByte(0x7F001, 1).ok());
  zelda3::GameData game_data;
  app::service::RenderService service(&rom_, &game_data);
  app::service::RenderRequest request;
  request.graphics_entrance_id = 0;
  const auto before = rom_.vector();
  const auto result = service.RenderDungeonRoom(request);
  EXPECT_TRUE(absl::IsFailedPrecondition(result.status())) << result.status();
  EXPECT_NE(result.status().message().find("expanded dungeon entrance layout"),
            absl::string_view::npos);
  EXPECT_EQ(rom_.vector(), before);
}

TEST_F(DungeonRenderCommandsTest,
       InvalidEntranceWithValidScalePreservesExistingOutput) {
  const auto output = directory_ / "output.png";
  const std::string sentinel = "existing image must survive";
  {
    std::ofstream out(output, std::ios::binary);
    out << sentinel;
    ASSERT_TRUE(out.good());
  }
  handlers::DungeonRenderCommandHandler handler;
  for (const char* value :
       {"", "nope", "0x79junk", "-1", "0x100000000", "85", "0xFF"}) {
    SCOPED_TRACE(value);
    std::string formatted;
    const auto status = handler.Run({"--room=0", "--output", output.string(),
                                     "--scale=1", "--entrance", value},
                                    nullptr, &formatted);
    EXPECT_TRUE(absl::IsInvalidArgument(status) || absl::IsOutOfRange(status))
        << status;
    EXPECT_EQ(ReadBytes(output),
              (std::vector<uint8_t>(sentinel.begin(), sentinel.end())));
  }
  std::string formatted;
  const auto missing =
      handler.Run({"--room=0", "--output", output.string(), "--entrance"},
                  nullptr, &formatted);
  EXPECT_TRUE(absl::IsInvalidArgument(missing)) << missing;
  EXPECT_EQ(ReadBytes(output),
            (std::vector<uint8_t>(sentinel.begin(), sentinel.end())));
  EXPECT_EQ(ReadBytes(source_), original_bytes_);
}

TEST_F(DungeonRenderCommandsTest, ServiceRejectsInvalidRegularEntranceIds) {
  zelda3::GameData game_data;
  app::service::RenderService service(&rom_, &game_data);
  const auto before = rom_.vector();
  for (int entrance_id : {-1, zelda3::kNumRegularDungeonEntrances, 0xFF}) {
    SCOPED_TRACE(entrance_id);
    app::service::RenderRequest request;
    request.entrance_id = entrance_id;
    EXPECT_TRUE(
        absl::IsOutOfRange(service.RenderDungeonRoom(request).status()));
  }
  EXPECT_EQ(rom_.vector(), before);
  EXPECT_EQ(ReadBytes(source_), original_bytes_);
}

TEST_F(DungeonRenderCommandsTest, ServiceRejectsEntranceRoomMismatch) {
  const zelda3::RoomEntrance entrance(&rom_, 0, false);
  ASSERT_TRUE(entrance.load_status().ok()) << entrance.load_status();
  zelda3::GameData game_data;
  app::service::RenderService service(&rom_, &game_data);
  const auto before = rom_.vector();
  app::service::RenderRequest request;
  request.room_id = entrance.room_ + 1;
  request.entrance_id = 0;
  const auto result = service.RenderDungeonRoom(request);
  EXPECT_TRUE(absl::IsInvalidArgument(result.status())) << result.status();
  EXPECT_NE(result.status().message().find("leads to room"),
            absl::string_view::npos);
  EXPECT_EQ(rom_.vector(), before);
  EXPECT_EQ(ReadBytes(source_), original_bytes_);
}

TEST_F(DungeonRenderCommandsTest, ServiceRejectsInvalidEntranceMainBlockset) {
  const auto layout = zelda3::ResolveEntranceLayout(rom_.vector());
  ASSERT_TRUE(layout.ok()) << layout.status();
  ASSERT_TRUE(
      rom_.WriteByte(layout->address(zelda3::EntranceField::kBlockset, 0), 0xFF)
          .ok());
  const zelda3::RoomEntrance entrance(&rom_, 0, false);
  ASSERT_TRUE(entrance.load_status().ok()) << entrance.load_status();
  ASSERT_EQ(entrance.blockset_, 0xFF);
  zelda3::GameData game_data;
  app::service::RenderService service(&rom_, &game_data);
  const auto before = rom_.vector();
  app::service::RenderRequest request;
  request.room_id = entrance.room_;
  request.entrance_id = 0;
  const auto result = service.RenderDungeonRoom(request);
  EXPECT_TRUE(absl::IsOutOfRange(result.status())) << result.status();
  EXPECT_NE(result.status().message().find("main blockset"),
            absl::string_view::npos);
  EXPECT_EQ(rom_.vector(), before);
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
