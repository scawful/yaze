#include "cli/service/resources/command_handler.h"

#include <chrono>
#include <filesystem>
#include <fstream>
#include <string>
#include <vector>

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include "core/features.h"
#include "core/project.h"
#include "zelda3/dungeon/custom_object.h"
#include "zelda3/dungeon/draw_routines/draw_routine_registry.h"

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

}  // namespace
}  // namespace resources
}  // namespace cli
}  // namespace yaze
