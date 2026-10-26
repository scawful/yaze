#include "cli/service/resources/command_handler.h"

#include <chrono>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <string>
#include <vector>

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include "absl/flags/declare.h"
#include "absl/flags/flag.h"
#include "cli/handlers/tools/test_helpers_commands.h"
#include "cli/service/rom/rom_sandbox_manager.h"
#include "core/features.h"
#include "core/project.h"
#include "zelda3/dungeon/custom_object.h"
#include "zelda3/dungeon/draw_routines/draw_routine_registry.h"

ABSL_DECLARE_FLAG(bool, sandbox);

namespace yaze {
namespace cli {
namespace resources {
namespace {

class NoRomCommandHandler : public CommandHandler {
 public:
  std::string GetName() const override { return "no-rom"; }
  std::string GetUsage() const override { return "no-rom"; }
  bool RequiresRom() const override { return false; }

  Rom* last_rom() const { return last_rom_; }

 protected:
  absl::Status ValidateArgs(const ArgumentParser&) override {
    return absl::OkStatus();
  }

  absl::Status Execute(Rom* rom, const ArgumentParser&,
                       OutputFormatter& formatter) override {
    last_rom_ = rom;
    formatter.AddField("ok", true);
    return absl::OkStatus();
  }

 private:
  Rom* last_rom_ = nullptr;
};

class MockRomCommandHandler : public CommandHandler {
 public:
  std::string GetName() const override { return "mock-rom"; }
  std::string GetUsage() const override { return "mock-rom"; }

  bool rom_loaded() const { return rom_loaded_; }

 protected:
  absl::Status ValidateArgs(const ArgumentParser&) override {
    return absl::OkStatus();
  }

  absl::Status Execute(Rom* rom, const ArgumentParser&,
                       OutputFormatter& formatter) override {
    rom_loaded_ = (rom != nullptr && rom->is_loaded());
    formatter.AddField("rom_loaded", rom_loaded_);
    return absl::OkStatus();
  }

 private:
  bool rom_loaded_ = false;
};

class FailingCommandHandler : public CommandHandler {
 public:
  std::string GetName() const override { return "failing"; }
  std::string GetUsage() const override { return "failing"; }
  bool RequiresRom() const override { return false; }

 protected:
  absl::Status ValidateArgs(const ArgumentParser&) override {
    return absl::OkStatus();
  }

  absl::Status Execute(Rom*, const ArgumentParser&,
                       OutputFormatter& formatter) override {
    formatter.AddField("status", "fail");
    formatter.AddField("reason", "expected");
    return absl::FailedPreconditionError("expected failure");
  }
};

class ProjectAwareCommandHandler : public CommandHandler {
 public:
  std::string GetName() const override { return "project-aware"; }
  std::string GetUsage() const override { return "project-aware"; }

  bool saw_project_context() const { return saw_project_context_; }
  bool custom_objects_enabled() const { return custom_objects_enabled_; }
  int routine_for_track_object() const { return routine_for_track_object_; }

 protected:
  absl::Status ValidateArgs(const ArgumentParser&) override {
    return absl::OkStatus();
  }

  absl::Status Execute(Rom* rom, const ArgumentParser&,
                       OutputFormatter& formatter) override {
    saw_project_context_ = (project_ != nullptr);
    custom_objects_enabled_ = core::FeatureFlags::get().kEnableCustomObjects;
    routine_for_track_object_ =
        zelda3::DrawRoutineRegistry::Get().GetRoutineIdForObject(0x31);
    formatter.AddField("rom_loaded", rom != nullptr && rom->is_loaded());
    formatter.AddField("saw_project_context", saw_project_context_);
    formatter.AddField("custom_objects_enabled", custom_objects_enabled_);
    formatter.AddField("track_routine", routine_for_track_object_);
    return absl::OkStatus();
  }

 private:
  bool saw_project_context_ = false;
  bool custom_objects_enabled_ = false;
  int routine_for_track_object_ = -1;
};

class ReadOnlyProbeCommandHandler : public CommandHandler {
 public:
  explicit ReadOnlyProbeCommandHandler(Rom* source_probe = nullptr)
      : source_probe_(source_probe) {}
  std::string GetName() const override { return "read-only-probe"; }
  std::string GetUsage() const override { return "read-only-probe"; }
  bool saw_read_only() const { return saw_read_only_; }
  bool source_writable() const { return source_writable_; }
  const std::string& active_filename() const { return active_filename_; }

