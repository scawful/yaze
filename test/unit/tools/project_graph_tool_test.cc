#include "cli/service/agent/tools/project_graph_tool.h"

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include <chrono>
#include <filesystem>
#include <fstream>
#include <map>
#include <string>

#include "absl/status/status.h"
#include "core/project.h"

namespace yaze::cli::agent::tools {
namespace {

using ::testing::HasSubstr;
using ::testing::Not;

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
    fs::create_directories(test_dir_ / "out");

    std::ofstream(test_dir_ / "a.txt") << "hello";

    project_.name = "TestProject";
    project_.metadata.description = "Test project description";
    project_.code_folder = test_dir_.string();
    project_.output_folder = (test_dir_ / "out").string();
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
  absl::Status status =
      tool.Run({"--query=info", "--format=json"}, nullptr, &output);
  EXPECT_TRUE(status.ok()) << status;
  EXPECT_THAT(output, HasSubstr("\"name\""));
  EXPECT_THAT(output, HasSubstr("TestProject"));
  EXPECT_THAT(output, HasSubstr(test_dir_.string()));
}

TEST_F(ProjectGraphToolTest, FilesQueryListsDirectoryEntries) {
  ProjectGraphTool tool;
  tool.SetProjectContext(&project_);

  std::string output;
  absl::Status status =
      tool.Run({"--query=files", "--format=json"}, nullptr, &output);
  EXPECT_TRUE(status.ok()) << status;
  EXPECT_THAT(output, HasSubstr("\"files\""));
  EXPECT_THAT(output, HasSubstr("a.txt"));
  EXPECT_THAT(output, HasSubstr("subdir"));
}

TEST_F(ProjectGraphToolTest, FilesQueryFailsForMissingPath) {
  ProjectGraphTool tool;
  tool.SetProjectContext(&project_);

  absl::Status status =
      tool.Run({"--query=files", "--path=does_not_exist"}, nullptr);
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

TEST_F(ProjectGraphToolTest,
       SymbolsQueryIncludesSourceLocationsWhenArtifactsExist) {
  ProjectGraphTool tool;
  tool.SetProjectContext(&project_);

  std::map<std::string, core::AsarSymbol> symbols;
  symbols["Start"] = {.name = "Start", .address = 0x008000};
  tool.SetAssemblySymbolTable(&symbols);

  std::ofstream(project_.GetZ3dkArtifactPath("sourcemap.json"))
      << R"({"version":1,"files":[{"id":1,"path":"asm/main.asm"}],"entries":[{"address":"0x008000","file_id":1,"line":12}]})";

  std::string output;
  absl::Status status =
      tool.Run({"--query=symbols", "--format=json"}, nullptr, &output);
  EXPECT_TRUE(status.ok()) << status;
  EXPECT_THAT(output, HasSubstr("asm/main.asm"));
  EXPECT_THAT(output, HasSubstr("\"line\": 12"));
}

TEST_F(ProjectGraphToolTest, LookupQueryResolvesSymbolToSourceLocation) {
  ProjectGraphTool tool;
  tool.SetProjectContext(&project_);

  std::ofstream(project_.GetZ3dkArtifactPath("symbols.mlb"))
      << "PRG:008000:Start\n";
  std::ofstream(project_.GetZ3dkArtifactPath("sourcemap.json"))
      << R"({"version":1,"files":[{"id":7,"path":"asm/main.asm"}],"entries":[{"address":"0x008000","file_id":7,"line":33}]})";

  std::string output;
  absl::Status status = tool.Run(
      {"--query=lookup", "--symbol=Start", "--format=json"}, nullptr, &output);
  EXPECT_TRUE(status.ok()) << status;
  EXPECT_THAT(output, HasSubstr("\"address\": \"$008000\""));
  EXPECT_THAT(output, HasSubstr("asm/main.asm"));
}

TEST_F(ProjectGraphToolTest, WritesQueryReturnsCoverageSummary) {
  ProjectGraphTool tool;
  tool.SetProjectContext(&project_);

  std::ofstream(project_.GetZ3dkArtifactPath("hooks.json"))
      << R"({"version":1,"hooks":[{"address":"0x808000","size":4,"kind":"hook","name":"Start","source":"asm/main.asm:12"},{"address":"0x1E9000","size":12,"kind":"patch","name":"Expanded","source":"asm/expanded.asm:4"}]})";

  std::string output;
  absl::Status status =
      tool.Run({"--query=writes", "--format=json"}, nullptr, &output);
  EXPECT_TRUE(status.ok()) << status;
  EXPECT_THAT(output, HasSubstr("\"address\": \"$808000\""));
  EXPECT_THAT(output, HasSubstr("\"bank\": \"$00\""));
  EXPECT_THAT(output, HasSubstr("\"bytes_written\": 4"));
  EXPECT_THAT(output, HasSubstr("\"bytes_written\": 12"));
  EXPECT_THAT(output, HasSubstr("asm/expanded.asm:4"));
}

TEST_F(ProjectGraphToolTest, BankQueryReturnsDisasmAndFilteredArtifacts) {
  ProjectGraphTool tool;
  tool.SetProjectContext(&project_);

  std::filesystem::create_directories(project_.GetZ3dkArtifactPath("z3disasm"));
  std::ofstream(project_.GetZ3dkArtifactPath("z3disasm") + "/bank_00.asm")
      << "; bank 00\n";
  std::ofstream(project_.GetZ3dkArtifactPath("sourcemap.json"))
      << R"({"version":1,"files":[{"id":1,"path":"asm/main.asm"},{"id":2,"path":"asm/other.asm"}],"entries":[{"address":"0x808000","file_id":1,"line":12},{"address":"0x018000","file_id":2,"line":44}]})";
  std::ofstream(project_.GetZ3dkArtifactPath("hooks.json"))
      << R"({"version":1,"hooks":[{"address":"0x008100","size":4,"kind":"hook","name":"Start","source":"asm/main.asm:12"},{"address":"0x1E9000","size":12,"kind":"patch","name":"Expanded","source":"asm/expanded.asm:4"}]})";

  std::string output;
  absl::Status status = tool.Run({"--query=bank", "--bank=00", "--format=json"},
                                 nullptr, &output);
  EXPECT_TRUE(status.ok()) << status;
  EXPECT_THAT(output, HasSubstr("bank_00.asm"));
  EXPECT_THAT(output, HasSubstr("asm/main.asm"));
  EXPECT_THAT(output, HasSubstr("\"disasm_file_exists\": true"));
  EXPECT_THAT(output, Not(HasSubstr("asm/other.asm")));
  EXPECT_THAT(output, Not(HasSubstr("asm/expanded.asm")));
}

TEST_F(ProjectGraphToolTest, UnknownQueryTypeIsInvalidArgument) {
  ProjectGraphTool tool;
  tool.SetProjectContext(&project_);

  absl::Status status = tool.Run({"--query=bogus"}, nullptr);
  EXPECT_FALSE(status.ok());
  EXPECT_EQ(status.code(), absl::StatusCode::kInvalidArgument);
}

}  // namespace
}  // namespace yaze::cli::agent::tools
