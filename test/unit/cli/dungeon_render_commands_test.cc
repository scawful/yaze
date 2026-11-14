#include "cli/handlers/game/dungeon_render_commands.h"
#include "cli/handlers/game/dungeon_edit_commands.h"
#include "cli/handlers/game/dungeon_render_object_plan.h"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <limits>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include <gtest/gtest.h>

#include "absl/flags/declare.h"
#include "absl/flags/flag.h"
#include "absl/strings/str_format.h"
#include "app/service/render_service.h"
#include "cli/handlers/command_handlers.h"
#include "cli/service/rom/rom_sandbox_manager.h"
#include "cli/service/usage_schema.h"
#include "core/hack_manifest.h"
#include "core/project.h"
#include "core/source_artifact_publisher.h"
#include "fixtures/lyra/native_directory_fixture.h"
#include "rom/snes.h"
#include "unit/zelda3/gfx_sheet_test_rom.h"
#include "util/rom_hash.h"
#include "zelda3/dungeon/dungeon_rom_addresses.h"
#include "zelda3/dungeon/dungeon_stream_allocator.h"
#include "zelda3/dungeon/dungeon_wall_profile.h"
#include "zelda3/dungeon/entrance_layout.h"
#include "zelda3/dungeon/object_layer_semantics.h"
#include "zelda3/dungeon/room_entrance.h"
#include "zelda3/dungeon/room_layer_manager.h"

ABSL_DECLARE_FLAG(bool, sandbox);