 protected:
  absl::Status ValidateArgs(const ArgumentParser&) override {
    return absl::OkStatus();
  }
  absl::Status Execute(Rom* rom, const ArgumentParser& parser,
                       OutputFormatter& formatter) override {
    saw_read_only_ = absl::IsPermissionDenied(rom->CheckLayoutWritable());
    source_writable_ =
        source_probe_ && source_probe_->CheckLayoutWritable().ok();
    active_filename_ = rom->filename();
    const auto byte = rom->ReadByte(0);
    if (!byte.ok()) {
      return byte.status();
    }
    formatter.AddField("first_byte", static_cast<int>(*byte));
    if (parser.HasFlag("dry-run")) {
      Rom simulated = *rom;
      return simulated.WriteByte(0, 0x77);
    }
    const auto action = parser.GetString("action").value_or("inspect");
    if (action == "write") {
      return rom->WriteByte(0, 0x77);
    }
    if (action == "save") {
      return rom->SaveToFile(
          {.filename = parser.GetString("out").value_or("")});
    }
    if (action == "dirty") {
      rom->set_dirty(true);
    }
    return absl::OkStatus();
  }

 private:
  Rom* source_probe_ = nullptr;
  bool saw_read_only_ = false;
  bool source_writable_ = false;
  std::string active_filename_;
};

class ReadOnlyCommandHandlerTest : public ::testing::Test {
 protected:
  void SetUp() override {
    previous_sandbox_flag_ = absl::GetFlag(FLAGS_sandbox);
    absl::SetFlag(&FLAGS_sandbox, false);
    directory_ =
        std::filesystem::temp_directory_path() /
        ("yaze_command_read_only_" +
         std::to_string(
             std::chrono::steady_clock::now().time_since_epoch().count()));
    std::filesystem::create_directories(directory_);
    source_path_ = directory_ / "synthetic.sfc";
    project_path_ = directory_ / "ReadOnly.yaze";
    {
      std::ofstream file(source_path_, std::ios::binary);
      file.write(reinterpret_cast<const char*>(bytes_.data()),
                 static_cast<std::streamsize>(bytes_.size()));
      ASSERT_TRUE(file.good());
    }
    {
      std::ofstream project(project_path_);
      project << "[project]\nname=ReadOnly\n\n[rom]\nwrite_policy=read_only\n";
      ASSERT_TRUE(project.good());
    }
    ASSERT_TRUE(rom_.LoadFromFile(source_path_.string()).ok());
  }

  void TearDown() override {
    if (sandbox_root_changed_) {
      auto& manager = RomSandboxManager::Instance();
      for (const auto& sandbox : manager.ListSandboxes()) {
        if (sandbox.directory.parent_path() == directory_ / "sandboxes") {
          manager.RemoveSandbox(sandbox.id).IgnoreError();
        }
      }
      manager.SetRootDirectory(previous_sandbox_root_);
    }
    absl::SetFlag(&FLAGS_sandbox, previous_sandbox_flag_);
    std::error_code ec;
    std::filesystem::remove_all(directory_, ec);
  }

  std::vector<std::string> Args(const std::string& action = "inspect") const {
    return {"--project-context=" + project_path_.string(), "--format=json",
            "--action=" + action};
  }

  std::vector<uint8_t> ReadBytes(const std::filesystem::path& path) const {
    std::ifstream file(path, std::ios::binary);
    return std::vector<uint8_t>(std::istreambuf_iterator<char>(file),
                                std::istreambuf_iterator<char>());
  }

  void UseTemporarySandboxRoot() {
    auto& manager = RomSandboxManager::Instance();
    previous_sandbox_root_ = manager.RootDirectory();
    manager.SetRootDirectory(directory_ / "sandboxes");
    sandbox_root_changed_ = true;
  }

