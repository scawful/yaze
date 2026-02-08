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
  ASSERT_TRUE(file.is_open()) << "Failed to open file for writing: "
                              << path.string();
  file << data;
  file.close();
}

std::string ReadTextFile(const std::filesystem::path& path) {
  std::ifstream file(path);
  EXPECT_TRUE(file.is_open()) << "Failed to open file for reading: "
                              << path.string();
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

}  // namespace yaze::project