namespace yaze::cli {
namespace {

// Opt-in compiled-ROM acceptance through the production loader and renderer.
// The companion context supplies the exact external custom objects; it never
// installs or writes a project. Ordinary ROM-free runs skip this fixture.
TEST(CompiledWallProfileAcceptance, GlaciaRoomsUseCompiledKitAndRender) {
  const char* fixture = std::getenv("YAZE_TEST_ROM_OOS");
  const char* context = std::getenv("YAZE_TEST_GLACIA_RENDER_CONTEXT");
  if (!fixture || !context)
    GTEST_SKIP()
        << "Supply candidate ROM and exact Glacia custom-object context";
  resources::CommandContext::Config config;
  config.rom_path = fixture;
  config.project_context_path = context;
  auto command = std::make_unique<resources::CommandContext>(config);
  ASSERT_TRUE(command->Initialize().ok());
  auto loaded = command->GetRom();
  ASSERT_TRUE(loaded.ok()) << loaded.status();
  Rom* rom = *loaded;
  const auto original = rom->vector();
  auto data = std::make_unique<zelda3::GameData>();
  ASSERT_TRUE(zelda3::LoadGameData(*rom, *data).ok());
  zelda3::RoomEntrance entrance(rom, 0x34);
  ASSERT_TRUE(entrance.load_status().ok());
  ASSERT_EQ(entrance.blockset_, 0x0B);
  for (int room : {0xDB, 0xCB, 0xBB, 0xBC, 0xBD, 0xAD, 0xAC, 0xCC, 0x9E, 0x9F,
                   0xAE, 0xAF, 0xBE, 0xBF, 0xCD, 0xCE, 0xCF, 0xDC, 0xDD}) {
    SCOPED_TRACE(room);
    const bool estate = room == 0xDB || room == 0xCB || room == 0xBB ||
                        room == 0xBC || room == 0xBD || room == 0xAD ||
                        room == 0xAC || room == 0xCC;
    auto profile = zelda3::ReadCompiledWallProfile(*rom, room);
    ASSERT_TRUE(profile.ok()) << profile.status();
    ASSERT_TRUE(profile->has_value());
    EXPECT_EQ((*profile)->blockset, estate ? 0x17 : 0x18);
    EXPECT_EQ((*profile)->palette, estate ? 0x26 : 0x24);
    EXPECT_EQ((*profile)->class_words.size(), 24);
    EXPECT_EQ((*profile)->slot6_high_palette,
              estate ? 3ULL : 0x0000003002700070ULL);
    // Use the same CPU Room/compositor path as RenderService. The Windows
    // focused target has no libpng; SDL's lossless BMP output keeps the exact
    // indexed surface palette without claiming CLI PNG qualification.
    auto loaded_room =
        std::make_unique<zelda3::Room>(zelda3::LoadRoomFromRom(rom, room));
    loaded_room->SetGameData(data.get());
    loaded_room->SetRenderEntranceBlockset(entrance.blockset_);
    loaded_room->LoadRoomGraphics();
    ASSERT_TRUE(loaded_room->compiled_wall_profile_status().ok());
    loaded_room->RenderRoomGraphics();
    EXPECT_EQ(loaded_room->resolved_main_blockset(), 0x0B);
    EXPECT_EQ(loaded_room->blocks()[3], estate ? 0xFE : 0xFD);
    EXPECT_EQ(loaded_room->blocks()[6], estate ? 0xF3 : 0xF4);
    EXPECT_EQ(
        zelda3::CountProjectObjectsWithoutData(loaded_room->GetTileObjects()),
        0);
    zelda3::RoomLayerManager layers;
    layers.ApplyLayerMerging(loaded_room->layer_merging());
    layers.ApplyRoomEffect(loaded_room->effect());
    layers.ApplyGameLayerRegisters(loaded_room->GameLayerRegisters());
    auto& bitmap = loaded_room->GetCompositeBitmap(layers);
    ASSERT_TRUE(bitmap.is_active());
    ASSERT_NE(bitmap.surface(), nullptr);
    EXPECT_EQ(bitmap.width(), 512);
    EXPECT_EQ(bitmap.height(), 512);
    if (const char* output = std::getenv("YAZE_TEST_GLACIA_RENDER_OUTPUT")) {
      const auto path =
          std::filesystem::path(output) / (std::to_string(room) + ".bmp");
      EXPECT_EQ(SDL_SaveBMP(bitmap.surface(), path.string().c_str()), 0)
          << SDL_GetError();
    }
  }
  EXPECT_EQ(rom->vector(), original);
}

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
  const auto entry =
      std::find_if(descriptor.entries.begin(), descriptor.entries.end(),
                   [](const auto& candidate) {
                     return candidate.name == "graphics-entrance";
                   });
  ASSERT_NE(entry, descriptor.entries.end());
  EXPECT_NE(entry->description.find("mutually exclusive"), std::string::npos);
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

TEST_F(DungeonRenderCommandsTest, ObjectPlanPathsRequireValues) {
  handlers::DungeonRenderCommandHandler handler;
  for (const std::vector<std::string> extra :
       {std::vector<std::string>{"--object-plan"},
        {"--object-plan="},
        {"--manifest=layout.json"},
        {"--object-plan=plan.json", "--manifest"}}) {
    std::vector<std::string> args{"--room=0", "--output=room.png"};
    args.insert(args.end(), extra.begin(), extra.end());
    EXPECT_FALSE(handler.ValidateArgs(resources::ArgumentParser(args)).ok());
  }
  EXPECT_TRUE(
      handler
          .ValidateArgs(resources::ArgumentParser(
              {"--room=0", "--output=room.png", "--object-plan", "plan.json"}))
          .ok());
}

TEST_F(DungeonRenderCommandsTest, ObjectPlanRefusesMalformedJsonWithoutOutput) {
  const auto plan = directory_ / "bad.json";
  {
    std::ofstream stream(plan);
    stream << "{not-json";
  }
  handlers::DungeonRenderCommandHandler handler;
  resources::OutputFormatter formatter(
      resources::OutputFormatter::Format::kJson);
  const auto status = handler.Execute(
      &rom_,
      resources::ArgumentParser({"--room=0", "--output",
                                 (directory_ / "room.png").string(),
                                 "--object-plan", plan.string()}),
      formatter);
  EXPECT_TRUE(absl::IsInvalidArgument(status)) << status;
  EXPECT_FALSE(std::filesystem::exists(directory_ / "room.png"));
  EXPECT_EQ(rom_.vector(), original_bytes_);
  EXPECT_EQ(ReadBytes(source_), original_bytes_);
}

TEST_F(DungeonRenderCommandsTest, ObjectPlanCannotOverwriteItsInputOrRom) {
  const auto plan = directory_ / "plan.json";
  {
    std::ofstream stream(plan);
    stream << "{}";
  }
  const auto plan_bytes = ReadBytes(plan);
  for (const auto& output : {plan, source_}) {
    handlers::DungeonRenderCommandHandler handler;
    resources::OutputFormatter formatter(
        resources::OutputFormatter::Format::kJson);
    const auto status = handler.Execute(
        &rom_,
        resources::ArgumentParser({"--room=0", "--output", output.string(),
                                   "--object-plan", plan.string()}),
        formatter);
    EXPECT_TRUE(absl::IsInvalidArgument(status)) << status;
    EXPECT_EQ(ReadBytes(plan), plan_bytes);
    EXPECT_EQ(ReadBytes(source_), original_bytes_);
    EXPECT_EQ(rom_.vector(), original_bytes_);
  }
}

class DungeonRenderObjectPlanTest : public ::testing::Test {
 protected:
  static constexpr int kTable = 0x70000;
  static constexpr int kData = 0x60000;
  static constexpr int kOther = 0x60100;
  Rom rom_;
  std::vector<uint8_t> baseline_;

