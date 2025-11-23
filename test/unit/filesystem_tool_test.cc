#include "cli/service/agent/tools/filesystem_tool.h"

#include <gtest/gtest.h>

#include "app/rom.h"
#include "cli/service/resources/command_context.h"

namespace yaze {
namespace cli {
namespace agent {
namespace tools {
namespace {

// Test fixture for FileSystemTool tests
class FileSystemToolTest : public ::testing::Test {
 protected:
  void SetUp() override {
    // Create test directories and files
    test_dir_ = std::filesystem::temp_directory_path() / "yaze_test";
    std::filesystem::create_directories(test_dir_ / "subdir");

    // Create test files
    std::ofstream(test_dir_ / "test.txt") << "Hello, World!";
    std::ofstream(test_dir_ / "subdir" / "nested.txt") << "Nested file content";
  }

  void TearDown() override {
    // Clean up test directory
    std::filesystem::remove_all(test_dir_);
  }

  std::filesystem::path test_dir_;
};

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

  // Create a multi-line file
  std::ofstream multiline_file(test_dir_ / "multiline.txt");
  for (int i = 0; i < 10; ++i) {
    multiline_file << "Line " << i << "\n";
  }
  multiline_file.close();

  std::vector<std::string> args = {
      "--path=" + (test_dir_ / "multiline.txt").string(),
      "--lines=5",
      "--format=json"
  };

  absl::Status status = tool.Run(args, nullptr);
  EXPECT_TRUE(status.ok()) << status.message();
}

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

}  // namespace
}  // namespace tools
}  // namespace agent
}  // namespace cli
}  // namespace yaze