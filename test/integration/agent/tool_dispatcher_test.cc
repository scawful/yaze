/**
 * @file tool_dispatcher_test.cc
 * @brief Integration tests for the ToolDispatcher
 *
 * Tests the ToolDispatcher's ability to route tool calls to the appropriate
 * handlers, manage tool preferences, and handle errors gracefully.
 */

#include "cli/service/agent/tool_dispatcher.h"

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include <filesystem>
#include <fstream>

#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "cli/service/ai/common.h"
#include "mocks/mock_rom.h"

namespace yaze {
namespace cli {
namespace agent {
namespace {

using ::testing::HasSubstr;
using ::testing::Not;

// Test fixture for ToolDispatcher tests
class ToolDispatcherTest : public ::testing::Test {
 protected:
  void SetUp() override {
    // Create test directories and files for filesystem tests
    test_dir_ = std::filesystem::temp_directory_path() / "yaze_dispatcher_test";
    std::filesystem::create_directories(test_dir_);

    // Create a test file
    std::ofstream(test_dir_ / "test.txt") << "Test file content for dispatcher";

    // Initialize mock ROM
    std::vector<uint8_t> test_data(1024, 0);
    auto status = mock_rom_.SetTestData(test_data);
    ASSERT_TRUE(status.ok());

    // Set up dispatcher with ROM context
    dispatcher_.SetRomContext(&mock_rom_);
  }

  void TearDown() override {
    // Clean up test directory
    std::filesystem::remove_all(test_dir_);
  }

  ToolCall CreateToolCall(const std::string& name,
                          const std::map<std::string, std::string>& args = {}) {
    ToolCall call;
    call.tool_name = name;
    call.args = args;
    return call;
  }