  static nlohmann::json Object(int id, int x, int y, int size = 0,
                               int list = 0) {
    return {{"id", id}, {"x", x}, {"y", y}, {"size", size}, {"list", list}};
  }

  static std::string Hash(const std::vector<uint8_t>& bytes) {
    return core::ComputeSourceArtifactSha256(std::string_view(
        reinterpret_cast<const char*>(bytes.data()), bytes.size()));
  }

  void Long(int pc, uint32_t value) {
    for (int byte = 0; byte < 3; ++byte) {
      rom_.mutable_data()[pc + byte] = (value >> (byte * 8)) & 255;
    }
  }

  void SetUp() override {
    ASSERT_TRUE(rom_.LoadFromData(std::vector<uint8_t>(0x200000, 0)).ok());
    Long(zelda3::kRoomObjectPointer, PcToSnes(kTable));
    Long(zelda3::kRoomHeaderPointer, PcToSnes(0x20000));
    rom_.mutable_data()[zelda3::kRoomHeaderPointerBank] =
        PcToSnes(0x21000) >> 16;
    for (int room = 0; room < zelda3::kNumberOfRooms; ++room) {
      Long(kTable + room * 3, PcToSnes(room == 0 ? kData : kOther));
      Long(zelda3::kDoorPointers + room * 3,
           PcToSnes(room == 0 ? kData + 14 : kOther + 8));
      rom_.mutable_data()[0x20000 + room * 2] = 0;
      rom_.mutable_data()[0x20001 + room * 2] = 0x90;
      const auto pot = PcToSnes(0xF000);
      rom_.mutable_data()[zelda3::kRoomItemsPointers + room * 2] = pot & 255;
      rom_.mutable_data()[zelda3::kRoomItemsPointers + room * 2 + 1] =
          (pot >> 8) & 255;
    }
    ASSERT_TRUE(rom_.WriteVector(0xF000, {0xFF, 0xFF}).ok());
    // Two stream objects in different stored lists plus an actual door.
    ASSERT_TRUE(rom_.WriteVector(
                        kData, {0x12, 0, 40, 12, 5, 0xFF, 0xFF, 20, 42, 0x63,
                                0xFF, 0xFF, 0xF0, 0xFF, 0x20, 0x38, 0xFF, 0xFF})
                    .ok());
    ASSERT_TRUE(rom_.WriteVector(kOther, {0, 0, 0xFF, 0xFF, 0xFF, 0xFF, 0xF0,
                                          0xFF, 0xFF, 0xFF})
                    .ok());
    rom_.set_dirty(false);
    rom_.set_filename("read-only-source.sfc");
    baseline_ = rom_.vector();
  }

  nlohmann::json Plan(nlohmann::json edits = nlohmann::json::array()) {
    const auto stream = zelda3::ReadDungeonObjectStream(rom_, 0);
    EXPECT_TRUE(stream.ok()) << stream.status();
    return {{"version", 1},
            {"room", 0},
            {"rom_sha256", Hash(rom_.vector())},
            {"object_stream_sha256",
             stream.ok() ? Hash(stream->encoded_stream) : ""},
            {"edits", std::move(edits)}};
  }

