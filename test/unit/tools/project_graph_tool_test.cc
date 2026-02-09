#include "cli/service/agent/tools/project_graph_tool.h"

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include <chrono>
#include <filesystem>
#include <fstream>
#include <string>

#include "absl/status/status.h"
#include "core/project.h"

namespace yaze::cli::agent::tools {
namespace {

using ::testing::HasSubstr;

namespace fs = std::filesystem;

fs::path MakeUniqueTempDir(const std::string& prefix) {
  const auto now = std::chrono::steady_clock::now().time_since_epoch().count();
  return fs::temp_directory_path() / (prefix + "_" + std::to_string(now));
}

class ProjectGraphToolTest : public ::testing::Test {
 protected:
  void SetUp() override {
    test_dir_ = MakeUniqueTempDir("yaze_project_graph_tool_test");
    fs::create_directories(test_dir_);
    fs::create_directories(test_dir_ / "subdir");

    std::ofstream(test_dir_ / "a.txt") << "hello";

    project_.name = "TestProject";
    project_.metadata.description = "Test project description";
    project_.code_folder = test_dir_.string();
  }

  void TearDown() override {
    std::error_code ec;
    fs::remove_all(test_dir_, ec);
  }

  fs::path test_dir_;
  project::YazeProject project_;
};

TEST_F(ProjectGraphToolTest, InfoQueryReturnsProjectFields) {
  ProjectGraphTool tool;
  tool.SetProjectContext(&project_);

  std::string output;
  absl::Status status = tool.Run({"--query=info", "--format=json"}, nullptr,
                                 &output);
  EXPECT_TRUE(status.ok()) << status;
  EXPECT_THAT(output, HasSubstr("\"name\""));
  EXPECT_THAT(output, HasSubstr("TestProject"));
  EXPECT_THAT(output, HasSubstr(test_dir_.string()));
}

TEST_F(ProjectGraphToolTest, FilesQueryListsDirectoryEntries) {
  ProjectGraphTool tool;
  tool.SetProjectContext(&project_);

  std::string output;
  absl::Status status = tool.Run({"--query=files", "--format=json"}, nullptr,
                                 &output);
  EXPECT_TRUE(status.ok()) << status;
  EXPECT_THAT(output, HasSubstr("\"files\""));
  EXPECT_THAT(output, HasSubstr("a.txt"));
  EXPECT_THAT(output, HasSubstr("subdir"));
}

TEST_F(ProjectGraphToolTest, FilesQueryFailsForMissingPath) {
  ProjectGraphTool tool;
  tool.SetProjectContext(&project_);

  absl::Status status = tool.Run({"--query=files", "--path=does_not_exist"},
                                 nullptr);
  EXPECT_FALSE(status.ok());
  EXPECT_EQ(status.code(), absl::StatusCode::kNotFound);
}

TEST_F(ProjectGraphToolTest, SymbolsQueryFailsWithoutAsarWrapper) {
  ProjectGraphTool tool;
  tool.SetProjectContext(&project_);

  absl::Status status = tool.Run({"--query=symbols"}, nullptr);
  EXPECT_FALSE(status.ok());
  EXPECT_EQ(status.code(), absl::StatusCode::kFailedPrecondition);
}

TEST_F(ProjectGraphToolTest, UnknownQueryTypeIsInvalidArgument) {
  ProjectGraphTool tool;
  tool.SetProjectContext(&project_);

  absl::Status status = tool.Run({"--query=bogus"}, nullptr);
  EXPECT_FALSE(status.ok());
  EXPECT_EQ(status.code(), absl::StatusCode::kInvalidArgument);
}

}  // namespace
}  // namespace

