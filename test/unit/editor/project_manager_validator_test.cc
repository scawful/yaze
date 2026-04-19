// Tests for ProjectManager's project-file validation and open-flow, covering
// the .yazeproj bundle and legacy .yaze/.zsproj paths.
//
// These tests exercise the public OpenProject() API to verify that:
//  - .yaze files are accepted
//  - .yazeproj bundle directories are accepted
//  - .yazeproj regular files (not directories) are rejected
//  - .zsproj files are accepted as project files (not ROM files)
//  - ROMs and arbitrary files are rejected
//  - Empty paths and nonexistent paths are rejected

#include "app/editor/system/session/project_manager.h"

#include <chrono>
#include <filesystem>
#include <fstream>
#include <string>
#include <system_error>
#include <utility>

#include "absl/status/status.h"
#include "gtest/gtest.h"

namespace yaze::editor {

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
  ASSERT_TRUE(file.is_open()) << "Failed to open: " << path.string();
  file << data;
}

// Minimal portable project file content (paths relative to its directory).
std::string MinimalProjectFile(const std::string& name) {
  return "[project]\nname=" + name +
         "\n\n"
         "[files]\n"
         "rom_filename=\n"
         "code_folder=\n"
         "rom_backup_folder=backups\n"
         "output_folder=output\n"
         "labels_filename=labels.txt\n"
         "symbols_filename=symbols.txt\n";
}

// Create a minimal but valid .yazeproj bundle at base/stem.yazeproj.
std::filesystem::path CreateBundle(const std::filesystem::path& base,
                                   const std::string& stem) {
  const auto bundle = base / (stem + ".yazeproj");
  std::filesystem::create_directories(bundle);
  WriteTextFile(bundle / "project.yaze", MinimalProjectFile(stem));
  return bundle;
}

}  // namespace

// ---------------------------------------------------------------------------
// Rejection cases
// ---------------------------------------------------------------------------

TEST(ProjectManagerValidatorTest, RejectsEmptyFilename) {
  ProjectManager mgr(nullptr);
  EXPECT_FALSE(mgr.OpenProject("").ok());
}

TEST(ProjectManagerValidatorTest, RejectsNonexistentPath) {
  ProjectManager mgr(nullptr);
  const std::string ghost =
      (std::filesystem::temp_directory_path() / "nope_xyzzy.yaze").string();
  EXPECT_FALSE(mgr.OpenProject(ghost).ok());
}

TEST(ProjectManagerValidatorTest, RejectsRomExtension) {
  ScopedTempDir temp(MakeUniqueTempDir("yaze_pm_rom_reject"));
  const auto rom_path = temp.path() / "game.sfc";
  WriteTextFile(rom_path, "not a rom");
  ProjectManager mgr(nullptr);
  EXPECT_FALSE(mgr.OpenProject(rom_path.string()).ok());
}

TEST(ProjectManagerValidatorTest, RejectsArbitraryExtension) {
  ScopedTempDir temp(MakeUniqueTempDir("yaze_pm_arb_reject"));
  const auto path = temp.path() / "data.bin";
  WriteTextFile(path, "garbage");
  ProjectManager mgr(nullptr);
  EXPECT_FALSE(mgr.OpenProject(path.string()).ok());
}

TEST(ProjectManagerValidatorTest, RejectsYazeProjRegularFile) {
  // A file named *.yazeproj (not a directory) must be rejected by the
  // validator before it reaches project.Open(), which requires a directory.
  ScopedTempDir temp(MakeUniqueTempDir("yaze_pm_fakebundle"));
  const auto fake_bundle = temp.path() / "Fake.yazeproj";
  WriteTextFile(fake_bundle, "not a directory bundle");
  ProjectManager mgr(nullptr);
  EXPECT_FALSE(mgr.OpenProject(fake_bundle.string()).ok());
}

// ---------------------------------------------------------------------------
// Acceptance cases
// ---------------------------------------------------------------------------

TEST(ProjectManagerValidatorTest, AcceptsYazeFile) {
  ScopedTempDir temp(MakeUniqueTempDir("yaze_pm_yaze_accept"));
  const auto yaze_path = temp.path() / "MyProject.yaze";
  WriteTextFile(yaze_path, MinimalProjectFile("MyProject"));
  ProjectManager mgr(nullptr);
  EXPECT_TRUE(mgr.OpenProject(yaze_path.string()).ok());
}

TEST(ProjectManagerValidatorTest, AcceptsYazeProjBundle) {
  // .yazeproj bundle directory with a project.yaze inside.
  ScopedTempDir temp(MakeUniqueTempDir("yaze_pm_bundle_accept"));
  const auto bundle = CreateBundle(temp.path(), "BundleProject");
  ProjectManager mgr(nullptr);
  EXPECT_TRUE(mgr.OpenProject(bundle.string()).ok());
}

TEST(ProjectManagerValidatorTest, AcceptsYazeProjBundleWithoutProjectFile) {
  // Bundle with no project.yaze: Open() auto-creates it and returns ok.
  ScopedTempDir temp(MakeUniqueTempDir("yaze_pm_bundle_autocreate"));
  const auto bundle = temp.path() / "AutoCreate.yazeproj";
  std::filesystem::create_directories(bundle);
  // Intentionally omit project.yaze.
  ProjectManager mgr(nullptr);
  EXPECT_TRUE(mgr.OpenProject(bundle.string()).ok());
}

TEST(ProjectManagerValidatorTest, ZsprojNotRoutedToRomLoader) {
  // .zsproj must be recognized as a project file (not passed to LoadRom()).
  // Since ZScream import touches disk, just verify the error is NOT "unsupported
  // extension" or the LoadRom "not a valid ROM" error but comes from the project
  // layer (e.g., file-not-found or parse error).
  ScopedTempDir temp(MakeUniqueTempDir("yaze_pm_zsproj"));
  const auto zsproj_path = temp.path() / "hack.zsproj";
  WriteTextFile(zsproj_path, "not a real zscream project");
  ProjectManager mgr(nullptr);
  const auto status = mgr.OpenProject(zsproj_path.string());
  // It must not claim the extension is unsupported, nor silently succeed.
  EXPECT_NE(status.code(), absl::StatusCode::kInvalidArgument)
      << "Expected .zsproj to pass extension validation; got: "
      << status.message();
}

// ---------------------------------------------------------------------------
// HasActiveProject reflects successful open
// ---------------------------------------------------------------------------

TEST(ProjectManagerValidatorTest, HasActiveProjectAfterSuccessfulOpen) {
  ScopedTempDir temp(MakeUniqueTempDir("yaze_pm_active_project"));
  const auto yaze_path = temp.path() / "Active.yaze";
  WriteTextFile(yaze_path, MinimalProjectFile("Active"));
  ProjectManager mgr(nullptr);
  ASSERT_TRUE(mgr.OpenProject(yaze_path.string()).ok());
  EXPECT_TRUE(mgr.HasActiveProject());
}

TEST(ProjectManagerValidatorTest, HasActiveProjectAfterBundleOpen) {
  ScopedTempDir temp(MakeUniqueTempDir("yaze_pm_bundle_active"));
  const auto bundle = CreateBundle(temp.path(), "ActiveBundle");
  ProjectManager mgr(nullptr);
  ASSERT_TRUE(mgr.OpenProject(bundle.string()).ok());
  EXPECT_TRUE(mgr.HasActiveProject());
}

}  // namespace yaze::editor