  void ExpectSourceUnchanged() {
    EXPECT_EQ(rom_.vector(), baseline_);
    EXPECT_FALSE(rom_.dirty());
    EXPECT_EQ(rom_.filename(), "read-only-source.sfc");
  }
};

TEST_F(DungeonRenderObjectPlanTest, EmptyPlanPreservesEveryByte) {
  const auto preview = handlers::StageDungeonRenderObjectPlan(rom_, 0, Plan());
  ASSERT_TRUE(preview.ok()) << preview.status();
  EXPECT_EQ(preview->rom->vector(), baseline_);
  EXPECT_EQ(preview->objects_before, 2);
  EXPECT_EQ(preview->objects_after, 2);
  EXPECT_EQ(preview->edit_count, 0);
  ExpectSourceUnchanged();
}

TEST_F(DungeonRenderObjectPlanTest,
       AppendGroupsListsAndPreservesHeaderDoorAndOtherRooms) {
  auto plan = Plan(nlohmann::json::array(
      {{{"op", "append"}, {"object", Object(5, 45, 3)}},
       {{"op", "append"}, {"object", Object(0x100, 12, 14, 0, 2)}}}));
  const auto preview = handlers::StageDungeonRenderObjectPlan(rom_, 0, plan);
  ASSERT_TRUE(preview.ok()) << preview.status();
  EXPECT_EQ(preview->objects_after, 4);
  auto room = std::make_unique<zelda3::Room>(
      zelda3::LoadRoomFromRom(preview->rom.get(), 0));
  room->LoadObjects();
  const auto& objects = room->GetTileObjects();
  ASSERT_EQ(objects.size(), 4);
  EXPECT_EQ(objects[0].x(), 10);
  EXPECT_EQ(objects[1].x(), 45);
  EXPECT_EQ(objects[2].GetLayerValue(), 1);
  EXPECT_EQ(objects[3].GetLayerValue(), 2);
  EXPECT_EQ(room->floor1(), 2);
  ASSERT_EQ(room->GetDoors().size(), 1);
  EXPECT_EQ(room->GetDoors()[0].byte1, 0x20);
  EXPECT_EQ(room->GetDoors()[0].byte2, 0x38);
  for (int other = 1; other < zelda3::kNumberOfRooms; ++other) {
    const auto old = zelda3::ReadDungeonObjectStream(rom_, other);
    const auto saved = zelda3::ReadDungeonObjectStream(*preview->rom, other);
    ASSERT_TRUE(old.ok() && saved.ok());
    EXPECT_EQ(old->data_pc, saved->data_pc);
    EXPECT_EQ(old->encoded_stream, saved->encoded_stream);
  }
  ExpectSourceUnchanged();
}

TEST_F(DungeonRenderObjectPlanTest, RemoveAndReplaceUseOriginalIndices) {
  auto plan = Plan(nlohmann::json::array(
      {{{"op", "remove"}, {"index", 0}, {"before", Object(5, 10, 3)}},
       {{"op", "replace"},
        {"index", 1},
        {"before", Object(0x63, 5, 10, 2, 1)},
        {"object", Object(0x63, 7, 10, 3, 1)}}}));
  const auto preview = handlers::StageDungeonRenderObjectPlan(rom_, 0, plan);
  ASSERT_TRUE(preview.ok()) << preview.status();
  auto room = std::make_unique<zelda3::Room>(
      zelda3::LoadRoomFromRom(preview->rom.get(), 0));
  room->LoadObjects();
  ASSERT_EQ(room->GetTileObjects().size(), 1);
  EXPECT_EQ(room->GetTileObjects()[0].x(), 7);
  EXPECT_EQ(room->GetTileObjects()[0].size(), 3);
  ExpectSourceUnchanged();
}

TEST_F(DungeonRenderObjectPlanTest, RejectsStaleIdentitiesAndObjectPreimages) {
  auto rom_hash = Plan();
  rom_hash["rom_sha256"] = std::string(64, '0');
  auto stream_hash = Plan();
  stream_hash["object_stream_sha256"] = std::string(64, '0');
  auto room_id = Plan();
  room_id["room"] = 1;
  auto stale_object =
      Plan(nlohmann::json::array({{{"op", "remove"},
                                   {"index", 1},
                                   {"before", Object(0x63, 6, 10, 2, 1)}}}));
  for (const auto& plan : {rom_hash, stream_hash, room_id, stale_object}) {
    EXPECT_TRUE(absl::IsFailedPrecondition(
        handlers::StageDungeonRenderObjectPlan(rom_, 0, plan).status()));
    ExpectSourceUnchanged();
  }
}

TEST_F(DungeonRenderObjectPlanTest,
       RejectsInvalidObjectNumbersBeforeNarrowing) {
  for (const auto& object :
       {Object(0xF8, 3, 4), Object(5, 256, 3), Object(5, 3, -1),
        Object(5, 63, 3), Object(5, 3, 4, 16), Object(0x100, 3, 4, 1),
        Object(5, 3, 4, 0, 3)}) {
    EXPECT_FALSE(handlers::StageDungeonRenderObjectPlan(
                     rom_, 0,
                     Plan(nlohmann::json::array(
                         {{{"op", "append"}, {"object", object}}})))
                     .ok());
    ExpectSourceUnchanged();
  }
  for (const nlohmann::json value :
       {nlohmann::json(true), nlohmann::json(1.5), nlohmann::json("5"),
        nlohmann::json(UINT64_MAX)}) {
    auto object = Object(5, 3, 4);
    object["id"] = value;
    EXPECT_FALSE(handlers::StageDungeonRenderObjectPlan(
                     rom_, 0,
                     Plan(nlohmann::json::array(
                         {{{"op", "append"}, {"object", object}}})))
                     .ok());
  }
}

TEST_F(DungeonRenderObjectPlanTest, RejectsUnknownFieldsAndRepeatedIndices) {
  auto unknown = Plan();
  unknown["memory_edits"] = nlohmann::json::array();
  auto duplicate = Plan(nlohmann::json::array(
      {{{"op", "remove"}, {"index", 0}, {"before", Object(5, 10, 3)}},
       {{"op", "remove"}, {"index", 0}, {"before", Object(5, 10, 3)}}}));
  auto unknown_op = Plan(nlohmann::json::array({{{"op", "write"}, {"pc", 0}}}));
  for (const auto& plan : {unknown, duplicate, unknown_op}) {
    EXPECT_TRUE(absl::IsInvalidArgument(
        handlers::StageDungeonRenderObjectPlan(rom_, 0, plan).status()));
    ExpectSourceUnchanged();
  }
}

TEST_F(DungeonRenderObjectPlanTest, SharedSourceNeedsDeclaredAllocation) {
  Long(kTable, PcToSnes(kOther));
  Long(zelda3::kDoorPointers, PcToSnes(kOther + 8));
  baseline_ = rom_.vector();
  const auto preview = handlers::StageDungeonRenderObjectPlan(
      rom_, 0,
      Plan(nlohmann::json::array(
          {{{"op", "append"}, {"object", Object(5, 10, 3)}}})));
  EXPECT_TRUE(absl::IsFailedPrecondition(preview.status())) << preview.status();
  ExpectSourceUnchanged();
}

TEST_F(DungeonRenderObjectPlanTest,
       DeclaredCopyOnWritePreservesSharedRoomsAndHonorsProtection) {
  Long(kTable, PcToSnes(kOther));
  Long(zelda3::kDoorPointers, PcToSnes(kOther + 8));
  baseline_ = rom_.vector();
  auto plan = Plan(nlohmann::json::array(
      {{{"op", "append"}, {"object", Object(5, 10, 3)}}}));
  const auto snes = [](int pc) {
    return absl::StrFormat("0x%06X", PcToSnes(pc));
  };
  nlohmann::json manifest = {
      {"manifest_version", 3},
      {"dungeon_stream_regions",
       {{"objects",
         {{"pointer_table", snes(kTable)},
          {"pointer_count", zelda3::kNumberOfRooms},
          {"pointer_encoding", "long24"},
          {"strategy", "copy_on_write"},
          {"data_regions",
           {{{"start", snes(kData)}, {"end", snes(kData + 0x1000)}}}},
          {"allocation_regions",
           {{{"start", snes(kData + 0x200)},
             {"end", snes(kData + 0x300)}}}}}}}}};
  core::HackManifest allocation;
  ASSERT_TRUE(allocation.LoadFromString(manifest.dump()).ok());
  const auto preview =
      handlers::StageDungeonRenderObjectPlan(rom_, 0, plan, &allocation);
  ASSERT_TRUE(preview.ok()) << preview.status();
  const auto room0 = zelda3::ReadDungeonObjectStream(*preview->rom, 0);
  ASSERT_TRUE(room0.ok()) << room0.status();
  EXPECT_EQ(room0->data_pc, kData + 0x200);
  for (int other = 1; other < zelda3::kNumberOfRooms; ++other) {
    const auto old = zelda3::ReadDungeonObjectStream(rom_, other);
    const auto saved = zelda3::ReadDungeonObjectStream(*preview->rom, other);
    ASSERT_TRUE(old.ok() && saved.ok());
    EXPECT_EQ(old->data_pc, saved->data_pc);
    EXPECT_EQ(old->encoded_stream, saved->encoded_stream);
  }
  manifest["protected_regions"] = {{"total_hooks", 1},
                                   {"regions",
                                    {{{"start", snes(kData + 0x200)},
                                      {"end", snes(kData + 0x201)},
                                      {"size", 1},
                                      {"hook_count", 1},
                                      {"module", "ObjectPreviewGuard"}}}}};
  core::HackManifest protected_allocation;
  ASSERT_TRUE(protected_allocation.LoadFromString(manifest.dump()).ok());
  EXPECT_TRUE(absl::IsPermissionDenied(handlers::StageDungeonRenderObjectPlan(
                                           rom_, 0, plan, &protected_allocation)
                                           .status()));
  ExpectSourceUnchanged();
}

class DungeonApplyObjectPlanTest : public DungeonRenderObjectPlanTest {
 protected:
  std::filesystem::path directory_;
  std::filesystem::path source_;
  std::filesystem::path plan_path_;
  bool previous_sandbox_ = false;

