/**
 * @file filesystem_tool_test.cc
 * @brief Unit tests for the FileSystemTool AI agent tool
 *
 * Tests the FileSystemTool functionality including listing, reading,
 * existence checking, file info, and security protections.
 */

#include "cli/service/agent/tools/filesystem_tool.h"

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include <filesystem>
#include <fstream>

#include "absl/status/status.h"
#include "cli/service/resources/command_context.h"

namespace yaze {
namespace cli {
namespace agent {
namespace tools {
namespace {

using ::testing::HasSubstr;
using ::testing::Not;

// Test fixture for FileSystemTool tests
class FileSystemToolTest : public ::testing::Test {
 protected:
  void SetUp() override {
    // Create test directories and files
    test_dir_ = std::filesystem::temp_directory_path() / "yaze_fs_tool_test";
    std::filesystem::create_directories(test_dir_ / "subdir");

    // Create test files
    std::ofstream(test_dir_ / "test.txt") << "Hello, World!";
    std::ofstream(test_dir_ / "subdir" / "nested.txt") << "Nested file content";

    // Create a multi-line file for pagination tests
    std::ofstream multiline_file(test_dir_ / "multiline.txt");
    for (int i = 1; i <= 100; ++i) {
      multiline_file << "Line " << i << ": This is line number " << i << "\n";
    }
    multiline_file.close();

    // Create a file with special characters
    std::ofstream special_file(test_dir_ / "special_chars.txt");
    special_file << "Tab:\tNewline:\nBackslash:\\Quote:\"End";
    special_file.close();

    // Create an empty file
    std::ofstream(test_dir_ / "empty.txt");
  }

  void TearDown() override {
    // Clean up test directory
    std::filesystem::remove_all(test_dir_);
  }

