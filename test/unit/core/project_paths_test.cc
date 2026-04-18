#include "core/project.h"

#include <chrono>
#include <filesystem>
#include <fstream>
#include <string>

#include "gtest/gtest.h"

namespace yaze::project {

namespace {

std::filesystem::path MakeUniqueTempDir(const std::string& prefix) {
  const auto now = std::chrono::steady_clock::now().time_since_epoch().count();
  return std::filesystem::temp_directory_path() /
         (prefix + "_" + std::to_string(now));
}

class ScopedTempDir {
 public:
  explicit ScopedTempDir(std::filesystem::path path) : path_(std::move(path)) {
    std::filesystem::create_directories(path_);
  }

  ~ScopedTempDir() {
    std::error_code ec;
    std::filesystem::remove_all(path_, ec);
  }

  const std::filesystem::path& path() const { return path_; }

 private:
  std::filesystem::path path_;
};

void WriteTextFile(const std::filesystem::path& path, const std::string& data) {
  std::filesystem::create_directories(path.parent_path());
  std::ofstream file(path, std::ios::out | std::ios::trunc);
  ASSERT_TRUE(file.is_open())
      << "Failed to open file for writing: " << path.string();
  file << data;
  file.close();
}

std::string ReadTextFile(const std::filesystem::path& path) {
  std::ifstream file(path);
  EXPECT_TRUE(file.is_open())
      << "Failed to open file for reading: " << path.string();
  std::string content((std::istreambuf_iterator<char>(file)),
                      std::istreambuf_iterator<char>());
  file.close();
  return content;
}

}  // namespace

TEST(ProjectPathsTest, OpenNormalizesRelativePathsToAbsolute) {
  ScopedTempDir temp(MakeUniqueTempDir("yaze_project_paths"));

  const auto rom_path = temp.path() / "roms" / "test.sfc";
  const auto code_path = temp.path() / "code";
  const auto backup_path = temp.path() / "backups";
  const auto output_path = temp.path() / "output";
  const auto labels_path = temp.path() / "labels.txt";
  const auto symbols_path = temp.path() / "symbols.txt";

  std::filesystem::create_directories(rom_path.parent_path());
  std::filesystem::create_directories(code_path);
  std::filesystem::create_directories(backup_path);
  std::filesystem::create_directories(output_path);
  WriteTextFile(rom_path, "not a real rom");

  const auto project_file = temp.path() / "TestProject.yaze";
  WriteTextFile(project_file,
                R"(
[project]
name=TestProject

[files]
rom_filename=roms/test.sfc
code_folder=code
rom_backup_folder=backups
output_folder=output
labels_filename=labels.txt
symbols_filename=symbols.txt
)");

  YazeProject project;
  ASSERT_TRUE(project.Open(project_file.string()).ok());

  EXPECT_EQ(project.rom_filename, rom_path.string());
  EXPECT_EQ(project.code_folder, code_path.string());
  EXPECT_EQ(project.rom_backup_folder, backup_path.string());
  EXPECT_EQ(project.output_folder, output_path.string());
  EXPECT_EQ(project.labels_filename, labels_path.string());
  EXPECT_EQ(project.symbols_filename, symbols_path.string());

  ASSERT_TRUE(project.Save().ok());
  const auto saved = ReadTextFile(project_file);

  // Serialized project files should remain portable (relative paths).
  EXPECT_NE(saved.find("rom_filename=roms/test.sfc"), std::string::npos);
  EXPECT_NE(saved.find("code_folder=code"), std::string::npos);
  EXPECT_NE(saved.find("rom_backup_folder=backups"), std::string::npos);
}

TEST(ProjectPathsTest, OpenLoadsZ3dkConfigWhenPresent) {
  ScopedTempDir temp(MakeUniqueTempDir("yaze_project_z3dk_paths"));

  const auto code_path = temp.path() / "code";
  const auto output_path = temp.path() / "output";
  const auto rom_path = temp.path() / "roms" / "base.sfc";
  std::filesystem::create_directories(code_path / "include");
  std::filesystem::create_directories(code_path / "asm");
  std::filesystem::create_directories(code_path / "build");
  std::filesystem::create_directories(code_path / "logs");
  std::filesystem::create_directories(code_path / "toolchain");
  std::filesystem::create_directories(output_path);
  std::filesystem::create_directories(rom_path.parent_path());
  WriteTextFile(rom_path, "not a real rom");

  WriteTextFile(code_path / "z3dk.toml",
                R"(
preset = "oracle"
include_paths = ["include", "asm"]
defines = ["FEATURE=1", "DEBUG"]
main_files = ["Main.asm", "Entry.asm"]
emit = ["build/hooks.json", "build/lint.json"]
std_includes = "toolchain/stdincludes.txt"
std_defines = "toolchain/stddefines.txt"
mapper = "lorom"
rom = "../roms/base.sfc"
rom_size = 1048576
symbols = "nocash"
symbols_path = "build/project.sym"
lsp_log_enabled = true
lsp_log_path = "logs/z3lsp.log"
warn_unused_symbols = true
warn_branch_outside_bank = false
warn_unknown_width = true
warn_org_collision = true
warn_unauthorized_hook = false
)");

  const auto project_file = temp.path() / "Z3dkProject.yaze";
  WriteTextFile(project_file,
                R"(
[project]
name=Z3dkProject

[files]
code_folder=code
output_folder=output
)");

  YazeProject project;
  ASSERT_TRUE(project.Open(project_file.string()).ok());

#ifdef YAZE_WITH_Z3DK
  ASSERT_TRUE(project.HasZ3dkConfig());
  EXPECT_EQ(project.z3dk_settings.config_path,
            (code_path / "z3dk.toml").string());
  EXPECT_EQ(project.z3dk_settings.preset, "oracle");
  ASSERT_EQ(project.z3dk_settings.include_paths.size(), 2u);
  EXPECT_EQ(project.z3dk_settings.include_paths[0],
            (code_path / "include").string());
  EXPECT_EQ(project.z3dk_settings.include_paths[1],
            (code_path / "asm").string());
  ASSERT_EQ(project.z3dk_settings.defines.size(), 2u);
  EXPECT_EQ(project.z3dk_settings.defines[0].first, "FEATURE");
  EXPECT_EQ(project.z3dk_settings.defines[0].second, "1");
  EXPECT_EQ(project.z3dk_settings.defines[1].first, "DEBUG");
  EXPECT_EQ(project.z3dk_settings.defines[1].second, "1");
  ASSERT_EQ(project.z3dk_settings.main_files.size(), 2u);
  EXPECT_EQ(project.z3dk_settings.main_files[0],
            (code_path / "Main.asm").string());
  EXPECT_EQ(project.z3dk_settings.main_files[1],
            (code_path / "Entry.asm").string());
  ASSERT_EQ(project.z3dk_settings.emits.size(), 2u);
  EXPECT_EQ(project.z3dk_settings.emits[0],
            (code_path / "build" / "hooks.json").string());
  EXPECT_EQ(project.z3dk_settings.emits[1],
            (code_path / "build" / "lint.json").string());
  EXPECT_EQ(project.z3dk_settings.std_includes_path,
            (code_path / "toolchain" / "stdincludes.txt").string());
  EXPECT_EQ(project.z3dk_settings.std_defines_path,
            (code_path / "toolchain" / "stddefines.txt").string());
  EXPECT_EQ(project.z3dk_settings.mapper, "lorom");
  EXPECT_EQ(project.z3dk_settings.rom_path, rom_path.string());
  EXPECT_EQ(project.z3dk_settings.rom_size, 1048576);
  EXPECT_EQ(project.z3dk_settings.symbols_format, "nocash");
  EXPECT_EQ(project.z3dk_settings.symbols_path,
            (code_path / "build" / "project.sym").string());
  ASSERT_TRUE(project.z3dk_settings.lsp_log_enabled.has_value());
  EXPECT_TRUE(*project.z3dk_settings.lsp_log_enabled);
  EXPECT_EQ(project.z3dk_settings.lsp_log_path,
            (code_path / "logs" / "z3lsp.log").string());
  EXPECT_EQ(project.z3dk_settings.artifact_paths.symbols_mlb,
            (output_path / "symbols.mlb").string());
  EXPECT_EQ(project.z3dk_settings.artifact_paths.sourcemap_json,
            (output_path / "sourcemap.json").string());
  EXPECT_EQ(project.z3dk_settings.artifact_paths.annotations_json,
            (output_path / "annotations.json").string());
  EXPECT_EQ(project.z3dk_settings.artifact_paths.hooks_json,
            (code_path / "build" / "hooks.json").string());
  EXPECT_EQ(project.z3dk_settings.artifact_paths.lint_json,
            (code_path / "build" / "lint.json").string());
  EXPECT_FALSE(project.z3dk_settings.warn_branch_outside_bank);
  EXPECT_FALSE(project.z3dk_settings.warn_unauthorized_hook);
#else
  EXPECT_FALSE(project.HasZ3dkConfig());
#endif
}

}  // namespace yaze::project