  static std::vector<uint8_t> ReadBytes(const std::filesystem::path& path) {
    return test::native_overworld::ReadBytes(path);
  }

  void SetUp() override {
    DungeonRenderObjectPlanTest::SetUp();
    previous_sandbox_ = absl::GetFlag(FLAGS_sandbox);
    absl::SetFlag(&FLAGS_sandbox, false);
    directory_ =
        std::filesystem::temp_directory_path() /
        ("yaze_apply_object_plan_" +
         std::to_string(
             std::chrono::steady_clock::now().time_since_epoch().count()));
    ASSERT_TRUE(std::filesystem::create_directory(directory_));
    source_ = directory_ / "source.sfc";
    plan_path_ = directory_ / "plan.json";
    FileBaseline();
  }

  void TearDown() override {
    Rom::SetStagingFailureForTesting(false);
    absl::SetFlag(&FLAGS_sandbox, previous_sandbox_);
    std::error_code error;
    std::filesystem::remove_all(directory_, error);
  }

  void FileBaseline() {
    test::native_overworld::WriteBytes(source_, rom_.vector());
    rom_.set_filename(source_.string());
    rom_.set_dirty(false);
    baseline_ = rom_.vector();
  }

  nlohmann::json Edits() {
    return Plan(nlohmann::json::array(
        {{{"op", "remove"}, {"index", 0}, {"before", Object(5, 10, 3)}},
         {{"op", "replace"},
          {"index", 1},
          {"before", Object(0x63, 5, 10, 2, 1)},
          {"object", Object(0x63, 7, 10, 3, 1)}},
         {{"op", "append"}, {"object", Object(5, 45, 3)}}}));
  }