  Rom rom_;
  std::filesystem::path directory_;
  std::filesystem::path source_path_;
  std::filesystem::path project_path_;
  const std::vector<uint8_t> bytes_ = std::vector<uint8_t>(0x8000, 0x11);
  std::filesystem::path previous_sandbox_root_;
  bool previous_sandbox_flag_ = false;
  bool sandbox_root_changed_ = false;
};

TEST(CommandHandlerTest, RunWithoutRomUsesFormatter) {
  NoRomCommandHandler handler;
  std::string output;
  auto status = handler.Run({"--format=text"}, nullptr, &output);

  ASSERT_TRUE(status.ok());
  EXPECT_EQ(handler.last_rom(), nullptr);
  EXPECT_THAT(output, ::testing::HasSubstr("=== Result ==="));
  EXPECT_THAT(output, ::testing::HasSubstr("ok"));
}

TEST(CommandHandlerTest, RunWithMockRomLoadsRom) {
  MockRomCommandHandler handler;
  std::string output;
  auto status = handler.Run({"--mock-rom"}, nullptr, &output);

  ASSERT_TRUE(status.ok());
  EXPECT_TRUE(handler.rom_loaded());
  EXPECT_THAT(output, ::testing::HasSubstr("rom_loaded"));
}

TEST(CommandHandlerTest, RunRejectsUnknownFormat) {
  NoRomCommandHandler handler;
  std::string output;
  auto status = handler.Run({"--format=bogus"}, nullptr, &output);

  EXPECT_FALSE(status.ok());
  EXPECT_THAT(std::string(status.message()),
              ::testing::HasSubstr("Unknown format"));
}

TEST(CommandHandlerTest, RunCapturesFormatterOutputOnExecuteFailure) {
  FailingCommandHandler handler;
  std::string output;
  const auto status = handler.Run({"--format=json"}, nullptr, &output);

  EXPECT_FALSE(status.ok());
  EXPECT_EQ(status.code(), absl::StatusCode::kFailedPrecondition);
  EXPECT_THAT(output, ::testing::HasSubstr("\"status\": \"fail\""));
  EXPECT_THAT(output, ::testing::HasSubstr("\"reason\": \"expected\""));
}

TEST(CommandHandlerTest, RunWithProjectContextAppliesEditorParityFlags) {
  const auto original_flags = core::FeatureFlags::get();
  const auto original_custom_object_state =
      zelda3::CustomObjectManager::Get().SnapshotState();

  auto temp_root =
      std::filesystem::temp_directory_path() /
      ("yaze_command_handler_project_context_" +
       std::to_string(
           std::chrono::steady_clock::now().time_since_epoch().count()));
  std::filesystem::create_directories(temp_root);
  struct Cleanup {
    core::FeatureFlags::Flags original_flags;
    zelda3::CustomObjectManager::State original_custom_object_state;
    std::filesystem::path temp_root;
    ~Cleanup() {
      core::FeatureFlags::get() = original_flags;
      zelda3::CustomObjectManager::Get().RestoreState(
          original_custom_object_state);
      zelda3::DrawRoutineRegistry::Get().RefreshFeatureFlagMappings();
      std::filesystem::remove_all(temp_root);
    }
  } cleanup{original_flags, original_custom_object_state, temp_root};

  const std::string project_name =
      "handler_ctx_" +
      std::to_string(
          std::chrono::steady_clock::now().time_since_epoch().count());
  project::YazeProject project;
  ASSERT_TRUE(project.Create(project_name, temp_root.string()).ok());
  auto custom_dir = temp_root / project_name / "custom";
  std::filesystem::create_directories(custom_dir);
  {
    std::ofstream out(custom_dir / "track_LR.bin",
                      std::ios::out | std::ios::binary | std::ios::trunc);
    out.put('\0');
    out.put('\0');
  }
  project.feature_flags.kEnableCustomObjects = true;
  project.custom_objects_folder = custom_dir.string();
  project.custom_object_files[0x31] = {"track_LR.bin"};
  ASSERT_TRUE(project.Save().ok());

  core::FeatureFlags::get().kEnableCustomObjects = false;
  zelda3::DrawRoutineRegistry::Get().RefreshFeatureFlagMappings();

  ProjectAwareCommandHandler handler;
  std::string output;
  auto status = handler.Run(
      {"--mock-rom", "--project-context=" + project.filepath, "--format=json"},
      nullptr, &output);

  EXPECT_TRUE(status.ok());
  EXPECT_TRUE(handler.saw_project_context());
  EXPECT_TRUE(handler.custom_objects_enabled());
  EXPECT_EQ(handler.routine_for_track_object(),
            zelda3::DrawRoutineIds::kCustomObject);

  EXPECT_FALSE(core::FeatureFlags::get().kEnableCustomObjects);
  EXPECT_EQ(zelda3::DrawRoutineRegistry::Get().GetRoutineIdForObject(0x31),
            zelda3::DrawRoutineIds::kNothing);
}

TEST_F(ReadOnlyCommandHandlerTest, InspectionAndCopiedDryRunRemainUsable) {
  ReadOnlyProbeCommandHandler handler;
  std::string output;
  ASSERT_TRUE(handler.Run(Args(), &rom_, &output).ok());
  EXPECT_TRUE(handler.saw_read_only());
  EXPECT_THAT(output, ::testing::HasSubstr("first_byte"));
  EXPECT_TRUE(rom_.CheckLayoutWritable().ok());

  auto args = Args("write");
  args.push_back("--dry-run");
  ASSERT_TRUE(handler.Run(args, &rom_, &output).ok());
  EXPECT_TRUE(handler.saw_read_only());
  EXPECT_EQ(rom_.vector(), bytes_);
  EXPECT_FALSE(rom_.dirty());
  EXPECT_TRUE(rom_.CheckLayoutWritable().ok());
}

TEST_F(ReadOnlyCommandHandlerTest,
       MutationAndPhysicalSaveAreDeniedAndRestoreState) {
  ReadOnlyProbeCommandHandler handler;
  std::string output;
  EXPECT_TRUE(
      absl::IsPermissionDenied(handler.Run(Args("write"), &rom_, &output)));
  EXPECT_EQ(rom_.vector(), bytes_);
  EXPECT_TRUE(rom_.CheckLayoutWritable().ok());

  const auto target = directory_ / "must-not-exist.sfc";
  auto args = Args("save");
  args.push_back("--out=" + target.string());
  EXPECT_TRUE(absl::IsPermissionDenied(handler.Run(args, &rom_, &output)));
  EXPECT_FALSE(std::filesystem::exists(target));
  EXPECT_FALSE(std::filesystem::exists(target.string() + ".tmp"));
  EXPECT_EQ(ReadBytes(source_path_), bytes_);
  EXPECT_TRUE(rom_.WriteByte(0, 0x22).ok());
}

TEST_F(ReadOnlyCommandHandlerTest, ToolsPatchV3RejectsReadOnlyProject) {
  handlers::ToolsPatchV3CommandHandler handler;
  const auto target = directory_ / "must-not-be-patched.sfc";
  std::string output;
  const auto status = handler.Run(
      {"--rom=" + source_path_.string(), "--output=" + target.string(),
       "--project-context=" + project_path_.string(), "--format=json"},
      nullptr, &output);

  EXPECT_TRUE(absl::IsPermissionDenied(status));
  EXPECT_FALSE(std::filesystem::exists(target));
  EXPECT_EQ(ReadBytes(source_path_), bytes_);
}

TEST_F(ReadOnlyCommandHandlerTest,
       SandboxScopesOnlyActiveCopyAndBlocksAutoSave) {
  UseTemporarySandboxRoot();
  ReadOnlyProbeCommandHandler handler(&rom_);
  std::string output;
  auto args = Args();
  args.push_back("--sandbox");
  ASSERT_TRUE(handler.Run(args, &rom_, &output).ok());
  EXPECT_TRUE(handler.saw_read_only());
  EXPECT_TRUE(handler.source_writable());
  EXPECT_NE(handler.active_filename(), source_path_.string());
  EXPECT_EQ(ReadBytes(handler.active_filename()), bytes_);

  args = Args("dirty");
  args.push_back("--sandbox");
  EXPECT_TRUE(absl::IsPermissionDenied(handler.Run(args, &rom_, &output)));
  EXPECT_TRUE(handler.saw_read_only());
  EXPECT_EQ(ReadBytes(handler.active_filename()), bytes_);
  EXPECT_EQ(ReadBytes(source_path_), bytes_);
  EXPECT_FALSE(rom_.dirty());
  EXPECT_TRUE(rom_.CheckLayoutWritable().ok());
}

TEST_F(ReadOnlyCommandHandlerTest, NestedScopesRetainExternalProtection) {
  ReadOnlyProbeCommandHandler handler;
  std::string output;
  {
    ScopedRomReadOnly outer(rom_);
    ASSERT_TRUE(handler.Run(Args(), &rom_, &output).ok());
    EXPECT_TRUE(absl::IsPermissionDenied(rom_.CheckLayoutWritable()));
    EXPECT_TRUE(
        absl::IsPermissionDenied(handler.Run(Args("write"), &rom_, &output)));
    EXPECT_TRUE(absl::IsPermissionDenied(rom_.WriteByte(0, 0x33)));
  }
  EXPECT_TRUE(rom_.WriteByte(0, 0x44).ok());
}

}  // namespace
}  // namespace resources
}  // namespace cli
}  // namespace yaze
