#include "core/project.h"

#include <chrono>
#include <filesystem>
#include <fstream>
#include <string>
#include <vector>

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

bool ContainsPath(const std::vector<std::string>& items,
                  const std::filesystem::path& target) {
  const std::string needle = target.string();
  for (const auto& item : items) {
    if (item == needle) {
      return true;
    }
  }
  return false;
}

}  // namespace

TEST(ProjectBundleTest, OpenCreatesProjectFileWhenMissing) {
  ScopedTempDir temp(MakeUniqueTempDir("yaze_project_bundle"));

  const auto bundle_root = temp.path() / "MyBundle.yazeproj";
  std::filesystem::create_directories(bundle_root);

  // iOS bundles commonly use `project/` for the code snapshot.
  std::filesystem::create_directories(bundle_root / "project");
  WriteTextFile(bundle_root / "rom", "not a real rom");

  YazeProject project;
  ASSERT_TRUE(project.Open(bundle_root.string()).ok());

  const auto expected_project_file = bundle_root / "project.yaze";
  EXPECT_EQ(project.filepath, expected_project_file.string());
  EXPECT_TRUE(std::filesystem::exists(expected_project_file));

  EXPECT_EQ(project.name, "MyBundle");
  EXPECT_EQ(project.rom_filename, (bundle_root / "rom").string());
  EXPECT_EQ(project.code_folder, (bundle_root / "project").string());

  const auto saved = ReadTextFile(expected_project_file);
  EXPECT_NE(saved.find("rom_filename=rom"), std::string::npos);
  EXPECT_NE(saved.find("code_folder=project"), std::string::npos);
}

TEST(ProjectBundleTest, ProjectManagerFindsBundleDirectories) {
  ScopedTempDir temp(MakeUniqueTempDir("yaze_project_discovery"));

  const auto bundle_root = temp.path() / "FoundMe.yazeproj";
  std::filesystem::create_directories(bundle_root);

  const auto found = ProjectManager::FindProjectsInDirectory(temp.path().string());
  EXPECT_TRUE(ContainsPath(found, bundle_root));
}

}  // namespace yaze::project