  std::filesystem::path test_dir_;
  yaze::test::MockRom mock_rom_;
  ToolDispatcher dispatcher_;
};

// =============================================================================
// Filesystem Tool Dispatch Tests
// =============================================================================

TEST_F(ToolDispatcherTest, FilesystemListDispatch) {
  auto call = CreateToolCall("filesystem-list", {
      {"path", test_dir_.string()}
  });

  auto result = dispatcher_.Dispatch(call);

  // Should succeed
  EXPECT_TRUE(result.ok()) << result.status().message();

  // Result should contain JSON output
  if (result.ok()) {
    EXPECT_THAT(*result, HasSubstr("test.txt"));
  }
}

TEST_F(ToolDispatcherTest, FilesystemReadDispatch) {
  auto call = CreateToolCall("filesystem-read", {
      {"path", (test_dir_ / "test.txt").string()}
  });

  auto result = dispatcher_.Dispatch(call);

  EXPECT_TRUE(result.ok()) << result.status().message();

  if (result.ok()) {
    EXPECT_THAT(*result, HasSubstr("Test file content"));
  }
}

TEST_F(ToolDispatcherTest, FilesystemExistsDispatch) {
  auto call = CreateToolCall("filesystem-exists", {
      {"path", (test_dir_ / "test.txt").string()}
  });

  auto result = dispatcher_.Dispatch(call);

  EXPECT_TRUE(result.ok()) << result.status().message();
}

TEST_F(ToolDispatcherTest, FilesystemInfoDispatch) {
  auto call = CreateToolCall("filesystem-info", {
      {"path", (test_dir_ / "test.txt").string()}
  });

  auto result = dispatcher_.Dispatch(call);

  EXPECT_TRUE(result.ok()) << result.status().message();
}

// =============================================================================
// Build Tool Dispatch Tests (Placeholder - Build tools may not be fully implemented)
// =============================================================================

TEST_F(ToolDispatcherTest, BuildStatusDispatch) {
  auto call = CreateToolCall("build-status", {});

  auto result = dispatcher_.Dispatch(call);

  // Build tools may not be fully implemented yet
  // This test verifies the dispatch routing works
  // It may fail with "handler not implemented" which is acceptable
  if (!result.ok()) {
    EXPECT_TRUE(absl::IsInternal(result.status()) ||
                absl::IsUnimplemented(result.status()))
        << "Unexpected error: " << result.status().message();
  }
}

TEST_F(ToolDispatcherTest, BuildConfigureDispatch) {
  auto call = CreateToolCall("build-configure", {
      {"preset", "mac-dbg"}
  });

  auto result = dispatcher_.Dispatch(call);

  // Build tools may not be fully implemented yet
  if (!result.ok()) {
    EXPECT_TRUE(absl::IsInternal(result.status()) ||
                absl::IsUnimplemented(result.status()))
        << "Unexpected error: " << result.status().message();
  }
}

// =============================================================================
// Tool Preferences Tests
// =============================================================================

TEST_F(ToolDispatcherTest, ToolPreferencesDisableFilesystem) {
  ToolDispatcher::ToolPreferences prefs;
  prefs.filesystem = false;

  dispatcher_.SetToolPreferences(prefs);

  auto call = CreateToolCall("filesystem-list", {
      {"path", test_dir_.string()}
  });

  auto result = dispatcher_.Dispatch(call);

  EXPECT_FALSE(result.ok());
  EXPECT_TRUE(absl::IsFailedPrecondition(result.status()))
      << "Expected FailedPrecondition when tool is disabled, got: "
      << result.status().message();
}

TEST_F(ToolDispatcherTest, ToolPreferencesDisableBuild) {
  ToolDispatcher::ToolPreferences prefs;
  prefs.build = false;

  dispatcher_.SetToolPreferences(prefs);

  auto call = CreateToolCall("build-status", {});

  auto result = dispatcher_.Dispatch(call);

  EXPECT_FALSE(result.ok());
  EXPECT_TRUE(absl::IsFailedPrecondition(result.status()))
      << "Expected FailedPrecondition when tool is disabled, got: "
      << result.status().message();
}

TEST_F(ToolDispatcherTest, ToolPreferencesDisableDungeon) {
  ToolDispatcher::ToolPreferences prefs;
  prefs.dungeon = false;

  dispatcher_.SetToolPreferences(prefs);

  auto call = CreateToolCall("dungeon-describe-room", {
      {"room", "0"}
  });

  auto result = dispatcher_.Dispatch(call);

  EXPECT_FALSE(result.ok());
  EXPECT_TRUE(absl::IsFailedPrecondition(result.status()))
      << "Expected FailedPrecondition when tool is disabled, got: "
      << result.status().message();
}

TEST_F(ToolDispatcherTest, ToolPreferencesDisableOverworld) {
  ToolDispatcher::ToolPreferences prefs;
  prefs.overworld = false;

  dispatcher_.SetToolPreferences(prefs);

  auto call = CreateToolCall("overworld-describe-map", {
      {"map", "0"}
  });

  auto result = dispatcher_.Dispatch(call);

  EXPECT_FALSE(result.ok());
  EXPECT_TRUE(absl::IsFailedPrecondition(result.status()))
      << "Expected FailedPrecondition when tool is disabled, got: "
      << result.status().message();
}

TEST_F(ToolDispatcherTest, ToolPreferencesEnableMultipleCategories) {
  ToolDispatcher::ToolPreferences prefs;
  prefs.filesystem = true;
  prefs.build = true;
  prefs.dungeon = false;
  prefs.overworld = false;

  dispatcher_.SetToolPreferences(prefs);

  // Filesystem should work
  auto fs_call = CreateToolCall("filesystem-exists", {
      {"path", test_dir_.string()}
  });
  auto fs_result = dispatcher_.Dispatch(fs_call);
  EXPECT_TRUE(fs_result.ok()) << fs_result.status().message();

  // Dungeon should be disabled
  auto dungeon_call = CreateToolCall("dungeon-describe-room", {
      {"room", "0"}
  });
  auto dungeon_result = dispatcher_.Dispatch(dungeon_call);
  EXPECT_FALSE(dungeon_result.ok());
  EXPECT_TRUE(absl::IsFailedPrecondition(dungeon_result.status()));
}

TEST_F(ToolDispatcherTest, GetToolPreferencesReturnsSetPreferences) {
  ToolDispatcher::ToolPreferences prefs;
  prefs.filesystem = false;
  prefs.build = true;
  prefs.dungeon = false;
  prefs.overworld = true;

  dispatcher_.SetToolPreferences(prefs);

  const auto& retrieved_prefs = dispatcher_.preferences();
  EXPECT_FALSE(retrieved_prefs.filesystem);
  EXPECT_TRUE(retrieved_prefs.build);
  EXPECT_FALSE(retrieved_prefs.dungeon);
  EXPECT_TRUE(retrieved_prefs.overworld);
}

// =============================================================================
// Error Handling Tests
// =============================================================================

TEST_F(ToolDispatcherTest, InvalidToolCallReturnsError) {
  auto call = CreateToolCall("invalid-nonexistent-tool", {});

  auto result = dispatcher_.Dispatch(call);

  EXPECT_FALSE(result.ok());
  EXPECT_TRUE(absl::IsInvalidArgument(result.status()))
      << "Expected InvalidArgument for unknown tool, got: "
      << result.status().message();
}

TEST_F(ToolDispatcherTest, EmptyToolNameReturnsError) {
  auto call = CreateToolCall("", {});

  auto result = dispatcher_.Dispatch(call);

  EXPECT_FALSE(result.ok());
  EXPECT_TRUE(absl::IsInvalidArgument(result.status()))
      << "Expected InvalidArgument for empty tool name, got: "
      << result.status().message();
}

TEST_F(ToolDispatcherTest, MissingRequiredArgumentsHandled) {
  // Try to call a tool that requires arguments without providing them
  auto call = CreateToolCall("filesystem-read", {});  // Missing --path

  auto result = dispatcher_.Dispatch(call);

  // Should fail due to missing required argument
  EXPECT_FALSE(result.ok())
      << "Expected error for missing required argument";
}

TEST_F(ToolDispatcherTest, InvalidArgumentValuesHandled) {
  auto call = CreateToolCall("filesystem-read", {
      {"path", "/definitely/nonexistent/path/to/file.txt"}
  });

  auto result = dispatcher_.Dispatch(call);

  // Should fail due to nonexistent path (security validation or not found)
  EXPECT_FALSE(result.ok());
}

// =============================================================================
// ROM Context Tests
// =============================================================================

TEST_F(ToolDispatcherTest, DispatchWithoutRomContextFails) {
  ToolDispatcher dispatcher;  // No ROM context set

  auto call = CreateToolCall("dungeon-describe-room", {
      {"room", "0"}
  });

  auto result = dispatcher.Dispatch(call);

  EXPECT_FALSE(result.ok());
  EXPECT_TRUE(absl::IsFailedPrecondition(result.status()))
      << "Expected FailedPrecondition without ROM context, got: "
      << result.status().message();
}

TEST_F(ToolDispatcherTest, FilesystemToolsWorkWithoutRomData) {
  // Create a new dispatcher with ROM set but not loaded with real data
  ToolDispatcher dispatcher;
  dispatcher.SetRomContext(&mock_rom_);

  auto call = CreateToolCall("filesystem-exists", {
      {"path", test_dir_.string()}
  });

  auto result = dispatcher.Dispatch(call);

  // Filesystem tools don't need actual ROM data
  EXPECT_TRUE(result.ok()) << result.status().message();
}

// =============================================================================
// Tool Name Resolution Tests
// =============================================================================

TEST_F(ToolDispatcherTest, ResourceListToolResolves) {
  auto call = CreateToolCall("resource-list", {
      {"type", "dungeon"}
  });

  // This test verifies the tool name resolves correctly
  // The actual execution might fail if labels aren't loaded
  auto result = dispatcher_.Dispatch(call);
  // We just verify it's not "unknown tool"
  EXPECT_FALSE(absl::IsInvalidArgument(result.status()) &&
               result.status().message().find("Unknown tool") != std::string::npos)
      << "resource-list should be a known tool";
}

TEST_F(ToolDispatcherTest, GuiToolResolves) {
  auto call = CreateToolCall("gui-discover-tool", {});

  auto result = dispatcher_.Dispatch(call);

  // GUI tools should resolve, even if execution fails
  EXPECT_FALSE(absl::IsInvalidArgument(result.status()) &&
               result.status().message().find("Unknown tool") != std::string::npos)
      << "gui-discover-tool should be a known tool";
}

TEST_F(ToolDispatcherTest, MessageToolResolves) {
  auto call = CreateToolCall("message-list", {});

  auto result = dispatcher_.Dispatch(call);

  // Message tools should resolve
  EXPECT_FALSE(absl::IsInvalidArgument(result.status()) &&
               result.status().message().find("Unknown tool") != std::string::npos)
      << "message-list should be a known tool";
}

TEST_F(ToolDispatcherTest, MusicToolResolves) {
  auto call = CreateToolCall("music-list", {});

  auto result = dispatcher_.Dispatch(call);

  // Music tools should resolve
  EXPECT_FALSE(absl::IsInvalidArgument(result.status()) &&
               result.status().message().find("Unknown tool") != std::string::npos)
      << "music-list should be a known tool";
}

TEST_F(ToolDispatcherTest, SpriteToolResolves) {
  auto call = CreateToolCall("sprite-list", {});

  auto result = dispatcher_.Dispatch(call);

  // Sprite tools should resolve
  EXPECT_FALSE(absl::IsInvalidArgument(result.status()) &&
               result.status().message().find("Unknown tool") != std::string::npos)
      << "sprite-list should be a known tool";
}

// =============================================================================
// Default Preferences Tests
// =============================================================================

TEST_F(ToolDispatcherTest, DefaultPreferencesEnableAllBasicTools) {
  ToolDispatcher dispatcher;
  const auto& prefs = dispatcher.preferences();

  EXPECT_TRUE(prefs.resources);
  EXPECT_TRUE(prefs.dungeon);
  EXPECT_TRUE(prefs.overworld);
  EXPECT_TRUE(prefs.messages);
  EXPECT_TRUE(prefs.dialogue);
  EXPECT_TRUE(prefs.gui);
  EXPECT_TRUE(prefs.music);
  EXPECT_TRUE(prefs.sprite);
  EXPECT_TRUE(prefs.filesystem);
  EXPECT_TRUE(prefs.build);
}

#ifndef YAZE_WITH_GRPC
TEST_F(ToolDispatcherTest, EmulatorDisabledWithoutGrpc) {
  ToolDispatcher dispatcher;
  const auto& prefs = dispatcher.preferences();

  // Emulator should be disabled when GRPC is not available
  EXPECT_FALSE(prefs.emulator);
}
#endif

#ifdef YAZE_WITH_GRPC
TEST_F(ToolDispatcherTest, EmulatorEnabledWithGrpc) {
  ToolDispatcher dispatcher;
  const auto& prefs = dispatcher.preferences();

  // Emulator should be enabled when GRPC is available
  EXPECT_TRUE(prefs.emulator);
}
#endif

// =============================================================================
// JSON Output Format Tests
// =============================================================================

TEST_F(ToolDispatcherTest, OutputIsValidJson) {
  auto call = CreateToolCall("filesystem-exists", {
      {"path", test_dir_.string()}
  });

  auto result = dispatcher_.Dispatch(call);

  ASSERT_TRUE(result.ok()) << result.status().message();

  // Basic JSON structure validation
  std::string output = *result;
  // Output should contain JSON-like structure (braces or brackets)
  EXPECT_TRUE(output.find('{') != std::string::npos ||
              output.find('[') != std::string::npos)
      << "Expected JSON output, got: " << output;
}

}  // namespace
}  // namespace agent
}  // namespace cli
}  // namespace yaze