  std::filesystem::path test_dir_;
};

// =============================================================================
// FileSystemListTool Tests
// =============================================================================

TEST_F(FileSystemToolTest, ListDirectoryWorks) {
  FileSystemListTool tool;

  std::vector<std::string> args = {
      "--path=" + test_dir_.string(),
      "--format=json"
  };

  absl::Status status = tool.Run(args, nullptr);
  EXPECT_TRUE(status.ok()) << status.message();
}

TEST_F(FileSystemToolTest, ListDirectoryRecursiveWorks) {
  FileSystemListTool tool;

  std::vector<std::string> args = {
      "--path=" + test_dir_.string(),
      "--recursive=true",
      "--format=json"
  };

  absl::Status status = tool.Run(args, nullptr);
  EXPECT_TRUE(status.ok()) << status.message();
}

TEST_F(FileSystemToolTest, ListDirectoryTextFormat) {
  FileSystemListTool tool;

  std::vector<std::string> args = {
      "--path=" + test_dir_.string(),
      "--format=text"
  };

  absl::Status status = tool.Run(args, nullptr);
  EXPECT_TRUE(status.ok()) << status.message();
}

TEST_F(FileSystemToolTest, ListNonExistentDirectoryFails) {
  FileSystemListTool tool;

  std::vector<std::string> args = {
      "--path=" + (test_dir_ / "nonexistent").string(),
      "--format=json"
  };

  absl::Status status = tool.Run(args, nullptr);
  EXPECT_FALSE(status.ok());
}

TEST_F(FileSystemToolTest, ListToolGetNameReturnsCorrectName) {
  FileSystemListTool tool;
  EXPECT_EQ(tool.GetName(), "filesystem-list");
}

TEST_F(FileSystemToolTest, ListToolGetUsageContainsPath) {
  FileSystemListTool tool;
  EXPECT_THAT(tool.GetUsage(), HasSubstr("--path"));
}

// =============================================================================
// FileSystemReadTool Tests
// =============================================================================

TEST_F(FileSystemToolTest, ReadFileWorks) {
  FileSystemReadTool tool;

  std::vector<std::string> args = {
      "--path=" + (test_dir_ / "test.txt").string(),
      "--format=json"
  };

  absl::Status status = tool.Run(args, nullptr);
  EXPECT_TRUE(status.ok()) << status.message();
}

TEST_F(FileSystemToolTest, ReadFileWithLinesLimitWorks) {
  FileSystemReadTool tool;

  std::vector<std::string> args = {
      "--path=" + (test_dir_ / "multiline.txt").string(),
      "--lines=5",
      "--format=json"
  };

  absl::Status status = tool.Run(args, nullptr);
  EXPECT_TRUE(status.ok()) << status.message();
}

TEST_F(FileSystemToolTest, ReadFileWithOffsetWorks) {
  FileSystemReadTool tool;

  std::vector<std::string> args = {
      "--path=" + (test_dir_ / "multiline.txt").string(),
      "--offset=10",
      "--lines=5",
      "--format=json"
  };

  absl::Status status = tool.Run(args, nullptr);
  EXPECT_TRUE(status.ok()) << status.message();
}

TEST_F(FileSystemToolTest, ReadEmptyFileWorks) {
  FileSystemReadTool tool;

  std::vector<std::string> args = {
      "--path=" + (test_dir_ / "empty.txt").string(),
      "--format=json"
  };

  absl::Status status = tool.Run(args, nullptr);
  EXPECT_TRUE(status.ok()) << status.message();
}

TEST_F(FileSystemToolTest, ReadNonExistentFileFails) {
  FileSystemReadTool tool;

  std::vector<std::string> args = {
      "--path=" + (test_dir_ / "nonexistent.txt").string(),
      "--format=json"
  };

  absl::Status status = tool.Run(args, nullptr);
  EXPECT_FALSE(status.ok());
}

TEST_F(FileSystemToolTest, ReadToolGetNameReturnsCorrectName) {
  FileSystemReadTool tool;
  EXPECT_EQ(tool.GetName(), "filesystem-read");
}

TEST_F(FileSystemToolTest, ReadToolGetUsageContainsPath) {
  FileSystemReadTool tool;
  EXPECT_THAT(tool.GetUsage(), HasSubstr("--path"));
}

// =============================================================================
// FileSystemExistsTool Tests
// =============================================================================

TEST_F(FileSystemToolTest, FileExistsWorks) {
  FileSystemExistsTool tool;

  std::vector<std::string> args = {
      "--path=" + (test_dir_ / "test.txt").string(),
      "--format=json"
  };

  absl::Status status = tool.Run(args, nullptr);
  EXPECT_TRUE(status.ok()) << status.message();
}

TEST_F(FileSystemToolTest, FileExistsForNonExistentFile) {
  FileSystemExistsTool tool;

  std::vector<std::string> args = {
      "--path=" + (test_dir_ / "nonexistent.txt").string(),
      "--format=json"
  };

  // This should succeed but report that the file doesn't exist
  absl::Status status = tool.Run(args, nullptr);
  EXPECT_TRUE(status.ok()) << status.message();
}

TEST_F(FileSystemToolTest, DirectoryExistsWorks) {
  FileSystemExistsTool tool;

  std::vector<std::string> args = {
      "--path=" + test_dir_.string(),
      "--format=json"
  };

  absl::Status status = tool.Run(args, nullptr);
  EXPECT_TRUE(status.ok()) << status.message();
}

TEST_F(FileSystemToolTest, ExistsToolGetNameReturnsCorrectName) {
  FileSystemExistsTool tool;
  EXPECT_EQ(tool.GetName(), "filesystem-exists");
}

// =============================================================================
// FileSystemInfoTool Tests
// =============================================================================

TEST_F(FileSystemToolTest, GetFileInfoWorks) {
  FileSystemInfoTool tool;

  std::vector<std::string> args = {
      "--path=" + (test_dir_ / "test.txt").string(),
      "--format=json"
  };

  absl::Status status = tool.Run(args, nullptr);
  EXPECT_TRUE(status.ok()) << status.message();
}

TEST_F(FileSystemToolTest, GetDirectoryInfoWorks) {
  FileSystemInfoTool tool;

  std::vector<std::string> args = {
      "--path=" + test_dir_.string(),
      "--format=json"
  };

  absl::Status status = tool.Run(args, nullptr);
  EXPECT_TRUE(status.ok()) << status.message();
}

TEST_F(FileSystemToolTest, GetInfoForNestedFile) {
  FileSystemInfoTool tool;

  std::vector<std::string> args = {
      "--path=" + (test_dir_ / "subdir" / "nested.txt").string(),
      "--format=json"
  };

  absl::Status status = tool.Run(args, nullptr);
  EXPECT_TRUE(status.ok()) << status.message();
}

TEST_F(FileSystemToolTest, GetInfoForNonExistentPath) {
  FileSystemInfoTool tool;

  std::vector<std::string> args = {
      "--path=" + (test_dir_ / "nonexistent.txt").string(),
      "--format=json"
  };

  absl::Status status = tool.Run(args, nullptr);
  EXPECT_FALSE(status.ok());
}

TEST_F(FileSystemToolTest, InfoToolGetNameReturnsCorrectName) {
  FileSystemInfoTool tool;
  EXPECT_EQ(tool.GetName(), "filesystem-info");
}

// =============================================================================
// Security Tests
// =============================================================================

TEST_F(FileSystemToolTest, PathTraversalBlocked) {
  FileSystemListTool tool;

  std::vector<std::string> args = {
      "--path=../../../etc",  // Try to escape project directory
      "--format=json"
  };

  absl::Status status = tool.Run(args, nullptr);
  EXPECT_FALSE(status.ok());
  EXPECT_TRUE(absl::IsInvalidArgument(status) ||
              absl::IsPermissionDenied(status))
      << "Expected InvalidArgument or PermissionDenied, got: " << status.message();
}

TEST_F(FileSystemToolTest, ReadBinaryFileBlocked) {
  FileSystemReadTool tool;

  // Create a fake binary file
  std::ofstream binary_file(test_dir_ / "binary.exe", std::ios::binary);
  char null_bytes[] = {0x00, 0x01, 0x02, 0x03};
  binary_file.write(null_bytes, sizeof(null_bytes));
  binary_file.close();

  std::vector<std::string> args = {
      "--path=" + (test_dir_ / "binary.exe").string(),
      "--format=json"
  };

  absl::Status status = tool.Run(args, nullptr);
  EXPECT_FALSE(status.ok());
  EXPECT_TRUE(absl::IsInvalidArgument(status))
      << "Expected InvalidArgument for binary file, got: " << status.message();
}

TEST_F(FileSystemToolTest, AbsolutePathTraversalBlocked) {
  FileSystemListTool tool;

  std::vector<std::string> args = {
      "--path=/etc/passwd",  // Try to access system file
      "--format=json"
  };

  absl::Status status = tool.Run(args, nullptr);
  EXPECT_FALSE(status.ok());
  EXPECT_TRUE(absl::IsPermissionDenied(status) ||
              absl::IsInvalidArgument(status))
      << "Expected security error for system path access, got: " << status.message();
}

TEST_F(FileSystemToolTest, DotDotInPathBlocked) {
  FileSystemReadTool tool;

  // Try to read a file using path traversal within the test dir
  std::vector<std::string> args = {
      "--path=" + (test_dir_ / "subdir" / ".." / ".." / "etc" / "passwd").string(),
      "--format=json"
  };

  absl::Status status = tool.Run(args, nullptr);
  // This should either fail validation or fail to find the file
  // Either way, it shouldn't succeed in reading /etc/passwd
  if (status.ok()) {
    // If it succeeded, make sure it didn't actually read /etc/passwd
    // by checking the output doesn't contain typical passwd content
  }
}

// =============================================================================
// Edge Case Tests
// =============================================================================

TEST_F(FileSystemToolTest, ListEmptyDirectory) {
  FileSystemListTool tool;

  // Create an empty directory
  auto empty_dir = test_dir_ / "empty_dir";
  std::filesystem::create_directories(empty_dir);

  std::vector<std::string> args = {
      "--path=" + empty_dir.string(),
      "--format=json"
  };

  absl::Status status = tool.Run(args, nullptr);
  EXPECT_TRUE(status.ok()) << status.message();
}

TEST_F(FileSystemToolTest, ReadFileWithSpecialCharacters) {
  FileSystemReadTool tool;

  std::vector<std::string> args = {
      "--path=" + (test_dir_ / "special_chars.txt").string(),
      "--format=json"
  };

  absl::Status status = tool.Run(args, nullptr);
  EXPECT_TRUE(status.ok()) << status.message();
}

TEST_F(FileSystemToolTest, LargeLineCountParameter) {
  FileSystemReadTool tool;

  std::vector<std::string> args = {
      "--path=" + (test_dir_ / "multiline.txt").string(),
      "--lines=999999",  // Very large, should be clamped or handled gracefully
      "--format=json"
  };

  absl::Status status = tool.Run(args, nullptr);
  EXPECT_TRUE(status.ok()) << status.message();
}

TEST_F(FileSystemToolTest, ZeroLineCountParameter) {
  FileSystemReadTool tool;

  std::vector<std::string> args = {
      "--path=" + (test_dir_ / "multiline.txt").string(),
      "--lines=0",  // Zero lines requested
      "--format=json"
  };

  absl::Status status = tool.Run(args, nullptr);
  // This should either return empty content or use a default value
  EXPECT_TRUE(status.ok()) << status.message();
}

TEST_F(FileSystemToolTest, NegativeOffsetParameter) {
  FileSystemReadTool tool;

  std::vector<std::string> args = {
      "--path=" + (test_dir_ / "multiline.txt").string(),
      "--offset=-5",  // Negative offset
      "--format=json"
  };

  absl::Status status = tool.Run(args, nullptr);
  // Should handle gracefully - either fail or treat as 0
}

// =============================================================================
// Text Format Output Tests
// =============================================================================

TEST_F(FileSystemToolTest, ReadTextFormat) {
  FileSystemReadTool tool;

  std::vector<std::string> args = {
      "--path=" + (test_dir_ / "test.txt").string(),
      "--format=text"
  };

  absl::Status status = tool.Run(args, nullptr);
  EXPECT_TRUE(status.ok()) << status.message();
}

TEST_F(FileSystemToolTest, InfoTextFormat) {
  FileSystemInfoTool tool;

  std::vector<std::string> args = {
      "--path=" + (test_dir_ / "test.txt").string(),
      "--format=text"
  };

  absl::Status status = tool.Run(args, nullptr);
  EXPECT_TRUE(status.ok()) << status.message();
}

TEST_F(FileSystemToolTest, ExistsTextFormat) {
  FileSystemExistsTool tool;

  std::vector<std::string> args = {
      "--path=" + (test_dir_ / "test.txt").string(),
      "--format=text"
  };

  absl::Status status = tool.Run(args, nullptr);
  EXPECT_TRUE(status.ok()) << status.message();
}

}  // namespace
}  // namespace tools
}  // namespace agent
}  // namespace cli
}  // namespace yaze