  absl::Status Apply(const nlohmann::json& plan, bool write,
                     std::string* output = nullptr,
                     std::vector<std::string> extra = {}) {
    {
      std::ofstream file(plan_path_);
      file << plan;
    }
    std::vector<std::string> args = {"--room=0",
                                     "--object-plan=" + plan_path_.string()};
    if (write)
      args.push_back("--write");
    args.insert(args.end(), extra.begin(), extra.end());
    std::string unused;
    handlers::DungeonApplyObjectPlanCommandHandler handler;
    return handler.Run(args, &rom_, output ? output : &unused);
  }

  void NativeBaseline() {
    test::native_overworld::NativeDirectoryFixture fixture;
    auto& image = fixture.bytes;
    // Keep the native descriptor and owned payloads intact. Only seed the
    // synthetic dungeon model, with object data outside native Tile16 storage.
    for (const auto& [begin, end] : std::vector<std::pair<int, int>>{
             {zelda3::kRoomObjectPointer, zelda3::kRoomObjectPointer + 3},
             {zelda3::kRoomHeaderPointer, zelda3::kRoomHeaderPointer + 3},
             {zelda3::kRoomHeaderPointerBank,
              zelda3::kRoomHeaderPointerBank + 1},
             {kTable, kTable + zelda3::kNumberOfRooms * 3},
             {zelda3::kDoorPointers,
              zelda3::kDoorPointers + zelda3::kNumberOfRooms * 3},
             {zelda3::kRoomItemsPointers,
              zelda3::kRoomItemsPointers + zelda3::kNumberOfRooms * 2},
             {0x20000, 0x20000 + zelda3::kNumberOfRooms * 2},
             {0xF000, 0xF002}}) {
      std::copy(baseline_.begin() + begin, baseline_.begin() + end,
                image.begin() + begin);
    }
    std::copy(baseline_.begin() + kData, baseline_.begin() + kData + 0x200,
              image.begin() + 0x90000);
    ASSERT_TRUE(rom_.LoadFromData(image).ok());
    for (int room = 0; room < zelda3::kNumberOfRooms; ++room) {
      Long(kTable + room * 3, PcToSnes(room == 0 ? 0x90000 : 0x90100));
      Long(zelda3::kDoorPointers + room * 3,
           PcToSnes(room == 0 ? 0x9000E : 0x90108));
    }
    FileBaseline();
    ASSERT_TRUE(handlers::IsNativeDungeonLayout(rom_));
  }
};

TEST_F(DungeonApplyObjectPlanTest, DryRunAndOneSaveReopenMatchPreviewExactly) {
  const auto plan = Edits();
  const auto staged = handlers::StageDungeonRenderObjectPlan(rom_, 0, plan);
  ASSERT_TRUE(staged.ok()) << staged.status();
  ASSERT_TRUE(Apply(plan, false).ok());
  EXPECT_EQ(rom_.vector(), baseline_);
  EXPECT_EQ(ReadBytes(source_), baseline_);
  EXPECT_EQ(std::distance(std::filesystem::directory_iterator(directory_),
                          std::filesystem::directory_iterator()),
            2);
  std::string output;
  const auto status = Apply(plan, true, &output);
  ASSERT_TRUE(status.ok()) << status << output;
  EXPECT_EQ(rom_.vector(), staged->rom->vector());
  Rom disk;
  ASSERT_TRUE(disk.LoadFromFile(source_.string()).ok());
  EXPECT_EQ(disk.vector(), staged->rom->vector());
  EXPECT_FALSE(rom_.dirty());
  int backups = 0;
  for (const auto& entry : std::filesystem::directory_iterator(directory_)) {
    if (entry.path().filename().string().find("source.sfc_backup_") == 0) {
      ++backups;
      EXPECT_EQ(ReadBytes(entry.path()), baseline_);
    }
  }
  EXPECT_EQ(backups, 1);
  EXPECT_NE(output.find("verified_reload"), std::string::npos);
}

TEST_F(DungeonApplyObjectPlanTest, RefusesStalePlanStreamObjectAndRoom) {
  auto stale_rom = Edits();
  stale_rom["rom_sha256"] = std::string(64, '0');
  auto stale_stream = Edits();
  stale_stream["object_stream_sha256"] = std::string(64, '0');
  auto stale_object = Edits();
  stale_object["edits"][0]["before"]["x"] = 11;
  auto wrong_room = Edits();
  wrong_room["room"] = 1;
  for (const auto& plan : {stale_rom, stale_stream, stale_object, wrong_room}) {
    EXPECT_TRUE(absl::IsFailedPrecondition(Apply(plan, true)));
    EXPECT_EQ(ReadBytes(source_), baseline_);
    EXPECT_EQ(rom_.vector(), baseline_);
  }
}

TEST_F(DungeonApplyObjectPlanTest, RefusesDirtyAndStaleDiskBaselines) {
  const auto plan = Edits();
  rom_.set_dirty(true);
  EXPECT_TRUE(absl::IsFailedPrecondition(Apply(plan, true)));
  rom_.set_dirty(false);
  auto changed = baseline_;
  changed[0x100] ^= 1;
  test::native_overworld::WriteBytes(source_, changed);
  EXPECT_TRUE(absl::IsFailedPrecondition(Apply(plan, true)));
  EXPECT_EQ(ReadBytes(source_), changed);
  EXPECT_EQ(rom_.vector(), baseline_);
}

TEST_F(DungeonApplyObjectPlanTest,
       FailedAtomicSavePreservesOriginalAndCleansStage) {
  Rom::SetStagingFailureForTesting(true);
  EXPECT_FALSE(Apply(Edits(), true).ok());
  EXPECT_EQ(ReadBytes(source_), baseline_);
  EXPECT_EQ(rom_.vector(), baseline_);
  EXPECT_FALSE(rom_.dirty());
  for (const auto& entry : std::filesystem::directory_iterator(directory_)) {
    EXPECT_EQ(entry.path().filename().string().find(".tmp"), std::string::npos);
  }
}

TEST_F(DungeonApplyObjectPlanTest, Native192SaveMatchesSamePreview) {
  NativeBaseline();
  const auto plan = Edits();
  const auto staged = handlers::StageDungeonRenderObjectPlan(rom_, 0, plan);
  ASSERT_TRUE(staged.ok()) << staged.status();
  const auto status = Apply(plan, true);
  ASSERT_TRUE(status.ok()) << status;
  EXPECT_EQ(ReadBytes(source_), staged->rom->vector());
  EXPECT_TRUE(handlers::IsNativeDungeonLayout(rom_));
}

TEST_F(DungeonApplyObjectPlanTest, NativeAtomicSaveFailurePreservesOriginal) {
  NativeBaseline();
  Rom::SetStagingFailureForTesting(true);
  EXPECT_FALSE(Apply(Edits(), true).ok());
  EXPECT_EQ(rom_.vector(), baseline_);
  EXPECT_EQ(ReadBytes(source_), baseline_);
  EXPECT_FALSE(rom_.dirty());
  EXPECT_TRUE(handlers::IsNativeDungeonLayout(rom_));
}

TEST_F(DungeonApplyObjectPlanTest,
       ExplicitManifestRequiresObjectAllocationContract) {
  const auto manifest = directory_ / "manifest.json";
  {
    std::ofstream file(manifest);
    file << R"({"manifest_version":3})";
  }
  const auto status =
      Apply(Edits(), true, nullptr, {"--manifest=" + manifest.string()});
  EXPECT_TRUE(absl::IsFailedPrecondition(status)) << status;
  EXPECT_EQ(ReadBytes(source_), baseline_);
}

TEST_F(DungeonApplyObjectPlanTest,
       NativeFenceRefusesManifestAllocationInDescriptor) {
  NativeBaseline();
  // A shared stream forces relocation. The manifest intentionally offers the
  // native directory as allocation space; its contract cannot grant ownership.
  Long(kTable, PcToSnes(0x90100));
  Long(zelda3::kDoorPointers, PcToSnes(0x90108));
  FileBaseline();
  const auto snes = [](int pc) {
    return absl::StrFormat("0x%06X", PcToSnes(pc));
  };
  const nlohmann::json manifest = {
      {"manifest_version", 3},
      {"dungeon_stream_regions",
       {{"objects",
         {{"pointer_table", snes(kTable)},
          {"pointer_count", zelda3::kNumberOfRooms},
          {"pointer_encoding", "long24"},
          {"strategy", "copy_on_write"},
          {"data_regions",
           {{{"start", snes(0x90000)}, {"end", snes(0x90200)}},
            {{"start", snes(0x147000)}, {"end", snes(0x147400)}}}},
          {"allocation_regions",
           {{{"start", snes(0x147000)}, {"end", snes(0x147400)}}}}}}}}};
  const auto path = directory_ / "manifest.json";
  {
    std::ofstream file(path);
    file << manifest;
  }
  const auto plan = Plan(nlohmann::json::array(
      {{{"op", "append"}, {"object", Object(5, 10, 3)}}}));
  const auto status =
      Apply(plan, true, nullptr, {"--manifest=" + path.string()});
  EXPECT_TRUE(absl::IsPermissionDenied(status)) << status;
  EXPECT_EQ(rom_.vector(), baseline_);
  EXPECT_EQ(ReadBytes(source_), baseline_);
}

TEST(DungeonApplyObjectPlanSchemaTest, RegisteredAndDiscoverable) {
  const auto all_handlers = handlers::CreateCliCommandHandlers();
  const auto found = std::find_if(
      all_handlers.begin(), all_handlers.end(), [](const auto& handler) {
        return handler->GetName() == "dungeon-apply-object-plan";
      });
  ASSERT_NE(found, all_handlers.end());
  const auto schema =
      UsageParametersToJsonSchema(ParseUsageParameters((*found)->GetUsage()));
  EXPECT_TRUE(schema["properties"].contains("object-plan"));
  EXPECT_TRUE(schema["properties"].contains("manifest"));
  EXPECT_EQ(schema["properties"]["write"]["type"], "boolean");
}

}  // namespace
}  // namespace yaze::cli
